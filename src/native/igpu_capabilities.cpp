#include "igpu_capabilities.h"

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "igpu_device.h"
#include "igpu_error.h"

// ============================================================================
// Capability query (Tier 3)
//
// igpu_get_capabilities() hands GML a plain struct. GMIDL has no top-level
// struct declaration syntax (verified: extgen rejects it), so the return is
// declared `gmval`, which extgen maps to gm::wire::DataStream.
//
// Why the struct is built with StructStream and then copied byte-for-byte:
//
//   StructStream::writeTo() emits, in order,
//       Struct tag (u8)  -- via codec::writeValue, i.e. a RAW byte
//       entry count (u16)-- likewise raw
//       then each key/value pair
//   which is exactly what __ext_core_buffer_unmarshal_value() in
//   ExtensionCore_api.gml expects.
//
//   DataStream::operator<<, by contrast, prepends a *kind byte* before every
//   value. Using it for the header would emit [255][255][count] where GML
//   reads [255] as the tag and the next two bytes as the count - producing
//   nonsense (observed at runtime as the number 255).
//
//   The stream also cannot simply be returned as a StructStream, because the
//   generated bridge declares the return type as DataStream, and returning a
//   derived object by value slices the virtual writeTo() override.
//
// So: build with StructStream (correct header), serialise through its public
// writeTo(), and hand those bytes to a DataStream for the bridge to copy out.
//
// Every key below is always present, so callers can read any field
// unconditionally.
// ============================================================================

namespace igpu
{
    namespace
    {
        bool has_backend()
        {
            return state().initialised && state().device != nullptr;
        }

        // Feature level of the borrowed device; gates the optional states.
        bool device_at_least(D3D_FEATURE_LEVEL level)
        {
            return has_backend() && state().device->GetFeatureLevel() >= level;
        }
    }

    const char* backend_name()
    {
        // Only the Windows D3D11 backend exists today. Reporting "none" on
        // every other platform is the honest answer and lets callers branch on
        // it instead of probing individual capabilities.
        return has_backend() ? "d3d11" : "none";
    }

    const char* shader_dialect()
    {
        return has_backend() ? "hlsl" : "";
    }

    bool supports(Capability capability)
    {
        const bool native = has_backend();

        switch (capability)
        {
        // ---- platform info ----
        // Reflects what GameMaker's os_get_info() actually exposes. Windows is
        // the only platform reporting a real adapter: Xbox returns 0 / "" for
        // video_adapter_*, and the GL platforms expose driver strings only.
        case Capability::AdapterInfo:    return state().adapter_desc_valid;
        case Capability::VideoMemory:    return state().adapter_desc_valid;
        case Capability::BackbufferSize: return state().swapchain != nullptr;

        // ---- shader compilation ----
        case Capability::ShaderCompileRuntime: return native;
        case Capability::ShaderStageVertex:    return native;
        case Capability::ShaderStagePixel:     return native;
        case Capability::ShaderStageCompute:   return native;

        // Geometry and tessellation are core from D3D feature level 11_0.
        case Capability::ShaderStageGeometry:     return device_at_least(D3D_FEATURE_LEVEL_11_0);
        case Capability::ShaderStageTessellation: return device_at_least(D3D_FEATURE_LEVEL_11_0);

        // Mesh/amplification shaders need D3D12 with shader model 6.5; the
        // interface reserves the stages but this backend cannot serve them.
        case Capability::ShaderStageMesh: return false;

        // ---- resources ----
        case Capability::Texture3D:             return native;
        case Capability::TextureArray:          return native;
        case Capability::TextureCubemap:        return native;
        case Capability::StructuredBuffer:      return device_at_least(D3D_FEATURE_LEVEL_11_0);
        case Capability::UnorderedAccess:       return device_at_least(D3D_FEATURE_LEVEL_11_0);
        case Capability::MultipleRenderTargets: return native;

        // ---- pipeline ----
        case Capability::Instancing:     return native;
        case Capability::IndirectDraw:   return native;
        case Capability::Queries:        return native;
        case Capability::Timestamps:     return native;
        case Capability::OcclusionQuery: return native;
        case Capability::Fence:          return native;
        case Capability::Wireframe:      return native;

        // ---- geometry submission ----
        // The input layout and buffer APIs are implemented, so these report
        // true whenever a device is bound.
        case Capability::InputLayout:   return native;
        case Capability::VertexBuffer:  return native;
        case Capability::IndexBuffer:   return native;
        case Capability::UniformBuffer: return native;
        case Capability::BufferResize:  return native;
        case Capability::BufferReadback: return native;

        case Capability::None:
        default:
            return false;
        }
    }

