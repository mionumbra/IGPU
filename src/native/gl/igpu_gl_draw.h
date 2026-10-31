#pragma once

#include "core/GMExtWire.h"

#include <cstdint>

namespace igpu
{
    void gl_resources_release();
    void gl_draw_capabilities_initialize();
    bool gl_instancing_supported();
    bool gl_indirect_draw_supported();
    bool gl_draw_instanced(std::uint64_t vertex, std::uint64_t instances, std::uint64_t layout,
                          std::int32_t primitive, std::int64_t first, std::int64_t count, std::int64_t instance_count,
                          std::int64_t blend, std::int64_t depth, std::int64_t raster, std::int64_t sampler);
    bool gl_draw_indirect_record(std::uint64_t vertex, std::uint64_t instances, std::uint64_t layout,
                                std::uint64_t index, bool indexed, std::int32_t primitive, std::uint64_t args,
                                std::int64_t offset, std::int64_t blend, std::int64_t depth,
                                std::int64_t raster, std::int64_t sampler);

    std::int64_t gl_buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride);
    bool gl_buffer_write(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& data);
    bool gl_buffer_resize(std::uint64_t buffer, std::int64_t size);
    bool gl_buffer_read(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& dest);
    std::int64_t gl_buffer_size(std::uint64_t buffer);
    bool gl_buffer_release(std::uint64_t buffer);

    std::int64_t gl_input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                        const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step,
                                        std::int32_t element_count, std::int32_t vertex_stride,
                                        std::int32_t instance_stride);
    bool gl_input_layout_release(std::uint64_t layout);

    std::int64_t gl_texture_create_kind(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                        std::int32_t format, bool render_target, bool storage);
    std::int64_t gl_texture_create_mips(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                        std::int32_t format, bool storage, std::int32_t mip_count);
    bool gl_texture_format_supported(std::int32_t format);
    bool gl_texture_generate_mips(std::uint64_t texture);
    bool gl_texture_release(std::uint64_t texture);
    std::int64_t gl_texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer,
                                       std::int32_t mip);

    std::int64_t gl_sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                       std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                       std::int32_t address_w, float level_offset, float finest,
                                                       float coarsest, std::int32_t anisotropy = 1);
    std::int64_t gl_sampler_state_create_filters_border_range(std::int32_t magnification, std::int32_t minification,
                                                              std::int32_t mip, std::int32_t address_u,
                                                              std::int32_t address_v, std::int32_t address_w, float red,
                                                              float green, float blue, float alpha, float level_offset,
                                                              float finest, float coarsest, std::int32_t anisotropy = 1);
    std::int64_t gl_sampler_state_create_compare(std::int32_t compare, std::int32_t magnification,
                                                  std::int32_t minification, std::int32_t mip,
                                                  std::int32_t address_u, std::int32_t address_v,
                                                  std::int32_t address_w, std::int32_t anisotropy,
                                                  float level_offset, float finest, float coarsest);
    std::int64_t gl_blend_state_create(bool enabled, std::int32_t src, std::int32_t dest, std::int32_t equation,
                                       std::int32_t src_alpha, std::int32_t dest_alpha, std::int32_t equation_alpha,
                                       bool write_red, bool write_green, bool write_blue, bool write_alpha);
    std::int64_t gl_raster_state_create(std::int32_t cull, std::int32_t fill, bool scissor, bool depth_clip);
    std::int64_t gl_depth_state_create(bool depth_test, bool depth_write, std::int32_t depth_func, bool stencil_enable,
                                       std::int32_t stencil_func, std::int32_t stencil_fail,
                                       std::int32_t stencil_depth_fail, std::int32_t stencil_pass,
                                       std::int32_t stencil_ref, std::int32_t stencil_read_mask,
                                       std::int32_t stencil_write_mask);
    bool gl_state_release(std::uint64_t handle);

    bool gl_draw(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                 std::int64_t first_vertex, std::int64_t vertex_count, std::int64_t blend_state,
                 std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state);

    bool gl_draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer,
                         std::int32_t primitive, std::int64_t first_index, std::int64_t index_count,
                         std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                         std::int64_t sampler_state);

    bool gl_draw_sampled(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                         std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                         std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                         std::int64_t sampler_state);

    bool gl_color_target_begin(std::uint64_t texture, std::int32_t& previous_framebuffer,
                               std::int32_t previous_viewport[4]);
    void gl_color_target_end(std::int32_t previous_framebuffer, const std::int32_t previous_viewport[4]);
    // Probe fixture. Attaches a depth buffer to an existing color target and clears that depth to 1.
    bool gl_attach_depth(std::uint64_t texture);

    bool gl_draw_to_render_targets_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                         std::int64_t first_vertex, std::int64_t vertex_count,
                                         const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers,
                                         const gm::wire::GMArrayView& mips, std::int64_t blend_state,
                                         std::int64_t depth_state, std::int64_t raster_state,
                                         std::int64_t sampler_state);
}
