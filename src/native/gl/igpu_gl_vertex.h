#pragma once
#include "core/GMExtWire.h"
#include "igpu_gl_loader.h"
#include <vector>

namespace igpu
{
struct GlVertexElement
{
    int usage = 0, index = 0, components = 0, step = 0, offset = 0;
    GLenum type = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
    bool integer = false;
};
struct GlVertexLayout
{
    int vertex_stride = 0, instance_stride = 0;
    std::vector<GlVertexElement> elements;
};
struct GlVertexAttribute
{
    GLuint location = 0;
    std::size_t element = 0;
};
using GlVertexBindings = std::vector<GlVertexAttribute>;
bool gl_vertex_layout_parse(const gm::wire::GMArrayView &usage, const gm::wire::GMArrayView &type,
                            const gm::wire::GMArrayView &step, int count, int vertex_stride, int instance_stride,
                            GlVertexLayout &out);
bool gl_vertex_layout_resolve(const GlVertexLayout &layout, GLuint program, GlVertexBindings &out);
void gl_vertex_layout_bind(const GlVertexLayout &layout, const GlVertexBindings &resolved, GLuint vertex,
                           GLuint instances);
void gl_vertex_layout_unbind(const GlVertexBindings &resolved);
} // namespace igpu
