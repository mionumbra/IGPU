#include "igpu_gl.h"

#include "igpu_capabilities.h"
#include "igpu_device.h"
#include "igpu_error.h"
#include "igpu_gl_draw.h"
#include "igpu_gl_loader.h"

#include <Windows.h>
#include <GL/gl.h>

#include <string>
#include <unordered_map>
#include <vector>

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#endif

namespace igpu
{
    namespace
    {
        class GlBackend final : public Backend
        {
        public:
            const char* name() const override { return "opengl"; }
            bool occlusion() const override { return false; }
            bool timestamps() const override { return false; }
            bool fences() const override { return false; }
            std::int64_t timestamp_frequency() const override { return 0; }

            bool texture_format(std::int32_t format) const override { return format == 6; }

            std::uint64_t query_create(std::int32_t) override { not_yet("igpu_query_create"); return 0; }
            bool query_begin(std::uint64_t) override { return not_yet("igpu_query_begin"); }
            bool query_end(std::uint64_t) override { return not_yet("igpu_query_end"); }
            bool query_ready(std::uint64_t) override { return not_yet("igpu_query_ready"); }
            std::int64_t query_result(std::uint64_t) override { not_yet("igpu_query_result"); return 0; }
            bool query_release(std::uint64_t) override { return not_yet("igpu_query_release"); }

            std::uint64_t fence_create() override { not_yet("igpu_fence_create"); return 0; }
            bool fence_signal(std::uint64_t) override { return not_yet("igpu_fence_signal"); }
            bool fence_signaled(std::uint64_t) override { return not_yet("igpu_fence_signaled"); }
            bool fence_release(std::uint64_t) override { return not_yet("igpu_fence_release"); }

            bool draw(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                      std::int64_t first_vertex, std::int64_t vertex_count,
                      std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                      std::int64_t sampler_state) override
            {
                return gl_draw(vertex_buffer, layout, primitive, first_vertex, vertex_count,
                               blend_state, depth_state, raster_state, sampler_state);
            }
            bool draw_instanced(std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::int64_t, std::int64_t,
                                std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t) override
            {
                return not_yet("igpu_draw_instanced");
            }
            bool draw_indirect(std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::uint64_t, std::int64_t,
                               std::int64_t, std::int64_t, std::int64_t, std::int64_t) override
            {
                return not_yet("igpu_draw_indirect");
            }
            bool draw_patch(std::uint64_t, std::uint64_t, std::int32_t, std::int64_t, std::int64_t, std::int64_t,
                            std::int64_t, std::int64_t, std::int64_t) override
            {
                return not_yet("igpu_draw_patch");
            }
            bool draw_indexed_indirect(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t,
                                       std::uint64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t,
                                       std::int64_t) override
            {
                return not_yet("igpu_draw_indexed_indirect");
            }
            bool draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer,
                              std::int32_t primitive, std::int64_t first_index, std::int64_t index_count,
                              std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                              std::int64_t sampler_state) override
            {
                return gl_draw_indexed(vertex_buffer, layout, index_buffer, primitive, first_index, index_count,
                                       blend_state, depth_state, raster_state, sampler_state);
            }
            std::int32_t draw_count() override { not_yet("igpu_get_draw_count"); return 0; }
            std::int32_t draw_restore_failures() override { not_yet("igpu_get_draw_restore_failures"); return 0; }
            bool is_vertex_buffer_bound(std::uint64_t) override { return not_yet("igpu_is_vertex_buffer_bound"); }

