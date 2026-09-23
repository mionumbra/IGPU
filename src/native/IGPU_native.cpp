#include "IGPU_native.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "igpu_device.h"
#include "igpu_capabilities.h"
#include "igpu_error.h"

using namespace gm::wire;
using namespace gm_structs;
using namespace gm_enums;

namespace
{
    constexpr const char* kIgpuVersion = "0.3.0";

    // Shader stages are backend-neutral (see spec.gmidl). This mirrors the
    // IgpuShaderStage enum; the numeric values must stay in sync with it.
    enum class ShaderStage : std::int32_t
    {
        Vertex = 0,
        Pixel = 1,
        Compute = 2,
        Geometry = 3,
        Hull = 4,
        Domain = 5,
        Mesh = 6,
        Amplification = 7
    };

    bool stage_from_int(std::int32_t raw, ShaderStage& out)
    {
        switch (raw)
        {
        case 0: out = ShaderStage::Vertex;        return true;
        case 1: out = ShaderStage::Pixel;         return true;
        case 2: out = ShaderStage::Compute;       return true;
        case 3: out = ShaderStage::Geometry;      return true;
        case 4: out = ShaderStage::Hull;          return true;
        case 5: out = ShaderStage::Domain;        return true;
        case 6: out = ShaderStage::Mesh;          return true;
        case 7: out = ShaderStage::Amplification; return true;
        default: return false;
        }
    }

