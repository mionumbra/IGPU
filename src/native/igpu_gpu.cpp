#include "igpu_backend.h"
#include "igpu_buffer.h"
#include "igpu_device.h"
#include "igpu_draw.h"
#include "igpu_error.h"
#include "igpu_input_layout.h"
#include "igpu_shader.h"
#include "igpu_state.h"
#include "igpu_texture.h"

#include <cmath>
#include <string>

namespace igpu
{
    namespace
    {
        // Checked before the virtual call. reset() destroys the active backend,
        // so it must not run from inside a method of that backend.
        void gm_colour_channels(std::int32_t border, float& red, float& green, float& blue)
        {
            const auto colour = static_cast<std::uint32_t>(border);
            red = static_cast<float>(colour & 0xFFu) / 255.0f;
            green = static_cast<float>((colour >> 8) & 0xFFu) / 255.0f;
            blue = static_cast<float>((colour >> 16) & 0xFFu) / 255.0f;
        }

        Backend* need(const char* entry)
        {
            if (!require_device(entry))
            {
                return nullptr;
            }
            Backend* backend = active_backend();
            if (backend == nullptr)
            {
                set_last_error(std::string(entry) + ": call igpu_init() first");
            }
            return backend;
        }
    }

    bool draw(std::uint64_t vertex_buffer,
              std::uint64_t layout,
              std::int32_t primitive,
              std::int64_t first_vertex,
              std::int64_t vertex_count,
              std::int64_t blend_state,
              std::int64_t depth_state,
              std::int64_t raster_state,
              std::int64_t sampler_state)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw");
        return backend != nullptr && backend->draw(
            vertex_buffer, layout, primitive, first_vertex, vertex_count,
            blend_state, depth_state, raster_state, sampler_state);
    }

    bool draw_instanced(std::uint64_t vertex_buffer,
                        std::uint64_t instance_buffer,
                        std::uint64_t layout,
                        std::int32_t primitive,
                        std::int64_t first_vertex,
                        std::int64_t vertex_count,
                        std::int64_t instance_count)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_instanced");
        return backend != nullptr && backend->draw_instanced(
            vertex_buffer, instance_buffer, layout, primitive, first_vertex, vertex_count, instance_count);
    }

    bool draw_indirect(std::uint64_t vertex_buffer,
                       std::uint64_t instance_buffer,
                       std::uint64_t layout,
                       std::int32_t primitive,
                       std::uint64_t args,
                       std::int64_t args_offset)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_indirect");
        return backend != nullptr && backend->draw_indirect(
            vertex_buffer, instance_buffer, layout, primitive, args, args_offset);
    }

    bool draw_patch(std::uint64_t vertex_buffer,
                    std::uint64_t layout,
                    std::int32_t control_points,
                    std::int64_t first_vertex,
                    std::int64_t vertex_count)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_patch");
        return backend != nullptr && backend->draw_patch(
            vertex_buffer, layout, control_points, first_vertex, vertex_count);
    }

    bool draw_indexed_indirect(std::uint64_t vertex_buffer,
                               std::uint64_t instance_buffer,
                               std::uint64_t layout,
                               std::uint64_t index_buffer,
                               std::int32_t primitive,
                               std::uint64_t args,
                               std::int64_t args_offset)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_indexed_indirect");
        return backend != nullptr && backend->draw_indexed_indirect(
            vertex_buffer, instance_buffer, layout, index_buffer, primitive, args, args_offset);
    }

    bool draw_indexed(std::uint64_t vertex_buffer,
                      std::uint64_t layout,
                      std::uint64_t index_buffer,
                      std::int32_t primitive,
                      std::int64_t first_index,
                      std::int64_t index_count,
                      std::int64_t blend_state,
                      std::int64_t depth_state,
                      std::int64_t raster_state,
                      std::int64_t sampler_state)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_indexed");
        return backend != nullptr && backend->draw_indexed(
            vertex_buffer, layout, index_buffer, primitive, first_index, index_count,
            blend_state, depth_state, raster_state, sampler_state);
    }

    std::int32_t draw_count()
    {
        Backend* backend = active_backend();
        return backend == nullptr ? 0 : backend->draw_count();
    }

    std::int32_t draw_restore_failures()
    {
        Backend* backend = active_backend();
        return backend == nullptr ? 0 : backend->draw_restore_failures();
    }

    bool is_vertex_buffer_bound(std::uint64_t buffer)
    {
        clear_last_error();
        Backend* backend = need("igpu_is_vertex_buffer_bound");
        return backend != nullptr && backend->is_vertex_buffer_bound(buffer);
    }

    std::int64_t blend_state_create(
        bool enabled,
        std::int32_t src,
        std::int32_t dest,
        std::int32_t equation,
        std::int32_t src_alpha,
        std::int32_t dest_alpha,
        std::int32_t equation_alpha,
        bool write_red,
        bool write_green,
        bool write_blue,
        bool write_alpha)
    {
        clear_last_error();
        Backend* backend = need("igpu_blend_state_create");
        return backend == nullptr ? 0 : backend->blend_state_create(
            enabled, src, dest, equation, src_alpha, dest_alpha, equation_alpha,
            write_red, write_green, write_blue, write_alpha);
    }

    std::int64_t depth_state_create(
        bool depth_test,
        bool depth_write,
        std::int32_t depth_func,
        bool stencil_enable,
        std::int32_t stencil_func,
        std::int32_t stencil_fail,
        std::int32_t stencil_depth_fail,
        std::int32_t stencil_pass,
        std::int32_t stencil_ref,
        std::int32_t stencil_read_mask,
        std::int32_t stencil_write_mask)
    {
        clear_last_error();
        Backend* backend = need("igpu_depth_state_create");
        return backend == nullptr ? 0 : backend->depth_state_create(
            depth_test, depth_write, depth_func, stencil_enable, stencil_func,
            stencil_fail, stencil_depth_fail, stencil_pass, stencil_ref,
            stencil_read_mask, stencil_write_mask);
    }

    std::int64_t raster_state_create(
        std::int32_t cull,
        std::int32_t fill,
        bool scissor,
        bool depth_clip)
    {
        clear_last_error();
        Backend* backend = need("igpu_raster_state_create");
        return backend == nullptr ? 0 : backend->raster_state_create(cull, fill, scissor, depth_clip);
    }

    std::int64_t sampler_state_create(
        std::int32_t filter,
        bool repeat,
        std::int32_t anisotropy)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create");
        return backend == nullptr ? 0 : backend->sampler_state_create(filter, repeat, anisotropy);
    }

    std::int64_t sampler_state_create_address(std::int32_t filter, std::int32_t address, std::int32_t anisotropy)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_address");
        return backend == nullptr ? 0 : backend->sampler_state_create_address(filter, address, anisotropy);
    }

    std::int64_t sampler_state_create_border(std::int32_t filter, std::int32_t anisotropy, std::int32_t border)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_border");
        if (backend == nullptr)
        {
            return 0;
        }
        // GameMaker colours are packed blue, green, red. Backends receive
        // channels in 0..1 and do not see that packing.
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        gm_colour_channels(border, red, green, blue);
        return backend->sampler_state_create_border(filter, anisotropy, red, green, blue, 1.0f);
    }

    std::int64_t sampler_state_create_axes(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_axes");
        return backend == nullptr ? 0 : backend->sampler_state_create_axes(
            filter, address_u, address_v, address_w, anisotropy);
    }

    std::int64_t sampler_state_create_axes_border(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        std::int32_t border)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_axes_border");
        if (backend == nullptr)
        {
            return 0;
        }
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        gm_colour_channels(border, red, green, blue);
        return backend->sampler_state_create_axes_border(
            filter, address_u, address_v, address_w, anisotropy, red, green, blue, 1.0f);
    }

    std::int64_t sampler_state_create_axes_range(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        float level_offset,
        float finest,
        float coarsest)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_axes_range");
        if (backend == nullptr)
        {
            return 0;
        }
        if (!std::isfinite(level_offset) || !std::isfinite(finest) || !std::isfinite(coarsest))
        {
            set_last_error("igpu_sampler_state_create_axes_range: level offset, finest and coarsest must be finite numbers of levels");
            return 0;
        }
        if (finest > coarsest)
        {
            set_last_error("igpu_sampler_state_create_axes_range: the finest level is above the coarsest level");
            return 0;
        }
        return backend->sampler_state_create_axes_range(
            filter, address_u, address_v, address_w, anisotropy, level_offset, finest, coarsest);
    }

    std::int64_t sampler_state_create_axes_border_range(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        std::int32_t border,
        float level_offset,
        float finest,
        float coarsest)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_axes_border_range");
        if (backend == nullptr)
        {
            return 0;
        }
        if (!std::isfinite(level_offset) || !std::isfinite(finest) || !std::isfinite(coarsest))
        {
            set_last_error("igpu_sampler_state_create_axes_border_range: level offset, finest and coarsest must be finite numbers of levels");
            return 0;
        }
        if (finest > coarsest)
        {
            set_last_error("igpu_sampler_state_create_axes_border_range: the finest level is above the coarsest level");
            return 0;
        }
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        gm_colour_channels(border, red, green, blue);
        return backend->sampler_state_create_axes_border_range(
            filter, address_u, address_v, address_w, anisotropy, red, green, blue, 1.0f, level_offset, finest, coarsest);
    }

    std::int64_t sampler_state_create_filters(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_filters");
        return backend == nullptr ? 0 : backend->sampler_state_create_filters(
            magnification, minification, mip, address_u, address_v, address_w);
    }

    std::int64_t sampler_state_create_filters_border(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t border)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_filters_border");
        if (backend == nullptr)
        {
            return 0;
        }
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        gm_colour_channels(border, red, green, blue);
        return backend->sampler_state_create_filters_border(
            magnification, minification, mip, address_u, address_v, address_w, red, green, blue, 1.0f);
    }

    std::int64_t sampler_state_create_filters_offset(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        float level_offset)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_filters_offset");
        if (backend == nullptr)
        {
            return 0;
        }
        if (!std::isfinite(level_offset))
        {
            set_last_error("igpu_sampler_state_create_filters_offset: level offset must be a finite number of levels");
            return 0;
        }
        return backend->sampler_state_create_filters_offset(
            magnification, minification, mip, address_u, address_v, address_w, level_offset);
    }

    std::int64_t sampler_state_create_filters_range(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        float level_offset,
        float finest,
        float coarsest)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_filters_range");
        if (backend == nullptr)
        {
            return 0;
        }
        if (!std::isfinite(level_offset) || !std::isfinite(finest) || !std::isfinite(coarsest))
        {
            set_last_error("igpu_sampler_state_create_filters_range: level offset, finest and coarsest must be finite numbers of levels");
            return 0;
        }
        if (finest > coarsest)
        {
            set_last_error("igpu_sampler_state_create_filters_range: the finest level is above the coarsest level");
            return 0;
        }
        return backend->sampler_state_create_filters_range(
            magnification, minification, mip, address_u, address_v, address_w, level_offset, finest, coarsest);
    }

    std::int64_t sampler_state_create_filters_border_range(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t border,
        float level_offset,
        float finest,
        float coarsest)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_filters_border_range");
        if (backend == nullptr)
        {
            return 0;
        }
        if (!std::isfinite(level_offset) || !std::isfinite(finest) || !std::isfinite(coarsest))
        {
            set_last_error("igpu_sampler_state_create_filters_border_range: level offset, finest and coarsest must be finite numbers of levels");
            return 0;
        }
        if (finest > coarsest)
        {
            set_last_error("igpu_sampler_state_create_filters_border_range: the finest level is above the coarsest level");
            return 0;
        }
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        gm_colour_channels(border, red, green, blue);
        return backend->sampler_state_create_filters_border_range(
            magnification, minification, mip, address_u, address_v, address_w, red, green, blue, 1.0f, level_offset, finest,
            coarsest);
    }

    std::int64_t sampler_state_create_full(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        std::int32_t border,
        std::int32_t compare,
        float level_offset,
        float finest,
        float coarsest)
    {
        const char* api = "igpu_sampler_state_create";
        clear_last_error();
        if (!std::isfinite(level_offset) || !std::isfinite(finest) || (coarsest >= 0.0f && !std::isfinite(coarsest)))
        {
            set_last_error(std::string(api) + ": level offset and level limits must be finite");
            return 0;
        }
        if (finest < 0.0f || (coarsest >= 0.0f && finest > coarsest))
        {
            set_last_error(std::string(api) + ": the finest level is above the coarsest level");
            return 0;
        }
        if (anisotropy < 1 || anisotropy > 16)
        {
            set_last_error(std::string(api) + ": anisotropy must be in 1..16");
            return 0;
        }
        const bool wants_border = address_u == 3 || address_v == 3 || address_w == 3;
        if (compare != 0)
        {
            if (compare < 1 || compare > 8)
            {
                set_last_error(std::string(api) + ": compare must be 0 or a cmpfunc_* constant");
                return 0;
            }
            if (wants_border)
            {
                set_last_error(std::string(api) + ": border address mode is not available on a comparison sampler");
                return 0;
            }
            if (anisotropy != 1 || level_offset != 0.0f || finest != 0.0f || coarsest >= 0.0f)
            {
                set_last_error(std::string(api) + ": a comparison sampler uses anisotropy 1 and an open level range");
                return 0;
            }
            return sampler_state_create_compare(
                compare, magnification, minification, mip, address_u, address_v, address_w);
        }
        if (anisotropy > 1 && (magnification != 1 || minification != 1 || mip != 1))
        {
            set_last_error(std::string(api) + ": anisotropy above 1 requires linear filters");
            return 0;
        }
        const float max_lod = coarsest < 0.0f ? 3.402823466e+38f : coarsest;
        if (anisotropy > 1)
        {
            if (wants_border)
            {
                return sampler_state_create_axes_border_range(
                    2, address_u, address_v, address_w, anisotropy, border, level_offset, finest, max_lod);
            }
            return sampler_state_create_axes_range(
                2, address_u, address_v, address_w, anisotropy, level_offset, finest, max_lod);
        }
        if (wants_border)
        {
            return sampler_state_create_filters_border_range(
                magnification, minification, mip, address_u, address_v, address_w, border, level_offset, finest, max_lod);
        }
        return sampler_state_create_filters_range(
            magnification, minification, mip, address_u, address_v, address_w, level_offset, finest, max_lod);
    }

    std::int64_t sampler_state_create_compare(
        std::int32_t compare,
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w)
    {
        clear_last_error();
        Backend* backend = need("igpu_sampler_state_create_compare");
        return backend == nullptr ? 0 : backend->sampler_state_create_compare(
            compare, magnification, minification, mip, address_u, address_v, address_w);
    }

    bool state_release(std::uint64_t handle)
    {
        clear_last_error();
        Backend* backend = need("igpu_state_release");
        return backend != nullptr && backend->state_release(handle);
    }

    std::int64_t texture_create(std::int32_t width, std::int32_t height, std::int32_t format, bool render_target)
    {
        clear_last_error();
        Backend* backend = need("igpu_texture_create");
        return backend == nullptr ? 0 : backend->texture_create(width, height, format, render_target);
    }

    std::int64_t texture_create_kind(
        std::int32_t kind,
        std::int32_t width,
        std::int32_t height,
        std::int32_t depth,
        std::int32_t format,
        bool render_target,
        bool storage)
    {
        clear_last_error();
        Backend* backend = need("igpu_texture_create_kind");
        return backend == nullptr ? 0 : backend->texture_create_kind(
            kind, width, height, depth, format, render_target, storage);
    }

    std::int64_t texture_create_mips(
        std::int32_t kind,
        std::int32_t width,
        std::int32_t height,
        std::int32_t depth,
        std::int32_t format,
        bool storage,
        std::int32_t mip_count)
    {
        clear_last_error();
        Backend* backend = need("igpu_texture_create_mips");
        return backend == nullptr ? 0 : backend->texture_create_mips(
            kind, width, height, depth, format, storage, mip_count);
    }

    bool texture_generate_mips(std::uint64_t texture)
    {
        clear_last_error();
        Backend* backend = need("igpu_texture_generate_mips");
        return backend != nullptr && backend->texture_generate_mips(texture);
    }

    bool texture_release(std::uint64_t texture)
    {
        clear_last_error();
        Backend* backend = need("igpu_texture_release");
        return backend != nullptr && backend->texture_release(texture);
    }

    std::int64_t texture_get_pixel(std::uint64_t texture, std::int32_t x, std::int32_t y)
    {
        clear_last_error();
        Backend* backend = need("igpu_texture_get_pixel");
        return backend == nullptr ? 0 : backend->texture_get_pixel(texture, x, y);
    }

    std::int64_t texture_read(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer)
    {
        clear_last_error();
        Backend* backend = need("igpu_texture_read");
        return backend == nullptr ? 0 : backend->texture_read(texture, x, y, layer);
    }

    std::int64_t texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip)
    {
        clear_last_error();
        Backend* backend = need("igpu_texture_read_level");
        return backend == nullptr ? 0 : backend->texture_read_level(texture, x, y, layer, mip);
    }

    bool draw_to_texture(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_to_texture");
        return backend != nullptr && backend->draw_to_texture(
            vertex_buffer, layout, primitive, first_vertex, vertex_count, texture);
    }

    bool draw_to_texture_layer(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture,
        std::int32_t layer)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_to_texture_layer");
        return backend != nullptr && backend->draw_to_texture_layer(
            vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, layer);
    }

    bool draw_to_texture_level(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture,
        std::int32_t layer,
        std::int32_t mip)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_to_texture_level");
        return backend != nullptr && backend->draw_to_texture_level(
            vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, layer, mip);
    }

    bool dispatch(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture)
    {
        clear_last_error();
        Backend* backend = need("igpu_dispatch");
        return backend != nullptr && backend->dispatch(groups_x, groups_y, groups_z, storage_texture);
    }

    bool dispatch_level(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture, std::int32_t mip)
    {
        clear_last_error();
        Backend* backend = need("igpu_dispatch_level");
        return backend != nullptr && backend->dispatch_level(groups_x, groups_y, groups_z, storage_texture, mip);
    }

    bool dispatch_buffer(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_buffer)
    {
        clear_last_error();
        Backend* backend = need("igpu_dispatch_buffer");
        return backend != nullptr && backend->dispatch_buffer(groups_x, groups_y, groups_z, storage_buffer);
    }

    bool dispatch_both(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                       std::uint64_t storage_texture, std::uint64_t storage_buffer)
    {
        clear_last_error();
        Backend* backend = need("igpu_dispatch_both");
        return backend != nullptr && backend->dispatch_both(
            groups_x, groups_y, groups_z, storage_texture, storage_buffer);
    }

    bool dispatch_writes(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                         const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets)
    {
        clear_last_error();
        Backend* backend = need("igpu_dispatch_writes");
        return backend != nullptr && backend->dispatch_writes(groups_x, groups_y, groups_z, kinds, targets);
    }

    bool draw_to_render_targets(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        const gm::wire::GMArrayView& targets)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_to_render_targets");
        return backend != nullptr && backend->draw_to_render_targets(
            vertex_buffer, layout, primitive, first_vertex, vertex_count, targets);
    }

    bool draw_to_render_targets_level(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        const gm::wire::GMArrayView& targets,
        const gm::wire::GMArrayView& mips)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_to_render_targets_level");
        return backend != nullptr && backend->draw_to_render_targets_level(
            vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, mips);
    }

    bool draw_to_render_targets_layer(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        const gm::wire::GMArrayView& targets,
        const gm::wire::GMArrayView& layers,
        const gm::wire::GMArrayView& mips)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_to_render_targets_layer");
        return backend != nullptr && backend->draw_to_render_targets_layer(
            vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, layers, mips);
    }

    bool draw_sampled(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture,
        std::int64_t sampler)
    {
        clear_last_error();
        Backend* backend = need("igpu_draw_sampled");
        return backend != nullptr && backend->draw_sampled(
            vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, sampler);
    }

    std::int64_t shader_compile(std::string_view source, std::string_view entry,
                                std::int32_t stage, std::string_view dialect)
    {
        clear_last_error();
        Backend* backend = need("igpu_shader_compile");
        return backend == nullptr ? 0 : backend->shader_compile(source, entry, stage, dialect);
    }

    bool shader_release(std::uint64_t shader)
    {
        Backend* backend = active_backend();
        if (backend == nullptr)
        {
            set_last_error("igpu_shader_release: unknown shader handle");
            return false;
        }
        return backend->shader_release(shader);
    }

    bool shader_bind(std::int64_t shader, std::int32_t stage)
    {
        clear_last_error();
        Backend* backend = need("igpu_shader_bind");
        return backend != nullptr && backend->shader_bind(shader, stage);
    }

    std::int64_t get_bound_shader(std::int32_t stage)
    {
        Backend* backend = active_backend();
        if (backend == nullptr)
        {
            clear_last_error();
            if (stage < 0 || stage > 7)
            {
                set_last_error("igpu_get_bound_shader: unknown stage value " + std::to_string(stage));
                return 0;
            }
            return 0;
        }
        return backend->get_bound_shader(stage);
    }

    std::int64_t buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride)
    {
        clear_last_error();
        Backend* backend = need("igpu_buffer_create");
        return backend == nullptr ? 0 : backend->buffer_create(size, usage, bind, stride);
    }

    bool buffer_write(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer data)
    {
        clear_last_error();
        Backend* backend = need("igpu_buffer_write");
        return backend != nullptr && backend->buffer_write(buffer, offset, data);
    }

    bool buffer_resize(std::uint64_t buffer, std::int64_t size)
    {
        clear_last_error();
        Backend* backend = need("igpu_buffer_resize");
        return backend != nullptr && backend->buffer_resize(buffer, size);
    }

    bool buffer_read(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer dest)
    {
        clear_last_error();
        Backend* backend = need("igpu_buffer_read");
        return backend != nullptr && backend->buffer_read(buffer, offset, dest);
    }

    std::int64_t buffer_size(std::uint64_t buffer)
    {
        Backend* backend = active_backend();
        return backend == nullptr ? 0 : backend->buffer_size(buffer);
    }

    bool buffer_release(std::uint64_t buffer)
    {
        Backend* backend = active_backend();
        if (backend == nullptr)
        {
            set_last_error("igpu_buffer_release: unknown buffer handle");
            return false;
        }
        return backend->buffer_release(buffer);
    }

    bool storage_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot)
    {
        clear_last_error();
        Backend* backend = need("igpu_storage_bind");
        return backend != nullptr && backend->storage_bind(buffer, stage, slot);
    }

    bool buffer_patch(std::uint64_t buffer, std::int64_t offset, const void* data,
                      std::size_t size, const char* entry)
    {
        clear_last_error();
        Backend* backend = need(entry);
        return backend != nullptr && backend->buffer_patch(buffer, offset, data, size, entry);
    }

    std::int64_t input_layout_create(
        std::int64_t shader,
        const gm::wire::GMArrayView& usage,
        const gm::wire::GMArrayView& type,
        std::int32_t element_count,
        std::int32_t stride)
    {
        clear_last_error();
        Backend* backend = need("igpu_input_layout_create");
        return backend == nullptr ? 0 : backend->input_layout_create(shader, usage, type, element_count, stride);
    }

    std::int64_t input_layout_create_step(
        std::int64_t shader,
        const gm::wire::GMArrayView& usage,
        const gm::wire::GMArrayView& type,
        const gm::wire::GMArrayView& step,
        std::int32_t element_count,
        std::int32_t vertex_stride,
        std::int32_t instance_stride)
    {
        clear_last_error();
        Backend* backend = need("igpu_input_layout_create");
        return backend == nullptr ? 0 : backend->input_layout_create_step(
            shader, usage, type, step, element_count, vertex_stride, instance_stride);
    }

    bool input_layout_release(std::uint64_t layout)
    {
        Backend* backend = active_backend();
        if (backend == nullptr)
        {
            set_last_error("igpu_input_layout_release: unknown layout handle");
            return false;
        }
        return backend->input_layout_release(layout);
    }
}
