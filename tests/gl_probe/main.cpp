#include "wgl_host.h"

#include "IGPU_native.h"

#include "core/GMExtWire.h"

#include <Windows.h>
#include <GL/gl.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    int fail(const char* message)
    {
        std::fprintf(stderr, "%s\n", message);
        igpu_shutdown();
        gl_probe_end();
        return 1;
    }
}

int main()
{
    if (igpu_bind_current())
    {
        std::fprintf(stderr, "bind succeeded with no context\n");
        return 1;
    }
    if (igpu_get_last_error() != "igpu_bind_current: no OpenGL context is current on this thread")
    {
        std::fprintf(stderr, "unexpected error: %s\n", igpu_get_last_error().c_str());
        return 1;
    }
    if (igpu_is_available())
    {
        std::fprintf(stderr, "available after a failed bind\n");
        return 1;
    }

    std::string error;
    if (!gl_probe_begin(error))
    {
        std::fprintf(stderr, "gl host failed: %s\n", error.c_str());
        return 1;
    }
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::printf("gl version  : %s\n", version != nullptr ? version : "(null)");
    std::printf("gl renderer : %s\n", renderer != nullptr ? renderer : "(null)");

    if (!igpu_bind_current())
    {
        std::fprintf(stderr, "bind failed: %s\n", igpu_get_last_error().c_str());
        gl_probe_end();
        return 1;
    }
    if (std::string(igpu_get_shader_dialect()) != "glsl")
    {
        std::fprintf(stderr, "dialect %s\n", igpu_get_shader_dialect().c_str());
        igpu_shutdown();
        gl_probe_end();
        return 1;
    }
    if (igpu_get_feature_level() != 0)
    {
        std::fprintf(stderr, "feature level lied: %d\n", igpu_get_feature_level());
        igpu_shutdown();
        gl_probe_end();
        return 1;
    }

    const char* vs =
        "#version 120\n"
        "attribute vec2 in_pos;\n"
        "void main() { gl_Position = vec4(in_pos, 0.0, 1.0); }\n";
    const char* ps =
        "#version 120\n"
        "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";
    if (igpu_shader_compile(vs, "main", 0, "hlsl") != 0)
    {
        return fail("hlsl dialect was accepted");
    }
    if (igpu_get_last_error().find("dialect 'hlsl' is not supported by the 'opengl' backend") == std::string::npos)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_shader_compile(vs, "main", 0, "glsl_es") != 0)
    {
        return fail("glsl_es was accepted on a desktop context");
    }
    if (igpu_shader_compile(vs, "not_main", 0, "glsl") != 0)
    {
        return fail("non-main entry was accepted");
    }
    if (igpu_shader_compile(vs, "main", 2, "") != 0)
    {
        return fail("compute stage was accepted");
    }
    const auto vert = igpu_shader_compile(vs, "main", 0, "");
    const auto frag = igpu_shader_compile(ps, "main", 1, "glsl");
    if (vert == 0 || frag == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_shader_bind(frag, 0))
    {
        return fail("pixel shader bound to the vertex stage");
    }
    if (igpu_get_bound_shader(0) != 0 || igpu_get_bound_shader(1) != 0)
    {
        return fail("a rejected bind changed the bound shader");
    }
    if (!igpu_shader_bind(vert, 0) || igpu_get_bound_shader(0) != vert)
    {
        return fail("vertex bind failed");
    }
    if (!igpu_shader_bind(frag, 1) || igpu_get_bound_shader(1) != frag)
    {
        return fail(igpu_get_last_error().c_str());
    }

    struct CapExpect
    {
        std::int32_t id;
        bool value;
        const char* name;
    };
    const CapExpect rows[] = {
        {1, true, "ShaderCompileRuntime"},
        {2, true, "ShaderStageVertex"},
        {3, true, "ShaderStagePixel"},
        {4, false, "ShaderStageCompute"},
        {20, false, "Texture3D"},
        {26, true, "Texture2D"},
        {47, true, "InputLayout"},
        {48, true, "VertexBuffer"},
        {49, false, "IndexBuffer"},
        {50, false, "UniformBuffer"},
        {53, true, "Draw"},
        {54, false, "DrawIndexed"},
        {55, true, "DrawStateRestore"},
        {63, false, "UniformReflection"},
    };
    for (const CapExpect& row : rows)
    {
        if (igpu_supports(row.id) != row.value)
        {
            std::fprintf(stderr, "capability %s expected %d\n", row.name, row.value ? 1 : 0);
            return fail(row.name);
        }
    }
    if (igpu_get_video_memory() != 0 || igpu_get_backbuffer_width() != 0 || igpu_get_backbuffer_height() != 0)
    {
        return fail("opengl reported d3d memory or a backbuffer");
    }
    if (igpu_get_adapter_description().empty())
    {
        return fail("renderer name is empty");
    }

    const auto encode_i32 = [](std::int32_t value) {
        std::vector<std::byte> bytes(7);
        bytes[0] = std::byte{1};
        bytes[1] = std::byte{0};
        bytes[2] = std::byte{6};
        std::memcpy(bytes.data() + 3, &value, 4);
        return bytes;
    };
    const auto encode_u64 = [](std::uint64_t value) {
        std::vector<std::byte> bytes(11);
        bytes[0] = std::byte{1};
        bytes[1] = std::byte{0};
        bytes[2] = std::byte{12};
        std::memcpy(bytes.data() + 3, &value, 8);
        return bytes;
    };
    const auto as_array = [](const std::vector<std::byte>& bytes) {
        return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array, bytes.data()));
    };

    const char* ps_blue =
        "#version 120\n"
        "void main() { gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0); }\n";
    const auto blue = igpu_shader_compile(ps_blue, "main", 1, "glsl");
    if (blue == 0 || !igpu_shader_bind(blue, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }

    const float full_screen[] = {
        -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
    };
    const float top_left[] = {
        -1.f, 0.f, -1.f, 1.f, 0.f, 1.f, -1.f, 0.f, 0.f, 1.f, 0.f, 0.f,
    };
    const auto full_buffer = igpu_buffer_create(48, 0, 1, 8);
    const auto part_buffer = igpu_buffer_create(48, 0, 1, 8);
    if (full_buffer == 0 || part_buffer == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(full_buffer), 0, gm::wire::GMBuffer(const_cast<float*>(full_screen), 48)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(part_buffer), 0, gm::wire::GMBuffer(const_cast<float*>(top_left), 48)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto usage_bytes = encode_i32(1);
    const auto type_bytes = encode_i32(2);
    const auto step_bytes = encode_i32(0);
    const auto layout = igpu_input_layout_create(vert, as_array(usage_bytes), as_array(type_bytes), as_array(step_bytes), 1, 8, 0);
    const auto texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (layout == 0 || texture == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto target_bytes = encode_u64(static_cast<std::uint64_t>(texture));
    const auto zero_bytes = encode_u64(0);
    const GLint saved_viewport[4] = {0, 0, 1, 1};
    glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(full_buffer), static_cast<std::uint64_t>(layout), 4, 0, 6,
                                     as_array(target_bytes), as_array(zero_bytes), as_array(zero_bytes), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(frag, 1) ||
        !igpu_draw_to_render_targets(static_cast<std::uint64_t>(part_buffer), static_cast<std::uint64_t>(layout), 4, 0, 6,
                                     as_array(target_bytes), as_array(zero_bytes), as_array(zero_bytes), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto top_left_pixel = igpu_texture_read(static_cast<std::uint64_t>(texture), 0, 0, 0, 0);
    const auto top_right_pixel = igpu_texture_read(static_cast<std::uint64_t>(texture), 7, 0, 0, 0);
    const auto bottom_left_pixel = igpu_texture_read(static_cast<std::uint64_t>(texture), 0, 7, 0, 0);
    std::printf("pixels       : %lld / %lld / %lld\n", static_cast<long long>(top_left_pixel),
                static_cast<long long>(top_right_pixel), static_cast<long long>(bottom_left_pixel));
    if (top_left_pixel != 255 || top_right_pixel != 16711680 || bottom_left_pixel != 16711680)
    {
        return fail("pixel readback did not match the top-left quadrant");
    }
    if (igpu_draw_to_render_targets(static_cast<std::uint64_t>(part_buffer), static_cast<std::uint64_t>(layout), 6, 0, 6,
                                    as_array(target_bytes), as_array(zero_bytes), as_array(zero_bytes), 0, 0, 0, 0))
    {
        return fail("triangle fan was accepted");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(texture), 0, 0, 0, 0) != 255)
    {
        return fail("triangle fan changed a pixel");
    }
    GLint viewport[4] = {};
    GLint framebuffer = 0;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(0x8CA6, &framebuffer);
    if (viewport[0] != 0 || viewport[1] != 0 || viewport[2] != 1 || viewport[3] != 1 || framebuffer != 0)
    {
        return fail("draw did not restore the viewport and framebuffer");
    }

    void* context = gl_probe_context();
    igpu_shutdown();
    if (!wglMakeCurrent(nullptr, nullptr) || !wglDeleteContext(static_cast<HGLRC>(context)))
    {
        std::fprintf(stderr, "wglDeleteContext failed; IGPU deleted the borrowed context\n");
        gl_probe_end();
        return 1;
    }
    gl_probe_forget();
    gl_probe_end();
    std::printf("PASS\n");
    return 0;
}
