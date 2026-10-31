#pragma once
#include "core/GMExtWire.h"
#include <cstdint>

namespace igpu
{
    struct GlStorageBuffer { std::uint32_t name = 0; std::int64_t bytes = 0; };
    struct GlStorageTexture { std::uint32_t name = 0, format = 0; std::int32_t level = 0; };
    bool gl_storage_buffer(std::uint64_t handle, GlStorageBuffer& out, const char* entry);
    bool gl_storage_texture(std::uint64_t handle, std::int32_t mip, GlStorageTexture& out, const char* entry);

    void gl_compute_initialize();
    bool gl_compute_supported();
    bool gl_read_storage_supported();
    bool gl_storage_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot);
    void gl_storage_release_buffer(std::uint64_t buffer);
    void gl_compute_resources_release();
    bool gl_dispatch_storage(std::uint32_t program, std::int32_t x, std::int32_t y, std::int32_t z,
                             const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets);
    bool gl_dispatch_single(std::uint32_t program, std::int32_t x, std::int32_t y, std::int32_t z,
                            std::uint64_t texture, std::int32_t mip, std::uint64_t buffer);
}
