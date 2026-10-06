#include "igpu_capabilities.h"

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "igpu_backend.h"
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
            refresh_device_status();
            const auto& s = state();
            return s.initialised && active_backend() != nullptr && !s.device_lost;
        }

        bool d3d11_backend()
        {
            return has_backend() && std::string_view(active_backend()->name()) == "d3d11";
        }

        bool opengl_backend()
        {
            return has_backend() && std::string_view(active_backend()->name()) == "opengl";
        }

        // Feature level of the borrowed device; gates the optional states.
        bool device_at_least(D3D_FEATURE_LEVEL level)
        {
            return d3d11_backend() && state().device != nullptr &&
                   state().device->GetFeatureLevel() >= level;
        }

        struct GraphicsProbe
        {
            bool active = false;
            std::string device_name;
            const char* backend = "";
            const char* dialect = "";
            int major = 0;
            int minor = 0;
            std::int32_t max_texture = 0;
        };

        GraphicsProbe g_probe;

        bool parse_gl_number(std::string_view text, int& major, int& minor)
        {
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] < '0' || text[i] > '9')
                {
                    continue;
                }
                major = 0;
                std::size_t j = i;
                while (j < text.size() && text[j] >= '0' && text[j] <= '9')
                {
                    major = major * 10 + (text[j] - '0');
                    ++j;
                    if (major > 9)
                    {
                        return false;
                    }
                }
                minor = 0;
                if (j < text.size() && text[j] == '.')
                {
                    ++j;
                    while (j < text.size() && text[j] >= '0' && text[j] <= '9')
                    {
                        minor = minor * 10 + (text[j] - '0');
                        ++j;
                    }
                }
                return major > 0;
            }
            return false;
        }

        bool classify_graphics(std::string_view version, std::string_view shading, GraphicsProbe& probe)
        {
            const auto es = version.find("OpenGL ES");
            const auto web = version.find("WebGL");
            std::string_view number = version;
            if (web != std::string_view::npos)
            {
                probe.backend = "webgl";
                probe.dialect = "glsl_es";
                number = version.substr(web + 5);
            }
            else if (es != std::string_view::npos)
            {
                probe.backend = "gles";
                probe.dialect = "glsl_es";
                number = version.substr(es + 9);
            }
            else
            {
                probe.backend = "opengl";
                probe.dialect = "glsl";
            }
            if (!parse_gl_number(number, probe.major, probe.minor))
            {
                return false;
            }
            if (probe.backend == std::string_view("opengl") && shading.find("ES") != std::string_view::npos)
            {
                probe.dialect = "glsl_es";
            }
            return true;
        }
    }

    bool set_graphics_info(std::string_view vendor, std::string_view version, std::string_view renderer,
                           std::string_view shading_language, std::int32_t max_texture_size)
    {
        clear_last_error();
        if (version.empty())
        {
            g_probe = {};
            return true;
        }
        if (max_texture_size < 0)
        {
            set_last_error("igpu_set_graphics_info: max_texture_size must not be negative");
            return false;
        }
        GraphicsProbe next;
        if (!classify_graphics(version, shading_language, next))
        {
            set_last_error("igpu_set_graphics_info: the version string is not an OpenGL, OpenGL ES, or WebGL version");
            return false;
        }
        next.active = true;
        next.max_texture = max_texture_size;
        if (!renderer.empty())
        {
            next.device_name = std::string(renderer);
        }
        else
        {
            next.device_name = std::string(vendor);
        }
        g_probe = std::move(next);
        return true;
    }

    const char* probed_backend()
    {
        return g_probe.active ? g_probe.backend : "";
    }

    const char* probed_dialect()
    {
        return g_probe.active ? g_probe.dialect : "";
    }

    std::string probed_device_name()
    {
        return g_probe.active ? g_probe.device_name : std::string{};
    }

    bool probed_format(std::int32_t format)
    {
        if (!g_probe.active)
        {
            return false;
        }
        const bool modern = (std::string_view(g_probe.backend) == "webgl" && g_probe.major >= 2) ||
                            (std::string_view(g_probe.backend) != "webgl" && g_probe.major >= 3);
        switch (format)
        {
        case 11: return true;                         // surface_rgba4unorm
        case 6:                                        // surface_rgba8unorm
        case 9:                                        // surface_r16float
        case 10:                                       // surface_r32float
        case 12:                                       // surface_r8unorm
        case 13:                                       // surface_rg8unorm
        case 14:                                       // surface_rgba16float
        case 15: return modern;                        // surface_rgba32float
        default: return false;
        }
    }

    const char* backend_name()
    {
        // A bound backend names itself. "none" is the honest answer when
        // nothing is bound and no graphics string was recorded.
        if (has_backend())
        {
            return active_backend()->name();
        }
        const char* probed = probed_backend();
        return probed[0] != '\0' ? probed : "none";
    }

    const char* shader_dialect()
    {
        if (has_backend())
        {
            if (std::string_view(active_backend()->name()) == "opengl")
            {
                return state().gl_dialect.c_str();
            }
            return "hlsl";
        }
        const char* probed = probed_dialect();
        return probed[0] != '\0' ? probed : "";
    }

    bool supports(Capability capability)
    {
        // The handover checker treats a bare `return native` as true on this
        // build. native is the D3D11 backend, not every bound backend.
        const bool native = d3d11_backend();

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
        case Capability::ShaderCompileRuntime: return native || opengl_backend();
        case Capability::ShaderStageVertex:    return native || opengl_backend();
        case Capability::ShaderStagePixel:     return native || opengl_backend();
        case Capability::ShaderStageCompute:   return native;

        // Geometry and tessellation are core from D3D feature level 11_0.
        case Capability::ShaderStageGeometry:     return device_at_least(D3D_FEATURE_LEVEL_11_0);
        case Capability::ShaderStageTessellation: return device_at_least(D3D_FEATURE_LEVEL_11_0);

        // Mesh/amplification shaders need D3D12 with shader model 6.5; the
        // interface reserves the stages but this backend cannot serve them.
        case Capability::ShaderStageMesh: return false;

        // ---- resources ----
        // These stay false until an API exists. Reporting true with no function
        // sends callers to a call that does not exist. verify_handover.ps1
        // checks the ones that return native.
        // Keep this comment ASCII: MSVC reads the file as code page 936, and a
        // UTF-8 byte of 0x5C in a comment escapes the newline and deletes the
        // next line. That is how Texture2D was compiled out.
        case Capability::Texture2D:             return native || opengl_backend();
        case Capability::Texture3D:             return native;
        case Capability::TextureArray:          return native;
        case Capability::TextureCubemap:        return native;
        case Capability::StructuredBuffer:      return device_at_least(D3D_FEATURE_LEVEL_11_0);
        // Writable images need a compute shader. Feature level 11.0 has them.
        case Capability::UnorderedAccess:       return device_at_least(D3D_FEATURE_LEVEL_11_0);
        case Capability::MultipleRenderTargets: return native;

        // ---- pipeline ----
        case Capability::Instancing:     return native;
        case Capability::IndirectDraw:   return device_at_least(D3D_FEATURE_LEVEL_11_0);
        case Capability::Queries:        return active_backend() != nullptr &&
                   (active_backend()->occlusion() || active_backend()->timestamps());
        case Capability::Timestamps:     return active_backend() != nullptr && active_backend()->timestamps();
        case Capability::OcclusionQuery: return active_backend() != nullptr && active_backend()->occlusion();
        case Capability::Fence:          return active_backend() != nullptr && active_backend()->fences();
        case Capability::Wireframe:      return native;  // IgpuFill.Wireframe on igpu_raster_state_create

        // ---- geometry submission ----
        // The input layout and buffer APIs are implemented, so these report
        // true whenever a device is bound.
        case Capability::InputLayout:   return native || opengl_backend();
        case Capability::VertexBuffer:  return native || opengl_backend();
        case Capability::IndexBuffer:   return native;
        case Capability::UniformBuffer: return native;
        case Capability::BufferResize:  return native;
        case Capability::BufferReadback: return native;

        // ---- drawing ----
        // DrawStateRestore reports whether IGPU puts the input assembler back
        // after a draw, by reading it from the device rather than guessing.
        // GameMaker's next draw rebinds its own vertex buffer, layout and
        // topology; the restore still matters for the index buffer, which
        // GameMaker never rebinds, and for any reader of the device in between.
        // A backend that could not read the assembler would report false.
        case Capability::Draw:             return native || opengl_backend();
        case Capability::DrawIndexed:      return native;
        case Capability::DrawStateRestore: return native || opengl_backend();
        case Capability::BlendState:       return native;
        case Capability::DepthState:       return native;
        case Capability::RasterState:      return native;
        case Capability::SamplerState:     return native;
        case Capability::UniformReflection: return native;

        case Capability::None:
        default:
            return false;
        }
    }

    gm::wire::DataStream build_capabilities()
    {
        const bool bound = has_backend();

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
        const int tier = bound ? 1 : (probed_backend()[0] != '\0' ? 2 : 3);
        caps.add("tier", static_cast<std::int32_t>(tier));
        std::string device_name = state().adapter_desc_valid
            ? igpu::narrow(state().adapter_desc.Description)
            : (!state().renderer_name.empty() ? state().renderer_name : probed_device_name());
        caps.add("device_name", std::string_view(device_name));
        caps.add("shader_dialect", std::string_view(shader_dialect()));

        // ---- shader capability ----
        caps.add("shader_stages", stages);
        caps.add("runtime_compile", supports(Capability::ShaderCompileRuntime));
        caps.add("compute", supports(Capability::ShaderStageCompute));
        caps.add("geometry", supports(Capability::ShaderStageGeometry));
        caps.add("tessellation", supports(Capability::ShaderStageTessellation));
        caps.add("mesh_shader", supports(Capability::ShaderStageMesh));

        // ---- resource capability ----
        caps.add("texture_2d", supports(Capability::Texture2D));
        caps.add("texture_3d", supports(Capability::Texture3D));
        caps.add("texture_array", supports(Capability::TextureArray));
        caps.add("texture_cubemap", supports(Capability::TextureCubemap));
        caps.add("structured_buffer", supports(Capability::StructuredBuffer));
        caps.add("uav", supports(Capability::UnorderedAccess));
        // Four, matching the engine's own simultaneous colour targets.
        // D3D11 allows eight; this API stops at four.
        caps.add("max_render_targets",
                 static_cast<std::int32_t>(supports(Capability::MultipleRenderTargets) ? 4 : 0));

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

        // ---- drawing ----
        caps.add("draw", supports(Capability::Draw));
        caps.add("draw_indexed", supports(Capability::DrawIndexed));
        caps.add("draw_state_restore", supports(Capability::DrawStateRestore));
        caps.add("blend_state", supports(Capability::BlendState));
        caps.add("depth_state", supports(Capability::DepthState));
        caps.add("raster_state", supports(Capability::RasterState));
        caps.add("sampler_state", supports(Capability::SamplerState));
        caps.add("uniform_reflection", supports(Capability::UniformReflection));

        // GameMaker's eight colour surface formats, in the engine's own order.
        // The key is always present. The value is whether this device can
        // create a texture of that format. surface_rgba4unorm can be created
        // and still be rejected as a render target.
        gm::wire::StructStream formats;
        const Backend* backend = active_backend();
        const auto add_format = [&](const char* name, std::int32_t id) {
            const bool supported = backend != nullptr ? backend->texture_format(id) : probed_format(id);
            formats.add(name, supported);
        };
        add_format("surface_rgba8unorm", 6);
        add_format("surface_r16float", 9);
        add_format("surface_r32float", 10);
        add_format("surface_rgba4unorm", 11);
        add_format("surface_r8unorm", 12);
        add_format("surface_rg8unorm", 13);
        add_format("surface_rgba16float", 14);
        add_format("surface_rgba32float", 15);
        caps.add("formats", formats);

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
