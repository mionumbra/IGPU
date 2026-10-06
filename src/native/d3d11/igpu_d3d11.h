#pragma once

#include <d3d11.h>

#include <memory>

#include "../igpu_backend.h"
#include "../igpu_reflect.h"

namespace igpu
{
    // The one place that turns borrowed device pointers into a Backend.
    // Another graphics API adds its own factory and is selected next to the
    // call in bind_device(). Entry points above this header do not include it.
    std::unique_ptr<Backend> make_d3d11_backend(ID3D11Device* device, ID3D11DeviceContext* context);

    // Bodies of the draw, texture and state operations. The Backend methods
    // forward here. The public igpu:: functions forward to the active Backend,
    // so these must not be named igpu::draw or a texture draw would recurse.
    namespace d3d11_impl
    {
        // Defaults stay on this free function so a texture draw can call draw()
        // with five arguments and still hit this implementation. The public
        // igpu::draw is a different function; calling that from here would recurse.
        bool draw(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                  std::int64_t first_vertex, std::int64_t vertex_count,
                  std::int64_t blend_state = 0, std::int64_t depth_state = 0,
                  std::int64_t raster_state = 0, std::int64_t sampler_state = 0);
        bool draw_instanced(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout,
                            std::int32_t primitive, std::int64_t first_vertex, std::int64_t vertex_count,
                            std::int64_t instance_count);
        bool draw_indirect(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout,
                           std::int32_t primitive, std::uint64_t args, std::int64_t args_offset);
        bool draw_patch(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t control_points,
                        std::int64_t first_vertex, std::int64_t vertex_count);
        bool draw_indexed_indirect(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout,
                                   std::uint64_t index_buffer, std::int32_t primitive, std::uint64_t args,
                                   std::int64_t args_offset);
        bool draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer,
                          std::int32_t primitive, std::int64_t first_index, std::int64_t index_count,
                          std::int64_t blend_state = 0, std::int64_t depth_state = 0,
                          std::int64_t raster_state = 0, std::int64_t sampler_state = 0);
        std::int32_t draw_count();
        std::int32_t draw_restore_failures();
        bool is_vertex_buffer_bound(std::uint64_t buffer);

