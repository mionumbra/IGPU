#include "igpu_gl_graphics.h"
#include "igpu_gl_loader.h"
#include "igpu_gl_compute.h"
#include "igpu_error.h"
#include <string>

namespace igpu
{
    namespace
    {
        bool geometry = false, tessellation = false, feedback = false;
        GLint max_patch = 0;
        GLuint linked_program = 0;
        bool linked_geometry = false, linked_tessellation = false;
        GLenum geometry_input = GL_POINTS, tessellation_output = GL_TRIANGLES;
        constexpr GLenum patches = 0x000E;
        bool fail(const char* entry, const char* message)
        { set_last_error(std::string(entry) + ": " + message); return false; }
    }
    void gl_graphics_initialize()
    {
        feedback = gl_context_at_least(4, 0) || gl_context_extension("GL_ARB_transform_feedback2");
        geometry = feedback && gl_context_at_least(3, 2);
        tessellation = feedback && (gl_context_at_least(4, 0) || gl_context_extension("GL_ARB_tessellation_shader"));
        max_patch = 0;
        if (tessellation) glGetIntegerv(0x8E7D, &max_patch);
        linked_program = 0; linked_geometry = linked_tessellation = false;
    }
    bool gl_geometry_supported() { return geometry; }
    bool gl_tessellation_supported() { return tessellation && max_patch > 0 && igpu_gl_fns().PatchParameteri; }
    bool gl_graphics_stage_supported(std::int32_t stage)
    {
        return stage == 0 || stage == 1 || (stage == 2 && gl_compute_supported()) ||
            (stage == 3 && gl_geometry_supported()) || ((stage == 4 || stage == 5) && gl_tessellation_supported());
    }
    bool gl_graphics_switch_allowed(const char* entry)
    {
        if (feedback)
        {
            GLboolean active = GL_FALSE, paused = GL_FALSE;
            glGetBooleanv(0x8E24, &active); glGetBooleanv(0x8E23, &paused);
            if (active && !paused) return fail(entry, "pause active transform feedback before changing the graphics program");
        }
        return true;
    }
    void gl_graphics_program_linked(std::uint32_t program, bool has_geometry, bool has_tessellation)
    {
        // Record executable stages immediately after our successful link. Later
        // attach/detach operations do not change the installed executable.
        linked_program = program;
        linked_geometry = program && has_geometry;
        linked_tessellation = program && has_tessellation;
        auto& f = igpu_gl_fns();
        if (linked_geometry)
        {
            GLint input = 0; f.GetProgramiv(program, 0x8917 /* GEOMETRY_INPUT_TYPE */, &input);
            geometry_input = static_cast<GLenum>(input);
        }
        if (linked_tessellation)
        {
            GLint mode = 0, point_mode = 0;
            f.GetProgramiv(program, 0x8E76 /* TESS_GEN_MODE */, &mode);
            f.GetProgramiv(program, 0x8E79 /* TESS_GEN_POINT_MODE */, &point_mode);
            tessellation_output = point_mode ? GL_POINTS : mode == 0x8E7A /* ISOLINES */ ? GL_LINES : GL_TRIANGLES;
        }
    }
    bool gl_graphics_draw_allowed(std::uint32_t mode, const char* entry)
    {
        GLint current = 0; glGetIntegerv(0x8B8D /* CURRENT_PROGRAM */, &current);
        const bool owned = current && static_cast<GLuint>(current) == linked_program;
        if (mode == patches && (!owned || !linked_tessellation))
            return fail(entry, "patch drawing requires the IGPU-linked hull and domain program to be current");
        if (!owned)
        {
            if (linked_geometry || linked_tessellation)
                return fail(entry, "bind the IGPU-linked geometry/tessellation program before drawing its stages");
            return true; // Existing external ordinary VS/PS programs remain caller-owned.
        }
        if (linked_tessellation && mode != patches)
            return fail(entry, "tessellation requires patch drawing instead of ordinary primitives");
        if (linked_geometry)
        {
            const auto input = mode == patches ? tessellation_output :
                mode == GL_POINTS ? GL_POINTS :
                (mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP) ? GL_LINES : GL_TRIANGLES;
            if (input != geometry_input) return fail(entry, "the submitted primitive does not match the geometry shader input");
        }
        return true;
    }
    bool gl_patch_begin(std::int32_t points, std::int32_t& previous)
    {
        if (!gl_tessellation_supported()) return fail("igpu_draw_patch", "this context does not support tessellation patch drawing");
        if (points < 1 || points > 32 || points > max_patch)
            return fail("igpu_draw_patch", "control points must be 1..32 and fit the native context limit");
        if (!gl_graphics_draw_allowed(patches, "igpu_draw_patch")) return false;
        glGetIntegerv(0x8E72, &previous); igpu_gl_fns().PatchParameteri(0x8E72, points);
        return true;
    }
    void gl_patch_end(std::int32_t previous) { igpu_gl_fns().PatchParameteri(0x8E72, previous); }
}
