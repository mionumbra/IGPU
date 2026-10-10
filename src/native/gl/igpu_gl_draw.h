#pragma once

#include "core/GMExtWire.h"

#include <cstdint>

namespace igpu
{
    void gl_resources_release();

    std::int64_t gl_buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride);
    bool gl_buffer_write(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& data);
    bool gl_buffer_release(std::uint64_t buffer);

    std::int64_t gl_input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                        const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step,
                                        std::int32_t element_count, std::int32_t vertex_stride,
                                        std::int32_t instance_stride);
    bool gl_input_layout_release(std::uint64_t layout);

    std::int64_t gl_texture_create_kind(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                        std::int32_t format, bool render_target, bool storage);
    bool gl_texture_release(std::uint64_t texture);
    std::int64_t gl_texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer,
                                       std::int32_t mip);

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

    bool gl_draw_to_render_targets_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                         std::int64_t first_vertex, std::int64_t vertex_count,
                                         const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers,
                                         const gm::wire::GMArrayView& mips, std::int64_t blend_state,
                                         std::int64_t depth_state, std::int64_t raster_state,
                                         std::int64_t sampler_state);
}
