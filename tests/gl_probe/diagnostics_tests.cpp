#include "diagnostics_tests.h"
#include "raster_stencil_tests.h"
#include "vertex_layout_tests.h"
#include "IGPU_native.h"
#include "native/igpu_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include "native/gl/igpu_gl_draw.h"
#include <array>
#include <vector>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
    using Handle = std::uint64_t;
    constexpr GLenum array_buffer = 0x8892, element_buffer = 0x8893, indirect_buffer = 0x8F3F;
    bool check(bool ok, const char* label)
    {
        if (!ok) std::fprintf(stderr, "draw diagnostics: %s: %s\n", label, igpu_get_last_error().c_str());
        return ok;
    }
    struct Array
    {
        std::vector<std::byte> bytes;
        explicit Array(std::initializer_list<Handle> values) : bytes(2 + 9 * values.size())
        {
            const auto count = static_cast<std::uint16_t>(values.size());
            std::memcpy(bytes.data(), &count, 2);
            std::size_t i = 0;
            for (auto value : values)
            {
                bytes[2 + 9 * i] = std::byte{12};
                std::memcpy(bytes.data() + 3 + 9 * i++, &value, 8);
            }
        }
        gm::wire::GMArrayView view() const
        { return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array, bytes.data())); }
    };
    struct Bindings
    {
        static constexpr GLenum fields[] = {0x8B8D, 0x85B5, 0x8894, 0x8895, 0x8F43, 0x8CA6, 0x8CAA};
        std::array<GLint, 7> values{};
        Bindings() { for (int i = 0; i < 7; ++i) glGetIntegerv(fields[i], &values[i]); }
        bool operator==(const Bindings& other) const { return values == other.values; }
        void restore() const
        {
            auto& f = igpu_gl_fns();
            f.UseProgram(values[0]); f.BindVertexArray(values[1]);
            f.BindBuffer(array_buffer, values[2]); f.BindBuffer(element_buffer, values[3]);
            f.BindBuffer(indirect_buffer, values[4]);
            f.BindFramebuffer(0x8CA9, values[5]); f.BindFramebuffer(0x8CA8, values[6]);
        }
    };
    IgpuGlGenBuffers original_gen = nullptr;
    GLuint generated_buffer = 0;
    void APIENTRY capture_buffer(GLsizei count, GLuint* buffers)
    { original_gen(count, buffers); if (count) generated_buffer = buffers[0]; }
    IgpuGlGenVertexArrays original_gen_vao = nullptr;
    GLuint generated_vao = 0;
    void APIENTRY capture_vao(GLsizei count, GLuint* arrays)
    { original_gen_vao(count, arrays); if (count) generated_vao = arrays[0]; }
    IgpuGlBindVertexArray original_bind_vao = nullptr;
    GLuint dropped_vao = 0;
    void APIENTRY drop_vao_restore(GLuint vao)
    { if (vao != dropped_vao) original_bind_vao(vao); }
    int vao_fault = 0;
    void APIENTRY corrupt_vao_restore(GLuint vao)
    {
        original_bind_vao(vao);
        if (vao == dropped_vao)
        {
            auto& f = igpu_gl_fns();
            const GLuint attribute = vao_fault > 2 ? 5 : 1;
            if (vao_fault == 1 || vao_fault == 3) f.VertexAttribDivisor(attribute, 0);
            else f.VertexAttribPointer(attribute, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
        }
    }
    IgpuGlBindBuffer original_bind_buffer = nullptr;
    GLuint dropped_indirect = 0;
    void APIENTRY drop_indirect_restore(GLenum target, GLuint buffer)
    { if (target != indirect_buffer || buffer != dropped_indirect) original_bind_buffer(target, buffer); }
    struct Fixture
    {
        Bindings saved;
        std::array<Handle, 6> old_stages{};
        std::vector<Handle> shaders;
        Handle vertex = 0, index = 0, args = 0, layout = 0, target = 0, second = 0, source = 0, comparison_source = 0, compare_state = 0;
        GLuint native_vertex = 0, vao = 0, foreign = 0;
        GLint saved_viewport[4]{};
        std::int32_t previous_target = 0, target_viewport[4]{};
        bool target_active = false;
        Fixture() { glPushAttrib(GL_ALL_ATTRIB_BITS); }
        ~Fixture()
        {
            auto& f = igpu_gl_fns();
            if (target_active) igpu::gl_color_target_end(previous_target, target_viewport);
            igpu_shader_bind(old_stages[2], 2);
            for (int stage : {3, 4, 5, 0, 1}) igpu_shader_bind(old_stages[stage], stage);
            for (auto shader : shaders) igpu_shader_release(shader);
            if (layout) igpu_input_layout_release(layout);
            for (auto buffer : {vertex, index, args}) if (buffer) igpu_buffer_release(buffer);
            if (compare_state) igpu_state_release(compare_state);
            for (auto texture : {target, second, source, comparison_source}) if (texture) igpu_texture_release(texture);
            using IsProgram = GLboolean(APIENTRY*)(GLuint);
            const auto exists = reinterpret_cast<IsProgram>(wglGetProcAddress("glIsProgram"));
            if (saved.values[0] && !exists(saved.values[0])) glGetIntegerv(0x8B8D, &saved.values[0]);
            saved.restore(); glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
            if (vao) f.DeleteVertexArrays(1, &vao);
            if (foreign) f.DeleteBuffers(1, &foreign);
            glPopAttrib();
        }
        Handle shader(const char* code, int stage)
        {
            const auto h = static_cast<Handle>(igpu_shader_compile(code, "main", stage, "glsl"));
            if (h) shaders.push_back(h);
            return h;
        }
        bool initialize()
        {
            for (int i = 0; i < 6; ++i) old_stages[i] = igpu_get_bound_shader(i);
            glGetIntegerv(GL_VIEWPORT, saved_viewport);
            for (auto capability : {GL_CULL_FACE, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_SCISSOR_TEST, GL_BLEND}) glDisable(capability);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            const auto vs = shader("#version 430\nlayout(location=0)in vec2 p;out vec2 uv;void main(){gl_Position=vec4(p,0,1);uv=p*.5+.5;}", 0);
            const auto ps = shader("#version 430\nlayout(location=0)out vec4 c;layout(location=1)out vec4 d;void main(){c=vec4(0,1,0,1);d=vec4(0,0,1,1);}", 1);
            if (!check(vs && ps && igpu_shader_bind(vs, 0) && igpu_shader_bind(ps, 1), "public graphics pipeline")) return false;
            auto& f = igpu_gl_fns(); original_gen = f.GenBuffers; f.GenBuffers = capture_buffer;
            vertex = igpu_buffer_create(24, 1, 1, 8); f.GenBuffers = original_gen; native_vertex = generated_buffer;
            index = igpu_buffer_create(6, 1, 2, 0); args = igpu_buffer_create(64, 0, 16, 0);
            Array usage{1}, type{2}, step{0};
            layout = igpu_input_layout_create(vs, usage.view(), type.view(), step.view(), 1, 8, 0);
            target = igpu_texture_create(0, 4, 4, 1, 6, true, false, 1);
            second = igpu_texture_create(0, 4, 4, 1, 6, true, false, 1);
            source = igpu_texture_create(0, 4, 4, 1, 6, true, false, 1);
            std::array<float, 6> positions{-1, -1, 3, -1, -1, 3};
            std::array<std::uint16_t, 3> indices{0, 1, 2};
            std::array<std::uint32_t, 16> commands{3, 1, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0, 0};
            if (!check(vertex && index && args && layout && target && second && source &&
                igpu_buffer_write(vertex, 0, gm::wire::GMBuffer(positions.data(), sizeof(positions))) &&
                igpu_buffer_write(index, 0, gm::wire::GMBuffer(indices.data(), sizeof(indices))) &&
                igpu_buffer_write(args, 0, gm::wire::GMBuffer(commands.data(), sizeof(commands))), "resource allocation/upload")) return false;
            f.GenVertexArrays(1, &vao); f.GenBuffers(1, &foreign); f.BindVertexArray(vao);
            f.BindBuffer(array_buffer, native_vertex); f.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
            f.DisableVertexAttribArray(0);
            f.BindBuffer(array_buffer, foreign); std::array<unsigned char, 64> bytes{};
            f.BufferData(array_buffer, bytes.size(), bytes.data(), 0x88E4);
            f.BindBuffer(element_buffer, foreign); f.BindBuffer(indirect_buffer, foreign);
            f.VertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4, reinterpret_cast<void*>(16));
            f.DisableVertexAttribArray(1);
            f.VertexAttribDivisor(0, 7); f.VertexAttribDivisor(1, 3);
            return check(glGetError() == GL_NO_ERROR, "native caller sentinel");
        }
        bool direct(std::int64_t count = 3)
        { return igpu_draw(vertex, 0, layout, 4, 0, count, 1, 0, 0, 0, 0); }
        template<class Submit> bool accepted(Submit submit, const char* label, int pixel = 65280, int samples = 16)
        {
            const auto count = igpu::draw_count(), failures = igpu::draw_restore_failures();
            const Bindings before;
            const auto query = igpu_query_create(0);
            const bool began = query && igpu_query_begin(query);
            const bool ok = began && submit();
            const bool ended = began && igpu_query_end(query);
            const auto actual_samples = ended ? igpu_query_result(query) : -1;
            if (query) igpu_query_release(query);
            const auto counted = igpu::draw_count() - count, lost = igpu::draw_restore_failures() - failures;
            const bool restored = Bindings() == before;
            const auto native_error = glGetError();
            const auto actual_pixel = igpu_texture_read(target, 1, 1, 0, 0);
            const bool passed = ok && ended && actual_samples == samples && counted == 1 && lost == 0 && restored && native_error == GL_NO_ERROR && actual_pixel == pixel;
            if (!passed) std::fprintf(stderr, "draw diagnostics observed: %s accepted=%d query=%d samples=%lld/%d counts=%d losses=%d restored=%d error=%x pixel=%lld/%d\n",
                label, ok, ended, static_cast<long long>(actual_samples), samples, counted, lost, restored, native_error, static_cast<long long>(actual_pixel), pixel);
            return check(passed, label);
        }
    };
}