    // Default compilation profile per stage. This is the ONE place where
    // backend-specific terminology is allowed, because IGPU is the thing that
    // translates a neutral request into a backend profile - callers never see it.
    const char* entry_target_for(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:        return "vs_5_0";
        case ShaderStage::Pixel:         return "ps_5_0";
        case ShaderStage::Compute:       return "cs_5_0";
        case ShaderStage::Geometry:      return "gs_5_0";
        case ShaderStage::Hull:          return "hs_5_0";
        case ShaderStage::Domain:        return "ds_5_0";
        case ShaderStage::Mesh:
        case ShaderStage::Amplification:
            // SM 6.5 mesh/amplification shaders are not available on the D3D11
            // path; reported as unsupported rather than silently mistranslated.
            return nullptr;
        }
        return nullptr;
    }

    const char* stage_name(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:        return "vertex";
        case ShaderStage::Pixel:         return "pixel";
        case ShaderStage::Compute:       return "compute";
        case ShaderStage::Geometry:      return "geometry";
        case ShaderStage::Hull:          return "hull";
        case ShaderStage::Domain:        return "domain";
        case ShaderStage::Mesh:          return "mesh";
        case ShaderStage::Amplification: return "amplification";
        }
        return "unknown";
    }

    // Maps a neutral capability back to the stage it refers to, so
    // compile_shader() can fail with the right message on unsupported stages.
    bool stage_supported(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:   return igpu::supports(igpu::Capability::ShaderStageVertex);
        case ShaderStage::Pixel:    return igpu::supports(igpu::Capability::ShaderStagePixel);
        case ShaderStage::Compute:  return igpu::supports(igpu::Capability::ShaderStageCompute);
        case ShaderStage::Geometry: return igpu::supports(igpu::Capability::ShaderStageGeometry);
        case ShaderStage::Hull:
        case ShaderStage::Domain:   return igpu::supports(igpu::Capability::ShaderStageTessellation);
        case ShaderStage::Mesh:
        case ShaderStage::Amplification:
            return igpu::supports(igpu::Capability::ShaderStageMesh);
        }
        return false;
    }

    std::string build_error_message(
        ShaderStage stage,
        ID3DBlob* errors,
        HRESULT hr)
    {
        std::string message = "igpu_shader_compile_";
        message += stage_name(stage);
        message += " failed (hr=0x";

        char hex[16] = {};
        std::snprintf(hex, sizeof(hex), "%08lX", static_cast<unsigned long>(hr));
        message += hex;
        message += ")";

        if (errors != nullptr && errors->GetBufferPointer() != nullptr)
        {
            message += ": ";
            message += static_cast<const char*>(errors->GetBufferPointer());
        }

        return message;
    }

    std::int64_t create_shader_handle(ID3D11DeviceChild* shader)
    {
        auto& s = igpu::state();
        const std::uint64_t id = s.next_shader_id++;
        s.shaders.emplace(id, shader);
        return static_cast<std::int64_t>(id);
    }

    std::int64_t compile_shader(
        ShaderStage stage,
        std::string_view source,
        std::string_view entry,
        std::string_view target)
    {
        igpu::clear_last_error();

        auto& s = igpu::state();
        if (!s.initialised || s.device == nullptr)
        {
            igpu::set_last_error("igpu_shader_compile: call igpu_init() first");
            return 0;
        }

        if (source.empty() || entry.empty())
        {
            igpu::set_last_error("igpu_shader_compile: source and entry must not be empty");
            return 0;
        }

        // Check the backend can actually do this stage BEFORE reaching the
        // compiler, so the caller gets a capability message rather than an
        // opaque HLSL error.
        if (!stage_supported(stage))
        {
            std::string message = "igpu_shader_compile: stage '";
            message += stage_name(stage);
            message += "' is not supported by the '";
            message += igpu::backend_name();
            message += "' backend (check igpu_supports)";
            igpu::set_last_error(std::move(message));
            return 0;
        }

        const char* default_target = entry_target_for(stage);
        if (target.empty() && default_target == nullptr)
        {
            igpu::set_last_error("igpu_shader_compile: no default profile for this stage");
            return 0;
        }

        const std::string target_profile =
            target.empty() ? std::string(default_target) : std::string(target);

        const std::string entry_point(entry);
        const std::string source_text(source);

        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        ID3DBlob* bytecode = nullptr;
        ID3DBlob* errors = nullptr;
        const HRESULT hr = ::D3DCompile(
            source_text.data(),
            source_text.size(),
            nullptr,
            nullptr,
            nullptr,
            entry_point.c_str(),
            target_profile.c_str(),
            flags,
            0,
            &bytecode,
            &errors);

        if (FAILED(hr) || bytecode == nullptr)
        {
            igpu::set_last_error(build_error_message(stage, errors, hr));
            if (errors != nullptr)
            {
                errors->Release();
            }
            if (bytecode != nullptr)
            {
                bytecode->Release();
            }
            return 0;
        }

        if (errors != nullptr)
        {
            errors->Release();
        }

        ID3D11DeviceChild* shader = nullptr;
        switch (stage)
        {
        case ShaderStage::Vertex:
        {
            ID3D11VertexShader* vs = nullptr;
            if (SUCCEEDED(s.device->CreateVertexShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr,
                    &vs)))
            {
                shader = vs;
            }
            break;
        }
        case ShaderStage::Pixel:
        {
            ID3D11PixelShader* ps = nullptr;
            if (SUCCEEDED(s.device->CreatePixelShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr,
                    &ps)))
            {
                shader = ps;
            }
            break;
        }
        case ShaderStage::Compute:
        {
            ID3D11ComputeShader* cs = nullptr;
            if (SUCCEEDED(s.device->CreateComputeShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr,
                    &cs)))
            {
                shader = cs;
            }
            break;
        }
        case ShaderStage::Geometry:
        {
            ID3D11GeometryShader* gs = nullptr;
            if (SUCCEEDED(s.device->CreateGeometryShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr,
                    &gs)))
            {
                shader = gs;
            }
            break;
        }
        case ShaderStage::Hull:
        {
            ID3D11HullShader* hs = nullptr;
            if (SUCCEEDED(s.device->CreateHullShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr,
                    &hs)))
            {
                shader = hs;
            }
            break;
        }
        case ShaderStage::Domain:
        {
            ID3D11DomainShader* ds = nullptr;
            if (SUCCEEDED(s.device->CreateDomainShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr,
                    &ds)))
            {
                shader = ds;
            }
            break;
        }
        case ShaderStage::Mesh:
        case ShaderStage::Amplification:
            // Unreachable: stage_supported() already rejected these. Kept so
            // the switch stays exhaustive under strict warnings.
            break;
        }

        bytecode->Release();

        if (shader == nullptr)
        {
            igpu::set_last_error(
                std::string("igpu_shader_compile_") + stage_name(stage) +
                ": CreateShader failed");
            return 0;
        }

        return create_shader_handle(shader);
    }
}

bool igpu_init(
    const gm::wire::GMValue& device,
    const gm::wire::GMValue& context,
    const gm::wire::GMValue& swapchain)
{
    igpu::clear_last_error();

    const auto read_pointer = [](const gm::wire::GMValue& value) -> void* {
        if (value.kind() != gm::wire::GMKind::Pointer)
        {
            return nullptr;
        }
        return reinterpret_cast<void*>(
            gm::byteio::readLe<std::uintptr_t>(value.data()));
    };

    auto* device_ptr = static_cast<ID3D11Device*>(read_pointer(device));
    auto* context_ptr = static_cast<ID3D11DeviceContext*>(read_pointer(context));
    auto* swapchain_ptr = static_cast<IDXGISwapChain*>(read_pointer(swapchain));

    return igpu::bind_device(device_ptr, context_ptr, swapchain_ptr);
}

