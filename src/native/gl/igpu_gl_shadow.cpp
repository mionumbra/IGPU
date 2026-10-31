#include "igpu_gl_shadow.h"

#include "igpu_error.h"
#include "igpu_gl_loader.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace igpu
{
    namespace
    {
        constexpr GLenum depth_format = 0x8DAB; // DEPTH_COMPONENT32F_NV
        constexpr GLenum framebuffer = 0x8D40;
        constexpr GLenum draw_framebuffer = 0x8CA9;
        constexpr GLenum read_framebuffer = 0x8CA8;
        constexpr GLenum base_level = 0x813C;
        constexpr GLenum max_level = 0x813D;
        constexpr GLenum unpack_buffer = 0x88EC;
        using DepthRangeNV = void(APIENTRY*)(GLdouble, GLdouble);

        bool valid_proc(PROC entry)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(entry);
            return address > 3 && address != (std::numeric_limits<std::uintptr_t>::max)();
        }

        bool has_extension(const char* expected)
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

        bool has_texture_swizzle()
        {
            int major = 0, minor = 0;
            const auto version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
            if (!version || std::sscanf(version, "%d.%d", &major, &minor) != 2) return false;
            return major > 3 || (major == 3 && minor >= 3) ||
                has_extension("GL_ARB_texture_swizzle") || has_extension("GL_EXT_texture_swizzle");
        }

        struct CopyState
        {
            GLuint source;
            DepthRangeNV depth_range;
            GLint texture = 0, unpack = 0, program = 0, vao = 0, draw = 0, read = 0;
            GLint viewport[4]{}, depth_func = 0, polygon[2]{}, source_base = 0, source_max = 0;
            GLboolean depth_mask = GL_FALSE;
            bool swizzle_supported = false;
            GLint source_swizzle = GL_RED;
            GLdouble range[2]{};
            static constexpr GLenum caps[] = { GL_DEPTH_TEST, GL_CULL_FACE,
                GL_SCISSOR_TEST, GL_STENCIL_TEST, 0x8C89 /* RASTERIZER_DISCARD */, 0x809D /* MULTISAMPLE */ };
            GLboolean enabled[sizeof(caps) / sizeof(caps[0])]{};
            std::vector<GLboolean> clips;

            CopyState(GLuint original, DepthRangeNV set_range, GLint mips) : source(original), depth_range(set_range)
            {
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
                glGetIntegerv(0x88EF /* PIXEL_UNPACK_BUFFER_BINDING */, &unpack);
                glGetIntegerv(0x8B8D /* CURRENT_PROGRAM */, &program);
                glGetIntegerv(0x85B5 /* VERTEX_ARRAY_BINDING */, &vao);
                glGetIntegerv(0x8CA6 /* DRAW_FRAMEBUFFER_BINDING */, &draw);
                glGetIntegerv(0x8CAA /* READ_FRAMEBUFFER_BINDING */, &read);
                glGetIntegerv(GL_VIEWPORT, viewport);
                glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
                glGetIntegerv(GL_POLYGON_MODE, polygon);
                glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
                glGetDoublev(GL_DEPTH_RANGE, range);
                for (std::size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
                {
                    enabled[i] = glIsEnabled(caps[i]);
                    glDisable(caps[i]);
                }
                GLint count = 0;
                glGetIntegerv(0x0D32 /* MAX_CLIP_DISTANCES */, &count);
                clips.resize(count);
                for (GLint i = 0; i < count; ++i)
                {
                    clips[i] = glIsEnabled(0x3000 /* CLIP_DISTANCE0 */ + i);
                    glDisable(0x3000 + i);
                }
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_ALWAYS);
                glDepthMask(GL_TRUE);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                // Allocation must not interpret nullptr as an offset into a foreign PBO.
                igpu_gl_fns().BindBuffer(unpack_buffer, 0);
                glBindTexture(GL_TEXTURE_2D, source);
                glGetTexParameteriv(GL_TEXTURE_2D, base_level, &source_base);
                glGetTexParameteriv(GL_TEXTURE_2D, max_level, &source_max);
                glTexParameteri(GL_TEXTURE_2D, base_level, 0);
                glTexParameteri(GL_TEXTURE_2D, max_level, mips - 1);
                swizzle_supported = has_texture_swizzle();
                if (swizzle_supported)
                {
                    glGetTexParameteriv(GL_TEXTURE_2D, 0x8E42 /* TEXTURE_SWIZZLE_R */, &source_swizzle);
                    glTexParameteri(GL_TEXTURE_2D, 0x8E42, GL_RED);
                }
            }

            ~CopyState()
            {
                auto& f = igpu_gl_fns();
                glBindTexture(GL_TEXTURE_2D, source);
                glTexParameteri(GL_TEXTURE_2D, base_level, source_base);
                glTexParameteri(GL_TEXTURE_2D, max_level, source_max);
                if (swizzle_supported) glTexParameteri(GL_TEXTURE_2D, 0x8E42, source_swizzle);
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
                f.BindBuffer(unpack_buffer, static_cast<GLuint>(unpack));
                f.UseProgram(static_cast<GLuint>(program));
                f.BindVertexArray(static_cast<GLuint>(vao));
                f.BindFramebuffer(draw_framebuffer, static_cast<GLuint>(draw));
                f.BindFramebuffer(read_framebuffer, static_cast<GLuint>(read));
                glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                glDepthFunc(depth_func);
                glDepthMask(depth_mask);
                depth_range(range[0], range[1]);
                if (polygon[0] == polygon[1]) glPolygonMode(GL_FRONT_AND_BACK, polygon[0]);
                else
                {
                    glPolygonMode(GL_FRONT, polygon[0]);
                    glPolygonMode(GL_BACK, polygon[1]);
                }
                for (std::size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
                {
                    if (enabled[i]) glEnable(caps[i]); else glDisable(caps[i]);
                }
                for (std::size_t i = 0; i < clips.size(); ++i)
                {
                    if (clips[i]) glEnable(0x3000 + static_cast<GLenum>(i));
                    else glDisable(0x3000 + static_cast<GLenum>(i));
                }
            }
        };

        bool level_matches(GLint level, GLint width, GLint height, bool depth)
        {
            GLint actual_width = 0, actual_height = 0, format = 0, border = -1;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &actual_width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &actual_height);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_INTERNAL_FORMAT, &format);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_BORDER, &border);
            return actual_width == width && actual_height == height && border == 0 &&
                (depth ? format == static_cast<GLint>(depth_format) : format == 0x822D || format == 0x822E);
        }

        bool create_program(GlShadow& shadow)
        {
            auto& f = igpu_gl_fns();
            shadow.program = f.CreateProgram();
            const char* sources[] = {
                "#version 130\nvoid main(){gl_Position=vec4(gl_VertexID==1?3.0:-1.0,gl_VertexID==2?3.0:-1.0,0,1);}",
                "#version 130\nuniform sampler2D source_tex; uniform int mip; uniform int negative;\n"
                "void main(){float v=texelFetch(source_tex,ivec2(gl_FragCoord.xy),mip).r;"
                "if((v<0.0)!=(negative!=0))discard; gl_FragDepth=v;}"
            };
            if (!shadow.program)
            {
                set_last_error("gl_shadow_sync: could not create depth copy program");
                return false;
            }
            for (int stage = 0; stage < 2; ++stage)
            {
                const GLuint shader = f.CreateShader(stage ? 0x8B30 : 0x8B31);
                if (!shader)
                {
                    set_last_error("gl_shadow_sync: could not create depth copy shader");
                    return false;
                }
                f.ShaderSource(shader, 1, &sources[stage], nullptr);
                f.CompileShader(shader);
                GLint compiled = 0;
                f.GetShaderiv(shader, 0x8B81 /* COMPILE_STATUS */, &compiled);
                if (compiled) f.AttachShader(shadow.program, shader);
                else
                {
                    char log[1024]{};
                    f.GetShaderInfoLog(shader, sizeof(log), nullptr, log);
                    set_last_error(std::string("gl_shadow_sync: depth copy shader compilation failed: ") + log);
                }
                f.DeleteShader(shader);
                if (!compiled) return false;
            }
            f.LinkProgram(shadow.program);
            GLint linked = 0;
            f.GetProgramiv(shadow.program, 0x8B82 /* LINK_STATUS */, &linked);
            if (!linked)
            {
                char log[1024]{};
                f.GetProgramInfoLog(shadow.program, sizeof(log), nullptr, log);
                set_last_error(std::string("gl_shadow_sync: depth copy program linking failed: ") + log);
                return false;
            }
            f.UseProgram(shadow.program);
            f.Uniform1i(f.GetUniformLocation(shadow.program, "source_tex"), 0);
            shadow.mip_uniform = f.GetUniformLocation(shadow.program, "mip");
            shadow.negative_uniform = f.GetUniformLocation(shadow.program, "negative");
            return shadow.mip_uniform >= 0 && shadow.negative_uniform >= 0;
        }
    }

    bool gl_shadow_supported()
    {
        const auto version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        int major = 0, minor = 0;
        if (!version || std::sscanf(version, "%d.%d", &major, &minor) != 2 || major < 3 ||
            !valid_proc(wglGetProcAddress("glDepthRangedNV"))) return false;
        return has_extension("GL_NV_depth_buffer_float");
    }

    bool gl_shadow_sync(GLuint source, std::int32_t width, std::int32_t height,
                        std::int32_t mips, GlShadow& shadow)
    {
        auto fail = [](const char* message) { set_last_error(message); return false; };
        if (!source || source == shadow.texture || width <= 0 || height <= 0 || mips <= 0 || !glIsTexture(source))
            return fail("gl_shadow_sync: invalid source or dimensions");
        int maximum_mips = 1;
        for (int edge = width > height ? width : height; edge > 1; edge /= 2) ++maximum_mips;
        if (mips > maximum_mips) return fail("gl_shadow_sync: invalid mip count");
        const auto entry = wglGetProcAddress("glDepthRangedNV");
        if (!valid_proc(entry) || (!shadow.texture && !gl_shadow_supported()))
            return fail("gl_shadow_sync: comparison sampling requires GL_NV_depth_buffer_float");
        const auto depth_range = reinterpret_cast<DepthRangeNV>(entry);
        auto& f = igpu_gl_fns();
        CopyState saved(source, depth_range, mips);
        const bool initializing = !shadow.texture;
        if (initializing)
        {
            glGenTextures(1, &shadow.texture);
            f.GenFramebuffers(1, &shadow.framebuffer);
            f.GenVertexArrays(1, &shadow.vertex_array);
            if (!shadow.texture || !shadow.framebuffer || !shadow.vertex_array)
            {
                gl_shadow_release(shadow);
                return fail("gl_shadow_sync: could not create private depth copy resources");
            }
            if (!create_program(shadow))
            {
                gl_shadow_release(shadow);
                return false;
            }
            glBindTexture(GL_TEXTURE_2D, shadow.texture);
            glTexParameteri(GL_TEXTURE_2D, base_level, 0);
            glTexParameteri(GL_TEXTURE_2D, max_level, mips - 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F /* CLAMP_TO_EDGE */);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        }
        f.BindFramebuffer(framebuffer, shadow.framebuffer);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        f.UseProgram(shadow.program);
        f.BindVertexArray(shadow.vertex_array);
        GLint mip_width = width, mip_height = height;
        for (GLint level = 0; level < mips; ++level)
        {
            glBindTexture(GL_TEXTURE_2D, source);
            if (!level_matches(level, mip_width, mip_height, false))
                return fail("gl_shadow_sync: source mip dimensions or R16F/R32F format mismatch");
            glBindTexture(GL_TEXTURE_2D, shadow.texture);
            if (!level_matches(level, mip_width, mip_height, true))
            {
                glTexImage2D(GL_TEXTURE_2D, level, depth_format, mip_width, mip_height, 0,
                             GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
                if (!level_matches(level, mip_width, mip_height, true))
                    return fail("gl_shadow_sync: float depth mirror allocation failed");
            }
            f.FramebufferTexture2D(framebuffer, 0x8D00 /* DEPTH_ATTACHMENT */, GL_TEXTURE_2D, shadow.texture, level);
            if (f.CheckFramebufferStatus(framebuffer) != 0x8CD5 /* FRAMEBUFFER_COMPLETE */)
                return fail("gl_shadow_sync: private depth framebuffer is incomplete");
            glBindTexture(GL_TEXTURE_2D, source);
            glViewport(0, 0, mip_width, mip_height);
            f.Uniform1i(shadow.mip_uniform, level);
            // One symmetric [-FLT_MAX,+FLT_MAX] range overflows the viewport's
            // float scale. Two signed ranges retain every finite float without it.
            const double limit = static_cast<double>((std::numeric_limits<float>::max)());
            for (int negative = 0; negative < 2; ++negative)
            {
                depth_range(negative ? -limit : 0.0, negative ? 0.0 : limit);
                f.Uniform1i(shadow.negative_uniform, negative);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
            mip_width = mip_width > 1 ? mip_width / 2 : 1;
            mip_height = mip_height > 1 ? mip_height / 2 : 1;
        }
        return true;
    }

    void gl_shadow_release(GlShadow& shadow)
    {
        auto& f = igpu_gl_fns();
        if (shadow.texture) glDeleteTextures(1, &shadow.texture);
        if (shadow.framebuffer && f.DeleteFramebuffers) f.DeleteFramebuffers(1, &shadow.framebuffer);
        if (shadow.program && f.DeleteProgram) f.DeleteProgram(shadow.program);
        if (shadow.vertex_array && f.DeleteVertexArrays) f.DeleteVertexArrays(1, &shadow.vertex_array);
        shadow = {};
    }
}
