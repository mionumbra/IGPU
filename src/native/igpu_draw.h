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

    // Binds the vertex buffer + layout and issues a draw, restoring GameMaker's
    // input-assembler state afterwards.
    //
    // `vertex_count` < 0 means "to the end of the buffer".
    bool draw(std::uint64_t vertex_buffer,
              std::uint64_t layout,
              std::int32_t primitive,
              std::int64_t first_vertex,
              std::int64_t vertex_count);

    bool draw_indexed(std::uint64_t vertex_buffer,
                      std::uint64_t layout,
                      std::uint64_t index_buffer,
                      std::int32_t primitive,
                      std::int64_t first_index,
                      std::int64_t index_count);

    std::int32_t draw_count();
    std::int32_t draw_restore_failures();

    // Reads the input assembler's slot-0 vertex buffer back from the device and
    // reports whether it is `buffer`. Queries the device rather than IGPU's own
    // bookkeeping, so it can prove a restore actually happened.
    bool is_vertex_buffer_bound(std::uint64_t buffer);
}
