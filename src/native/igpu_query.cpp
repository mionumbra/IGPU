#include "igpu_backend.h"
#include "igpu_error.h"

#include <string>

namespace igpu
{
    namespace
    {
        Backend* need(const char* entry)
        {
            Backend* backend = active_backend();
            if (backend == nullptr)
            {
                set_last_error(std::string(entry) + ": call igpu_init() first");
            }
            return backend;
        }
    }

    std::uint64_t query_create(std::int32_t kind)
    {
        clear_last_error();
        Backend* backend = need("igpu_query_create");
        return backend == nullptr ? 0 : backend->query_create(kind);
    }

    bool query_begin(std::uint64_t query)
    {
        clear_last_error();
        Backend* backend = need("igpu_query_begin");
        return backend != nullptr && backend->query_begin(query);
    }

    bool query_end(std::uint64_t query)
    {
        clear_last_error();
        Backend* backend = need("igpu_query_end");
        return backend != nullptr && backend->query_end(query);
    }

    bool query_ready(std::uint64_t query)
    {
        clear_last_error();
        Backend* backend = need("igpu_query_ready");
        return backend != nullptr && backend->query_ready(query);
    }

    std::int64_t query_result(std::uint64_t query)
    {
        clear_last_error();
        Backend* backend = need("igpu_query_result");
        return backend == nullptr ? 0 : backend->query_result(query);
    }

    bool query_release(std::uint64_t query)
    {
        clear_last_error();
        Backend* backend = need("igpu_query_release");
        return backend != nullptr && backend->query_release(query);
    }

    std::int64_t timestamp_frequency()
    {
        clear_last_error();
        Backend* backend = need("igpu_timestamp_frequency");
        return backend == nullptr ? 0 : backend->timestamp_frequency();
    }

    std::uint64_t fence_create()
    {
        clear_last_error();
        Backend* backend = need("igpu_fence_create");
        return backend == nullptr ? 0 : backend->fence_create();
    }

    bool fence_signal(std::uint64_t fence)
    {
        clear_last_error();
        Backend* backend = need("igpu_fence_signal");
        return backend != nullptr && backend->fence_signal(fence);
    }

    bool fence_signaled(std::uint64_t fence)
    {
        clear_last_error();
        Backend* backend = need("igpu_fence_signaled");
        return backend != nullptr && backend->fence_signaled(fence);
    }

    bool fence_release(std::uint64_t fence)
    {
        clear_last_error();
        Backend* backend = need("igpu_fence_release");
        return backend != nullptr && backend->fence_release(fence);
    }
}
