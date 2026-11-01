#include "igpu_gl_draw.h"

#include "igpu_error.h"
#include "igpu_gl_loader.h"
#include "igpu_gl_shadow.h"
#include "igpu_gl_compute.h"
#include "igpu_gl_uniform.h"
#include "igpu_gl_targets.h"
#include "igpu_gl_color_state.h"
#include "igpu_gl_texture.h"
#include "igpu_gl_graphics.h"

#include <Windows.h>
#include <GL/gl.h>

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_CURRENT_PROGRAM 0x8B8D
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT 0x8370
#endif

#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BASE_LEVEL
#define GL_TEXTURE_BASE_LEVEL 0x813C
#endif

#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif

#ifndef GL_TEXTURE_LOD_BIAS
#define GL_TEXTURE_LOD_BIAS 0x8501
#endif

#ifndef GL_TEXTURE_MIN_LOD
#define GL_TEXTURE_MIN_LOD 0x813A
#endif

#ifndef GL_TEXTURE_MAX_LOD
#define GL_TEXTURE_MAX_LOD 0x813B
#endif

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_NEAREST_MIPMAP_NEAREST
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#endif

#ifndef GL_LINEAR_MIPMAP_NEAREST
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#endif

#ifndef GL_NEAREST_MIPMAP_LINEAR
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#endif

#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

namespace igpu
{
    namespace
    {
        struct BufferObject
        {
            GLuint id = 0;
            std::int64_t size = 0;
            std::int32_t stride = 0;
            std::int32_t bind = 0;
            GLenum target = GL_ARRAY_BUFFER;
            std::int32_t usage = 0;
        };

        struct LayoutObject
        {
            int vertex_stride = 0;
            int instance_stride = 0;
            bool instance_colour = false;
        };
        bool g_instancing_context = false, g_indirect_context = false, g_depth_clamp_context = false;

        std::unordered_map<std::uint64_t, BufferObject> g_buffers;
        std::uint64_t g_next_buffer = 1;
        std::uint64_t g_next_layout = 1;
        std::unordered_map<std::uint64_t, LayoutObject> g_layouts;
        struct SamplerObject
        {
            std::int32_t address = 0;
            std::int32_t address_v = 0;
            bool linear = false;
            float border_red = 0.f;
            float border_green = 0.f;
            float border_blue = 0.f;
            float border_alpha = 1.f;
            bool linear_min = false;
            float level_offset = 0.f;
            float finest = 0.f;
            // Matches the no-limit value in sampler_state_create_full.
            float coarsest = 3.402823466e+38f;
            bool linear_mip = false;
            std::int32_t anisotropy = 1;
            GLenum compare = GL_NONE;
            std::int32_t address_w = 0;
        };

        std::unordered_map<std::uint64_t, SamplerObject> g_samplers;
        struct BlendObject
        {
            bool enabled = false;
            GLenum src_rgb = GL_ONE;
            GLenum dst_rgb = GL_ZERO;
            GLenum src_alpha = GL_ONE;
            GLenum dst_alpha = GL_ZERO;
            GLenum equation_rgb = 0x8006;
            GLenum equation_alpha = 0x8006;
            bool write_red = true;
            bool write_green = true;
            bool write_blue = true;
            bool write_alpha = true;
        };
        std::unordered_map<std::uint64_t, BlendObject> g_blends;
        struct RasterObject
        {
            // 0 disables culling. 1 drops clockwise faces. 2 drops counter-clockwise faces.
            // Clockwise is front, matching the Direct3D rasterizer.
            // Scissor uses the rectangle already set by the caller. This draw does not change the box.
            std::int32_t cull = 0;
            bool scissor = false;
            std::int32_t fill = 0;
            bool depth_clip = true;
        };
        std::unordered_map<std::uint64_t, RasterObject> g_rasters;
        struct DepthObject
        {
            bool test = false;
            bool write = false;
            GLenum func = GL_LESS;
            bool stencil = false;
            GLenum stencil_func = GL_ALWAYS;
            GLint stencil_ref = 0;
            GLuint read_mask = 255, write_mask = 255;
            GLenum fail = GL_KEEP, depth_fail = GL_KEEP, pass = GL_KEEP;
        };
        std::unordered_map<std::uint64_t, DepthObject> g_depths;
        std::uint64_t g_next_sampler = 1;

        bool gl_blend_factor(std::int32_t raw, bool alpha_slot, GLenum& out)
        {
            switch (raw)
            {
            case 1: out = GL_ZERO; return true;
            case 2: out = GL_ONE; return true;
            case 3: out = alpha_slot ? GL_SRC_ALPHA : GL_SRC_COLOR; return true;
            case 4: out = alpha_slot ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE_MINUS_SRC_COLOR; return true;
            case 5: out = GL_SRC_ALPHA; return true;
            case 6: out = GL_ONE_MINUS_SRC_ALPHA; return true;
            case 7: out = GL_DST_ALPHA; return true;
            case 8: out = GL_ONE_MINUS_DST_ALPHA; return true;
            case 9: out = alpha_slot ? GL_DST_ALPHA : GL_DST_COLOR; return true;
            case 10: out = alpha_slot ? GL_ONE_MINUS_DST_ALPHA : GL_ONE_MINUS_DST_COLOR; return true;
            case 11: out = GL_SRC_ALPHA_SATURATE; return true;
            default: return false;
            }
        }

        bool gl_blend_equation(std::int32_t raw, GLenum& out)
        {
            switch (raw)
            {
            case 1: out = 0x8006; return true;
            case 2: out = 0x8008; return true;
            case 3: out = 0x800B; return true;
            case 4: out = 0x8007; return true;
            case 5: out = 0x800A; return true;
            default: return false;
            }
        }
        GLuint g_vertex_array = 0;

        bool read_i64(const gm::wire::GMValue& value, std::int64_t& out)
        {
            if (value.is<std::uint64_t>())
            {
                out = static_cast<std::int64_t>(value.as<std::uint64_t>());
                return true;
            }
            if (value.is<std::int32_t>())
            {
                out = value.as<std::int32_t>();
                return true;
            }
            if (value.is<double>())
            {
                out = static_cast<std::int64_t>(value.as<double>());
                return true;
            }
            return false;
        }

