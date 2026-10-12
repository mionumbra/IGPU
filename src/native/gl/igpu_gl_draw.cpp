#include "igpu_gl_draw.h"

#include "igpu_error.h"
#include "igpu_gl_loader.h"

#include <Windows.h>
#include <GL/gl.h>

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
        };

        struct TextureObject
        {
            GLuint texture = 0;
            GLuint framebuffer = 0;
            std::int32_t width = 0;
            std::int32_t height = 0;
        };

        std::unordered_map<std::uint64_t, BufferObject> g_buffers;
        std::unordered_map<std::uint64_t, TextureObject> g_textures;
        std::uint64_t g_next_buffer = 1;
        std::uint64_t g_next_texture = 1;
        std::uint64_t g_next_layout = 1;
        std::unordered_map<std::uint64_t, int> g_layouts;
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
            std::int32_t cull = 0;
        };
        std::unordered_map<std::uint64_t, RasterObject> g_rasters;
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

        bool gl_pipeline_accepted(const char* entry, std::int32_t primitive, std::int64_t depth_state, GLenum& mode)
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
            if (depth_state != 0)
            {
                set_last_error(std::string(entry) + ": the opengl backend draws with no extra state");
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
        };

        void save_cull(SavedCull& saved)
        {
            saved.enabled = glIsEnabled(GL_CULL_FACE);
            glGetIntegerv(GL_CULL_FACE_MODE, &saved.mode);
            glGetIntegerv(GL_FRONT_FACE, &saved.front);
        }

        void apply_cull(const RasterObject& raster)
        {
            glFrontFace(GL_CW);
            if (raster.cull == 0)
            {
                glDisable(GL_CULL_FACE);
                return;
            }
            glEnable(GL_CULL_FACE);
            glCullFace(raster.cull == 1 ? GL_FRONT : GL_BACK);
        }

        void restore_cull(const SavedCull& saved)
        {
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
        }

        bool gl_draw_accepted(const char* entry, std::int32_t primitive, std::int64_t blend_state,
                              std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state,
                              const BlendObject*& blend, const RasterObject*& raster, GLenum& mode)
        {
            if (!gl_pipeline_accepted(entry, primitive, depth_state, mode))
            {
                return false;
            }
            if (!gl_resolve_blend(entry, blend_state, blend))
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
        if (fns.DeleteBuffers != nullptr)
        {
            for (const auto& entry : g_buffers)
            {
                fns.DeleteBuffers(1, &entry.second.id);
            }
        }
        if (fns.DeleteFramebuffers != nullptr)
        {
            for (const auto& entry : g_textures)
            {
                fns.DeleteFramebuffers(1, &entry.second.framebuffer);
            }
        }
        for (const auto& entry : g_textures)
        {
            glDeleteTextures(1, &entry.second.texture);
        }
        g_buffers.clear();
        g_textures.clear();
        g_layouts.clear();
        g_samplers.clear();
        g_blends.clear();
        g_rasters.clear();
        g_next_sampler = 1;
        g_vertex_array = 0;
        g_next_buffer = 1;
        g_next_texture = 1;
        g_next_layout = 1;
    }

    std::int64_t gl_buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride)
    {
        const bool position = usage == 0 && bind == 1 && size > 0 && size % 8 == 0 && (stride == 0 || stride == 8);
        const bool coloured = usage == 0 && bind == 1 && size > 0 && size % 12 == 0 && stride == 12;
        const bool textured = usage == 0 && bind == 1 && size > 0 && size % 16 == 0 && stride == 16;
        const bool index = usage == 0 && bind == 2 && stride == 0 && size > 0 && size % 2 == 0;
        if (!position && !coloured && !textured && !index)
        {
            set_last_error("igpu_buffer_create: the opengl backend only accepts a static float2 vertex buffer, a static position-and-colour vertex buffer, a static position-and-texcoord vertex buffer, or a static 16-bit index buffer");
            return 0;
        }
        const auto& fns = igpu_gl_fns();
        const GLenum target = index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
        const std::int32_t stored_stride = index ? 0 : (textured ? 16 : (coloured ? 12 : 8));
        GLuint id = 0;
        fns.GenBuffers(1, &id);
        fns.BindBuffer(target, id);
        fns.BufferData(target, static_cast<std::ptrdiff_t>(size), nullptr, GL_STATIC_DRAW);
        fns.BindBuffer(target, 0);
        const std::uint64_t handle = g_next_buffer++;
        g_buffers.emplace(handle, BufferObject{id, size, stored_stride, bind, target});
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
        if (offset != 0 || static_cast<std::int64_t>(data.length()) != it->second.size)
        {
            set_last_error("igpu_buffer_write: the opengl backend only replaces the whole buffer from offset 0");
            return false;
        }
        const auto& fns = igpu_gl_fns();
        fns.BindBuffer(it->second.target, it->second.id);
        fns.BufferData(it->second.target, static_cast<std::ptrdiff_t>(it->second.size), data.data(), GL_STATIC_DRAW);
        fns.BindBuffer(it->second.target, 0);
        return true;
    }

    bool gl_buffer_release(std::uint64_t buffer)
    {
        const auto it = g_buffers.find(buffer);
        if (it == g_buffers.end())
        {
            set_last_error("igpu_buffer_release: unknown buffer handle");
            return false;
        }
        igpu_gl_fns().DeleteBuffers(1, &it->second.id);
        g_buffers.erase(it);
        return true;
    }

    std::int64_t gl_input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                        const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step,
                                        std::int32_t element_count, std::int32_t vertex_stride,
                                        std::int32_t instance_stride)
    {
        if (shader == 0 || instance_stride != 0)
        {
            set_last_error("igpu_input_layout_create: the opengl backend only accepts a float2 position, a float2 position followed by a colour, or a float2 position followed by a float2 texcoord");
            return 0;
        }
        auto element_is = [&](std::size_t index, std::int64_t expect_usage, std::int64_t expect_type) {
            if (usage.size() <= index || type.size() <= index || step.size() <= index)
            {
                return false;
            }
            std::int64_t usage_value = 0;
            std::int64_t type_value = 0;
            std::int64_t step_value = 0;
            return read_i64(usage[index], usage_value) && read_i64(type[index], type_value) &&
                   read_i64(step[index], step_value) && usage_value == expect_usage && type_value == expect_type &&
                   step_value == 0;
        };
        const bool position = element_count == 1 && (vertex_stride == 0 || vertex_stride == 8) && element_is(0, 1, 2);
        const bool coloured = element_count == 2 && (vertex_stride == 0 || vertex_stride == 12) &&
                              element_is(0, 1, 2) && element_is(1, 2, 5);
        const bool textured = element_count == 2 && (vertex_stride == 0 || vertex_stride == 16) &&
                              element_is(0, 1, 2) && element_is(1, 4, 2);
        if (!position && !coloured && !textured)
        {
            set_last_error("igpu_input_layout_create: the opengl backend only accepts a float2 position, a float2 position followed by a colour, or a float2 position followed by a float2 texcoord");
            return 0;
        }
        const std::uint64_t handle = g_next_layout++;
        g_layouts.emplace(handle, textured ? 16 : (coloured ? 12 : 8));
        return static_cast<std::int64_t>(handle);
    }

    bool gl_input_layout_release(std::uint64_t layout)
    {
        return g_layouts.erase(layout) != 0;
    }

    std::int64_t gl_texture_create_kind(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                        std::int32_t format, bool render_target, bool storage)
    {
        if (kind != 0 || depth != 1 || format != 6 || !render_target || storage || width <= 0 || height <= 0)
        {
            set_last_error("igpu_texture_create: the opengl backend only accepts one rgba8 render target");
            return 0;
        }
        const auto& fns = igpu_gl_fns();
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLuint framebuffer = 0;
        fns.GenFramebuffers(1, &framebuffer);
        fns.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        fns.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        const GLenum status = fns.CheckFramebufferStatus(GL_FRAMEBUFFER);
        // This driver drops draws into an image that has never been cleared.
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        fns.BindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            fns.DeleteFramebuffers(1, &framebuffer);
            glDeleteTextures(1, &texture);
            set_last_error("igpu_texture_create: the framebuffer is incomplete");
            return 0;
        }
        const std::uint64_t handle = g_next_texture++;
        g_textures.emplace(handle, TextureObject{texture, framebuffer, width, height});
        return static_cast<std::int64_t>(handle);
    }

    bool gl_texture_release(std::uint64_t texture)
    {
        const auto it = g_textures.find(texture);
        if (it == g_textures.end())
        {
            set_last_error("igpu_texture_release: unknown texture handle");
            return false;
        }
        igpu_gl_fns().DeleteFramebuffers(1, &it->second.framebuffer);
        glDeleteTextures(1, &it->second.texture);
        g_textures.erase(it);
        return true;
    }

    std::int64_t gl_texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer,
                                       std::int32_t mip)
    {
        const auto it = g_textures.find(texture);
        if (it == g_textures.end() || layer != 0 || mip != 0 || x < 0 || y < 0 || x >= it->second.width ||
            y >= it->second.height)
        {
            set_last_error("igpu_texture_read: the pixel is outside this rgba8 texture");
            return 0;
        }
        const auto& fns = igpu_gl_fns();
        GLint previous = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        fns.BindFramebuffer(GL_FRAMEBUFFER, it->second.framebuffer);
        unsigned char pixel[4] = {};
        const int gl_row = it->second.height - 1 - y;
        glReadPixels(x, gl_row, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        fns.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
        return static_cast<std::int64_t>(pixel[0] | (pixel[1] << 8) | (pixel[2] << 16));
    }

    bool gl_color_target_begin(std::uint64_t texture, std::int32_t& previous_framebuffer,
                               std::int32_t previous_viewport[4])
    {
        const auto it = g_textures.find(texture);
        if (it == g_textures.end())
        {
            set_last_error("gl_color_target_begin: unknown texture");
            return false;
        }
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
        glGetIntegerv(GL_VIEWPORT, previous_viewport);
        igpu_gl_fns().BindFramebuffer(GL_FRAMEBUFFER, it->second.framebuffer);
        glViewport(0, 0, it->second.width, it->second.height);
        return true;
    }

    void gl_color_target_end(std::int32_t previous_framebuffer, const std::int32_t previous_viewport[4])
    {
        igpu_gl_fns().BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
        glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    }

    bool gl_draw_to_render_targets_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                         std::int64_t first_vertex, std::int64_t vertex_count,
                                         const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers,
                                         const gm::wire::GMArrayView& mips, std::int64_t blend_state,
                                         std::int64_t depth_state, std::int64_t raster_state,
                                         std::int64_t sampler_state)
    {
        GLenum mode = GL_TRIANGLES;
        const BlendObject* blend = nullptr;
        const RasterObject* raster = nullptr;
        if (!gl_draw_accepted("igpu_draw_to_render_targets", primitive, blend_state, depth_state, raster_state,
                              sampler_state, blend, raster, mode))
        {
            return false;
        }
        if (targets.size() != 1 || layers.size() != 1 || mips.size() != 1)
        {
            set_last_error("igpu_draw_to_render_targets: the opengl backend accepts one target");
            return false;
        }
        std::int64_t target = 0;
        std::int64_t layer = 0;
        std::int64_t mip = 0;
        if (!read_i64(targets[0], target) || !read_i64(layers[0], layer) || !read_i64(mips[0], mip) || layer != 0 ||
            mip != 0)
        {
            set_last_error("igpu_draw_to_render_targets: the opengl backend accepts one target");
            return false;
        }
        const auto texture = g_textures.find(static_cast<std::uint64_t>(target));
        const auto buffer = g_buffers.find(vertex_buffer);
        if (texture == g_textures.end() || buffer == g_buffers.end())
        {
            set_last_error("igpu_draw_to_render_targets: unknown buffer, layout, or texture");
            return false;
        }
        const auto layout_it = g_layouts.find(layout);
        if (layout_it == g_layouts.end() || layout_it->second != buffer->second.stride)
        {
            set_last_error("igpu_draw_to_render_targets: the vertex buffer stride does not match the input layout");
            return false;
        }
        if (buffer->second.bind != 1)
        {
            set_last_error("igpu_draw_to_render_targets: the vertex buffer was not created with IgpuBufferBind.Vertex");
            return false;
        }
        if (first_vertex < 0 || vertex_count <= 0 ||
            (first_vertex + vertex_count) * buffer->second.stride > buffer->second.size)
        {
            set_last_error("igpu_draw_to_render_targets: the vertex range is outside the buffer");
            return false;
        }
        GLint program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        if (program == 0)
        {
            set_last_error("igpu_draw_to_render_targets: no shader program is bound");
            return false;
        }

        const auto& fns = igpu_gl_fns();
        if (g_vertex_array == 0)
        {
            fns.GenVertexArrays(1, &g_vertex_array);
        }
        fns.BindVertexArray(g_vertex_array);
        GLint previous_fbo = 0;
        GLint previous_viewport[4] = {};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
        glGetIntegerv(GL_VIEWPORT, previous_viewport);
        fns.BindFramebuffer(GL_FRAMEBUFFER, texture->second.framebuffer);
        glViewport(0, 0, texture->second.width, texture->second.height);
        fns.BindBuffer(GL_ARRAY_BUFFER, buffer->second.id);
        bind_vertices(buffer->second, first_vertex);
        SavedBlend saved_blend{};
        SavedCull saved_cull{};
        if (blend != nullptr)
        {
            save_blend(saved_blend);
            apply_blend(*blend);
        }
        if (raster != nullptr)
        {
            save_cull(saved_cull);
            apply_cull(*raster);
        }
        glDrawArrays(mode, 0, static_cast<GLsizei>(vertex_count));
        if (raster != nullptr)
        {
            restore_cull(saved_cull);
        }
        if (blend != nullptr)
        {
            restore_blend(saved_blend);
        }
        unbind_vertices();
        fns.BindBuffer(GL_ARRAY_BUFFER, 0);
        glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
        fns.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));
        fns.BindVertexArray(0);
        return true;
    }

    bool gl_draw(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                 std::int64_t first_vertex, std::int64_t vertex_count, std::int64_t blend_state,
                 std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state)
    {
        GLenum mode = GL_TRIANGLES;
        const BlendObject* blend = nullptr;
        const RasterObject* raster = nullptr;
        if (!gl_draw_accepted("igpu_draw", primitive, blend_state, depth_state, raster_state, sampler_state, blend,
                              raster, mode))
        {
            return false;
        }
        const auto vertex = g_buffers.find(vertex_buffer);
        if (vertex == g_buffers.end())
        {
            set_last_error("igpu_draw: unknown buffer or layout");
            return false;
        }
        const auto layout_it = g_layouts.find(layout);
        if (layout_it == g_layouts.end() || layout_it->second != vertex->second.stride)
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
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array);
        fns.BindVertexArray(g_vertex_array);
        fns.BindBuffer(GL_ARRAY_BUFFER, vertex->second.id);
        bind_vertices(vertex->second, 0);
        SavedBlend saved_blend{};
        SavedCull saved_cull{};
        if (blend != nullptr)
        {
            save_blend(saved_blend);
            apply_blend(*blend);
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
        if (blend != nullptr)
        {
            restore_blend(saved_blend);
        }
        unbind_vertices();
        fns.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array));
        fns.BindVertexArray(0);
        return true;
    }

    bool gl_draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer,
                         std::int32_t primitive, std::int64_t first_index, std::int64_t index_count,
                         std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                         std::int64_t sampler_state)
    {
        GLenum mode = GL_TRIANGLES;
        const BlendObject* blend = nullptr;
        const RasterObject* raster = nullptr;
        if (!gl_draw_accepted("igpu_draw_indexed", primitive, blend_state, depth_state, raster_state, sampler_state,
                              blend, raster, mode))
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
        if (layout_it == g_layouts.end() || layout_it->second != vertex->second.stride)
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
        GLint previous_element = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &previous_element);
        fns.BindVertexArray(g_vertex_array);
        fns.BindBuffer(GL_ARRAY_BUFFER, vertex->second.id);
        bind_vertices(vertex->second, 0);
        fns.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index->second.id);
        SavedBlend saved_blend{};
        SavedCull saved_cull{};
        if (blend != nullptr)
        {
            save_blend(saved_blend);
            apply_blend(*blend);
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
        if (blend != nullptr)
        {
            restore_blend(saved_blend);
        }
        unbind_vertices();
        fns.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array));
        fns.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(previous_element));
        fns.BindVertexArray(0);
        return true;
    }

    bool gl_draw_sampled(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                         std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                         std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                         std::int64_t sampler_state)
    {
        GLenum mode = GL_TRIANGLES;
        const BlendObject* blend = nullptr;
        if (!gl_pipeline_accepted("igpu_draw_sampled", primitive, depth_state, mode))
        {
            return false;
        }
        if (raster_state != 0)
        {
            set_last_error("igpu_draw_sampled: the opengl backend draws with no extra state");
            return false;
        }
        if (!gl_resolve_blend("igpu_draw_sampled", blend_state, blend))
        {
            return false;
        }
        const auto source = g_textures.find(texture);
        if (source == g_textures.end())
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
        if (layout_it == g_layouts.end() || layout_it->second != vertex->second.stride)
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
        if (static_cast<GLuint>(draw_fbo) == source->second.framebuffer)
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
        bool linear = false;
        bool linear_min = false;
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
            linear = sampler_it->second.linear;
            linear_min = sampler_it->second.linear_min;
            border_color[0] = sampler_it->second.border_red;
            border_color[1] = sampler_it->second.border_green;
            border_color[2] = sampler_it->second.border_blue;
            border_color[3] = sampler_it->second.border_alpha;
        }
        if (g_vertex_array == 0)
        {
            fns.GenVertexArrays(1, &g_vertex_array);
        }
        GLint previous_array = 0;
        GLint previous_texture = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
        glBindTexture(GL_TEXTURE_2D, source->second.texture);
        GLint previous_wrap_s = GL_CLAMP_TO_EDGE;
        GLint previous_wrap_t = GL_CLAMP_TO_EDGE;
        GLint previous_mag = GL_NEAREST;
        GLint previous_min = GL_NEAREST;
        GLfloat previous_border[4] = {0.f, 0.f, 0.f, 0.f};
        const bool border = address == 3;
        if (sampler_state != 0)
        {
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &previous_wrap_s);
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &previous_wrap_t);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_wrap(address));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_wrap(address_v));
            if (border)
            {
                glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, previous_border);
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
            }
            if (linear)
            {
                glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &previous_mag);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            if (linear_min)
            {
                glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &previous_min);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }
        }
        fns.Uniform1i(sampler, 0);
        fns.BindVertexArray(g_vertex_array);
        fns.BindBuffer(GL_ARRAY_BUFFER, vertex->second.id);
        bind_vertices(vertex->second, 0);
        SavedBlend saved_blend{};
        if (blend != nullptr)
        {
            save_blend(saved_blend);
            apply_blend(*blend);
        }
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
        if (blend != nullptr)
        {
            restore_blend(saved_blend);
        }
        unbind_vertices();
        fns.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array));
        fns.BindVertexArray(0);
        if (sampler_state != 0)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, previous_wrap_s);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, previous_wrap_t);
            if (border)
            {
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, previous_border);
            }
            if (linear)
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, previous_mag);
            }
            if (linear_min)
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, previous_min);
            }
        }
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
        return true;
    }

    std::int64_t gl_sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                       std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                       std::int32_t address_w, float level_offset, float finest,
                                                       float coarsest)
    {
        // One mip. finest must be the base level. A non-negative coarsest
        // cannot select another level on this backend.
        // Magnification and minification may each be nearest or linear.
        // Mip filtering stays nearest. GL_LINEAR minification is not a mip chain.
        // U, V, and W may differ. W is checked and not stored; a 2D draw uses U and V.
        if (mip != 0 || (magnification != 0 && magnification != 1) || (minification != 0 && minification != 1))
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
        if (level_offset != 0.f || finest != 0.f || !(coarsest >= 0.f))
        {
            set_last_error("igpu_sampler_state_create: the opengl backend has no mip chain");
            return 0;
        }
        const bool linear = magnification == 1;
        const bool linear_min = minification == 1;
        const std::uint64_t handle = g_next_sampler++;
        g_samplers.emplace(handle, SamplerObject{address_u, address_v, linear, 0.f, 0.f, 0.f, 1.f, linear_min});
        return static_cast<std::int64_t>(handle);
    }

    std::int64_t gl_sampler_state_create_filters_border_range(std::int32_t magnification, std::int32_t minification,
                                                              std::int32_t mip, std::int32_t address_u,
                                                              std::int32_t address_v, std::int32_t address_w, float red,
                                                              float green, float blue, float alpha, float level_offset,
                                                              float finest, float coarsest)
    {
        // One mip. Magnification may be nearest or linear. Minification and mip stay nearest.
        // Every axis must be border. The border color is applied only for this draw.
        if (minification != 0 || mip != 0 || (magnification != 0 && magnification != 1))
        {
            set_last_error("igpu_sampler_state_create: the opengl backend only samples the nearest texel");
            return 0;
        }
        if (address_u != 3 || address_v != 3 || address_w != 3)
        {
            set_last_error("igpu_sampler_state_create: the opengl backend only accepts border on every axis");
            return 0;
        }
        if (level_offset != 0.f || finest != 0.f || !(coarsest >= 0.f))
        {
            set_last_error("igpu_sampler_state_create: the opengl backend has no mip chain");
            return 0;
        }
        const bool linear = magnification == 1;
        const std::uint64_t handle = g_next_sampler++;
        g_samplers.emplace(handle, SamplerObject{3, 3, linear, red, green, blue, alpha});
        return static_cast<std::int64_t>(handle);
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

    std::int64_t gl_raster_state_create(std::int32_t cull, std::int32_t fill, bool scissor, bool depth_clip)
    {
        if (cull != 0 && cull != 1 && cull != 2)
        {
            set_last_error("igpu_raster_state_create: cull must be cull_noculling, cull_clockwise or cull_counterclockwise");
            return 0;
        }
        if (fill != 0)
        {
            set_last_error("igpu_raster_state_create: the opengl backend draws solid triangles only");
            return 0;
        }
        if (scissor)
        {
            set_last_error("igpu_raster_state_create: the opengl backend does not scissor");
            return 0;
        }
        if (!depth_clip)
        {
            set_last_error("igpu_raster_state_create: the opengl backend keeps depth clip enabled");
            return 0;
        }
        const std::uint64_t handle = g_next_sampler++;
        g_rasters.emplace(handle, RasterObject{cull});
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
        if (g_samplers.erase(handle) == 0)
        {
            set_last_error("igpu_state_release: unknown sampler handle");
            return false;
        }
        return true;
    }
}
