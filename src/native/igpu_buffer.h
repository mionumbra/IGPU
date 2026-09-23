#pragma once

#include <cstdint>

#include "core/GMExtWire.h"

namespace igpu
{
    // How often a buffer's contents change. Mirrors IgpuBufferUsage in
    // spec.gmidl; the numeric values must stay in sync with it.
    enum class BufferUsage : std::int32_t
    {
        Static = 0,
        Dynamic = 1,
        Staging = 2
    };

    // What a buffer may be used for. Bitwise-OR of these is passed in.
    // Mirrors IgpuBufferBind in spec.gmidl.
    enum class BufferBind : std::int32_t
    {
        None = 0,
        Vertex = 1,
        Index = 2,
        Uniform = 4,
        Storage = 8
    };

    std::int64_t buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind);

    // Copies the contents of a GameMaker buffer into the GPU buffer.
    // GMBuffer carries its own length, so no separate size is needed and the
    // copy can never exceed the source allocation.
    bool buffer_write(std::uint64_t buffer, std::int64_t offset,
                      gm::wire::GMBuffer data);

    bool buffer_resize(std::uint64_t buffer, std::int64_t size);

    // Copies bytes out of the GPU buffer into a GameMaker buffer.
    bool buffer_read(std::uint64_t buffer, std::int64_t offset,
                     gm::wire::GMBuffer dest);

    std::int64_t buffer_size(std::uint64_t buffer);

    bool buffer_release(std::uint64_t buffer);
}
