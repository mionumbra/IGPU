#include "wgl_host.h"

#include "IGPU_native.h"

#include "core/GMExtWire.h"
#include "native/gl/igpu_gl_draw.h"

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
        {49, true, "IndexBuffer"},
        {50, false, "UniformBuffer"},
        {53, true, "Draw"},
        {54, true, "DrawIndexed"},
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

    const float indexed_vertices[] = {
        -1.f, -1.f, 0.f, -1.f, -1.f, 1.f, 0.f, 1.f, 1.f, -1.f, 1.f, 1.f,
    };
    const std::uint16_t indexed_indices[] = {
        0, 1, 2, 1, 3, 2, 1, 4, 3, 4, 5, 3,
    };
    const auto indexed_vertices_buffer = igpu_buffer_create(48, 0, 1, 8);
    const auto indexed_index_buffer = igpu_buffer_create(24, 0, 2, 0);
    if (indexed_vertices_buffer == 0 || indexed_index_buffer == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(indexed_vertices_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(indexed_vertices), 48)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(indexed_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(indexed_indices), 24)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_buffer_create(24, 1, 2, 0) != 0 || igpu_buffer_create(24, 0, 2, 2) != 0 ||
        igpu_buffer_create(23, 0, 2, 0) != 0)
    {
        return fail("dynamic, strided, or odd index buffer was accepted");
    }
    const auto indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (indexed_texture == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto indexed_target = encode_u64(static_cast<std::uint64_t>(indexed_texture));
    if (!igpu_shader_bind(blue, 1) ||
        !igpu_draw_to_render_targets(static_cast<std::uint64_t>(full_buffer), static_cast<std::uint64_t>(layout), 4, 0, 6,
                                     as_array(indexed_target), as_array(zero_bytes), as_array(zero_bytes), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(frag, 1))
    {
        return fail("red shader rebound failed");
    }
    if (igpu_draw_to_render_targets(static_cast<std::uint64_t>(indexed_index_buffer), static_cast<std::uint64_t>(layout), 4, 0, 6,
                                    as_array(indexed_target), as_array(zero_bytes), as_array(zero_bytes), 0, 0, 0, 0))
    {
        return fail("index buffer was accepted as a vertex buffer");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(indexed_texture), 1, 4, 0, 0) != 16711680 ||
        igpu_texture_read(static_cast<std::uint64_t>(indexed_texture), 6, 4, 0, 0) != 16711680)
    {
        return fail("drawing an index buffer as vertices changed a pixel");
    }

    std::int32_t saved_fbo = 0;
    std::int32_t held_viewport[4] = {};
    const auto reject_leaves_blue = [&](const char* label) {
        if (igpu_texture_read(static_cast<std::uint64_t>(indexed_texture), 1, 4, 0, 0) != 16711680 ||
            igpu_texture_read(static_cast<std::uint64_t>(indexed_texture), 6, 4, 0, 0) != 16711680)
        {
            std::fprintf(stderr, "%s changed a pixel\n", label);
            return false;
        }
        return true;
    };
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(indexed_texture), saved_fbo, held_viewport))
    {
        return fail("color target begin failed");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_vertices_buffer), 4, 0, 6, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(saved_fbo, held_viewport);
        return fail("vertex buffer was accepted as an index buffer");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          999999, 4, 0, 6, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(saved_fbo, held_viewport);
        return fail("unknown index buffer was accepted");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_index_buffer), 4, 0, 13, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(saved_fbo, held_viewport);
        return fail("thirteen indices were accepted");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_index_buffer), 6, 0, 6, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(saved_fbo, held_viewport);
        return fail("triangle fan was accepted");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_index_buffer), 4, 0, 6, 1, 0, 0, 0))
    {
        igpu::gl_color_target_end(saved_fbo, held_viewport);
        return fail("blend state was accepted");
    }
    igpu::gl_color_target_end(saved_fbo, held_viewport);
    if (!reject_leaves_blue("rejected indexed draw"))
    {
        return fail("rejected indexed draw changed a pixel");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(indexed_texture), saved_fbo, held_viewport))
    {
        return fail("color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(indexed_index_buffer), 4, 0, 6, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(saved_fbo, held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(saved_fbo, held_viewport);
    const auto indexed_left = igpu_texture_read(static_cast<std::uint64_t>(indexed_texture), 1, 4, 0, 0);
    const auto indexed_right = igpu_texture_read(static_cast<std::uint64_t>(indexed_texture), 6, 4, 0, 0);
    std::printf("indexed pixels: %lld / %lld\n", static_cast<long long>(indexed_left),
                static_cast<long long>(indexed_right));
    if (indexed_left != 255 || indexed_right != 16711680)
    {
        return fail("first six indices did not paint only the left half");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(indexed_texture), saved_fbo, held_viewport))
    {
        return fail("color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(indexed_index_buffer), 4, 6, -1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(saved_fbo, held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(saved_fbo, held_viewport);
    const auto tail_left = igpu_texture_read(static_cast<std::uint64_t>(indexed_texture), 1, 4, 0, 0);
    const auto tail_right = igpu_texture_read(static_cast<std::uint64_t>(indexed_texture), 6, 4, 0, 0);
    std::printf("indexed tail  : %lld / %lld\n", static_cast<long long>(tail_left),
                static_cast<long long>(tail_right));
    if (tail_left != 255 || tail_right != 255)
    {
        return fail("the tail index range did not paint only the right half");
    }

    if (igpu_texture_read(static_cast<std::uint64_t>(texture), 0, 0, 0, 0) != 255)
    {
        return fail("indexed draw changed the first texture");
    }
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(part_buffer), static_cast<std::uint64_t>(layout), 4, 0, 6,
                                     as_array(target_bytes), as_array(zero_bytes), as_array(zero_bytes), 0, 0, 0, 0))
    {
        return fail("non-indexed draw failed after indexed draw");
    }
    GLint viewport_after[4] = {};
    GLint framebuffer_after = 0;
    glGetIntegerv(GL_VIEWPORT, viewport_after);
    glGetIntegerv(0x8CA6, &framebuffer_after);
    if (viewport_after[2] != 1 || viewport_after[3] != 1 || framebuffer_after != 0)
    {
        return fail("indexed section left the framebuffer or viewport bound");
    }

    const float draw_halves[] = {
        -1.f, -1.f, 0.f, -1.f, -1.f, 1.f, -1.f, 1.f, 0.f, -1.f, 0.f, 1.f,
        0.f, -1.f, 1.f, -1.f, 0.f, 1.f, 0.f, 1.f, 1.f, -1.f, 1.f, 1.f,
    };
    const auto draw_buffer = igpu_buffer_create(96, 0, 1, 8);
    if (draw_buffer == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(draw_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(draw_halves), 96)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto draw_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (draw_texture == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    auto draw_pixels_are = [&](std::int64_t left, std::int64_t right, const char* label) {
        const auto got_left = igpu_texture_read(static_cast<std::uint64_t>(draw_texture), 1, 4, 0, 0);
        const auto got_right = igpu_texture_read(static_cast<std::uint64_t>(draw_texture), 6, 4, 0, 0);
        if (got_left != left || got_right != right)
        {
            std::fprintf(stderr, "%s left=%lld right=%lld\n", label,
                         static_cast<long long>(got_left), static_cast<long long>(got_right));
            return false;
        }
        return true;
    };
    if (!draw_pixels_are(0, 0, "fresh draw texture"))
    {
        return fail("fresh draw texture was not clear black");
    }

    std::int32_t draw_saved_fbo = 0;
    std::int32_t draw_held_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(draw_texture), draw_saved_fbo, draw_held_viewport))
    {
        return fail("draw color target begin failed");
    }
    GLint draw_fbo = 0;
    GLint draw_viewport[4] = {};
    GLint draw_program = 0;
    glGetIntegerv(0x8CA6, &draw_fbo);
    glGetIntegerv(GL_VIEWPORT, draw_viewport);
    glGetIntegerv(0x8B8D, &draw_program);
    const auto bound_vert = igpu_get_bound_shader(0);
    const auto bound_frag = igpu_get_bound_shader(1);
    if (draw_program == 0 || bound_vert == 0 || bound_frag == 0)
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail("draw probe lost the bound program before drawing");
    }

    if (igpu_draw(static_cast<std::uint64_t>(draw_buffer), 0, static_cast<std::uint64_t>(layout),
                  6, 0, 6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail("triangle fan was accepted by igpu_draw");
    }
    if (igpu_draw(static_cast<std::uint64_t>(draw_buffer), 0, static_cast<std::uint64_t>(layout),
                  4, 0, 6, 1, 1, 0, 0, 0))
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail("blend state was accepted by igpu_draw");
    }
    if (igpu_draw(static_cast<std::uint64_t>(draw_buffer), 0, static_cast<std::uint64_t>(layout),
                  4, 0, 6, 1, 0, 1, 0, 0) ||
        igpu_draw(static_cast<std::uint64_t>(draw_buffer), 0, static_cast<std::uint64_t>(layout),
                  4, 0, 6, 1, 0, 0, 1, 0) ||
        igpu_draw(static_cast<std::uint64_t>(draw_buffer), 0, static_cast<std::uint64_t>(layout),
                  4, 0, 6, 1, 0, 0, 0, 1))
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail("depth, raster, or sampler state was accepted by igpu_draw");
    }
    if (igpu_draw(static_cast<std::uint64_t>(indexed_index_buffer), 0, static_cast<std::uint64_t>(layout),
                  4, 0, 6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail("index buffer was accepted as a vertex buffer by igpu_draw");
    }
    if (igpu_draw(999999, 0, static_cast<std::uint64_t>(layout), 4, 0, 6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail("unknown vertex buffer was accepted by igpu_draw");
    }
    if (igpu_draw(static_cast<std::uint64_t>(draw_buffer), 0, static_cast<std::uint64_t>(layout),
                  4, 0, 13, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail("thirteen vertices were accepted by igpu_draw");
    }
    igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
    if (!draw_pixels_are(0, 0, "rejected igpu_draw"))
    {
        return fail("a rejected igpu_draw changed a pixel");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(draw_texture), draw_saved_fbo, draw_held_viewport))
    {
        return fail("draw color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(draw_buffer), 0, static_cast<std::uint64_t>(layout),
                   4, 0, 6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    GLint draw_fbo_after = 0;
    GLint draw_viewport_after[4] = {};
    GLint draw_program_after = 0;
    GLint draw_array_after = 0;
    GLint draw_vao_after = 0;
    glGetIntegerv(0x8CA6, &draw_fbo_after);
    glGetIntegerv(GL_VIEWPORT, draw_viewport_after);
    glGetIntegerv(0x8B8D, &draw_program_after);
    glGetIntegerv(0x8894, &draw_array_after);
    glGetIntegerv(0x85B5, &draw_vao_after);
    igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
    if (draw_fbo_after != draw_fbo || draw_program_after != draw_program || draw_array_after != 0 ||
        draw_vao_after != 0 || draw_viewport_after[0] != draw_viewport[0] ||
        draw_viewport_after[1] != draw_viewport[1] || draw_viewport_after[2] != draw_viewport[2] ||
        draw_viewport_after[3] != draw_viewport[3])
    {
        return fail("igpu_draw changed the framebuffer, viewport, program, array buffer, or vertex array");
    }
    if (igpu_get_bound_shader(0) != bound_vert || igpu_get_bound_shader(1) != bound_frag)
    {
        return fail("igpu_draw changed the bound shader handles");
    }
    const auto draw_left = igpu_texture_read(static_cast<std::uint64_t>(draw_texture), 1, 4, 0, 0);
    const auto draw_right = igpu_texture_read(static_cast<std::uint64_t>(draw_texture), 6, 4, 0, 0);
    std::printf("draw pixels   : %lld / %lld\n", static_cast<long long>(draw_left),
                static_cast<long long>(draw_right));
    if (draw_left != 255 || draw_right != 0)
    {
        return fail("triangle list did not paint only the left half");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(draw_texture), draw_saved_fbo, draw_held_viewport))
    {
        return fail("draw color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(draw_buffer), 0, static_cast<std::uint64_t>(layout),
                   4, 6, -1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(draw_saved_fbo, draw_held_viewport);
    const auto draw_tail_left = igpu_texture_read(static_cast<std::uint64_t>(draw_texture), 1, 4, 0, 0);
    const auto draw_tail_right = igpu_texture_read(static_cast<std::uint64_t>(draw_texture), 6, 4, 0, 0);
    std::printf("draw tail     : %lld / %lld\n", static_cast<long long>(draw_tail_left),
                static_cast<long long>(draw_tail_right));
    if (draw_tail_left != 255 || draw_tail_right != 255)
    {
        return fail("vertex_count -1 did not paint only the right half");
    }

    const auto encode_i32_2 = [](std::int32_t first, std::int32_t second) {
        std::vector<std::byte> bytes(12);
        bytes[0] = std::byte{2};
        bytes[1] = std::byte{0};
        bytes[2] = std::byte{6};
        std::memcpy(bytes.data() + 3, &first, 4);
        bytes[7] = std::byte{6};
        std::memcpy(bytes.data() + 8, &second, 4);
        return bytes;
    };

    const char* colour_vs =
        "#version 120\n"
        "attribute vec2 in_pos;\n"
        "attribute vec4 in_colour;\n"
        "varying vec4 v_colour;\n"
        "void main() {\n"
        "  gl_Position = vec4(in_pos, 0.0, 1.0);\n"
        "  v_colour = in_colour;\n"
        "}\n";
    const char* colour_ps =
        "#version 120\n"
        "varying vec4 v_colour;\n"
        "void main() { gl_FragColor = v_colour; }\n";
    const auto colour_vert = igpu_shader_compile(colour_vs, "main", 0, "glsl");
    const auto colour_frag = igpu_shader_compile(colour_ps, "main", 1, "glsl");
    if (colour_vert == 0 || colour_frag == 0 ||
        !igpu_shader_bind(colour_vert, 0) || !igpu_shader_bind(colour_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }

    struct ColourVertex
    {
        float x;
        float y;
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    };
    static_assert(sizeof(ColourVertex) == 12, "colour vertex must be tightly packed");
    const float colour_pos[] = {
        -1.f, -1.f, 0.f, -1.f, -1.f, 1.f, -1.f, 1.f, 0.f, -1.f, 0.f, 1.f,
        0.f, -1.f, 1.f, -1.f, 0.f, 1.f, 0.f, 1.f, 1.f, -1.f, 1.f, 1.f,
    };
    ColourVertex colour_vertices[12] = {};
    for (int i = 0; i < 12; ++i)
    {
        colour_vertices[i].x = colour_pos[i * 2];
        colour_vertices[i].y = colour_pos[i * 2 + 1];
        const bool left = i < 6;
        colour_vertices[i].r = left ? 255 : 0;
        colour_vertices[i].g = 0;
        colour_vertices[i].b = left ? 0 : 255;
        colour_vertices[i].a = 255;
    }
    const auto colour_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(colour_vertices)), 0, 1, 12);
    if (colour_buffer == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(colour_buffer), 0,
                           gm::wire::GMBuffer(colour_vertices, sizeof(colour_vertices))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto colour_usage = encode_i32_2(1, 2);
    const auto colour_type = encode_i32_2(2, 5);
    const auto colour_step = encode_i32_2(0, 0);
    const auto colour_layout = igpu_input_layout_create(
        colour_vert, as_array(colour_usage), as_array(colour_type), as_array(colour_step), 2, 12, 0);
    if (colour_layout == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto reversed_usage = encode_i32_2(2, 1);
    const auto reversed_type = encode_i32_2(5, 2);
    if (igpu_input_layout_create(colour_vert, as_array(reversed_usage), as_array(reversed_type),
                                 as_array(colour_step), 2, 12, 0) != 0)
    {
        return fail("colour-before-position layout was accepted");
    }

    const auto colour_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (colour_texture == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    auto colour_pixels_are = [&](std::int64_t left, std::int64_t right, const char* label) {
        const auto got_left = igpu_texture_read(static_cast<std::uint64_t>(colour_texture), 1, 4, 0, 0);
        const auto got_right = igpu_texture_read(static_cast<std::uint64_t>(colour_texture), 6, 4, 0, 0);
        if (got_left != left || got_right != right)
        {
            std::fprintf(stderr, "%s left=%lld right=%lld\n", label,
                         static_cast<long long>(got_left), static_cast<long long>(got_right));
            return false;
        }
        return true;
    };
    if (!colour_pixels_are(0, 0, "fresh colour texture"))
    {
        return fail("fresh colour texture was not clear black");
    }

    std::int32_t colour_saved_fbo = 0;
    std::int32_t colour_held_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(colour_texture), colour_saved_fbo, colour_held_viewport))
    {
        return fail("colour target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(full_buffer), 0, static_cast<std::uint64_t>(colour_layout),
                  4, 0, 6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail("float2 buffer was accepted by a colour layout");
    }
    if (igpu_draw(static_cast<std::uint64_t>(colour_buffer), 0, static_cast<std::uint64_t>(layout),
                  4, 0, 6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail("colour buffer was accepted by a float2 layout");
    }
    if (!colour_pixels_are(0, 0, "rejected colour draw"))
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail("a rejected colour draw changed pixels");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(colour_buffer), 0, static_cast<std::uint64_t>(colour_layout),
                   4, 0, 12, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
    const auto colour_left = igpu_texture_read(static_cast<std::uint64_t>(colour_texture), 1, 4, 0, 0);
    const auto colour_right = igpu_texture_read(static_cast<std::uint64_t>(colour_texture), 6, 4, 0, 0);
    std::printf("colour pixels  : %lld / %lld\n", static_cast<long long>(colour_left),
                static_cast<long long>(colour_right));
    if (colour_left != 255 || colour_right != 16711680)
    {
        return fail("vertex colour did not paint red on the left and blue on the right");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(colour_texture), colour_saved_fbo, colour_held_viewport))
    {
        return fail("colour leak target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(full_buffer), 0, static_cast<std::uint64_t>(layout),
                   4, 0, 6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
    const auto leak_left = igpu_texture_read(static_cast<std::uint64_t>(colour_texture), 1, 4, 0, 0);
    const auto leak_right = igpu_texture_read(static_cast<std::uint64_t>(colour_texture), 6, 4, 0, 0);
    std::printf("colour tail    : %lld / %lld\n", static_cast<long long>(leak_left),
                static_cast<long long>(leak_right));
    if (leak_left != 0 || leak_right != 0)
    {
        return fail("position-only draw left the colour attribute enabled");
    }

    ColourVertex alpha_vertices[6] = {};
    for (int i = 0; i < 6; ++i)
    {
        alpha_vertices[i].x = colour_pos[i * 2];
        alpha_vertices[i].y = colour_pos[i * 2 + 1];
        alpha_vertices[i].r = 255;
        alpha_vertices[i].a = 0;
    }
    const auto alpha_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(alpha_vertices)), 0, 1, 12);
    const auto alpha_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (alpha_buffer == 0 || alpha_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(alpha_buffer), 0,
                           gm::wire::GMBuffer(alpha_vertices, sizeof(alpha_vertices))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(alpha_texture), colour_saved_fbo, colour_held_viewport))
    {
        return fail("alpha target begin failed");
    }
    if (glIsEnabled(0x0BE2) == GL_TRUE)
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail("the probe context already had blending enabled");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(alpha_buffer), 0, static_cast<std::uint64_t>(colour_layout),
                   4, 0, 6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (glIsEnabled(0x0BE2) == GL_TRUE)
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail("igpu_draw enabled blending");
    }
    igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
    const auto alpha_pixel = igpu_texture_read(static_cast<std::uint64_t>(alpha_texture), 1, 4, 0, 0);
    std::printf("colour alpha   : %lld\n", static_cast<long long>(alpha_pixel));
    if (alpha_pixel != 255)
    {
        return fail("alpha 0 did not write red");
    }

    unsigned short colour_indices[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const auto colour_index_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(colour_indices)), 0, 2, 0);
    const auto indexed_colour_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (colour_index_buffer == 0 || indexed_colour_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(colour_index_buffer), 0,
                           gm::wire::GMBuffer(colour_indices, sizeof(colour_indices))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(indexed_colour_texture), colour_saved_fbo,
                                     colour_held_viewport))
    {
        return fail("indexed colour target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(colour_buffer), static_cast<std::uint64_t>(colour_layout),
                           static_cast<std::uint64_t>(colour_index_buffer), 4, 0, 6, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(colour_buffer), static_cast<std::uint64_t>(colour_layout),
                           static_cast<std::uint64_t>(colour_index_buffer), 4, 6, -1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(colour_saved_fbo, colour_held_viewport);
    const auto colour_indexed_left = igpu_texture_read(static_cast<std::uint64_t>(indexed_colour_texture), 1, 4, 0, 0);
    const auto colour_indexed_right = igpu_texture_read(static_cast<std::uint64_t>(indexed_colour_texture), 6, 4, 0, 0);
    std::printf("colour indexed : %lld / %lld\n", static_cast<long long>(colour_indexed_left),
                static_cast<long long>(colour_indexed_right));
    if (colour_indexed_left != 255 || colour_indexed_right != 16711680)
    {
        return fail("indexed vertex colour did not paint red then blue");
    }

    const auto colour_target_bytes = encode_u64(static_cast<std::uint64_t>(colour_texture));
    const auto colour_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(colour_buffer),
                                     static_cast<std::uint64_t>(colour_layout), 4, 6, 6,
                                     as_array(colour_target_bytes), as_array(colour_zero_bytes),
                                     as_array(colour_zero_bytes), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto target_left = igpu_texture_read(static_cast<std::uint64_t>(colour_texture), 1, 4, 0, 0);
    const auto target_right = igpu_texture_read(static_cast<std::uint64_t>(colour_texture), 6, 4, 0, 0);
    std::printf("colour target  : %lld / %lld\n", static_cast<long long>(target_left),
                static_cast<long long>(target_right));
    if (target_left != 0 || target_right != 16711680)
    {
        return fail("draw-to-targets did not take the colour from vertex 6");
    }

    if (!igpu_shader_bind(vert, 0) || !igpu_shader_bind(frag, 1))
    {
        return fail("could not restore the constant red shader");
    }

    if (igpu_texture_read(static_cast<std::uint64_t>(texture), 0, 0, 0, 0) != 255)
    {
        return fail("igpu_draw changed the first texture");
    }
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(part_buffer), static_cast<std::uint64_t>(layout), 4, 0, 6,
                                     as_array(target_bytes), as_array(zero_bytes), as_array(zero_bytes), 0, 0, 0, 0))
    {
        return fail("draw-to-targets failed after igpu_draw");
    }
    GLint viewport_after_draw[4] = {};
    GLint framebuffer_after_draw = 0;
    glGetIntegerv(GL_VIEWPORT, viewport_after_draw);
    glGetIntegerv(0x8CA6, &framebuffer_after_draw);
    if (viewport_after_draw[2] != 1 || viewport_after_draw[3] != 1 || framebuffer_after_draw != 0)
    {
        return fail("igpu_draw section left the framebuffer or viewport bound");
    }

    const float strip_vertices[] = {
        0.f, -1.f, 0.f, 1.f, -0.25f, -1.f, -1.f, 0.f,
        0.f, -1.f, 0.f, 1.f, 0.25f, -1.f, 1.f, 0.f,
    };
    const std::uint16_t strip_indices[] = {0, 1, 2, 3, 4, 5, 6, 7};
    const auto strip_buffer = igpu_buffer_create(64, 0, 1, 8);
    const auto strip_index_buffer = igpu_buffer_create(16, 0, 2, 0);
    const auto strip_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto strip_indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto strip_target_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (strip_buffer == 0 || strip_index_buffer == 0 || strip_texture == 0 ||
        strip_indexed_texture == 0 || strip_target_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(strip_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(strip_vertices), 64)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(strip_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(strip_indices), 16)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    auto strip_pixels_are = [&](std::int64_t texture, std::int64_t left, std::int64_t right,
                                const char* label) {
        const auto got_left = igpu_texture_read(static_cast<std::uint64_t>(texture), 1, 4, 0, 0);
        const auto got_right = igpu_texture_read(static_cast<std::uint64_t>(texture), 6, 4, 0, 0);
        if (got_left != left || got_right != right)
        {
            std::fprintf(stderr, "%s left=%lld right=%lld\n", label,
                         static_cast<long long>(got_left), static_cast<long long>(got_right));
            return false;
        }
        return true;
    };
    if (!strip_pixels_are(strip_texture, 0, 0, "fresh strip texture"))
    {
        return fail("fresh strip texture was not clear black");
    }

    std::int32_t strip_saved_fbo = 0;
    std::int32_t strip_held_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("strip color target begin failed");
    }
    const auto reject_strip = [&](std::int32_t primitive, std::int64_t blend, std::int64_t depth,
                                  std::int64_t raster, std::int64_t sampler, const char* what,
                                  const char* error_part) {
        if (igpu_draw(static_cast<std::uint64_t>(strip_buffer), 0, static_cast<std::uint64_t>(layout),
                      primitive, 0, 4, 1, blend, depth, raster, sampler))
        {
            igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
            std::fprintf(stderr, "%s was accepted\n", what);
            return false;
        }
        if (igpu_get_last_error().find(error_part) == std::string::npos)
        {
            igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
            std::fprintf(stderr, "%s error: %s\n", what, igpu_get_last_error().c_str());
            return false;
        }
        return true;
    };
    if (!reject_strip(0, 0, 0, 0, 0, "primitive 0", "does not draw this primitive") ||
        !reject_strip(7, 0, 0, 0, 0, "primitive 7", "does not draw this primitive") ||
        !reject_strip(6, 0, 0, 0, 0, "triangle fan", "trianglefan") ||
        !reject_strip(5, 1, 0, 0, 0, "strip blend", "draws with no extra state") ||
        !reject_strip(5, 0, 1, 0, 0, "strip depth", "draws with no extra state") ||
        !reject_strip(5, 0, 0, 1, 0, "strip raster", "draws with no extra state") ||
        !reject_strip(5, 0, 0, 0, 1, "strip sampler", "draws with no extra state"))
    {
        return fail("strip rejection check failed");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (!strip_pixels_are(strip_texture, 0, 0, "rejected strip"))
    {
        return fail("a rejected strip draw changed a pixel");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("strip color target begin failed");
    }
    GLint strip_fbo = 0;
    GLint strip_viewport[4] = {};
    GLint strip_program = 0;
    glGetIntegerv(0x8CA6, &strip_fbo);
    glGetIntegerv(GL_VIEWPORT, strip_viewport);
    glGetIntegerv(0x8B8D, &strip_program);
    if (!igpu_draw(static_cast<std::uint64_t>(strip_buffer), 0, static_cast<std::uint64_t>(layout),
                   5, 0, 4, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    GLint strip_fbo_after = 0;
    GLint strip_viewport_after[4] = {};
    GLint strip_program_after = 0;
    GLboolean strip_cull = glIsEnabled(0x0B44);
    glGetIntegerv(0x8CA6, &strip_fbo_after);
    glGetIntegerv(GL_VIEWPORT, strip_viewport_after);
    glGetIntegerv(0x8B8D, &strip_program_after);
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (strip_cull != GL_FALSE || strip_fbo_after != strip_fbo || strip_program_after != strip_program ||
        strip_viewport_after[0] != strip_viewport[0] || strip_viewport_after[1] != strip_viewport[1] ||
        strip_viewport_after[2] != strip_viewport[2] || strip_viewport_after[3] != strip_viewport[3])
    {
        return fail("igpu_draw strip changed the framebuffer, viewport, program, or cull face");
    }
    const auto strip_left = igpu_texture_read(static_cast<std::uint64_t>(strip_texture), 1, 4, 0, 0);
    const auto strip_right = igpu_texture_read(static_cast<std::uint64_t>(strip_texture), 6, 4, 0, 0);
    std::printf("strip pixels  : %lld / %lld\n", static_cast<long long>(strip_left),
                static_cast<long long>(strip_right));
    if (strip_left != 255 || strip_right != 0)
    {
        return fail("triangle strip did not paint only the left sample");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("strip color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(strip_buffer), 0, static_cast<std::uint64_t>(layout),
                   5, 4, -1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto strip_tail_left = igpu_texture_read(static_cast<std::uint64_t>(strip_texture), 1, 4, 0, 0);
    const auto strip_tail_right = igpu_texture_read(static_cast<std::uint64_t>(strip_texture), 6, 4, 0, 0);
    std::printf("strip tail    : %lld / %lld\n", static_cast<long long>(strip_tail_left),
                static_cast<long long>(strip_tail_right));
    if (strip_tail_left != 255 || strip_tail_right != 255)
    {
        return fail("triangle strip tail did not paint only the right sample");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_indexed_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("indexed strip color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(strip_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(strip_index_buffer), 5, 0, 4, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto strip_index_left = igpu_texture_read(static_cast<std::uint64_t>(strip_indexed_texture), 1, 4, 0, 0);
    const auto strip_index_right = igpu_texture_read(static_cast<std::uint64_t>(strip_indexed_texture), 6, 4, 0, 0);
    std::printf("strip indexed : %lld / %lld\n", static_cast<long long>(strip_index_left),
                static_cast<long long>(strip_index_right));
    if (strip_index_left != 255 || strip_index_right != 0)
    {
        return fail("indexed triangle strip did not paint only the left sample");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_indexed_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("indexed strip color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(strip_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(strip_index_buffer), 5, 4, -1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto strip_index_tail_left =
        igpu_texture_read(static_cast<std::uint64_t>(strip_indexed_texture), 1, 4, 0, 0);
    const auto strip_index_tail_right =
        igpu_texture_read(static_cast<std::uint64_t>(strip_indexed_texture), 6, 4, 0, 0);
    std::printf("strip index tail: %lld / %lld\n", static_cast<long long>(strip_index_tail_left),
                static_cast<long long>(strip_index_tail_right));
    if (strip_index_tail_left != 255 || strip_index_tail_right != 255)
    {
        return fail("indexed triangle strip tail did not paint only the right sample");
    }

    const auto strip_target_bytes = encode_u64(static_cast<std::uint64_t>(strip_target_texture));
    const auto strip_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(strip_buffer), static_cast<std::uint64_t>(layout),
                                     5, 0, 4, as_array(strip_target_bytes), as_array(strip_zero_bytes),
                                     as_array(strip_zero_bytes), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    GLint strip_target_viewport[4] = {};
    GLint strip_target_fbo = 0;
    glGetIntegerv(GL_VIEWPORT, strip_target_viewport);
    glGetIntegerv(0x8CA6, &strip_target_fbo);
    if (strip_target_viewport[2] != 1 || strip_target_viewport[3] != 1 || strip_target_fbo != 0)
    {
        return fail("strip draw-to-targets left the framebuffer or viewport bound");
    }
    const auto strip_target_left = igpu_texture_read(static_cast<std::uint64_t>(strip_target_texture), 1, 4, 0, 0);
    const auto strip_target_right = igpu_texture_read(static_cast<std::uint64_t>(strip_target_texture), 6, 4, 0, 0);
    std::printf("strip target  : %lld / %lld\n", static_cast<long long>(strip_target_left),
                static_cast<long long>(strip_target_right));
    if (strip_target_left != 255 || strip_target_right != 0)
    {
        return fail("draw-to-targets triangle strip did not paint only the left sample");
    }

    const float point_vertices[] = {-0.625f, -0.125f, 0.625f, -0.125f};
    const std::uint16_t point_indices[] = {0, 1};
    const auto point_buffer = igpu_buffer_create(16, 0, 1, 8);
    const auto point_index_buffer = igpu_buffer_create(4, 0, 2, 0);
    const auto point_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto point_indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (point_buffer == 0 || point_index_buffer == 0 || point_texture == 0 || point_indexed_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(point_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(point_vertices), 16)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(point_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(point_indices), 4)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(point_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("point color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(point_buffer), 0, static_cast<std::uint64_t>(layout),
                   1, 0, 1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto point_left = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 1, 4, 0, 0);
    const auto point_right = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 6, 4, 0, 0);
    const auto point_neighbor_x = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 2, 4, 0, 0);
    const auto point_neighbor_y = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 1, 3, 0, 0);
    std::printf("point pixels  : %lld / %lld\n", static_cast<long long>(point_left),
                static_cast<long long>(point_right));
    if (point_left != 255 || point_right != 0 || point_neighbor_x != 0 || point_neighbor_y != 0)
    {
        return fail("point list did not light only the left pixel center");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(point_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("point color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(point_buffer), 0, static_cast<std::uint64_t>(layout),
                   1, 1, -1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    GLfloat point_size = 0.f;
    GLfloat line_width = 0.f;
    glGetFloatv(0x0B11, &point_size);
    glGetFloatv(0x0B21, &line_width);
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto point_tail_left = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 1, 4, 0, 0);
    const auto point_tail_right = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 6, 4, 0, 0);
    const auto point_tail_neighbor = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 5, 4, 0, 0);
    std::printf("point tail    : %lld / %lld\n", static_cast<long long>(point_tail_left),
                static_cast<long long>(point_tail_right));
    if (point_tail_left != 255 || point_tail_right != 255 || point_tail_neighbor != 0 ||
        point_size != 1.f || line_width != 1.f)
    {
        return fail("point tail changed the point size, line width, or a neighbor");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(point_indexed_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("indexed point color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(point_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(point_index_buffer), 1, 0, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto point_index_left = igpu_texture_read(static_cast<std::uint64_t>(point_indexed_texture), 1, 4, 0, 0);
    const auto point_index_right = igpu_texture_read(static_cast<std::uint64_t>(point_indexed_texture), 6, 4, 0, 0);
    std::printf("point indexed : %lld / %lld\n", static_cast<long long>(point_index_left),
                static_cast<long long>(point_index_right));
    if (point_index_left != 255 || point_index_right != 0)
    {
        return fail("indexed point did not light only the left pixel");
    }

    const float line_vertices[] = {
        -0.875f, -0.125f, -0.375f, -0.125f, 0.375f, -0.125f, 0.875f, -0.125f,
    };
    const auto line_buffer = igpu_buffer_create(32, 0, 1, 8);
    const auto line_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto line_target_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (line_buffer == 0 || line_texture == 0 || line_target_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(line_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(line_vertices), 32)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(line_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("line color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(line_buffer), 0, static_cast<std::uint64_t>(layout),
                   2, 0, 4, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto line_left = igpu_texture_read(static_cast<std::uint64_t>(line_texture), 1, 4, 0, 0);
    const auto line_gap = igpu_texture_read(static_cast<std::uint64_t>(line_texture), 3, 4, 0, 0);
    const auto line_right = igpu_texture_read(static_cast<std::uint64_t>(line_texture), 6, 4, 0, 0);
    const auto line_off = igpu_texture_read(static_cast<std::uint64_t>(line_texture), 1, 1, 0, 0);
    std::printf("line pixels   : %lld / %lld / %lld\n", static_cast<long long>(line_left),
                static_cast<long long>(line_gap), static_cast<long long>(line_right));
    if (line_left != 255 || line_gap != 0 || line_right != 255 || line_off != 0)
    {
        return fail("line list did not keep the gap and the off-row pixel clear");
    }

    const float line_strip_vertices[] = {-0.875f, -0.125f, -0.375f, -0.125f, 0.875f, -0.125f};
    const std::uint16_t line_strip_indices[] = {0, 1, 2};
    const auto line_strip_buffer = igpu_buffer_create(24, 0, 1, 8);
    const auto line_strip_index_buffer = igpu_buffer_create(6, 0, 2, 0);
    const auto line_strip_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto line_strip_indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (line_strip_buffer == 0 || line_strip_index_buffer == 0 || line_strip_texture == 0 ||
        line_strip_indexed_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(line_strip_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(line_strip_vertices), 24)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(line_strip_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(line_strip_indices), 6)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(line_strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("line strip color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(line_strip_buffer), 0, static_cast<std::uint64_t>(layout),
                   3, 0, 2, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto line_strip_left = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 1, 4, 0, 0);
    const auto line_strip_right = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 6, 4, 0, 0);
    std::printf("line strip    : %lld / %lld\n", static_cast<long long>(line_strip_left),
                static_cast<long long>(line_strip_right));
    if (line_strip_left != 255 || line_strip_right != 0)
    {
        return fail("line strip first segment painted the right sample");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(line_strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("line strip color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(line_strip_buffer), 0, static_cast<std::uint64_t>(layout),
                   3, 1, -1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto line_strip_tail_left = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 1, 4, 0, 0);
    const auto line_strip_tail_right = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 6, 4, 0, 0);
    const auto line_strip_off = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 1, 1, 0, 0);
    std::printf("line strip tail: %lld / %lld\n", static_cast<long long>(line_strip_tail_left),
                static_cast<long long>(line_strip_tail_right));
    if (line_strip_tail_left != 255 || line_strip_tail_right != 255 || line_strip_off != 0)
    {
        return fail("line strip tail did not light the right sample on the same row");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(line_strip_indexed_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("indexed line strip color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(line_strip_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(line_strip_index_buffer), 3, 0, 3, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto line_index_left =
        igpu_texture_read(static_cast<std::uint64_t>(line_strip_indexed_texture), 1, 4, 0, 0);
    const auto line_index_right =
        igpu_texture_read(static_cast<std::uint64_t>(line_strip_indexed_texture), 6, 4, 0, 0);
    std::printf("line indexed  : %lld / %lld\n", static_cast<long long>(line_index_left),
                static_cast<long long>(line_index_right));
    if (line_index_left != 255 || line_index_right != 255)
    {
        return fail("indexed line strip did not light both samples in one draw");
    }

    const auto line_target_bytes = encode_u64(static_cast<std::uint64_t>(line_target_texture));
    const auto line_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(line_buffer), static_cast<std::uint64_t>(layout),
                                     2, 0, 2, as_array(line_target_bytes), as_array(line_zero_bytes),
                                     as_array(line_zero_bytes), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    GLint line_target_viewport[4] = {};
    GLint line_target_fbo = 0;
    glGetIntegerv(GL_VIEWPORT, line_target_viewport);
    glGetIntegerv(0x8CA6, &line_target_fbo);
    if (line_target_viewport[2] != 1 || line_target_viewport[3] != 1 || line_target_fbo != 0)
    {
        return fail("line draw-to-targets left the framebuffer or viewport bound");
    }
    const auto line_target_left = igpu_texture_read(static_cast<std::uint64_t>(line_target_texture), 1, 4, 0, 0);
    const auto line_target_right = igpu_texture_read(static_cast<std::uint64_t>(line_target_texture), 6, 4, 0, 0);
    std::printf("line target   : %lld / %lld\n", static_cast<long long>(line_target_left),
                static_cast<long long>(line_target_right));
    if (line_target_left != 255 || line_target_right != 0)
    {
        return fail("draw-to-targets line did not paint only the left sample");
    }

    if (!igpu_shader_bind(colour_vert, 0) || !igpu_shader_bind(colour_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto sample_source = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sample_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (sample_source == 0 || sample_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    std::int32_t sample_saved_fbo = 0;
    std::int32_t sample_held_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_source), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("sample source target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(colour_buffer), 0, static_cast<std::uint64_t>(colour_layout),
                   4, 0, 12, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    const auto sample_source_left = igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0);
    const auto sample_source_right = igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0);
    std::printf("sample source : %lld / %lld\n", static_cast<long long>(sample_source_left),
                static_cast<long long>(sample_source_right));
    if (sample_source_left != 255 || sample_source_right != 16711680)
    {
        return fail("sample source was not red on the left and blue on the right");
    }

    const char* sample_vs =
        "#version 120\n"
        "attribute vec2 in_pos;\n"
        "attribute vec2 in_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_Position = vec4(in_pos, 0.0, 1.0);\n"
        "  v_uv = in_uv;\n"
        "}\n";
    const char* sample_ps =
        "#version 120\n"
        "uniform sampler2D igpu_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() { gl_FragColor = texture2D(igpu_tex, v_uv); }\n";
    const auto sample_vert = igpu_shader_compile(sample_vs, "main", 0, "glsl");
    const auto sample_frag = igpu_shader_compile(sample_ps, "main", 1, "glsl");
    if (sample_vert == 0 || sample_frag == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    struct SampleVertex
    {
        float x;
        float y;
        float u;
        float v;
    };
    static_assert(sizeof(SampleVertex) == 16, "sample vertex must be tightly packed");
    const SampleVertex sample_vertices[6] = {
        {-1.f, -1.f, 0.f, 0.f}, {1.f, -1.f, 1.f, 0.f}, {-1.f, 1.f, 0.f, 1.f},
        {1.f, -1.f, 1.f, 0.f},  {1.f, 1.f, 1.f, 1.f},  {-1.f, 1.f, 0.f, 1.f},
    };
    const auto sample_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(sample_vertices)), 0, 1, 16);
    if (sample_buffer == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(sample_buffer), 0,
                           gm::wire::GMBuffer(const_cast<SampleVertex*>(sample_vertices), sizeof(sample_vertices))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto sample_usage = encode_i32_2(1, 4);
    const auto sample_type = encode_i32_2(2, 2);
    const auto sample_step = encode_i32_2(0, 0);
    const auto sample_layout = igpu_input_layout_create(
        sample_vert, as_array(sample_usage), as_array(sample_type), as_array(sample_step), 2, 16, 0);
    if (sample_layout == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(sample_vert, 0) || !igpu_shader_bind(sample_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_dest), sample_saved_fbo, sample_held_viewport))
    {
        return fail("sample dest target begin failed");
    }
    if (!igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout),
                           4, 0, 3, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    const auto sample_left = igpu_texture_read(static_cast<std::uint64_t>(sample_dest), 1, 4, 0, 0);
    const auto sample_right = igpu_texture_read(static_cast<std::uint64_t>(sample_dest), 6, 4, 0, 0);
    std::printf("sample pixels : %lld / %lld\n", static_cast<long long>(sample_left),
                static_cast<long long>(sample_right));
    if (sample_left != 255 || sample_right != 0)
    {
        return fail("sampled draw did not paint only the left texel from the texture");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_dest), sample_saved_fbo, sample_held_viewport))
    {
        return fail("sample tail target begin failed");
    }
    if (!igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout),
                           4, 3, -1, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    const auto sample_tail_left = igpu_texture_read(static_cast<std::uint64_t>(sample_dest), 1, 4, 0, 0);
    const auto sample_tail_right = igpu_texture_read(static_cast<std::uint64_t>(sample_dest), 6, 4, 0, 0);
    const auto sample_source_after_left = igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0);
    const auto sample_source_after_right = igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0);
    std::printf("sample tail   : %lld / %lld\n", static_cast<long long>(sample_tail_left),
                static_cast<long long>(sample_tail_right));
    if (sample_tail_left != 255 || sample_tail_right != 16711680)
    {
        return fail("sampled tail was not red on the left and blue on the right");
    }
    if (sample_source_after_left != 255 || sample_source_after_right != 16711680)
    {
        return fail("sampled draw changed the source texture");
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
