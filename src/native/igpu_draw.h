#pragma once

#include <cstdint>

namespace igpu
{
    // Primitive topology, mirroring GameMaker's pr_* constants. Values were
    // read back from the runtime, not inferred.
    enum class Primitive : std::int32_t
    {
        PointList = 1,
        LineList = 2,
        LineStrip = 3,
        TriangleList = 4,
        TriangleStrip = 5,
        TriangleFan = 6
    };

    // Binds the vertex buffer + layout and issues a draw, then puts the
    // input-assembler state back. GameMaker rebinds its own vertex buffer,
    // layout and topology on its next draw; this still restores the index
    // buffer, which GameMaker never touches, and leaves the device consistent
    // for anything that reads the assembler before that draw.
    //
    // `vertex_count` < 0 means "to the end of the buffer".
    // State handles of 0 leave that stage untouched. Non-zero handles are
    // applied for this draw only; the previous device state is put back
    // before return.
    bool draw(std::uint64_t vertex_buffer,
              std::uint64_t layout,
              std::int32_t primitive,
              std::int64_t first_vertex,
              std::int64_t vertex_count,
              std::int64_t blend_state = 0,
              std::int64_t depth_state = 0,
              std::int64_t raster_state = 0,
              std::int64_t sampler_state = 0);

    // instance_buffer 0 draws copies that differ only by the instance number.
    // A non-zero buffer supplies the layout's per-instance elements.
    bool draw_instanced(std::uint64_t vertex_buffer,
                        std::uint64_t instance_buffer,
                        std::uint64_t layout,
                        std::int32_t primitive,
                        std::int64_t first_vertex,
                        std::int64_t vertex_count,
                        std::int64_t instance_count);

    bool draw_indirect(std::uint64_t vertex_buffer,
                       std::uint64_t instance_buffer,
                       std::uint64_t layout,
                       std::int32_t primitive,
                       std::uint64_t args,
                       std::int64_t args_offset);

    // control_points is 1..32. vertex_count must cover whole patches.
    // A hull shader and a domain shader are required to already be bound.
    bool draw_patch(std::uint64_t vertex_buffer,
                    std::uint64_t layout,
                    std::int32_t control_points,
                    std::int64_t first_vertex,
                    std::int64_t vertex_count);

    bool draw_indexed_indirect(std::uint64_t vertex_buffer,
                               std::uint64_t instance_buffer,
                               std::uint64_t layout,
                               std::uint64_t index_buffer,
                               std::int32_t primitive,
                               std::uint64_t args,
                               std::int64_t args_offset);

    bool draw_indexed(std::uint64_t vertex_buffer,
                      std::uint64_t layout,
                      std::uint64_t index_buffer,
                      std::int32_t primitive,
                      std::int64_t first_index,
                      std::int64_t index_count,
                      std::int64_t blend_state = 0,
                      std::int64_t depth_state = 0,
                      std::int64_t raster_state = 0,
                      std::int64_t sampler_state = 0);

    std::int32_t draw_count();
    std::int32_t draw_restore_failures();

    // Reads the input assembler's slot-0 vertex buffer back from the device and
    // reports whether it is `buffer`. Queries the device rather than IGPU's own
    // bookkeeping, so it can prove a restore actually happened.
    bool is_vertex_buffer_bound(std::uint64_t buffer);
}
