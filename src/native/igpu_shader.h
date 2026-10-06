#pragma once

#include <cstdint>
#include <string_view>

namespace igpu
{
    // Public shader entry points. They forward to the active backend.
    std::int64_t shader_compile(std::string_view source, std::string_view entry,
                                std::int32_t stage, std::string_view dialect);
    bool shader_release(std::uint64_t shader);
    bool shader_bind(std::int64_t shader, std::int32_t stage);
    std::int64_t get_bound_shader(std::int32_t stage);
}