bool run_gl_diagnostics_tests()
{
    Fixture x;
    if (!x.initialize()) return false;
    auto& f = igpu_gl_fns();
    const Bindings before;
    if (!check(igpu::is_vertex_buffer_bound(x.vertex) && Bindings() == before, "attribute0 source is bound despite another generic ARRAY_BUFFER")) return false;
    f.BindVertexArray(0); glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    f.BindBuffer(array_buffer, x.native_vertex); f.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    f.BindBuffer(array_buffer, x.foreign);
    const bool compatibility_default = igpu::is_vertex_buffer_bound(x.vertex);
    const auto compatibility_error = glGetError();
    glPopClientAttrib(); f.BindVertexArray(x.vao); f.BindBuffer(array_buffer, x.foreign);
    if (!check(compatibility_default && compatibility_error == GL_NO_ERROR && Bindings() == before,
        "compatibility default VAO0 keeps its real source query")) return false;
    f.BindBuffer(array_buffer, x.foreign); f.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    f.BindBuffer(array_buffer, x.native_vertex);
    if (!check(!igpu::is_vertex_buffer_bound(x.vertex), "generic binding alone is not a vertex source")) return false;
    f.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr); f.BindBuffer(array_buffer, x.foreign);
    const auto released = igpu_buffer_create(8, 1, 1, 8);
    if (!check(released && igpu_buffer_release(released) && !igpu::is_vertex_buffer_bound(released) &&
        !igpu::is_vertex_buffer_bound(x.index) && !igpu::is_vertex_buffer_bound(0) &&
        !igpu::is_vertex_buffer_bound((std::numeric_limits<Handle>::max)()) && igpu_get_last_error().empty(), "quiet invalid/released/wrong-role binding query")) return false;
    if (!check(igpu::gl_color_target_begin(x.target, x.previous_target, x.target_viewport), "target begin")) return false;
    x.target_active = true;
    if (!x.accepted([&]{ return x.direct(); }, "ordinary draw counted once with real green pixel") ||
        !x.accepted([&]{ return igpu_draw_indexed(x.vertex, x.layout, x.index, 4, 0, 3, 0, 0, 0, 0); }, "indexed draw counted once") ||
        !x.accepted([&]{ return igpu_draw(x.vertex, 0, x.layout, 4, 0, 3, 2, 0, 0, 0, 0); }, "instanced draw counted once", 65280, 32) ||
        !x.accepted([&]{ return igpu_draw_indirect(x.vertex, 0, x.layout, 4, x.args, 0, 0, 0, 0, 0); }, "indirect draw counted once") ||
        !x.accepted([&]{ return igpu_draw_indexed_indirect(x.vertex, 0, x.layout, x.index, 4, x.args, 32, 0, 0, 0, 0); }, "indexed indirect draw counted once")) return false;
    Array targets{x.target, x.second}, layers{0, 0}, mips{0, 0};
    if (!x.accepted([&]{ return igpu_draw_to_render_targets(x.vertex, x.layout, 4, 0, 3, targets.view(), layers.view(), mips.view(), 0, 0, 0, 0); }, "MRT wrapper counted once") ||
        !check(igpu_texture_read(x.second, 1, 1, 0, 0) == 16711680, "MRT actual second pixel")) return false;
    const auto sampled_ps = x.shader("#version 430\nin vec2 uv;uniform sampler2D igpu_tex;out vec4 c;void main(){c=texture(igpu_tex,uv);}", 1);
    if (!check(sampled_ps && igpu_shader_bind(sampled_ps, 1), "sampled pipeline") ||
        !x.accepted([&]{ return igpu_draw_sampled(x.vertex, x.layout, 4, 0, 3, x.source, 0, 0, 0, 0); }, "sampled draw counted once", 0)) return false;
    x.comparison_source = igpu_texture_create(0, 4, 4, 1, 10, false, false, 1);
    x.compare_state = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 255, 8, 0.f, 0.f, -1.f);
    const auto compare_ps = x.shader("#version 430\nin vec2 uv;uniform sampler2DShadow igpu_tex;out vec4 c;void main(){c=vec4(texture(igpu_tex,vec3(uv,.5)),0,0,1);}", 1);
    if (!check(x.comparison_source && x.compare_state && compare_ps && igpu_shader_bind(compare_ps, 1), "comparison pipeline") ||
        !x.accepted([&]{ return igpu_draw_sampled(x.vertex, x.layout, 4, 0, 3, x.comparison_source, 0, 0, 0, x.compare_state); }, "comparison mirror auxiliary draw excluded", 255) ||
        !check(igpu_shader_bind(x.shaders[1], 1), "ordinary pipeline after sampling")) return false;
    const auto control = x.shader("#version 430\nlayout(vertices=3)out;void main(){gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;if(gl_InvocationID==0){gl_TessLevelOuter[0]=1;gl_TessLevelOuter[1]=1;gl_TessLevelOuter[2]=1;gl_TessLevelInner[0]=1;}}", 4);
    const auto evaluation = x.shader("#version 430\nlayout(triangles,equal_spacing,cw)in;void main(){gl_Position=gl_TessCoord.x*gl_in[0].gl_Position+gl_TessCoord.y*gl_in[1].gl_Position+gl_TessCoord.z*gl_in[2].gl_Position;}", 5);
    if (!check(control && evaluation && igpu_shader_bind(control, 4) && igpu_shader_bind(evaluation, 5), "patch pipeline") ||
        !x.accepted([&]{ return igpu_draw_patch(x.vertex, x.layout, 3, 0, 3, 0, 0, 0, 0); }, "patch draw counted once") ||
        !check(igpu_shader_bind(0, 4) && igpu_shader_bind(0, 5), "ordinary pipeline after patch")) return false;
    const auto compute = x.shader("#version 430\nlayout(local_size_x=1)in;layout(std430,binding=0)buffer Output{uint words[];};void main(){words[0]=123u;}", 2);
    const auto storage = igpu_buffer_create(16, 0, 8, 4);
    const auto before_compute = igpu::draw_count(), before_compute_failures = igpu::draw_restore_failures();
    Array kinds{1}, outputs{static_cast<Handle>(storage)}, zero{0}; std::array<std::uint32_t, 4> words{};
    const bool computed = compute && storage && igpu_shader_bind(compute, 2) &&
        igpu_dispatch(1, 1, 1, kinds.view(), outputs.view(), zero.view(), zero.view()) &&
        igpu_buffer_read(storage, 0, gm::wire::GMBuffer(words.data(), sizeof(words)));
    if (storage) igpu_buffer_release(storage);
    if (!check(computed && words[0] == 123 && igpu::draw_count() == before_compute &&
        igpu::draw_restore_failures() == before_compute_failures, "actual compute write excluded from draw counters")) return false;
    const auto count = igpu::draw_count(), failures = igpu::draw_restore_failures();
    const Bindings valid;
    if (!check(!x.direct(0) && !igpu_draw_indexed(x.vertex, x.layout, x.index, 4, 0, 4, 0, 0, 0, 0) &&
        !igpu_draw_indirect(x.vertex, 0, x.layout, 4, x.args, 2, 0, 0, 0, 0) &&
        igpu::draw_count() == count && igpu::draw_restore_failures() == failures && Bindings() == valid,
        "rejected submissions do not count or lose caller input state")) return false;
    original_bind_vao = f.BindVertexArray; dropped_vao = x.vao; f.BindVertexArray = drop_vao_restore;
    const bool submitted = x.direct(); f.BindVertexArray = original_bind_vao;
    const auto fault_count = igpu::draw_count(), fault_failures = igpu::draw_restore_failures();
    f.BindVertexArray(x.vao);
    if (!check(submitted && fault_count == count + 1 && fault_failures == failures + 1,
        "actual dropped VAO restore increments failure counter once") ||
        !x.accepted([&]{ return x.direct(); }, "subsequent clean draw does not add a restoration failure")) return false;
    f.VertexAttribDivisor(5, 3);
    f.VertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4, reinterpret_cast<void*>(16));
    for (int fault : {1, 2, 3, 4})
    {
        const auto before_count = igpu::draw_count(), before_failures = igpu::draw_restore_failures();
        const Bindings same_bindings;
        vao_fault = fault; f.BindVertexArray = corrupt_vao_restore;
        const bool drawn = x.direct(); f.BindVertexArray = original_bind_vao;
        const bool detected = drawn && Bindings() == same_bindings && igpu::draw_count() == before_count + 1 &&
            igpu::draw_restore_failures() == before_failures + 1;
        const GLuint attribute = fault > 2 ? 5 : 1;
        f.VertexAttribDivisor(attribute, 3);
        f.VertexAttribPointer(attribute, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4, reinterpret_cast<void*>(16));
        const char* label = fault > 2 ? (fault == 3 ? "actual slot5 divisor corruption detected with identical bindings" : "actual slot5 format corruption detected with identical bindings") :
            (fault == 1 ? "actual divisor corruption detected with identical bindings" : "actual format/pointer corruption detected with identical bindings");
        if (!check(detected, label)) return false;
    }
    const auto indirect_count = igpu::draw_count(), indirect_failures = igpu::draw_restore_failures();
    original_bind_buffer = f.BindBuffer; dropped_indirect = x.foreign; f.BindBuffer = drop_indirect_restore;
    const bool indirect_drawn = igpu_draw_indirect(x.vertex, 0, x.layout, 4, x.args, 0, 0, 0, 0, 0);
    f.BindBuffer = original_bind_buffer;
    GLint actual_indirect = 0; glGetIntegerv(0x8F43, &actual_indirect);
    f.BindBuffer(indirect_buffer, x.foreign);
    if (!check(indirect_drawn && actual_indirect != static_cast<GLint>(x.foreign) && igpu::draw_count() == indirect_count + 1 &&
        igpu::draw_restore_failures() == indirect_failures + 1, "actual dropped indirect restoration detected")) return false;
    const auto error_count = igpu::draw_count(), error_failures = igpu::draw_restore_failures();
    glEnable(0xFFFF);
    const bool error_drawn = x.direct();
    const auto pending_error = glGetError();
    if (!check(error_drawn && pending_error == GL_INVALID_ENUM && igpu::draw_count() == error_count + 1 &&
        igpu::draw_restore_failures() == error_failures, "pending caller GL error is preserved by diagnostic snapshots") ||
        !x.accepted([&]{ return x.direct(); }, "clean final drawing after all observed native faults")) return false;
    std::printf("draw diagnostics: native attribute0 binding, direct/indexed/instanced/indirect/MRT/sample/patch counters, compute/helper exclusions, rejects and actual VAO/divisor/format/slot5/indirect restoration faults passed\n");
    return true;
}