            std::int64_t blend_state_create(bool, std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                            std::int32_t, bool, bool, bool, bool) override
            {
                not_yet("igpu_blend_state_create");
                return 0;
            }
            std::int64_t depth_state_create(bool, bool, std::int32_t, bool, std::int32_t, std::int32_t, std::int32_t,
                                            std::int32_t, std::int32_t, std::int32_t, std::int32_t) override
            {
                not_yet("igpu_depth_state_create");
                return 0;
            }
            std::int64_t raster_state_create(std::int32_t, std::int32_t, bool, bool) override
            {
                not_yet("igpu_raster_state_create");
                return 0;
            }
            std::int64_t sampler_state_create(std::int32_t, bool, std::int32_t) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_address(std::int32_t, std::int32_t, std::int32_t) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_border(std::int32_t, std::int32_t, float, float, float, float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_axes(std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_axes_border(std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                          std::int32_t, float, float, float, float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_axes_range(std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                         std::int32_t, float, float, float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_axes_border_range(std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                                std::int32_t, float, float, float, float, float, float,
                                                                float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_filters(std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                      std::int32_t, std::int32_t) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_filters_border(std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                             std::int32_t, std::int32_t, float, float, float, float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_filters_offset(std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                             std::int32_t, std::int32_t, float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_filters_range(std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                            std::int32_t, std::int32_t, float, float, float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_filters_border_range(std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                                   std::int32_t, std::int32_t, float, float, float, float,
                                                                   float, float, float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            std::int64_t sampler_state_create_compare(std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                                      std::int32_t, std::int32_t, std::int32_t, float, float, float) override
            {
                not_yet("igpu_sampler_state_create");
                return 0;
            }
            bool state_release(std::uint64_t) override { return not_yet("igpu_state_release"); }

            std::int64_t texture_create(std::int32_t, std::int32_t, std::int32_t, bool) override
            {
                not_yet("igpu_texture_create");
                return 0;
            }
            std::int64_t texture_create_kind(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                             std::int32_t format, bool render_target, bool storage) override
            {
                return gl_texture_create_kind(kind, width, height, depth, format, render_target, storage);
            }
            std::int64_t texture_create_mips(std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t, bool, std::int32_t) override
            {
                not_yet("igpu_texture_create");
                return 0;
            }
            bool texture_generate_mips(std::uint64_t) override { return not_yet("igpu_texture_generate_mips"); }
            bool texture_release(std::uint64_t texture) override { return gl_texture_release(texture); }
            std::int64_t texture_get_pixel(std::uint64_t, std::int32_t, std::int32_t) override
            {
                not_yet("igpu_texture_read");
                return 0;
            }
            std::int64_t texture_read(std::uint64_t, std::int32_t, std::int32_t, std::int32_t) override
            {
                not_yet("igpu_texture_read");
                return 0;
            }
            std::int64_t texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer,
                                            std::int32_t mip) override
            {
                return gl_texture_read_level(texture, x, y, layer, mip);
            }
            bool draw_to_texture(std::uint64_t, std::uint64_t, std::int32_t, std::int64_t, std::int64_t, std::uint64_t) override
            {
                return not_yet("igpu_draw_to_render_targets");
            }
            bool draw_to_texture_layer(std::uint64_t, std::uint64_t, std::int32_t, std::int64_t, std::int64_t, std::uint64_t, std::int32_t) override
            {
                return not_yet("igpu_draw_to_render_targets");
            }
            bool draw_to_texture_level(std::uint64_t, std::uint64_t, std::int32_t, std::int64_t, std::int64_t, std::uint64_t, std::int32_t, std::int32_t) override
            {
                return not_yet("igpu_draw_to_render_targets");
            }
            bool dispatch(std::int32_t, std::int32_t, std::int32_t, std::uint64_t) override { return not_yet("igpu_dispatch"); }
            bool dispatch_level(std::int32_t, std::int32_t, std::int32_t, std::uint64_t, std::int32_t) override { return not_yet("igpu_dispatch"); }
            bool dispatch_buffer(std::int32_t, std::int32_t, std::int32_t, std::uint64_t) override { return not_yet("igpu_dispatch"); }
            bool dispatch_both(std::int32_t, std::int32_t, std::int32_t, std::uint64_t, std::uint64_t) override { return not_yet("igpu_dispatch"); }
            bool dispatch_writes(std::int32_t, std::int32_t, std::int32_t, const gm::wire::GMArrayView&, const gm::wire::GMArrayView&) override
            {
                return not_yet("igpu_dispatch");
            }
            bool draw_to_render_targets(std::uint64_t, std::uint64_t, std::int32_t, std::int64_t, std::int64_t, const gm::wire::GMArrayView&) override
            {
                return not_yet("igpu_draw_to_render_targets");
            }
            bool draw_to_render_targets_level(std::uint64_t, std::uint64_t, std::int32_t, std::int64_t, std::int64_t,
                                              const gm::wire::GMArrayView&, const gm::wire::GMArrayView&) override
            {
                return not_yet("igpu_draw_to_render_targets");
            }
            bool draw_to_render_targets_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                              std::int64_t first_vertex, std::int64_t vertex_count,
                                              const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers,
                                              const gm::wire::GMArrayView& mips, std::int64_t blend_state,
                                              std::int64_t depth_state, std::int64_t raster_state,
                                              std::int64_t sampler_state) override
            {
                return gl_draw_to_render_targets_layer(
                    vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, layers, mips, blend_state,
                    depth_state, raster_state, sampler_state);
            }
            bool draw_sampled(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                              std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                              std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                              std::int64_t sampler_state) override
            {
                return gl_draw_sampled(vertex_buffer, layout, primitive, first_vertex, vertex_count, texture,
                                       blend_state, depth_state, raster_state, sampler_state);
            }

            bool reflect_uniforms(const void*, std::size_t, UniformLayout& out) override
            {
                out = {};
                return not_yet("igpu_shader_reflect");
            }
            bool uniform_bind(std::uint64_t, std::int32_t, std::int32_t) override { return not_yet("igpu_uniform_bind"); }

            ~GlBackend() override
            {
                const auto& fns = igpu_gl_fns();
                if (program != 0 && fns.DeleteProgram != nullptr)
                {
                    fns.DeleteProgram(program);
                }
                if (fns.DeleteShader != nullptr)
                {
                    for (const auto& entry : shaders)
                    {
                        if (entry.second.shader != 0)
                        {
                            fns.DeleteShader(entry.second.shader);
                        }
                    }
                }
                gl_resources_release();
            }

            std::int64_t shader_compile(std::string_view source, std::string_view entry, std::int32_t stage, std::string_view dialect) override
            {
                if (source.empty() || entry.empty())
                {
                    set_last_error("igpu_shader_compile: source and entry must not be empty");
                    return 0;
                }
                if (stage < 0 || stage > 7)
                {
                    set_last_error("igpu_shader_compile: unknown shader stage");
                    return 0;
                }
                if (stage != 0 && stage != 1)
                {
                    std::string message = "igpu_shader_compile: stage '";
                    message += stage_name_of(stage);
                    message += "' is not supported by the 'opengl' backend (check igpu_supports)";
                    set_last_error(std::move(message));
                    return 0;
                }
                if (entry != "main")
                {
                    set_last_error("igpu_shader_compile: the opengl backend only accepts the entry point 'main'");
                    return 0;
                }
                if (dialect == "hlsl" || (dialect != "" && dialect != "glsl" && dialect != "glsl_es"))
                {
                    std::string message = "igpu_shader_compile: dialect '";
                    message += std::string(dialect);
                    message += "' is not supported by the 'opengl' backend (it compiles '";
                    message += shader_dialect();
                    message += "')";
                    set_last_error(std::move(message));
                    return 0;
                }
                if (dialect == "glsl_es" && std::string_view(shader_dialect()) != "glsl_es")
                {
                    set_last_error("igpu_shader_compile: dialect 'glsl_es' does not match this OpenGL context");
                    return 0;
                }

                const auto& fns = igpu_gl_fns();
                const GLenum type = stage == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
                const GLuint shader = fns.CreateShader(type);
                const std::string text(source);
                const char* text_ptr = text.c_str();
                fns.ShaderSource(shader, 1, &text_ptr, nullptr);
                fns.CompileShader(shader);
                GLint ok = 0;
                fns.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
                if (ok == 0)
                {
                    std::string message = "igpu_shader_compile: ";
                    message += info_log(shader, true);
                    fns.DeleteShader(shader);
                    set_last_error(std::move(message));
                    return 0;
                }

                const std::uint64_t id = next_shader++;
                shaders.emplace(id, ShaderObject{shader, stage});
                return static_cast<std::int64_t>(id);
            }

            bool shader_release(std::uint64_t shader) override
            {
                const auto it = shaders.find(shader);
                if (it == shaders.end())
                {
                    set_last_error("igpu_shader_release: unknown shader handle");
                    return false;
                }
                for (auto bound_it = bound.begin(); bound_it != bound.end();)
                {
                    if (bound_it->second == shader)
                    {
                        bound_it = bound.erase(bound_it);
                    }
                    else
                    {
                        ++bound_it;
                    }
                }
                igpu_gl_fns().DeleteShader(it->second.shader);
                shaders.erase(it);
                relink();
                return true;
            }

            bool shader_bind(std::int64_t shader, std::int32_t stage) override
            {
                if (stage != 0 && stage != 1)
                {
                    set_last_error("igpu_shader_bind: unknown shader stage");
                    return false;
                }
                if (shader == 0)
                {
                    bound.erase(stage);
                    return relink();
                }
                const auto it = shaders.find(static_cast<std::uint64_t>(shader));
                if (it == shaders.end())
                {
                    set_last_error("igpu_shader_bind: unknown shader handle");
                    return false;
                }
                if (it->second.stage != stage)
                {
                    set_last_error(std::string("igpu_shader_bind: the handle was compiled for stage '") +
                                   stage_name_of(it->second.stage) + "' but was bound to '" + stage_name_of(stage) + "'");
                    return false;
                }
                bound[stage] = static_cast<std::uint64_t>(shader);
                return relink();
            }

            std::int64_t get_bound_shader(std::int32_t stage) override
            {
                const auto it = bound.find(stage);
                return it == bound.end() ? 0 : static_cast<std::int64_t>(it->second);
            }

            std::int64_t buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride) override
            {
                return gl_buffer_create(size, usage, bind, stride);
            }
            bool buffer_write(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& data) override
            {
                return gl_buffer_write(buffer, offset, data);
            }
            bool buffer_resize(std::uint64_t, std::int64_t) override { return not_yet("igpu_buffer_resize"); }
            bool buffer_read(std::uint64_t, std::int64_t, const gm::wire::GMBuffer&) override { return not_yet("igpu_buffer_read"); }
            std::int64_t buffer_size(std::uint64_t) override { not_yet("igpu_buffer_size"); return 0; }
            bool buffer_release(std::uint64_t buffer) override { return gl_buffer_release(buffer); }
            bool storage_bind(std::uint64_t, std::int32_t, std::int32_t) override { return not_yet("igpu_storage_bind"); }
            bool buffer_patch(std::uint64_t, std::int64_t, const void*, std::size_t, const char*) override
            {
                return not_yet("igpu_buffer_write");
            }

            std::int64_t input_layout_create(std::int64_t, const gm::wire::GMArrayView&, const gm::wire::GMArrayView&,
                                             std::int32_t, std::int32_t) override
            {
                not_yet("igpu_input_layout_create");
                return 0;
            }
            std::int64_t input_layout_create_step(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                                  const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step,
                                                  std::int32_t element_count, std::int32_t vertex_stride,
                                                  std::int32_t instance_stride) override
            {
                return gl_input_layout_create(shader, usage, type, step, element_count, vertex_stride, instance_stride);
            }
            bool input_layout_release(std::uint64_t layout) override { return gl_input_layout_release(layout); }

        private:
            struct ShaderObject
            {
                GLuint shader = 0;
                std::int32_t stage = -1;
            };

            std::unordered_map<std::uint64_t, ShaderObject> shaders;
            std::unordered_map<std::int32_t, std::uint64_t> bound;
            std::uint64_t next_shader = 1;
            GLuint program = 0;

            static const char* stage_name_of(std::int32_t stage)
            {
                switch (stage)
                {
                case 0: return "vertex";
                case 1: return "pixel";
                case 2: return "compute";
                case 3: return "geometry";
                case 4: return "hull";
                case 5: return "domain";
                case 6: return "mesh";
                case 7: return "amplification";
                default: return "unknown";
                }
            }

            static std::string info_log(GLuint object, bool shader)
            {
                const auto& fns = igpu_gl_fns();
                GLint length = 0;
                if (shader)
                {
                    fns.GetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
                }
                else
                {
                    fns.GetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
                }
                if (length <= 1)
                {
                    return "compile failed";
                }
                std::vector<char> text(static_cast<std::size_t>(length));
                if (shader)
                {
                    fns.GetShaderInfoLog(object, length, nullptr, text.data());
                }
                else
                {
                    fns.GetProgramInfoLog(object, length, nullptr, text.data());
                }
                return std::string(text.data());
            }

            bool relink()
            {
                const auto& fns = igpu_gl_fns();
                if (program != 0)
                {
                    fns.DeleteProgram(program);
                    program = 0;
                }
                const auto vertex = bound.find(0);
                const auto pixel = bound.find(1);
                if (vertex == bound.end() || pixel == bound.end())
                {
                    fns.UseProgram(0);
                    return true;
                }
                program = fns.CreateProgram();
                fns.AttachShader(program, shaders[vertex->second].shader);
                fns.AttachShader(program, shaders[pixel->second].shader);
                fns.BindAttribLocation(program, 0, "in_pos");
                fns.BindAttribLocation(program, 1, "in_colour");
                fns.BindAttribLocation(program, 1, "in_uv");
                fns.LinkProgram(program);
                GLint ok = 0;
                fns.GetProgramiv(program, GL_LINK_STATUS, &ok);
                if (ok == 0)
                {
                    std::string message = "igpu_shader_bind: ";
                    message += info_log(program, false);
                    fns.DeleteProgram(program);
                    program = 0;
                    set_last_error(std::move(message));
                    return false;
                }
                fns.UseProgram(program);
                return true;
            }

            bool not_yet(const char* entry) const
            {
                set_last_error(std::string(entry) + ": the opengl backend does not implement this call yet");
                return false;
            }
        };
    }

    bool bind_current_context()
    {
        clear_last_error();
        auto& s = state();
        if (s.initialised || active_backend() != nullptr)
        {
            set_last_error("igpu_bind_current: a graphics device is already bound; call igpu_shutdown() first");
            return false;
        }
        if (wglGetCurrentContext() == nullptr)
        {
            set_last_error("igpu_bind_current: no OpenGL context is current on this thread");
            return false;
        }
        if (!gl_load())
        {
            return false;
        }
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        if (version == nullptr || renderer == nullptr)
        {
            set_last_error("igpu_bind_current: the current context did not report a version");
            return false;
        }
        s.renderer_name = renderer;
        s.gl_dialect = (std::string(version).find("OpenGL ES") != std::string::npos) ? "glsl_es" : "glsl";
        set_active_backend(std::make_unique<GlBackend>());
        s.initialised = true;
        clear_last_error();
        return true;
    }

    std::unique_ptr<Backend> make_gl_backend()
    {
        return std::make_unique<GlBackend>();
    }
}
