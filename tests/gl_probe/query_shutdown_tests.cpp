#include "query_shutdown_tests.h"
#include "IGPU_native.h"
#include "native/gl/igpu_gl_loader.h"

#include <array>
#include <cstdio>
#include <vector>

namespace
{
    constexpr GLenum samples_passed = 0x8914, current_query = 0x8865;
    using IsQuery = GLboolean(APIENTRY*)(GLuint);
    using IsSync = GLboolean(APIENTRY*)(IgpuGlSync);
    struct Allocations
    {
        static Allocations* active;
        IgpuGlGenQueries gen = igpu_gl_fns().GenQueries;
        IgpuGlFenceSync fence = igpu_gl_fns().FenceSync;
        std::vector<GLuint> queries;
        std::vector<IgpuGlSync> syncs;
        Allocations()
        {
            active = this;
            igpu_gl_fns().GenQueries = observe_queries;
            igpu_gl_fns().FenceSync = observe_fence;
        }
        ~Allocations() { stop(); }
        void stop()
        {
            if (active != this) return;
            igpu_gl_fns().GenQueries = gen;
            igpu_gl_fns().FenceSync = fence;
            active = nullptr;
        }
        static void APIENTRY observe_queries(GLsizei count, GLuint* names)
        {
            active->gen(count, names);
            active->queries.insert(active->queries.end(), names, names + count);
        }
        static IgpuGlSync APIENTRY observe_fence(GLenum condition, GLbitfield flags)
        {
            const auto sync = active->fence(condition, flags);
            if (sync) active->syncs.push_back(sync);
            return sync;
        }
    };
    Allocations* Allocations::active = nullptr;
    bool check(bool result, const char* label)
    {
        if (!result) std::fprintf(stderr, "query shutdown: %s: %s\n", label, igpu_get_last_error().c_str());
        return result;
    }
    bool query_buffer_interoperability()
    {
        int major = 0, minor = 0;
        const auto version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        if (!version || std::sscanf(version, "%d.%d", &major, &minor) != 2 ||
            major < 4 || (major == 4 && minor < 4)) return true;
        constexpr GLenum query_buffer = 0x9192, query_buffer_binding = 0x9193;
        auto& f = igpu_gl_fns();
        GLint previous = 0;
        glGetIntegerv(query_buffer_binding, &previous);
        GLuint foreign = 0;
        f.GenBuffers(1, &foreign);
        f.BindBuffer(query_buffer, foreign);
        std::array<unsigned char, 64> sentinel{}, actual{};
        sentinel.fill(0xA9);
        f.BufferData(query_buffer, sentinel.size(), sentinel.data(), 0x88E4);
        const auto query = igpu_query_create(0);
        const bool ended = query && igpu_query_begin(query) && igpu_query_end(query);
        glFinish();
        const bool ready = ended && igpu_query_ready(query);
        const bool result = ready && igpu_query_result(query) == 0 && igpu_get_last_error().empty();
        GLint held = 0;
        glGetIntegerv(query_buffer_binding, &held);
        f.GetBufferSubData(query_buffer, 0, actual.size(), actual.data());
        const auto error = glGetError();
        f.BindBuffer(query_buffer, static_cast<GLuint>(previous));
        f.DeleteBuffers(1, &foreign);
        if (query) igpu_query_release(query);
        if (!check(ready && result && held == static_cast<GLint>(foreign) && actual == sentinel &&
            error == GL_NO_ERROR, "CPU query results preserve the external query-buffer binding and bytes"))
        {
            std::fprintf(stderr, "query-buffer ready=%d result=%d held=%d expected=%u GLerror=0x%x\n",
                ready, result, held, foreign, error);
            return false;
        }
        std::printf("query state: bound external query-buffer retained while CPU ready/result collection succeeds\n");
        return true;
    }

}

bool run_gl_query_shutdown_tests()
{
    if (!query_buffer_interoperability()) return false;
    auto& f = igpu_gl_fns();
    const auto is_query = reinterpret_cast<IsQuery>(wglGetProcAddress("glIsQuery"));
    const auto is_sync = reinterpret_cast<IsSync>(wglGetProcAddress("glIsSync"));
    if (!check(is_query && is_sync, "native inspection functions")) return false;
    const auto context = wglGetCurrentContext();
    Allocations observed;
    const auto ended = igpu_query_create(0), active = igpu_query_create(0);
    const auto timing = igpu_query_create(1), timing_open = igpu_query_create(1);
    const auto unsignaled = igpu_fence_create(), signaled = igpu_fence_create();
    if (!check(ended && active && timing && timing_open && unsignaled && signaled &&
        igpu_query_begin(ended) && igpu_query_end(ended) &&
        igpu_query_begin(timing) && igpu_query_end(timing) &&
        igpu_query_begin(timing_open) && igpu_query_begin(active) &&
        igpu_fence_signal(signaled), "prepare pending, open and unsignaled objects")) return false;
    std::size_t live = 0;
    for (const auto name : observed.queries) live += is_query(name) ? 1 : 0;
    if (!check(live >= 4 && observed.syncs.size() == 1 && is_sync(observed.syncs[0]),
        "driver objects exist before shutdown")) return false;
    igpu_shutdown();
    GLint current = -1;
    f.GetQueryiv(samples_passed, current_query, &current);
    if (!check(current == 0 && !igpu_is_available() && wglGetCurrentContext() == context,
        "shutdown ends owned query and preserves borrowed context")) return false;
    for (const auto name : observed.queries)
        if (!check(!is_query(name), "pending native query was deleted")) return false;
    for (const auto sync : observed.syncs)
        if (!check(!is_sync(sync), "pending native sync was deleted")) return false;
    observed.stop();
    if (!check(igpu_bind_current() && igpu_timestamp_frequency() == 0,
        "rebind resets the timestamp frequency")) return false;
    if (!check(!igpu_query_release(ended) && !igpu_query_begin(active) &&
        !igpu_query_release(timing) && !igpu_fence_signal(signaled),
        "old public handles remain invalid after rebind")) return false;
    const auto reused = igpu_query_create(0);
    if (!check(reused > active && igpu_query_begin(reused) && igpu_query_end(reused),
        "new handles and queries work after rebind")) return false;

    // An external caller owns this raw query; teardown must leave it open.
    GLuint external = 0;
    f.GenQueries(1, &external);
    f.BeginQuery(samples_passed, external);
    igpu_shutdown();
    current = 0;
    f.GetQueryiv(samples_passed, current_query, &current);
    const bool preserved = current == static_cast<GLint>(external) && is_query(external) &&
        wglGetCurrentContext() == context;
    f.EndQuery(samples_passed);
    f.DeleteQueries(1, &external);
    if (!check(preserved, "shutdown preserves the external active occlusion query")) return false;
    if (!check(igpu_bind_current(), "final rebind")) return false;
    std::printf("query shutdown: pending and open queries/syncs deleted, stale handles rejected, external query/context preserved\n");
    return true;
}
