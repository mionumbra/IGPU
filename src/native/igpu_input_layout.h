#pragma once

#include <cstdint>

#include "core/GMExtWire.h"

namespace igpu
{
    // Vertex element description, expressed in GameMaker's own vocabulary.
    // The numeric values mirror vertex_usage_* / vertex_type_* so a caller can
    // pass the GML constants straight through. They were read back from the
    // runtime rather than assumed - note that tangent=8 and binormal=9, which
    // alphabetical ordering would get wrong.
    enum class VertexUsage : std::int32_t
    {
        Position     = 1,
        Colour       = 2,
        Normal       = 3,
        Texcoord     = 4,
        BlendWeight  = 5,
        BlendIndices = 6,
        PSize        = 7,
        Tangent      = 8,
        Binormal     = 9
    };

    enum class VertexType : std::int32_t
    {
        Float1 = 1,
        Float2 = 2,
        Float3 = 3,
        Float4 = 4,
        Colour = 5,
        UByte4 = 6
    };

    // Builds a layout for `shader` from the parallel usage/type arrays.
    //
    // `stride` of 0 means "tightly packed" and is computed from the element
    // sizes. Returns a layout handle, or 0 on failure with an error set.
    std::int64_t input_layout_create(
        std::int64_t shader,
        const gm::wire::GMArrayView& usage,
        const gm::wire::GMArrayView& type,
        std::int32_t element_count,
        std::int32_t stride);

    std::int64_t input_layout_create_step(
        std::int64_t shader,
        const gm::wire::GMArrayView& usage,
        const gm::wire::GMArrayView& type,
        const gm::wire::GMArrayView& step,
        std::int32_t element_count,
        std::int32_t vertex_stride,
        std::int32_t instance_stride);

    bool input_layout_release(std::uint64_t layout);
}
