#pragma once

#include <cstdint>

namespace igpu
{
    std::uint64_t query_create(std::int32_t kind);
    bool query_begin(std::uint64_t query);
    bool query_end(std::uint64_t query);
    bool query_ready(std::uint64_t query);
    std::int64_t query_result(std::uint64_t query);
    bool query_release(std::uint64_t query);
    std::int64_t timestamp_frequency();

    std::uint64_t fence_create();
    bool fence_signal(std::uint64_t fence);
    bool fence_signaled(std::uint64_t fence);
    bool fence_release(std::uint64_t fence);
}