        std::int64_t blend_state_create(bool enabled, std::int32_t src, std::int32_t dest, std::int32_t equation,
                                        std::int32_t src_alpha, std::int32_t dest_alpha, std::int32_t equation_alpha,
                                        bool write_red, bool write_green, bool write_blue, bool write_alpha);
        std::int64_t depth_state_create(bool depth_test, bool depth_write, std::int32_t depth_func,
                                        bool stencil_enable, std::int32_t stencil_func, std::int32_t stencil_fail,
                                        std::int32_t stencil_depth_fail, std::int32_t stencil_pass,
                                        std::int32_t stencil_ref, std::int32_t stencil_read_mask,
                                        std::int32_t stencil_write_mask);
        std::int64_t raster_state_create(std::int32_t cull, std::int32_t fill, bool scissor, bool depth_clip);
        std::int64_t sampler_state_create(std::int32_t filter, bool repeat, std::int32_t anisotropy);
        std::int64_t sampler_state_create_address(std::int32_t filter, std::int32_t address, std::int32_t anisotropy);
        std::int64_t sampler_state_create_border(std::int32_t filter, std::int32_t anisotropy,
                                                 float red, float green, float blue, float alpha);
        std::int64_t sampler_state_create_axes(std::int32_t filter, std::int32_t address_u, std::int32_t address_v,
                                               std::int32_t address_w, std::int32_t anisotropy);
        std::int64_t sampler_state_create_axes_border(std::int32_t filter, std::int32_t address_u, std::int32_t address_v,
                                                      std::int32_t address_w, std::int32_t anisotropy,
                                                      float red, float green, float blue, float alpha);
        std::int64_t sampler_state_create_axes_range(std::int32_t filter, std::int32_t address_u, std::int32_t address_v,
                                                     std::int32_t address_w, std::int32_t anisotropy, float level_offset,
                                                     float finest, float coarsest);
        std::int64_t sampler_state_create_axes_border_range(std::int32_t filter, std::int32_t address_u,
                                                            std::int32_t address_v, std::int32_t address_w,
                                                            std::int32_t anisotropy, float red, float green, float blue,
                                                            float alpha, float level_offset, float finest, float coarsest);
        std::int64_t sampler_state_create_filters(std::int32_t magnification, std::int32_t minification, std::int32_t mip,
                                                  std::int32_t address_u, std::int32_t address_v, std::int32_t address_w);
        std::int64_t sampler_state_create_filters_border(std::int32_t magnification, std::int32_t minification,
                                                         std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                         std::int32_t address_w, float red, float green, float blue, float alpha);
        std::int64_t sampler_state_create_filters_offset(std::int32_t magnification, std::int32_t minification,
                                                         std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                         std::int32_t address_w, float level_offset);
        std::int64_t sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                        std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                        std::int32_t address_w, float level_offset, float finest,
                                                        float coarsest);
        std::int64_t sampler_state_create_filters_border_range(std::int32_t magnification, std::int32_t minification,
                                                               std::int32_t mip, std::int32_t address_u,
                                                               std::int32_t address_v, std::int32_t address_w, float red,
                                                               float green, float blue, float alpha, float level_offset,
                                                               float finest, float coarsest);
        std::int64_t sampler_state_create_compare(std::int32_t compare, std::int32_t magnification,
                                                  std::int32_t minification, std::int32_t mip, std::int32_t address_u,
                                                  std::int32_t address_v, std::int32_t address_w);
        bool state_release(std::uint64_t handle);

        std::int64_t texture_create(std::int32_t width, std::int32_t height, std::int32_t format, bool render_target);
        std::int64_t texture_create_kind(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                         std::int32_t format, bool render_target, bool storage, std::int32_t mip_count = 1);
        std::int64_t texture_create_mips(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                         std::int32_t format, bool storage, std::int32_t mip_count);
        bool texture_generate_mips(std::uint64_t texture);
        bool texture_release(std::uint64_t texture);
        std::int64_t texture_get_pixel(std::uint64_t texture, std::int32_t x, std::int32_t y);
        std::int64_t texture_read(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer);
        std::int64_t texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip);
        bool draw_to_texture(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                             std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture);
        bool draw_to_texture_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                   std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                                   std::int32_t layer);
        bool draw_to_texture_level(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                   std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                                   std::int32_t layer, std::int32_t mip);
        bool dispatch(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture);
        bool dispatch_level(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture, std::int32_t mip);
        bool dispatch_buffer(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_buffer);
        bool dispatch_both(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                           std::uint64_t storage_texture, std::uint64_t storage_buffer);
        bool dispatch_writes(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                             const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets);
        bool draw_to_render_targets(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                    std::int64_t first_vertex, std::int64_t vertex_count,
                                    const gm::wire::GMArrayView& targets);
        bool draw_to_render_targets_level(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                          std::int64_t first_vertex, std::int64_t vertex_count,
                                          const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& mips);
        bool draw_to_render_targets_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                          std::int64_t first_vertex, std::int64_t vertex_count,
                                          const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers,
                                          const gm::wire::GMArrayView& mips);
        bool draw_sampled(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                          std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                          std::int64_t sampler);

        bool reflect_uniforms(const void* bytecode, std::size_t size, UniformLayout& out);

        std::int64_t shader_compile(std::string_view source, std::string_view entry,
                                    std::int32_t stage, std::string_view dialect);
        bool shader_release(std::uint64_t shader);
        bool shader_bind(std::int64_t shader, std::int32_t stage);
        std::int64_t get_bound_shader(std::int32_t stage);

        std::int64_t buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride);
        bool buffer_write(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& data);
        bool buffer_resize(std::uint64_t buffer, std::int64_t size);
        bool buffer_read(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& dest);
        std::int64_t buffer_size(std::uint64_t buffer);
        bool buffer_release(std::uint64_t buffer);
        bool buffer_patch(std::uint64_t buffer, std::int64_t offset, const void* data,
                          std::size_t size, const char* entry);

        std::int64_t input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                         const gm::wire::GMArrayView& type, std::int32_t element_count,
                                         std::int32_t stride);
        std::int64_t input_layout_create_step(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                              const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step,
                                              std::int32_t element_count, std::int32_t vertex_stride,
                                              std::int32_t instance_stride);
        bool input_layout_release(std::uint64_t layout);

        bool texture_format(std::int32_t format);

        // Rewrites GLSL or GLSL ES into HLSL. The entry point name is kept.
        // False leaves an igpu_shader_compile error.
        bool translate_glsl_to_hlsl(std::string_view source, std::int32_t stage, std::string_view entry,
                                    std::string& hlsl);
    }
}