void igpu_shutdown()
{
    igpu::release_all();
}

std::string igpu_version()
{
    return kIgpuVersion;
}

bool igpu_is_available()
{
    return igpu::state().initialised;
}

std::int32_t igpu_get_feature_level()
{
    const auto& s = igpu::state();
    if (!s.initialised)
    {
        return static_cast<std::int32_t>(IgpuFeatureLevel::Unknown);
    }

    switch (s.feature_level)
    {
    case D3D_FEATURE_LEVEL_11_1:
        return static_cast<std::int32_t>(IgpuFeatureLevel::Level_11_1);
    case D3D_FEATURE_LEVEL_12_0:
        return static_cast<std::int32_t>(IgpuFeatureLevel::Level_12_0);
    case D3D_FEATURE_LEVEL_12_1:
        return static_cast<std::int32_t>(IgpuFeatureLevel::Level_12_1);
    default:
        return static_cast<std::int32_t>(IgpuFeatureLevel::Level_11_0);
    }
}

std::string igpu_get_adapter_description()
{
    const auto& s = igpu::state();
    if (!s.adapter_desc_valid)
    {
        return {};
    }
    return igpu::narrow(s.adapter_desc.Description);
}

std::int64_t igpu_get_video_memory()
{
    const auto& s = igpu::state();
    if (!s.adapter_desc_valid)
    {
        return 0;
    }
    return static_cast<std::int64_t>(s.adapter_desc.DedicatedVideoMemory);
}

std::int32_t igpu_get_backbuffer_width()
{
    return igpu::state().backbuffer_width;
}

std::int32_t igpu_get_backbuffer_height()
{
    return igpu::state().backbuffer_height;
}

// ---------------------------------------------------------------------------
// Capability query (Tier 3)
// ---------------------------------------------------------------------------

gm::wire::DataStream igpu_get_capabilities()
{
    return igpu::build_capabilities();
}

bool igpu_supports(std::int32_t capability)
{
    return igpu::supports(static_cast<igpu::Capability>(capability));
}

std::string igpu_get_shader_dialect()
{
    return igpu::shader_dialect();
}

// ---------------------------------------------------------------------------
// Runtime shader compilation
// ---------------------------------------------------------------------------

std::int64_t igpu_shader_compile(
    std::string_view source,
    std::string_view entry,
    std::int32_t stage,
    std::string_view dialect)
{
    ShaderStage resolved{};
    if (!stage_from_int(stage, resolved))
    {
        igpu::set_last_error("igpu_shader_compile: unknown shader stage");
        return 0;
    }

    // Only HLSL is available on the current backend. An explicit conflicting
    // dialect is a caller error worth reporting rather than silently ignoring.
    if (!dialect.empty() && dialect != "hlsl")
    {
        std::string message = "igpu_shader_compile: dialect '";
        message += std::string(dialect);
        message += "' is not supported by the '";
        message += igpu::backend_name();
        message += "' backend (it compiles '";
        message += igpu::shader_dialect();
        message += "')";
        igpu::set_last_error(std::move(message));
        return 0;
    }

    return compile_shader(resolved, source, entry, {});
}

std::int64_t igpu_shader_compile_vertex(
    std::string_view source,
    std::string_view entry,
    std::string_view dialect)
{
    return igpu_shader_compile(source, entry, static_cast<std::int32_t>(ShaderStage::Vertex), dialect);
}

std::int64_t igpu_shader_compile_pixel(
    std::string_view source,
    std::string_view entry,
    std::string_view dialect)
{
    return igpu_shader_compile(source, entry, static_cast<std::int32_t>(ShaderStage::Pixel), dialect);
}

std::int64_t igpu_shader_compile_compute(
    std::string_view source,
    std::string_view entry,
    std::string_view dialect)
{
    return igpu_shader_compile(source, entry, static_cast<std::int32_t>(ShaderStage::Compute), dialect);
}

bool igpu_shader_release(std::uint64_t shader)
{
    auto& s = igpu::state();
    const auto it = s.shaders.find(shader);
    if (it == s.shaders.end())
    {
        igpu::set_last_error("igpu_shader_release: unknown shader handle");
        return false;
    }

    if (it->second != nullptr)
    {
        it->second->Release();
    }
    s.shaders.erase(it);
    return true;
}

std::string igpu_get_last_error()
{
    return igpu::last_error();
}
