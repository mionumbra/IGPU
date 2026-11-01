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
        const bool wide = gl_context_at_least(4, 1) || gl_context_extension("GL_ARB_vertex_attrib_64bit");
        features_ = (indirect ? 1u : 0u) | (integer ? 2u : 0u) | (divisor ? 4u : 0u) | (wide ? 8u : 0u);
        constexpr GLenum bindings[] = {0x85B5, 0x8894, 0x8895, 0x8F43};
        for (int i = 0; i < (indirect ? 4 : 3); ++i) glGetIntegerv(bindings[i], &bindings_[i]);
        constexpr GLenum fields[] = {0x8622, 0x8623, 0x8624, 0x8625, 0x886A, 0x889F};
        auto& f = igpu_gl_fns();
        for (GLuint attribute = 0; attribute < 2; ++attribute)
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
