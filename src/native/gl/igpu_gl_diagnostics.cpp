#include "igpu_gl_diagnostics.h"
#include "igpu_gl_loader.h"
#include <limits>

namespace igpu
{
    namespace
    {
        std::int32_t draws = 0, restore_failures = 0;
        void increment(std::int32_t& value)
        { if (value < (std::numeric_limits<std::int32_t>::max)()) ++value; }
    }
    GlDrawInputState::GlDrawInputState()
    {
        const bool indirect = gl_context_at_least(4, 0) || gl_context_extension("GL_ARB_draw_indirect");
        const bool integer = gl_context_at_least(3, 0) || gl_context_extension("GL_EXT_gpu_shader4");
        const bool divisor = gl_context_at_least(3, 3) || gl_context_extension("GL_ARB_instanced_arrays");
        const bool wide = gl_context_at_least(4, 3); // LONG is a 4.3 query token, independent of LPointer.
        features_ = (indirect ? 1u : 0u) | (integer ? 2u : 0u) | (divisor ? 4u : 0u) | (wide ? 8u : 0u);
        constexpr GLenum bindings[] = {0x85B5, 0x8894, 0x8895, 0x8F43};
        glGetIntegerv(bindings[0], &bindings_[0]);
        const bool attributes = gl_vertex_array_has_attributes(static_cast<std::uint32_t>(bindings_[0]));
        glGetIntegerv(bindings[1], &bindings_[1]);
        if (attributes) glGetIntegerv(bindings[2], &bindings_[2]);
        if (indirect) glGetIntegerv(bindings[3], &bindings_[3]);
        if (!attributes) return; // Core VAO0 is no object; querying its attributes is invalid.
        features_ |= 16u;
        constexpr GLenum fields[] = {0x8622, 0x8623, 0x8624, 0x8625, 0x886A, 0x889F};
        auto& f = igpu_gl_fns();
        GLint count = 0; glGetIntegerv(0x8869 /* MAX_VERTEX_ATTRIBS */, &count);
        attributes_.resize(static_cast<std::size_t>(count)); pointers_.resize(static_cast<std::size_t>(count));
        for (GLuint attribute = 0; attribute < static_cast<GLuint>(count); ++attribute)
        {
            for (int i = 0; i < 6; ++i) f.GetVertexAttribiv(attribute, fields[i], &attributes_[attribute][i]);
            if (integer) f.GetVertexAttribiv(attribute, 0x88FD, &attributes_[attribute][6]);
            if (divisor) f.GetVertexAttribiv(attribute, 0x88FE, &attributes_[attribute][7]);
            if (wide) f.GetVertexAttribiv(attribute, 0x874E, &attributes_[attribute][8]);
            f.GetVertexAttribPointerv(attribute, 0x8645, &pointers_[attribute]);
        }
    }
    bool GlDrawInputState::matches_current() const
    {
        const GlDrawInputState after;
        return features_ == after.features_ && bindings_ == after.bindings_ &&
            attributes_ == after.attributes_ && pointers_ == after.pointers_;
    }
    std::int32_t gl_diagnostic_draw_count() { return draws; }
    std::int32_t gl_diagnostic_restore_failures() { return restore_failures; }
    void gl_diagnostic_record(bool accepted, bool restored)
    {
        if (accepted) increment(draws);
        if (!restored) increment(restore_failures);
    }
}
