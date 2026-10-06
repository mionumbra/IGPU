#pragma once

#include <cstdint>

#include "core/GMExtWire.h"

namespace igpu
{
    std::int64_t texture_create(std::int32_t width, std::int32_t height, std::int32_t format, bool render_target);
    std::int64_t texture_create_kind(
        std::int32_t kind,
        std::int32_t width,
        std::int32_t height,
        std::int32_t depth,
        std::int32_t format,
        bool render_target,
        bool storage);
    std::int64_t texture_create_mips(
        std::int32_t kind,
        std::int32_t width,
        std::int32_t height,
        std::int32_t depth,
        std::int32_t format,
        bool storage,
        std::int32_t mip_count);
    bool texture_generate_mips(std::uint64_t texture);
    bool texture_release(std::uint64_t texture);
    std::int64_t texture_get_pixel(std::uint64_t texture, std::int32_t x, std::int32_t y);
    std::int64_t texture_read(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer);
    std::int64_t texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip);

    bool draw_to_texture(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture);

    bool draw_to_texture_layer(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture,
        std::int32_t layer);

    bool draw_to_texture_level(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture,
        std::int32_t layer,
        std::int32_t mip);

    bool dispatch(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture);
    bool dispatch_level(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture, std::int32_t mip);
    bool dispatch_buffer(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_buffer);
    bool dispatch_both(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                       std::uint64_t storage_texture, std::uint64_t storage_buffer);
    bool dispatch_writes(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                         const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets);

    bool draw_to_render_targets(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        const gm::wire::GMArrayView& targets);

    bool draw_to_render_targets_level(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        const gm::wire::GMArrayView& targets,
        const gm::wire::GMArrayView& mips);

    bool draw_to_render_targets_layer(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        const gm::wire::GMArrayView& targets,
        const gm::wire::GMArrayView& layers,
        const gm::wire::GMArrayView& mips,
        std::int64_t blend_state,
        std::int64_t depth_state,
        std::int64_t raster_state,
        std::int64_t sampler_state);

    bool draw_sampled(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture,
        std::int64_t blend_state,
        std::int64_t depth_state,
        std::int64_t raster_state,
        std::int64_t sampler_state);
}
