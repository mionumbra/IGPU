#include "vertex_layout_tests.h"
#include "IGPU_native.h"
#include "native/igpu_input_layout.h"
#include "native/igpu_draw.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include "native/gl/igpu_gl_texture.h"
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
using Handle = std::uint64_t;
constexpr GLenum array_buffer = 0x8892, framebuffer = 0x8D40;
bool check(bool ok, const char *label)
{
    if (!ok)
        std::fprintf(stderr, "vertex layout: %s: %s\n", label, igpu_get_last_error().c_str());
    return ok;
}
struct Wire
{
    std::vector<std::byte> bytes;
    Wire(std::initializer_list<std::int64_t> values) : Wire(std::vector<std::int64_t>(values))
    {
    }
    explicit Wire(const std::vector<std::int64_t> &values) : bytes(2 + 9 * values.size())
    {
        gm::byteio::writeLe(bytes.data(), static_cast<std::uint16_t>(values.size()));
        std::size_t i = 2;
        for (auto v : values)
        {
            bytes[i] = static_cast<std::byte>(gm::wire::GMKind::UInt64);
            gm::byteio::writeLe(bytes.data() + i + 1, v);
            i += 9;
        }
    }
    gm::wire::GMArrayView view() const
    {
        return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array, bytes.data()));
    }
};
Wire decimal(double value)
{
    Wire wire{0};
    wire.bytes[2] = static_cast<std::byte>(gm::wire::GMKind::Double);
    std::memcpy(wire.bytes.data() + 3, &value, sizeof(value));
    return wire;
}
void number(std::vector<unsigned char> &bytes, std::size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
struct Snapshot
{
    std::array<GLint, 7> bindings{};
    std::array<GLint, 4> viewport{};
    std::array<std::array<GLint, 8>, 2> attributes{};
    std::array<void *, 2> pointers{};
    Snapshot()
    {
        const GLenum fields[] = {0x8B8D, 0x85B5, 0x8894, 0x8895, 0x8CA6, 0x8CAA, 0x8F43};
        for (int i = 0; i < 7; ++i)
            glGetIntegerv(fields[i], &bindings[i]);
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        const GLenum params[] = {0x8622, 0x8623, 0x8624, 0x8625, 0x886A, 0x889F, 0x88FD, 0x88FE};
        auto &f = igpu_gl_fns();
        for (int a = 0; a < 2; ++a)
        {
            for (int i = 0; i < 8; ++i)
                f.GetVertexAttribiv(5 + a, params[i], &attributes[a][i]);
            f.GetVertexAttribPointerv(5 + a, 0x8645, &pointers[a]);
        }
    }
    bool operator==(const Snapshot &b) const
    {
        return bindings == b.bindings && viewport == b.viewport && attributes == b.attributes && pointers == b.pointers;
    }
};
struct Fixture
{
    std::array<Handle, 6> stages{};
    std::array<GLint, 6> saved{};
    std::array<GLint, 4> viewport{};
    std::array<GLint, 2> polygon{};
    std::array<GLfloat, 4> clear{};
    std::array<GLboolean, 4> mask{};
    std::array<GLboolean, 6> enables{};
    const std::array<GLenum, 6> caps{GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, 0x8C89};
    GLuint vao = 0, foreign = 0, fbo = 0;
    Handle target = 0, source = 0, ps = 0, vs = 0;
    std::vector<Handle> shaders, buffers, layouts;
    Fixture()
    {
        for (int i = 0; i < 6; ++i)
            stages[i] = igpu_get_bound_shader(i);
        const GLenum fields[] = {0x8B8D, 0x85B5, 0x8894, 0x8895, 0x8CA6, 0x8CAA};
        for (int i = 0; i < 6; ++i)
            glGetIntegerv(fields[i], &saved[i]);
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        glGetIntegerv(GL_POLYGON_MODE, polygon.data());
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear.data());
        glGetBooleanv(GL_COLOR_WRITEMASK, mask.data());
        for (int i = 0; i < 6; ++i)
            enables[i] = glIsEnabled(caps[i]);
    }
    ~Fixture()
    {
        auto &f = igpu_gl_fns();
        igpu_shader_bind(0, 1);
        for (int s : {3, 4, 5, 0, 1})
            igpu_shader_bind(stages[s], s);
        for (auto h : layouts)
            igpu_input_layout_release(h);
        for (auto h : buffers)
            igpu_buffer_release(h);
        for (auto h : shaders)
            igpu_shader_release(h);
        if (ps)
            igpu_shader_release(ps);
        if (fbo)
            f.DeleteFramebuffers(1, &fbo);
        if (target)
            igpu_texture_release(target);
        if (source)
            igpu_texture_release(source);
        f.BindVertexArray(saved[1]);
        f.BindBuffer(array_buffer, saved[2]);
        if (saved[1] || igpu::gl_vertex_array_has_attributes(0))
            f.BindBuffer(0x8893, saved[3]);
        f.BindFramebuffer(0x8CA9, saved[4]);
        f.BindFramebuffer(0x8CA8, saved[5]);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        if (igpu::gl_vertex_array_has_attributes(0))
        {
            glPolygonMode(GL_FRONT, polygon[0]);
            glPolygonMode(GL_BACK, polygon[1]);
        }
        else
            glPolygonMode(GL_FRONT_AND_BACK, polygon[0]);
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glColorMask(mask[0], mask[1], mask[2], mask[3]);
        for (int i = 0; i < 6; ++i)
        {
            if (enables[i])
                glEnable(caps[i]);
            else
                glDisable(caps[i]);
        }
        using IsProgram = GLboolean(APIENTRY *)(GLuint);
        auto exists = reinterpret_cast<IsProgram>(wglGetProcAddress("glIsProgram"));
        if (!saved[0] || exists(saved[0]))
            f.UseProgram(saved[0]);
        if (vao)
            f.DeleteVertexArrays(1, &vao);
        if (foreign)
            f.DeleteBuffers(1, &foreign);
    }
    bool bind(const std::string &text, const char *pixel = nullptr)
    {
        if (!pixel)
            pixel = "#version 330\nin vec4 c;out vec4 out_colour;void main(){out_colour=c;}";
        vs = igpu_shader_compile(text.c_str(), "main", 0, "glsl");
        if (!check(vs != 0, "vertex compile"))
            return false;
        shaders.push_back(vs);
        if (pixel)
        {
            const auto next = igpu_shader_compile(pixel, "main", 1, "glsl");
            if (!next)
                return check(false, "pixel compile");
            if (ps)
                shaders.push_back(ps);
            ps = next;
        }
        return check(igpu_shader_bind(0, 1) && igpu_shader_bind(vs, 0) && igpu_shader_bind(ps, 1),
                     "bind typed program");
    }
    bool initialize()
    {
        auto &f = igpu_gl_fns();
        f.GenVertexArrays(1, &vao);
        f.BindVertexArray(vao);
        f.GenBuffers(1, &foreign);
        f.BindBuffer(array_buffer, foreign);
        std::array<unsigned char, 128> bytes{};
        bytes.fill(91);
        f.BufferData(array_buffer, bytes.size(), bytes.data(), 0x88E8);
        f.VertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, 12, reinterpret_cast<void *>(4));
        f.EnableVertexAttribArray(5);
        f.VertexAttribDivisor(5, 3);
        using IPointer = void(APIENTRY *)(GLuint, GLint, GLenum, GLsizei, const void *);
        auto ip = reinterpret_cast<IPointer>(wglGetProcAddress("glVertexAttribIPointer"));
        ip(6, 4, GL_UNSIGNED_BYTE, 12, reinterpret_cast<void *>(8));
        f.EnableVertexAttribArray(6);
        f.VertexAttribDivisor(6, 5);
        for (auto cap : caps)
            glDisable(cap);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        target = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
        source = igpu_texture_create(0, 1, 1, 1, 6, true, false, 1);
        if (!check(target && source, "target fixture"))
            return false;
        f.GenFramebuffers(1, &fbo);
        f.BindFramebuffer(framebuffer, fbo);
        f.FramebufferTexture2D(framebuffer, 0x8CE0, GL_TEXTURE_2D, igpu::gl_find_texture(source)->texture, 0);
        glViewport(0, 0, 1, 1);
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        f.FramebufferTexture2D(framebuffer, 0x8CE0, GL_TEXTURE_2D, igpu::gl_find_texture(target)->texture, 0);
        glViewport(0, 0, 8, 8);
        const char *pixel = "#version 330\nin vec4 c;out vec4 out_colour;void main(){out_colour=c;}";
        return bind(
                   "#version 330\nin vec2 in_pos;out vec4 c;void main(){gl_Position=vec4(in_pos,0,1);c=vec4(1,0,0,1);}",
                   pixel) &&
               check(!glGetError(), "fixture native state");
    }
    Handle buffer(const std::vector<unsigned char> &bytes, int stride, int role = 1)
    {
        auto h = igpu_buffer_create(bytes.size(), role == 16 ? 0 : 1, role, stride);
        if (!check(h > 0, "typed/padded vertex buffer allocation"))
            return 0;
        buffers.push_back(h);
        return check(
                   igpu_buffer_write(h, 0, gm::wire::GMBuffer(const_cast<unsigned char *>(bytes.data()), bytes.size())),
                   "vertex upload")
                   ? h
                   : 0;
    }
    Handle layout(const Wire &usage, const Wire &type, const Wire &step, int count, int stride, int instance = 0,
                  bool native = false)
    {
        auto h = native ? igpu::input_layout_create(vs, usage.view(), type.view(), count, stride)
                        : igpu_input_layout_create(vs, usage.view(), type.view(), step.view(), count, stride, instance);
        if (h)
            layouts.push_back(h);
        else
            check(false, "typed/padded input layout allocation");
        return h;
    }
    template <class Submit> bool image(Submit submit, int expected, const char *label)
    {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        const Snapshot before;
        const auto count = igpu::draw_count(), failures = igpu::draw_restore_failures();
        const bool accepted = submit();
        bool ok = check(accepted, label);
        ok = check(before == Snapshot{} && glGetError() == GL_NO_ERROR,
                   "all caller bindings, higher attributes and GL error preserved") &&
             ok;
        ok = check(igpu::draw_count() == count + (accepted ? 1 : 0) && igpu::draw_restore_failures() == failures,
                   "exact typed draw diagnostics") &&
             ok;
        if (accepted)
        {
            bool reported = false;
            for (int y : {1, 3, 5, 7})
                for (int x : {1, 3, 5, 7})
                {
                    const auto pixel = igpu_texture_read(target, x, y, 0, 0);
                    if (pixel != expected)
                    {
                        if (!reported)
                            std::fprintf(stderr, "vertex pixel [%s]: expected%d actual%lld at%d,%d\n", label, expected,
                                         static_cast<long long>(pixel), x, y);
                        reported = true;
                        ok = false;
                    }
                }
        }
        std::array<unsigned char, 128> bytes{};
        auto &f = igpu_gl_fns();
        GLint previous = 0;
        glGetIntegerv(0x8894, &previous);
        f.BindBuffer(array_buffer, foreign);
        f.GetBufferSubData(array_buffer, 0, bytes.size(), bytes.data());
        f.BindBuffer(array_buffer, previous);
        for (auto v : bytes)
            if (v != 91)
            {
                ok = check(false, "foreign byte91 untouched") && ok;
                break;
            }
        return ok;
    }
    bool direct(Handle b, Handle l)
    {
        return igpu_draw(b, 0, l, 4, 0, 3, 1, 0, 0, 0, 0);
    }
    template <class Submit> bool rejected(Submit submit, const char *label)
    {
        const Snapshot before;
        const auto n = igpu::draw_count(), r = igpu::draw_restore_failures();
        return check(!submit() && !igpu_get_last_error().empty() && before == Snapshot{} && !glGetError() &&
                         n == igpu::draw_count() && r == igpu::draw_restore_failures(),
                     label);
    }
};
std::vector<unsigned char> vertices(int stride, int position_components = 2, int offset = 0)
{
    std::vector<unsigned char> bytes(3 * stride, 0x7f);
    const float xy[][2] = {{-1, -1}, {3, -1}, {-1, 3}};
    for (int i = 0; i < 3; ++i)
        for (int c = 0; c < position_components; ++c)
            number(bytes, i * stride + offset + 4 * c, c < 2 ? xy[i][c] : (c == 3 ? 1.f : 0.f));
    return bytes;
}
// Strictly simulate the legacy specification's permitted post-draw current-value
// changes, after native drawing and before IGPU completes its binding scope.
using SetFloat = void(APIENTRY*)(GLuint, const GLfloat*);
using SetInt = void(APIENTRY*)(GLuint, const GLint*);
using SetUint = void(APIENTRY*)(GLuint, const GLuint*);
using SetDouble = void(APIENTRY*)(GLuint, const GLdouble*);
using GetFloat = void(APIENTRY*)(GLuint, GLenum, GLfloat*);
using GetInt = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetUint = void(APIENTRY*)(GLuint, GLenum, GLuint*);
using GetDouble = void(APIENTRY*)(GLuint, GLenum, GLdouble*);
IgpuGlDisableVertexAttribArray real_disable = nullptr;
SetFloat set_float = nullptr;
SetInt set_int = nullptr;
SetUint set_uint = nullptr;
SetDouble set_double = nullptr;
int current_clobbers = 0;
void APIENTRY clobber_current(GLuint location)
{
    real_disable(location);
    if (location == 5) {
        const GLfloat red[] = {1, 0, 0, 1};
        set_float(location, red);
        ++current_clobbers;
    } else if (location == 6) {
        const GLint bad[] = {-99, 0, 0, 1};
        set_int(location, bad);
        ++current_clobbers;
    } else if (location == 7) {
        const GLuint bad[] = {99, 0, 0, 1};
        set_uint(location, bad);
        ++current_clobbers;
    } else if (location == 8 && set_double) {
        const GLdouble bad[] = {-99, 0, 0, 1};
        set_double(location, bad);
        ++current_clobbers;
    }
}
bool current_values(Fixture& x)
{
    const bool compatibility = igpu::gl_vertex_array_has_attributes(0);
    set_float = reinterpret_cast<SetFloat>(wglGetProcAddress("glVertexAttrib4fv"));
    set_int = reinterpret_cast<SetInt>(wglGetProcAddress("glVertexAttribI4iv"));
    set_uint = reinterpret_cast<SetUint>(wglGetProcAddress("glVertexAttribI4uiv"));
    set_double = reinterpret_cast<SetDouble>(wglGetProcAddress("glVertexAttribL4dv"));
    const auto get_float = reinterpret_cast<GetFloat>(wglGetProcAddress("glGetVertexAttribfv"));
    const auto get_int = reinterpret_cast<GetInt>(wglGetProcAddress("glGetVertexAttribIiv"));
    const auto get_uint = reinterpret_cast<GetUint>(wglGetProcAddress("glGetVertexAttribIuiv"));
    const auto get_double = reinterpret_cast<GetDouble>(wglGetProcAddress("glGetVertexAttribLdv"));
    if (!check(
            set_float && set_int && set_uint && get_float && get_int && get_uint
                && (!compatibility || (set_double && get_double)),
            "known-kind current attribute entrypoints"
        ))
        return false;
    if (compatibility)
        glPushAttrib(GL_CURRENT_BIT);
    auto& f = igpu_gl_fns();
    real_disable = f.DisableVertexAttribArray;
    GLint enabled5 = 0;
    f.GetVertexAttribiv(5, 0x8622, &enabled5);
    real_disable(5);
    struct Cleanup {
        bool compatibility;
        GLint enabled5;
        ~Cleanup()
        {
            auto& f = igpu_gl_fns();
            f.DisableVertexAttribArray = real_disable;
            if (enabled5)
                f.EnableVertexAttribArray(5);
            if (compatibility)
                glPopAttrib();
            else {
                // The fixture owns this context and explicitly seeds its defaults.
                const GLfloat defaults[] = {0, 0, 0, 1};
                set_float(5, defaults);
                set_float(7, defaults);
            }
        }
    } cleanup {compatibility, enabled5};
    const char* simple = "#version 330\nlayout(location=0)in vec2 in_pos;layout(location=5)in vec4 in_colour;"
                         "out vec4 c;void main(){gl_Position=vec4(in_pos,0,1);c=in_colour;}";
    const char* typed = "#version 330\nlayout(location=0)in vec2 in_pos;layout(location=5)in vec4 in_colour;"
                        "layout(location=6)in float in_normal;layout(location=7)in float in_uv;"
                        "layout(location=8)in float in_tangent;out vec4 c;void main(){"
                        "gl_Position=vec4(in_pos,0,1);c=in_colour+vec4(in_normal+in_uv+in_tangent);}";
    if (!x.bind(compatibility ? typed : simple))
        return false;
    auto bytes = vertices(36);
    for (int i = 0; i < 3; ++i) {
        const float values[] = {1, 0, 0, 1, 0, 0, 0};
        std::memcpy(bytes.data() + i * 36 + 8, values, sizeof(values));
    }
    const auto vb = x.buffer(bytes, 36),
               supplied = x.layout(Wire {1, 2, 3, 4, 8}, Wire {2, 4, 1, 1, 1}, Wire {0, 0, 0, 0, 0}, 5, 36),
               omitted = x.layout(Wire {1}, Wire {2}, Wire {0}, 1, 36);
    if (!vb || !supplied || !omitted)
        return false;
    const std::array<GLfloat, 4> green {0, 1, 0, 1}, untouched {0.125f, 0.25f, 0.5f, 1};
    const std::array<GLint, 4> signed_value {-1234567, 71, -2, 1};
    const std::array<GLuint, 4> unsigned_value {4000000000u, 17, 23, 1};
    const std::array<GLdouble, 4> double_value {0.100000000000007, -12.25, 7.5, 1};
    set_float(5, green.data());
    if (compatibility) {
        set_int(6, signed_value.data());
        set_uint(7, unsigned_value.data());
        set_double(8, double_value.data());
    } else
        set_float(7, untouched.data());
    current_clobbers = 0;
    f.DisableVertexAttribArray = clobber_current;
    bool ok = x.image([&] { return x.direct(vb, supplied); }, 255, "arrays render before legacy current clobber");
    f.DisableVertexAttribArray = real_disable;
    ok = check(current_clobbers == (compatibility ? 4 : 1), "strict legacy clobber reached every supplied location")
        && ok;
    if (compatibility) {
        // Getter kinds are known from these fixture setters, never inferred from arrays.
        std::array<GLfloat, 4> actual_float {};
        std::array<GLint, 4> actual_int {};
        std::array<GLuint, 4> actual_uint {};
        std::array<GLdouble, 4> actual_double {};
        get_float(5, 0x8626, actual_float.data());
        get_int(6, 0x8626, actual_int.data());
        get_uint(7, 0x8626, actual_uint.data());
        get_double(8, 0x8626, actual_double.data());
        ok = check(
                 actual_float == green && actual_int == signed_value && actual_uint == unsigned_value
                     && actual_double == double_value,
                 "opaque current state preserves float/int/uint/double despite float arrays"
             )
            && ok;
    } else {
        std::array<GLfloat, 4> actual {};
        get_float(7, 0x8626, actual.data());
        ok = check(actual == untouched, "core omitted current constant remains untouched") && ok;
        // Older core permits supplied locations to become undefined. Callers reseed.
        set_float(5, green.data());
    }
    if (!x.bind(simple))
        return false;
    ok = x.image(
             [&] { return x.direct(vb, omitted); },
             65280,
             compatibility ? "omitted colour retains original green after array draw"
                           : "old core omitted colour uses explicitly reseeded green"
         )
        && ok;
    if (compatibility) {
        GLint before = 0, after = 0, maximum = 0;
        glGetIntegerv(GL_ATTRIB_STACK_DEPTH, &before);
        ok = x.rejected(
                 [&] { return igpu_draw_sampled(vb, supplied, 4, 0, 3, x.source, 0, 0, 0, 0); },
                 "late sampled-uniform failure balances current state"
             )
            && ok;
        glGetIntegerv(GL_ATTRIB_STACK_DEPTH, &after);
        ok = check(after == before, "late rejection leaves attrib stack balanced") && ok;
        glGetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH, &maximum);
        for (int i = before; i < maximum; ++i)
            glPushAttrib(GL_CURRENT_BIT);
        ok = x.rejected([&] { return x.direct(vb, supplied); }, "full current attrib stack rejects before draw") && ok;
        glGetIntegerv(GL_ATTRIB_STACK_DEPTH, &after);
        ok = check(after == maximum, "full stack rejection does not pop caller entries") && ok;
        for (int i = before; i < maximum; ++i)
            glPopAttrib();
    }
    if (ok)
        std::printf(
            "vertex current values: %s, literal omission pixels and known-kind queries passed\n",
            compatibility ? "opaque compatibility float/int/uint/double, full/late-error stack balance"
                          : "core untouched omitted constants and explicit legacy reseeding"
        );
    return ok;
}
bool formats(Fixture &x)
{
    bool ok = true;
    for (int type = 1; type <= 6; ++type)
    {
        const int n = type <= 4 ? type : 4, size = type <= 4 ? 4 * type : 4, stride = 12 + size + 4;
        auto bytes = vertices(stride, 3);
        for (int i = 0; i < 3; ++i)
            for (int c = 0; c < n; ++c)
            {
                if (type <= 4)
                    number(bytes, i * stride + 12 + 4 * c, c == 3 ? 1.f : float(64 * (c + 1)) / 255.f);
                else
                    bytes[i * stride + 12 + c] = static_cast<unsigned char>(c == 3 ? 255 : 64 * (c + 1));
            }
        const std::string gltype = type == 6 ? "uvec4" : type == 1 ? "float" : "vec" + std::to_string(n);
        const std::string color = type == 6 ? "vec4(in_colour)/255.0"
                                  : n == 1  ? "vec4(in_colour,0,0,1)"
                                  : n == 2  ? "vec4(in_colour,0,1)"
                                  : n == 3  ? "vec4(in_colour,1)"
                                            : "in_colour";
        if (!x.bind("#version 330\nin vec3 in_pos;in " + gltype +
                    " in_colour;out vec4 c;void main(){gl_Position=vec4(in_pos,1);c=" + color + ";}"))
        {
            ok = false;
            continue;
        }
        auto b = x.buffer(bytes, stride);
        auto l = x.layout(Wire{1, 2}, Wire{3, type}, Wire{0, 0}, 2, stride);
        const int expected = 64 + (n >= 2 ? 128 * 256 : 0) + (n >= 3 ? 192 * 65536 : 0);
        if (b && l)
            ok = x.image([&] { return x.direct(b, l); }, expected, "all six vertex formats") && ok;
        else
            ok = false;
    }
    for (int n : {1, 3, 4})
    {
        const int stride = 4 * n;
        auto bytes = vertices(stride, n);
        const std::string gltype = n == 1 ? "float" : "vec" + std::to_string(n),
                          pos = n == 1   ? "vec4(in_pos,gl_VertexID==2?3:-1,0,1)"
                                : n == 3 ? "vec4(in_pos,1)"
                                         : "in_pos";
        if (!x.bind("#version 330\nin " + gltype + " in_pos;out vec4 c;void main(){gl_Position=" + pos +
                    ";c=vec4(0,1,0,1);}"))
        {
            ok = false;
            continue;
        }
        auto b = x.buffer(bytes, stride);
        auto l = x.layout(Wire{1}, Wire{n}, Wire{0}, 1, -1, 0, true);
        if (b && l)
            ok = x.image([&] { return x.direct(b, l); }, 65280, "position widths and native packed negative stride") &&
                 ok;
        else
            ok = false;
    }
    return ok;
}
bool semantics(Fixture &x)
{
    bool ok = true;
    const char *names[] = {"in_pos1",         "in_colour0", "in_normal",  "in_uv0",     "in_blendweight",
                           "in_blendindices", "in_psize",   "in_tangent", "in_binormal"};
    for (int usage = 1; usage <= 9; ++usage)
    {
        std::string name = names[usage - 1];
        if (!x.bind("#version 330\nlayout(location=7)in vec4 in_pos;layout(location=3)in vec4 " + name +
                    ";out vec4 c;void main(){gl_Position=in_pos;c=" + name + ";}"))
        {
            ok = false;
            continue;
        }
        auto bytes = vertices(36, 4);
        for (int i = 0; i < 3; ++i)
            for (int c = 0; c < 4; ++c)
                number(bytes, 36 * i + 16 + 4 * c, c == 3 ? 1.f : float(64 * (c + 1)) / 255.f);
        auto b = x.buffer(bytes, 36);
        auto l = x.layout(Wire{1, usage}, Wire{4, 4}, Wire{0, 0}, 2, 36);
        if (b && l)
            ok = x.image([&] { return x.direct(b, l); }, 12615744, "all nine semantics at nonordinal locations") && ok;
        else
            ok = false;
    }
    const char *text = "#version 330\nlayout(location=7)in vec3 in_pos;layout(location=1)in vec4 "
                       "in_colour;layout(location=3)in vec2 in_uv;layout(location=5)in vec2 in_uv1;out vec4 c;void "
                       "main(){gl_Position=vec4(in_pos,1);c=vec4(in_uv.x,in_uv1.y,in_colour.b,1);}";
    if (!x.bind(text))
        return false;
    auto bytes = vertices(36, 3, 12);
    for (int i = 0; i < 3; ++i)
    {
        number(bytes, i * 36, 64.f / 255);
        number(bytes, i * 36 + 4, 0);
        bytes[i * 36 + 8] = 0;
        bytes[i * 36 + 9] = 0;
        bytes[i * 36 + 10] = 192;
        bytes[i * 36 + 11] = 255;
        number(bytes, i * 36 + 24, 0);
        number(bytes, i * 36 + 28, 128.f / 255);
    }
    auto b = x.buffer(bytes, 36);
    auto l = x.layout(Wire{4, 2, 1, 4}, Wire{2, 5, 3, 2}, Wire{0, 0, 0, 0}, 4, 36, 0, true);
    if (b && l)
        ok = x.image([&] { return x.direct(b, l); }, 12615744, "reordered colour+UV, repeated UV and padded offsets") &&
             ok;
    else
        ok = false;
    return ok;
}
bool paths(Fixture &x)
{
    if (!x.bind("#version 330\nin vec3 in_pos;in vec4 in_colour;in vec2 in_uv;out vec4 c;out vec2 uv;void "
                "main(){gl_Position=vec4(in_pos,1);c=in_colour;uv=in_uv;}",
                "#version 330\nin vec4 c;in vec2 uv;uniform sampler2D igpu_tex;out vec4 o;void "
                "main(){o=c+vec4(uv*0.001,0,0);}"))
        return false;
    auto bytes = vertices(28, 3);
    for (int i = 0; i < 3; ++i)
    {
        bytes[28 * i + 12] = 64;
        bytes[28 * i + 13] = 128;
        bytes[28 * i + 14] = 192;
        bytes[28 * i + 15] = 255;
        number(bytes, 28 * i + 16, .25f);
        number(bytes, 28 * i + 20, .25f);
    }
    auto b = x.buffer(bytes, 28);
    auto l = x.layout(Wire{1, 2, 4}, Wire{3, 5, 2}, Wire{0, 0, 0}, 3, 28);
    bool ok = b && l;
    if (ok)
    {
        Wire targets{static_cast<std::int64_t>(x.target)}, zero{0};
        ok = x.image(
                 [&]
                 {
                     return igpu_draw_to_render_targets(b, l, 4, 0, 3, targets.view(), zero.view(), zero.view(), 0, 0,
                                                        0, 0);
                 },
                 12615744, "generic multi-element render target path") &&
             ok;
        std::vector<unsigned char> idx(6);
        const std::uint16_t v[] = {0, 1, 2};
        std::memcpy(idx.data(), v, 6);
        auto ib = x.buffer(idx, 0, 2);
        if (ib)
            ok = x.image([&] { return igpu_draw_indexed(b, l, ib, 4, 0, 3, 0, 0, 0, 0); }, 12615744,
                         "generic indexed path") &&
                 ok;
        else
            ok = false;
    }
    if (!x.bind("#version 330\nin vec3 in_pos;in vec4 in_colour;in vec2 in_uv;out vec4 c;out vec2 uv;void "
                "main(){gl_Position=vec4(in_pos,1);c=in_colour;uv=in_uv;}",
                "#version 330\nin vec4 c;in vec2 uv;uniform sampler2D igpu_tex;out vec4 o;void "
                "main(){o=c*texture(igpu_tex,uv);}"))
        return false;
    if (b && l)
        ok = x.image([&] { return igpu_draw_sampled(b, l, 4, 0, 3, x.source, 0, 0, 0, 0); }, 12615744,
                     "generic sampled path") &&
             ok;
    return ok;
}
bool instances(Fixture &x)
{
    if (!x.bind("#version 330\nin vec3 in_pos;in vec2 in_uv;in vec4 in_colour;in vec4 in_blendweight;out vec4 c;void "
                "main(){gl_Position=vec4(in_pos+vec3(in_uv,0),1);c=in_colour*in_blendweight;}"))
        return false;
    auto vb = x.buffer(vertices(16, 3), 16);
    std::vector<unsigned char> data(64);
    for (int record = 0; record < 2; ++record)
    {
        const int base = 32 * record;
        number(data, base, record ? 0.f : 8.f);
        number(data, base + 4, record ? 0.f : 8.f);
        for (int c = 0; c < 4; ++c)
        {
            data[base + 8 + c] = static_cast<unsigned char>(record || c == 3 ? 255 : 64 * (c + 1));
            number(data, base + 12 + 4 * c, record ? (c == 1 || c == 3 ? 1.f : 0.f) : (c == 3 ? 1.f : .5f));
        }
    }
    auto ib = x.buffer(data, 32);
    auto l = x.layout(Wire{4, 1, 2, 5}, Wire{2, 3, 5, 4}, Wire{1, 0, 1, 1}, 4, 16, 32);
    const bool inputs = vb && ib && l;
    bool ok = inputs;
    if (inputs)
        ok = x.image([&] { return igpu_draw(vb, ib, l, 4, 0, 3, 2, 0, 0, 0, 0); }, 65280,
                     "multiple independently advancing instance fields") &&
             ok;
    if (igpu::gl_indirect_draw_supported())
    {
        std::vector<unsigned char> args(36);
        const std::uint32_t a[] = {3, 2, 0, 0, 3, 2, 0, 0, 0};
        std::memcpy(args.data(), a, sizeof(a));
        auto ab = x.buffer(args, 0, 16);
        std::vector<unsigned char> idx(6);
        const std::uint16_t indices[] = {0, 1, 2};
        std::memcpy(idx.data(), indices, 6);
        auto ix = x.buffer(idx, 0, 2);
        if (inputs && ab)
            ok = x.image([&] { return igpu_draw_indirect(vb, ib, l, 4, ab, 0, 0, 0, 0, 0); }, 65280,
                         "generic indirect descriptors") &&
                 ok;
        if (inputs && ab && ix)
            ok = x.image([&] { return igpu_draw_indexed_indirect(vb, ib, l, ix, 4, ab, 16, 0, 0, 0, 0); }, 65280,
                         "generic indexed indirect descriptors") &&
                 ok;
    }
    std::vector<unsigned char> packed(56);
    for (int i = 0; i < 2; ++i)
        std::memcpy(packed.data() + 28 * i, data.data() + 32 * i, 28);
    const auto packed_vertex = x.buffer(vertices(12, 3), 12), packed_instances = x.buffer(packed, 28);
    const auto packed_layout = x.layout(Wire{4, 1, 2, 5}, Wire{2, 3, 5, 4}, Wire{1, 0, 1, 1}, 4, -1, -1);
    if (packed_vertex && packed_instances && packed_layout)
        ok = x.image([&] { return igpu_draw(packed_vertex, packed_instances, packed_layout, 4, 0, 3, 2, 0, 0, 0, 0); },
                     65280, "independent packed negative stream strides") &&
             ok;
    else
        ok = false;
    if (vb && l)
        ok = x.rejected([&] { return igpu_draw(vb, 0, l, 4, 0, 3, 1, 0, 0, 0, 0); },
                        "missing instance stream rejects without submission") &&
             ok;
    return ok;
}
bool invalid(Fixture &x)
{
    bool ok = true;
    const auto shader = x.vs;
    Wire u{1}, t{2}, s{0};
    const auto dead =
        igpu_shader_compile("#version 330\nin vec2 p;void main(){gl_Position=vec4(p,0,1);}", "main", 0, "glsl");
    if (!check(dead && igpu_shader_release(dead), "released vertex-shader fixture"))
        return false;
    const auto anchor = x.layout(u, t, s, 1, 8);
    Wire empty{};
    for (auto create :
         std::vector<std::function<std::int64_t()>>{
             [&] { return igpu_input_layout_create(shader, u.view(), t.view(), s.view(), -1, 8, 0); },
             [&]
             {
                 return igpu_input_layout_create(shader, u.view(), t.view(), s.view(),
                                                 (std::numeric_limits<int>::max)(), 8, 0);
             },
             [&] { return igpu_input_layout_create(shader, u.view(), t.view(), empty.view(), 1, 8, 0); },
             [&] { return igpu_input_layout_create(shader, decimal(1.5).view(), t.view(), s.view(), 1, 8, 0); },
             [&]
             {
                 return igpu_input_layout_create(
                     shader, u.view(), decimal(std::numeric_limits<double>::infinity()).view(), s.view(), 1, 8, 0);
             },
             [&]
             {
                 return igpu_input_layout_create(shader, u.view(), t.view(),
                                                 decimal(std::numeric_limits<double>::quiet_NaN()).view(), 1, 8, 0);
             },
             [&] { return igpu_input_layout_create(shader, u.view(), t.view(), s.view(), 0, 8, 0); },
             [&] { return igpu_input_layout_create(shader, u.view(), t.view(), s.view(), 2, 8, 0); },
             [&] { return igpu_input_layout_create(shader, Wire{10}.view(), t.view(), s.view(), 1, 8, 0); },
             [&] { return igpu_input_layout_create(shader, u.view(), Wire{7}.view(), s.view(), 1, 8, 0); },
             [&] { return igpu_input_layout_create(shader, u.view(), t.view(), Wire{2}.view(), 1, 8, 0); },
             [&] { return igpu_input_layout_create(shader, u.view(), t.view(), s.view(), 1, 4, 0); },
             [&] { return igpu_input_layout_create(shader, u.view(), t.view(), s.view(), 1, 2049, 0); },
             [&] { return igpu_input_layout_create(shader, u.view(), t.view(), s.view(), 1, 8, 4); },
             [&] { return igpu_input_layout_create(0x7fffffff, u.view(), t.view(), s.view(), 1, 8, 0); },
             [&] { return igpu_input_layout_create(dead, u.view(), t.view(), s.view(), 1, 8, 0); },
             [&] { return igpu_input_layout_create(x.ps, u.view(), t.view(), s.view(), 1, 8, 0); }})
    {
        const Snapshot before;
        const auto h = create();
        ok = check(h == 0 && !igpu_get_last_error().empty() && before == Snapshot{} && !glGetError(),
                   "invalid descriptors/shader reject quietly") &&
             ok;
        if (h)
            x.layouts.push_back(h);
    }
    const auto next = x.layout(decimal(1), decimal(2), decimal(0), 1, 8);
    ok = check(anchor && next == anchor + 1,
               "invalid descriptors consume no layout handles and integral doubles accepted") &&
         ok;
    auto &f = igpu_gl_fns();
    const auto integer_pointer = f.VertexAttribIPointer;
    f.VertexAttribIPointer = nullptr;
    const Snapshot before_integer;
    const auto gated_integer =
        igpu_input_layout_create(shader, Wire{1, 2}.view(), Wire{2, 6}.view(), Wire{0, 0}.view(), 2, 12, 0);
    const bool integer_quiet =
        gated_integer == 0 && !igpu_get_last_error().empty() && before_integer == Snapshot{} && !glGetError();
    f.VertexAttribIPointer = integer_pointer;
    ok = check(integer_quiet, "missing integer pointer rejects only integer layouts without state mutation") && ok;
    const auto divisor = f.VertexAttribDivisor;
    f.VertexAttribDivisor = nullptr;
    const Snapshot before_instance;
    const auto gated_instance =
        igpu_input_layout_create(shader, Wire{1, 2}.view(), Wire{2, 5}.view(), Wire{0, 1}.view(), 2, 8, 4);
    const bool instance_quiet =
        gated_instance == 0 && !igpu_get_last_error().empty() && before_instance == Snapshot{} && !glGetError();
    f.VertexAttribDivisor = divisor;
    ok = check(instance_quiet, "missing divisor rejects instance layouts without state mutation") && ok;
    if (!x.bind("#version 330\nin vec2 in_pos;in uvec4 in_colour;out vec4 c;void "
                "main(){gl_Position=vec4(in_pos,0,1);c=vec4(in_colour)/255.0;}"))
        return false;
    auto bytes = vertices(12);
    for (int i = 0; i < 3; ++i)
        for (int c = 0; c < 4; ++c)
            bytes[i * 12 + 8 + c] = 255;
    const auto vb = x.buffer(bytes, 12), normalized = x.layout(Wire{1, 2}, Wire{2, 5}, Wire{0, 0}, 2, 12);
    if (vb && normalized)
        ok = x.rejected([&] { return x.direct(vb, normalized); },
                        "float/normalized descriptor cannot drive a live integer shader input") &&
             ok;
    else
        ok = false;
    if (!x.bind("#version 330\nin vec2 in_pos;in mat4 in_colour;out vec4 c;void "
                "main(){gl_Position=vec4(in_pos,0,1);c=in_colour[0];}"))
        return false;
    const auto matrix_vertex = x.buffer(vertices(24), 24),
               matrix_layout = x.layout(Wire{1, 2}, Wire{2, 4}, Wire{0, 0}, 2, 24);
    if (matrix_vertex && matrix_layout)
        ok = x.rejected([&] { return x.direct(matrix_vertex, matrix_layout); },
                        "matrix shader inputs reject without partial attribute mutation") &&
             ok;
    else
        ok = false;
    return ok;
}
} // namespace
bool run_gl_vertex_layout_tests()
{
    Fixture x;
    if (!x.initialize())
        return false;
    auto b = x.buffer(vertices(8), 8);
    auto l = x.layout(Wire{1}, Wire{2}, Wire{0}, 1, 8);
    if (!b || !l || !x.image([&] { return x.direct(b, l); }, 255, "canonical float2 control"))
        return false;
    bool ok = formats(x);
    ok = semantics(x) && ok;
    ok = paths(x) && ok;
    ok = instances(x) && ok;
    ok = invalid(x) && ok;
    ok = current_values(x) && ok;
    if (ok)
        std::printf("vertex layouts: six formats, nine usages, repeated/reordered/padded fields, all draw routes, "
                    "integer/instance semantics and higher native attributes passed\n");
    return ok;
}
