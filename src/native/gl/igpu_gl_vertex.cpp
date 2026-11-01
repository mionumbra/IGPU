#include "igpu_gl_vertex.h"
#include "igpu_gl_draw.h"
#include "igpu_error.h"
#include <array>
#include <cmath>
#include <string>
#include <algorithm>
#include <limits>

namespace igpu
{
namespace
{
constexpr const char *names[] = {"in_pos",          "in_colour", "in_normal",  "in_uv",      "in_blendweight",
                                 "in_blendindices", "in_psize",  "in_tangent", "in_binormal"};
bool integer(const gm::wire::GMValue &value, int &out)
{
    if (value.is<std::int32_t>())
    {
        out = value.as<std::int32_t>();
        return true;
    }
    if (value.is<std::uint64_t>())
    {
        const auto n = value.as<std::uint64_t>();
        if (n > static_cast<std::uint64_t>((std::numeric_limits<int>::max)()))
            return false;
        out = static_cast<int>(n);
        return true;
    }
    if (value.is<double>())
    {
        const double n = value.as<double>();
        if (!std::isfinite(n) || std::trunc(n) != n || n < (std::numeric_limits<int>::min)() ||
            n > (std::numeric_limits<int>::max)())
            return false;
        out = static_cast<int>(n);
        return true;
    }
    return false;
}
bool fail(const char *text)
{
    set_last_error(std::string("igpu_input_layout_create: ") + text);
    return false;
}
bool semantic_name(const std::string &value)
{
    for (const auto prefix : names)
    {
        const std::string base = prefix;
        if (value.compare(0, base.size(), base) != 0)
            continue;
        const auto suffix = value.substr(base.size());
        if (suffix.empty() || std::all_of(suffix.begin(), suffix.end(), [](char c) { return c >= '0' && c <= '9'; }))
            return true;
    }
    return false;
}
bool divisor_supported()
{
    return igpu_gl_fns().VertexAttribDivisor &&
           (gl_context_at_least(3, 3) || gl_context_extension("GL_ARB_instanced_arrays"));
}
struct Active
{
    GLint location = -1, size = 0;
    GLenum type = 0;
    std::string name;
};
} // namespace
GlVertexBindings::~GlVertexBindings()
{
    if (current_saved)
        glPopAttrib();
}
bool gl_vertex_layout_parse(const gm::wire::GMArrayView &usage, const gm::wire::GMArrayView &type,
                            const gm::wire::GMArrayView &step, int count, int vertex_stride, int instance_stride,
                            GlVertexLayout &out)
{
    GLint maximum = 0;
    glGetIntegerv(0x8869 /* MAX_VERTEX_ATTRIBS */, &maximum);
    if (count <= 0 || count > maximum || static_cast<std::size_t>(count) > usage.size() ||
        static_cast<std::size_t>(count) > type.size() || static_cast<std::size_t>(count) > step.size())
        return fail("element_count must fit the usage/type/step arrays and context attribute limit");
    GlVertexLayout candidate;
    std::array<int, 9> indices{};
    int offsets[2]{};
    const bool integer_inputs =
        igpu_gl_fns().VertexAttribIPointer && (gl_context_at_least(3, 0) || gl_context_extension("GL_EXT_gpu_shader4"));
    for (int i = 0; i < count; ++i)
    {
        int u = 0, t = 0, s = 0;
        if (!integer(usage[i], u) || !integer(type[i], t) || !integer(step[i], s) || u < 1 || u > 9 || t < 1 || t > 6 ||
            s < 0 || s > 1)
            return fail("usage/type/step must be valid integer descriptors");
        if (t == 6 && !integer_inputs)
            return fail("this context does not support integer vertex attributes");
        if (s && !gl_instancing_supported())
            return fail("this context does not support instanced attributes");
        GlVertexElement element;
        element.usage = u;
        element.index = indices[u - 1]++;
        element.components = t <= 4 ? t : 4;
        element.type = t <= 4 ? GL_FLOAT : GL_UNSIGNED_BYTE;
        element.normalized = t == 5 ? GL_TRUE : GL_FALSE;
        element.integer = t == 6;
        element.step = s;
        element.offset = offsets[s];
        offsets[s] += t <= 4 ? t * 4 : 4;
        candidate.elements.push_back(element);
    }
    if (vertex_stride > 2048 || instance_stride > 2048 || (vertex_stride > 0 && vertex_stride < offsets[0]) ||
        (instance_stride > 0 && (offsets[1] == 0 || instance_stride < offsets[1])))
        return fail("stream stride must cover its elements and be at most2048 bytes");
    candidate.vertex_stride = vertex_stride > 0 ? vertex_stride : offsets[0];
    candidate.instance_stride = instance_stride > 0 ? instance_stride : offsets[1];
    if (candidate.vertex_stride > 2048 || candidate.instance_stride > 2048)
        return fail("packed stream exceeds2048 bytes");
    out = std::move(candidate);
    return true;
}
bool gl_vertex_layout_resolve(const GlVertexLayout &layout, GLuint program, GlVertexBindings &out)
{
    const auto &f = igpu_gl_fns();
    GLint count = 0, length = 0;
    f.GetProgramiv(program, 0x8B89 /* ACTIVE_ATTRIBUTES */, &count);
    f.GetProgramiv(program, 0x8B8A /* ACTIVE_ATTRIBUTE_MAX_LENGTH */, &length);
    std::vector<char> text(static_cast<std::size_t>(std::max(length, 1)));
    std::vector<Active> active;
    for (GLuint i = 0; i < static_cast<GLuint>(count); ++i)
    {
        Active a;
        GLsizei n = 0;
        f.GetActiveAttrib(program, i, static_cast<GLsizei>(text.size()), &n, &a.size, &a.type, text.data());
        a.name.assign(text.data(), n);
        a.location = f.GetAttribLocation(program, a.name.c_str());
        if (a.location >= 0)
            active.push_back(std::move(a));
    }
    std::vector<GlVertexAttribute> candidate;
    for (std::size_t i = 0; i < layout.elements.size(); ++i)
    {
        const auto &e = layout.elements[i];
        const std::string base = names[e.usage - 1];
        const std::string name = e.index ? base + std::to_string(e.index) : base;
        GLint location = f.GetAttribLocation(program, name.c_str());
        if (location < 0 && !e.index)
            location = f.GetAttribLocation(program, (base + "0").c_str());
        auto attribute = active.end();
        if (location >= 0)
            attribute =
                std::find_if(active.begin(), active.end(), [&](const Active &a) { return a.location == location; });
        else
            attribute = std::find_if(active.begin(), active.end(), [&](const Active &a)
                                     { return a.location == static_cast<GLint>(i) && !semantic_name(a.name); });
        if (attribute == active.end())
            continue;
        location = attribute->location;
        const auto kind = attribute->type;
        const bool floating = kind == GL_FLOAT || (kind >= 0x8B50 && kind <= 0x8B52);
        const bool integral = kind == GL_INT || kind == GL_UNSIGNED_INT || (kind >= 0x8B53 && kind <= 0x8B55) ||
                              (kind >= 0x8DC6 && kind <= 0x8DC8);
        if (attribute->size != 1 || (!e.integer && !floating) || (e.integer && !integral))
        {
            set_last_error("igpu_draw: vertex format is incompatible with a live shader input");
            return false;
        }
        if (std::any_of(candidate.begin(), candidate.end(),
                        [&](const GlVertexAttribute &a) { return a.location == static_cast<GLuint>(location); }))
        {
            set_last_error("igpu_draw: vertex descriptors resolve to overlapping input locations");
            return false;
        }
        candidate.push_back({static_cast<GLuint>(location), i});
    }
    GLint flags = 0;
    if (gl_context_at_least(3, 0))
        f.GetIntegerv(0x821E /* CONTEXT_FLAGS */, &flags);
    if (!(flags & 1 /* CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT */) && gl_vertex_array_has_attributes(0))
    {
        GLint depth = 0, maximum = 0;
        f.GetIntegerv(GL_ATTRIB_STACK_DEPTH, &depth);
        f.GetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH, &maximum);
        if (depth >= maximum)
        {
            set_last_error("igpu_draw: the caller attribute stack has no room to preserve current values");
            return false;
        }
        // Current kinds cannot be queried. Preserve them opaquely in compatibility;
        // core/forward contexts follow native current-value rules (see the guide).
        glPushAttrib(GL_CURRENT_BIT);
        out.current_saved = true;
    }
    out.attributes = std::move(candidate);
    return true;
}
void gl_vertex_layout_bind(const GlVertexLayout &layout, const GlVertexBindings &resolved, GLuint vertex,
                           GLuint instances)
{
    const auto &f = igpu_gl_fns();
    const bool divisor = divisor_supported();
    for (const auto &a : resolved.attributes)
    {
        const auto &e = layout.elements[a.element];
        f.BindBuffer(0x8892 /* ARRAY_BUFFER */, e.step ? instances : vertex);
        f.EnableVertexAttribArray(a.location);
        const auto pointer = reinterpret_cast<const void *>(static_cast<std::uintptr_t>(e.offset));
        const auto stride = e.step ? layout.instance_stride : layout.vertex_stride;
        if (e.integer)
            f.VertexAttribIPointer(a.location, e.components, e.type, stride, pointer);
        else
            f.VertexAttribPointer(a.location, e.components, e.type, e.normalized, stride, pointer);
        if (divisor)
            f.VertexAttribDivisor(a.location, e.step);
    }
}
void gl_vertex_layout_unbind(const GlVertexBindings &resolved)
{
    const auto &f = igpu_gl_fns();
    const bool divisor = divisor_supported();
    for (const auto &a : resolved.attributes)
    {
        if (divisor)
            f.VertexAttribDivisor(a.location, 0);
        f.DisableVertexAttribArray(a.location);
    }
}
} // namespace igpu
