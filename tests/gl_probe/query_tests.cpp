#include "query_tests.h"
#include "IGPU_native.h"
#include "native/igpu_capabilities.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include "native/gl/igpu_gl_shadow.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using Handle = std::int64_t;
    constexpr GLenum samples_passed = 0x8914, current_query = 0x8865;
    constexpr GLenum result_available = 0x8867, query_result = 0x8866;
    constexpr GLenum array_buffer = 0x8892, element_buffer = 0x8893;
    constexpr GLenum pack_buffer = 0x88EB, unpack_buffer = 0x88EC;
    constexpr GLenum draw_framebuffer = 0x8CA9, read_framebuffer = 0x8CA8;
    constexpr std::int64_t nanoseconds_per_second = 1000000000;

    bool check(bool ok, const char* label)
    {
        if (!ok) std::fprintf(stderr, "query tests: %s: %s\n", label, igpu_get_last_error().c_str());
        return ok;
    }
    bool accepted(bool ok, const char* label)
    {
        return check(ok && igpu_get_last_error().empty(), label);
    }
    bool rejected(bool ok, const char* label)
    {
        return check(!ok && !igpu_get_last_error().empty(), label);
    }
    template<class T> struct Replace
    {
        T& slot;
        T previous;
        Replace(T& field, T value) : slot(field), previous(field) { slot = value; }
        ~Replace() { slot = previous; }
        Replace(const Replace&) = delete;
        Replace& operator=(const Replace&) = delete;
    };
    template<class Poll> bool wait_for(Poll poll, const char* label)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        for (unsigned attempt = 0; attempt < 2000; ++attempt)
        {
            const bool ready = poll();
            if (!check(igpu_get_last_error().empty(), label)) return false;
            if (ready) return true;
            if (std::chrono::steady_clock::now() >= deadline) break;
            Sleep(1);
        }
        std::fprintf(stderr, "query tests: %s: readiness deadline exceeded (2 seconds)\n", label);
        return false;
    }
    struct Query
    {
        Handle id = 0;
        bool open = false;
        explicit Query(int kind) : id(igpu_query_create(kind)) {}
        ~Query()
        {
            if (id) { if (open) igpu_query_end(id); igpu_query_release(id); }
        }
        bool begin() { const bool ok = igpu_query_begin(id); if (ok) open = true; return ok; }
        bool end() { const bool ok = igpu_query_end(id); if (ok) open = false; return ok; }
        bool release()
        {
            const bool ok = igpu_query_release(id);
            if (ok) { id = 0; open = false; }
            return ok;
        }
        Query(const Query&) = delete;
        Query& operator=(const Query&) = delete;
    };
    struct Fence
    {
        Handle id = igpu_fence_create();
        ~Fence() { if (id) igpu_fence_release(id); }
        bool release() { const bool ok = igpu_fence_release(id); if (ok) id = 0; return ok; }
        Fence() = default;
        Fence(const Fence&) = delete;
        Fence& operator=(const Fence&) = delete;
    };
    bool not_ready(Handle id, const char* label)
    {
        const bool ready = igpu_query_ready(id);
        return check(!ready && igpu_get_last_error().empty(), label);
    }
    bool result_rejected(Handle id, const char* label)
    {
        igpu_query_result(id);
        return check(!igpu_get_last_error().empty(), label);
    }
    bool collect(Handle id, std::int64_t& value)
    {
        // Query polling needs submitted work. Fence polling deliberately does not flush here.
        glFlush();
        if (!wait_for([&] { return igpu_query_ready(id); }, "query readiness")) return false;
        value = igpu_query_result(id);
        return check(igpu_get_last_error().empty(), "collect completed query");
    }
    bool count_is(Query& query, std::int64_t expected, const char* label)
    {
        std::int64_t actual = -1;
        if (!collect(query.id, actual)) return false;
        if (actual != expected)
            std::fprintf(stderr, "query tests: %s: got %lld samples, expected %lld\n", label,
                         static_cast<long long>(actual), static_cast<long long>(expected));
        return check(actual == expected, label);
    }
    GLint active_samples()
    {
        GLint id = 0;
        igpu_gl_fns().GetQueryiv(samples_passed, current_query, &id);
        return id;
    }

    constexpr GLenum binding_fields[] = {0x8B8D, 0x85B5, 0x8894, 0x8895, 0x8CA6, 0x8CAA,
                                         GL_TEXTURE_BINDING_2D, 0x88ED, 0x88EF, 0x84E0};
    constexpr GLenum pixel_fields[] = {GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS,
        GL_PACK_SKIP_PIXELS, GL_PACK_SWAP_BYTES, GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH,
        GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SWAP_BYTES};
    constexpr GLenum enable_fields[] = {GL_DEPTH_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_STENCIL_TEST,
        GL_BLEND, GL_ALPHA_TEST, 0x8C89, 0x809D, GL_CLIP_PLANE0};
    struct State
    {
        std::array<GLint, 10> bindings{}, pixels{};
        std::array<GLint, 4> viewport{}, scissor{};
        std::array<GLint, 2> polygon{};
        std::array<GLboolean, 9> enabled{};
        std::array<GLboolean, 4> color_mask{};
        std::array<GLdouble, 2> depth_range{};
        GLint depth_func = 0;
        GLboolean depth_mask = GL_FALSE;
        State()
        {
            for (std::size_t i = 0; i < bindings.size(); ++i) glGetIntegerv(binding_fields[i], &bindings[i]);
            for (std::size_t i = 0; i < pixels.size(); ++i) glGetIntegerv(pixel_fields[i], &pixels[i]);
            for (std::size_t i = 0; i < enabled.size(); ++i) enabled[i] = glIsEnabled(enable_fields[i]);
            glGetIntegerv(GL_VIEWPORT, viewport.data()); glGetIntegerv(GL_SCISSOR_BOX, scissor.data());
            glGetIntegerv(GL_POLYGON_MODE, polygon.data()); glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask); glGetBooleanv(GL_COLOR_WRITEMASK, color_mask.data());
            glGetDoublev(GL_DEPTH_RANGE, depth_range.data());
        }
        bool operator==(const State& b) const
        {
            for (std::size_t i = 0; i < bindings.size(); ++i)
                if (bindings[i] != b.bindings[i])
                    std::fprintf(stderr, "query state: binding 0x%x changed %d -> %d\n",
                                 binding_fields[i], bindings[i], b.bindings[i]);
            return bindings == b.bindings && pixels == b.pixels && viewport == b.viewport &&
                scissor == b.scissor && polygon == b.polygon && enabled == b.enabled &&
                color_mask == b.color_mask && depth_range == b.depth_range &&
                depth_func == b.depth_func && depth_mask == b.depth_mask;
        }
    };
    struct RestoreState
    {
        State saved;
        RestoreState() { glPushAttrib(GL_ALL_ATTRIB_BITS); }
        ~RestoreState()
        {
            glPopAttrib();
            auto& f = igpu_gl_fns();
            f.UseProgram(saved.bindings[0]); f.BindVertexArray(saved.bindings[1]);
            f.BindBuffer(array_buffer, saved.bindings[2]); f.BindBuffer(element_buffer, saved.bindings[3]);
            f.BindFramebuffer(draw_framebuffer, saved.bindings[4]);
            f.BindFramebuffer(read_framebuffer, saved.bindings[5]);
            glBindTexture(GL_TEXTURE_2D, saved.bindings[6]);
            f.BindBuffer(pack_buffer, saved.bindings[7]); f.BindBuffer(unpack_buffer, saved.bindings[8]);
            for (std::size_t i = 0; i < saved.pixels.size(); ++i) glPixelStorei(pixel_fields[i], saved.pixels[i]);
        }
    };
    struct Target
    {
        std::int32_t framebuffer = 0, viewport[4]{};
        GLint read = 0;
        bool valid = false;
        explicit Target(Handle texture)
        {
            glGetIntegerv(0x8CAA, &read);
            valid = igpu::gl_color_target_begin(texture, framebuffer, viewport);
        }
        ~Target()
        {
            if (valid) { igpu::gl_color_target_end(framebuffer, viewport); igpu_gl_fns().BindFramebuffer(read_framebuffer, read); }
        }
    };
    std::array<std::byte, 7> wire(std::int32_t value)
    {
        std::array<std::byte, 7> bytes{}; bytes[0] = std::byte{1}; bytes[2] = std::byte{6};
        std::memcpy(bytes.data() + 3, &value, sizeof(value)); return bytes;
    }
    gm::wire::GMArrayView view(const std::array<std::byte, 7>& bytes)
    {
        return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array, bytes.data()));
    }
    struct Drawing
    {
        Handle shader = 0, layout = 0, vertex = 0, index = 0, target = 0, source = 0;
        bool owns_shader = false;
        GLuint program = 0, sentinel_vao = 0, sentinel_buffers[2]{};
        ~Drawing()
        {
            auto& f = igpu_gl_fns();
            if (program) f.DeleteProgram(program);
            if (sentinel_vao) f.DeleteVertexArrays(1, &sentinel_vao);
            f.DeleteBuffers(2, sentinel_buffers);
            if (layout) igpu_input_layout_release(layout);
            if (vertex) igpu_buffer_release(vertex);
            if (index) igpu_buffer_release(index);
            if (target) igpu_texture_release(target);
            if (source) igpu_texture_release(source);
            if (shader && owns_shader) igpu_shader_release(shader);
        }
        bool initialize()
        {
            const char* sources[] = {
                "#version 120\nattribute vec2 in_pos; void main(){gl_Position=vec4(in_pos,0,1);}",
                "#version 120\nuniform sampler2D igpu_tex; uniform int sampled;\n"
                "void main(){gl_FragColor=sampled==1?texture2D(igpu_tex,vec2(0.5)):vec4(1,0,0,1);}"};
            shader = igpu_get_bound_shader(0);
            if (!shader) { shader = igpu_shader_compile(sources[0], "main", 0, "glsl"); owns_shader = true; }
            const auto usage = wire(1), type = wire(2), step = wire(0);
            layout = igpu_input_layout_create(shader, view(usage), view(type), view(step), 1, 8, 0);
            vertex = igpu_buffer_create(24, 1, 1, 8); index = igpu_buffer_create(6, 1, 2, 0);
            float positions[] = {-1,-1, 3,-1, -1,3};
            std::uint16_t indices[] = {0,1,2};
            if (!check(shader && layout && vertex && index, "draw resources") ||
                !accepted(igpu_buffer_write(vertex, 0, gm::wire::GMBuffer(positions, sizeof(positions))), "vertex data") ||
                !accepted(igpu_buffer_write(index, 0, gm::wire::GMBuffer(indices, sizeof(indices))), "index data")) return false;
            target = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
            source = igpu_texture_create(0, 2, 2, 1, 6, true, false, 1);
            auto& f = igpu_gl_fns(); program = f.CreateProgram();
            if (!check(target && source && program, "8x8 draw target/program")) return false;
            for (int stage = 0; stage < 2; ++stage)
            {
                const GLuint object = f.CreateShader(stage ? 0x8B30 : 0x8B31);
                if (!check(object != 0, "raw shader creation")) return false;
                f.ShaderSource(object, 1, &sources[stage], nullptr); f.CompileShader(object);
                GLint compiled = 0; f.GetShaderiv(object, 0x8B81, &compiled);
                if (compiled) f.AttachShader(program, object);
                else { char log[512]{}; f.GetShaderInfoLog(object, sizeof(log), nullptr, log); std::fprintf(stderr, "query shader: %s\n", log); }
                f.DeleteShader(object);
                if (!check(compiled != 0, "raw shader compilation")) return false;
            }
            f.BindAttribLocation(program, 0, "in_pos"); f.LinkProgram(program);
            GLint linked = 0; f.GetProgramiv(program, 0x8B82, &linked);
            if (!check(linked != 0, "raw program link")) return false;
            f.GenVertexArrays(1, &sentinel_vao); f.GenBuffers(2, sentinel_buffers);
            for (int i = 0; i < 2; ++i)
            { f.BindBuffer(i ? unpack_buffer : pack_buffer, sentinel_buffers[i]); f.BufferData(i ? unpack_buffer : pack_buffer, 512, nullptr, 0x88E0); }
            RestoreState state; Target output(source);
            if (!check(output.valid, "sampled source target")) return false;
            glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClearColor(1,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            return check(glGetError() == GL_NO_ERROR, "draw fixture GL errors");
        }
        bool draw(int path = 0)
        {
            RestoreState state; Target output(target);
            if (!check(output.valid, "draw target")) return false;
            for (const auto capability : enable_fields) glDisable(capability);
            GLint clips = 0; glGetIntegerv(0x0D32, &clips);
            for (GLint i = 0; i < clips; ++i) glDisable(GL_CLIP_PLANE0 + i);
            glDisable(GL_POLYGON_SMOOTH); glDisable(GL_POLYGON_STIPPLE); glDisable(GL_COLOR_LOGIC_OP);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            auto& f = igpu_gl_fns(); f.UseProgram(program);
            f.Uniform1i(f.GetUniformLocation(program, "sampled"), path == 2 ? 1 : 0);
            const bool drew = path == 1 ? igpu_draw_indexed(vertex, layout, index, 4, 0, 3, 0,0,0,0) :
                path == 2 ? igpu_draw_sampled(vertex, layout, 4, 0, 3, source, 0,0,0,0) :
                            igpu_draw(vertex, 0, layout, 4, 0, 3, 1, 0,0,0,0);
            if (!accepted(drew, "public fullscreen draw")) return false;
            f.BindBuffer(pack_buffer, 0);
            for (int i = 0; i < 5; ++i) glPixelStorei(pixel_fields[i], i ? 0 : 1);
            std::array<unsigned char, 8 * 8 * 4> image{};
            glReadPixels(0,0,8,8,GL_RGBA,GL_UNSIGNED_BYTE,image.data());
            for (std::size_t i = 0; i < image.size(); i += 4)
                if (!check(image[i] == 255 && image[i+1] == 0 && image[i+2] == 0 && image[i+3] == 255,
                           "all 64 pixels must be opaque red")) return false;
            return check(glGetError() == GL_NO_ERROR, "public draw GL errors");
        }
        void sentinels()
        {
            auto& f = igpu_gl_fns(); f.UseProgram(program); f.BindVertexArray(sentinel_vao);
            f.BindBuffer(pack_buffer, sentinel_buffers[0]); f.BindBuffer(unpack_buffer, sentinel_buffers[1]);
            glBindTexture(GL_TEXTURE_2D, 0);
            glViewport(2,3,5,6); glScissor(1,2,3,4);
            glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glEnable(GL_SCISSOR_TEST);
            glEnable(GL_STENCIL_TEST); glEnable(GL_CLIP_PLANE0);
            glDepthFunc(GL_GREATER); glDepthMask(GL_FALSE); glDepthRange(.25,.75);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glColorMask(GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);
        }
    };
    struct Shadow
    {
        igpu::GlShadow value;
        ~Shadow() { igpu::gl_shadow_release(value); }
    };
    struct FloatSource
    {
        GLuint name = 0;
        ~FloatSource() { if (name) glDeleteTextures(1, &name); }
        bool initialize()
        {
            RestoreState restore;
            auto& f = igpu_gl_fns(); f.BindBuffer(unpack_buffer, 0);
            for (int i = 5; i < 10; ++i) glPixelStorei(pixel_fields[i], i == 5 ? 1 : 0);
            glGenTextures(1, &name); glBindTexture(GL_TEXTURE_2D, name);
            for (int mip = 0; mip < 4; ++mip)
            {
                const int edge = 8 >> mip;
                std::vector<float> data(edge * edge);
                for (std::size_t i = 0; i < data.size(); ++i) data[i] = i % 2 ? -.25f : .75f;
                glTexImage2D(GL_TEXTURE_2D, mip, 0x822E, edge, edge, 0, GL_RED, GL_FLOAT, data.data());
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, 0x813C, 1); glTexParameteri(GL_TEXTURE_2D, 0x813D, 2);
            glTexParameteri(GL_TEXTURE_2D, 0x8E42, GL_ONE);
            return check(name && glGetError() == GL_NO_ERROR, "four-mip R32F source");
        }
        std::array<GLint, 3> parameters() const
        {
            GLint bound = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound); glBindTexture(GL_TEXTURE_2D, name);
            std::array<GLint, 3> values{};
            const GLenum fields[] = {0x813C, 0x813D, 0x8E42};
            for (int i = 0; i < 3; ++i) glGetTexParameteriv(GL_TEXTURE_2D, fields[i], &values[i]);
            glBindTexture(GL_TEXTURE_2D, bound); return values;
        }
    };
    bool sync_preserves(FloatSource& source, Shadow& shadow, Query& query)
    {
        const State before; const auto parameters = source.parameters();
        if (!accepted(igpu::gl_shadow_sync(source.name,8,8,4,shadow.value), "shadow sync")) return false;
        return check(before == State{} && parameters == source.parameters(), "shadow caller state/source parameters") &&
            check(active_samples() != 0, "public query resumed after shadow") &&
            not_ready(query.id, "public query remains open after shadow") &&
            check(glGetError() == GL_NO_ERROR, "shadow GL errors");
    }

    void APIENTRY no_queries(GLsizei count, GLuint* names)
    { for (GLsizei i = 0; i < count; ++i) names[i] = 0; }
    GLuint APIENTRY no_program() { return 0; }
    IgpuGlSync APIENTRY no_fence(GLenum, GLbitfield) { return nullptr; }
    GLenum APIENTRY bad_wait(IgpuGlSync, GLbitfield, std::uint64_t) { return 0x911D; }

    bool lifecycle_and_pixels(Drawing& drawing)
    {
        Query query(0), competing(0);
        if (!accepted(query.id && competing.id, "occlusion creation") ||
            !not_ready(query.id, "new query readiness") ||
            !rejected(query.end(), "end before begin") || !result_rejected(query.id, "result before begin") ||
            !accepted(query.begin(), "begin") || !not_ready(query.id, "open query readiness")) return false;
        const auto held = active_samples();
        if (!rejected(query.begin(), "duplicate begin") || !rejected(competing.begin(), "competing begin") ||
            !check(active_samples() == held, "invalid begin preserves active query") ||
            !result_rejected(query.id, "open result") || !accepted(query.end(), "empty end") ||
            !count_is(query, 0, "empty interval") || !rejected(query.end(), "duplicate end")) return false;
        for (int path = 0; path < 3; ++path)
            if (!accepted(query.begin(), "reuse begin") || !drawing.draw(path) ||
                !accepted(query.end(), "reuse end") || !count_is(query, 64, "one full-screen public draw") ||
                !check(igpu_query_result(query.id) == 64 && igpu_get_last_error().empty(), "cached repeat result")) return false;
        if (!accepted(query.begin(), "multiple draw begin")) return false;
        for (int path = 0; path < 3; ++path) if (!drawing.draw(path)) return false;
        if (!accepted(query.end(), "multiple draw end") || !count_is(query, 192, "three public draw paths") ||
            !accepted(query.begin(), "release open begin")) return false;
        const auto old = query.id;
        if (!accepted(query.release(), "release open query") || !check(active_samples() == 0, "release ends owned query") ||
            !accepted(competing.begin(), "competing query usable after release") || !accepted(competing.end(), "competitor end") ||
            !count_is(competing, 0, "competitor result")) return false;
        const std::uint64_t invalid[] = {0, static_cast<std::uint64_t>(old), (std::numeric_limits<std::uint64_t>::max)()};
        for (const auto id : invalid)
            if (!rejected(igpu_query_begin(id), "unknown begin") || !rejected(igpu_query_end(id), "unknown end") ||
                !rejected(igpu_query_ready(id), "unknown ready") || !result_rejected(id, "unknown result") ||
                !rejected(igpu_query_release(id), "unknown release")) return false;
        for (const auto kind : {-1, 2, 100})
            if (!rejected(igpu_query_create(kind) != 0, "invalid kind")) return false;
        std::printf("query samples: 0/64/192 counts, reuse and three public draw paths; lifecycle/invalid handles passed\n");
        return true;
    }
    bool timestamps_and_fences(Drawing& drawing)
    {
        if (!check(igpu_timestamp_frequency() == 0, "frequency before first completed interval")) return false;
        Query first(1), second(1);
        if (!accepted(first.id && second.id, "timestamp creation") || !accepted(first.begin(), "timestamp first begin") ||
            !not_ready(first.id, "open timestamp readiness") || !result_rejected(first.id, "open timestamp result") ||
            !accepted(second.begin(), "overlapping timestamp begin") || !drawing.draw() ||
            !accepted(first.end(), "overlapping first end") || !accepted(second.end(), "overlapping second end")) return false;
        std::int64_t a = 0, b = 0;
        if (!collect(first.id, a) || !collect(second.id, b) || !check(a > 0 && b > 0, "positive real GPU intervals") ||
            !check(igpu_timestamp_frequency() == nanoseconds_per_second, "nanosecond frequency") ||
            !accepted(first.begin(), "timestamp reuse") || !drawing.draw() || !accepted(first.end(), "timestamp reuse end") ||
            !collect(first.id, a) || !check(a > 0, "reused interval") ||
            !accepted(second.begin(), "timestamp release-open begin") || !accepted(second.release(), "timestamp release open")) return false;
        Fence fence, unsignaled;
        if (!accepted(fence.id && unsignaled.id, "fence creation") ||
            !check(!igpu_fence_signaled(fence.id) && igpu_get_last_error().empty(), "new fence unsignaled") ||
            !accepted(igpu_fence_signal(fence.id), "fence signal") ||
            !wait_for([&]{ return igpu_fence_signaled(fence.id); }, "fence completion") ||
            !drawing.draw() || !accepted(igpu_fence_signal(fence.id), "fence signal again") ||
            !wait_for([&]{ return igpu_fence_signaled(fence.id); }, "repeated fence completion")) return false;
        auto& f = igpu_gl_fns();
        { Replace<IgpuGlFenceSync> missing(f.FenceSync, no_fence);
          if (!rejected(igpu_fence_signal(fence.id), "failed replacement")) return false; }
        if (!accepted(igpu_fence_signaled(fence.id), "failed replacement retained old real sync")) return false;
        { Replace<IgpuGlClientWaitSync> broken(f.ClientWaitSync, bad_wait);
          if (!rejected(igpu_fence_signaled(fence.id), "driver wait error")) return false; }
        const auto old = fence.id;
        if (!accepted(fence.release(), "release signaled") || !accepted(unsignaled.release(), "release unsignaled")) return false;
        for (const std::uint64_t id : {std::uint64_t{0}, static_cast<std::uint64_t>(old), (std::numeric_limits<std::uint64_t>::max)()})
            if (!rejected(igpu_fence_signal(id), "unknown fence signal") ||
                !rejected(igpu_fence_signaled(id), "unknown fence poll") ||
                !rejected(igpu_fence_release(id), "unknown fence release")) return false;
        std::printf("query timing: overlapping/reused positive GPU intervals; fences signal/re-signal/poll/failure/release passed\n");
        return true;
    }
    bool shadow_counts(Drawing& drawing)
    {
        FloatSource source; Shadow shadow;
        if (!source.initialize()) return false;
        drawing.sentinels();
        Query query(0);
        if (!accepted(query.begin(), "shadow-only query begin") || !sync_preserves(source, shadow, query) ||
            !sync_preserves(source, shadow, query) || !accepted(query.end(), "shadow-only end") ||
            !count_is(query, 0, "private mip copies count zero") || !accepted(query.begin(), "mixed query begin") ||
            !drawing.draw() || !sync_preserves(source, shadow, query) || !drawing.draw(1) ||
            !sync_preserves(source, shadow, query) || !accepted(query.end(), "mixed end") ||
            !count_is(query, 128, "two real draws with hidden copies")) return false;
        auto& f = igpu_gl_fns();
        if (!accepted(query.begin(), "failure query begin")) return false;
        const State before; const auto held = active_samples();
        { Replace<IgpuGlGenQueries> missing(f.GenQueries, no_queries);
          if (!rejected(igpu::gl_shadow_sync(source.name,8,8,4,shadow.value), "continuation allocation failure") ||
              !check(active_samples() == held && before == State{}, "reservation failure preserves active/state")) return false; }
        { Shadow fresh; Replace<IgpuGlCreateProgram> missing(f.CreateProgram, no_program);
          if (!rejected(igpu::gl_shadow_sync(source.name,8,8,4,fresh.value), "copy program failure") ||
              !check(active_samples() != 0 && before == State{}, "copy failure resumes query and restores state")) return false; }
        if (!drawing.draw() || !accepted(query.end(), "failed-copy query end") ||
            !count_is(query, 64, "real draw after private-copy errors")) return false;
        GLuint raw = 0; f.GenQueries(1, &raw); f.BeginQuery(samples_passed, raw);
        const State external;
        const bool preserved = rejected(query.begin(), "external query begin collision") &&
            rejected(igpu::gl_shadow_sync(source.name,8,8,4,shadow.value), "external query comparison collision") &&
            active_samples() == static_cast<GLint>(raw) && external == State{};
        f.EndQuery(samples_passed); glFinish();
        std::uint64_t raw_result = 1; f.GetQueryObjectui64v(raw, query_result, &raw_result); f.DeleteQueries(1, &raw);
        if (!check(preserved && raw_result == 0 && glGetError() == GL_NO_ERROR, "external query preserved without hidden draws")) return false;
        std::printf("query shadow: four-mip hidden draws excluded, repeated segmentation and allocation/program-failure resume passed\n");
        return true;
    }
    bool capabilities()
    {
        using C = igpu::Capability;
        auto& f = igpu_gl_fns();
        if (!check(igpu::supports(C::Queries) && igpu::supports(C::OcclusionQuery) &&
            igpu::supports(C::Timestamps) && igpu::supports(C::Fence), "supported capability families")) return false;
        { Replace<IgpuGlQueryCounter> missing(f.QueryCounter, nullptr);
          if (!check(!igpu::supports(C::Timestamps) && igpu::supports(C::OcclusionQuery) && igpu::supports(C::Queries),
              "missing timestamp entry gates only timestamps") || !rejected(igpu_query_create(1) != 0, "missing timestamp creation")) return false; }
        { Replace<IgpuGlClientWaitSync> missing(f.ClientWaitSync, nullptr);
          if (!check(!igpu::supports(C::Fence) && igpu::supports(C::OcclusionQuery), "missing sync entry gates only fences") ||
              !rejected(igpu_fence_create() != 0, "missing fence creation")) return false; }
        return true;
    }
}

bool run_gl_query_tests()
{
    // Older fixtures delete their last raw program while it is still current.
    // Drop that deferred deletion before saving a program we must restore later.
    auto& f = igpu_gl_fns();
    GLint program = 0, deleted = 0;
    glGetIntegerv(0x8B8D, &program);
    if (program) f.GetProgramiv(program, 0x8B80 /* DELETE_STATUS */, &deleted);
    if (deleted) f.UseProgram(0);
    bool passed = false;
    {
        RestoreState restore;
        Drawing drawing;
        passed = capabilities() && drawing.initialize() && lifecycle_and_pixels(drawing) &&
            timestamps_and_fences(drawing) && shadow_counts(drawing);
    }
    return passed && check(glGetError() == GL_NO_ERROR, "query fixture cleanup GL errors");
}
