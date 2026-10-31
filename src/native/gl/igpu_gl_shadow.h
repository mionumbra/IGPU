#pragma once

#include <Windows.h>
#include <GL/gl.h>

#include <cstdint>

namespace igpu
{
    struct GlShadow
    {
        GLuint texture = 0;
        GLuint framebuffer = 0;
        GLuint program = 0;
        GLuint vertex_array = 0;
        GLint mip_uniform = -1;
        GLint negative_uniform = -1;
    };

    bool gl_shadow_supported();
    // Refresh every mip with two GPU depth passes, preserving all finite R16F/R32F
    // values. NV_depth_buffer_float supplies unclamped signed depth ranges. Programs
    // and objects are reused per source; O(total texels), no CPU texel readback.
    bool gl_shadow_sync(GLuint source, std::int32_t width, std::int32_t height,
                        std::int32_t mips, GlShadow& shadow);
    void gl_shadow_release(GlShadow& shadow);
}