bool run_gl_diagnostics_shutdown_tests()
{
    const auto context = wglGetCurrentContext();
    Handle stale = 0; GLuint private_vao = 0;
    igpu_shutdown();
    if (!check(igpu_bind_current(), "lifetime fresh backend")) return false;
    struct CallerInput
    {
        Bindings saved;
        GLuint vao = 0, buffer = 0;
        std::array<unsigned char, 64> bytes{};
        CallerInput()
        {
            auto& f = igpu_gl_fns(); bytes.fill(0xA6);
            f.GenVertexArrays(1, &vao); f.GenBuffers(1, &buffer);
            f.BindVertexArray(vao); f.BindBuffer(array_buffer, buffer);
            f.BufferData(array_buffer, bytes.size(), bytes.data(), 0x88E4);
            f.VertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, nullptr);
            f.BindBuffer(element_buffer, buffer); f.BindBuffer(indirect_buffer, buffer);
        }
        ~CallerInput()
        {
            auto& f = igpu_gl_fns(); saved.restore();
            f.DeleteVertexArrays(1, &vao); f.DeleteBuffers(1, &buffer);
        }
    } caller;
    const Bindings foreign_bindings;
    {
        Fixture x;
        if (!x.initialize() || !check(igpu::gl_color_target_begin(x.target, x.previous_target, x.target_viewport), "lifetime target")) return false;
        x.target_active = true; stale = x.vertex;
        auto& f = igpu_gl_fns(); original_gen_vao = f.GenVertexArrays; generated_vao = 0; f.GenVertexArrays = capture_vao;
        const bool drawn = x.accepted([&]{ return x.direct(); }, "pre-shutdown draw"); f.GenVertexArrays = original_gen_vao; private_vao = generated_vao;
        if (!drawn || !check(private_vao != 0, "owned draw VAO observed")) return false;
    }
    const auto count = igpu::draw_count(), failures = igpu::draw_restore_failures();
    igpu_shutdown();
    using IsVertexArray = GLboolean(APIENTRY*)(GLuint);
    const auto is_vao = reinterpret_cast<IsVertexArray>(wglGetProcAddress("glIsVertexArray"));
    using IsBuffer = GLboolean(APIENTRY*)(GLuint);
    const auto is_buffer = reinterpret_cast<IsBuffer>(wglGetProcAddress("glIsBuffer"));
    std::array<unsigned char, 64> actual_bytes{};
    igpu_gl_fns().GetBufferSubData(array_buffer, 0, actual_bytes.size(), actual_bytes.data());
    if (!check(!is_vao(private_vao) && is_vao(caller.vao) && is_buffer(caller.buffer) &&
        Bindings() == foreign_bindings && actual_bytes == caller.bytes,
        "shutdown deletes owned draw VAO and preserves foreign caller input/bytes")) return false;
    if (!check(!igpu_is_available() && wglGetCurrentContext() == context && igpu::draw_count() == 0 &&
        igpu::draw_restore_failures() == 0, "inactive facade counters and borrowed context")) return false;
    if (!check(igpu_bind_current() && igpu::draw_count() == count && igpu::draw_restore_failures() == failures &&
        !igpu::is_vertex_buffer_bound(stale) && igpu_get_last_error().empty(), "rebind cumulative totals and quiet stale handles")) return false;
    {
        Fixture x;
        if (!x.initialize() || !check(x.vertex != stale && igpu::is_vertex_buffer_bound(x.vertex) &&
            igpu::gl_color_target_begin(x.target, x.previous_target, x.target_viewport), "fresh resource binding after rebind")) return false;
        x.target_active = true;
        if (!x.accepted([&]{ return x.direct(); }, "post-rebind draw") || !check(igpu::draw_count() == count + 1, "cumulative count advanced")) return false;
    }
    std::printf("draw diagnostics shutdown: inactive zero facade, cumulative GL totals, owned VAO deletion/foreign bytes, stale handles and fresh actual drawing passed\n");
    return true;
}