    gm::wire::DataStream build_capabilities()
    {
        const bool native = has_backend();

        // Enumerate the stages this backend can compile for, so callers can
        // iterate instead of probing one at a time.
        //
        // Each push() must be given an explicit std::string_view: the literals
        // would otherwise bind to the bool overload (pointer-to-bool is a
        // standard conversion, while string_view needs a user-defined one),
        // silently emitting five booleans and desynchronising the whole struct.
        gm::wire::ArrayStream stages;
        if (supports(Capability::ShaderStageVertex))  stages.push(std::string_view("vertex"));
        if (supports(Capability::ShaderStagePixel))   stages.push(std::string_view("pixel"));
        if (supports(Capability::ShaderStageCompute)) stages.push(std::string_view("compute"));
        if (supports(Capability::ShaderStageGeometry))     stages.push(std::string_view("geometry"));
        if (supports(Capability::ShaderStageTessellation)) stages.push(std::string_view("tessellation"));
        if (supports(Capability::ShaderStageMesh))         stages.push(std::string_view("mesh"));

        gm::wire::StructStream caps;

        // Every string value must be an explicit std::string_view. Passing a
        // bare `const char*` binds to addKeyValue's `const T&` template with
        // T = char[N], and the value then encodes as something other than a
        // string (observed at runtime as the number 1).
        // ---- backend identity ----
        caps.add("backend", std::string_view(backend_name()));
        caps.add("tier", static_cast<std::int32_t>(native ? 1 : 3));
        caps.add("device_name",
                 state().adapter_desc_valid
                     ? igpu::narrow(state().adapter_desc.Description)
                     : std::string{});
        caps.add("shader_dialect", std::string_view(shader_dialect()));

        // ---- shader capability ----
        caps.add("shader_stages", stages);
        caps.add("runtime_compile", supports(Capability::ShaderCompileRuntime));
        caps.add("compute", supports(Capability::ShaderStageCompute));
        caps.add("geometry", supports(Capability::ShaderStageGeometry));
        caps.add("tessellation", supports(Capability::ShaderStageTessellation));
        caps.add("mesh_shader", supports(Capability::ShaderStageMesh));

        // ---- resource capability ----
        caps.add("texture_3d", supports(Capability::Texture3D));
        caps.add("texture_array", supports(Capability::TextureArray));
        caps.add("texture_cubemap", supports(Capability::TextureCubemap));
        caps.add("structured_buffer", supports(Capability::StructuredBuffer));
        caps.add("uav", supports(Capability::UnorderedAccess));
        caps.add("max_render_targets",
                 static_cast<std::int32_t>(
                     supports(Capability::MultipleRenderTargets)
                         ? D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT
                         : 0));

        // ---- pipeline capability ----
        caps.add("instancing", supports(Capability::Instancing));
        caps.add("indirect_draw", supports(Capability::IndirectDraw));
        caps.add("queries", supports(Capability::Queries));

        // ---- geometry submission ----
        caps.add("input_layout", supports(Capability::InputLayout));
        caps.add("vertex_buffer", supports(Capability::VertexBuffer));
        caps.add("index_buffer", supports(Capability::IndexBuffer));
        caps.add("uniform_buffer", supports(Capability::UniformBuffer));
        caps.add("buffer_resize", supports(Capability::BufferResize));
        caps.add("buffer_readback", supports(Capability::BufferReadback));

        // Serialise through StructStream::writeTo(), which is the only thing
        // that knows both the entry count and the raw-header encoding, then
        // seed those exact bytes into the returned stream's own buffer.
        //
        // The bytes must land in the DataStream base's buffer rather than in a
        // derived member: this function returns DataStream by value, so any
        // derived state (including a virtual writeTo override) is sliced away.
        std::vector<std::byte> bytes;
        bytes.reserve(1024);
        gm::byteio::VectorWriter writer(bytes);
        caps.writeTo(writer);

        SeedableDataStream out;
        out.append(bytes.data(), bytes.size());
        return out;
    }
}
