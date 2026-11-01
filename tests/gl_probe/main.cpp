#include "wgl_host.h"
#include "texture_tests.h"
#include "buffer_tests.h"
#include "query_tests.h"
#include "instanced_tests.h"
#include "compute_tests.h"
#include "uniform_tests.h"
#include "target_tests.h"
#include "layered_texture_tests.h"
#include "query_shutdown_tests.h"

#include "IGPU_native.h"
#include "igpu_capabilities.h"

#include "core/GMExtWire.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"

#include <Windows.h>
#include <GL/gl.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    constexpr GLenum texture_anisotropy = 0x84FE;
    constexpr GLenum device_max_anisotropy = 0x84FF;
    constexpr GLenum texture_wrap_r = 0x8072;
    constexpr GLenum texture_base_level = 0x813C;
    constexpr GLenum texture_max_level = 0x813D;
    constexpr GLenum texture_lod_bias = 0x8501;
    constexpr GLenum texture_min_lod = 0x813A;
    constexpr GLenum texture_max_lod = 0x813B;

    struct TextureParameters
    {
        std::array<GLint, 7> integers{};
        std::array<GLfloat, 8> floats{};

        bool operator==(const TextureParameters& other) const
        {
            return integers == other.integers && floats == other.floats;
        }
    };

    constexpr GLenum integer_texture_parameters[] = {
        GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, texture_wrap_r, GL_TEXTURE_MIN_FILTER,
        GL_TEXTURE_MAG_FILTER, texture_base_level, texture_max_level,
    };
    constexpr GLenum float_texture_parameters[] = {
        texture_anisotropy, texture_lod_bias, texture_min_lod, texture_max_lod,
    };

    TextureParameters bound_texture_parameters()
    {
        TextureParameters result;
        for (std::size_t i = 0; i < result.integers.size(); ++i)
        {
            glGetTexParameteriv(GL_TEXTURE_2D, integer_texture_parameters[i], &result.integers[i]);
        }
        for (std::size_t i = 0; i < 4; ++i)
        {
            glGetTexParameterfv(GL_TEXTURE_2D, float_texture_parameters[i], &result.floats[i]);
        }
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, result.floats.data() + 4);
        return result;
    }

    TextureParameters texture_parameters(GLuint texture)
    {
        GLint previous = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glBindTexture(GL_TEXTURE_2D, texture);
        const auto result = bound_texture_parameters();
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
        return result;
    }

    void set_texture_parameters(GLuint texture, const TextureParameters& parameters)
    {
        GLint previous = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glBindTexture(GL_TEXTURE_2D, texture);
        for (std::size_t i = 0; i < parameters.integers.size(); ++i)
        {
            glTexParameteri(GL_TEXTURE_2D, integer_texture_parameters[i], parameters.integers[i]);
        }
        for (std::size_t i = 0; i < 4; ++i)
        {
            glTexParameterf(GL_TEXTURE_2D, float_texture_parameters[i], parameters.floats[i]);
        }
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, parameters.floats.data() + 4);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
    }

    // Sampler draw-boundary observer: Uniform1i follows sampler application;
    // the remaining vertex setup does not alter texture parameters. Forward to real GL.
    struct SamplerDrawObserver
    {
        static SamplerDrawObserver* active;
        IgpuGlUniform1i forward = nullptr;
        unsigned calls = 0;
        GLint texture = 0;
        TextureParameters parameters;

        static void APIENTRY observe(GLint location, GLint value)
        {
            auto& observer = *active;
            ++observer.calls;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &observer.texture);
            observer.parameters = bound_texture_parameters();
            observer.forward(location, value);
        }

        bool install()
        {
            if (active != nullptr || igpu_gl_fns().Uniform1i == nullptr)
            {
                return false;
            }
            forward = igpu_gl_fns().Uniform1i;
            active = this;
            igpu_gl_fns().Uniform1i = &observe;
            return true;
        }

        bool stop()
        {
            if (active == this)
            {
                igpu_gl_fns().Uniform1i = forward;
                active = nullptr;
            }
            return true;
        }

        ~SamplerDrawObserver() { stop(); }
    };
    SamplerDrawObserver* SamplerDrawObserver::active = nullptr;

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
        return fail("vertex source was accepted as a compute shader");
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
        {4, true, "ShaderStageCompute"},
        {20, true, "Texture3D"},
        {21, true, "TextureArray"},
        {22, true, "TextureCubemap"},
        {26, true, "Texture2D"},
        {47, true, "InputLayout"},
        {48, true, "VertexBuffer"},
        {49, true, "IndexBuffer"},
        {50, true, "UniformBuffer"},
        {53, true, "Draw"},
        {54, true, "DrawIndexed"},
        {55, true, "DrawStateRestore"},
        {63, true, "UniformReflection"},
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
    const auto dynamic_indices = igpu_buffer_create(24, 1, 2, 0);
    if (!dynamic_indices || !igpu_buffer_release(dynamic_indices) || igpu_buffer_create(24, 0, 2, 2) != 0 ||
        igpu_buffer_create(23, 0, 2, 0) != 0)
    {
        return fail("dynamic index creation or invalid index description validation failed");
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
        !reject_strip(5, 1, 0, 0, 0, "strip blend", "unknown blend handle") ||
        !reject_strip(5, 0, 1, 0, 0, "strip depth", "unknown depth handle") ||
        !reject_strip(5, 0, 0, 1, 0, "strip raster", "unknown raster handle") ||
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

    const SampleVertex nearest_vertices[6] = {
        {-1.f, -1.f, 0.484375f, 0.5f}, {1.f, -1.f, 0.484375f, 0.5f}, {-1.f, 1.f, 0.484375f, 0.5f},
        {1.f, -1.f, 0.484375f, 0.5f},  {1.f, 1.f, 0.484375f, 0.5f},  {-1.f, 1.f, 0.484375f, 0.5f},
    };
    const auto nearest_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(nearest_vertices)), 0, 1, 16);
    const auto nearest_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (nearest_buffer == 0 || nearest_dest == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(nearest_buffer), 0,
                           gm::wire::GMBuffer(const_cast<SampleVertex*>(nearest_vertices), sizeof(nearest_vertices))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(nearest_dest), sample_saved_fbo, sample_held_viewport))
    {
        return fail("nearest target begin failed");
    }
    if (!igpu_draw_sampled(static_cast<std::uint64_t>(nearest_buffer), static_cast<std::uint64_t>(sample_layout),
                           4, 0, 6, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    const auto nearest_left = igpu_texture_read(static_cast<std::uint64_t>(nearest_dest), 1, 4, 0, 0);
    const auto nearest_right = igpu_texture_read(static_cast<std::uint64_t>(nearest_dest), 6, 4, 0, 0);
    std::printf("sample nearest: %lld / %lld\n", static_cast<long long>(nearest_left),
                static_cast<long long>(nearest_right));
    if (nearest_left != 255 || nearest_right != 255)
    {
        return fail("nearest sample at u=0.484375 was not pure red");
    }

    GLint sample_bound_texture = 1;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &sample_bound_texture);
    if (sample_bound_texture != 0)
    {
        return fail("sampled draw left a texture bound");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_source), sample_saved_fbo, sample_held_viewport))
    {
        return fail("feedback target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout),
                          4, 0, 6, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("sampling the current color target was accepted");
    }
    if (std::string(igpu_get_last_error()).find("the texture is the current color target") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("rejected feedback draw changed the source texture");
    }

    const auto sample_reject = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (sample_reject == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_reject), sample_saved_fbo, sample_held_viewport))
    {
        return fail("reject target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout),
                          4, 0, 6, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("a program without igpu_tex was accepted");
    }
    if (std::string(igpu_get_last_error()).find("no sampler uniform 'igpu_tex'") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_reject), 1, 4, 0, 0) != 0 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_reject), 6, 4, 0, 0) != 0)
    {
        return fail("rejected sample changed pixels");
    }

    if (!igpu_shader_bind(sample_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_reject), sample_saved_fbo, sample_held_viewport))
    {
        return fail("state reject target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout),
                          4, 0, 6, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 99) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("an unknown sampler handle was not rejected");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout),
                          4, 0, 6, 0, 0, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown texture handle") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("texture 0 was not rejected");
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_reject), 1, 4, 0, 0) != 0 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_reject), 6, 4, 0, 0) != 0)
    {
        return fail("rejected sampler handle or texture 0 changed pixels");
    }

    const auto stride_zero = igpu_buffer_create(96, 0, 1, 0);
    if (stride_zero == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_reject), sample_saved_fbo, sample_held_viewport))
    {
        return fail("stride reject target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(stride_zero), static_cast<std::uint64_t>(sample_layout),
                          4, 0, 6, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("stride does not match") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("stride 0 buffer was accepted as texcoord data");
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_reject), 1, 4, 0, 0) != 0)
    {
        return fail("rejected stride 0 sample changed pixels");
    }

    const auto wrap_repeat = igpu_sampler_state_create(0, 0, 0, 1, 1, 1, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto wrap_clamp = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (wrap_repeat == 0 || wrap_clamp == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_buffer = igpu_buffer_create(96, 0, 1, 16);
    const auto wrap_repeat_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_clamp_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_restored_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_negative_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_edge_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_nearest_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_black_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (wrap_buffer == 0 || wrap_repeat_dest == 0 || wrap_clamp_dest == 0 || wrap_restored_dest == 0 ||
        wrap_negative_dest == 0 || wrap_edge_dest == 0 || wrap_nearest_dest == 0 || wrap_black_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto write_uv = [&](float u, float v) -> bool {
        SampleVertex verts[6] = {
            {-1.f, -1.f, u, v}, {1.f, -1.f, u, v}, {-1.f, 1.f, u, v},
            {1.f, -1.f, u, v},  {1.f, 1.f, u, v},  {-1.f, 1.f, u, v},
        };
        return igpu_buffer_write(static_cast<std::uint64_t>(wrap_buffer), 0,
                                 gm::wire::GMBuffer(verts, sizeof(verts)));
    };
    const auto sample_uv = [&](std::int64_t dest, std::int64_t sampler) -> bool {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(dest), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw_sampled(
            static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
            static_cast<std::uint64_t>(sample_source), 0, 0, 0, sampler);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    if (!igpu_shader_bind(sample_vert, 0) || !igpu_shader_bind(sample_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!write_uv(1.0625f, 0.5f) || !sample_uv(wrap_repeat_dest, wrap_repeat))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_repeat_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_repeat_dest), 1, 4, 0, 0);
    const auto wrap_repeat_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_repeat_dest), 6, 4, 0, 0);
    std::printf("wrap repeat  : %lld / %lld\n", static_cast<long long>(wrap_repeat_left),
                static_cast<long long>(wrap_repeat_right));
    if (wrap_repeat_left != 255 || wrap_repeat_right != 255)
    {
        return fail("repeat sample at u=1.0625 was not pure red");
    }
    if (!sample_uv(wrap_clamp_dest, wrap_clamp))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_clamp_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_clamp_dest), 1, 4, 0, 0);
    const auto wrap_clamp_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_clamp_dest), 6, 4, 0, 0);
    std::printf("wrap clamp   : %lld / %lld\n", static_cast<long long>(wrap_clamp_left),
                static_cast<long long>(wrap_clamp_right));
    if (wrap_clamp_left != 16711680 || wrap_clamp_right != 16711680)
    {
        return fail("clamp sample at u=1.0625 was not pure blue");
    }
    if (!sample_uv(wrap_restored_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_restored_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_restored_dest), 1, 4, 0, 0);
    const auto wrap_restored_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_restored_dest), 6, 4, 0, 0);
    std::printf("wrap restored: %lld / %lld\n", static_cast<long long>(wrap_restored_left),
                static_cast<long long>(wrap_restored_right));
    if (wrap_restored_left != 16711680 || wrap_restored_right != 16711680)
    {
        return fail("sampler 0 did not stay clamped after a repeat draw");
    }
    if (!write_uv(-0.0625f, 0.5f) || !sample_uv(wrap_negative_dest, wrap_repeat))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_negative_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_negative_dest), 1, 4, 0, 0);
    const auto wrap_negative_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_negative_dest), 6, 4, 0, 0);
    std::printf("wrap negative: %lld / %lld\n", static_cast<long long>(wrap_negative_left),
                static_cast<long long>(wrap_negative_right));
    if (wrap_negative_left != 16711680 || wrap_negative_right != 16711680)
    {
        return fail("repeat sample at u=-0.0625 was not pure blue");
    }
    if (!sample_uv(wrap_edge_dest, wrap_clamp))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_edge_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_edge_dest), 1, 4, 0, 0);
    const auto wrap_edge_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_edge_dest), 6, 4, 0, 0);
    std::printf("wrap edge    : %lld / %lld\n", static_cast<long long>(wrap_edge_left),
                static_cast<long long>(wrap_edge_right));
    if (wrap_edge_left != 255 || wrap_edge_right != 255)
    {
        return fail("clamp sample at u=-0.0625 was not pure red");
    }
    if (!write_uv(0.484375f, 0.5f) || !sample_uv(wrap_nearest_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_nearest_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_nearest_dest), 1, 4, 0, 0);
    const auto wrap_nearest_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_nearest_dest), 6, 4, 0, 0);
    std::printf("wrap nearest : %lld / %lld\n", static_cast<long long>(wrap_nearest_left),
                static_cast<long long>(wrap_nearest_right));
    if (wrap_nearest_left != 255 || wrap_nearest_right != 255)
    {
        return fail("sampler 0 at u=0.484375 was not pure red after wrap draws");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("wrap draws changed the source texture");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(wrap_black_dest), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("wrap reject target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                          static_cast<std::uint64_t>(sample_source), 0, 0, 0, 99) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("unknown sampler 99 was accepted");
    }
    if (igpu_draw(static_cast<std::uint64_t>(wrap_buffer), 0, static_cast<std::uint64_t>(sample_layout), 4, 0, 6, 1,
                  0, 0, 0, wrap_repeat) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("igpu_draw accepted a sampler");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_index_buffer), 4, 0, 6, 0, 0, 0, wrap_repeat) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("igpu_draw_indexed accepted a sampler");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                          static_cast<std::uint64_t>(sample_source), 1, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown blend handle") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("a sampler handle was accepted as a sampled blend");
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 1, 4, 0, 0) != 0 ||
        igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 6, 4, 0, 0) != 0)
    {
        return fail("rejected wrap draw changed pixels");
    }
    if (!write_uv(1.0625f, 0.5f) || !sample_uv(wrap_restored_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_restored_dest), 1, 4, 0, 0) != 16711680)
    {
        return fail("unknown sampler 99 left the texture repeating");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_source), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("wrap feedback target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                          static_cast<std::uint64_t>(sample_source), 0, 0, 0, wrap_repeat) ||
        std::string(igpu_get_last_error()).find("the texture is the current color target") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("repeat sampler was allowed to sample the current color target");
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("rejected repeat feedback changed the source texture");
    }

    const auto linear_clamp = igpu_sampler_state_create(1, 0, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto linear_repeat = igpu_sampler_state_create(1, 0, 0, 1, 1, 1, 1, 0, 0, 0.f, 0.f, -1.f);
    if (linear_clamp == 0 || linear_repeat == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto linear_seam_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto linear_repeat_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto linear_edge_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto linear_point_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto linear_wrap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (linear_seam_dest == 0 || linear_repeat_dest == 0 || linear_edge_dest == 0 || linear_point_dest == 0 ||
        linear_wrap_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!write_uv(0.5f, 0.5f) || !sample_uv(linear_seam_dest, linear_clamp))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto linear_seam_left = igpu_texture_read(static_cast<std::uint64_t>(linear_seam_dest), 1, 4, 0, 0);
    const auto linear_seam_right = igpu_texture_read(static_cast<std::uint64_t>(linear_seam_dest), 6, 4, 0, 0);
    std::printf("linear seam  : %lld / %lld\n", static_cast<long long>(linear_seam_left),
                static_cast<long long>(linear_seam_right));
    if (linear_seam_left != 8388736 || linear_seam_right != 8388736)
    {
        return fail("linear magnification at u=0.5 was not half red and half blue");
    }
    if (!write_uv(1.5f, 0.5f) || !sample_uv(linear_repeat_dest, linear_repeat))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto linear_repeat_left = igpu_texture_read(static_cast<std::uint64_t>(linear_repeat_dest), 1, 4, 0, 0);
    const auto linear_repeat_right = igpu_texture_read(static_cast<std::uint64_t>(linear_repeat_dest), 6, 4, 0, 0);
    std::printf("linear repeat: %lld / %lld\n", static_cast<long long>(linear_repeat_left),
                static_cast<long long>(linear_repeat_right));
    if (linear_repeat_left != 8388736 || linear_repeat_right != 8388736)
    {
        return fail("repeat plus linear at u=1.5 was not the interior half blend");
    }
    if (!sample_uv(linear_edge_dest, linear_clamp))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto linear_edge_left = igpu_texture_read(static_cast<std::uint64_t>(linear_edge_dest), 1, 4, 0, 0);
    const auto linear_edge_right = igpu_texture_read(static_cast<std::uint64_t>(linear_edge_dest), 6, 4, 0, 0);
    std::printf("linear edge  : %lld / %lld\n", static_cast<long long>(linear_edge_left),
                static_cast<long long>(linear_edge_right));
    if (linear_edge_left != 16711680 || linear_edge_right != 16711680)
    {
        return fail("clamp plus linear at u=1.5 was not the edge texel");
    }
    if (!write_uv(0.484375f, 0.5f) || !sample_uv(linear_point_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto linear_point_left = igpu_texture_read(static_cast<std::uint64_t>(linear_point_dest), 1, 4, 0, 0);
    const auto linear_point_right = igpu_texture_read(static_cast<std::uint64_t>(linear_point_dest), 6, 4, 0, 0);
    std::printf("linear point : %lld / %lld\n", static_cast<long long>(linear_point_left),
                static_cast<long long>(linear_point_right));
    if (linear_point_left != 255 || linear_point_right != 255)
    {
        return fail("sampler 0 at u=0.484375 was not pure red after linear draws");
    }
    if (!write_uv(1.0625f, 0.5f) || !sample_uv(linear_wrap_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto linear_wrap_left = igpu_texture_read(static_cast<std::uint64_t>(linear_wrap_dest), 1, 4, 0, 0);
    const auto linear_wrap_right = igpu_texture_read(static_cast<std::uint64_t>(linear_wrap_dest), 6, 4, 0, 0);
    std::printf("linear wrap  : %lld / %lld\n", static_cast<long long>(linear_wrap_left),
                static_cast<long long>(linear_wrap_right));
    if (linear_wrap_left != 16711680 || linear_wrap_right != 16711680)
    {
        return fail("sampler 0 at u=1.0625 was not pure blue after linear draws");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("linear draws changed the source texture");
    }
    std::int32_t linear_saved = 0;
    std::int32_t linear_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(wrap_black_dest), linear_saved, linear_viewport))
    {
        return fail("linear reject target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(wrap_buffer), 0, static_cast<std::uint64_t>(sample_layout), 4, 0, 6, 1, 0,
                  0, 0, linear_clamp) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(linear_saved, linear_viewport);
        return fail("igpu_draw accepted a linear sampler");
    }
    igpu::gl_color_target_end(linear_saved, linear_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 1, 4, 0, 0) != 0)
    {
        return fail("rejected linear draw changed pixels");
    }
    const auto wrap_mirror = igpu_sampler_state_create(0, 0, 0, 2, 2, 2, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto linear_mirror = igpu_sampler_state_create(1, 0, 0, 2, 2, 2, 1, 0, 0, 0.f, 0.f, -1.f);
    if (wrap_mirror == 0 || linear_mirror == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_nearest_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mirror_repeat_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mirror_clamp_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mirror_negative_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mirror_point_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mirror_linear_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mirror_restore_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mirror_plain_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (mirror_nearest_dest == 0 || mirror_repeat_dest == 0 || mirror_clamp_dest == 0 ||
        mirror_negative_dest == 0 || mirror_point_dest == 0 || mirror_linear_dest == 0 ||
        mirror_restore_dest == 0 || mirror_plain_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(sample_vert, 0) || !igpu_shader_bind(sample_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!write_uv(1.6f, 0.5f) || !sample_uv(mirror_nearest_dest, wrap_mirror))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_nearest_left = igpu_texture_read(static_cast<std::uint64_t>(mirror_nearest_dest), 1, 4, 0, 0);
    const auto mirror_nearest_right = igpu_texture_read(static_cast<std::uint64_t>(mirror_nearest_dest), 6, 4, 0, 0);
    std::printf("mirror nearest: %lld / %lld\n", static_cast<long long>(mirror_nearest_left),
                static_cast<long long>(mirror_nearest_right));
    if (mirror_nearest_left != 255 || mirror_nearest_right != 255)
    {
        return fail("mirror at u=1.6 was not the red texel");
    }
    if (!sample_uv(mirror_repeat_dest, wrap_repeat))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_repeat_left = igpu_texture_read(static_cast<std::uint64_t>(mirror_repeat_dest), 1, 4, 0, 0);
    const auto mirror_repeat_right = igpu_texture_read(static_cast<std::uint64_t>(mirror_repeat_dest), 6, 4, 0, 0);
    std::printf("mirror repeat : %lld / %lld\n", static_cast<long long>(mirror_repeat_left),
                static_cast<long long>(mirror_repeat_right));
    if (mirror_repeat_left != 16711680 || mirror_repeat_right != 16711680)
    {
        return fail("repeat at u=1.6 was not the blue texel");
    }
    if (!sample_uv(mirror_clamp_dest, wrap_clamp))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_clamp_left = igpu_texture_read(static_cast<std::uint64_t>(mirror_clamp_dest), 1, 4, 0, 0);
    const auto mirror_clamp_right = igpu_texture_read(static_cast<std::uint64_t>(mirror_clamp_dest), 6, 4, 0, 0);
    std::printf("mirror clamp  : %lld / %lld\n", static_cast<long long>(mirror_clamp_left),
                static_cast<long long>(mirror_clamp_right));
    if (mirror_clamp_left != 16711680 || mirror_clamp_right != 16711680)
    {
        return fail("clamp at u=1.6 was not the edge texel");
    }
    if (!write_uv(-0.625f, 0.5f) || !sample_uv(mirror_negative_dest, wrap_mirror))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_negative_left = igpu_texture_read(static_cast<std::uint64_t>(mirror_negative_dest), 1, 4, 0, 0);
    const auto mirror_negative_right = igpu_texture_read(static_cast<std::uint64_t>(mirror_negative_dest), 6, 4, 0, 0);
    std::printf("mirror negative: %lld / %lld\n", static_cast<long long>(mirror_negative_left),
                static_cast<long long>(mirror_negative_right));
    if (mirror_negative_left != 16711680 || mirror_negative_right != 16711680)
    {
        return fail("mirror at u=-0.625 was not the blue texel");
    }
    if (!write_uv(0.484375f, 0.5f) || !sample_uv(mirror_point_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_point_left = igpu_texture_read(static_cast<std::uint64_t>(mirror_point_dest), 1, 4, 0, 0);
    const auto mirror_point_right = igpu_texture_read(static_cast<std::uint64_t>(mirror_point_dest), 6, 4, 0, 0);
    std::printf("mirror point  : %lld / %lld\n", static_cast<long long>(mirror_point_left),
                static_cast<long long>(mirror_point_right));
    if (mirror_point_left != 255 || mirror_point_right != 255)
    {
        return fail("sampler 0 at u=0.484375 was not pure red after nearest mirror");
    }
    if (!write_uv(1.5f, 0.5f) || !sample_uv(mirror_linear_dest, linear_mirror))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_linear_left = igpu_texture_read(static_cast<std::uint64_t>(mirror_linear_dest), 1, 4, 0, 0);
    const auto mirror_linear_right = igpu_texture_read(static_cast<std::uint64_t>(mirror_linear_dest), 6, 4, 0, 0);
    std::printf("mirror linear : %lld / %lld\n", static_cast<long long>(mirror_linear_left),
                static_cast<long long>(mirror_linear_right));
    if (mirror_linear_left != 8388736 || mirror_linear_right != 8388736)
    {
        return fail("linear mirror at u=1.5 was not half red and half blue");
    }
    std::int32_t mirror_saved = 0;
    std::int32_t mirror_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(wrap_black_dest), mirror_saved, mirror_viewport))
    {
        return fail("mirror reject target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(wrap_buffer), 0, static_cast<std::uint64_t>(sample_layout), 4, 0, 6, 1,
                  0, 0, 0, wrap_mirror) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(mirror_saved, mirror_viewport);
        return fail("igpu_draw accepted a mirror sampler");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_index_buffer), 4, 0, 6, 0, 0, 0, wrap_mirror) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(mirror_saved, mirror_viewport);
        return fail("igpu_draw_indexed accepted a mirror sampler");
    }
    const auto mirror_target_bytes = encode_u64(static_cast<std::uint64_t>(wrap_black_dest));
    const auto mirror_zero_bytes = encode_u64(0);
    if (igpu_draw_to_render_targets(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout),
                                    4, 0, 6, as_array(mirror_target_bytes), as_array(mirror_zero_bytes),
                                    as_array(mirror_zero_bytes), 0, 0, 0, wrap_mirror) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(mirror_saved, mirror_viewport);
        return fail("igpu_draw_to_render_targets accepted a mirror sampler");
    }
    igpu::gl_color_target_end(mirror_saved, mirror_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 1, 4, 0, 0) != 0 ||
        igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 6, 4, 0, 0) != 0)
    {
        return fail("rejected mirror draw changed pixels");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_source), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("mirror feedback target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                          static_cast<std::uint64_t>(sample_source), 0, 0, 0, wrap_mirror) ||
        std::string(igpu_get_last_error()).find("the texture is the current color target") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("mirror sampler was allowed to sample the current color target");
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("rejected mirror feedback changed the source texture");
    }
    if (!write_uv(1.6f, 0.5f) || !sample_uv(mirror_restore_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_restore_left = igpu_texture_read(static_cast<std::uint64_t>(mirror_restore_dest), 1, 4, 0, 0);
    const auto mirror_restore_right = igpu_texture_read(static_cast<std::uint64_t>(mirror_restore_dest), 6, 4, 0, 0);
    std::printf("mirror restore: %lld / %lld\n", static_cast<long long>(mirror_restore_left),
                static_cast<long long>(mirror_restore_right));
    if (mirror_restore_left != 16711680 || mirror_restore_right != 16711680)
    {
        return fail("sampler 0 at u=1.6 was not the edge texel after mirror draws");
    }
    if (!write_uv(0.484375f, 0.5f) || !sample_uv(mirror_plain_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mirror_plain_left = igpu_texture_read(static_cast<std::uint64_t>(mirror_plain_dest), 1, 4, 0, 0);
    const auto mirror_plain_right = igpu_texture_read(static_cast<std::uint64_t>(mirror_plain_dest), 6, 4, 0, 0);
    std::printf("mirror plain  : %lld / %lld\n", static_cast<long long>(mirror_plain_left),
                static_cast<long long>(mirror_plain_right));
    if (mirror_plain_left != 255 || mirror_plain_right != 255)
    {
        return fail("sampler 0 at u=0.484375 was not pure red after linear mirror");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("mirror draws changed the source texture");
    }
    const auto axes_repeat = igpu_sampler_state_create(0, 0, 0, 1, 0, 2, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto axes_tall = igpu_sampler_state_create(0, 0, 0, 0, 1, 2, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto axes_mirror = igpu_sampler_state_create(0, 0, 0, 2, 1, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto axes_low = igpu_sampler_state_create(0, 0, 0, 2, 0, 1, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto axes_wide = igpu_sampler_state_create(0, 0, 0, 0, 0, 1, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto axes_linear = igpu_sampler_state_create(1, 0, 0, 2, 1, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (axes_repeat == 0 || axes_tall == 0 || axes_mirror == 0 || axes_low == 0 || axes_wide == 0 ||
        axes_linear == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_unknown = igpu_sampler_state_create(0, 0, 0, 4, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (axes_unknown != 0 ||
        std::string(igpu_get_last_error()).find("clamp, repeat, or mirror") == std::string::npos)
    {
        return fail("an unknown address mode was accepted");
    }
    const auto axes_tall_source = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_repeat_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_tall_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_mirror_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_low_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_wide_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_linear_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_restore_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_wrap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_plain_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto axes_reject_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (axes_tall_source == 0 || axes_repeat_dest == 0 || axes_tall_dest == 0 || axes_mirror_dest == 0 ||
        axes_low_dest == 0 || axes_wide_dest == 0 || axes_linear_dest == 0 || axes_restore_dest == 0 ||
        axes_wrap_dest == 0 || axes_plain_dest == 0 || axes_reject_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const float axes_pos[] = {
        -1.f, -1.f, 1.f, -1.f, -1.f, 0.f, 1.f, -1.f, 1.f, 0.f, -1.f, 0.f,
        -1.f, 0.f, 1.f, 0.f, -1.f, 1.f, 1.f, 0.f, 1.f, 1.f, -1.f, 1.f,
    };
    ColourVertex axes_vertices[12] = {};
    for (int i = 0; i < 12; ++i)
    {
        axes_vertices[i].x = axes_pos[i * 2];
        axes_vertices[i].y = axes_pos[i * 2 + 1];
        const bool bottom = i < 6;
        axes_vertices[i].r = bottom ? 255 : 0;
        axes_vertices[i].g = 0;
        axes_vertices[i].b = bottom ? 0 : 255;
        axes_vertices[i].a = 255;
    }
    const auto axes_colour = igpu_buffer_create(static_cast<std::int64_t>(sizeof(axes_vertices)), 0, 1, 12);
    if (axes_colour == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(axes_colour), 0,
                           gm::wire::GMBuffer(axes_vertices, sizeof(axes_vertices))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(colour_vert, 0) || !igpu_shader_bind(colour_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(axes_tall_source), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("vertical source target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(axes_colour), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 12,
                   1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(axes_tall_source), 1, 7, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(axes_tall_source), 1, 0, 0, 0) != 16711680)
    {
        return fail("vertical source was not red on the bottom and blue on the top");
    }
    if (!igpu_shader_bind(sample_vert, 0) || !igpu_shader_bind(sample_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto sample_from = [&](std::int64_t dest, std::int64_t sampler, std::int64_t source) -> bool {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(dest), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw_sampled(
            static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
            static_cast<std::uint64_t>(source), 0, 0, 0, sampler);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    if (!write_uv(1.0625f, 0.5f) || !sample_from(axes_repeat_dest, axes_repeat, sample_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_repeat_left = igpu_texture_read(static_cast<std::uint64_t>(axes_repeat_dest), 1, 4, 0, 0);
    const auto axes_repeat_right = igpu_texture_read(static_cast<std::uint64_t>(axes_repeat_dest), 6, 4, 0, 0);
    std::printf("axes repeat  : %lld / %lld\n", static_cast<long long>(axes_repeat_left),
                static_cast<long long>(axes_repeat_right));
    if (axes_repeat_left != 255 || axes_repeat_right != 255)
    {
        return fail("repeat on U at u=1.0625 was not pure red");
    }
    if (!write_uv(0.5f, 1.0625f) || !sample_from(axes_tall_dest, axes_tall, axes_tall_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_tall_left = igpu_texture_read(static_cast<std::uint64_t>(axes_tall_dest), 1, 4, 0, 0);
    const auto axes_tall_right = igpu_texture_read(static_cast<std::uint64_t>(axes_tall_dest), 6, 4, 0, 0);
    std::printf("axes tall    : %lld / %lld\n", static_cast<long long>(axes_tall_left),
                static_cast<long long>(axes_tall_right));
    if (axes_tall_left != 255 || axes_tall_right != 255)
    {
        return fail("repeat on V at v=1.0625 was not the bottom texel");
    }
    if (!write_uv(1.6f, 0.5f) || !sample_from(axes_mirror_dest, axes_mirror, sample_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_mirror_left = igpu_texture_read(static_cast<std::uint64_t>(axes_mirror_dest), 1, 4, 0, 0);
    const auto axes_mirror_right = igpu_texture_read(static_cast<std::uint64_t>(axes_mirror_dest), 6, 4, 0, 0);
    std::printf("axes mirror  : %lld / %lld\n", static_cast<long long>(axes_mirror_left),
                static_cast<long long>(axes_mirror_right));
    if (axes_mirror_left != 255 || axes_mirror_right != 255)
    {
        return fail("mirror on U at u=1.6 was not pure red");
    }
    if (!write_uv(0.5f, -0.625f) || !sample_from(axes_low_dest, axes_low, axes_tall_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_low_left = igpu_texture_read(static_cast<std::uint64_t>(axes_low_dest), 1, 4, 0, 0);
    const auto axes_low_right = igpu_texture_read(static_cast<std::uint64_t>(axes_low_dest), 6, 4, 0, 0);
    std::printf("axes low     : %lld / %lld\n", static_cast<long long>(axes_low_left),
                static_cast<long long>(axes_low_right));
    if (axes_low_left != 255 || axes_low_right != 255)
    {
        return fail("clamp on V at v=-0.625 was not the bottom texel");
    }
    if (!write_uv(0.5f, -0.0625f) || !sample_from(axes_wide_dest, axes_wide, axes_tall_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_wide_left = igpu_texture_read(static_cast<std::uint64_t>(axes_wide_dest), 1, 4, 0, 0);
    const auto axes_wide_right = igpu_texture_read(static_cast<std::uint64_t>(axes_wide_dest), 6, 4, 0, 0);
    std::printf("axes wide    : %lld / %lld\n", static_cast<long long>(axes_wide_left),
                static_cast<long long>(axes_wide_right));
    if (axes_wide_left != 255 || axes_wide_right != 255)
    {
        return fail("repeat on W changed the T wrap");
    }
    if (!write_uv(1.5f, 0.5f) || !sample_from(axes_linear_dest, axes_linear, sample_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_linear_left = igpu_texture_read(static_cast<std::uint64_t>(axes_linear_dest), 1, 4, 0, 0);
    const auto axes_linear_right = igpu_texture_read(static_cast<std::uint64_t>(axes_linear_dest), 6, 4, 0, 0);
    std::printf("axes linear  : %lld / %lld\n", static_cast<long long>(axes_linear_left),
                static_cast<long long>(axes_linear_right));
    if (axes_linear_left != 8388736 || axes_linear_right != 8388736)
    {
        return fail("linear mirror on U at u=1.5 was not half red and half blue");
    }
    if (!write_uv(0.5f, 1.0625f) || !sample_from(axes_restore_dest, 0, axes_tall_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_restore_left = igpu_texture_read(static_cast<std::uint64_t>(axes_restore_dest), 1, 4, 0, 0);
    const auto axes_restore_right = igpu_texture_read(static_cast<std::uint64_t>(axes_restore_dest), 6, 4, 0, 0);
    std::printf("axes restore : %lld / %lld\n", static_cast<long long>(axes_restore_left),
                static_cast<long long>(axes_restore_right));
    if (axes_restore_left != 16711680 || axes_restore_right != 16711680)
    {
        return fail("sampler 0 at v=1.0625 was not the top texel");
    }
    if (!write_uv(1.6f, 0.5f) || !sample_from(axes_wrap_dest, 0, sample_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_wrap_left = igpu_texture_read(static_cast<std::uint64_t>(axes_wrap_dest), 1, 4, 0, 0);
    const auto axes_wrap_right = igpu_texture_read(static_cast<std::uint64_t>(axes_wrap_dest), 6, 4, 0, 0);
    std::printf("axes wrap    : %lld / %lld\n", static_cast<long long>(axes_wrap_left),
                static_cast<long long>(axes_wrap_right));
    if (axes_wrap_left != 16711680 || axes_wrap_right != 16711680)
    {
        return fail("sampler 0 at u=1.6 was not the edge texel");
    }
    if (!write_uv(0.484375f, 0.5f) || !sample_from(axes_plain_dest, 0, sample_source))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto axes_plain_left = igpu_texture_read(static_cast<std::uint64_t>(axes_plain_dest), 1, 4, 0, 0);
    const auto axes_plain_right = igpu_texture_read(static_cast<std::uint64_t>(axes_plain_dest), 6, 4, 0, 0);
    std::printf("axes plain   : %lld / %lld\n", static_cast<long long>(axes_plain_left),
                static_cast<long long>(axes_plain_right));
    if (axes_plain_left != 255 || axes_plain_right != 255)
    {
        return fail("sampler 0 at u=0.484375 was not pure red after linear mixed axes");
    }
    std::int32_t axes_saved = 0;
    std::int32_t axes_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(axes_reject_dest), axes_saved, axes_viewport))
    {
        return fail("axes reject target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(wrap_buffer), 0, static_cast<std::uint64_t>(sample_layout), 4, 0, 6, 1,
                  0, 0, 0, axes_repeat) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(axes_saved, axes_viewport);
        return fail("igpu_draw accepted a mixed-axis sampler");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_index_buffer), 4, 0, 6, 0, 0, 0, axes_repeat) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(axes_saved, axes_viewport);
        return fail("igpu_draw_indexed accepted a mixed-axis sampler");
    }
    const auto axes_target_bytes = encode_u64(static_cast<std::uint64_t>(axes_reject_dest));
    const auto axes_zero_bytes = encode_u64(0);
    if (igpu_draw_to_render_targets(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout),
                                    4, 0, 6, as_array(axes_target_bytes), as_array(axes_zero_bytes),
                                    as_array(axes_zero_bytes), 0, 0, 0, axes_repeat) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(axes_saved, axes_viewport);
        return fail("igpu_draw_to_render_targets accepted a mixed-axis sampler");
    }
    igpu::gl_color_target_end(axes_saved, axes_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(axes_reject_dest), 1, 4, 0, 0) != 0 ||
        igpu_texture_read(static_cast<std::uint64_t>(axes_reject_dest), 6, 4, 0, 0) != 0)
    {
        return fail("rejected mixed-axis draw changed pixels");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(axes_tall_source), 1, 7, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(axes_tall_source), 1, 0, 0, 0) != 16711680 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("mixed-axis draws changed a source texture");
    }
    const auto border_green = igpu_sampler_state_create(0, 0, 0, 3, 3, 3, 1, 65280, 0, 0.f, 0.f, -1.f);
    const auto border_red = igpu_sampler_state_create(0, 0, 0, 3, 3, 3, 1, 255, 0, 0.f, 0.f, -1.f);
    const auto border_linear = igpu_sampler_state_create(1, 0, 0, 3, 3, 3, 1, 65280, 0, 0.f, 0.f, -1.f);
    if (border_green == 0 || border_red == 0 || border_linear == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_outside_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_negative_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_red_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_clamp_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_repeat_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_linear_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_restore_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_plain_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_outside_dest == 0 || border_negative_dest == 0 || border_red_dest == 0 ||
        border_clamp_dest == 0 || border_repeat_dest == 0 || border_linear_dest == 0 ||
        border_restore_dest == 0 || border_plain_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(sample_vert, 0) || !igpu_shader_bind(sample_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!write_uv(1.6f, 0.5f) || !sample_uv(border_outside_dest, border_green))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_outside_left = igpu_texture_read(static_cast<std::uint64_t>(border_outside_dest), 1, 4, 0, 0);
    const auto border_outside_right = igpu_texture_read(static_cast<std::uint64_t>(border_outside_dest), 6, 4, 0, 0);
    std::printf("border outside: %lld / %lld\n", static_cast<long long>(border_outside_left),
                static_cast<long long>(border_outside_right));
    if (border_outside_left != 65280 || border_outside_right != 65280)
    {
        return fail("border at u=1.6 was not pure green");
    }
    if (!write_uv(-0.0625f, 0.5f) || !sample_uv(border_negative_dest, border_green))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_negative_left = igpu_texture_read(static_cast<std::uint64_t>(border_negative_dest), 1, 4, 0, 0);
    const auto border_negative_right = igpu_texture_read(static_cast<std::uint64_t>(border_negative_dest), 6, 4, 0, 0);
    std::printf("border negative: %lld / %lld\n", static_cast<long long>(border_negative_left),
                static_cast<long long>(border_negative_right));
    if (border_negative_left != 65280 || border_negative_right != 65280)
    {
        return fail("border at u=-0.0625 was not pure green");
    }
    if (!write_uv(1.6f, 0.5f) || !sample_uv(border_red_dest, border_red))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_red_left = igpu_texture_read(static_cast<std::uint64_t>(border_red_dest), 1, 4, 0, 0);
    const auto border_red_right = igpu_texture_read(static_cast<std::uint64_t>(border_red_dest), 6, 4, 0, 0);
    std::printf("border red    : %lld / %lld\n", static_cast<long long>(border_red_left),
                static_cast<long long>(border_red_right));
    if (border_red_left != 255 || border_red_right != 255)
    {
        return fail("red border at u=1.6 was not pure red");
    }
    if (!sample_uv(border_clamp_dest, wrap_clamp))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_clamp_left = igpu_texture_read(static_cast<std::uint64_t>(border_clamp_dest), 1, 4, 0, 0);
    const auto border_clamp_right = igpu_texture_read(static_cast<std::uint64_t>(border_clamp_dest), 6, 4, 0, 0);
    std::printf("border clamp  : %lld / %lld\n", static_cast<long long>(border_clamp_left),
                static_cast<long long>(border_clamp_right));
    if (border_clamp_left != 16711680 || border_clamp_right != 16711680)
    {
        return fail("clamp at u=1.6 was not the edge texel after border draws");
    }
    if (!sample_uv(border_repeat_dest, wrap_repeat))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_repeat_left = igpu_texture_read(static_cast<std::uint64_t>(border_repeat_dest), 1, 4, 0, 0);
    const auto border_repeat_right = igpu_texture_read(static_cast<std::uint64_t>(border_repeat_dest), 6, 4, 0, 0);
    std::printf("border repeat : %lld / %lld\n", static_cast<long long>(border_repeat_left),
                static_cast<long long>(border_repeat_right));
    if (border_repeat_left != 16711680 || border_repeat_right != 16711680)
    {
        return fail("repeat at u=1.6 was not the blue texel after border draws");
    }
    if (!write_uv(1.0f, 0.5f) || !sample_uv(border_linear_dest, border_linear))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_linear_left = igpu_texture_read(static_cast<std::uint64_t>(border_linear_dest), 1, 4, 0, 0);
    const auto border_linear_right = igpu_texture_read(static_cast<std::uint64_t>(border_linear_dest), 6, 4, 0, 0);
    std::printf("border linear : %lld / %lld\n", static_cast<long long>(border_linear_left),
                static_cast<long long>(border_linear_right));
    if (border_linear_left != 8421376 || border_linear_right != 8421376)
    {
        return fail("linear border at u=1.0 was not half green and half blue");
    }
    std::int32_t border_saved = 0;
    std::int32_t border_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(wrap_black_dest), border_saved, border_viewport))
    {
        return fail("border reject target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(wrap_buffer), 0, static_cast<std::uint64_t>(sample_layout), 4, 0, 6, 1, 0,
                  0, 0, border_green) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(border_saved, border_viewport);
        return fail("igpu_draw accepted a border sampler");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_index_buffer), 4, 0, 6, 0, 0, 0, border_green) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(border_saved, border_viewport);
        return fail("igpu_draw_indexed accepted a border sampler");
    }
    const auto border_target_bytes = encode_u64(static_cast<std::uint64_t>(wrap_black_dest));
    const auto border_zero_bytes = encode_u64(0);
    if (igpu_draw_to_render_targets(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout),
                                    4, 0, 6, as_array(border_target_bytes), as_array(border_zero_bytes),
                                    as_array(border_zero_bytes), 0, 0, 0, border_green) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(border_saved, border_viewport);
        return fail("igpu_draw_to_render_targets accepted a border sampler");
    }
    igpu::gl_color_target_end(border_saved, border_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 1, 4, 0, 0) != 0 ||
        igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 6, 4, 0, 0) != 0)
    {
        return fail("rejected border draw changed pixels");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_source), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("border feedback target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                          static_cast<std::uint64_t>(sample_source), 0, 0, 0, border_green) ||
        std::string(igpu_get_last_error()).find("the texture is the current color target") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("border sampler was allowed to sample the current color target");
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("rejected border feedback changed the source texture");
    }
    if (!write_uv(1.6f, 0.5f) || !sample_uv(border_restore_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_restore_left = igpu_texture_read(static_cast<std::uint64_t>(border_restore_dest), 1, 4, 0, 0);
    const auto border_restore_right = igpu_texture_read(static_cast<std::uint64_t>(border_restore_dest), 6, 4, 0, 0);
    std::printf("border restore: %lld / %lld\n", static_cast<long long>(border_restore_left),
                static_cast<long long>(border_restore_right));
    if (border_restore_left != 16711680 || border_restore_right != 16711680)
    {
        return fail("sampler 0 at u=1.6 was not the edge texel after border draws");
    }
    if (!write_uv(0.484375f, 0.5f) || !sample_uv(border_plain_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_plain_left = igpu_texture_read(static_cast<std::uint64_t>(border_plain_dest), 1, 4, 0, 0);
    const auto border_plain_right = igpu_texture_read(static_cast<std::uint64_t>(border_plain_dest), 6, 4, 0, 0);
    std::printf("border plain  : %lld / %lld\n", static_cast<long long>(border_plain_left),
                static_cast<long long>(border_plain_right));
    if (border_plain_left != 255 || border_plain_right != 255)
    {
        return fail("sampler 0 at u=0.484375 was not pure red after linear border");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("border draws changed the source texture");
    }

    // 2x2 source: left column red, right column blue. The gradient shader
    // samples (0.4, 0.5). A tiny derivative takes magnification; a derivative
    // of 1 takes minification. v_uv.x selects which one, so pixel (1, 4) and
    // pixel (6, 4) disagree when the two filters disagree.
    const auto minify_source = igpu_texture_create(0, 2, 2, 1, 6, true, false, 1);
    const auto minify_linear_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto minify_swap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto minify_plain_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto minify_both_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto minify_both0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (minify_source == 0 || minify_linear_dest == 0 || minify_swap_dest == 0 || minify_plain_dest == 0 ||
        minify_both_dest == 0 || minify_both0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(colour_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(colour_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(minify_source), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("minify source target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(colour_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 12,
                   1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(minify_source), 0, 0, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(minify_source), 1, 0, 0, 0) != 16711680)
    {
        return fail("minify source was not red on the left and blue on the right");
    }
    const char* minify_ps =
        "#version 120\n"
        "#extension GL_ARB_shader_texture_lod : require\n"
        "uniform sampler2D igpu_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  vec4 magnified = texture2DGradARB(igpu_tex, vec2(0.4, 0.5), vec2(0.001, 0.0), vec2(0.0, 0.001));\n"
        "  vec4 minified = texture2DGradARB(igpu_tex, vec2(0.4, 0.5), vec2(1.0, 0.0), vec2(0.0, 0.001));\n"
        "  gl_FragColor = v_uv.x < 0.5 ? magnified : minified;\n"
        "}\n";
    const auto minify_frag = igpu_shader_compile(minify_ps, "main", 1, "glsl");
    if (minify_frag == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(minify_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto minify_linear = igpu_sampler_state_create(0, 1, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (minify_linear == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto minify_swap = igpu_sampler_state_create(1, 0, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (minify_swap == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto draw_minify = [&](std::int64_t dest, std::int64_t sampler) -> bool {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(dest), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer),
                                            static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                                            static_cast<std::uint64_t>(minify_source), 0, 0, 0, sampler);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    if (!draw_minify(minify_linear_dest, minify_linear))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto minify_linear_left = igpu_texture_read(static_cast<std::uint64_t>(minify_linear_dest), 1, 4, 0, 0);
    const auto minify_linear_right = igpu_texture_read(static_cast<std::uint64_t>(minify_linear_dest), 6, 4, 0, 0);
    std::printf("minify linear : %lld / %lld\n", static_cast<long long>(minify_linear_left),
                static_cast<long long>(minify_linear_right));
    if (minify_linear_left != 255 || minify_linear_right != 5046450)
    {
        return fail("point magnification and linear minification were not red and the 178/77 blend");
    }
    if (!draw_minify(minify_swap_dest, minify_swap))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto minify_swap_left = igpu_texture_read(static_cast<std::uint64_t>(minify_swap_dest), 1, 4, 0, 0);
    const auto minify_swap_right = igpu_texture_read(static_cast<std::uint64_t>(minify_swap_dest), 6, 4, 0, 0);
    std::printf("minify swap   : %lld / %lld\n", static_cast<long long>(minify_swap_left),
                static_cast<long long>(minify_swap_right));
    if (minify_swap_left != 5046450 || minify_swap_right != 255)
    {
        return fail("linear magnification and point minification were not the 178/77 blend and red");
    }
    if (!draw_minify(minify_plain_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto minify_plain_left = igpu_texture_read(static_cast<std::uint64_t>(minify_plain_dest), 1, 4, 0, 0);
    const auto minify_plain_right = igpu_texture_read(static_cast<std::uint64_t>(minify_plain_dest), 6, 4, 0, 0);
    std::printf("minify plain  : %lld / %lld\n", static_cast<long long>(minify_plain_left),
                static_cast<long long>(minify_plain_right));
    if (minify_plain_left != 255 || minify_plain_right != 255)
    {
        return fail("sampler 0 did not restore nearest magnification and minification");
    }
    // Same 2x2 and the same shader. Magnification and minification are both
    // linear, and mip filtering stays nearest. There is no mip chain, so
    // each side is the 178/77 blend of the red and blue texels. Nearest on
    // either side would stay pure red. Sampler 0 must restore both filters.
    const auto minify_both = igpu_sampler_state_create(1, 1, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (minify_both == 0 || !draw_minify(minify_both_dest, minify_both))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto minify_both_left = igpu_texture_read(static_cast<std::uint64_t>(minify_both_dest), 1, 4, 0, 0);
    const auto minify_both_right = igpu_texture_read(static_cast<std::uint64_t>(minify_both_dest), 6, 4, 0, 0);
    std::printf("minify both   : %lld / %lld\n", static_cast<long long>(minify_both_left),
                static_cast<long long>(minify_both_right));
    if (minify_both_left != 5046450 || minify_both_right != 5046450)
    {
        return fail("linear magnification and linear minification were not both the 178/77 blend");
    }
    if (!draw_minify(minify_both0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto minify_both0_left = igpu_texture_read(static_cast<std::uint64_t>(minify_both0_dest), 1, 4, 0, 0);
    const auto minify_both0_right = igpu_texture_read(static_cast<std::uint64_t>(minify_both0_dest), 6, 4, 0, 0);
    std::printf("minify both0  : %lld / %lld\n", static_cast<long long>(minify_both0_left),
                static_cast<long long>(minify_both0_right));
    if (minify_both0_left != 255 || minify_both0_right != 255)
    {
        return fail("sampler 0 did not restore nearest magnification and minification after both were linear");
    }
    const auto border_min_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_min0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_min_dest == 0 || border_min0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_min = igpu_sampler_state_create(0, 1, 0, 3, 3, 3, 1, 0, 0, 0.f, 0.f, -1.f);
    if (border_min == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!draw_minify(border_min_dest, border_min))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_min_left = igpu_texture_read(static_cast<std::uint64_t>(border_min_dest), 1, 4, 0, 0);
    const auto border_min_right = igpu_texture_read(static_cast<std::uint64_t>(border_min_dest), 6, 4, 0, 0);
    std::printf("border min   : %lld / %lld\n", static_cast<long long>(border_min_left),
                static_cast<long long>(border_min_right));
    if (border_min_left != 255 || border_min_right != 5046450)
    {
        return fail("border point magnification and linear minification were not red and the 178/77 blend");
    }
    if (!draw_minify(border_min0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_min0_left = igpu_texture_read(static_cast<std::uint64_t>(border_min0_dest), 1, 4, 0, 0);
    const auto border_min0_right = igpu_texture_read(static_cast<std::uint64_t>(border_min0_dest), 6, 4, 0, 0);
    std::printf("border min0  : %lld / %lld\n", static_cast<long long>(border_min0_left),
                static_cast<long long>(border_min0_right));
    if (border_min0_left != 255 || border_min0_right != 255)
    {
        return fail("sampler 0 did not restore nearest minification after the border sampler");
    }
    // Same 2x2, no mip chain, and a green border. The left half samples
    // u=1.0 with a tiny derivative, so linear magnification mixes the blue
    // edge texel with that border. The right half samples (0.4, 0.5) with a
    // derivative of 1, so linear minification is the 178/77 seam. Nearest
    // magnification would stay on the blue edge. Nearest minification would
    // stay on the red texel. Sampler 0 on this shader is that blue edge and
    // that red texel. Sampler 0 on the (0.4, 0.5) shader is pure red, so a
    // stuck linear magnification filter cannot hide behind the seam.
    const auto border_both_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_both0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_both1_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_both_dest == 0 || border_both0_dest == 0 || border_both1_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const char* border_both_ps =
        "#version 120\n"
        "#extension GL_ARB_shader_texture_lod : require\n"
        "uniform sampler2D igpu_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  vec4 magnified = texture2DGradARB(igpu_tex, vec2(1.0, 0.5), vec2(0.001, 0.0), vec2(0.0, 0.001));\n"
        "  vec4 minified = texture2DGradARB(igpu_tex, vec2(0.4, 0.5), vec2(1.0, 0.0), vec2(0.0, 0.001));\n"
        "  gl_FragColor = v_uv.x < 0.5 ? magnified : minified;\n"
        "}\n";
    const auto border_both_frag = igpu_shader_compile(border_both_ps, "main", 1, "glsl");
    if (border_both_frag == 0 || !igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(border_both_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_both = igpu_sampler_state_create(1, 1, 0, 3, 3, 3, 1, 65280, 0, 0.f, 0.f, -1.f);
    if (border_both == 0 || !draw_minify(border_both_dest, border_both))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_both_left = igpu_texture_read(static_cast<std::uint64_t>(border_both_dest), 1, 4, 0, 0);
    const auto border_both_right = igpu_texture_read(static_cast<std::uint64_t>(border_both_dest), 6, 4, 0, 0);
    std::printf("border both  : %lld / %lld\n", static_cast<long long>(border_both_left),
                static_cast<long long>(border_both_right));
    if (border_both_left != 8421376 || border_both_right != 5046450)
    {
        return fail("border linear magnification and linear minification were not the green-blue edge and the 178/77 blend");
    }
    if (!draw_minify(border_both0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_both0_left = igpu_texture_read(static_cast<std::uint64_t>(border_both0_dest), 1, 4, 0, 0);
    const auto border_both0_right = igpu_texture_read(static_cast<std::uint64_t>(border_both0_dest), 6, 4, 0, 0);
    std::printf("border both0 : %lld / %lld\n", static_cast<long long>(border_both0_left),
                static_cast<long long>(border_both0_right));
    if (border_both0_left != 16711680 || border_both0_right != 255)
    {
        return fail("sampler 0 did not restore the blue edge and the red texel after border linear magnification and minification");
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(minify_frag), 1) || !draw_minify(border_both1_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_both1_left = igpu_texture_read(static_cast<std::uint64_t>(border_both1_dest), 1, 4, 0, 0);
    const auto border_both1_right = igpu_texture_read(static_cast<std::uint64_t>(border_both1_dest), 6, 4, 0, 0);
    std::printf("border both1 : %lld / %lld\n", static_cast<long long>(border_both1_left),
                static_cast<long long>(border_both1_right));
    if (border_both1_left != 255 || border_both1_right != 255)
    {
        return fail("sampler 0 did not restore nearest magnification after border linear magnification and minification");
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }

    const auto border_mixed = igpu_sampler_state_create(0, 0, 0, 3, 0, 0, 1, 65280, 0, 0.f, 0.f, -1.f);
    if (border_mixed != 0 || std::string(igpu_get_last_error()).find("border on every axis") == std::string::npos)
    {
        return fail("border mixed with clamp was accepted");
    }
    const auto wrap_compare = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 1, 0.f, 0.f, -1.f);
    if (wrap_compare <= 0 || !igpu_state_release(wrap_compare))
    {
        return fail("comparison sampler could not be created and released");
    }
    const auto wrap_aniso = igpu_sampler_state_create(1, 1, 1, 1, 1, 1, 2, 0, 0, 0.f, 0.f, -1.f);
    if (wrap_aniso <= 0)
    {
        std::fprintf(stderr, "anisotropic constructor: %s\n", igpu_get_last_error().c_str());
        return fail("noncomparison anisotropic sampler with linear min/mag/mip was rejected");
    }
    if (glGetError() != GL_NO_ERROR)
    {
        return fail("OpenGL error before anisotropic sampler coverage");
    }
    GLfloat aniso_device_max = 0.f;
    glGetFloatv(device_max_anisotropy, &aniso_device_max);
    if (glGetError() != GL_NO_ERROR || aniso_device_max < 2.f)
    {
        return fail("anisotropic sampler was created without usable driver anisotropy support");
    }
    const GLint requested_aniso = aniso_device_max >= 4.f ? 4 : 2;
    const GLfloat caller_aniso = requested_aniso == 4 ? 2.f : 1.f;
    const auto aniso_blend = igpu_blend_state_create(false, 2, 2, 1, 2, 2, 1, true, true, true, true);
    const auto aniso_four = igpu_sampler_state_create(1, 1, 1, 0, 1, 2, requested_aniso, 0, 0, 0.f, 0.f, -1.f);
    if (aniso_blend != wrap_aniso + 1 || aniso_four != aniso_blend + 1)
    {
        return fail("anisotropic samplers do not share blend-state handle numbering");
    }

    // Public validation still requires all three filter bits and anisotropy in 1..16.
    for (const auto address : {0, 3})
    {
        for (int bits = 0; bits < 7; ++bits)
        {
            if (igpu_sampler_state_create(bits & 1, (bits >> 1) & 1, (bits >> 2) & 1,
                                          address, address, address, 2, 65280, 0, 0.f, 0.f, -1.f) != 0 ||
                igpu_get_last_error().find("requires linear filters") == std::string::npos)
            {
                return fail("anisotropy accepted a non-linear mag, min, or mip filter");
            }
        }
        for (const auto invalid : {-1, 0, 17, 2147483647})
        {
            if (igpu_sampler_state_create(1, 1, 1, address, address, address, invalid,
                                          65280, 0, 0.f, 0.f, -1.f) != 0 ||
                igpu_get_last_error().find("1..16") == std::string::npos)
            {
                return fail("anisotropy outside the public 1..16 range was accepted");
            }
        }
    }
    if (igpu_sampler_state_create(1, 1, 1, 3, 0, 3, 2, 65280, 0, 0.f, 0.f, -1.f) != 0 ||
        igpu_get_last_error().find("border on every axis") == std::string::npos)
    {
        return fail("anisotropy accepted mixed border address axes");
    }
    if (igpu_sampler_state_create(1, 1, 1, 0, 4, 0, 2, 0, 0, 0.f, 0.f, -1.f) != 0)
    {
        return fail("anisotropy accepted an invalid address mode");
    }
    const auto aniso_border = igpu_sampler_state_create(1, 1, 1, 3, 3, 3, requested_aniso, 65280, 0, 0.f, 0.f, -1.f);
    if (aniso_border != aniso_four + 1)
    {
        return fail("all-border anisotropy failed or rejected constructors consumed state handles");
    }
    const auto aniso_limit = igpu_sampler_state_create(1, 1, 1, 0, 0, 0, 16, 0, 0, 0.f, 0.f, -1.f);
    if ((aniso_device_max >= 16.f && aniso_limit <= 0) ||
        (aniso_device_max < 16.f && (aniso_limit != 0 || igpu_get_last_error().find("max") == std::string::npos)))
    {
        return fail("anisotropy 16 did not obey the explicit device maximum contract");
    }

    const auto get_attachment = reinterpret_cast<void(APIENTRY*)(GLenum, GLenum, GLenum, GLint*)>(
        wglGetProcAddress("glGetFramebufferAttachmentParameteriv"));
    if (get_attachment == nullptr)
    {
        return fail("cannot observe the source GL texture for anisotropic draws");
    }
    const auto texture_name = [&](std::int64_t source) -> GLuint {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(source), saved, viewport))
        {
            return 0;
        }
        GLint name = 0;
        get_attachment(0x8D40, 0x8CE0, 0x8CD1, &name); // framebuffer, color attachment, object name
        igpu::gl_color_target_end(saved, viewport);
        return static_cast<GLuint>(name);
    };
    // A uniform source avoids driver-dependent footprints at repeat seams.
    // Address selection is checked directly at the sampler draw boundary below.
    const auto aniso_fixture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    std::int32_t aniso_fixture_fbo = 0;
    std::int32_t aniso_fixture_viewport[4] = {};
    if (aniso_fixture == 0 || !igpu::gl_color_target_begin(aniso_fixture, aniso_fixture_fbo, aniso_fixture_viewport))
    {
        return fail("anisotropic uniform source target creation failed");
    }
    GLfloat aniso_clear_color[4] = {};
    glGetFloatv(GL_COLOR_CLEAR_VALUE, aniso_clear_color);
    glClearColor(1.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(aniso_clear_color[0], aniso_clear_color[1], aniso_clear_color[2], aniso_clear_color[3]);
    igpu::gl_color_target_end(aniso_fixture_fbo, aniso_fixture_viewport);
    if (igpu_texture_read(aniso_fixture, 1, 4, 0, 0) != 255 || igpu_texture_read(aniso_fixture, 6, 4, 0, 0) != 255)
    {
        return fail("anisotropic source is not uniformly red");
    }
    const GLuint aniso_source_name = texture_name(aniso_fixture);
    if (aniso_source_name == 0)
    {
        return fail("could not capture anisotropic source texture names");
    }
    const auto aniso_source_original = texture_parameters(aniso_source_name);
    if (aniso_source_original.integers[5] != 0 || aniso_source_original.integers[6] != 0)
    {
        return fail("a single-level anisotropic source did not initialize base/max mip level to zero");
    }
    auto aniso_caller = aniso_source_original;
    aniso_caller.floats = {caller_aniso, -0.25f, 0.f, 7.f, 0.25f, 0.5f, 0.75f, 1.f};
    set_texture_parameters(aniso_source_name, aniso_caller);
    const auto aniso_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto aniso_reject_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (aniso_dest == 0 || aniso_reject_dest == 0 || glGetError() != GL_NO_ERROR)
    {
        return fail("anisotropic draw fixture setup failed");
    }
    const auto aniso_pixels = [&](std::int64_t dest, std::int64_t expected) -> bool {
        const auto left = igpu_texture_read(static_cast<std::uint64_t>(dest), 1, 4, 0, 0);
        const auto right = igpu_texture_read(static_cast<std::uint64_t>(dest), 6, 4, 0, 0);
        if (left != expected || right != expected)
        {
            std::fprintf(stderr, "aniso pixels: actual=%lld/%lld expected=%lld\n",
                         static_cast<long long>(left), static_cast<long long>(right), static_cast<long long>(expected));
        }
        return left == expected && right == expected;
    };
    SamplerDrawObserver aniso_observer;
    if (!aniso_observer.install())
    {
        return fail("could not install the real-draw anisotropy observer");
    }
    const auto draw_aniso = [&](std::int64_t source, GLuint name, std::int64_t sampler,
                                GLint anisotropy, GLint wrap_s, GLint wrap_t, bool border) -> bool {
        const auto before = texture_parameters(name);
        GLint binding_before = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding_before);
        aniso_observer.calls = 0;
        const bool drew = sample_from(aniso_dest, sampler, source);
        GLint binding_after = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding_after);
        const auto& observed = aniso_observer.parameters;
        const bool applied = aniso_observer.calls == 1 && aniso_observer.texture == static_cast<GLint>(name) &&
                             observed.floats[0] == static_cast<GLfloat>(anisotropy) &&
                             observed.integers[0] == wrap_s && observed.integers[1] == wrap_t &&
                             observed.integers[3] == GL_LINEAR_MIPMAP_LINEAR && observed.integers[4] == GL_LINEAR &&
                             (!border || (observed.floats[4] == 0.f && observed.floats[5] == 1.f &&
                                          observed.floats[6] == 0.f && observed.floats[7] == 1.f));
        if (!drew || !applied || !(texture_parameters(name) == before) || binding_before != binding_after ||
            glGetError() != GL_NO_ERROR)
        {
            std::fprintf(stderr, "aniso draw: sampler=%lld requested=%d observed=%g calls=%u error=%s\n",
                         static_cast<long long>(sampler), anisotropy, observed.floats[0], aniso_observer.calls,
                         igpu_get_last_error().c_str());
            return false;
        }
        return true;
    };
    if (!write_uv(1.25f, 0.5f) || !draw_aniso(aniso_fixture, aniso_source_name, wrap_aniso, 2, GL_REPEAT, GL_REPEAT, false) ||
        !aniso_pixels(aniso_dest, 255))
    {
        return fail("anisotropy 2 did not sample the uniform red fixture and restore caller parameters");
    }
    if (!write_uv(0.25f, 0.5f) ||
        !draw_aniso(aniso_fixture, aniso_source_name, aniso_four, requested_aniso, 0x812F, GL_REPEAT, false) ||
        !aniso_pixels(aniso_dest, 255))
    {
        return fail("requested anisotropy was not applied at the real draw and restored to the caller sentinel");
    }
    if (aniso_limit > 0 &&
        (!draw_aniso(aniso_fixture, aniso_source_name, aniso_limit, 16, 0x812F, 0x812F, false) ||
         !aniso_pixels(aniso_dest, 255)))
    {
        return fail("maximum public anisotropy did not reach the real draw");
    }
    constexpr GLint aniso_wraps[] = {0x812F, GL_REPEAT, 0x8370}; // clamp, repeat, mirror
    for (GLint u = 0; u < 3; ++u)
    {
        for (GLint v = 0; v < 3; ++v)
        {
            for (GLint w = 0; w < 3; ++w)
            {
                const auto sampler = igpu_sampler_state_create(1, 1, 1, u, v, w, 2, 0, 0, 0.f, 0.f, -1.f);
                if (sampler <= 0 || !write_uv(1.25f, 0.5f) ||
                    !draw_aniso(aniso_fixture, aniso_source_name, sampler, 2, aniso_wraps[u], aniso_wraps[v], false) ||
                    !aniso_pixels(aniso_dest, 255) || !write_uv(0.5f, 1.25f) ||
                    !draw_aniso(aniso_fixture, aniso_source_name, sampler, 2, aniso_wraps[u], aniso_wraps[v], false) ||
                    !aniso_pixels(aniso_dest, 255) || !igpu_state_release(sampler))
                {
                    std::fprintf(stderr, "anisotropic address combination: %d/%d/%d\n", u, v, w);
                    return fail("anisotropic U/V/W address selection, pixels, or restoration failed");
                }
            }
        }
    }
    if (!write_uv(2.f, 0.5f) ||
        !draw_aniso(aniso_fixture, aniso_source_name, aniso_border, requested_aniso, 0x812D, 0x812D, true) ||
        !aniso_pixels(aniso_dest, 65280) || !write_uv(0.25f, 0.5f) ||
        !draw_aniso(aniso_fixture, aniso_source_name, aniso_border, requested_aniso, 0x812D, 0x812D, true) ||
        !aniso_pixels(aniso_dest, 255))
    {
        return fail("all-border anisotropy lost its supplied green border, interior texel, or caller border color");
    }
    if (!igpu_state_release(wrap_aniso) || igpu_state_release(wrap_aniso) ||
        igpu_get_last_error().find("unknown sampler handle") == std::string::npos)
    {
        return fail("anisotropic sampler release did not retire its handle");
    }
    for (const auto invalid : {wrap_aniso, static_cast<std::int64_t>(2147483647), aniso_blend})
    {
        GLint viewport_before[4] = {};
        GLint framebuffer_before = 0;
        GLint binding_before = 0;
        glGetIntegerv(GL_VIEWPORT, viewport_before);
        glGetIntegerv(0x8CA6, &framebuffer_before);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding_before);
        aniso_observer.calls = 0;
        if (sample_from(aniso_reject_dest, invalid, aniso_fixture) ||
            igpu_get_last_error().find("unknown sampler handle") == std::string::npos)
        {
            return fail("a released, unknown, or blend handle was accepted as an anisotropic sampler");
        }
        GLint aniso_viewport_after[4] = {};
        GLint aniso_framebuffer_after = 0;
        GLint binding_after = 0;
        glGetIntegerv(GL_VIEWPORT, aniso_viewport_after);
        glGetIntegerv(0x8CA6, &aniso_framebuffer_after);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding_after);
        if (aniso_observer.calls != 0 || !aniso_pixels(aniso_reject_dest, 0) ||
            !(texture_parameters(aniso_source_name) == aniso_caller) ||
            framebuffer_before != aniso_framebuffer_after || binding_before != binding_after ||
            std::memcmp(viewport_before, aniso_viewport_after, sizeof(viewport_before)) != 0 || glGetError() != GL_NO_ERROR)
        {
            return fail("rejected anisotropic draw changed pixels, source parameters, binding, framebuffer, or viewport");
        }
    }
    if (!write_uv(0.25f, 0.5f) ||
        !draw_aniso(aniso_fixture, aniso_source_name, aniso_four, requested_aniso, 0x812F, GL_REPEAT, false) ||
        !aniso_pixels(aniso_dest, 255) || !igpu_state_release(aniso_four) || !igpu_state_release(aniso_border) ||
        !igpu_state_release(aniso_blend) || (aniso_limit > 0 && !igpu_state_release(aniso_limit)) || !aniso_observer.stop())
    {
        return fail("anisotropic release affected a live sampler or failed to restore the draw observer");
    }
    set_texture_parameters(aniso_source_name, aniso_source_original);
    if (glGetError() != GL_NO_ERROR)
    {
        return fail("OpenGL error after anisotropic creation, draw, rejection, and restoration coverage");
    }
    std::printf("aniso draw   : during-draw state, 27 address triples, border, validation, release; max=%g\n", aniso_device_max);
    const auto lod_source = igpu_texture_create(0, 2, 2, 1, 6, true, false, 2);
    const auto lod_hold_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto lod_coarse_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto lod_plain_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto lod_finest_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto lod_fine0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto lod_cap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto lod_cap0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto lod_uncap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (lod_source == 0 || lod_hold_dest == 0 || lod_coarse_dest == 0 || lod_plain_dest == 0 ||
        lod_finest_dest == 0 || lod_fine0_dest == 0 || lod_cap_dest == 0 || lod_cap0_dest == 0 ||
        lod_uncap_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    ColourVertex lod_red[6] = {};
    ColourVertex lod_green[6] = {};
    const float lod_pos[] = {
        -1.f, -1.f, 1.f, -1.f, -1.f, 1.f,
        1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
    };
    for (int i = 0; i < 6; ++i)
    {
        lod_red[i].x = lod_pos[i * 2];
        lod_red[i].y = lod_pos[i * 2 + 1];
        lod_red[i].r = 255;
        lod_red[i].a = 255;
        lod_green[i].x = lod_pos[i * 2];
        lod_green[i].y = lod_pos[i * 2 + 1];
        lod_green[i].g = 255;
        lod_green[i].a = 255;
    }
    const auto lod_red_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(lod_red)), 0, 1, 12);
    const auto lod_green_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(lod_green)), 0, 1, 12);
    if (lod_red_buffer == 0 || lod_green_buffer == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(lod_red_buffer), 0, gm::wire::GMBuffer(lod_red, sizeof(lod_red))) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(lod_green_buffer), 0,
                           gm::wire::GMBuffer(lod_green, sizeof(lod_green))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(colour_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(colour_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_target = encode_u64(static_cast<std::uint64_t>(lod_source));
    const auto lod_zero = encode_u64(0);
    const auto lod_mip1 = encode_u64(1);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(lod_red_buffer),
                                     static_cast<std::uint64_t>(colour_layout), 4, 0, 6, as_array(lod_target),
                                     as_array(lod_zero), as_array(lod_zero), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(lod_green_buffer),
                                     static_cast<std::uint64_t>(colour_layout), 4, 0, 6, as_array(lod_target),
                                     as_array(lod_zero), as_array(lod_mip1), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(lod_source), 0, 0, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(lod_source), 1, 0, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(lod_source), 0, 0, 0, 1) != 65280)
    {
        return fail("mip chain was not red on level 0 and green on level 1");
    }
    const char* lod_ps =
        "#version 120\n"
        "#extension GL_ARB_shader_texture_lod : require\n"
        "uniform sampler2D igpu_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_FragColor = texture2DGradARB(igpu_tex, vec2(0.25, 0.25), vec2(0.125, 0.0), vec2(0.0, 0.125));\n"
        "}\n";
    const auto lod_frag = igpu_shader_compile(lod_ps, "main", 1, "glsl");
    if (lod_frag == 0 || !igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(lod_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_hold = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto lod_coarse = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 0, 4.f, 0.f, -1.f);
    const auto lod_finest = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 0, 0.f, 1.f, -1.f);
    const auto lod_cap = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 0, 4.f, 0.f, 0.f);
    if (lod_hold == 0 || lod_coarse == 0 || lod_finest == 0 || lod_cap == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto draw_lod = [&](std::int64_t dest, std::int64_t sampler) -> bool {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(dest), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer),
                                            static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                                            static_cast<std::uint64_t>(lod_source), 0, 0, 0, sampler);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    // Constant-color mip levels make bias/range checks independent of anisotropic image quality.
    const GLuint aniso_lod_name = texture_name(lod_source);
    if (aniso_lod_name == 0)
    {
        return fail("could not capture the anisotropic mip source texture");
    }
    const auto aniso_lod_original = texture_parameters(aniso_lod_name);
    auto aniso_lod_caller = aniso_lod_original;
    aniso_lod_caller.floats = {caller_aniso, 4.f, 1.f, 1.f, 0.25f, 0.5f, 0.75f, 1.f};
    set_texture_parameters(aniso_lod_name, aniso_lod_caller);
    if (!aniso_observer.install())
    {
        return fail("could not observe the real anisotropic mip draw");
    }
    struct AnisoLodCase { float bias; float finest; float coarsest; std::int64_t pixel; };
    const AnisoLodCase aniso_lod_cases[] = {
        {0.f, 0.f, -1.f, 255}, {4.f, 0.f, -1.f, 65280},
        {0.f, 1.f, -1.f, 65280}, {4.f, 0.f, 0.f, 255},
    };
    for (const auto address : {0, 3})
    {
        for (const auto& test : aniso_lod_cases)
        {
            const auto sampler = igpu_sampler_state_create(1, 1, 1, address, address, address, requested_aniso,
                                                           65280, 0, test.bias, test.finest, test.coarsest);
            aniso_observer.calls = 0;
            if (sampler <= 0 || !draw_lod(aniso_dest, sampler))
            {
                return fail("anisotropic LOD sampler creation or draw failed");
            }
            const auto& observed = aniso_observer.parameters;
            const GLfloat expected_bias = test.bias;
            const GLfloat expected_max = test.coarsest < 0.f ? 3.402823466e+38f : test.coarsest;
            if (aniso_observer.calls != 1 || aniso_observer.texture != static_cast<GLint>(aniso_lod_name) ||
                observed.floats[0] != static_cast<GLfloat>(requested_aniso) || observed.floats[1] != expected_bias ||
                observed.floats[2] != test.finest || observed.floats[3] != expected_max ||
                observed.integers[3] != GL_LINEAR_MIPMAP_LINEAR || observed.integers[4] != GL_LINEAR ||
                (address == 3 && (observed.floats[4] != 0.f || observed.floats[5] != 1.f ||
                                  observed.floats[6] != 0.f || observed.floats[7] != 1.f)) ||
                !(texture_parameters(aniso_lod_name) == aniso_lod_caller) || !aniso_pixels(aniso_dest, test.pixel) ||
                !igpu_state_release(sampler) || glGetError() != GL_NO_ERROR)
            {
                std::fprintf(stderr, "aniso LOD: address=%d bias=%g finest=%g coarsest=%g\n",
                             address, test.bias, test.finest, test.coarsest);
                return fail("anisotropic LOD bias/range, mip pixels, border color, or parameter restoration failed");
            }
        }
    }
    aniso_observer.calls = 0;
    if (!draw_lod(aniso_dest, 0) || !aniso_pixels(aniso_dest, 65280) ||
        !(texture_parameters(aniso_lod_name) == aniso_lod_caller) || !aniso_observer.stop())
    {
        return fail("sampler zero did not retain the caller's mip and anisotropy state after anisotropic draws");
    }
    set_texture_parameters(aniso_lod_name, aniso_lod_original);
    if (glGetError() != GL_NO_ERROR)
    {
        return fail("OpenGL error after anisotropic LOD coverage");
    }
    std::printf("aniso LOD    : normal/border base, bias, finest, coarsest; draw-state and mip-range restored\n");

    if (!draw_lod(lod_hold_dest, lod_hold))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_hold_left = igpu_texture_read(static_cast<std::uint64_t>(lod_hold_dest), 1, 4, 0, 0);
    const auto lod_hold_right = igpu_texture_read(static_cast<std::uint64_t>(lod_hold_dest), 6, 4, 0, 0);
    std::printf("lod hold     : %lld / %lld\n", static_cast<long long>(lod_hold_left),
                static_cast<long long>(lod_hold_right));
    if (lod_hold_left != 255 || lod_hold_right != 255)
    {
        return fail("level offset 0 did not stay on the red base level");
    }
    if (!draw_lod(lod_coarse_dest, lod_coarse))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_coarse_left = igpu_texture_read(static_cast<std::uint64_t>(lod_coarse_dest), 1, 4, 0, 0);
    const auto lod_coarse_right = igpu_texture_read(static_cast<std::uint64_t>(lod_coarse_dest), 6, 4, 0, 0);
    std::printf("lod coarse   : %lld / %lld\n", static_cast<long long>(lod_coarse_left),
                static_cast<long long>(lod_coarse_right));
    if (lod_coarse_left != 65280 || lod_coarse_right != 65280)
    {
        return fail("level offset 4 did not read the green coarser level");
    }
    if (!draw_lod(lod_plain_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_plain_left = igpu_texture_read(static_cast<std::uint64_t>(lod_plain_dest), 1, 4, 0, 0);
    const auto lod_plain_right = igpu_texture_read(static_cast<std::uint64_t>(lod_plain_dest), 6, 4, 0, 0);
    std::printf("lod plain    : %lld / %lld\n", static_cast<long long>(lod_plain_left),
                static_cast<long long>(lod_plain_right));
    if (lod_plain_left != 255 || lod_plain_right != 255)
    {
        return fail("sampler 0 did not restore the base level after the level offset");
    }
    if (!draw_lod(lod_finest_dest, lod_finest))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_finest_left = igpu_texture_read(static_cast<std::uint64_t>(lod_finest_dest), 1, 4, 0, 0);
    const auto lod_finest_right = igpu_texture_read(static_cast<std::uint64_t>(lod_finest_dest), 6, 4, 0, 0);
    std::printf("lod finest   : %lld / %lld\n", static_cast<long long>(lod_finest_left),
                static_cast<long long>(lod_finest_right));
    if (lod_finest_left != 65280 || lod_finest_right != 65280)
    {
        return fail("finest level 1 did not read the green coarser level");
    }
    if (!draw_lod(lod_fine0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_fine0_left = igpu_texture_read(static_cast<std::uint64_t>(lod_fine0_dest), 1, 4, 0, 0);
    const auto lod_fine0_right = igpu_texture_read(static_cast<std::uint64_t>(lod_fine0_dest), 6, 4, 0, 0);
    std::printf("lod fine0    : %lld / %lld\n", static_cast<long long>(lod_fine0_left),
                static_cast<long long>(lod_fine0_right));
    if (lod_fine0_left != 255 || lod_fine0_right != 255)
    {
        return fail("sampler 0 did not restore the base level after the finest level");
    }
    if (!draw_lod(lod_cap_dest, lod_cap))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_cap_left = igpu_texture_read(static_cast<std::uint64_t>(lod_cap_dest), 1, 4, 0, 0);
    const auto lod_cap_right = igpu_texture_read(static_cast<std::uint64_t>(lod_cap_dest), 6, 4, 0, 0);
    std::printf("lod cap      : %lld / %lld\n", static_cast<long long>(lod_cap_left),
                static_cast<long long>(lod_cap_right));
    if (lod_cap_left != 255 || lod_cap_right != 255)
    {
        return fail("coarsest level 0 did not keep the red base level");
    }
    if (!draw_lod(lod_cap0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_cap0_left = igpu_texture_read(static_cast<std::uint64_t>(lod_cap0_dest), 1, 4, 0, 0);
    const auto lod_cap0_right = igpu_texture_read(static_cast<std::uint64_t>(lod_cap0_dest), 6, 4, 0, 0);
    std::printf("lod cap0     : %lld / %lld\n", static_cast<long long>(lod_cap0_left),
                static_cast<long long>(lod_cap0_right));
    if (lod_cap0_left != 255 || lod_cap0_right != 255)
    {
        return fail("sampler 0 did not restore the base level after the coarsest level");
    }
    if (!draw_lod(lod_uncap_dest, lod_coarse))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto lod_uncap_left = igpu_texture_read(static_cast<std::uint64_t>(lod_uncap_dest), 1, 4, 0, 0);
    const auto lod_uncap_right = igpu_texture_read(static_cast<std::uint64_t>(lod_uncap_dest), 6, 4, 0, 0);
    std::printf("lod uncap    : %lld / %lld\n", static_cast<long long>(lod_uncap_left),
                static_cast<long long>(lod_uncap_right));
    if (lod_uncap_left != 65280 || lod_uncap_right != 65280)
    {
        return fail("coarsest level 0 stayed applied after the draw");
    }
    const auto border_lod_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_lod0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_lod_dest == 0 || border_lod0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_lod = igpu_sampler_state_create(0, 0, 0, 3, 3, 3, 1, 65280, 0, 4.f, 0.f, -1.f);
    if (border_lod == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!draw_lod(border_lod_dest, border_lod))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_lod_left = igpu_texture_read(static_cast<std::uint64_t>(border_lod_dest), 1, 4, 0, 0);
    const auto border_lod_right = igpu_texture_read(static_cast<std::uint64_t>(border_lod_dest), 6, 4, 0, 0);
    std::printf("border lod   : %lld / %lld\n", static_cast<long long>(border_lod_left),
                static_cast<long long>(border_lod_right));
    if (border_lod_left != 65280 || border_lod_right != 65280)
    {
        return fail("border level offset 4 did not read the green coarser level");
    }
    if (!draw_lod(border_lod0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_lod0_left = igpu_texture_read(static_cast<std::uint64_t>(border_lod0_dest), 1, 4, 0, 0);
    const auto border_lod0_right = igpu_texture_read(static_cast<std::uint64_t>(border_lod0_dest), 6, 4, 0, 0);
    std::printf("border lod0  : %lld / %lld\n", static_cast<long long>(border_lod0_left),
                static_cast<long long>(border_lod0_right));
    if (border_lod0_left != 255 || border_lod0_right != 255)
    {
        return fail("sampler 0 did not restore the base level after the border level offset");
    }
    const auto border_fine_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_fine0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_fine_dest == 0 || border_fine0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_fine = igpu_sampler_state_create(0, 0, 0, 3, 3, 3, 1, 0, 0, 0.f, 1.f, -1.f);
    if (border_fine == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!draw_lod(border_fine_dest, border_fine))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_fine_left = igpu_texture_read(static_cast<std::uint64_t>(border_fine_dest), 1, 4, 0, 0);
    const auto border_fine_right = igpu_texture_read(static_cast<std::uint64_t>(border_fine_dest), 6, 4, 0, 0);
    std::printf("border fine  : %lld / %lld\n", static_cast<long long>(border_fine_left),
                static_cast<long long>(border_fine_right));
    if (border_fine_left != 65280 || border_fine_right != 65280)
    {
        return fail("border finest level 1 did not read the green coarser level");
    }
    if (!draw_lod(border_fine0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_fine0_left = igpu_texture_read(static_cast<std::uint64_t>(border_fine0_dest), 1, 4, 0, 0);
    const auto border_fine0_right = igpu_texture_read(static_cast<std::uint64_t>(border_fine0_dest), 6, 4, 0, 0);
    std::printf("border fine0 : %lld / %lld\n", static_cast<long long>(border_fine0_left),
                static_cast<long long>(border_fine0_right));
    if (border_fine0_left != 255 || border_fine0_right != 255)
    {
        return fail("sampler 0 did not restore the base level after the border finest level");
    }
    const auto border_cap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_cap0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_uncap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_cap_dest == 0 || border_cap0_dest == 0 || border_uncap_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_cap = igpu_sampler_state_create(0, 0, 0, 3, 3, 3, 1, 0, 0, 4.f, 0.f, 0.f);
    if (border_cap == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!draw_lod(border_cap_dest, border_cap))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_cap_left = igpu_texture_read(static_cast<std::uint64_t>(border_cap_dest), 1, 4, 0, 0);
    const auto border_cap_right = igpu_texture_read(static_cast<std::uint64_t>(border_cap_dest), 6, 4, 0, 0);
    std::printf("border cap   : %lld / %lld\n", static_cast<long long>(border_cap_left),
                static_cast<long long>(border_cap_right));
    if (border_cap_left != 255 || border_cap_right != 255)
    {
        return fail("border coarsest level 0 did not keep the red base level");
    }
    if (!draw_lod(border_cap0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_cap0_left = igpu_texture_read(static_cast<std::uint64_t>(border_cap0_dest), 1, 4, 0, 0);
    const auto border_cap0_right = igpu_texture_read(static_cast<std::uint64_t>(border_cap0_dest), 6, 4, 0, 0);
    std::printf("border cap0  : %lld / %lld\n", static_cast<long long>(border_cap0_left),
                static_cast<long long>(border_cap0_right));
    if (border_cap0_left != 255 || border_cap0_right != 255)
    {
        return fail("sampler 0 did not restore the base level after the border coarsest level");
    }
    if (!draw_lod(border_uncap_dest, border_lod))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_uncap_left = igpu_texture_read(static_cast<std::uint64_t>(border_uncap_dest), 1, 4, 0, 0);
    const auto border_uncap_right = igpu_texture_read(static_cast<std::uint64_t>(border_uncap_dest), 6, 4, 0, 0);
    std::printf("border uncap : %lld / %lld\n", static_cast<long long>(border_uncap_left),
                static_cast<long long>(border_uncap_right));
    if (border_uncap_left != 65280 || border_uncap_right != 65280)
    {
        return fail("border coarsest level 0 stayed applied after the draw");
    }
    const char* mip_ps =
        "#version 120\n"
        "#extension GL_ARB_shader_texture_lod : require\n"
        "uniform sampler2D igpu_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_FragColor = texture2DLod(igpu_tex, vec2(0.25, 0.5), 0.5);\n"
        "}\n";
    const auto mip_frag = igpu_shader_compile(mip_ps, "main", 1, "glsl");
    if (mip_frag == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mip_point = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (mip_point == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mip_linear = igpu_sampler_state_create(0, 0, 1, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (mip_linear == 0)
    {
        const std::string mip_error = igpu_get_last_error();
        return fail(mip_error.c_str());
    }
    const auto mip_point_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mip_fine_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mip_cap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mip_linear_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mip_bias_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mip_plain_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (mip_point_dest == 0 || mip_fine_dest == 0 || mip_cap_dest == 0 || mip_linear_dest == 0 ||
        mip_bias_dest == 0 || mip_plain_dest == 0 || !igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(mip_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!draw_lod(mip_point_dest, mip_point))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mip_point_left = igpu_texture_read(static_cast<std::uint64_t>(mip_point_dest), 1, 4, 0, 0);
    const auto mip_point_right = igpu_texture_read(static_cast<std::uint64_t>(mip_point_dest), 6, 4, 0, 0);
    std::printf("mip point    : %lld / %lld\n", static_cast<long long>(mip_point_left),
                static_cast<long long>(mip_point_right));
    if (mip_point_left != 65280 || mip_point_right != 65280)
    {
        return fail("nearest mip at lod 0.5 did not read the green coarser level");
    }
    const auto mip_fine = igpu_sampler_state_create(0, 0, 1, 0, 0, 0, 1, 0, 0, 0.f, 1.f, -1.f);
    if (mip_fine == 0)
    {
        const std::string mip_fine_error = igpu_get_last_error();
        return fail(mip_fine_error.c_str());
    }
    if (!draw_lod(mip_fine_dest, mip_fine))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mip_fine_left = igpu_texture_read(static_cast<std::uint64_t>(mip_fine_dest), 1, 4, 0, 0);
    const auto mip_fine_right = igpu_texture_read(static_cast<std::uint64_t>(mip_fine_dest), 6, 4, 0, 0);
    std::printf("mip fine     : %lld / %lld\n", static_cast<long long>(mip_fine_left),
                static_cast<long long>(mip_fine_right));
    if (mip_fine_left != 65280 || mip_fine_right != 65280)
    {
        return fail("finest level 1 with linear mip filtering did not read the green coarser level");
    }
    const auto mip_cap = igpu_sampler_state_create(0, 0, 1, 0, 0, 0, 1, 0, 0, 0.f, 0.f, 0.f);
    if (mip_cap == 0)
    {
        const std::string mip_cap_error = igpu_get_last_error();
        return fail(mip_cap_error.c_str());
    }
    if (!draw_lod(mip_cap_dest, mip_cap))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mip_cap_left = igpu_texture_read(static_cast<std::uint64_t>(mip_cap_dest), 1, 4, 0, 0);
    const auto mip_cap_right = igpu_texture_read(static_cast<std::uint64_t>(mip_cap_dest), 6, 4, 0, 0);
    std::printf("mip cap      : %lld / %lld\n", static_cast<long long>(mip_cap_left),
                static_cast<long long>(mip_cap_right));
    if (mip_cap_left != 255 || mip_cap_right != 255)
    {
        return fail("coarsest level 0 with linear mip filtering did not keep the red base level");
    }
    if (!draw_lod(mip_linear_dest, mip_linear))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mip_linear_left = igpu_texture_read(static_cast<std::uint64_t>(mip_linear_dest), 1, 4, 0, 0);
    const auto mip_linear_right = igpu_texture_read(static_cast<std::uint64_t>(mip_linear_dest), 6, 4, 0, 0);
    std::printf("mip linear   : %lld / %lld\n", static_cast<long long>(mip_linear_left),
                static_cast<long long>(mip_linear_right));
    if (mip_linear_left != 32896 || mip_linear_right != 32896)
    {
        return fail("linear mip after the coarsest level did not return to the red-green mix");
    }
    const auto mip_bias = igpu_sampler_state_create(0, 0, 1, 0, 0, 0, 1, 0, 0, 0.5f, 0.f, -1.f);
    if (mip_bias == 0 || !draw_lod(mip_bias_dest, mip_bias))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mip_bias_left = igpu_texture_read(static_cast<std::uint64_t>(mip_bias_dest), 1, 4, 0, 0);
    const auto mip_bias_right = igpu_texture_read(static_cast<std::uint64_t>(mip_bias_dest), 6, 4, 0, 0);
    std::printf("mip bias     : %lld / %lld\n", static_cast<long long>(mip_bias_left),
                static_cast<long long>(mip_bias_right));
    if (mip_bias_left != 65280 || mip_bias_right != 65280)
    {
        return fail("level offset 0.5 with linear mip filtering did not read the green coarser level");
    }
    if (!draw_lod(mip_plain_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mip_plain_left = igpu_texture_read(static_cast<std::uint64_t>(mip_plain_dest), 1, 4, 0, 0);
    const auto mip_plain_right = igpu_texture_read(static_cast<std::uint64_t>(mip_plain_dest), 6, 4, 0, 0);
    std::printf("mip plain    : %lld / %lld\n", static_cast<long long>(mip_plain_left),
                static_cast<long long>(mip_plain_right));
    if (mip_plain_left != 65280 || mip_plain_right != 65280)
    {
        return fail("sampler 0 did not restore nearest mip filtering after the linear mip draw");
    }
    // Same 2x2, texture2DLod at (0.25, 0.5), lod 0.5. Border is blue.
    // Linear mip filtering blends red and green. A leaked border is not 32896.
    const auto border_mip_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mip0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_mip_dest == 0 || border_mip0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mip = igpu_sampler_state_create(0, 0, 1, 3, 3, 3, 1, 16711680, 0, 0.f, 0.f, -1.f);
    if (border_mip == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!draw_lod(border_mip_dest, border_mip))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mip_left = igpu_texture_read(static_cast<std::uint64_t>(border_mip_dest), 1, 4, 0, 0);
    const auto border_mip_right = igpu_texture_read(static_cast<std::uint64_t>(border_mip_dest), 6, 4, 0, 0);
    std::printf("border mip   : %lld / %lld\n", static_cast<long long>(border_mip_left),
                static_cast<long long>(border_mip_right));
    if (border_mip_left != 32896 || border_mip_right != 32896)
    {
        return fail("border linear mip filtering at lod 0.5 was not the red-green mix");
    }
    if (!draw_lod(border_mip0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mip0_left = igpu_texture_read(static_cast<std::uint64_t>(border_mip0_dest), 1, 4, 0, 0);
    const auto border_mip0_right = igpu_texture_read(static_cast<std::uint64_t>(border_mip0_dest), 6, 4, 0, 0);
    std::printf("border mip0  : %lld / %lld\n", static_cast<long long>(border_mip0_left),
                static_cast<long long>(border_mip0_right));
    if (border_mip0_left != 65280 || border_mip0_right != 65280)
    {
        return fail("sampler 0 did not restore nearest mip filtering after the border sampler");
    }
    // Same 2x2 and texture2DLod. Offset 0.5 pushes lod 0.5 onto the green
    // level. The blue border must not leak into that texel. Sampler 0 must
    // not keep the offset.
    const auto border_mbias_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mbias0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_mbias_dest == 0 || border_mbias0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mbias = igpu_sampler_state_create(0, 0, 1, 3, 3, 3, 1, 16711680, 0, 0.5f, 0.f, -1.f);
    if (border_mbias == 0 || !draw_lod(border_mbias_dest, border_mbias))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mbias_left = igpu_texture_read(static_cast<std::uint64_t>(border_mbias_dest), 1, 4, 0, 0);
    const auto border_mbias_right = igpu_texture_read(static_cast<std::uint64_t>(border_mbias_dest), 6, 4, 0, 0);
    std::printf("border mbias : %lld / %lld\n", static_cast<long long>(border_mbias_left),
                static_cast<long long>(border_mbias_right));
    if (border_mbias_left != 65280 || border_mbias_right != 65280)
    {
        return fail("border level offset 0.5 with linear mip filtering did not read the green coarser level");
    }
    if (!draw_lod(border_mbias0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mbias0_left = igpu_texture_read(static_cast<std::uint64_t>(border_mbias0_dest), 1, 4, 0, 0);
    const auto border_mbias0_right = igpu_texture_read(static_cast<std::uint64_t>(border_mbias0_dest), 6, 4, 0, 0);
    std::printf("border mbias0: %lld / %lld\n", static_cast<long long>(border_mbias0_left),
                static_cast<long long>(border_mbias0_right));
    if (border_mbias0_left != 65280 || border_mbias0_right != 65280)
    {
        return fail("sampler 0 did not restore nearest mip filtering after the border level offset");
    }
    // Same 2x2 and texture2DLod at lod 0.5. Finest level 1 clamps that
    // fractional level onto green. The blue border stays outside the texel.
    // The next border sampler has no finest level, so the red-green mix
    // returns. Sampler 0 must not keep the linear mip filter.
    const auto border_mfine_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mfree_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mfine0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_mfine_dest == 0 || border_mfree_dest == 0 || border_mfine0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mfine = igpu_sampler_state_create(0, 0, 1, 3, 3, 3, 1, 16711680, 0, 0.f, 1.f, -1.f);
    if (border_mfine == 0 || !draw_lod(border_mfine_dest, border_mfine))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mfine_left = igpu_texture_read(static_cast<std::uint64_t>(border_mfine_dest), 1, 4, 0, 0);
    const auto border_mfine_right = igpu_texture_read(static_cast<std::uint64_t>(border_mfine_dest), 6, 4, 0, 0);
    std::printf("border mfine : %lld / %lld\n", static_cast<long long>(border_mfine_left),
                static_cast<long long>(border_mfine_right));
    if (border_mfine_left != 65280 || border_mfine_right != 65280)
    {
        return fail("border finest level 1 with linear mip filtering did not read the green coarser level");
    }
    if (!draw_lod(border_mfree_dest, border_mip))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mfree_left = igpu_texture_read(static_cast<std::uint64_t>(border_mfree_dest), 1, 4, 0, 0);
    const auto border_mfree_right = igpu_texture_read(static_cast<std::uint64_t>(border_mfree_dest), 6, 4, 0, 0);
    std::printf("border mfree : %lld / %lld\n", static_cast<long long>(border_mfree_left),
                static_cast<long long>(border_mfree_right));
    if (border_mfree_left != 32896 || border_mfree_right != 32896)
    {
        return fail("border finest level stayed applied after the linear mip draw");
    }
    if (!draw_lod(border_mfine0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mfine0_left = igpu_texture_read(static_cast<std::uint64_t>(border_mfine0_dest), 1, 4, 0, 0);
    const auto border_mfine0_right = igpu_texture_read(static_cast<std::uint64_t>(border_mfine0_dest), 6, 4, 0, 0);
    std::printf("border mfine0: %lld / %lld\n", static_cast<long long>(border_mfine0_left),
                static_cast<long long>(border_mfine0_right));
    if (border_mfine0_left != 65280 || border_mfine0_right != 65280)
    {
        return fail("sampler 0 did not restore nearest mip filtering after the border finest level");
    }
    // Same 2x2 and texture2DLod at lod 0.5. Coarsest level 0 keeps that
    // fractional level on the red base. The blue border stays outside the
    // texel. The next border sampler has no coarsest limit, so the
    // red-green mix returns. Sampler 0 must not keep the linear mip filter.
    const auto border_mcap_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mopen_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mcap0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_mcap_dest == 0 || border_mopen_dest == 0 || border_mcap0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mcap = igpu_sampler_state_create(0, 0, 1, 3, 3, 3, 1, 16711680, 0, 0.f, 0.f, 0.f);
    if (border_mcap == 0 || !draw_lod(border_mcap_dest, border_mcap))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mcap_left = igpu_texture_read(static_cast<std::uint64_t>(border_mcap_dest), 1, 4, 0, 0);
    const auto border_mcap_right = igpu_texture_read(static_cast<std::uint64_t>(border_mcap_dest), 6, 4, 0, 0);
    std::printf("border mcap  : %lld / %lld\n", static_cast<long long>(border_mcap_left),
                static_cast<long long>(border_mcap_right));
    if (border_mcap_left != 255 || border_mcap_right != 255)
    {
        return fail("border coarsest level 0 with linear mip filtering did not keep the red base level");
    }
    if (!draw_lod(border_mopen_dest, border_mip))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mopen_left = igpu_texture_read(static_cast<std::uint64_t>(border_mopen_dest), 1, 4, 0, 0);
    const auto border_mopen_right = igpu_texture_read(static_cast<std::uint64_t>(border_mopen_dest), 6, 4, 0, 0);
    std::printf("border mopen : %lld / %lld\n", static_cast<long long>(border_mopen_left),
                static_cast<long long>(border_mopen_right));
    if (border_mopen_left != 32896 || border_mopen_right != 32896)
    {
        return fail("border coarsest level stayed applied after the linear mip draw");
    }
    if (!draw_lod(border_mcap0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mcap0_left = igpu_texture_read(static_cast<std::uint64_t>(border_mcap0_dest), 1, 4, 0, 0);
    const auto border_mcap0_right = igpu_texture_read(static_cast<std::uint64_t>(border_mcap0_dest), 6, 4, 0, 0);
    std::printf("border mcap0 : %lld / %lld\n", static_cast<long long>(border_mcap0_left),
                static_cast<long long>(border_mcap0_right));
    if (border_mcap0_left != 65280 || border_mcap0_right != 65280)
    {
        return fail("sampler 0 did not restore nearest mip filtering after the border coarsest level");
    }
    // Level 0 is red on the left and blue on the right. Level 1 is green.
    // texture2DLod at (0.4, 0.5) with lod 0.5. Nearest minification stays on
    // the red texel. Linear minification mixes that seam with green.
    const auto tri_source = igpu_texture_create(0, 2, 2, 1, 6, true, false, 2);
    const auto tri_near_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto tri_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto tri_plain_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (tri_source == 0 || tri_near_dest == 0 || tri_dest == 0 || tri_plain_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(colour_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(colour_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto tri_target = encode_u64(static_cast<std::uint64_t>(tri_source));
    const auto tri_zero = encode_u64(0);
    const auto tri_mip1 = encode_u64(1);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(colour_buffer),
                                     static_cast<std::uint64_t>(colour_layout), 4, 0, 12, as_array(tri_target),
                                     as_array(tri_zero), as_array(tri_zero), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(lod_green_buffer),
                                     static_cast<std::uint64_t>(colour_layout), 4, 0, 6, as_array(tri_target),
                                     as_array(tri_zero), as_array(tri_mip1), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(tri_source), 0, 0, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(tri_source), 1, 0, 0, 0) != 16711680 ||
        igpu_texture_read(static_cast<std::uint64_t>(tri_source), 0, 0, 0, 1) != 65280)
    {
        return fail("trilinear source was not red, blue, then green");
    }
    const char* tri_ps =
        "#version 120\n"
        "#extension GL_ARB_shader_texture_lod : require\n"
        "uniform sampler2D igpu_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_FragColor = texture2DLod(igpu_tex, vec2(0.4, 0.5), 0.5);\n"
        "}\n";
    const auto tri_frag = igpu_shader_compile(tri_ps, "main", 1, "glsl");
    if (tri_frag == 0 || !igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(tri_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto tri_near = igpu_sampler_state_create(0, 0, 1, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto tri_linear = igpu_sampler_state_create(0, 1, 1, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (tri_near == 0 || tri_linear == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto draw_tri = [&](std::int64_t dest, std::int64_t sampler) -> bool {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(dest), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer),
                                            static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                                            static_cast<std::uint64_t>(tri_source), 0, 0, 0, sampler);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    if (!draw_tri(tri_near_dest, tri_near))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto tri_near_left = igpu_texture_read(static_cast<std::uint64_t>(tri_near_dest), 1, 4, 0, 0);
    const auto tri_near_right = igpu_texture_read(static_cast<std::uint64_t>(tri_near_dest), 6, 4, 0, 0);
    if (tri_near_left != 32896 || tri_near_right != 32896)
    {
        return fail("nearest minification at u=0.4 did not mix red with the green coarser level");
    }
    if (!draw_tri(tri_dest, tri_linear))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto tri_left = igpu_texture_read(static_cast<std::uint64_t>(tri_dest), 1, 4, 0, 0);
    const auto tri_right = igpu_texture_read(static_cast<std::uint64_t>(tri_dest), 6, 4, 0, 0);
    std::printf("mip tri      : %lld / %lld\n", static_cast<long long>(tri_left), static_cast<long long>(tri_right));
    if (tri_left != 2523225 || tri_right != 2523225)
    {
        return fail("linear minification with linear mip filtering did not mix the seam with green");
    }
    if (!draw_tri(tri_plain_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto tri_plain_left = igpu_texture_read(static_cast<std::uint64_t>(tri_plain_dest), 1, 4, 0, 0);
    const auto tri_plain_right = igpu_texture_read(static_cast<std::uint64_t>(tri_plain_dest), 6, 4, 0, 0);
    if (tri_plain_left != 65280 || tri_plain_right != 65280)
    {
        return fail("sampler 0 did not restore nearest mip filtering after trilinear sampling");
    }
    // Same 2x2 and texture2DLod at (0.4, 0.5), lod 0.5. Linear minification
    // mixes the red-blue seam with the green coarser level. Level 1 is 1x1,
    // so its linear filter at u=0.4 also takes 0.1 of the blue border.
    // That is red 89, green 115, blue 51. Clamp-to-edge would stay at
    // 2523225. The next border sampler uses nearest minification, so the
    // red-green mix returns. Sampler 0 must not keep the linear min filter.
    const auto border_mtri_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mnear_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mtri0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_mtri_dest == 0 || border_mnear_dest == 0 || border_mtri0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mtri = igpu_sampler_state_create(0, 1, 1, 3, 3, 3, 1, 16711680, 0, 0.f, 0.f, -1.f);
    if (border_mtri == 0 || !draw_tri(border_mtri_dest, border_mtri))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mtri_left = igpu_texture_read(static_cast<std::uint64_t>(border_mtri_dest), 1, 4, 0, 0);
    const auto border_mtri_right = igpu_texture_read(static_cast<std::uint64_t>(border_mtri_dest), 6, 4, 0, 0);
    std::printf("border mtri  : %lld / %lld\n", static_cast<long long>(border_mtri_left),
                static_cast<long long>(border_mtri_right));
    if (border_mtri_left != 3371865 || border_mtri_right != 3371865)
    {
        return fail("border linear minification with linear mip filtering did not mix the seam, green, and border");
    }
    if (!draw_tri(border_mnear_dest, border_mip))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mnear_left = igpu_texture_read(static_cast<std::uint64_t>(border_mnear_dest), 1, 4, 0, 0);
    const auto border_mnear_right = igpu_texture_read(static_cast<std::uint64_t>(border_mnear_dest), 6, 4, 0, 0);
    std::printf("border mnear : %lld / %lld\n", static_cast<long long>(border_mnear_left),
                static_cast<long long>(border_mnear_right));
    if (border_mnear_left != 32896 || border_mnear_right != 32896)
    {
        return fail("border linear minification stayed applied after the nearest minification draw");
    }
    if (!draw_tri(border_mtri0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mtri0_left = igpu_texture_read(static_cast<std::uint64_t>(border_mtri0_dest), 1, 4, 0, 0);
    const auto border_mtri0_right = igpu_texture_read(static_cast<std::uint64_t>(border_mtri0_dest), 6, 4, 0, 0);
    std::printf("border mtri0 : %lld / %lld\n", static_cast<long long>(border_mtri0_left),
                static_cast<long long>(border_mtri0_right));
    if (border_mtri0_left != 65280 || border_mtri0_right != 65280)
    {
        return fail("sampler 0 did not restore nearest mip filtering after the border trilinear draw");
    }
    // Same 2x2. The left half uses a tiny derivative, so it takes
    // magnification: linear magnification of level 0 at u=0.4 is the 178/77
    // seam. The right half uses texture2DLod at lod 0.5, which takes
    // minification. Nearest minification with linear mip filtering blends the
    // red texel with the green coarser level. A two-level texture is created
    // with nearest mip filtering, so lod 0.5 on sampler 0 is green. A
    // derivative of 1 selects level 1 for both of those min filters.
    const auto mag_mip_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto mag_mip0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (mag_mip_dest == 0 || mag_mip0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const char* mag_mip_ps =
        "#version 120\n"
        "#extension GL_ARB_shader_texture_lod : require\n"
        "uniform sampler2D igpu_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  vec4 magnified = texture2DGradARB(igpu_tex, vec2(0.4, 0.5), vec2(0.001, 0.0), vec2(0.0, 0.001));\n"
        "  vec4 minified = texture2DLod(igpu_tex, vec2(0.4, 0.5), 0.5);\n"
        "  gl_FragColor = v_uv.x < 0.5 ? magnified : minified;\n"
        "}\n";
    const auto mag_mip_frag = igpu_shader_compile(mag_mip_ps, "main", 1, "glsl");
    if (mag_mip_frag == 0 || !igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(mag_mip_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mag_mip = igpu_sampler_state_create(1, 0, 1, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (mag_mip == 0 || !draw_tri(mag_mip_dest, mag_mip))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mag_mip_left = igpu_texture_read(static_cast<std::uint64_t>(mag_mip_dest), 1, 4, 0, 0);
    const auto mag_mip_right = igpu_texture_read(static_cast<std::uint64_t>(mag_mip_dest), 6, 4, 0, 0);
    std::printf("mag mip      : %lld / %lld\n", static_cast<long long>(mag_mip_left),
                static_cast<long long>(mag_mip_right));
    if (mag_mip_left != 5046450 || mag_mip_right != 32896)
    {
        return fail("linear magnification with linear mip filtering was not the 178/77 seam and the red-green mix");
    }
    if (!draw_tri(mag_mip0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto mag_mip0_left = igpu_texture_read(static_cast<std::uint64_t>(mag_mip0_dest), 1, 4, 0, 0);
    const auto mag_mip0_right = igpu_texture_read(static_cast<std::uint64_t>(mag_mip0_dest), 6, 4, 0, 0);
    std::printf("mag mip0     : %lld / %lld\n", static_cast<long long>(mag_mip0_left),
                static_cast<long long>(mag_mip0_right));
    if (mag_mip0_left != 255 || mag_mip0_right != 65280)
    {
        return fail("sampler 0 did not restore nearest magnification and nearest mip filtering");
    }
    // Same 2x2 and the same shader. Magnification, minification, and mip
    // filtering are all linear. The left half is still the 178/77 seam.
    // The right half is texture2DLod at (0.4, 0.5), lod 0.5. Linear
    // minification mixes the red-blue seam into that level, then linear
    // mip filtering mixes the result with green. Nearest minification
    // would stay at 32896. Sampler 0 must restore both filters.
    const auto tri_mag_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto tri_mag0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (tri_mag_dest == 0 || tri_mag0_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto tri_mag = igpu_sampler_state_create(1, 1, 1, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (tri_mag == 0 || !draw_tri(tri_mag_dest, tri_mag))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto tri_mag_left = igpu_texture_read(static_cast<std::uint64_t>(tri_mag_dest), 1, 4, 0, 0);
    const auto tri_mag_right = igpu_texture_read(static_cast<std::uint64_t>(tri_mag_dest), 6, 4, 0, 0);
    std::printf("tri mag      : %lld / %lld\n", static_cast<long long>(tri_mag_left),
                static_cast<long long>(tri_mag_right));
    if (tri_mag_left != 5046450 || tri_mag_right != 2523225)
    {
        return fail("linear magnification with linear minification and linear mip filtering was not the 178/77 seam and the seam-green mix");
    }
    if (!draw_tri(tri_mag0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto tri_mag0_left = igpu_texture_read(static_cast<std::uint64_t>(tri_mag0_dest), 1, 4, 0, 0);
    const auto tri_mag0_right = igpu_texture_read(static_cast<std::uint64_t>(tri_mag0_dest), 6, 4, 0, 0);
    std::printf("tri mag0     : %lld / %lld\n", static_cast<long long>(tri_mag0_left),
                static_cast<long long>(tri_mag0_right));
    if (tri_mag0_left != 255 || tri_mag0_right != 65280)
    {
        return fail("sampler 0 did not restore nearest magnification and nearest mip filtering after trilinear magnification");
    }
    // Same 2x2 and a green border. The left half samples u=1.0 with a tiny
    // derivative, so linear magnification mixes the blue edge texel with
    // that border. The right half uses texture2DLod at (0.4, 0.5), lod 0.5.
    // Nearest minification stays on the red texel and ignores the border.
    // Linear mip filtering then blends it with the green coarser level.
    // Sampler 0 on this shader is the blue edge and green level 1. Sampler 0
    // on the previous u=0.4 shader is pure red, so a stuck linear
    // magnification filter cannot hide behind the edge texel.
    const auto border_magm_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mag0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_mag1_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_magm_dest == 0 || border_mag0_dest == 0 || border_mag1_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const char* border_magm_ps =
        "#version 120\n"
        "#extension GL_ARB_shader_texture_lod : require\n"
        "uniform sampler2D igpu_tex;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  vec4 magnified = texture2DGradARB(igpu_tex, vec2(1.0, 0.5), vec2(0.001, 0.0), vec2(0.0, 0.001));\n"
        "  vec4 minified = texture2DLod(igpu_tex, vec2(0.4, 0.5), 0.5);\n"
        "  gl_FragColor = v_uv.x < 0.5 ? magnified : minified;\n"
        "}\n";
    const auto border_magm_frag = igpu_shader_compile(border_magm_ps, "main", 1, "glsl");
    if (border_magm_frag == 0 || !igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(border_magm_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_magm = igpu_sampler_state_create(1, 0, 1, 3, 3, 3, 1, 65280, 0, 0.f, 0.f, -1.f);
    if (border_magm == 0 || !draw_tri(border_magm_dest, border_magm))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_magm_left = igpu_texture_read(static_cast<std::uint64_t>(border_magm_dest), 1, 4, 0, 0);
    const auto border_magm_right = igpu_texture_read(static_cast<std::uint64_t>(border_magm_dest), 6, 4, 0, 0);
    std::printf("border magm  : %lld / %lld\n", static_cast<long long>(border_magm_left),
                static_cast<long long>(border_magm_right));
    if (border_magm_left != 8421376 || border_magm_right != 32896)
    {
        return fail("border linear magnification with linear mip filtering was not the green-blue edge and the red-green mix");
    }
    if (!draw_tri(border_mag0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mag0_left = igpu_texture_read(static_cast<std::uint64_t>(border_mag0_dest), 1, 4, 0, 0);
    const auto border_mag0_right = igpu_texture_read(static_cast<std::uint64_t>(border_mag0_dest), 6, 4, 0, 0);
    std::printf("border mag0  : %lld / %lld\n", static_cast<long long>(border_mag0_left),
                static_cast<long long>(border_mag0_right));
    if (border_mag0_left != 16711680 || border_mag0_right != 65280)
    {
        return fail("sampler 0 did not restore the blue edge and nearest mip filtering after the border draw");
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(mag_mip_frag), 1) || !draw_tri(border_mag1_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_mag1_left = igpu_texture_read(static_cast<std::uint64_t>(border_mag1_dest), 1, 4, 0, 0);
    const auto border_mag1_right = igpu_texture_read(static_cast<std::uint64_t>(border_mag1_dest), 6, 4, 0, 0);
    std::printf("border mag1  : %lld / %lld\n", static_cast<long long>(border_mag1_left),
                static_cast<long long>(border_mag1_right));
    if (border_mag1_left != 255 || border_mag1_right != 65280)
    {
        return fail("sampler 0 did not restore nearest magnification after the border draw");
    }
    // Same 2x2, green border, and the border magnification shader. The left
    // half samples u=1.0 with a tiny derivative, so linear magnification
    // mixes the blue edge texel with the green border. The right half uses
    // texture2DLod at (0.4, 0.5), lod 0.5. Linear minification mixes the
    // red-blue seam into that level, then linear mip filtering mixes the
    // result with the green coarser level. The green border matches that
    // coarse texel, so the right pixel stays at the clamp trilinear value.
    // Nearest minification would stay at the red-green mix. Sampler 0 on
    // this shader is the blue edge and green level 1. Sampler 0 on the
    // u=0.4 shader is pure red, so a stuck linear magnification filter
    // cannot hide behind the clamped edge texel.
    const auto border_trim_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_trim0_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto border_trim1_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (border_trim_dest == 0 || border_trim0_dest == 0 || border_trim1_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(border_magm_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_trim = igpu_sampler_state_create(1, 1, 1, 3, 3, 3, 1, 65280, 0, 0.f, 0.f, -1.f);
    if (border_trim == 0 || !draw_tri(border_trim_dest, border_trim))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_trim_left = igpu_texture_read(static_cast<std::uint64_t>(border_trim_dest), 1, 4, 0, 0);
    const auto border_trim_right = igpu_texture_read(static_cast<std::uint64_t>(border_trim_dest), 6, 4, 0, 0);
    std::printf("border trim  : %lld / %lld\n", static_cast<long long>(border_trim_left),
                static_cast<long long>(border_trim_right));
    if (border_trim_left != 8421376 || border_trim_right != 2523225)
    {
        return fail("border linear magnification with linear minification and linear mip filtering was not the green-blue edge and the seam-green mix");
    }
    if (!draw_tri(border_trim0_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_trim0_left = igpu_texture_read(static_cast<std::uint64_t>(border_trim0_dest), 1, 4, 0, 0);
    const auto border_trim0_right = igpu_texture_read(static_cast<std::uint64_t>(border_trim0_dest), 6, 4, 0, 0);
    std::printf("border trim0 : %lld / %lld\n", static_cast<long long>(border_trim0_left),
                static_cast<long long>(border_trim0_right));
    if (border_trim0_left != 16711680 || border_trim0_right != 65280)
    {
        return fail("sampler 0 did not restore the blue edge and nearest mip filtering after border trilinear magnification");
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(mag_mip_frag), 1) || !draw_tri(border_trim1_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto border_trim1_left = igpu_texture_read(static_cast<std::uint64_t>(border_trim1_dest), 1, 4, 0, 0);
    const auto border_trim1_right = igpu_texture_read(static_cast<std::uint64_t>(border_trim1_dest), 6, 4, 0, 0);
    std::printf("border trim1 : %lld / %lld\n", static_cast<long long>(border_trim1_left),
                static_cast<long long>(border_trim1_right));
    if (border_trim1_left != 255 || border_trim1_right != 65280)
    {
        return fail("sampler 0 did not restore nearest magnification after border trilinear magnification");
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!write_uv(1.0625f, 0.5f))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_state_release(static_cast<std::uint64_t>(wrap_repeat)) ||
        igpu_state_release(static_cast<std::uint64_t>(wrap_repeat)) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        return fail("sampler release did not retire the handle");
    }
    if (sample_uv(wrap_black_dest, wrap_repeat) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        return fail("released sampler was accepted");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 1, 4, 0, 0) != 0)
    {
        return fail("released sampler changed pixels");
    }
    if (!sample_uv(wrap_clamp_dest, wrap_clamp) ||
        igpu_texture_read(static_cast<std::uint64_t>(wrap_clamp_dest), 1, 4, 0, 0) != 16711680)
    {
        return fail("releasing the repeat sampler broke the clamp sampler");
    }
    if (!igpu_supports(static_cast<std::int32_t>(igpu::Capability::SamplerState)))
    {
        return fail("SamplerState is false on the OpenGL backend");
    }

    const auto blend_bad_factor = igpu_blend_state_create(
        true, 99, 2, 1, 2, 2, 1, true, true, true, true);
    if (blend_bad_factor != 0 ||
        std::string(igpu_get_last_error()).find("bm_zero through bm_src_alpha_sat") == std::string::npos)
    {
        return fail("unknown blend factor was accepted");
    }
    const auto blend_bad_equation = igpu_blend_state_create(
        true, 2, 2, 99, 2, 2, 1, true, true, true, true);
    if (blend_bad_equation != 0 ||
        std::string(igpu_get_last_error()).find("bm_eq_") == std::string::npos)
    {
        return fail("unknown blend equation was accepted");
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(colour_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(colour_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto blend_src_alpha = igpu_blend_state_create(
        true, 5, 6, 1, 5, 6, 1, true, true, true, true);
    const auto blend_max = igpu_blend_state_create(
        true, 2, 2, 2, 2, 2, 2, true, true, true, true);
    const auto blend_mask = igpu_blend_state_create(
        false, 2, 2, 1, 2, 2, 1, false, true, true, true);
    if (blend_src_alpha == 0 || blend_max == 0 || blend_mask == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }

    const float blend_pos[] = {
        -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f, 1.f,
    };
    const auto fill_blend = [&](ColourVertex* out, unsigned char r, unsigned char g, unsigned char b,
                                unsigned char a) {
        for (int i = 0; i < 6; ++i)
        {
            out[i].x = blend_pos[i * 2];
            out[i].y = blend_pos[i * 2 + 1];
            out[i].r = r;
            out[i].g = g;
            out[i].b = b;
            out[i].a = a;
        }
    };
    ColourVertex blend_blue_vertices[6] = {};
    ColourVertex blend_blue0_vertices[6] = {};
    ColourVertex blend_red0_vertices[6] = {};
    ColourVertex blend_red_vertices[6] = {};
    fill_blend(blend_blue_vertices, 0, 0, 255, 255);
    fill_blend(blend_blue0_vertices, 0, 0, 255, 0);
    fill_blend(blend_red0_vertices, 255, 0, 0, 0);
    fill_blend(blend_red_vertices, 255, 0, 0, 255);
    const auto blend_blue_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(blend_blue_vertices)), 0, 1, 12);
    const auto blend_blue0_buffer =
        igpu_buffer_create(static_cast<std::int64_t>(sizeof(blend_blue0_vertices)), 0, 1, 12);
    const auto blend_red0_buffer =
        igpu_buffer_create(static_cast<std::int64_t>(sizeof(blend_red0_vertices)), 0, 1, 12);
    const auto blend_red_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(blend_red_vertices)), 0, 1, 12);
    const std::uint16_t blend_indices[] = {0, 1, 2, 3, 4, 5};
    const auto blend_index_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(blend_indices)), 0, 2, 0);
    if (blend_blue_buffer == 0 || blend_blue0_buffer == 0 || blend_red0_buffer == 0 || blend_red_buffer == 0 ||
        blend_index_buffer == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(blend_blue_buffer), 0,
                           gm::wire::GMBuffer(blend_blue_vertices, sizeof(blend_blue_vertices))) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(blend_blue0_buffer), 0,
                           gm::wire::GMBuffer(blend_blue0_vertices, sizeof(blend_blue0_vertices))) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(blend_red0_buffer), 0,
                           gm::wire::GMBuffer(blend_red0_vertices, sizeof(blend_red0_vertices))) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(blend_red_buffer), 0,
                           gm::wire::GMBuffer(blend_red_vertices, sizeof(blend_red_vertices))) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(blend_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(blend_indices), sizeof(blend_indices))))
    {
        return fail(igpu_get_last_error().c_str());
    }

    const auto report_blend = [](const char* label, std::int64_t texture, std::int64_t expect) {
        const auto left = igpu_texture_read(static_cast<std::uint64_t>(texture), 1, 4, 0, 0);
        const auto right = igpu_texture_read(static_cast<std::uint64_t>(texture), 6, 4, 0, 0);
        std::printf("%s: %lld / %lld\n", label, static_cast<long long>(left), static_cast<long long>(right));
        return left == expect && right == expect;
    };
    const auto blend_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto blend_indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto blend_target_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto blend_mask_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (blend_texture == 0 || blend_indexed_texture == 0 || blend_target_texture == 0 || blend_mask_texture == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }

    std::int32_t blend_saved = 0;
    std::int32_t blend_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_texture), blend_saved, blend_viewport))
    {
        return fail("blend target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_blue_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0,
                   6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend base    ", blend_texture, 16711680))
    {
        return fail("opaque blue did not cover the blend target");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_texture), blend_saved, blend_viewport))
    {
        return fail("blend hold target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_red0_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0,
                   6, 1, blend_src_alpha, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (glIsEnabled(0x0BE2) == GL_TRUE)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("src-alpha draw left blending enabled");
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend hold    ", blend_texture, 16711680))
    {
        return fail("src-alpha with alpha 0 replaced the destination");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_texture), blend_saved, blend_viewport))
    {
        return fail("blend replace target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_red_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                   1, blend_src_alpha, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend replace ", blend_texture, 255))
    {
        return fail("src-alpha with alpha 255 did not replace the destination");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_texture), blend_saved, blend_viewport))
    {
        return fail("blend plain target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_blue0_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0,
                   6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (glIsEnabled(0x0BE2) == GL_TRUE)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("handle 0 enabled blending");
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend plain   ", blend_texture, 16711680))
    {
        return fail("handle 0 with alpha 0 did not replace the colour");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_texture), blend_saved, blend_viewport))
    {
        return fail("blend max target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_red_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                   1, blend_max, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (glIsEnabled(0x0BE2) == GL_TRUE)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("max draw left blending enabled");
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend max     ", blend_texture, 16711935))
    {
        return fail("max of red and blue was not fuchsia");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_texture), blend_saved, blend_viewport))
    {
        return fail("blend max off target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_red0_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0,
                   6, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend max off ", blend_texture, 255))
    {
        return fail("handle 0 after max did not write red");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_indexed_texture), blend_saved, blend_viewport))
    {
        return fail("blend indexed target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_blue_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0,
                   6, 1, 0, 0, 0, 0) ||
        !igpu_draw_indexed(static_cast<std::uint64_t>(blend_red0_buffer), static_cast<std::uint64_t>(colour_layout),
                           static_cast<std::uint64_t>(blend_index_buffer), 4, 0, 6, blend_src_alpha, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend indexed ", blend_indexed_texture, 16711680))
    {
        return fail("indexed src-alpha with alpha 0 replaced the destination");
    }

    const auto blend_target_bytes = encode_u64(static_cast<std::uint64_t>(blend_target_texture));
    const auto blend_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(blend_blue_buffer),
                                     static_cast<std::uint64_t>(colour_layout), 4, 0, 6, as_array(blend_target_bytes),
                                     as_array(blend_zero_bytes), as_array(blend_zero_bytes), 0, 0, 0, 0) ||
        !igpu_draw_to_render_targets(static_cast<std::uint64_t>(blend_red0_buffer),
                                     static_cast<std::uint64_t>(colour_layout), 4, 0, 6, as_array(blend_target_bytes),
                                     as_array(blend_zero_bytes), as_array(blend_zero_bytes), blend_src_alpha, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!report_blend("blend target  ", blend_target_texture, 16711680))
    {
        return fail("draw-to-targets src-alpha with alpha 0 replaced the destination");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_mask_texture), blend_saved, blend_viewport))
    {
        return fail("blend mask target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_red_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                   1, blend_mask, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    GLboolean blend_mask_restored[4] = {};
    glGetBooleanv(0x0C23, blend_mask_restored);
    if (glIsEnabled(0x0BE2) == GL_TRUE || !blend_mask_restored[0] || !blend_mask_restored[1] ||
        !blend_mask_restored[2] || !blend_mask_restored[3])
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("masked draw left blending on or a colour channel masked");
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend mask    ", blend_mask_texture, 0))
    {
        return fail("a disabled blend with red writes off painted red");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_mask_texture), blend_saved, blend_viewport))
    {
        return fail("blend unmask target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(blend_red_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                   1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (!report_blend("blend unmask  ", blend_mask_texture, 255))
    {
        return fail("handle 0 did not restore red writes");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_mask_texture), blend_saved, blend_viewport))
    {
        return fail("blend reject target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(blend_blue_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                  1, 999999, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown blend handle") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("unknown blend handle was accepted");
    }
    if (igpu_draw(static_cast<std::uint64_t>(blend_blue_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                  1, -1, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("not a state object") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("a negative blend handle was accepted");
    }
    if (igpu_draw(static_cast<std::uint64_t>(blend_blue_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                  1, 0, 1, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("a depth handle was accepted with a blend backend");
    }
    if (igpu_draw(static_cast<std::uint64_t>(blend_blue_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                  1, 0, 0, 1, 0) ||
        std::string(igpu_get_last_error()).find("unknown raster handle") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("an unknown raster handle was accepted with a blend backend");
    }
    const auto blend_sample_red0 = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto blend_sample_red = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto blend_sample_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto blend_sample_repeat = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto blend_sample_clamp = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto blend_sample_repeat_sampler =
        igpu_sampler_state_create(0, 0, 0, 1, 1, 1, 1, 0, 0, 0.f, 0.f, -1.f);
    if (blend_sample_red0 == 0 || blend_sample_red == 0 || blend_sample_dest == 0 || blend_sample_repeat == 0 ||
        blend_sample_clamp == 0 || blend_sample_repeat_sampler == 0)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    const auto paint_colour = [&](std::int64_t texture, std::int64_t buffer) {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(texture), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(buffer), 0, static_cast<std::uint64_t>(colour_layout),
                                     4, 0, 6, 1, 0, 0, 0, 0);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    if (!paint_colour(blend_sample_red0, blend_red0_buffer) || !paint_colour(blend_sample_red, blend_red_buffer) ||
        !paint_colour(blend_sample_dest, blend_blue_buffer) ||
        !paint_colour(blend_sample_repeat, blend_blue_buffer) ||
        !paint_colour(blend_sample_clamp, blend_blue_buffer))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    const auto sample_blend = [&](std::int64_t dest, std::int64_t source, std::int64_t blend, std::int64_t sampler,
                                   std::uint64_t buffer) {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(dest), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw_sampled(buffer, static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                                             static_cast<std::uint64_t>(source), blend, 0, 0, sampler);
        const bool restored = glIsEnabled(0x0BE2) == GL_FALSE;
        igpu::gl_color_target_end(saved, viewport);
        return drew && restored;
    };
    if (!sample_blend(blend_sample_dest, blend_sample_red0, blend_src_alpha, 0,
                      static_cast<std::uint64_t>(sample_buffer)))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (!report_blend("sample blend0 ", blend_sample_dest, 16711680))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled src-alpha with alpha 0 replaced the destination");
    }
    if (!sample_blend(blend_sample_dest, blend_sample_red, blend_src_alpha, 0,
                      static_cast<std::uint64_t>(sample_buffer)))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (!report_blend("sample blend1 ", blend_sample_dest, 255))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled src-alpha with alpha 255 did not replace the destination");
    }
    if (!sample_blend(blend_sample_dest, blend_sample_red0, 0, 0, static_cast<std::uint64_t>(sample_buffer)))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (!report_blend("sample plain  ", blend_sample_dest, 255))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled handle 0 with alpha 0 did not write red");
    }
    if (!write_uv(1.0625f, 0.5f) ||
        !sample_blend(blend_sample_repeat, sample_source, blend_src_alpha, blend_sample_repeat_sampler,
                      static_cast<std::uint64_t>(wrap_buffer)))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (!report_blend("sample repeat ", blend_sample_repeat, 255))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled blend did not apply repeat wrapping");
    }
    if (!sample_blend(blend_sample_clamp, sample_source, 0, 0, static_cast<std::uint64_t>(wrap_buffer)))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    if (!report_blend("sample clamped", blend_sample_clamp, 16711680))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled blend left the repeat wrap in place");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0,
                          6, static_cast<std::uint64_t>(blend_sample_red), 999999, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown blend handle") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled draw accepted an unknown blend handle");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0,
                          6, static_cast<std::uint64_t>(blend_sample_red), -1, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("not a state object") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled draw accepted a negative blend handle");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0,
                          6, static_cast<std::uint64_t>(blend_sample_red), 0, 1, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled draw accepted an unknown depth handle");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0,
                          6, static_cast<std::uint64_t>(blend_sample_red), 0, 0, 1, 0) ||
        std::string(igpu_get_last_error()).find("unknown raster handle") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("sampled draw accepted an unknown raster handle");
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(colour_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(colour_frag), 1))
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(blend_mask_texture), 1, 4, 0, 0) != 255)
    {
        return fail("a rejected blend draw changed pixels");
    }
    if (!igpu_state_release(static_cast<std::uint64_t>(blend_src_alpha)) ||
        igpu_state_release(static_cast<std::uint64_t>(blend_src_alpha)) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        return fail("blend release did not retire the handle");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(blend_mask_texture), blend_saved, blend_viewport))
    {
        return fail("released blend target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(blend_blue_buffer), 0, static_cast<std::uint64_t>(colour_layout), 4, 0, 6,
                  1, blend_src_alpha, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown blend handle") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("a released blend handle was accepted");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0,
                          6, static_cast<std::uint64_t>(blend_sample_red), blend_src_alpha, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown blend handle") == std::string::npos)
    {
        igpu::gl_color_target_end(blend_saved, blend_viewport);
        return fail("a sampled draw accepted a released blend handle");
    }
    igpu::gl_color_target_end(blend_saved, blend_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(blend_mask_texture), 1, 4, 0, 0) != 255)
    {
        return fail("a released blend handle changed pixels");
    }
    if (!igpu_supports(static_cast<std::int32_t>(igpu::Capability::BlendState)))
    {
        return fail("BlendState is false on the OpenGL backend");
    }

    const auto cull_clockwise = igpu_raster_state_create(1, 0, false, true);
    const auto cull_counter = igpu_raster_state_create(2, 0, false, true);
    const auto cull_none = igpu_raster_state_create(0, 0, false, true);
    if (cull_clockwise == 0 || cull_counter == 0 || cull_none == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_raster_state_create(99, 0, false, true) != 0 ||
        std::string(igpu_get_last_error()).find("cull_noculling") == std::string::npos)
    {
        return fail("an unknown cull mode was accepted");
    }
    if (igpu_raster_state_create(0, 1, false, true) != 0 ||
        std::string(igpu_get_last_error()).find("solid") == std::string::npos)
    {
        return fail("wireframe fill was accepted");
    }
    if (igpu_raster_state_create(0, 0, false, false) != 0 ||
        std::string(igpu_get_last_error()).find("depth clip") == std::string::npos)
    {
        return fail("disabled depth clip was accepted");
    }
    const float cull_ccw_vertices[] = {-1.f, -1.f, 0.f, -1.f, -1.f, 1.f};
    const float cull_cw_vertices[] = {0.f, -1.f, -1.f, -1.f, -1.f, 1.f};
    const std::uint16_t cull_indices[] = {0, 1, 2};
    const auto cull_ccw_buffer = igpu_buffer_create(24, 0, 1, 8);
    const auto cull_cw_buffer = igpu_buffer_create(24, 0, 1, 8);
    const auto cull_index_buffer = igpu_buffer_create(6, 0, 2, 0);
    const auto cull_keep_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto cull_drop_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto cull_none_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto cull_front_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto cull_back_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto cull_indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto cull_target_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (cull_ccw_buffer == 0 || cull_cw_buffer == 0 || cull_index_buffer == 0 || cull_keep_texture == 0 ||
        cull_drop_texture == 0 || cull_none_texture == 0 || cull_front_texture == 0 || cull_back_texture == 0 ||
        cull_indexed_texture == 0 || cull_target_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(cull_ccw_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(cull_ccw_vertices), 24)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(cull_cw_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(cull_cw_vertices), 24)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(cull_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(cull_indices), 6)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto report_cull = [&](const char* label, std::int64_t texture, std::int64_t left, std::int64_t right) {
        const auto got_left = igpu_texture_read(static_cast<std::uint64_t>(texture), 1, 4, 0, 0);
        const auto got_right = igpu_texture_read(static_cast<std::uint64_t>(texture), 6, 4, 0, 0);
        std::printf("%s: %lld / %lld\n", label, static_cast<long long>(got_left), static_cast<long long>(got_right));
        return got_left == left && got_right == right;
    };
    const auto draw_cull = [&](std::int64_t texture, std::int64_t buffer, std::int64_t raster) {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(texture), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3,
                                     1, 0, 0, raster, 0);
        const bool cull_off = glIsEnabled(0x0B44) == GL_FALSE;
        igpu::gl_color_target_end(saved, viewport);
        return drew && cull_off;
    };
    if (!draw_cull(cull_keep_texture, cull_ccw_buffer, cull_clockwise) ||
        !report_cull("cull keep    ", cull_keep_texture, 255, 0))
    {
        return fail("cull_clockwise dropped a counter-clockwise triangle");
    }
    if (!draw_cull(cull_drop_texture, cull_ccw_buffer, cull_counter) ||
        !report_cull("cull drop    ", cull_drop_texture, 0, 0))
    {
        return fail("cull_counterclockwise kept a counter-clockwise triangle");
    }
    if (!draw_cull(cull_drop_texture, cull_ccw_buffer, 0) ||
        !report_cull("cull plain   ", cull_drop_texture, 255, 0))
    {
        return fail("handle 0 did not restore culling after a dropped triangle");
    }
    if (!draw_cull(cull_none_texture, cull_ccw_buffer, cull_none) ||
        !report_cull("cull none    ", cull_none_texture, 255, 0))
    {
        return fail("cull_noculling dropped a counter-clockwise triangle");
    }
    if (!draw_cull(cull_front_texture, cull_cw_buffer, cull_clockwise) ||
        !report_cull("cull front   ", cull_front_texture, 0, 0))
    {
        return fail("cull_clockwise kept a clockwise triangle");
    }
    if (!draw_cull(cull_back_texture, cull_cw_buffer, cull_counter) ||
        !report_cull("cull back    ", cull_back_texture, 255, 0))
    {
        return fail("cull_counterclockwise dropped a clockwise triangle");
    }
    {
        std::int32_t saved = 0;
        std::int32_t held[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(cull_indexed_texture), saved, held))
        {
            return fail("cull indexed target begin failed");
        }
        const bool drew = igpu_draw_indexed(static_cast<std::uint64_t>(cull_ccw_buffer),
                                             static_cast<std::uint64_t>(layout),
                                             static_cast<std::uint64_t>(cull_index_buffer), 4, 0, 3, 0, 0,
                                             cull_counter, 0);
        const bool cull_off = glIsEnabled(0x0B44) == GL_FALSE;
        igpu::gl_color_target_end(saved, held);
        if (!drew || !cull_off || !report_cull("cull indexed ", cull_indexed_texture, 0, 0))
        {
            return fail("indexed cull_counterclockwise kept a counter-clockwise triangle");
        }
    }
    const auto cull_target_bytes = encode_u64(static_cast<std::uint64_t>(cull_target_texture));
    const auto cull_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(cull_ccw_buffer), static_cast<std::uint64_t>(layout),
                                     4, 0, 3, as_array(cull_target_bytes), as_array(cull_zero_bytes),
                                     as_array(cull_zero_bytes), 0, 0, cull_counter, 0) ||
        glIsEnabled(0x0B44) == GL_TRUE || !report_cull("cull target  ", cull_target_texture, 0, 0))
    {
        return fail("render-target cull_counterclockwise kept a counter-clockwise triangle");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(cull_keep_texture), strip_saved_fbo, strip_held_viewport))
    {
        return fail("cull reject target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(cull_ccw_buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3, 1,
                  0, 0, 999999, 0) ||
        std::string(igpu_get_last_error()).find("unknown raster handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("an unknown raster handle was accepted");
    }
    if (igpu_draw(static_cast<std::uint64_t>(cull_ccw_buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3, 1, 0,
                  0, -1, 0) ||
        std::string(igpu_get_last_error()).find("not a state object") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("a negative raster handle was accepted");
    }
    if (igpu_draw(static_cast<std::uint64_t>(cull_ccw_buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3, 1, 0, 1,
                  0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("a depth handle was accepted with a raster backend");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(cull_ccw_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0,
                          3, static_cast<std::uint64_t>(cull_keep_texture), 0, 0, cull_clockwise, 0) ||
        std::string(igpu_get_last_error()).find("stride does not match") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("a sampled raster draw ignored the position buffer stride");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(cull_keep_texture), 1, 4, 0, 0) != 255)
    {
        return fail("a rejected raster draw changed pixels");
    }
    if (!igpu_state_release(static_cast<std::uint64_t>(cull_none)) ||
        igpu_state_release(static_cast<std::uint64_t>(cull_none)) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        return fail("raster release did not retire the handle");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(cull_keep_texture), strip_saved_fbo, strip_held_viewport))
    {
        return fail("released raster target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(cull_ccw_buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3, 1, 0, 0,
                  cull_none, 0) ||
        std::string(igpu_get_last_error()).find("unknown raster handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("a released raster handle was accepted");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(cull_keep_texture), 1, 4, 0, 0) != 255)
    {
        return fail("a released raster handle changed pixels");
    }
    if (!igpu_supports(static_cast<std::int32_t>(igpu::Capability::RasterState)) ||
        igpu_supports(static_cast<std::int32_t>(igpu::Capability::Wireframe)))
    {
        return fail("RasterState or Wireframe has the wrong value on the OpenGL backend");
    }

    const auto scissor_on = igpu_raster_state_create(0, 0, true, true);
    const auto scissor_cull = igpu_raster_state_create(2, 0, true, true);
    if (scissor_on == 0 || scissor_cull == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    // Caller rectangle is the left half of the 8x8 target. The backend toggles
    // the test and leaves this box alone. The triangle covers the whole target.
    glScissor(0, 0, 4, 8);
    const float scissor_vertices[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
    const std::uint16_t scissor_indices[] = {0, 1, 2};
    const auto scissor_buffer = igpu_buffer_create(24, 0, 1, 8);
    const auto scissor_index_buffer = igpu_buffer_create(6, 0, 2, 0);
    const auto scissor_clip_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto scissor_index_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto scissor_target_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto scissor_drop_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto scissor_held_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto scissor_widen_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto scissor_back_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (scissor_buffer == 0 || scissor_index_buffer == 0 || scissor_clip_texture == 0 || scissor_index_texture == 0 ||
        scissor_target_texture == 0 || scissor_drop_texture == 0 || scissor_held_texture == 0 ||
        scissor_widen_texture == 0 || scissor_back_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(scissor_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(scissor_vertices), 24)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(scissor_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(scissor_indices), 6)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto report_scissor = [&](const char* label, std::int64_t texture, std::int64_t left, std::int64_t right) {
        const auto got_left = igpu_texture_read(static_cast<std::uint64_t>(texture), 1, 4, 0, 0);
        const auto got_right = igpu_texture_read(static_cast<std::uint64_t>(texture), 6, 4, 0, 0);
        std::printf("%s: %lld / %lld\n", label, static_cast<long long>(got_left), static_cast<long long>(got_right));
        return got_left == left && got_right == right;
    };
    const auto scissor_box_kept = []() {
        GLint box[4] = {};
        glGetIntegerv(GL_SCISSOR_BOX, box);
        return box[0] == 0 && box[1] == 0 && box[2] == 4 && box[3] == 8;
    };
    const auto draw_scissor = [&](std::int64_t texture, std::int64_t raster) {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(texture), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(scissor_buffer), 0, static_cast<std::uint64_t>(layout),
                                     4, 0, 3, 1, 0, 0, raster, 0);
        const bool test_off = glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE;
        igpu::gl_color_target_end(saved, viewport);
        return drew && test_off && scissor_box_kept();
    };
    if (!draw_scissor(scissor_clip_texture, scissor_on) ||
        !report_scissor("scissor clip  ", scissor_clip_texture, 255, 0))
    {
        return fail("a scissor raster state drew outside the caller rectangle");
    }
    if (!draw_scissor(scissor_clip_texture, 0) ||
        !report_scissor("scissor plain ", scissor_clip_texture, 255, 255))
    {
        return fail("handle 0 left the scissor test enabled");
    }
    {
        std::int32_t saved = 0;
        std::int32_t held[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(scissor_index_texture), saved, held))
        {
            return fail("scissor indexed target begin failed");
        }
        const bool drew = igpu_draw_indexed(static_cast<std::uint64_t>(scissor_buffer),
                                             static_cast<std::uint64_t>(layout),
                                             static_cast<std::uint64_t>(scissor_index_buffer), 4, 0, 3, 0, 0,
                                             scissor_on, 0);
        const bool test_off = glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE;
        igpu::gl_color_target_end(saved, held);
        if (!drew || !test_off || !scissor_box_kept() ||
            !report_scissor("scissor index ", scissor_index_texture, 255, 0))
        {
            return fail("indexed scissor drew outside the caller rectangle");
        }
    }
    const auto scissor_target_bytes = encode_u64(static_cast<std::uint64_t>(scissor_target_texture));
    const auto scissor_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(scissor_buffer), static_cast<std::uint64_t>(layout), 4,
                                     0, 3, as_array(scissor_target_bytes), as_array(scissor_zero_bytes),
                                     as_array(scissor_zero_bytes), 0, 0, scissor_on, 0) ||
        glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE || !scissor_box_kept() ||
        !report_scissor("scissor target", scissor_target_texture, 255, 0))
    {
        return fail("render-target scissor drew outside the caller rectangle");
    }
    if (!draw_scissor(scissor_drop_texture, scissor_cull) ||
        !report_scissor("scissor drop  ", scissor_drop_texture, 0, 0))
    {
        return fail("scissor kept a counter-clockwise triangle that cull should drop");
    }
    glEnable(GL_SCISSOR_TEST);
    {
        std::int32_t saved = 0;
        std::int32_t scissor_view[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(scissor_held_texture), saved, scissor_view))
        {
            return fail("scissor held target begin failed");
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(scissor_buffer), 0, static_cast<std::uint64_t>(layout),
                                     4, 0, 3, 1, 0, 0, 0, 0);
        const bool test_on = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
        igpu::gl_color_target_end(saved, scissor_view);
        if (!drew || !test_on || !scissor_box_kept() ||
            !report_scissor("scissor held  ", scissor_held_texture, 255, 0))
        {
            return fail("handle 0 changed the caller scissor test");
        }
    }
    {
        std::int32_t saved = 0;
        std::int32_t scissor_view[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(scissor_widen_texture), saved, scissor_view))
        {
            return fail("scissor widen target begin failed");
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(scissor_buffer), 0, static_cast<std::uint64_t>(layout),
                                     4, 0, 3, 1, 0, 0, cull_clockwise, 0);
        const bool test_on = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
        igpu::gl_color_target_end(saved, scissor_view);
        if (!drew || !test_on || !scissor_box_kept() ||
            !report_scissor("scissor widen ", scissor_widen_texture, 255, 255))
        {
            return fail("a solid raster state left the caller scissor test enabled during the draw");
        }
    }
    {
        std::int32_t saved = 0;
        std::int32_t scissor_view[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(scissor_back_texture), saved, scissor_view))
        {
            return fail("scissor back target begin failed");
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(scissor_buffer), 0, static_cast<std::uint64_t>(layout),
                                     4, 0, 3, 1, 0, 0, 0, 0);
        const bool test_on = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
        igpu::gl_color_target_end(saved, scissor_view);
        if (!drew || !test_on || !scissor_box_kept() ||
            !report_scissor("scissor back  ", scissor_back_texture, 255, 0))
        {
            return fail("handle 0 did not see the restored scissor test");
        }
    }
    glDisable(GL_SCISSOR_TEST);
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(scissor_clip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("scissor reject target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(scissor_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0,
                          3, static_cast<std::uint64_t>(scissor_clip_texture), 0, 0, scissor_on, 0) ||
        std::string(igpu_get_last_error()).find("stride does not match") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("a sampled scissor draw ignored the position buffer stride");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(scissor_clip_texture), 6, 4, 0, 0) != 255)
    {
        return fail("a rejected scissor draw changed pixels");
    }
    if (!igpu_state_release(static_cast<std::uint64_t>(scissor_on)) ||
        igpu_state_release(static_cast<std::uint64_t>(scissor_on)) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        return fail("scissor release did not retire the handle");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(scissor_back_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("released scissor target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(scissor_buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3, 1, 0, 0,
                  scissor_on, 0) ||
        std::string(igpu_get_last_error()).find("unknown raster handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("a released scissor handle was accepted");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(scissor_back_texture), 1, 4, 0, 0) != 255)
    {
        return fail("a released scissor handle changed pixels");
    }

    // Near window depth is 0. Far window depth is 0.5. The caller clears depth to 1.
    const auto depth_less = igpu_depth_state_create(true, true, 2, false, 8, 1, 1, 1, 0, 255, 255);
    const auto depth_nowrite = igpu_depth_state_create(true, false, 2, false, 8, 1, 1, 1, 0, 255, 255);
    const auto depth_off = igpu_depth_state_create(false, false, 8, false, 8, 1, 1, 1, 0, 255, 255);
    if (depth_less == 0 || depth_nowrite == 0 || depth_off == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_depth_state_create(true, true, 99, false, 8, 1, 1, 1, 0, 255, 255) != 0 ||
        std::string(igpu_get_last_error()).find("depth function must be a cmpfunc_* constant") == std::string::npos)
    {
        return fail("unknown depth function was accepted");
    }
    if (igpu_depth_state_create(true, true, 2, false, 99, 1, 1, 1, 0, 255, 255) != 0 ||
        std::string(igpu_get_last_error()).find("stencil function must be cmpfunc_*") == std::string::npos)
    {
        return fail("unknown stencil function was accepted");
    }
    if (igpu_depth_state_create(true, true, 2, false, 8, 1, 1, 1, 256, 255, 255) != 0 ||
        std::string(igpu_get_last_error()).find("0..255") == std::string::npos)
    {
        return fail("stencil ref outside 0..255 was accepted");
    }
    if (igpu_depth_state_create(true, true, 2, true, 8, 1, 1, 1, 0, 255, 255) != 0 ||
        std::string(igpu_get_last_error()).find("stencil") == std::string::npos)
    {
        return fail("stencil was enabled");
    }

    const char* depth_near_vs =
        "#version 120\n"
        "attribute vec2 in_pos;\n"
        "void main() { gl_Position = vec4(in_pos, -1.0, 1.0); }\n";
    const char* depth_far_vs =
        "#version 120\n"
        "attribute vec2 in_pos;\n"
        "void main() { gl_Position = vec4(in_pos, 0.0, 1.0); }\n";
    const char* depth_green_ps =
        "#version 120\n"
        "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n";
    const auto depth_near = igpu_shader_compile(depth_near_vs, "main", 0, "glsl");
    const auto depth_far = igpu_shader_compile(depth_far_vs, "main", 0, "glsl");
    const auto depth_green = igpu_shader_compile(depth_green_ps, "main", 1, "glsl");
    const float depth_vertices[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};
    const std::uint16_t depth_indices[] = {0, 1, 2};
    const auto depth_buffer = igpu_buffer_create(24, 0, 1, 8);
    const auto depth_index_buffer = igpu_buffer_create(6, 0, 2, 0);
    const auto depth_less_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto depth_write_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto depth_off_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto depth_plain_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto depth_index_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto depth_target_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto depth_held_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (depth_near == 0 || depth_far == 0 || depth_green == 0 || depth_buffer == 0 || depth_index_buffer == 0 ||
        depth_less_texture == 0 || depth_write_texture == 0 || depth_off_texture == 0 || depth_plain_texture == 0 ||
        depth_index_texture == 0 || depth_target_texture == 0 || depth_held_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(depth_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(depth_vertices), 24)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(depth_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(depth_indices), 6)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    const auto depth_defaults = []() {
        GLint func = 0;
        GLboolean mask = GL_FALSE;
        glGetIntegerv(GL_DEPTH_FUNC, &func);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
        return glIsEnabled(GL_DEPTH_TEST) == GL_FALSE && func == GL_LESS && mask == GL_TRUE;
    };
    const auto report_depth = [&](const char* label, std::int64_t texture, std::int64_t expect) {
        const auto left = igpu_texture_read(static_cast<std::uint64_t>(texture), 1, 4, 0, 0);
        const auto right = igpu_texture_read(static_cast<std::uint64_t>(texture), 6, 4, 0, 0);
        std::printf("%s: %lld / %lld\n", label, static_cast<long long>(left), static_cast<long long>(right));
        return left == expect && right == expect;
    };
    const auto draw_depth = [&](std::int64_t texture, std::int64_t depth) {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(texture), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(depth_buffer), 0, static_cast<std::uint64_t>(layout), 4,
                                     0, 3, 1, 0, depth, 0, 0);
        igpu::gl_color_target_end(saved, viewport);
        return drew && depth_defaults();
    };
    const auto prepare_depth = [&](std::int64_t texture) {
        return igpu::gl_attach_depth(static_cast<std::uint64_t>(texture));
    };
    if (!prepare_depth(depth_less_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1) || !draw_depth(depth_less_texture, depth_less) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_green), 1) ||
        !draw_depth(depth_less_texture, depth_less) || !report_depth("depth less   ", depth_less_texture, 255))
    {
        return fail("a farther triangle passed the less depth test");
    }
    if (!prepare_depth(depth_write_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1) || !draw_depth(depth_write_texture, depth_nowrite) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_green), 1) ||
        !draw_depth(depth_write_texture, depth_less) || !report_depth("depth write  ", depth_write_texture, 65280))
    {
        return fail("a depth write of false stored the near depth");
    }
    if (!prepare_depth(depth_off_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1) || !draw_depth(depth_off_texture, depth_less) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_green), 1) ||
        !draw_depth(depth_off_texture, depth_off) || !report_depth("depth off    ", depth_off_texture, 65280))
    {
        return fail("a disabled depth test kept the near colour");
    }
    if (!prepare_depth(depth_plain_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1) || !draw_depth(depth_plain_texture, depth_less) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_green), 1) || !draw_depth(depth_plain_texture, 0) ||
        !report_depth("depth plain  ", depth_plain_texture, 65280))
    {
        return fail("handle 0 left the depth test enabled");
    }
    if (!prepare_depth(depth_index_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1) || !draw_depth(depth_index_texture, depth_less))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_green), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    {
        std::int32_t saved = 0;
        std::int32_t depth_view[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(depth_index_texture), saved, depth_view))
        {
            return fail("depth indexed target begin failed");
        }
        const bool drew = igpu_draw_indexed(static_cast<std::uint64_t>(depth_buffer), static_cast<std::uint64_t>(layout),
                                             static_cast<std::uint64_t>(depth_index_buffer), 4, 0, 3, 0, depth_less, 0,
                                             0);
        igpu::gl_color_target_end(saved, depth_view);
        if (!drew || !depth_defaults() || !report_depth("depth index  ", depth_index_texture, 255))
        {
            return fail("indexed draw let a farther triangle pass the less depth test");
        }
    }
    if (!prepare_depth(depth_target_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1) || !draw_depth(depth_target_texture, depth_less) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_green), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto depth_target_bytes = encode_u64(static_cast<std::uint64_t>(depth_target_texture));
    const auto depth_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(depth_buffer), static_cast<std::uint64_t>(layout), 4, 0,
                                     3, as_array(depth_target_bytes), as_array(depth_zero_bytes),
                                     as_array(depth_zero_bytes), 0, depth_less, 0, 0) ||
        !depth_defaults() || !report_depth("depth target ", depth_target_texture, 255))
    {
        return fail("render-target draw let a farther triangle pass the less depth test");
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    if (!prepare_depth(depth_held_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    {
        std::int32_t saved = 0;
        std::int32_t depth_view[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(depth_held_texture), saved, depth_view))
        {
            return fail("depth held target begin failed");
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(depth_buffer), 0, static_cast<std::uint64_t>(layout), 4,
                                     0, 3, 1, 0, depth_less, 0, 0);
        GLint func = 0;
        GLboolean mask = GL_TRUE;
        glGetIntegerv(GL_DEPTH_FUNC, &func);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
        const bool held = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE && func == GL_GREATER && mask == GL_FALSE;
        igpu::gl_color_target_end(saved, depth_view);
        if (!drew || !held || !report_depth("depth held   ", depth_held_texture, 255))
        {
            return fail("a depth draw did not write back the caller depth test, function, and mask");
        }
    }
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    // Near window depth is 0. Far window depth is 0.5. Every vertex samples the centre texel.
    const char* sample_depth_near_vs =
        "#version 120\n"
        "attribute vec2 in_pos;\n"
        "attribute vec2 in_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_Position = vec4(in_pos, -1.0, 1.0);\n"
        "  v_uv = in_uv;\n"
        "}\n";
    const char* sample_depth_far_vs =
        "#version 120\n"
        "attribute vec2 in_pos;\n"
        "attribute vec2 in_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  gl_Position = vec4(in_pos, 0.0, 1.0);\n"
        "  v_uv = in_uv;\n"
        "}\n";
    const auto sample_depth_near = igpu_shader_compile(sample_depth_near_vs, "main", 0, "glsl");
    const auto sample_depth_far = igpu_shader_compile(sample_depth_far_vs, "main", 0, "glsl");
    const SampleVertex sample_depth_vertices[3] = {
        {-1.f, -1.f, 0.5f, 0.5f},
        {3.f, -1.f, 0.5f, 0.5f},
        {-1.f, 3.f, 0.5f, 0.5f},
    };
    const auto sample_depth_buffer =
        igpu_buffer_create(static_cast<std::int64_t>(sizeof(sample_depth_vertices)), 0, 1, 16);
    const auto sample_depth_red = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sample_depth_green = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sample_depth_less_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sample_depth_write_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sample_depth_off_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sample_depth_plain_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sample_depth_held_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (sample_depth_near == 0 || sample_depth_far == 0 || sample_depth_buffer == 0 || sample_depth_red == 0 ||
        sample_depth_green == 0 || sample_depth_less_texture == 0 || sample_depth_write_texture == 0 ||
        sample_depth_off_texture == 0 || sample_depth_plain_texture == 0 || sample_depth_held_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(sample_depth_buffer), 0,
                           gm::wire::GMBuffer(const_cast<SampleVertex*>(sample_depth_vertices),
                                              sizeof(sample_depth_vertices))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto paint_solid = [&](std::int64_t texture, std::int64_t pixel_shader) {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu_shader_bind(static_cast<std::uint64_t>(vert), 0) ||
            !igpu_shader_bind(static_cast<std::uint64_t>(pixel_shader), 1) ||
            !igpu::gl_color_target_begin(static_cast<std::uint64_t>(texture), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw(static_cast<std::uint64_t>(depth_buffer), 0, static_cast<std::uint64_t>(layout), 4,
                                     0, 3, 1, 0, 0, 0, 0);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    const auto draw_sample_depth = [&](std::int64_t texture, std::int64_t source, std::int64_t depth) {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(texture), saved, viewport))
        {
            return false;
        }
        const bool drew =
            igpu_draw_sampled(static_cast<std::uint64_t>(sample_depth_buffer), static_cast<std::uint64_t>(sample_layout),
                              4, 0, 3, static_cast<std::uint64_t>(source), 0, depth, 0, 0);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    if (!paint_solid(sample_depth_red, frag) ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_depth_red), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_depth_red), 6, 4, 0, 0) != 255 ||
        !paint_solid(sample_depth_green, depth_green) ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_depth_green), 1, 4, 0, 0) != 65280 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_depth_green), 6, 4, 0, 0) != 65280)
    {
        return fail("sample depth sources were not solid red and solid green");
    }
    if (!prepare_depth(sample_depth_less_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1) ||
        !draw_sample_depth(sample_depth_less_texture, sample_depth_red, depth_less))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!depth_defaults() || !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1) ||
        !draw_sample_depth(sample_depth_less_texture, sample_depth_green, depth_less) || !depth_defaults() ||
        !report_depth("sample depth ", sample_depth_less_texture, 255))
    {
        return fail("a farther sampled triangle passed the less depth test");
    }
    if (!prepare_depth(sample_depth_write_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1) ||
        !draw_sample_depth(sample_depth_write_texture, sample_depth_red, depth_nowrite))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!depth_defaults() || !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1) ||
        !draw_sample_depth(sample_depth_write_texture, sample_depth_green, depth_less) || !depth_defaults() ||
        !report_depth("sample dwrite", sample_depth_write_texture, 65280))
    {
        return fail("a depth write of false stored the near depth on a sampled draw");
    }
    if (!prepare_depth(sample_depth_off_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1) ||
        !draw_sample_depth(sample_depth_off_texture, sample_depth_red, depth_less))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!depth_defaults() || !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1) ||
        !draw_sample_depth(sample_depth_off_texture, sample_depth_green, depth_off) || !depth_defaults() ||
        !report_depth("sample doff  ", sample_depth_off_texture, 65280))
    {
        return fail("a disabled depth test kept the near colour on a sampled draw");
    }
    if (!prepare_depth(sample_depth_plain_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1) ||
        !draw_sample_depth(sample_depth_plain_texture, sample_depth_red, depth_less))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!depth_defaults() || !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_far), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1) ||
        !draw_sample_depth(sample_depth_plain_texture, sample_depth_green, 0) || !depth_defaults() ||
        !report_depth("sample d0    ", sample_depth_plain_texture, 65280))
    {
        return fail("handle 0 left the depth test enabled on a sampled draw");
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    if (!prepare_depth(sample_depth_held_texture) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_depth_near), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    {
        std::int32_t saved = 0;
        std::int32_t depth_view[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_depth_held_texture), saved, depth_view))
        {
            return fail("sample depth held target begin failed");
        }
        const bool drew = igpu_draw_sampled(
            static_cast<std::uint64_t>(sample_depth_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 3,
            static_cast<std::uint64_t>(sample_depth_red), 0, depth_less, 0, 0);
        GLint func = 0;
        GLboolean mask = GL_TRUE;
        glGetIntegerv(GL_DEPTH_FUNC, &func);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
        const bool held = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE && func == GL_GREATER && mask == GL_FALSE;
        igpu::gl_color_target_end(saved, depth_view);
        if (!drew || !held || !report_depth("sample dheld ", sample_depth_held_texture, 255))
        {
            return fail("a sampled depth draw did not write back the caller depth test, function, and mask");
        }
    }
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(depth_plain_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("depth reject target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(depth_buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3, 1, 0,
                  999999, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("unknown depth handle was accepted");
    }
    if (igpu_draw(static_cast<std::uint64_t>(depth_buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3, 1, 0, -1, 0,
                  0) ||
        std::string(igpu_get_last_error()).find("not a state object") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("a negative depth handle was accepted");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(depth_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(depth_index_buffer), 4, 0, 3, 0, 999999, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("indexed draw accepted an unknown depth handle");
    }
    if (igpu_draw_to_render_targets(static_cast<std::uint64_t>(depth_buffer), static_cast<std::uint64_t>(layout), 4, 0,
                                    3, as_array(depth_target_bytes), as_array(depth_zero_bytes),
                                    as_array(depth_zero_bytes), 0, 999999, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("render-target draw accepted an unknown depth handle");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_depth_buffer), static_cast<std::uint64_t>(sample_layout), 4,
                          0, 3, static_cast<std::uint64_t>(sample_depth_red), 0, 999999, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("sampled draw accepted an unknown depth handle");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_depth_buffer), static_cast<std::uint64_t>(sample_layout), 4,
                          0, 3, static_cast<std::uint64_t>(sample_depth_red), 0, -1, 0, 0) ||
        std::string(igpu_get_last_error()).find("not a state object") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("sampled draw accepted a negative depth handle");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_depth_buffer), static_cast<std::uint64_t>(sample_layout), 4,
                          0, 3, static_cast<std::uint64_t>(sample_depth_red), 0, 0, 999999, 0) ||
        std::string(igpu_get_last_error()).find("unknown raster handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("sampled draw accepted an unknown raster handle");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(depth_plain_texture), 1, 4, 0, 0) != 65280)
    {
        return fail("a rejected depth draw changed pixels");
    }
    if (!igpu_state_release(static_cast<std::uint64_t>(depth_less)) ||
        igpu_state_release(static_cast<std::uint64_t>(depth_less)) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        return fail("depth release did not retire the handle");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(depth_plain_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("released depth target begin failed");
    }
    if (igpu_draw(static_cast<std::uint64_t>(depth_buffer), 0, static_cast<std::uint64_t>(layout), 4, 0, 3, 1, 0,
                  depth_less, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("a released depth handle was accepted");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_depth_buffer), static_cast<std::uint64_t>(sample_layout), 4,
                          0, 3, static_cast<std::uint64_t>(sample_depth_red), 0, depth_less, 0, 0) ||
        std::string(igpu_get_last_error()).find("unknown depth handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("sampled draw accepted a released depth handle");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(depth_plain_texture), 1, 4, 0, 0) != 65280)
    {
        return fail("a released depth handle changed pixels");
    }
    if (!igpu_supports(static_cast<std::int32_t>(igpu::Capability::DepthState)))
    {
        return fail("DepthState is false on the OpenGL backend");
    }

    // Sampled draws apply one raster handle, then write back cull and the scissor test.
    const auto sample_cull_cw = igpu_raster_state_create(1, 0, false, true);
    const auto sample_cull_ccw = igpu_raster_state_create(2, 0, false, true);
    const auto sample_scissor = igpu_raster_state_create(0, 0, true, true);
    const auto sample_solid = igpu_raster_state_create(0, 0, false, true);
    const auto sample_blend_write = igpu_blend_state_create(false, 2, 2, 1, 2, 2, 1, true, true, true, true);
    const auto sample_repeat = igpu_sampler_state_create(0, 0, 0, 1, 1, 1, 1, 0, 0, 0.f, 0.f, -1.f);
    if (sample_cull_cw == 0 || sample_cull_ccw == 0 || sample_scissor == 0 || sample_solid == 0 ||
        sample_blend_write == 0 || sample_repeat == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const SampleVertex sample_ccw[] = {
        {-1.f, -1.f, 0.5f, 0.5f},
        {0.f, -1.f, 0.5f, 0.5f},
        {-1.f, 1.f, 0.5f, 0.5f},
    };
    const SampleVertex sample_cw[] = {
        {0.f, -1.f, 0.5f, 0.5f},
        {-1.f, -1.f, 0.5f, 0.5f},
        {-1.f, 1.f, 0.5f, 0.5f},
    };
    const SampleVertex sample_full[] = {
        {-1.f, -1.f, 0.5f, 0.5f},
        {3.f, -1.f, 0.5f, 0.5f},
        {-1.f, 3.f, 0.5f, 0.5f},
    };
    const SampleVertex sample_repeat_uv[] = {
        {-1.f, -1.f, 1.0625f, 0.5f},
        {3.f, -1.f, 1.0625f, 0.5f},
        {-1.f, 3.f, 1.0625f, 0.5f},
    };
    const auto sample_ccw_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(sample_ccw)), 0, 1, 16);
    const auto sample_cw_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(sample_cw)), 0, 1, 16);
    const auto sample_full_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(sample_full)), 0, 1, 16);
    const auto sample_repeat_buffer = igpu_buffer_create(static_cast<std::int64_t>(sizeof(sample_repeat_uv)), 0, 1, 16);
    const auto sr_keep = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sr_drop = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sr_front = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sr_back = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sr_clip = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sr_held = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sr_wide = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sr_restore = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto sr_rep = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (sample_ccw_buffer == 0 || sample_cw_buffer == 0 || sample_full_buffer == 0 || sample_repeat_buffer == 0 ||
        sr_keep == 0 || sr_drop == 0 || sr_front == 0 || sr_back == 0 || sr_clip == 0 || sr_held == 0 ||
        sr_wide == 0 || sr_restore == 0 || sr_rep == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(sample_ccw_buffer), 0,
                           gm::wire::GMBuffer(const_cast<SampleVertex*>(sample_ccw), sizeof(sample_ccw))) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(sample_cw_buffer), 0,
                           gm::wire::GMBuffer(const_cast<SampleVertex*>(sample_cw), sizeof(sample_cw))) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(sample_full_buffer), 0,
                           gm::wire::GMBuffer(const_cast<SampleVertex*>(sample_full), sizeof(sample_full))) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(sample_repeat_buffer), 0,
                           gm::wire::GMBuffer(const_cast<SampleVertex*>(sample_repeat_uv), sizeof(sample_repeat_uv))))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto report_sr = [&](const char* label, std::int64_t texture, std::int64_t left, std::int64_t right) {
        const auto got_left = igpu_texture_read(static_cast<std::uint64_t>(texture), 1, 4, 0, 0);
        const auto got_right = igpu_texture_read(static_cast<std::uint64_t>(texture), 6, 4, 0, 0);
        std::printf("%s: %lld / %lld\n", label, static_cast<long long>(got_left), static_cast<long long>(got_right));
        return got_left == left && got_right == right;
    };
    const auto draw_sr = [&](std::int64_t texture, std::int64_t buffer, std::uint64_t source, std::int64_t blend,
                             std::int64_t depth, std::int64_t raster, std::int64_t sampler) {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(texture), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw_sampled(static_cast<std::uint64_t>(buffer), static_cast<std::uint64_t>(sample_layout),
                                             4, 0, 3, source, blend, depth, raster, sampler);
        const bool cull_off = glIsEnabled(GL_CULL_FACE) == GL_FALSE;
        igpu::gl_color_target_end(saved, viewport);
        return drew && cull_off;
    };
    if (!draw_sr(sr_keep, sample_ccw_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, sample_cull_cw, 0) ||
        !report_sr("sample ckeep ", sr_keep, 255, 0))
    {
        return fail("sampled cull_clockwise dropped a counter-clockwise triangle");
    }
    if (!draw_sr(sr_drop, sample_ccw_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, sample_cull_ccw, 0) ||
        !report_sr("sample cdrop ", sr_drop, 0, 0))
    {
        return fail("sampled cull_counterclockwise kept a counter-clockwise triangle");
    }
    if (!draw_sr(sr_drop, sample_ccw_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, 0, 0) ||
        !report_sr("sample c0    ", sr_drop, 255, 0))
    {
        return fail("handle 0 did not restore culling after a sampled drop");
    }
    if (!draw_sr(sr_front, sample_cw_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, sample_cull_cw, 0) ||
        !report_sr("sample cfront", sr_front, 0, 0))
    {
        return fail("sampled cull_clockwise kept a clockwise triangle");
    }
    if (!draw_sr(sr_back, sample_cw_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, sample_cull_ccw, 0) ||
        !report_sr("sample cback ", sr_back, 255, 0))
    {
        return fail("sampled cull_counterclockwise dropped a clockwise triangle");
    }
    glScissor(0, 0, 4, 8);
    if (!draw_sr(sr_clip, sample_full_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, sample_scissor, 0) ||
        glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE || !scissor_box_kept() ||
        !report_sr("sample clip  ", sr_clip, 255, 0))
    {
        return fail("sampled scissor drew outside the caller rectangle");
    }
    if (!draw_sr(sr_clip, sample_full_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, 0, 0) ||
        glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE || !scissor_box_kept() ||
        !report_sr("sample s0    ", sr_clip, 255, 255))
    {
        return fail("handle 0 left the scissor test enabled after a sampled draw");
    }
    glEnable(GL_SCISSOR_TEST);
    if (!draw_sr(sr_held, sample_full_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, 0, 0) ||
        glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE || !scissor_box_kept() ||
        !report_sr("sample sheld ", sr_held, 255, 0))
    {
        return fail("handle 0 changed the caller scissor test on a sampled draw");
    }
    if (!draw_sr(sr_wide, sample_full_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, sample_solid, 0) ||
        glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE || !scissor_box_kept() ||
        !report_sr("sample swide ", sr_wide, 255, 255))
    {
        return fail("a sampled raster state left the caller scissor test enabled during the draw");
    }
    if (!draw_sr(sr_restore, sample_full_buffer, static_cast<std::uint64_t>(sample_depth_red), 0, 0, 0, 0) ||
        glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE || !scissor_box_kept() ||
        !report_sr("sample sback ", sr_restore, 255, 0))
    {
        return fail("handle 0 did not see the restored scissor test on a sampled draw");
    }
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (!igpu::gl_attach_depth(static_cast<std::uint64_t>(sr_rep)) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_vert), 0) ||
        !igpu_shader_bind(static_cast<std::uint64_t>(sample_frag), 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    {
        std::int32_t saved = 0;
        std::int32_t sr_view[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sr_rep), saved, sr_view))
        {
            return fail("sample raster combined target begin failed");
        }
        const bool drew = igpu_draw_sampled(
            static_cast<std::uint64_t>(sample_repeat_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 3,
            static_cast<std::uint64_t>(sample_source), sample_blend_write, depth_off, sample_cull_cw, sample_repeat);
        GLint func = 0;
        GLboolean depth_mask = GL_TRUE;
        GLboolean color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
        glGetIntegerv(GL_DEPTH_FUNC, &func);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
        glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
        const bool held = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE && func == GL_GREATER && depth_mask == GL_FALSE &&
                          glIsEnabled(GL_CULL_FACE) == GL_FALSE && glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE &&
                          glIsEnabled(GL_BLEND) == GL_FALSE && color_mask[0] == GL_FALSE;
        igpu::gl_color_target_end(saved, sr_view);
        if (!drew || !held || !report_sr("sample crep  ", sr_rep, 255, 255))
        {
            return fail("a sampled draw did not apply depth, blend, cull, and repeat together");
        }
    }
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (!draw_sr(sr_rep, sample_repeat_buffer, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 0) ||
        !report_sr("sample cr0   ", sr_rep, 16711680, 16711680))
    {
        return fail("handle 0 left the repeat wrap in place after a sampled raster draw");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sr_keep), strip_saved_fbo, strip_held_viewport))
    {
        return fail("sample raster reject target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_ccw_buffer), static_cast<std::uint64_t>(sample_layout), 4,
                          0, 3, static_cast<std::uint64_t>(sample_depth_red), 0, 0, 999999, 0) ||
        std::string(igpu_get_last_error()).find("unknown raster handle") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("sampled draw accepted an unknown raster handle");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_ccw_buffer), static_cast<std::uint64_t>(sample_layout), 4,
                          0, 3, static_cast<std::uint64_t>(sample_depth_red), 0, 0, -1, 0) ||
        std::string(igpu_get_last_error()).find("not a state object") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("sampled draw accepted a negative raster handle");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_full_buffer), static_cast<std::uint64_t>(sample_layout), 4,
                          0, 3, static_cast<std::uint64_t>(sr_keep), 0, 0, sample_cull_cw, 0) ||
        std::string(igpu_get_last_error()).find("the texture is the current color target") == std::string::npos)
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail("sampled raster draw accepted the current color target");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sr_keep), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sr_keep), 6, 4, 0, 0) != 0)
    {
        return fail("a rejected sampled raster draw changed pixels");
    }
    if (!igpu_state_release(static_cast<std::uint64_t>(sample_solid)) ||
        igpu_draw_sampled(static_cast<std::uint64_t>(sample_ccw_buffer), static_cast<std::uint64_t>(sample_layout), 4,
                          0, 3, static_cast<std::uint64_t>(sample_depth_red), 0, 0, sample_solid, 0) ||
        std::string(igpu_get_last_error()).find("unknown raster handle") == std::string::npos)
    {
        return fail("sampled draw accepted a released raster handle");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(sr_keep), 1, 4, 0, 0) != 255)
    {
        return fail("a released sampled raster handle changed pixels");
    }

    if (!run_gl_texture_tests())
    {
        return fail("floating texture and comparison suite failed" );
    }

    if (!run_gl_buffer_tests())
    {
        return fail("buffer lifecycle suite failed");
    }

    if (!run_gl_compute_tests())
    {
        return fail("compute and storage suite failed");
    }

    if (!run_gl_uniform_tests())
    {
        return fail("uniform reflection and buffer suite failed");
    }

    if (!run_gl_target_tests())
    {
        return fail("multiple render-target suite failed");
    }

    if (!run_gl_layered_texture_tests())
    {
        return fail("layered texture suite failed");
    }

    if (!run_gl_instanced_tests())
    {
        return fail("instanced and indirect draw suite failed");
    }

    if (!run_gl_query_tests())
    {
        return fail("query, timestamp and fence suite failed");
    }

    if (!run_gl_query_shutdown_tests())
    {
        return fail("query and fence shutdown suite failed");
    }

    if (!run_gl_compute_shutdown_tests())
    {
        return fail("compute and storage shutdown suite failed");
    }

    if (!run_gl_uniform_shutdown_tests())
    {
        return fail("uniform buffer shutdown suite failed");
    }

    if (!run_gl_layered_texture_shutdown_tests())
    {
        return fail("layered texture shutdown suite failed");
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
