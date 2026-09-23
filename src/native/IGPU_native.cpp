#include "IGPU_native.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "igpu_device.h"
#include "igpu_error.h"

using namespace gm::wire;
using namespace gm_structs;
using namespace gm_enums;

namespace
{
    constexpr const char* kIgpuVersion = "0.2.0";

    enum class ShaderStage
    {
        Vertex,
        Pixel,
        Compute
    };

    const char* entry_target_for(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:  return "vs_5_0";
        case ShaderStage::Pixel:   return "ps_5_0";
        case ShaderStage::Compute: return "cs_5_0";
        }
        return "vs_5_0";
    }

    const char* stage_name(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:  return "vertex";
        case ShaderStage::Pixel:   return "pixel";
        case ShaderStage::Compute: return "compute";
        }
        return "unknown";
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

        const std::string target_profile =
            target.empty() ? entry_target_for(stage) : std::string(target);

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

std::int64_t igpu_shader_compile_vertex(
    std::string_view source,
    std::string_view entry,
    std::string_view target)
{
    return compile_shader(ShaderStage::Vertex, source, entry, target);
}

std::int64_t igpu_shader_compile_pixel(
    std::string_view source,
    std::string_view entry,
    std::string_view target)
{
    return compile_shader(ShaderStage::Pixel, source, entry, target);
}

std::int64_t igpu_shader_compile_compute(
    std::string_view source,
    std::string_view entry,
    std::string_view target)
{
    return compile_shader(ShaderStage::Compute, source, entry, target);
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
