#pragma once
#include "igpu_reflect.h"
#include <cstddef>
#include <cstdint>
namespace igpu
{
    struct GlUniformBuffer { std::uint32_t name = 0; std::int64_t bytes = 0; };
    bool gl_uniform_buffer(std::uint64_t buffer, GlUniformBuffer& out, const char* entry);
    bool gl_uniform_patch(std::uint64_t buffer, std::int64_t offset, const void* data, std::size_t bytes, const char* entry);
    void gl_uniform_initialize();
    bool gl_uniform_supported();
    bool gl_uniform_reflect(std::uint32_t shader, std::int32_t stage, UniformLayout& out);
    bool gl_uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot);
    bool gl_uniform_validate_inputs(std::int32_t first, std::int32_t last, const char* entry);
    void gl_uniform_release_buffer(std::uint64_t buffer);
    void gl_uniform_resources_release();
}