        void bind_vertices(const BufferObject& buffer, std::int64_t pointer_vertex)
        {
            const auto& fns = igpu_gl_fns();
            const auto base = static_cast<std::uintptr_t>(pointer_vertex * buffer.stride);
            fns.EnableVertexAttribArray(0);
            fns.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, buffer.stride,
                                    reinterpret_cast<const void*>(base));
            if (buffer.stride == 12)
            {
                fns.EnableVertexAttribArray(1);
                fns.VertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, buffer.stride,
                                        reinterpret_cast<const void*>(base + 8));
            }
            else if (buffer.stride == 16)
            {
                fns.EnableVertexAttribArray(1);
                fns.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, buffer.stride,
                                        reinterpret_cast<const void*>(base + 8));
            }
            else
            {
                fns.DisableVertexAttribArray(1);
            }
        }

        void unbind_vertices()
        {
            const auto& fns = igpu_gl_fns();
            fns.DisableVertexAttribArray(0);
            fns.DisableVertexAttribArray(1);
        }

        bool gl_primitive_mode(std::int32_t primitive, GLenum& mode)
        {
            switch (primitive)
            {
            case 1: mode = GL_POINTS; return true;
            case 2: mode = GL_LINES; return true;
            case 3: mode = GL_LINE_STRIP; return true;
            case 4: mode = GL_TRIANGLES; return true;
            case 5: mode = GL_TRIANGLE_STRIP; return true;
            default: return false;
            }
        }

        bool gl_pipeline_accepted(const char* entry, std::int32_t primitive, GLenum& mode)
        {
            if (primitive == 6)
            {
                set_last_error(std::string(entry) + ": the 'trianglefan' primitive has no backend equivalent");
                return false;
            }
            if (!gl_primitive_mode(primitive, mode))
            {
                set_last_error(std::string(entry) + ": the opengl backend does not draw this primitive");
                return false;
            }
            return true;
        }

        bool gl_resolve_blend(const char* entry, std::int64_t blend_state, const BlendObject*& out)
        {
            out = nullptr;
            if (blend_state == 0)
            {
                return true;
            }
            if (blend_state < 0)
            {
                set_last_error(std::string(entry) + ": blend handle is not a state object");
                return false;
            }
            const auto found = g_blends.find(static_cast<std::uint64_t>(blend_state));
            if (found == g_blends.end())
            {
                set_last_error(std::string(entry) + ": unknown blend handle");
                return false;
            }
            out = &found->second;
            return true;
        }

        struct SavedBlend
        {
            GlColorState indexed;
            GLboolean enabled = GL_FALSE;
            GLint src_rgb = GL_ONE;
            GLint dst_rgb = GL_ZERO;
            GLint src_alpha = GL_ONE;
            GLint dst_alpha = GL_ZERO;
            GLint equation_rgb = 0x8006;
            GLint equation_alpha = 0x8006;
            GLboolean mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
        };

        void save_blend(SavedBlend& saved)
        {
            if (saved.indexed.capture(true)) return;
            saved.enabled = glIsEnabled(GL_BLEND);
            glGetIntegerv(0x80C9, &saved.src_rgb);
            glGetIntegerv(0x80C8, &saved.dst_rgb);
            glGetIntegerv(0x80CB, &saved.src_alpha);
            glGetIntegerv(0x80CA, &saved.dst_alpha);
            glGetIntegerv(0x8009, &saved.equation_rgb);
            glGetIntegerv(0x883D, &saved.equation_alpha);
            glGetBooleanv(0x0C23, saved.mask);
        }

        void apply_blend(const BlendObject& blend)
        {
            const auto& fns = igpu_gl_fns();
            if (blend.enabled)
            {
                glEnable(GL_BLEND);
            }
            else
            {
                glDisable(GL_BLEND);
            }
            fns.BlendFuncSeparate(blend.src_rgb, blend.dst_rgb, blend.src_alpha, blend.dst_alpha);
            fns.BlendEquationSeparate(blend.equation_rgb, blend.equation_alpha);
            glColorMask(blend.write_red ? GL_TRUE : GL_FALSE, blend.write_green ? GL_TRUE : GL_FALSE,
                        blend.write_blue ? GL_TRUE : GL_FALSE, blend.write_alpha ? GL_TRUE : GL_FALSE);
        }

        void restore_blend(const SavedBlend& saved)
        {
            if (saved.indexed.restore()) return;
            const auto& fns = igpu_gl_fns();
            if (saved.enabled == GL_TRUE)
            {
                glEnable(GL_BLEND);
            }
            else
            {
                glDisable(GL_BLEND);
            }
            fns.BlendFuncSeparate(static_cast<GLenum>(saved.src_rgb), static_cast<GLenum>(saved.dst_rgb),
                                  static_cast<GLenum>(saved.src_alpha), static_cast<GLenum>(saved.dst_alpha));
            fns.BlendEquationSeparate(static_cast<GLenum>(saved.equation_rgb),
                                      static_cast<GLenum>(saved.equation_alpha));
            glColorMask(saved.mask[0], saved.mask[1], saved.mask[2], saved.mask[3]);
        }

        bool gl_resolve_raster(const char* entry, std::int64_t raster_state, const RasterObject*& out)
        {
            out = nullptr;
            if (raster_state == 0)
            {
                return true;
            }
            if (raster_state < 0)
            {
                set_last_error(std::string(entry) + ": raster handle is not a state object");
                return false;
            }
            const auto found = g_rasters.find(static_cast<std::uint64_t>(raster_state));
            if (found == g_rasters.end())
            {
                set_last_error(std::string(entry) + ": unknown raster handle");
                return false;
            }
            out = &found->second;
            return true;
        }

        struct SavedCull
        {
            GLboolean enabled = GL_FALSE;
            GLint mode = GL_BACK;
            GLint front = GL_CCW;
            GLboolean scissor = GL_FALSE;
            GLint polygon[2] = {GL_FILL, GL_FILL};
            bool has_clamp = false;
            GLboolean clamp = GL_FALSE;
        };

        void save_cull(SavedCull& saved)
        {
            saved.enabled = glIsEnabled(GL_CULL_FACE);
            glGetIntegerv(GL_CULL_FACE_MODE, &saved.mode);
            glGetIntegerv(GL_FRONT_FACE, &saved.front);
            saved.scissor = glIsEnabled(GL_SCISSOR_TEST);
            glGetIntegerv(GL_POLYGON_MODE, saved.polygon);
            saved.has_clamp = g_depth_clamp_context;
            if (saved.has_clamp) saved.clamp = glIsEnabled(0x864F /* DEPTH_CLAMP */);
        }

        void apply_cull(const RasterObject& raster)
        {
            glPolygonMode(GL_FRONT_AND_BACK, raster.fill == 1 ? GL_LINE : GL_FILL);
            if (g_depth_clamp_context)
            {
                if (raster.depth_clip) glDisable(0x864F); else glEnable(0x864F);
            }
            glFrontFace(GL_CW);
            if (raster.cull == 0)
            {
                glDisable(GL_CULL_FACE);
            }
            else
            {
                glEnable(GL_CULL_FACE);
                glCullFace(raster.cull == 1 ? GL_FRONT : GL_BACK);
            }
            if (raster.scissor)
            {
                glEnable(GL_SCISSOR_TEST);
            }
            else
            {
                glDisable(GL_SCISSOR_TEST);
            }
        }

        void restore_cull(const SavedCull& saved)
        {
            if (saved.polygon[0] == saved.polygon[1]) glPolygonMode(GL_FRONT_AND_BACK, saved.polygon[0]);
            else
            {
                // Different native face modes imply a compatibility context.
                glPolygonMode(GL_FRONT, saved.polygon[0]); glPolygonMode(GL_BACK, saved.polygon[1]);
            }
            if (saved.has_clamp)
            {
                if (saved.clamp) glEnable(0x864F); else glDisable(0x864F);
            }
            glFrontFace(static_cast<GLenum>(saved.front));
            glCullFace(static_cast<GLenum>(saved.mode));
            if (saved.enabled == GL_TRUE)
            {
                glEnable(GL_CULL_FACE);
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }
            if (saved.scissor == GL_TRUE)
            {
                glEnable(GL_SCISSOR_TEST);
            }
            else
            {
                glDisable(GL_SCISSOR_TEST);
            }
        }

        bool gl_resolve_depth(const char* entry, std::int64_t depth_state, const DepthObject*& out)
        {
            out = nullptr;
            if (depth_state == 0)
            {
                return true;
            }
            if (depth_state < 0)
            {
                set_last_error(std::string(entry) + ": depth handle is not a state object");
                return false;
            }
            const auto found = g_depths.find(static_cast<std::uint64_t>(depth_state));
            if (found == g_depths.end())
            {
                set_last_error(std::string(entry) + ": unknown depth handle");
                return false;
            }
            out = &found->second;
            return true;
        }

        struct SavedStencilFace
        {
            GLint function = GL_ALWAYS, reference = 0, read_mask = -1, write_mask = -1;
            GLint fail = GL_KEEP, depth_fail = GL_KEEP, pass = GL_KEEP;
        };
        void save_stencil_face(SavedStencilFace& saved, bool back)
        {
            const GLenum front[] = {0x0B92,0x0B97,0x0B93,0x0B98,0x0B94,0x0B95,0x0B96};
            const GLenum reverse[] = {0x8800,0x8CA3,0x8CA4,0x8CA5,0x8801,0x8802,0x8803};
            const auto* fields = back ? reverse : front;
            GLint* values[] = {&saved.function,&saved.reference,&saved.read_mask,&saved.write_mask,&saved.fail,&saved.depth_fail,&saved.pass};
            for (int i = 0; i < 7; ++i) glGetIntegerv(fields[i], values[i]);
        }
        void restore_stencil_face(const SavedStencilFace& saved, GLenum face)
        {
            auto& f = igpu_gl_fns();
            f.StencilFuncSeparate(face, static_cast<GLenum>(saved.function), saved.reference, static_cast<GLuint>(saved.read_mask));
            f.StencilMaskSeparate(face, static_cast<GLuint>(saved.write_mask));
            f.StencilOpSeparate(face, static_cast<GLenum>(saved.fail), static_cast<GLenum>(saved.depth_fail), static_cast<GLenum>(saved.pass));
        }
        struct SavedDepth
        {
            GLboolean enabled = GL_FALSE;
            GLint func = GL_LESS;
            GLboolean mask = GL_TRUE;
            GLboolean stencil = GL_FALSE;
            SavedStencilFace front, back;
        };

        void save_depth(SavedDepth& saved)
        {
            saved.enabled = glIsEnabled(GL_DEPTH_TEST);
            glGetIntegerv(GL_DEPTH_FUNC, &saved.func);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &saved.mask);
            saved.stencil = glIsEnabled(GL_STENCIL_TEST);
            save_stencil_face(saved.front, false); save_stencil_face(saved.back, true);
        }

        void apply_depth(const DepthObject& depth)
        {
            if (depth.test)
            {
                glEnable(GL_DEPTH_TEST);
            }
            else
            {
                glDisable(GL_DEPTH_TEST);
            }
            glDepthFunc(depth.func);
            glDepthMask(depth.write ? GL_TRUE : GL_FALSE);
            if (depth.stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
            auto& f = igpu_gl_fns();
            f.StencilFuncSeparate(GL_FRONT_AND_BACK, depth.stencil_func, depth.stencil_ref, depth.read_mask);
            f.StencilMaskSeparate(GL_FRONT_AND_BACK, depth.write_mask);
            f.StencilOpSeparate(GL_FRONT_AND_BACK, depth.fail, depth.depth_fail, depth.pass);
        }

        void restore_depth(const SavedDepth& saved)
        {
            restore_stencil_face(saved.front, GL_FRONT); restore_stencil_face(saved.back, GL_BACK);
            if (saved.stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
            glDepthFunc(static_cast<GLenum>(saved.func));
            glDepthMask(saved.mask);
            if (saved.enabled == GL_TRUE)
            {
                glEnable(GL_DEPTH_TEST);
            }
            else
            {
                glDisable(GL_DEPTH_TEST);
            }
        }

        bool gl_draw_accepted(const char* entry, std::int32_t primitive, std::int64_t blend_state,
                              std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state,
                              const BlendObject*& blend, const DepthObject*& depth, const RasterObject*& raster,
                              GLenum& mode, bool patch = false)
        {
            if (!gl_uniform_validate_inputs(0, 1, entry) || !gl_uniform_validate_inputs(3, 5, entry)) return false;
            if (!gl_pipeline_accepted(entry, primitive, mode))
            {
                return false;
            }
            if (!gl_graphics_draw_allowed(patch ? 0x000E : mode, entry)) return false;
            if (!gl_resolve_blend(entry, blend_state, blend))
            {
                return false;
            }
            if (!gl_resolve_depth(entry, depth_state, depth))
            {
                return false;
            }
            if (!gl_resolve_raster(entry, raster_state, raster))
            {
                return false;
            }
            if (sampler_state != 0)
            {
                set_last_error(std::string(entry) + ": the opengl backend draws with no extra state");
                return false;
            }
            return true;
        }

        GLint gl_wrap(std::int32_t address)
        {
            if (address == 1)
            {
                return GL_REPEAT;
            }
            if (address == 2)
            {
                return GL_MIRRORED_REPEAT;
            }
            if (address == 3)
            {
                return GL_CLAMP_TO_BORDER;
            }
            return GL_CLAMP_TO_EDGE;
        }
    }

    void gl_resources_release()
    {
        const auto& fns = igpu_gl_fns();
        if (g_vertex_array && fns.DeleteVertexArrays) fns.DeleteVertexArrays(1, &g_vertex_array);
        if (fns.DeleteBuffers != nullptr)
        {
            for (const auto& entry : g_buffers)
            {
                fns.DeleteBuffers(1, &entry.second.id);
            }
        }
        gl_textures_release();
        g_buffers.clear();
        g_layouts.clear();
        g_samplers.clear();
        g_blends.clear();
        g_rasters.clear();
        g_depths.clear();
        g_next_sampler = 1;
        g_vertex_array = 0;
        g_next_layout = 1;
    }

    namespace
    {
        constexpr GLenum buffer_size_parameter = 0x8764;
        constexpr GLenum buffer_mapped_parameter = 0x88BC;
        constexpr GLenum copy_read_buffer = 0x8F36;
        constexpr GLenum copy_write_buffer = 0x8F37;

        struct ArrayBufferBinding
        {
            GLint previous = 0;
            ArrayBufferBinding() { glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous); }
            ~ArrayBufferBinding() { igpu_gl_fns().BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous)); }
        };

        struct CopyBufferBindings
        {
            GLint read = 0, write = 0;
            CopyBufferBindings()
            {
                glGetIntegerv(copy_read_buffer, &read);
                glGetIntegerv(copy_write_buffer, &write);
            }
            ~CopyBufferBindings()
            {
                igpu_gl_fns().BindBuffer(copy_read_buffer, static_cast<GLuint>(read));
                igpu_gl_fns().BindBuffer(copy_write_buffer, static_cast<GLuint>(write));
            }
        };

        bool valid_buffer_size(std::int64_t size, std::int32_t bind, std::int32_t stride)
        {
            if (size <= 0 || size > (std::numeric_limits<GLint>::max)()) return false;
            if (bind == 1) return stride > 0 && size % stride == 0;
            if (bind == 2) return size % 2 == 0;
            if (bind == 4) return size % 4 == 0;
            if (bind == 8) return stride > 0 && size % stride == 0;
            if (bind == 16) return size >= 16 && size % 4 == 0;
            return bind == 0;
        }

        GLenum buffer_hint(std::int32_t usage)
        {
            return usage == 1 ? 0x88E8 /* DYNAMIC_DRAW */ : usage == 2 ? 0x88E1 /* STREAM_READ */ : GL_STATIC_DRAW;
        }

        bool buffer_unmapped(GLenum target, const char* entry)
        {
            GLint mapped = GL_FALSE;
            igpu_gl_fns().GetBufferParameteriv(target, buffer_mapped_parameter, &mapped);
            if (!mapped) return true;
            set_last_error(std::string(entry) + ": the buffer is mapped by the caller");
            return false;
        }

        bool allocated_buffer_size(GLenum target, std::int64_t expected)
        {
            GLint allocated = 0;
            igpu_gl_fns().GetBufferParameteriv(target, buffer_size_parameter, &allocated);
            return allocated == expected;
        }

        void zero_buffer_range(GLenum target, std::int64_t offset, std::int64_t count)
        {
            static constexpr unsigned char zeros[65536]{};
            while (count > 0)
            {
                const auto chunk = count < static_cast<std::int64_t>(sizeof(zeros)) ? count : static_cast<std::int64_t>(sizeof(zeros));
                igpu_gl_fns().BufferSubData(target, static_cast<std::ptrdiff_t>(offset),
                                           static_cast<std::ptrdiff_t>(chunk), zeros);
                offset += chunk;
                count -= chunk;
            }
        }

        bool buffer_range_valid(std::int64_t size, std::int64_t offset, const gm::wire::GMBuffer& data,
                                const char* entry)
        {
            const auto bytes = static_cast<std::uint64_t>(data.length());
            if (offset < 0 || offset > size || bytes == 0 || data.data() == nullptr ||
                bytes > static_cast<std::uint64_t>(size - offset))
            {
                set_last_error(std::string(entry) + ": nonempty data and an in-bounds byte range are required");
                return false;
            }
            return true;
        }
    }

    std::int64_t gl_buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride)
    {
        const std::int32_t stored_stride = bind == 1 && stride == 0 ? 8 : stride;
        const bool vertex = bind == 1 && (stored_stride == 8 || stored_stride == 12 || stored_stride == 16);
        const bool storage = bind == 8 && stride >= 4 && stride <= 2048 && stride % 4 == 0;
        const bool other = (bind == 0 || bind == 2 || bind == 4 || bind == 16) && stride == 0;
        if (usage < 0 || usage > 2 || (usage == 2 ? bind != 0 : bind == 0) ||
            (!vertex && !storage && !other) || (bind == 16 && usage != 0) ||
            !valid_buffer_size(size, bind, stored_stride))
        {
            set_last_error("igpu_buffer_create: invalid size, usage, single bind role, or supported record stride");
            return 0;
        }
        auto& f = igpu_gl_fns();
        ArrayBufferBinding saved;
        GLuint id = 0;
        f.GenBuffers(1, &id);
        if (!id)
        {
            set_last_error("igpu_buffer_create: could not create a GL buffer");
            return 0;
        }
        // A buffer's storage is independent of its eventual bind role. ARRAY_BUFFER
        // allocation avoids changing the caller's VAO element-buffer attachment.
        f.BindBuffer(GL_ARRAY_BUFFER, id);
        f.BufferData(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(size), nullptr, buffer_hint(usage));
        if (!allocated_buffer_size(GL_ARRAY_BUFFER, size))
        {
            f.DeleteBuffers(1, &id);
            set_last_error("igpu_buffer_create: buffer allocation failed");
            return 0;
        }
        zero_buffer_range(GL_ARRAY_BUFFER, 0, size);
        const auto handle = g_next_buffer++;
        g_buffers.emplace(handle, BufferObject{id, size, stored_stride, bind,
            static_cast<GLenum>(bind == 2 ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER), usage});
        return static_cast<std::int64_t>(handle);
    }

    bool gl_buffer_write(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& data)
    {
        const auto it = g_buffers.find(buffer);
        if (it == g_buffers.end())
        {
            set_last_error("igpu_buffer_write: unknown buffer handle");
            return false;
        }
        const auto& entry = it->second;
        if (!buffer_range_valid(entry.size, offset, data, "igpu_buffer_write")) return false;
        if (entry.usage == 2 || (entry.bind == 16 && (offset != 0 || static_cast<std::uint64_t>(data.length()) != static_cast<std::uint64_t>(entry.size))))
        {
            set_last_error("igpu_buffer_write: Staging is read-only; an Indirect buffer requires a whole upload");
            return false;
        }
        ArrayBufferBinding saved;
        auto& f = igpu_gl_fns();
        f.BindBuffer(GL_ARRAY_BUFFER, entry.id);
        if (!buffer_unmapped(GL_ARRAY_BUFFER, "igpu_buffer_write")) return false;
        f.BufferSubData(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(offset),
                        static_cast<std::ptrdiff_t>(data.length()), data.data());
        return true;
    }

    bool gl_buffer_read(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& dest)
    {
        const auto it = g_buffers.find(buffer);
        if (it == g_buffers.end())
        {
            set_last_error("igpu_buffer_read: unknown buffer handle");
            return false;
        }
        const auto& entry = it->second;
        if (!buffer_range_valid(entry.size, offset, dest, "igpu_buffer_read")) return false;
        if (entry.usage != 2 && entry.bind != 8)
        {
            set_last_error("igpu_buffer_read: only Staging or Storage buffers support CPU readback");
            return false;
        }
        ArrayBufferBinding saved;
        auto& f = igpu_gl_fns();
        f.BindBuffer(GL_ARRAY_BUFFER, entry.id);
        if (!buffer_unmapped(GL_ARRAY_BUFFER, "igpu_buffer_read")) return false;
        f.GetBufferSubData(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(offset),
                           static_cast<std::ptrdiff_t>(dest.length()), dest.data());
        return true;
    }

    std::int64_t gl_buffer_size(std::uint64_t buffer)
    {
        const auto it = g_buffers.find(buffer);
        if (it != g_buffers.end()) return it->second.size;
        set_last_error("igpu_buffer_size: unknown buffer handle");
        return 0;
    }

    bool gl_buffer_resize(std::uint64_t buffer, std::int64_t size)
    {
        const auto it = g_buffers.find(buffer);
        if (it == g_buffers.end())
        {
            set_last_error("igpu_buffer_resize: unknown buffer handle");
            return false;
        }
        auto& entry = it->second;
        if (entry.usage != 1 || !valid_buffer_size(size, entry.bind, entry.stride))
        {
            set_last_error("igpu_buffer_resize: a Dynamic buffer and a valid positive aligned size are required");
            return false;
        }
        auto& f = igpu_gl_fns();
        if (!f.CopyBufferSubData)
        {
            set_last_error("igpu_buffer_resize: this OpenGL context lacks copy-buffer support");
            return false;
        }
        CopyBufferBindings saved;
        f.BindBuffer(copy_read_buffer, entry.id);
        if (!buffer_unmapped(copy_read_buffer, "igpu_buffer_resize")) return false;
        if (size == entry.size) return true;
        const std::int64_t prefix = size < entry.size ? size : entry.size;
        GLuint backup = 0;
        f.GenBuffers(1, &backup);
        if (!backup)
        {
            set_last_error("igpu_buffer_resize: could not create a GPU backup");
            return false;
        }
        struct Backup
        {
            GLuint name;
            ~Backup() { igpu_gl_fns().DeleteBuffers(1, &name); }
        } owned{backup};
        f.BindBuffer(copy_write_buffer, backup);
        f.BufferData(copy_write_buffer, static_cast<std::ptrdiff_t>(prefix), nullptr, 0x88E2 /* STREAM_COPY */);
        if (!allocated_buffer_size(copy_write_buffer, prefix))
        {
            set_last_error("igpu_buffer_resize: GPU backup allocation failed");
            return false;
        }
        f.CopyBufferSubData(copy_read_buffer, copy_write_buffer, 0, 0, static_cast<std::ptrdiff_t>(prefix));
        // Keep the native object, so VAO references and caller bindings stay valid.
        f.BindBuffer(copy_write_buffer, entry.id);
        f.BufferData(copy_write_buffer, static_cast<std::ptrdiff_t>(size), nullptr, buffer_hint(entry.usage));
        if (!allocated_buffer_size(copy_write_buffer, size))
        {
            set_last_error("igpu_buffer_resize: new allocation failed");
            return false;
        }
        f.BindBuffer(copy_read_buffer, backup);
        f.CopyBufferSubData(copy_read_buffer, copy_write_buffer, 0, 0, static_cast<std::ptrdiff_t>(prefix));
        zero_buffer_range(copy_write_buffer, prefix, size - prefix);
        entry.size = size;
        return true;
    }

    bool gl_is_vertex_buffer_bound(std::uint64_t buffer)
    {
        const auto found = g_buffers.find(buffer);
        if (found == g_buffers.end() || found->second.bind != 1) return false;
        GLint vao = 0; glGetIntegerv(0x85B5 /* VERTEX_ARRAY_BINDING */, &vao);
        if (!gl_vertex_array_has_attributes(static_cast<std::uint32_t>(vao))) return false;
        GLint source = 0;
        igpu_gl_fns().GetVertexAttribiv(0, 0x889F /* VERTEX_ATTRIB_ARRAY_BUFFER_BINDING */, &source);
        return static_cast<GLuint>(source) == found->second.id;
    }

    bool gl_buffer_release(std::uint64_t buffer)
    {
        const auto it = g_buffers.find(buffer);
        if (it == g_buffers.end())
        {
            set_last_error("igpu_buffer_release: unknown buffer handle");
            return false;
        }
        gl_uniform_release_buffer(buffer);
        gl_storage_release_buffer(buffer);
        igpu_gl_fns().DeleteBuffers(1, &it->second.id);
        g_buffers.erase(it);
        return true;
    }

    void gl_draw_capabilities_initialize()
    {
        g_depth_clamp_context = gl_context_at_least(3, 2) || gl_context_extension("GL_ARB_depth_clamp");
        g_instancing_context = gl_context_at_least(3, 3) ||
            (gl_context_extension("GL_ARB_draw_instanced") && gl_context_extension("GL_ARB_instanced_arrays"));
        g_indirect_context = (gl_context_at_least(4, 0) || gl_context_extension("GL_ARB_draw_indirect")) &&
            (gl_context_at_least(4, 2) || gl_context_extension("GL_ARB_base_instance"));
    }
    bool gl_instancing_supported()
    {
        const auto& f = igpu_gl_fns();
        return g_instancing_context && f.DrawArraysInstanced && f.VertexAttribDivisor;
    }
    bool gl_indirect_draw_supported()
    {
        const auto& f = igpu_gl_fns();
        return gl_instancing_supported() && g_indirect_context && f.DrawArraysIndirect && f.DrawElementsIndirect;
    }

    std::int64_t gl_input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                        const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step,
                                        std::int32_t element_count, std::int32_t vertex_stride,
                                        std::int32_t instance_stride)
    {
        auto element_is = [&](std::size_t index, std::int64_t expect_usage, std::int64_t expect_type,
                              std::int64_t expect_step = 0) {
            if (usage.size() <= index || type.size() <= index || step.size() <= index) return false;
            std::int64_t u = 0, t = 0, s = 0;
            return read_i64(usage[index], u) && read_i64(type[index], t) && read_i64(step[index], s) &&
                u == expect_usage && t == expect_type && s == expect_step;
        };
        const bool position = element_count == 1 && (vertex_stride == 0 || vertex_stride == 8) && element_is(0, 1, 2);
        const bool coloured = element_count == 2 && (vertex_stride == 0 || vertex_stride == 12) &&
                              element_is(0, 1, 2) && element_is(1, 2, 5);
        const bool textured = element_count == 2 && (vertex_stride == 0 || vertex_stride == 16) &&
                              element_is(0, 1, 2) && element_is(1, 4, 2);
        const bool instance_record = (instance_stride == 8 || instance_stride == 12 || instance_stride == 16) &&
                                    (vertex_stride == 0 || vertex_stride == 8) && element_count == 2 && element_is(0, 1, 2);
        const bool instance_colour = instance_record && element_is(1, 2, 5, 1);
        const bool instance_uv = instance_record && element_is(1, 4, 2, 1);
        const bool instanced = instance_colour || instance_uv;
        if (!shader || (!instanced && (!(position || coloured || textured) || instance_stride != 0)))
        {
            set_last_error("igpu_input_layout_create: the opengl backend accepts float2 position with optional colour/float2 texcoord; step-1 inputs require an 8/12/16-byte instance record");
            return 0;
        }
        if (instanced && !gl_instancing_supported())
        {
            set_last_error("igpu_input_layout_create: this OpenGL context does not support instanced attributes");
            return 0;
        }
        const auto handle = g_next_layout++;
        g_layouts.emplace(handle, LayoutObject{instanced ? 8 : (textured ? 16 : (coloured ? 12 : 8)),
                                              instanced ? instance_stride : 0, instance_colour});
        return static_cast<std::int64_t>(handle);
    }

    bool gl_input_layout_release(std::uint64_t layout)
    {
        return g_layouts.erase(layout) != 0;
    }

    bool gl_uniform_buffer(std::uint64_t handle, GlUniformBuffer& out, const char* entry)
    {
        const auto found = g_buffers.find(handle);
        if (found == g_buffers.end() || found->second.bind != 4)
        {
            set_last_error(std::string(entry) + ": the buffer needs IgpuBufferBind.Uniform");
            return false;
        }
        ArrayBufferBinding saved;
        igpu_gl_fns().BindBuffer(GL_ARRAY_BUFFER, found->second.id);
        if (!buffer_unmapped(GL_ARRAY_BUFFER, entry)) return false;
        out = {found->second.id, found->second.size};
        return true;
    }
    bool gl_uniform_patch(std::uint64_t buffer, std::int64_t offset, const void* data, std::size_t size, const char* entry)
    {
        GlUniformBuffer resolved;
        if (!gl_uniform_buffer(buffer, resolved, entry)) return false;
        if (!data || size == 0 || offset < 0 || offset > resolved.bytes || size > static_cast<std::uint64_t>(resolved.bytes-offset))
        {
            set_last_error(std::string(entry) + ": the member does not fit in the uniform buffer");
            return false;
        }
        ArrayBufferBinding saved;
        auto& f = igpu_gl_fns();f.BindBuffer(GL_ARRAY_BUFFER, resolved.name);
        f.BufferSubData(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(offset), static_cast<std::ptrdiff_t>(size), data);
        return true;
    }

    bool gl_storage_buffer(std::uint64_t handle, GlStorageBuffer& out, const char* entry)
    {
        const auto found = g_buffers.find(handle);
        if (found == g_buffers.end() || found->second.bind != 8)
        {
            set_last_error(std::string(entry) + ": the buffer needs IgpuBufferBind.Storage");
            return false;
        }
        ArrayBufferBinding saved;
        igpu_gl_fns().BindBuffer(GL_ARRAY_BUFFER, found->second.id);
        if (!buffer_unmapped(GL_ARRAY_BUFFER, entry)) return false;
        out = {found->second.id, found->second.size};
        return true;
    }
    bool gl_target_vertex_range(std::uint64_t vertex,std::int64_t first,std::int64_t&count)
    {
        const auto found=g_buffers.find(vertex);
        if(found==g_buffers.end()||found->second.bind!=1||found->second.stride<=0)
        {set_last_error("igpu_draw_to_render_targets: a valid Vertex buffer is required");return false;}
        const auto&b=found->second;const auto total=b.size/b.stride;
        if(first<0||first>total){set_last_error("igpu_draw_to_render_targets: vertex range is outside the buffer");return false;}
        if(count<0)count=total-first;
        if(count<=0||count>total-first||first>(std::numeric_limits<GLint>::max)()||count>(std::numeric_limits<GLsizei>::max)())
        {set_last_error("igpu_draw_to_render_targets: vertex range is outside the buffer");return false;}
        GLint previous=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);
        igpu_gl_fns().BindBuffer(GL_ARRAY_BUFFER,b.id);const bool ok=buffer_unmapped(GL_ARRAY_BUFFER,"igpu_draw_to_render_targets");
        igpu_gl_fns().BindBuffer(GL_ARRAY_BUFFER,static_cast<GLuint>(previous));return ok;
    }

    bool gl_draw_to_render_targets_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                         std::int64_t first_vertex, std::int64_t vertex_count,
                                         const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers,
                                         const gm::wire::GMArrayView& mips, std::int64_t blend_state,
                                         std::int64_t depth_state, std::int64_t raster_state,
                                         std::int64_t sampler_state)
    {
        return gl_draw_targets(vertex_buffer,layout,primitive,first_vertex,vertex_count,targets,&layers,&mips,
                               blend_state,depth_state,raster_state,sampler_state);
    }

    static bool gl_draw_impl(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                 std::int64_t first_vertex, std::int64_t vertex_count, std::int64_t blend_state,
                 std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state, bool patch)
    {
        GLenum mode = GL_TRIANGLES;
        const BlendObject* blend = nullptr;
        const DepthObject* depth = nullptr;
        const RasterObject* raster = nullptr;
        if (!gl_draw_accepted("igpu_draw", primitive, blend_state, depth_state, raster_state, sampler_state, blend,
                              depth, raster, mode, patch))
        {
            return false;
        }
        if (patch) mode = 0x000E /* PATCHES */;
        const auto vertex = g_buffers.find(vertex_buffer);
        if (vertex == g_buffers.end())
        {
            set_last_error("igpu_draw: unknown buffer or layout");
            return false;
        }
        const auto layout_it = g_layouts.find(layout);
        if (layout_it == g_layouts.end() || (layout_it->second.vertex_stride != vertex->second.stride || layout_it->second.instance_stride != 0))
        {
            set_last_error("igpu_draw: the vertex buffer stride does not match the input layout");
            return false;
        }
        if (vertex->second.bind != 1 || vertex->second.stride <= 0)
        {
            set_last_error("igpu_draw: the vertex buffer was not created with IgpuBufferBind.Vertex");
            return false;
        }
        const std::int64_t vertex_total = vertex->second.size / vertex->second.stride;
        if (vertex_count < 0)
        {
            vertex_count = vertex_total - first_vertex;
        }
        if (first_vertex < 0 || vertex_count <= 0)
        {
            set_last_error("igpu_draw: the resolved vertex count is zero");
            return false;
        }
        if (first_vertex + vertex_count > vertex_total)
        {
            set_last_error("igpu_draw: the vertex range is outside the buffer");
            return false;
        }
        GLint program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        if (program == 0)
        {
            set_last_error("igpu_draw: no shader program is bound");
            return false;
        }

        const auto& fns = igpu_gl_fns();
        if (g_vertex_array == 0)
        {
            fns.GenVertexArrays(1, &g_vertex_array);
        }
        GLint previous_array = 0;
        GLint previous_vertex_array = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array);
        glGetIntegerv(0x85B5 /* VERTEX_ARRAY_BINDING */, &previous_vertex_array);
        fns.BindVertexArray(g_vertex_array);
        fns.BindBuffer(GL_ARRAY_BUFFER, vertex->second.id);
        bind_vertices(vertex->second, 0);
        SavedBlend saved_blend{};
        SavedDepth saved_depth{};
        SavedCull saved_cull{};
        if (blend != nullptr)
        {
            save_blend(saved_blend);
            apply_blend(*blend);
        }
        if (depth != nullptr)
        {
            save_depth(saved_depth);
            apply_depth(*depth);
        }
        if (raster != nullptr)
        {
            save_cull(saved_cull);
            apply_cull(*raster);
        }
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
        if (raster != nullptr)
        {
            restore_cull(saved_cull);
        }
        if (depth != nullptr)
        {
            restore_depth(saved_depth);
        }
        if (blend != nullptr)
        {
            restore_blend(saved_blend);
        }
        unbind_vertices();
        fns.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array));
        fns.BindVertexArray(static_cast<GLuint>(previous_vertex_array));
        return true;
    }

    bool gl_draw(std::uint64_t vertex,std::uint64_t layout,std::int32_t primitive,std::int64_t first,std::int64_t count,
                 std::int64_t blend,std::int64_t depth,std::int64_t raster,std::int64_t sampler)
    {return gl_draw_impl(vertex,layout,primitive,first,count,blend,depth,raster,sampler,false);}
    bool gl_draw_patch(std::uint64_t vertex,std::uint64_t layout,std::int32_t points,std::int64_t first,std::int64_t count,
                       std::int64_t blend,std::int64_t depth,std::int64_t raster,std::int64_t sampler)
    {
        if(!gl_target_vertex_range(vertex,first,count))return false;
        if(points<1||points>32||count%points){set_last_error("igpu_draw_patch: control points must be1..32 and vertex count must contain whole patches");return false;}
        std::int32_t previous=0;if(!gl_patch_begin(points,previous))return false;
        struct Patch{std::int32_t previous;~Patch(){gl_patch_end(previous);}}saved{previous};
        return gl_draw_impl(vertex,layout,4,first,count,blend,depth,raster,sampler,true);
    }

    namespace
    {
        struct AdvancedDraw
        {
            std::uint64_t vertex = 0, instances = 0, layout = 0, index = 0, args = 0;
            std::int64_t first = 0, count = 0, instance_count = 0, offset = 0;
            std::int32_t primitive = 0;
            bool indirect = false, indexed = false;
        };

        bool draw_advanced(const char* entry, AdvancedDraw draw, std::int64_t blend_state,
                           std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state)
        {
            const auto fail = [entry](const char* message) { set_last_error(std::string(entry) + ": " + message); return false; };
            if (draw.indirect ? !gl_indirect_draw_supported() : !gl_instancing_supported())
                return fail("this OpenGL context does not support this draw path");
            GLenum mode = GL_TRIANGLES;
            const BlendObject* blend = nullptr;
            const DepthObject* depth = nullptr;
            const RasterObject* raster = nullptr;
            if (!gl_draw_accepted(entry, draw.primitive, blend_state, depth_state, raster_state, sampler_state,
                                  blend, depth, raster, mode)) return false;
            const auto vertex = g_buffers.find(draw.vertex);
            const auto layout = g_layouts.find(draw.layout);
            if (vertex == g_buffers.end() || layout == g_layouts.end()) return fail("unknown vertex buffer or input layout");
            if (vertex->second.bind != 1 || vertex->second.stride <= 0) return fail("the vertex buffer needs IgpuBufferBind.Vertex and a stride");
            const auto& input = layout->second;
            if (input.vertex_stride != vertex->second.stride) return fail("the vertex buffer stride does not match the input layout");
            const BufferObject* instances = nullptr;
            if (draw.instances)
            {
                const auto found = g_buffers.find(draw.instances);
                if (found == g_buffers.end()) return fail("unknown instance buffer handle");
                instances = &found->second;
                if (instances->bind != 1 || instances->stride <= 0) return fail("the instance buffer needs IgpuBufferBind.Vertex and a stride");
            }
            if (input.instance_stride && (!instances || instances->stride != input.instance_stride))
                return fail("the instance buffer stride does not match the input layout");
            const BufferObject* index = nullptr;
            const BufferObject* arguments = nullptr;
            if (draw.indirect)
            {
                const auto found = g_buffers.find(draw.args);
                if (found == g_buffers.end() || found->second.bind != 16) return fail("the argument buffer needs IgpuBufferBind.Indirect");
                arguments = &found->second;
                const std::int64_t record = draw.indexed ? 20 : 16;
                if (arguments->size < record || draw.offset < 0 || draw.offset % 4 || draw.offset > arguments->size - record)
                    return fail("the argument record does not fit in the buffer");
                if (draw.indexed)
                {
                    const auto found_index = g_buffers.find(draw.index);
                    if (!draw.index || found_index == g_buffers.end() || found_index->second.bind != 2)
                        return fail("a 16-bit IgpuBufferBind.Index buffer is required");
                    index = &found_index->second;
                }
            }
            else
            {
                const auto total = vertex->second.size / vertex->second.stride;
                if (draw.first < 0 || draw.first >= total) return fail("the first vertex is outside the buffer");
                if (draw.count < 0) draw.count = total - draw.first;
                if (draw.count <= 0 || draw.count > total - draw.first) return fail("the vertex range is outside the buffer");
                if (draw.instance_count <= 0 || draw.instance_count > (std::numeric_limits<GLsizei>::max)())
                    return fail("instance_count must be a positive OpenGL draw count");
                if (instances && draw.instance_count > instances->size / instances->stride)
                    return fail("instance_count exceeds the instance buffer");
            }
            GLint program = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            if (!program) return fail("no shader program is bound");
            const auto& f = igpu_gl_fns();
            {
                ArrayBufferBinding saved;
                const BufferObject* buffers[] = { &vertex->second, instances, index, arguments };
                for (const auto buffer : buffers)
                {
                    if (!buffer) continue;
                    f.BindBuffer(GL_ARRAY_BUFFER, buffer->id);
                    if (!buffer_unmapped(GL_ARRAY_BUFFER, entry)) return false;
                }
            }
            if (!g_vertex_array)
            {
                f.GenVertexArrays(1, &g_vertex_array);
                if (!g_vertex_array) return fail("could not allocate the draw vertex array");
            }
            GLint saved_array = 0, saved_vao = 0, saved_index = 0, saved_indirect = 0;
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &saved_array);
            glGetIntegerv(0x85B5 /* VERTEX_ARRAY_BINDING */, &saved_vao);
            if (draw.indirect) glGetIntegerv(0x8F43 /* DRAW_INDIRECT_BUFFER_BINDING */, &saved_indirect);
            f.BindVertexArray(g_vertex_array);
            if (draw.indexed) glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &saved_index);
            f.BindBuffer(GL_ARRAY_BUFFER, vertex->second.id);
            f.VertexAttribDivisor(0, 0);
            f.VertexAttribDivisor(1, 0);
            bind_vertices(vertex->second, 0);
            if (input.instance_stride)
            {
                f.BindBuffer(GL_ARRAY_BUFFER, instances->id);
                f.EnableVertexAttribArray(1);
                f.VertexAttribPointer(1, input.instance_colour ? 4 : 2, input.instance_colour ? GL_UNSIGNED_BYTE : GL_FLOAT,
                                      input.instance_colour ? GL_TRUE : GL_FALSE, input.instance_stride, nullptr);
                f.VertexAttribDivisor(1, 1);
            }
            if (draw.indexed) f.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index->id);
            if (draw.indirect) f.BindBuffer(0x8F3F /* DRAW_INDIRECT_BUFFER */, arguments->id);
            SavedBlend saved_blend{};
            SavedDepth saved_depth{};
            SavedCull saved_cull{};
            if (blend) { save_blend(saved_blend); apply_blend(*blend); }
            if (depth) { save_depth(saved_depth); apply_depth(*depth); }
            if (raster) { save_cull(saved_cull); apply_cull(*raster); }
            if (!draw.indirect)
                f.DrawArraysInstanced(mode, static_cast<GLint>(draw.first), static_cast<GLsizei>(draw.count),
                                      static_cast<GLsizei>(draw.instance_count));
            else
            {
                const auto offset = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(draw.offset));
                if (draw.indexed) f.DrawElementsIndirect(mode, GL_UNSIGNED_SHORT, offset);
                else f.DrawArraysIndirect(mode, offset);
            }
            if (raster) restore_cull(saved_cull);
            if (depth) restore_depth(saved_depth);
            if (blend) restore_blend(saved_blend);
            f.VertexAttribDivisor(1, 0);
            unbind_vertices();
            if (draw.indexed) f.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(saved_index));
            if (draw.indirect) f.BindBuffer(0x8F3F, static_cast<GLuint>(saved_indirect));
            f.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(saved_array));
            f.BindVertexArray(static_cast<GLuint>(saved_vao));
            return true;
        }
    }

    bool gl_draw_instanced(std::uint64_t vertex, std::uint64_t instances, std::uint64_t layout,
                          std::int32_t primitive, std::int64_t first, std::int64_t count, std::int64_t instance_count,
                          std::int64_t blend, std::int64_t depth, std::int64_t raster, std::int64_t sampler)
    {
        AdvancedDraw draw;
        draw.vertex = vertex; draw.instances = instances; draw.layout = layout; draw.primitive = primitive;
        draw.first = first; draw.count = count; draw.instance_count = instance_count;
        return draw_advanced("igpu_draw_instanced", draw, blend, depth, raster, sampler);
    }
    bool gl_draw_indirect_record(std::uint64_t vertex, std::uint64_t instances, std::uint64_t layout,
                                std::uint64_t index, bool indexed, std::int32_t primitive, std::uint64_t args,
                                std::int64_t offset, std::int64_t blend, std::int64_t depth,
                                std::int64_t raster, std::int64_t sampler)
    {
        AdvancedDraw draw;
        draw.vertex = vertex; draw.instances = instances; draw.layout = layout; draw.index = index;
        draw.args = args; draw.offset = offset; draw.primitive = primitive; draw.indirect = true; draw.indexed = indexed;
        return draw_advanced(indexed ? "igpu_draw_indexed_indirect" : "igpu_draw_indirect", draw, blend, depth, raster, sampler);
    }

    bool gl_draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer,
                         std::int32_t primitive, std::int64_t first_index, std::int64_t index_count,
                         std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                         std::int64_t sampler_state)
    {
        GLenum mode = GL_TRIANGLES;
        const BlendObject* blend = nullptr;
        const DepthObject* depth = nullptr;
        const RasterObject* raster = nullptr;
        if (!gl_draw_accepted("igpu_draw_indexed", primitive, blend_state, depth_state, raster_state, sampler_state,
                              blend, depth, raster, mode))
        {
            return false;
        }
        const auto vertex = g_buffers.find(vertex_buffer);
        const auto index = g_buffers.find(index_buffer);
        if (vertex == g_buffers.end() || index == g_buffers.end())
        {
            set_last_error("igpu_draw_indexed: unknown buffer or layout");
            return false;
        }
        const auto layout_it = g_layouts.find(layout);
        if (layout_it == g_layouts.end() || (layout_it->second.vertex_stride != vertex->second.stride || layout_it->second.instance_stride != 0))
        {
            set_last_error("igpu_draw_indexed: the vertex buffer stride does not match the input layout");
            return false;
        }
        if (vertex->second.bind != 1)
        {
            set_last_error("igpu_draw_indexed: the vertex buffer was not created with IgpuBufferBind.Vertex");
            return false;
        }
        if (index->second.bind != 2)
        {
            set_last_error("igpu_draw_indexed: the index buffer was not created with IgpuBufferBind.Index");
            return false;
        }
        constexpr std::int64_t kIndexSize = 2;
        const std::int64_t index_total = index->second.size / kIndexSize;
        if (index_count < 0)
        {
            index_count = index_total - first_index;
        }
        if (first_index < 0 || index_count <= 0)
        {
            set_last_error("igpu_draw_indexed: the resolved index count is zero");
            return false;
        }
        if (first_index + index_count > index_total)
        {
            set_last_error("igpu_draw_indexed: the index range is outside the buffer");
            return false;
        }
        GLint program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        if (program == 0)
        {
            set_last_error("igpu_draw_indexed: no shader program is bound");
            return false;
        }

        const auto& fns = igpu_gl_fns();
        if (g_vertex_array == 0)
        {
            fns.GenVertexArrays(1, &g_vertex_array);
        }
        GLint previous_array = 0;
        GLint previous_vertex_array = 0;
        GLint previous_element = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array);
        glGetIntegerv(0x85B5 /* VERTEX_ARRAY_BINDING */, &previous_vertex_array);
        fns.BindVertexArray(g_vertex_array);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &previous_element);
        fns.BindBuffer(GL_ARRAY_BUFFER, vertex->second.id);
        bind_vertices(vertex->second, 0);
        fns.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index->second.id);
        SavedBlend saved_blend{};
        SavedDepth saved_depth{};
        SavedCull saved_cull{};
        if (blend != nullptr)
        {
            save_blend(saved_blend);
            apply_blend(*blend);
        }
        if (depth != nullptr)
        {
            save_depth(saved_depth);
            apply_depth(*depth);
        }
        if (raster != nullptr)
        {
            save_cull(saved_cull);
            apply_cull(*raster);
        }
        glDrawElements(mode, static_cast<GLsizei>(index_count), GL_UNSIGNED_SHORT,
                       reinterpret_cast<const void*>(static_cast<std::uintptr_t>(first_index * kIndexSize)));
        if (raster != nullptr)
        {
            restore_cull(saved_cull);
        }
        if (depth != nullptr)
        {
            restore_depth(saved_depth);
        }
        if (blend != nullptr)
        {
            restore_blend(saved_blend);
        }
        unbind_vertices();
        fns.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array));
        fns.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(previous_element));
        fns.BindVertexArray(static_cast<GLuint>(previous_vertex_array));
        return true;
    }

    bool gl_draw_sampled(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                         std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                         std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                         std::int64_t sampler_state)
    {
        GLenum mode = GL_TRIANGLES;
        const BlendObject* blend = nullptr;
        const DepthObject* depth = nullptr;
        const RasterObject* raster = nullptr;
        if (!gl_uniform_validate_inputs(0, 1, "igpu_draw_sampled") || !gl_uniform_validate_inputs(3, 5, "igpu_draw_sampled")) return false;
        if (!gl_pipeline_accepted("igpu_draw_sampled", primitive, mode))
        {
            return false;
        }
        if (!gl_graphics_draw_allowed(mode, "igpu_draw_sampled")) return false;
        if (!gl_resolve_blend("igpu_draw_sampled", blend_state, blend) ||
            !gl_resolve_depth("igpu_draw_sampled", depth_state, depth) ||
            !gl_resolve_raster("igpu_draw_sampled", raster_state, raster))
        {
            return false;
        }
        auto* source = gl_find_texture(texture);
        if (!source)
        {
            set_last_error("igpu_draw_sampled: unknown texture handle");
            return false;
        }
        const auto vertex = g_buffers.find(vertex_buffer);
        if (vertex == g_buffers.end())
        {
            set_last_error("igpu_draw_sampled: unknown buffer or layout");
            return false;
        }
        const auto layout_it = g_layouts.find(layout);
        if (layout_it == g_layouts.end() || (layout_it->second.vertex_stride != vertex->second.stride || layout_it->second.instance_stride != 0))
        {
            set_last_error("igpu_draw_sampled: the vertex buffer stride does not match the input layout");
            return false;
        }
        if (vertex->second.bind != 1 || vertex->second.stride <= 0)
        {
            set_last_error("igpu_draw_sampled: the vertex buffer was not created with IgpuBufferBind.Vertex");
            return false;
        }
        const std::int64_t vertex_total = vertex->second.size / vertex->second.stride;
        if (vertex_count < 0)
        {
            vertex_count = vertex_total - first_vertex;
        }
        if (first_vertex < 0 || vertex_count <= 0)
        {
            set_last_error("igpu_draw_sampled: the resolved vertex count is zero");
            return false;
        }
        if (first_vertex + vertex_count > vertex_total)
        {
            set_last_error("igpu_draw_sampled: the vertex range is outside the buffer");
            return false;
        }
        GLint program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        if (program == 0)
        {
            set_last_error("igpu_draw_sampled: no shader program is bound");
            return false;
        }
        GLint draw_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &draw_fbo);
        if (static_cast<GLuint>(draw_fbo) == source->framebuffer)
        {
            set_last_error("igpu_draw_sampled: the texture is the current color target");
            return false;
        }
        const auto& fns = igpu_gl_fns();
        const GLint sampler = fns.GetUniformLocation(static_cast<GLuint>(program), "igpu_tex");
        if (sampler < 0)
        {
            set_last_error("igpu_draw_sampled: the bound program has no sampler uniform 'igpu_tex'");
            return false;
        }
        std::int32_t address = 0;
        std::int32_t address_v = 0;
        std::int32_t address_w = 0;
        bool linear = false;
        bool linear_min = false;
        bool mip_linear = false;
        float level_offset = 0.f;
        float finest = 0.f;
        float coarsest = 3.402823466e+38f;
        bool adjust_min = false;
        bool adjust_bias = false;
        bool adjust_min_lod = false;
        bool adjust_max_lod = false;
        std::int32_t anisotropy = 1;
        GLenum comparison = GL_NONE;
        GLfloat border_color[4] = {0.f, 0.f, 0.f, 1.f};
        if (sampler_state != 0)
        {
            const auto sampler_it = g_samplers.find(static_cast<std::uint64_t>(sampler_state));
            if (sampler_it == g_samplers.end())
            {
                set_last_error("igpu_draw_sampled: unknown sampler handle");
                return false;
            }
            address = sampler_it->second.address;
            address_v = sampler_it->second.address_v;
            address_w = sampler_it->second.address_w;
            linear = sampler_it->second.linear;
            linear_min = sampler_it->second.linear_min;
            mip_linear = sampler_it->second.linear_mip;
            level_offset = sampler_it->second.level_offset;
            finest = sampler_it->second.finest;
            coarsest = sampler_it->second.coarsest;
            anisotropy = sampler_it->second.anisotropy;
            comparison = sampler_it->second.compare;
            border_color[0] = sampler_it->second.border_red;
            border_color[1] = sampler_it->second.border_green;
            border_color[2] = sampler_it->second.border_blue;
            border_color[3] = sampler_it->second.border_alpha;
        }
        GLenum sampler_type = GL_NONE;
        GLint uniform_count = 0;
        fns.GetProgramiv(static_cast<GLuint>(program), 0x8B86 /* ACTIVE_UNIFORMS */, &uniform_count);
        for (GLint index = 0; index < uniform_count; ++index)
        {
            char name[64] = {};
            GLsizei length = 0;
            GLint count = 0;
            GLenum type = GL_NONE;
            fns.GetActiveUniform(static_cast<GLuint>(program), index, sizeof(name), &length, &count, &type, name);
            if (std::strcmp(name, "igpu_tex") == 0 && count == 1)
            {
                sampler_type = type;
                break;
            }
        }
        const bool comparing = comparison != GL_NONE;
        const GLenum ordinary_type=source->kind==1?0x8B5Fu:source->kind==2?0x8DC1u:source->kind==3?0x8B60u:0x8B5Eu;
        if ((comparing && source->kind != 0) || sampler_type != (comparing ? 0x8B62u /* SAMPLER_2D_SHADOW */ : ordinary_type))
        {
            set_last_error("igpu_draw_sampled: igpu_tex must match the native texture kind; comparison requires 2D sampler2DShadow");
            return false;
        }
        if (comparing && source->format != 9 && source->format != 10)
        {
            set_last_error("igpu_draw_sampled: comparison sampling requires an R16F or R32F texture");
            return false;
        }
        struct TextureUnit
        {
            GLint previous=0;
            TextureUnit(){glGetIntegerv(0x84E0,&previous);igpu_gl_fns().ActiveTexture(0x84C0);}
            ~TextureUnit(){igpu_gl_fns().ActiveTexture(static_cast<GLenum>(previous));}
        } texture_unit;
        if (comparing && !gl_shadow_sync(source->texture, source->width, source->height,
                                         source->mips, source->shadow))
        {
            return false;
        }
        if (g_vertex_array == 0)
        {
            fns.GenVertexArrays(1, &g_vertex_array);
        }
        GLint previous_array = 0;
        GLint previous_vertex_array = 0;
        GLint previous_texture = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array);
        glGetIntegerv(0x85B5 /* VERTEX_ARRAY_BINDING */, &previous_vertex_array);
        glGetIntegerv(source->binding, &previous_texture);
        glBindTexture(source->target, comparing ? source->shadow.texture : source->texture);
        GLint previous_wrap_s = GL_CLAMP_TO_EDGE;
        GLint previous_wrap_t = GL_CLAMP_TO_EDGE;
        GLint previous_wrap_r = GL_CLAMP_TO_EDGE;
        GLint previous_mag = GL_NEAREST;
        GLint previous_min = GL_NEAREST;
        GLfloat previous_border[4] = {0.f, 0.f, 0.f, 0.f};
        GLfloat previous_bias = 0.f;
        GLfloat previous_min_lod = -1000.f;
        GLfloat previous_max_lod = 1000.f;
        GLfloat previous_aniso = 1.f;
        const bool border = address == 3;
        const bool limit_coarsest = coarsest < 3.402823466e+38f;
        const bool select_mip = comparing || level_offset != 0.f || finest != 0.f || limit_coarsest;
        const bool adjust_aniso = anisotropy > 1;
        if (comparing)
        {
            glTexParameteri(source->target, 0x884C /* TEXTURE_COMPARE_MODE */, 0x884E /* COMPARE_REF_TO_TEXTURE */);
            glTexParameteri(source->target, 0x884D /* TEXTURE_COMPARE_FUNC */, comparison);
        }
        if (sampler_state != 0)
        {
            glGetTexParameteriv(source->target, GL_TEXTURE_WRAP_S, &previous_wrap_s);
            glGetTexParameteriv(source->target, GL_TEXTURE_WRAP_T, &previous_wrap_t);
            glTexParameteri(source->target, GL_TEXTURE_WRAP_S, gl_wrap(address));
            glTexParameteri(source->target, GL_TEXTURE_WRAP_T, gl_wrap(address_v));
            if (source->kind != 0)
            {
                glGetTexParameteriv(source->target, 0x8072 /* TEXTURE_WRAP_R */, &previous_wrap_r);
                glTexParameteri(source->target, 0x8072, gl_wrap(address_w));
            }
            if (border)
            {
                glGetTexParameterfv(source->target, GL_TEXTURE_BORDER_COLOR, previous_border);
                glTexParameterfv(source->target, GL_TEXTURE_BORDER_COLOR, border_color);
            }
            if (linear)
            {
                glGetTexParameteriv(source->target, GL_TEXTURE_MAG_FILTER, &previous_mag);
                glTexParameteri(source->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            if (linear_min && !select_mip)
            {
                glGetTexParameteriv(source->target, GL_TEXTURE_MIN_FILTER, &previous_min);
                glTexParameteri(source->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                adjust_min = true;
            }
            if (select_mip)
            {
                glGetTexParameteriv(source->target, GL_TEXTURE_MIN_FILTER, &previous_min);
                glTexParameteri(source->target, GL_TEXTURE_MIN_FILTER,
                                linear_min ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST);
                adjust_min = true;
            }
            if (mip_linear)
            {
                if (!adjust_min)
                {
                    glGetTexParameteriv(source->target, GL_TEXTURE_MIN_FILTER, &previous_min);
                }
                glTexParameteri(source->target, GL_TEXTURE_MIN_FILTER,
                                linear_min ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
                adjust_min = true;
            }
            // An anisotropic state specifies every LOD field, including defaults.
            if (comparing || adjust_aniso || level_offset != 0.f)
            {
                glGetTexParameterfv(source->target, GL_TEXTURE_LOD_BIAS, &previous_bias);
                glTexParameterf(source->target, GL_TEXTURE_LOD_BIAS, level_offset);
                adjust_bias = true;
            }
            if (comparing || adjust_aniso || finest != 0.f)
            {
                glGetTexParameterfv(source->target, GL_TEXTURE_MIN_LOD, &previous_min_lod);
                glTexParameterf(source->target, GL_TEXTURE_MIN_LOD, finest);
                adjust_min_lod = true;
            }
            if (comparing || adjust_aniso || limit_coarsest)
            {
                glGetTexParameterfv(source->target, GL_TEXTURE_MAX_LOD, &previous_max_lod);
                glTexParameterf(source->target, GL_TEXTURE_MAX_LOD, coarsest);
                adjust_max_lod = true;
            }
            if (adjust_aniso)
            {
                glGetTexParameterfv(source->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, &previous_aniso);
                glTexParameterf(source->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, static_cast<GLfloat>(anisotropy));
            }
        }
        fns.Uniform1i(sampler, 0);
        fns.BindVertexArray(g_vertex_array);
        fns.BindBuffer(GL_ARRAY_BUFFER, vertex->second.id);
        bind_vertices(vertex->second, 0);
        SavedBlend saved_blend{};
        SavedDepth saved_depth{};
        SavedCull saved_cull{};
        if (blend != nullptr)
        {
            save_blend(saved_blend);
            apply_blend(*blend);
        }
        if (depth != nullptr)
        {
            save_depth(saved_depth);
            apply_depth(*depth);
        }
        if (raster != nullptr)
        {
            save_cull(saved_cull);
            apply_cull(*raster);
        }
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
        if (raster != nullptr)
        {
            restore_cull(saved_cull);
        }
        if (depth != nullptr)
        {
            restore_depth(saved_depth);
        }
        if (blend != nullptr)
        {
            restore_blend(saved_blend);
        }
        unbind_vertices();
        fns.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array));
        fns.BindVertexArray(static_cast<GLuint>(previous_vertex_array));
        if (sampler_state != 0)
        {
            glTexParameteri(source->target, GL_TEXTURE_WRAP_S, previous_wrap_s);
            glTexParameteri(source->target, GL_TEXTURE_WRAP_T, previous_wrap_t);
            if (source->kind != 0) glTexParameteri(source->target, 0x8072, previous_wrap_r);
            if (border)
            {
                glTexParameterfv(source->target, GL_TEXTURE_BORDER_COLOR, previous_border);
            }
            if (linear)
            {
                glTexParameteri(source->target, GL_TEXTURE_MAG_FILTER, previous_mag);
            }
            if (adjust_bias)
            {
                glTexParameterf(source->target, GL_TEXTURE_LOD_BIAS, previous_bias);
            }
            if (adjust_min_lod)
            {
                glTexParameterf(source->target, GL_TEXTURE_MIN_LOD, previous_min_lod);
            }
            if (adjust_max_lod)
            {
                glTexParameterf(source->target, GL_TEXTURE_MAX_LOD, previous_max_lod);
            }
            if (adjust_min)
            {
                glTexParameteri(source->target, GL_TEXTURE_MIN_FILTER, previous_min);
            }
            if (adjust_aniso)
            {
                glTexParameterf(source->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, previous_aniso);
            }
        }
        glBindTexture(source->target, static_cast<GLuint>(previous_texture));
        return true;
    }

    namespace
    {
        bool gl_anisotropy_accepted(std::int32_t anisotropy)
        {
            if (anisotropy == 1)
            {
                return true;
            }
            if (anisotropy < 1 || anisotropy > 16)
            {
                set_last_error("igpu_sampler_state_create: anisotropy must be in 1..16");
                return false;
            }
            // Check this context rather than caching capabilities across binds.
            // GL 4.6 makes anisotropic filtering core; earlier contexts advertise
            // ARB or EXT support. Indexed extension queries keep core profiles
            // from generating GL_INVALID_ENUM on GL_EXTENSIONS.
            int major = 0;
            int minor = 0;
            const auto version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
            if (version != nullptr)
            {
                std::sscanf(version, "%d.%d", &major, &minor);
            }
            bool supported = major > 4 || (major == 4 && minor >= 6);
            if (!supported && major >= 3)
            {
                using GetStringi = const GLubyte*(APIENTRY*)(GLenum, GLuint);
                const PROC entry = wglGetProcAddress("glGetStringi");
                if (entry != nullptr && entry != reinterpret_cast<PROC>(1) && entry != reinterpret_cast<PROC>(2) &&
                    entry != reinterpret_cast<PROC>(3) && entry != reinterpret_cast<PROC>(-1))
                {
                    const auto get_string_i = reinterpret_cast<GetStringi>(entry);
                    GLint count = 0;
                    glGetIntegerv(0x821D /* GL_NUM_EXTENSIONS */, &count);
                    for (GLint i = 0; i < count && !supported; ++i)
                    {
                        const auto extension = reinterpret_cast<const char*>(get_string_i(GL_EXTENSIONS, i));
                        supported = extension != nullptr &&
                            (std::strcmp(extension, "GL_EXT_texture_filter_anisotropic") == 0 ||
                             std::strcmp(extension, "GL_ARB_texture_filter_anisotropic") == 0);
                    }
                }
            }
            else if (!supported)
            {
                const auto extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
                const std::string names = extensions == nullptr ? "" : std::string(" ") + extensions + " ";
                supported = names.find(" GL_EXT_texture_filter_anisotropic ") != std::string::npos ||
                            names.find(" GL_ARB_texture_filter_anisotropic ") != std::string::npos;
            }
            if (!supported)
            {
                set_last_error("igpu_sampler_state_create: this OpenGL context does not support anisotropic filtering");
                return false;
            }
            GLfloat maximum = 1.f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
            if (static_cast<GLfloat>(anisotropy) > maximum)
            {
                set_last_error("igpu_sampler_state_create: anisotropy exceeds this OpenGL context's maximum");
                return false;
            }
            return true;
        }
    }

    std::int64_t gl_sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                       std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                       std::int32_t address_w, float level_offset, float finest,
                                                       float coarsest, std::int32_t anisotropy)
    {
        // Magnification and minification may each be nearest or linear.
        // Linear mip filtering blends neighboring levels. Nearest
        // minification stays on one texel in the chosen level. Linear
        // minification blends texels inside that level. Either is accepted
        // with any level offset, any finest level, and any coarsest level.
        // The offset is added to the shader LOD. A finest level above 0
        // raises the selection. A coarsest level below the unlimited
        // sentinel lowers it. U, V, and W may differ; all are retained.
        const bool mip_linear = mip == 1;
        if ((mip != 0 && !mip_linear) || (magnification != 0 && magnification != 1) ||
            (minification != 0 && minification != 1))
        {
            set_last_error("igpu_sampler_state_create: the opengl backend only samples the nearest texel");
            return 0;
        }
        const bool known_u = address_u == 0 || address_u == 1 || address_u == 2;
        const bool known_v = address_v == 0 || address_v == 1 || address_v == 2;
        const bool known_w = address_w == 0 || address_w == 1 || address_w == 2;
        if (!known_u || !known_v || !known_w)
        {
            set_last_error("igpu_sampler_state_create: the opengl backend only accepts clamp, repeat, or mirror on every axis");
            return 0;
        }
        if (finest < 0.f || !(coarsest >= 0.f))
        {
            set_last_error("igpu_sampler_state_create: the opengl backend has no mip chain");
            return 0;
        }
        if (!gl_anisotropy_accepted(anisotropy))
        {
            return 0;
        }
        const bool linear = magnification == 1;
        const bool linear_min = minification == 1;
        const std::uint64_t handle = g_next_sampler++;
        g_samplers.emplace(handle, SamplerObject{address_u, address_v, linear, 0.f, 0.f, 0.f, 1.f, linear_min, level_offset,
                                                 finest, coarsest, mip_linear, anisotropy});
        g_samplers.at(handle).address_w = address_w;
        return static_cast<std::int64_t>(handle);
    }

    std::int64_t gl_sampler_state_create_filters_border_range(std::int32_t magnification, std::int32_t minification,
                                                              std::int32_t mip, std::int32_t address_u,
                                                              std::int32_t address_v, std::int32_t address_w, float red,
                                                              float green, float blue, float alpha, float level_offset,
                                                              float finest, float coarsest, std::int32_t anisotropy)
    {
        // Magnification and minification may each be nearest or linear.
        // Linear mip filtering blends neighboring levels. A level offset
        // selects a coarser mip. A finest level above 0 raises that
        // selection. A coarsest level below the unlimited sentinel lowers
        // it. Every axis must be border. The border color is applied only
        // for this draw.
        const bool mip_linear = mip == 1;
        if ((mip != 0 && !mip_linear) || (magnification != 0 && magnification != 1) ||
            (minification != 0 && minification != 1))
        {
            set_last_error("igpu_sampler_state_create: the opengl backend only samples the nearest texel");
            return 0;
        }
        if (address_u != 3 || address_v != 3 || address_w != 3)
        {
            set_last_error("igpu_sampler_state_create: the opengl backend only accepts border on every axis");
            return 0;
        }
        if (finest < 0.f || !(coarsest >= 0.f))
        {
            set_last_error("igpu_sampler_state_create: the opengl backend has no mip chain");
            return 0;
        }
        if (!gl_anisotropy_accepted(anisotropy))
        {
            return 0;
        }
        const bool linear = magnification == 1;
        const bool linear_min = minification == 1;
        const std::uint64_t handle = g_next_sampler++;
        g_samplers.emplace(handle, SamplerObject{3, 3, linear, red, green, blue, alpha, linear_min, level_offset,
                                                 finest, coarsest, mip_linear, anisotropy});
        g_samplers.at(handle).address_w = address_w;
        return static_cast<std::int64_t>(handle);
    }

    std::int64_t gl_sampler_state_create_compare(std::int32_t compare, std::int32_t magnification,
                                                  std::int32_t minification, std::int32_t mip,
                                                  std::int32_t address_u, std::int32_t address_v,
                                                  std::int32_t address_w, std::int32_t anisotropy,
                                                  float level_offset, float finest, float coarsest)
    {
        static const GLenum functions[] = {GL_NONE, GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL,
                                           GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};
        if (compare < 1 || compare > 8)
        {
            set_last_error("igpu_sampler_state_create: compare must be a cmpfunc_* constant");
            return 0;
        }
        if (anisotropy > 1 && (magnification != 1 || minification != 1 || mip != 1))
        {
            set_last_error("igpu_sampler_state_create: anisotropy above 1 requires linear filters");
            return 0;
        }
        if (!gl_shadow_supported())
        {
            set_last_error("igpu_sampler_state_create: comparison sampling requires GL_NV_depth_buffer_float");
            return 0;
        }
        const auto handle = gl_sampler_state_create_filters_range(magnification, minification, mip,
                            address_u, address_v, address_w, level_offset, finest, coarsest, anisotropy);
        if (handle > 0)
        {
            g_samplers.at(static_cast<std::uint64_t>(handle)).compare = functions[compare];
        }
        return handle;
    }
    std::int64_t gl_blend_state_create(bool enabled, std::int32_t src, std::int32_t dest, std::int32_t equation,
                                       std::int32_t src_alpha, std::int32_t dest_alpha, std::int32_t equation_alpha,
                                       bool write_red, bool write_green, bool write_blue, bool write_alpha)
    {
        BlendObject object{};
        object.enabled = enabled;
        object.write_red = write_red;
        object.write_green = write_green;
        object.write_blue = write_blue;
        object.write_alpha = write_alpha;
        if (!gl_blend_factor(src, false, object.src_rgb) || !gl_blend_factor(dest, false, object.dst_rgb) ||
            !gl_blend_factor(src_alpha, true, object.src_alpha) ||
            !gl_blend_factor(dest_alpha, true, object.dst_alpha))
        {
            set_last_error("igpu_blend_state_create: blend factor must be a bm_* constant (bm_zero through bm_src_alpha_sat)");
            return 0;
        }
        if (!gl_blend_equation(equation, object.equation_rgb) || !gl_blend_equation(equation_alpha, object.equation_alpha))
        {
            set_last_error("igpu_blend_state_create: blend equation must be a bm_eq_* constant");
            return 0;
        }
        const std::uint64_t handle = g_next_sampler++;
        g_blends.emplace(handle, object);
        return static_cast<std::int64_t>(handle);
    }

    std::int64_t gl_depth_state_create(bool depth_test, bool depth_write, std::int32_t depth_func, bool stencil_enable,
                                       std::int32_t stencil_func, std::int32_t stencil_fail,
                                       std::int32_t stencil_depth_fail, std::int32_t stencil_pass,
                                       std::int32_t stencil_ref, std::int32_t stencil_read_mask,
                                       std::int32_t stencil_write_mask)
    {
        auto compare = [](std::int32_t raw, GLenum& out) {
            switch (raw)
            {
            case 1: out = GL_NEVER; return true;
            case 2: out = GL_LESS; return true;
            case 3: out = GL_EQUAL; return true;
            case 4: out = GL_LEQUAL; return true;
            case 5: out = GL_GREATER; return true;
            case 6: out = GL_NOTEQUAL; return true;
            case 7: out = GL_GEQUAL; return true;
            case 8: out = GL_ALWAYS; return true;
            default: return false;
            }
        };
        auto stencil_op = [](std::int32_t raw) { return raw >= 1 && raw <= 8; };
        GLenum func = GL_LESS;
        GLenum stencil_compare = GL_ALWAYS;
        if (!compare(depth_func, func))
        {
            set_last_error("igpu_depth_state_create: depth function must be a cmpfunc_* constant");
            return 0;
        }
        if (!compare(stencil_func, stencil_compare) || !stencil_op(stencil_fail) || !stencil_op(stencil_depth_fail) ||
            !stencil_op(stencil_pass))
        {
            set_last_error("igpu_depth_state_create: stencil function must be cmpfunc_* and stencil ops must be stencilop_*");
            return 0;
        }
        if (stencil_ref < 0 || stencil_ref > 255 || stencil_read_mask < 0 || stencil_read_mask > 255 ||
            stencil_write_mask < 0 || stencil_write_mask > 255)
        {
            set_last_error("igpu_depth_state_create: stencil ref and masks must be in 0..255");
            return 0;
        }
        constexpr GLenum operations[] = {GL_KEEP,GL_ZERO,GL_REPLACE,GL_INCR,GL_DECR,GL_INVERT,0x8507,0x8508};
        const std::uint64_t handle = g_next_sampler++;
        g_depths.emplace(handle, DepthObject{depth_test,depth_write,func,stencil_enable,stencil_compare,stencil_ref,
            static_cast<GLuint>(stencil_read_mask),static_cast<GLuint>(stencil_write_mask),
            operations[stencil_fail-1],operations[stencil_depth_fail-1],operations[stencil_pass-1]});
        return static_cast<std::int64_t>(handle);
    }

    std::int64_t gl_raster_state_create(std::int32_t cull, std::int32_t fill, bool scissor, bool depth_clip)
    {
        if (cull != 0 && cull != 1 && cull != 2)
        {
            set_last_error("igpu_raster_state_create: cull must be cull_noculling, cull_clockwise or cull_counterclockwise");
            return 0;
        }
        if (fill < 0 || fill > 1)
        {
            set_last_error("igpu_raster_state_create: fill must be 0 (solid) or 1 (wireframe)");
            return 0;
        }
        if (!depth_clip && !g_depth_clamp_context)
        {
            set_last_error("igpu_raster_state_create: depth clip false requires native depth clamp support");
            return 0;
        }
        const std::uint64_t handle = g_next_sampler++;
        g_rasters.emplace(handle, RasterObject{cull, scissor, fill, depth_clip});
        return static_cast<std::int64_t>(handle);
    }

    bool gl_state_release(std::uint64_t handle)
    {
        if (g_blends.erase(handle) != 0)
        {
            return true;
        }
        if (g_rasters.erase(handle) != 0)
        {
            return true;
        }
        if (g_depths.erase(handle) != 0)
        {
            return true;
        }
        if (g_samplers.erase(handle) == 0)
        {
            set_last_error("igpu_state_release: unknown sampler handle");
            return false;
        }
        return true;
    }
}
