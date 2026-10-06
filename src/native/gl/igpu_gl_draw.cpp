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
        g_vertex_array = 0;
        g_next_buffer = 1;
        g_next_texture = 1;
        g_next_layout = 1;
    }

    std::int64_t gl_buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride)
    {
        const bool vertex = usage == 0 && bind == 1 && size > 0 && size % 8 == 0 && (stride == 0 || stride == 8);
        const bool index = usage == 0 && bind == 2 && stride == 0 && size > 0 && size % 2 == 0;
        if (!vertex && !index)
        {
            set_last_error("igpu_buffer_create: the opengl backend only accepts a static float2 vertex buffer or a static 16-bit index buffer");
            return 0;
        }
        const auto& fns = igpu_gl_fns();
        const GLenum target = index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
        GLuint id = 0;
        fns.GenBuffers(1, &id);
        fns.BindBuffer(target, id);
        fns.BufferData(target, static_cast<std::ptrdiff_t>(size), nullptr, GL_STATIC_DRAW);
        fns.BindBuffer(target, 0);
        const std::uint64_t handle = g_next_buffer++;
        const std::int32_t stored_stride = vertex ? 8 : 0;
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
        if (shader == 0 || element_count != 1 || instance_stride != 0 || (vertex_stride != 0 && vertex_stride != 8) ||
            usage.size() < 1 || type.size() < 1 || step.size() < 1)
        {
            set_last_error("igpu_input_layout_create: the opengl backend only accepts a single float2 position");
            return 0;
        }
        std::int64_t usage_value = 0;
        std::int64_t type_value = 0;
        std::int64_t step_value = 0;
        if (!read_i64(usage[0], usage_value) || !read_i64(type[0], type_value) || !read_i64(step[0], step_value) ||
            usage_value != 1 || type_value != 2 || step_value != 0)
        {
            set_last_error("igpu_input_layout_create: the opengl backend only accepts a single float2 position");
            return 0;
        }
        const std::uint64_t handle = g_next_layout++;
        g_layouts.emplace(handle, 1);
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
        if (primitive == 6)
        {
            set_last_error("igpu_draw_to_render_targets: the 'trianglefan' primitive has no backend equivalent");
            return false;
        }
        if (targets.size() != 1 || layers.size() != 1 || mips.size() != 1)
        {
            set_last_error("igpu_draw_to_render_targets: the opengl backend accepts one target");
            return false;
        }
        if (blend_state != 0 || depth_state != 0 || raster_state != 0 || sampler_state != 0 || primitive != 4)
        {
            set_last_error("igpu_draw_to_render_targets: the opengl backend only draws a triangle list with no extra state");
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
        if (texture == g_textures.end() || buffer == g_buffers.end() || g_layouts.find(layout) == g_layouts.end())
        {
            set_last_error("igpu_draw_to_render_targets: unknown buffer, layout, or texture");
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
        fns.EnableVertexAttribArray(0);
        fns.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, buffer->second.stride,
                                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(first_vertex * buffer->second.stride)));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertex_count));
        fns.DisableVertexAttribArray(0);
        fns.BindBuffer(GL_ARRAY_BUFFER, 0);
        glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
        fns.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));
        fns.BindVertexArray(0);
        return true;
    }

    bool gl_draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer,
                         std::int32_t primitive, std::int64_t first_index, std::int64_t index_count,
                         std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state,
                         std::int64_t sampler_state)
    {
        if (primitive == 6)
        {
            set_last_error("igpu_draw_indexed: the 'trianglefan' primitive has no backend equivalent");
            return false;
        }
        if (blend_state != 0 || depth_state != 0 || raster_state != 0 || sampler_state != 0 || primitive != 4)
        {
            set_last_error("igpu_draw_indexed: the opengl backend only draws a triangle list with no extra state");
            return false;
        }
        const auto vertex = g_buffers.find(vertex_buffer);
        const auto index = g_buffers.find(index_buffer);
        if (vertex == g_buffers.end() || index == g_buffers.end() || g_layouts.find(layout) == g_layouts.end())
        {
            set_last_error("igpu_draw_indexed: unknown buffer or layout");
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
        fns.EnableVertexAttribArray(0);
        fns.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertex->second.stride, nullptr);
        fns.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index->second.id);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count), GL_UNSIGNED_SHORT,
                       reinterpret_cast<const void*>(static_cast<std::uintptr_t>(first_index * kIndexSize)));
        fns.DisableVertexAttribArray(0);
        fns.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array));
        fns.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(previous_element));
        fns.BindVertexArray(0);
        return true;
    }
}
