#pragma once

#include <cstdint>

namespace igpu
{
    void gl_queries_initialize();
    void gl_queries_release();
    bool gl_occlusion_supported();
    bool gl_timestamps_supported();
    bool gl_fences_supported();
    std::int64_t gl_timestamp_frequency();

    std::uint64_t gl_query_create(std::int32_t kind);
    bool gl_query_begin(std::uint64_t query);
    bool gl_query_end(std::uint64_t query);
    bool gl_query_ready(std::uint64_t query);
    std::int64_t gl_query_result(std::uint64_t query);
    bool gl_query_release(std::uint64_t query);
    std::uint64_t gl_fence_create();
    bool gl_fence_signal(std::uint64_t fence);
    bool gl_fence_signaled(std::uint64_t fence);
    bool gl_fence_release(std::uint64_t fence);

    // Hidden comparison-copy draws must not count as public occlusion samples.
    // Reserve the continuation before ending the current segment, then resume on
    // every exit. External raw GL queries are rejected instead of being altered.
    class GlOcclusionPause
    {
    public:
        GlOcclusionPause();
        ~GlOcclusionPause();
        bool valid() const { return valid_; }
        GlOcclusionPause(const GlOcclusionPause&) = delete;
        GlOcclusionPause& operator=(const GlOcclusionPause&) = delete;
    private:
        unsigned int continuation_ = 0;
        bool valid_ = false;
    };
}
