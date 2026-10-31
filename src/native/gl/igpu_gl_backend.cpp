#include "igpu_gl.h"

#include "igpu_capabilities.h"
#include "igpu_device.h"
#include "igpu_error.h"
#include "igpu_gl_draw.h"
#include "igpu_gl_compute.h"
#include "igpu_gl_uniform.h"
#include "igpu_gl_targets.h"
#include "igpu_gl_texture.h"
#include "igpu_gl_loader.h"
#include "igpu_gl_query.h"

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
            GlBackend() { gl_queries_initialize(); gl_draw_capabilities_initialize(); gl_compute_initialize(); gl_uniform_initialize(); gl_targets_initialize(); gl_textures_initialize(); }
            const char* name() const override { return "opengl"; }
            bool occlusion() const override { return gl_occlusion_supported(); }
            bool timestamps() const override { return gl_timestamps_supported(); }
            bool fences() const override { return gl_fences_supported(); }
            bool instancing_supported() const override { return gl_instancing_supported(); }
            bool indirect_draw_supported() const override { return gl_indirect_draw_supported(); }
            bool texture_kind_supported(std::int32_t kind) const override { return gl_texture_kind_supported(kind); }
            bool multiple_render_targets_supported() const override { return gl_multiple_targets_supported(); }
            bool uniform_buffer_supported() const override { return gl_uniform_supported(); }
            bool uniform_reflection_supported() const override { return gl_uniform_supported(); }
            const UniformLayout* shader_uniforms(std::uint64_t shader) override
            {
                const auto it = shaders.find(shader);
                if (it == shaders.end()) return nullptr;
                if (!it->second.uniform_ready)
                {
                    if (!gl_uniform_reflect(it->second.shader, it->second.stage, it->second.uniforms)) return nullptr;
                    it->second.uniform_ready = true;
                }
                return &it->second.uniforms;
            }
            std::int64_t uniform_buffer_bytes(std::uint64_t buffer, const char* entry) override
            {
                GlUniformBuffer resolved;
                return gl_uniform_buffer(buffer, resolved, entry) ? resolved.bytes : 0;
            }
            bool compute_supported() const override { return gl_compute_supported(); }
            bool structured_buffer_supported() const override { return gl_read_storage_supported(); }
            bool buffer_resize_supported() const override { return igpu_gl_fns().CopyBufferSubData != nullptr; }
            std::int64_t timestamp_frequency() const override { return gl_timestamp_frequency(); }

            bool texture_format(std::int32_t format) const override { return gl_texture_format_supported(format); }

            std::uint64_t query_create(std::int32_t kind) override { return gl_query_create(kind); }
            bool query_begin(std::uint64_t id) override { return gl_query_begin(id); }
            bool query_end(std::uint64_t id) override { return gl_query_end(id); }
            bool query_ready(std::uint64_t id) override { return gl_query_ready(id); }
            std::int64_t query_result(std::uint64_t id) override { return gl_query_result(id); }
            bool query_release(std::uint64_t id) override { return gl_query_release(id); }

            std::uint64_t fence_create() override { return gl_fence_create(); }
            bool fence_signal(std::uint64_t id) override { return gl_fence_signal(id); }
            bool fence_signaled(std::uint64_t id) override { return gl_fence_signaled(id); }
            bool fence_release(std::uint64_t id) override { return gl_fence_release(id); }

            bool draw(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                      std::int64_t first_vertex, std::int64_t vertex_count,
                      std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                      std::int64_t sampler_state) override
            {
                return gl_draw(vertex_buffer, layout, primitive, first_vertex, vertex_count,
                               blend_state, depth_state, raster_state, sampler_state);
            }
            bool draw_instanced(std::uint64_t vertex, std::uint64_t instances, std::uint64_t layout,
                                std::int32_t primitive, std::int64_t first, std::int64_t count,
                                std::int64_t instance_count, std::int64_t blend, std::int64_t depth,
                                std::int64_t raster, std::int64_t sampler) override
            {
                return gl_draw_instanced(vertex, instances, layout, primitive, first, count, instance_count,
                                         blend, depth, raster, sampler);
            }
            bool draw_indirect(std::uint64_t vertex, std::uint64_t instances, std::uint64_t layout,
                               std::int32_t primitive, std::uint64_t args, std::int64_t offset,
                               std::int64_t blend, std::int64_t depth, std::int64_t raster, std::int64_t sampler) override
            {
                return gl_draw_indirect_record(vertex, instances, layout, 0, false, primitive, args, offset,
                                               blend, depth, raster, sampler);
            }
            bool draw_patch(std::uint64_t, std::uint64_t, std::int32_t, std::int64_t, std::int64_t, std::int64_t,
                            std::int64_t, std::int64_t, std::int64_t) override
            {
                return not_yet("igpu_draw_patch");
            }
            bool draw_indexed_indirect(std::uint64_t vertex, std::uint64_t instances, std::uint64_t layout,
                                       std::uint64_t index, std::int32_t primitive, std::uint64_t args,
                                       std::int64_t offset, std::int64_t blend, std::int64_t depth,
                                       std::int64_t raster, std::int64_t sampler) override
            {
                return gl_draw_indirect_record(vertex, instances, layout, index, true, primitive, args, offset,
                                               blend, depth, raster, sampler);
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

            std::int64_t blend_state_create(bool enabled, std::int32_t src, std::int32_t dest, std::int32_t equation,
                                            std::int32_t src_alpha, std::int32_t dest_alpha, std::int32_t equation_alpha,
                                            bool write_red, bool write_green, bool write_blue, bool write_alpha) override
            {
                return gl_blend_state_create(enabled, src, dest, equation, src_alpha, dest_alpha, equation_alpha,
                                             write_red, write_green, write_blue, write_alpha);
            }
            std::int64_t depth_state_create(bool depth_test, bool depth_write, std::int32_t depth_func,
                                            bool stencil_enable, std::int32_t stencil_func, std::int32_t stencil_fail,
                                            std::int32_t stencil_depth_fail, std::int32_t stencil_pass,
                                            std::int32_t stencil_ref, std::int32_t stencil_read_mask,
                                            std::int32_t stencil_write_mask) override
            {
                return gl_depth_state_create(depth_test, depth_write, depth_func, stencil_enable, stencil_func,
                                             stencil_fail, stencil_depth_fail, stencil_pass, stencil_ref,
                                             stencil_read_mask, stencil_write_mask);
            }
            std::int64_t raster_state_create(std::int32_t cull, std::int32_t fill, bool scissor, bool depth_clip) override
            {
                return gl_raster_state_create(cull, fill, scissor, depth_clip);
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
            std::int64_t sampler_state_create_axes_range(std::int32_t filter, std::int32_t address_u,
                                                         std::int32_t address_v, std::int32_t address_w,
                                                         std::int32_t anisotropy, float level_offset, float finest,
                                                         float coarsest) override
            {
                if (filter != 2)
                {
                    not_yet("igpu_sampler_state_create");
                    return 0;
                }
                return gl_sampler_state_create_filters_range(
                    1, 1, 1, address_u, address_v, address_w, level_offset, finest, coarsest, anisotropy);
            }
            std::int64_t sampler_state_create_axes_border_range(std::int32_t filter, std::int32_t address_u,
                                                                std::int32_t address_v, std::int32_t address_w,
                                                                std::int32_t anisotropy, float red, float green,
                                                                float blue, float alpha, float level_offset,
                                                                float finest, float coarsest) override
            {
                if (filter != 2)
                {
                    not_yet("igpu_sampler_state_create");
                    return 0;
                }
                return gl_sampler_state_create_filters_border_range(
                    1, 1, 1, address_u, address_v, address_w, red, green, blue, alpha,
                    level_offset, finest, coarsest, anisotropy);
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
            std::int64_t sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                            std::int32_t mip, std::int32_t address_u,
                                                            std::int32_t address_v, std::int32_t address_w,
                                                            float level_offset, float finest, float coarsest) override
            {
                return gl_sampler_state_create_filters_range(
                    magnification, minification, mip, address_u, address_v, address_w, level_offset, finest, coarsest);
            }
            std::int64_t sampler_state_create_filters_border_range(std::int32_t magnification, std::int32_t minification,
                                                                   std::int32_t mip, std::int32_t address_u,
                                                                   std::int32_t address_v, std::int32_t address_w,
                                                                   float red, float green, float blue, float alpha,
                                                                   float level_offset, float finest,
                                                                   float coarsest) override
            {
                return gl_sampler_state_create_filters_border_range(
                    magnification, minification, mip, address_u, address_v, address_w, red, green, blue, alpha,
                    level_offset, finest, coarsest);
            }
            std::int64_t sampler_state_create_compare(std::int32_t compare, std::int32_t magnification,
                                                      std::int32_t minification, std::int32_t mip,
                                                      std::int32_t address_u, std::int32_t address_v,
                                                      std::int32_t address_w, std::int32_t anisotropy,
                                                      float level_offset, float finest, float coarsest) override
            {
                return gl_sampler_state_create_compare(compare, magnification, minification, mip, address_u,
                                                       address_v, address_w, anisotropy, level_offset, finest, coarsest);
            }
            bool state_release(std::uint64_t handle) override { return gl_state_release(handle); }

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
            std::int64_t texture_create_mips(std::int32_t kind, std::int32_t width, std::int32_t height,
                                             std::int32_t depth, std::int32_t format, bool storage,
                                             std::int32_t mip_count) override
            {
                return gl_texture_create_mips(kind, width, height, depth, format, storage, mip_count);
            }
            bool texture_generate_mips(std::uint64_t texture) override { return gl_texture_generate_mips(texture); }
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
            bool dispatch(std::int32_t x, std::int32_t y, std::int32_t z, std::uint64_t texture) override
            { return gl_dispatch_single(compute_program, x, y, z, texture, 0, 0); }
            bool dispatch_level(std::int32_t x, std::int32_t y, std::int32_t z, std::uint64_t texture, std::int32_t mip) override
            { return gl_dispatch_single(compute_program, x, y, z, texture, mip, 0); }
            bool dispatch_buffer(std::int32_t x, std::int32_t y, std::int32_t z, std::uint64_t buffer) override
            { return gl_dispatch_single(compute_program, x, y, z, 0, 0, buffer); }
            bool dispatch_both(std::int32_t x, std::int32_t y, std::int32_t z, std::uint64_t texture, std::uint64_t buffer) override
            {
                if (!texture || !buffer)
                {
                    set_last_error("igpu_dispatch: pass both a storage texture and buffer");
                    return false;
                }
                return gl_dispatch_single(compute_program, x, y, z, texture, 0, buffer);
            }
            bool dispatch_writes(std::int32_t x, std::int32_t y, std::int32_t z,
                                 const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets) override
            { return gl_dispatch_storage(compute_program, x, y, z, kinds, targets); }
            bool draw_to_render_targets(std::uint64_t vertex,std::uint64_t layout,std::int32_t primitive,std::int64_t first,std::int64_t count,const gm::wire::GMArrayView&targets) override
            {return gl_draw_targets(vertex,layout,primitive,first,count,targets,nullptr,nullptr);}
            bool draw_to_render_targets_level(std::uint64_t vertex,std::uint64_t layout,std::int32_t primitive,std::int64_t first,std::int64_t count,
                                              const gm::wire::GMArrayView&targets,const gm::wire::GMArrayView&mips) override
            {return gl_draw_targets(vertex,layout,primitive,first,count,targets,nullptr,&mips);}
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
            bool uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot) override
            { return gl_uniform_bind(buffer, stage, slot); }

            ~GlBackend() override
            {
                gl_uniform_resources_release();
                gl_compute_resources_release();
                gl_queries_release();
                const auto& fns = igpu_gl_fns();
                if (program != 0 && fns.DeleteProgram != nullptr)
                {
                    fns.DeleteProgram(program);
                }
                if (compute_program != 0 && fns.DeleteProgram != nullptr)
                {
                    fns.DeleteProgram(compute_program);
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
                if (stage != 0 && stage != 1 && (stage != 2 || !gl_compute_supported()))
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
                const GLenum type = stage == 0 ? GL_VERTEX_SHADER : stage == 1 ? GL_FRAGMENT_SHADER : 0x91B9 /* COMPUTE_SHADER */;
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
                const auto stage = it->second.stage;
                igpu_gl_fns().DeleteShader(it->second.shader);
                shaders.erase(it);
                if (stage == 2)
                {
                    if (bound.find(2) == bound.end() && compute_program)
                    {
                        igpu_gl_fns().DeleteProgram(compute_program);
                        compute_program = 0;
                    }
                }
                else relink();
                return true;
            }

            bool shader_bind(std::int64_t shader, std::int32_t stage) override
            {
                if (stage != 0 && stage != 1 && (stage != 2 || !gl_compute_supported()))
                {
                    set_last_error("igpu_shader_bind: unknown or unsupported shader stage");
                    return false;
                }
                if (shader == 0)
                {
                    bound.erase(stage);
                    if (stage == 2)
                    {
                        if (compute_program) igpu_gl_fns().DeleteProgram(compute_program);
                        compute_program = 0;
                        return true;
                    }
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
                if (stage == 2)
                {
                    const auto& fns = igpu_gl_fns();
                    const GLuint replacement = fns.CreateProgram();
                    fns.AttachShader(replacement, it->second.shader);
                    fns.LinkProgram(replacement);
                    GLint ok = 0;
                    fns.GetProgramiv(replacement, GL_LINK_STATUS, &ok);
                    if (!ok)
                    {
                        set_last_error("igpu_shader_bind: " + info_log(replacement, false));
                        fns.DeleteProgram(replacement);
                        return false;
                    }
                    if (compute_program) fns.DeleteProgram(compute_program);
                    compute_program = replacement;
                    bound[2] = static_cast<std::uint64_t>(shader);
                    return true;
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
            bool buffer_resize(std::uint64_t buffer, std::int64_t size) override { return gl_buffer_resize(buffer, size); }
            bool buffer_read(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& dest) override
            {
                return gl_buffer_read(buffer, offset, dest);
            }
            std::int64_t buffer_size(std::uint64_t buffer) override { return gl_buffer_size(buffer); }
            bool buffer_release(std::uint64_t buffer) override { return gl_buffer_release(buffer); }
            bool storage_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot) override
            { return gl_storage_bind(buffer, stage, slot); }
            bool buffer_patch(std::uint64_t buffer, std::int64_t offset, const void* data, std::size_t size, const char* entry) override
            { return gl_uniform_patch(buffer, offset, data, size, entry); }

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
                UniformLayout uniforms;
                bool uniform_ready = false;
            };

            std::unordered_map<std::uint64_t, ShaderObject> shaders;
            std::unordered_map<std::int32_t, std::uint64_t> bound;
            std::uint64_t next_shader = 1;
            GLuint program = 0;
            GLuint compute_program = 0;

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