namespace
{
    IgpuGlGetVertexAttribiv core_forward_attrib = nullptr;
    int unsupported_long_queries = 0;
    void APIENTRY strict_core41_attrib(GLuint index, GLenum pname, GLint* value)
    {
        if (pname == 0x874E)
        {
            ++unsupported_long_queries;
            *value = 0; glEnable(0xFFFF); // Model the 4.1 contract: this token is unsupported.
        }
        else core_forward_attrib(index, pname, value);
    }
}

bool run_gl_diagnostics_core_tests()
{
    const auto dc = wglGetCurrentDC();
    const auto compatibility = wglGetCurrentContext();
    using CreateContext = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
    const auto create_context = reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
    if (!check(create_context != nullptr, "core regression context factory")) return false;
    igpu_shutdown();
    const int attributes[] = {0x2091, 4, 0x2092, 1, 0x9126, 1, 0};
    const auto core = create_context(dc, nullptr, attributes);
    auto restore_context = [&]
    {
        igpu_shutdown();
        const bool restored = wglMakeCurrent(dc, compatibility) != FALSE;
        const bool deleted = !core || wglDeleteContext(core) != FALSE;
        return restored && deleted && igpu_bind_current();
    };
    if (!core || !wglMakeCurrent(dc, core) || !igpu_bind_current())
    { restore_context(); return check(false, "create and bind actual4.1 core context"); }
    auto& f = igpu_gl_fns();
    const auto vertex_shader = igpu_shader_compile("#version 410\nlayout(location=0)in vec2 p;void main(){gl_Position=vec4(p,0,1);}", "main", 0, "glsl");
    const auto pixel_shader = igpu_shader_compile("#version 410\nout vec4 c;void main(){c=vec4(0,1,0,1);}", "main", 1, "glsl");
    const auto vertex = igpu_buffer_create(24, 1, 1, 8);
    Array usage{1}, type{2}, step{0};
    const auto layout = igpu_input_layout_create(vertex_shader, usage.view(), type.view(), step.view(), 1, 8, 0);
    const auto target = igpu_texture_create(0, 4, 4, 1, 6, true, false, 1);
    std::array<float, 6> positions{-1, -1, 3, -1, -1, 3};
    bool ok = vertex_shader && pixel_shader && vertex && layout && target &&
        igpu_buffer_write(vertex, 0, gm::wire::GMBuffer(positions.data(), sizeof(positions))) &&
        igpu_shader_bind(vertex_shader, 0) && igpu_shader_bind(pixel_shader, 1);
    const auto setup_error = glGetError();
    if (!check(ok && setup_error == GL_NO_ERROR, "core fixture without compatibility state"))
    { restore_context(); return false; }
    f.BindVertexArray(0);
    const bool reported = igpu::is_vertex_buffer_bound(vertex);
    const auto query_error = glGetError();
    ok = check(!reported && query_error == GL_NO_ERROR, "core VAO0 vertex-source query is quiet false") && ok;
    Array targets{static_cast<Handle>(target)}, zero{0};
    auto submit = [&](std::int64_t count)
    { return igpu_draw_to_render_targets(vertex, layout, 4, 0, count, targets.view(), zero.view(), zero.view(), 0, 0, 0, 0); };
    const auto before = igpu::draw_count(), failures = igpu::draw_restore_failures();
    const bool drawn = submit(3);const auto draw_error = glGetError();
    GLint vao = -1;glGetIntegerv(0x85B5, &vao);
    ok = check(drawn && draw_error == GL_NO_ERROR && vao == 0 && igpu::draw_count() == before + 1 &&
        igpu::draw_restore_failures() == failures && igpu_texture_read(target, 1, 1, 0, 0) == 65280,
        "core VAO0 accepted drawing restores0 without creating GLerror") && ok;
    const bool rejected = !submit(0);const auto reject_text = igpu_get_last_error();const auto rejected_error = glGetError();
    ok = check(rejected && !reject_text.empty() && rejected_error == GL_NO_ERROR &&
        igpu::draw_count() == before + 1 && igpu::draw_restore_failures() == failures,
        "core VAO0 rejected submission adds neither query error nor counts") && ok;
    GLuint caller = 0;f.GenVertexArrays(1, &caller);f.BindVertexArray(caller);
    core_forward_attrib = f.GetVertexAttribiv;unsupported_long_queries = 0;f.GetVertexAttribiv = strict_core41_attrib;
    const bool strict_drawn = submit(3);f.GetVertexAttribiv = core_forward_attrib;
    const auto strict_error = glGetError();
    ok = check(strict_drawn && unsupported_long_queries == 0 && strict_error == GL_NO_ERROR,
        "strict4.1 driver receives no4.3 LONG attribute token") && ok;
    ok = run_gl_raster_stencil_core_tests(vertex, layout, target) && ok;
    ok = run_gl_vertex_layout_tests() && ok;
    f.DeleteVertexArrays(1, &caller);
    const bool restored = restore_context();
    if (!check(restored && ok, "core4.1 regressions and original borrowed compatibility context restored")) return false;
    std::printf("draw diagnostics core: actual4.1 core VAO0 quiet binding/accepted/rejected paths, strict LONG-token guard and borrowed compatibility rebind passed\n");
    return true;
}
