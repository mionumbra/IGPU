#include "igpu_gl_query.h"

#include "igpu_error.h"
#include "igpu_gl_loader.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace igpu
{
    namespace
    {
        constexpr GLenum samples_passed = 0x8914;
        constexpr GLenum any_samples_passed = 0x8C2F;
        constexpr GLenum conservative_samples = 0x8D6A;
        constexpr GLenum current_query = 0x8865;
        constexpr GLenum counter_bits = 0x8864;
        constexpr GLenum query_result = 0x8866;
        constexpr GLenum result_available = 0x8867;
        constexpr GLenum timestamp = 0x8E28;
        constexpr GLenum gpu_commands_complete = 0x9117;
        constexpr GLenum already_signaled = 0x911A;
        constexpr GLenum condition_satisfied = 0x911C;
        constexpr GLenum wait_failed = 0x911D;
        constexpr std::int64_t nanoseconds_per_second = 1000000000;

        struct Query
        {
            int kind = 0;
            std::vector<GLuint> names;
            bool begun = false;
            bool ended = false;
            bool collected = false;
            std::int64_t value = 0;
        };
        struct Fence { IgpuGlSync sync = nullptr; };
        std::unordered_map<std::uint64_t, Query> queries;
        std::unordered_map<std::uint64_t, Fence> fences;
        std::uint64_t next_query = 1, next_fence = 1, active_occlusion = 0;
        GLint sample_bits = 0, timestamp_bits = 0;
        bool query64 = false, any_queries = false, conservative_queries = false, sync_supported = false;
        bool query_buffers = false;
        std::int64_t frequency = 0;

        bool fail(const char* entry, const char* message)
        {
            set_last_error(std::string(entry) + ": " + message);
            return false;
        }

        bool valid_proc(PROC entry)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(entry);
            return address > 3 && address != (std::numeric_limits<std::uintptr_t>::max)();
        }

        bool extension(const char* expected, int major)
        {
            if (major >= 3)
            {
                using GetStringi = const GLubyte*(APIENTRY*)(GLenum, GLuint);
                const auto entry = wglGetProcAddress("glGetStringi");
                if (!valid_proc(entry)) return false;
                const auto get_string = reinterpret_cast<GetStringi>(entry);
                GLint count = 0;
                glGetIntegerv(0x821D /* NUM_EXTENSIONS */, &count);
                for (GLint i = 0; i < count; ++i)
                {
                    const auto name = reinterpret_cast<const char*>(get_string(GL_EXTENSIONS, i));
                    if (name && std::strcmp(name, expected) == 0) return true;
                }
                return false;
            }
            const auto list = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
            if (!list) return false;
            const std::string all = std::string(" ") + list + " ";
            return all.find(std::string(" ") + expected + " ") != std::string::npos;
        }

        bool common_queries()
        {
            const auto& f = igpu_gl_fns();
            return f.GenQueries && f.DeleteQueries && f.GetQueryiv && f.GetQueryObjectiv;
        }

        struct CurrentQuery { GLenum target = 0; GLuint name = 0; };
        CurrentQuery current_occlusion_query()
        {
            const auto& f = igpu_gl_fns();
            if (!f.GetQueryiv) return {};
            const GLenum targets[] = { samples_passed, any_samples_passed, conservative_samples };
            const int count = conservative_queries ? 3 : (any_queries ? 2 : 1);
            for (int i = 0; i < count; ++i)
            {
                GLint name = 0;
                f.GetQueryiv(targets[i], current_query, &name);
                if (name) return { targets[i], static_cast<GLuint>(name) };
            }
            return {};
        }

        Query* find_query(std::uint64_t id, const char* entry)
        {
            const auto it = queries.find(id);
            if (it == queries.end()) { fail(entry, "unknown query handle"); return nullptr; }
            return &it->second;
        }
        Fence* find_fence(std::uint64_t id, const char* entry)
        {
            const auto it = fences.find(id);
            if (it == fences.end()) { fail(entry, "unknown fence handle"); return nullptr; }
            return &it->second;
        }
        void delete_names(Query& query)
        {
            auto& f = igpu_gl_fns();
            if (!query.names.empty() && f.DeleteQueries)
                f.DeleteQueries(static_cast<GLsizei>(query.names.size()), query.names.data());
            query.names.clear();
        }
        bool allocate_names(int kind, std::vector<GLuint>& names, const char* entry)
        {
            names.assign(kind == 0 ? 1 : 2, 0);
            auto& f = igpu_gl_fns();
            f.GenQueries(static_cast<GLsizei>(names.size()), names.data());
            for (const auto name : names)
            {
                if (name) continue;
                f.DeleteQueries(static_cast<GLsizei>(names.size()), names.data());
                names.clear();
                return fail(entry, "could not allocate OpenGL query objects");
            }
            return true;
        }
        struct CpuQueryResults
        {
            GLint previous = 0;
            CpuQueryResults()
            {
                if (!query_buffers) return;
                glGetIntegerv(0x9193 /* QUERY_BUFFER_BINDING */, &previous);
                if (previous) igpu_gl_fns().BindBuffer(0x9192 /* QUERY_BUFFER */, 0);
            }
            ~CpuQueryResults()
            {
                if (previous) igpu_gl_fns().BindBuffer(0x9192 /* QUERY_BUFFER */, static_cast<GLuint>(previous));
            }
        };
        enum class Collected { Pending, Ready, Error };
        Collected collect(Query& query, const char* entry)
        {
            if (query.collected) return Collected::Ready;
            CpuQueryResults cpu_results;
            auto& f = igpu_gl_fns();
            for (const auto name : query.names)
            {
                GLint ready = 0;
                f.GetQueryObjectiv(name, result_available, &ready);
                if (!ready) return Collected::Pending;
            }
            const auto limit = static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
            std::uint64_t result = 0;
            if (query.kind == 0)
            {
                for (const auto name : query.names)
                {
                    std::uint64_t samples = 0;
                    if (query64 && f.GetQueryObjectui64v) f.GetQueryObjectui64v(name, query_result, &samples);
                    else
                    {
                        GLuint value = 0;
                        f.GetQueryObjectuiv(name, query_result, &value);
                        samples = value;
                    }
                    if (samples > limit - result)
                    {
                        fail(entry, "the sample count exceeds the signed result range");
                        return Collected::Error;
                    }
                    result += samples;
                }
            }
            else
            {
                std::uint64_t start = 0, end = 0;
                f.GetQueryObjectui64v(query.names[0], query_result, &start);
                f.GetQueryObjectui64v(query.names[1], query_result, &end);
                result = end - start;
                if (timestamp_bits < 64) result &= (std::uint64_t{1} << timestamp_bits) - 1;
                if (result > limit)
                {
                    fail(entry, "the timestamp interval exceeds the signed result range");
                    return Collected::Error;
                }
                frequency = nanoseconds_per_second;
            }
            query.value = static_cast<std::int64_t>(result);
            query.collected = true;
            return Collected::Ready;
        }
    }

    void gl_queries_initialize()
    {
        sample_bits = timestamp_bits = 0;
        frequency = 0;
        int major = 0, minor = 0;
        const auto version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        if (version) std::sscanf(version, "%d.%d", &major, &minor);
        const auto at_least = [=](int a, int b) { return major > a || (major == a && minor >= b); };
        query64 = at_least(3, 3) || extension("GL_ARB_timer_query", major);
        any_queries = at_least(3, 3) || extension("GL_ARB_occlusion_query2", major);
        conservative_queries = at_least(4, 3) || extension("GL_ARB_ES3_compatibility", major);
        query_buffers = at_least(4, 4) || extension("GL_ARB_query_buffer_object", major);
        sync_supported = at_least(3, 2) || extension("GL_ARB_sync", major);
        const auto& f = igpu_gl_fns();
        if (f.GetQueryiv)
        {
            f.GetQueryiv(samples_passed, counter_bits, &sample_bits);
            if (query64) f.GetQueryiv(timestamp, counter_bits, &timestamp_bits);
        }
    }

    bool gl_occlusion_supported()
    {
        const auto& f = igpu_gl_fns();
        return common_queries() && f.BeginQuery && f.EndQuery && sample_bits > 0 && sample_bits <= 64 &&
            ((query64 && f.GetQueryObjectui64v) || (sample_bits <= 32 && f.GetQueryObjectuiv));
    }
    bool gl_timestamps_supported()
    {
        const auto& f = igpu_gl_fns();
        return common_queries() && query64 && timestamp_bits > 0 && timestamp_bits <= 64 &&
            f.QueryCounter && f.GetQueryObjectui64v;
    }
    bool gl_fences_supported()
    {
        const auto& f = igpu_gl_fns();
        return sync_supported && f.FenceSync && f.ClientWaitSync && f.DeleteSync;
    }
    std::int64_t gl_timestamp_frequency() { return frequency; }

    std::uint64_t gl_query_create(std::int32_t kind)
    {
        const char* entry = "igpu_query_create";
        if (kind != 0 && kind != 1) { fail(entry, "kind must be IgpuQueryKind.Occlusion or Timestamp"); return 0; }
        if (kind == 0 ? !gl_occlusion_supported() : !gl_timestamps_supported())
        { fail(entry, "this OpenGL context does not support this query kind"); return 0; }
        Query query;
        query.kind = kind;
        if (!allocate_names(kind, query.names, entry)) return 0;
        const auto id = next_query++;
        queries.emplace(id, std::move(query));
        return id;
    }

    bool gl_query_begin(std::uint64_t id)
    {
        const char* entry = "igpu_query_begin";
        auto* query = find_query(id, entry);
        if (!query) return false;
        if (query->begun && !query->ended) return fail(entry, "the query is already open");
        if (query->kind == 0 ? !gl_occlusion_supported() : !gl_timestamps_supported())
            return fail(entry, "this OpenGL context does not support this query kind");
        if (query->kind == 0 && (active_occlusion || current_occlusion_query().name))
            return fail(entry, "another occlusion query is already open");
        if (query->begun)
        {
            std::vector<GLuint> replacement;
            if (!allocate_names(query->kind, replacement, entry)) return false;
            delete_names(*query);
            query->names = std::move(replacement);
        }
        if (query->kind == 0)
        {
            igpu_gl_fns().BeginQuery(samples_passed, query->names.back());
            active_occlusion = id;
        }
        else igpu_gl_fns().QueryCounter(query->names[0], timestamp);
        query->begun = true;
        query->ended = query->collected = false;
        query->value = 0;
        return true;
    }

    bool gl_query_end(std::uint64_t id)
    {
        const char* entry = "igpu_query_end";
        auto* query = find_query(id, entry);
        if (!query) return false;
        if (!query->begun || query->ended) return fail(entry, "begin the query first");
        if (query->kind == 0)
        {
            const auto current = current_occlusion_query();
            if (active_occlusion != id || current.target != samples_passed || current.name != query->names.back())
                return fail(entry, "the active OpenGL query was changed externally");
            igpu_gl_fns().EndQuery(samples_passed);
            active_occlusion = 0;
        }
        else igpu_gl_fns().QueryCounter(query->names[1], timestamp);
        query->ended = true;
        return true;
    }

    bool gl_query_ready(std::uint64_t id)
    {
        auto* query = find_query(id, "igpu_query_ready");
        return query && query->ended && collect(*query, "igpu_query_ready") == Collected::Ready;
    }
    std::int64_t gl_query_result(std::uint64_t id)
    {
        const char* entry = "igpu_query_result";
        auto* query = find_query(id, entry);
        if (!query) return 0;
        if (!query->ended) { fail(entry, "end the query first"); return 0; }
        auto status = collect(*query, entry);
        if (status == Collected::Ready) return query->value;
        if (status == Collected::Error) return 0;
        glFlush();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        do
        {
            Sleep(1);
            status = collect(*query, entry);
            if (status == Collected::Ready) return query->value;
            if (status == Collected::Error) return 0;
        } while (std::chrono::steady_clock::now() < deadline);
        fail(entry, "the result is not ready");
        return 0;
    }
    bool gl_query_release(std::uint64_t id)
    {
        const auto it = queries.find(id);
        if (it == queries.end()) return fail("igpu_query_release", "unknown query handle");
        if (active_occlusion == id)
        {
            const auto current = current_occlusion_query();
            if (current.target == samples_passed && current.name == it->second.names.back())
                igpu_gl_fns().EndQuery(samples_passed);
            active_occlusion = 0;
        }
        delete_names(it->second);
        queries.erase(it);
        return true;
    }

    std::uint64_t gl_fence_create()
    {
        if (!gl_fences_supported())
        { fail("igpu_fence_create", "this OpenGL context does not support sync objects"); return 0; }
        const auto id = next_fence++;
        fences.emplace(id, Fence{});
        return id;
    }
    bool gl_fence_signal(std::uint64_t id)
    {
        const char* entry = "igpu_fence_signal";
        auto* fence = find_fence(id, entry);
        if (!fence) return false;
        if (!gl_fences_supported()) return fail(entry, "this OpenGL context does not support sync objects");
        auto& f = igpu_gl_fns();
        const auto replacement = f.FenceSync(gpu_commands_complete, 0);
        if (!replacement) return fail(entry, "could not allocate an OpenGL sync object");
        if (fence->sync) f.DeleteSync(fence->sync);
        fence->sync = replacement;
        // Submit once at signal time; subsequent signaled calls remain zero-timeout polls.
        glFlush();
        return true;
    }
    bool gl_fence_signaled(std::uint64_t id)
    {
        auto* fence = find_fence(id, "igpu_fence_signaled");
        if (!fence || !fence->sync) return false;
        const auto& f = igpu_gl_fns();
        if (!f.ClientWaitSync) return fail("igpu_fence_signaled", "this OpenGL context does not support sync polling");
        const GLenum result = f.ClientWaitSync(fence->sync, 0, 0);
        if (result == wait_failed) return fail("igpu_fence_signaled", "OpenGL sync polling failed");
        return result == already_signaled || result == condition_satisfied;
    }
    bool gl_fence_release(std::uint64_t id)
    {
        const auto it = fences.find(id);
        if (it == fences.end()) return fail("igpu_fence_release", "unknown fence handle");
        if (it->second.sync) igpu_gl_fns().DeleteSync(it->second.sync);
        fences.erase(it);
        return true;
    }

    GlOcclusionPause::GlOcclusionPause()
    {
        const auto current = current_occlusion_query();
        if (!active_occlusion)
        {
            valid_ = !current.name;
            if (!valid_) fail("gl_shadow_sync", "comparison sampling cannot run inside an external occlusion query");
            return;
        }
        auto& query = queries.at(active_occlusion);
        if (current.target != samples_passed || current.name != query.names.back())
        {
            fail("gl_shadow_sync", "the active OpenGL query was changed externally");
            return;
        }
        auto& f = igpu_gl_fns();
        f.GenQueries(1, &continuation_);
        if (!continuation_)
        {
            fail("gl_shadow_sync", "could not reserve the occlusion continuation");
            return;
        }
        query.names.push_back(continuation_);
        f.EndQuery(samples_passed);
        valid_ = true;
    }
    GlOcclusionPause::~GlOcclusionPause()
    {
        if (continuation_) igpu_gl_fns().BeginQuery(samples_passed, continuation_);
    }

    void gl_queries_release()
    {
        if (active_occlusion) gl_query_release(active_occlusion);
        for (auto& entry : queries) delete_names(entry.second);
        queries.clear();
        for (const auto& entry : fences)
            if (entry.second.sync) igpu_gl_fns().DeleteSync(entry.second.sync);
        fences.clear();
        frequency = 0;
    }
}
