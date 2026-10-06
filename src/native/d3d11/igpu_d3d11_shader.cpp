#include "igpu_d3d11.h"

#include <d3dcompiler.h>

#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <string_view>

#include "../igpu_capabilities.h"
#include "../igpu_device.h"
#include "../igpu_error.h"

namespace igpu
{
namespace d3d11_impl
{
    namespace
    {
        // Mirrors IgpuShaderStage. Callers pass the integer; this file is the
        // one place that turns it into a Direct3D shader profile.
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

        const char* entry_target_for(ShaderStage stage)
        {
            switch (stage)
            {
            case ShaderStage::Vertex:   return "vs_5_0";
            case ShaderStage::Pixel:    return "ps_5_0";
            case ShaderStage::Compute:  return "cs_5_0";
            case ShaderStage::Geometry: return "gs_5_0";
            case ShaderStage::Hull:     return "hs_5_0";
            case ShaderStage::Domain:   return "ds_5_0";
            case ShaderStage::Mesh:
            case ShaderStage::Amplification:
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

        const char* stored_stage_name(std::int32_t raw)
        {
            ShaderStage stage{};
            return stage_from_int(raw, stage) ? stage_name(stage) : "unknown";
        }

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

        std::string build_error_message(ShaderStage stage, ID3DBlob* errors, HRESULT hr)
        {
            std::string message = "igpu_shader_compile (";
            message += stage_name(stage);
            message += ") failed (hr=0x";
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

        std::int64_t create_shader_handle(
            ID3D11DeviceChild* shader, ID3DBlob* bytecode, ShaderStage stage, UniformLayout layout)
        {
            auto& s = igpu::state();
            const std::uint64_t id = s.next_shader_id++;
            DeviceState::ShaderEntry entry;
            entry.object = shader;
            entry.bytecode = bytecode;
            entry.stage = static_cast<std::int32_t>(stage);
            entry.uniforms = std::move(layout);
            s.shaders.emplace(id, std::move(entry));
            return static_cast<std::int64_t>(id);
        }
    }

    std::int64_t shader_compile(std::string_view source, std::string_view entry_point,
                                std::int32_t stage_raw, std::string_view dialect)
    {
        clear_last_error();
        if (!require_device("igpu_shader_compile"))
        {
            return 0;
        }

        ShaderStage stage{};
        if (!stage_from_int(stage_raw, stage))
        {
            set_last_error("igpu_shader_compile: unknown shader stage");
            return 0;
        }

        if (source.empty() || entry_point.empty())
        {
            set_last_error("igpu_shader_compile: source and entry must not be empty");
            return 0;
        }

        if (!stage_supported(stage))
        {
            std::string message = "igpu_shader_compile: stage '";
            message += stage_name(stage);
            message += "' is not supported by the '";
            message += igpu::backend_name();
            message += "' backend (check igpu_supports)";
            set_last_error(std::move(message));
            return 0;
        }

        const char* default_target = entry_target_for(stage);
        if (default_target == nullptr)
        {
            set_last_error("igpu_shader_compile: no default profile for this stage");
            return 0;
        }

        std::string source_text(source);
        const std::string entry_name(entry_point);
        if (dialect.empty() || dialect == "hlsl")
        {
        }
        else if (dialect == "glsl" || dialect == "glsl_es")
        {
            std::string translated;
            if (!translate_glsl_to_hlsl(source, static_cast<std::int32_t>(stage), entry_point, translated))
            {
                return 0;
            }
            source_text = std::move(translated);
        }
        else
        {
            std::string message = "igpu_shader_compile: dialect '";
            message += std::string(dialect);
            message += "' is not supported by the '";
            message += igpu::backend_name();
            message += "' backend (it compiles '";
            message += igpu::shader_dialect();
            message += "')";
            set_last_error(std::move(message));
            return 0;
        }

        const std::string profile(default_target);

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
            entry_name.c_str(),
            profile.c_str(),
            flags,
            0,
            &bytecode,
            &errors);

        if (FAILED(hr) || bytecode == nullptr)
        {
            set_last_error(build_error_message(stage, errors, hr));
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

        auto& s = state();
        ID3D11DeviceChild* shader = nullptr;
        switch (stage)
        {
        case ShaderStage::Vertex:
        {
            ID3D11VertexShader* vs = nullptr;
            if (SUCCEEDED(s.device->CreateVertexShader(
                    bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &vs)))
            {
                shader = vs;
            }
            break;
        }
        case ShaderStage::Pixel:
        {
            ID3D11PixelShader* ps = nullptr;
            if (SUCCEEDED(s.device->CreatePixelShader(
                    bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &ps)))
            {
                shader = ps;
            }
            break;
        }
        case ShaderStage::Compute:
        {
            ID3D11ComputeShader* cs = nullptr;
            if (SUCCEEDED(s.device->CreateComputeShader(
                    bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &cs)))
            {
                shader = cs;
            }
            break;
        }
        case ShaderStage::Geometry:
        {
            ID3D11GeometryShader* gs = nullptr;
            if (SUCCEEDED(s.device->CreateGeometryShader(
                    bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &gs)))
            {
                shader = gs;
            }
            break;
        }
        case ShaderStage::Hull:
        {
            ID3D11HullShader* hs = nullptr;
            if (SUCCEEDED(s.device->CreateHullShader(
                    bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &hs)))
            {
                shader = hs;
            }
            break;
        }
        case ShaderStage::Domain:
        {
            ID3D11DomainShader* ds = nullptr;
            if (SUCCEEDED(s.device->CreateDomainShader(
                    bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &ds)))
            {
                shader = ds;
            }
            break;
        }
        case ShaderStage::Mesh:
        case ShaderStage::Amplification:
            break;
        }

        if (shader == nullptr)
        {
            bytecode->Release();
            set_last_error(std::string("igpu_shader_compile_") + stage_name(stage) + ": CreateShader failed");
            return 0;
        }

        UniformLayout layout;
        if (!reflect_uniforms(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), layout))
        {
            shader->Release();
            bytecode->Release();
            if (last_error().empty())
            {
                set_last_error("igpu_shader_compile: the shader's uniform layout could not be read");
            }
            return 0;
        }
        return create_shader_handle(shader, bytecode, stage, std::move(layout));
    }

    bool shader_release(std::uint64_t shader)
    {
        auto* entry = find_shader(shader);
        if (entry == nullptr)
        {
            set_last_error("igpu_shader_release: unknown shader handle");
            return false;
        }
        if (entry->object != nullptr)
        {
            entry->object->Release();
        }
        if (entry->bytecode != nullptr)
        {
            entry->bytecode->Release();
        }
        auto& s = state();
        for (auto it = s.bound_shaders.begin(); it != s.bound_shaders.end();)
        {
            it = (it->second == shader) ? s.bound_shaders.erase(it) : std::next(it);
        }
        s.shaders.erase(shader);
        return true;
    }

    bool shader_bind(std::int64_t shader, std::int32_t stage_raw)
    {
        clear_last_error();
        if (!require_device("igpu_shader_bind"))
        {
            return false;
        }

        ShaderStage stage{};
        if (!stage_from_int(stage_raw, stage))
        {
            set_last_error(
                "igpu_shader_bind: unknown stage value " + std::to_string(stage_raw) +
                " (use an IgpuShaderStage constant)");
            return false;
        }

        auto& s = state();
        if (shader == 0)
        {
            switch (stage)
            {
            case ShaderStage::Vertex:   s.context->VSSetShader(nullptr, nullptr, 0); break;
            case ShaderStage::Pixel:    s.context->PSSetShader(nullptr, nullptr, 0); break;
            case ShaderStage::Compute:  s.context->CSSetShader(nullptr, nullptr, 0); break;
            case ShaderStage::Geometry: s.context->GSSetShader(nullptr, nullptr, 0); break;
            case ShaderStage::Hull:     s.context->HSSetShader(nullptr, nullptr, 0); break;
            case ShaderStage::Domain:   s.context->DSSetShader(nullptr, nullptr, 0); break;
            default:
                set_last_error(
                    std::string("igpu_shader_bind: stage '") + stage_name(stage) +
                    "' has no backend stage to unbind");
                return false;
            }
            s.bound_shaders.erase(static_cast<std::int32_t>(stage));
            return true;
        }

        auto* entry = find_shader(static_cast<std::uint64_t>(shader));
        if (entry == nullptr)
        {
            set_last_error("igpu_shader_bind: unknown shader handle");
            return false;
        }
        if (entry->stage != static_cast<std::int32_t>(stage))
        {
            set_last_error(
                std::string("igpu_shader_bind: the handle was compiled for stage '") +
                stored_stage_name(entry->stage) + "' but was bound to '" +
                stage_name(stage) + "'");
            return false;
        }

        switch (stage)
        {
        case ShaderStage::Vertex:
            s.context->VSSetShader(static_cast<ID3D11VertexShader*>(entry->object), nullptr, 0);
            break;
        case ShaderStage::Pixel:
            s.context->PSSetShader(static_cast<ID3D11PixelShader*>(entry->object), nullptr, 0);
            break;
        case ShaderStage::Compute:
            s.context->CSSetShader(static_cast<ID3D11ComputeShader*>(entry->object), nullptr, 0);
            break;
        case ShaderStage::Geometry:
            s.context->GSSetShader(static_cast<ID3D11GeometryShader*>(entry->object), nullptr, 0);
            break;
        case ShaderStage::Hull:
            s.context->HSSetShader(static_cast<ID3D11HullShader*>(entry->object), nullptr, 0);
            break;
        case ShaderStage::Domain:
            s.context->DSSetShader(static_cast<ID3D11DomainShader*>(entry->object), nullptr, 0);
            break;
        default:
            set_last_error(
                std::string("igpu_shader_bind: stage '") + stage_name(stage) +
                "' has no backend stage object");
            return false;
        }

        s.bound_shaders[static_cast<std::int32_t>(stage)] = static_cast<std::uint64_t>(shader);
        return true;
    }

    std::int64_t get_bound_shader(std::int32_t stage_raw)
    {
        clear_last_error();
        ShaderStage stage{};
        if (!stage_from_int(stage_raw, stage))
        {
            set_last_error("igpu_get_bound_shader: unknown stage value " + std::to_string(stage_raw));
            return 0;
        }
        const auto& bound = state().bound_shaders;
        const auto it = bound.find(static_cast<std::int32_t>(stage));
        return it == bound.end() ? 0 : static_cast<std::int64_t>(it->second);
    }
}
}
