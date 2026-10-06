#include "IGPU_native.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "igpu_backend.h"
#include "igpu_buffer.h"
#include "igpu_device.h"
#include "igpu_capabilities.h"
#include "igpu_draw.h"
#include "igpu_error.h"
#include "igpu_input_layout.h"
#include "igpu_query.h"
#include "igpu_reflect.h"
#include "igpu_shader.h"
#include "igpu_state.h"
#include "igpu_texture.h"

using namespace gm::wire;
using namespace gm_structs;
using namespace gm_enums;

namespace
{
    constexpr const char* kIgpuVersion = "0.5.0";

}

bool igpu_init(
    const gm::wire::GMValue& device,
    const gm::wire::GMValue& context,
    const gm::wire::GMValue& swapchain)
{
    igpu::clear_last_error();

    const auto read_pointer = [](const gm::wire::GMValue& value) -> void* {
        if (value.kind() != gm::wire::GMKind::Pointer)
        {
            return nullptr;
        }
        return reinterpret_cast<void*>(
            gm::byteio::readLe<std::uintptr_t>(value.data()));
    };

    auto* device_ptr = static_cast<ID3D11Device*>(read_pointer(device));
    auto* context_ptr = static_cast<ID3D11DeviceContext*>(read_pointer(context));
    auto* swapchain_ptr = static_cast<IDXGISwapChain*>(read_pointer(swapchain));

    return igpu::bind_device(device_ptr, context_ptr, swapchain_ptr);
}

bool igpu_bind_current()
{
#if defined(IGPU_HAS_OPENGL)
    return igpu::bind_current_context();
#else
    igpu::set_last_error("igpu_bind_current: this build has no OpenGL backend");
    return false;
#endif
}

void igpu_shutdown()
{
    igpu::release_all();
}

std::string igpu_version()
{
    return kIgpuVersion;
}

bool igpu_is_available()
{
    igpu::refresh_device_status();
    const auto& s = igpu::state();
    return s.initialised && !s.device_lost;
}

bool igpu_device_lost()
{
    return igpu::device_was_removed();
}

std::int32_t igpu_get_feature_level()
{
    const auto& s = igpu::state();
    if (!s.initialised || s.device == nullptr)
    {
        return static_cast<std::int32_t>(IgpuFeatureLevel::Unknown);
    }

    switch (s.feature_level)
    {
    case D3D_FEATURE_LEVEL_11_1:
        return static_cast<std::int32_t>(IgpuFeatureLevel::Level_11_1);
    case D3D_FEATURE_LEVEL_12_0:
        return static_cast<std::int32_t>(IgpuFeatureLevel::Level_12_0);
    case D3D_FEATURE_LEVEL_12_1:
        return static_cast<std::int32_t>(IgpuFeatureLevel::Level_12_1);
    default:
        return static_cast<std::int32_t>(IgpuFeatureLevel::Level_11_0);
    }
}

std::string igpu_get_adapter_description()
{
    const auto& s = igpu::state();
    if (s.adapter_desc_valid)
    {
        return igpu::narrow(s.adapter_desc.Description);
    }
    if (!s.renderer_name.empty())
    {
        return s.renderer_name;
    }
    return igpu::probed_device_name();
}

bool igpu_set_graphics_info(std::string_view vendor, std::string_view version, std::string_view renderer,
                            std::string_view shading_language, std::int32_t max_texture_size)
{
    return igpu::set_graphics_info(vendor, version, renderer, shading_language, max_texture_size);
}

std::int64_t igpu_get_video_memory()
{
    const auto& s = igpu::state();
    if (!s.adapter_desc_valid)
    {
        return 0;
    }
    return static_cast<std::int64_t>(s.adapter_desc.DedicatedVideoMemory);
}

std::int32_t igpu_get_backbuffer_width()
{
    return igpu::state().backbuffer_width;
}

std::int32_t igpu_get_backbuffer_height()
{
    return igpu::state().backbuffer_height;
}

// ---------------------------------------------------------------------------
// Capability query (Tier 3)
// ---------------------------------------------------------------------------

gm::wire::DataStream igpu_get_capabilities()
{
    return igpu::build_capabilities();
}

bool igpu_supports(std::int32_t capability)
{
    return igpu::supports(static_cast<igpu::Capability>(capability));
}

std::string igpu_get_shader_dialect()
{
    return igpu::shader_dialect();
}

// ---------------------------------------------------------------------------
// Runtime shader compilation
// ---------------------------------------------------------------------------

std::int64_t igpu_shader_compile(
    std::string_view source,
    std::string_view entry,
    std::int32_t stage,
    std::string_view dialect)
{
    return igpu::shader_compile(source, entry, stage, dialect);
}

std::int64_t igpu_shader_compile_vertex(
    std::string_view source,
    std::string_view entry,
    std::string_view dialect)
{
    return igpu_shader_compile(source, entry, 0, dialect);
}

std::int64_t igpu_shader_compile_pixel(
    std::string_view source,
    std::string_view entry,
    std::string_view dialect)
{
    return igpu_shader_compile(source, entry, 1, dialect);
}

std::int64_t igpu_shader_compile_compute(
    std::string_view source,
    std::string_view entry,
    std::string_view dialect)
{
    return igpu_shader_compile(source, entry, 2, dialect);
}

bool igpu_shader_release(std::uint64_t shader)
{
    return igpu::shader_release(shader);
}

bool igpu_shader_bind(std::int64_t shader, std::int32_t stage_raw)
{
    return igpu::shader_bind(shader, stage_raw);
}

std::int64_t igpu_get_bound_shader(std::int32_t stage_raw)
{
    return igpu::get_bound_shader(stage_raw);
}

std::string igpu_get_last_error()
{
    return igpu::last_error();
}

// ---------------------------------------------------------------------------
// Input layout
// ---------------------------------------------------------------------------

std::int64_t igpu_input_layout_create(
    std::int64_t shader,
    const gm::wire::GMArrayView& usage,
    const gm::wire::GMArrayView& type,
    const gm::wire::GMArrayView& step,
    std::int32_t element_count,
    std::int32_t vertex_stride,
    std::int32_t instance_stride)
{
    return igpu::input_layout_create_step(
        shader, usage, type, step, element_count, vertex_stride, instance_stride);
}

std::int64_t igpu_input_layout_create_step(
    std::int64_t shader,
    const gm::wire::GMArrayView& usage,
    const gm::wire::GMArrayView& type,
    const gm::wire::GMArrayView& step,
    std::int32_t element_count,
    std::int32_t vertex_stride,
    std::int32_t instance_stride)
{
    return igpu::input_layout_create_step(
        shader, usage, type, step, element_count, vertex_stride, instance_stride);
}

bool igpu_input_layout_release(std::uint64_t layout)
{
    return igpu::input_layout_release(layout);
}

// ---------------------------------------------------------------------------
// Buffers
// ---------------------------------------------------------------------------

std::int64_t igpu_buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind,
                               std::int32_t stride)
{
    return igpu::buffer_create(size, usage, bind, stride);
}

bool igpu_buffer_write(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer data)
{
    return igpu::buffer_write(buffer, offset, data);
}

bool igpu_buffer_resize(std::uint64_t buffer, std::int64_t size)
{
    return igpu::buffer_resize(buffer, size);
}

bool igpu_buffer_read(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer dest)
{
    return igpu::buffer_read(buffer, offset, dest);
}

std::int64_t igpu_buffer_size(std::uint64_t buffer)
{
    return igpu::buffer_size(buffer);
}

bool igpu_buffer_release(std::uint64_t buffer)
{
    return igpu::buffer_release(buffer);
}

bool igpu_storage_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot)
{
    return igpu::storage_bind(buffer, stage, slot);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

bool igpu_draw(
    std::uint64_t vertex_buffer,
    std::uint64_t instance_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    std::int64_t instance_count,
    std::int64_t blend_state,
    std::int64_t depth_state,
    std::int64_t raster_state,
    std::int64_t sampler_state)
{
    if (instance_count == 1 && instance_buffer == 0)
    {
        return igpu::draw(
            vertex_buffer, layout, primitive, first_vertex, vertex_count,
            blend_state, depth_state, raster_state, sampler_state);
    }
    return igpu::draw_instanced(
        vertex_buffer, instance_buffer, layout, primitive, first_vertex, vertex_count, instance_count,
        blend_state, depth_state, raster_state, sampler_state);
}

bool igpu_draw_instanced(
    std::uint64_t vertex_buffer,
    std::uint64_t instance_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    std::int64_t instance_count)
{
    return igpu::draw_instanced(
        vertex_buffer, instance_buffer, layout, primitive, first_vertex, vertex_count, instance_count,
        0, 0, 0, 0);
}

bool igpu_draw_indirect(
    std::uint64_t vertex_buffer,
    std::uint64_t instance_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::uint64_t args,
    std::int64_t args_offset,
    std::int64_t blend_state,
    std::int64_t depth_state,
    std::int64_t raster_state,
    std::int64_t sampler_state)
{
    return igpu::draw_indirect(
        vertex_buffer, instance_buffer, layout, primitive, args, args_offset,
        blend_state, depth_state, raster_state, sampler_state);
}

bool igpu_draw_patch(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t control_points,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    std::int64_t blend_state,
    std::int64_t depth_state,
    std::int64_t raster_state,
    std::int64_t sampler_state)
{
    return igpu::draw_patch(
        vertex_buffer, layout, control_points, first_vertex, vertex_count,
        blend_state, depth_state, raster_state, sampler_state);
}

bool igpu_draw_indexed_indirect(
    std::uint64_t vertex_buffer,
    std::uint64_t instance_buffer,
    std::uint64_t layout,
    std::uint64_t index_buffer,
    std::int32_t primitive,
    std::uint64_t args,
    std::int64_t args_offset,
    std::int64_t blend_state,
    std::int64_t depth_state,
    std::int64_t raster_state,
    std::int64_t sampler_state)
{
    return igpu::draw_indexed_indirect(
        vertex_buffer, instance_buffer, layout, index_buffer, primitive, args, args_offset,
        blend_state, depth_state, raster_state, sampler_state);
}

bool igpu_draw_indexed(
    std::uint64_t vertex_buffer,
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
    return igpu::draw_indexed(
        vertex_buffer, layout, index_buffer, primitive, first_index, index_count,
        blend_state, depth_state, raster_state, sampler_state);
}

std::int64_t igpu_blend_state_create(
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
    return igpu::blend_state_create(
        enabled, src, dest, equation, src_alpha, dest_alpha, equation_alpha,
        write_red, write_green, write_blue, write_alpha);
}

std::int64_t igpu_depth_state_create(
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
    return igpu::depth_state_create(
        depth_test, depth_write, depth_func, stencil_enable, stencil_func,
        stencil_fail, stencil_depth_fail, stencil_pass, stencil_ref,
        stencil_read_mask, stencil_write_mask);
}

std::int64_t igpu_raster_state_create(
    std::int32_t cull,
    std::int32_t fill,
    bool scissor,
    bool depth_clip)
{
    return igpu::raster_state_create(cull, fill, scissor, depth_clip);
}

std::int64_t igpu_sampler_state_create(
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
    return igpu::sampler_state_create_full(
        magnification, minification, mip, address_u, address_v, address_w,
        anisotropy, border, compare, level_offset, finest, coarsest);
}

std::int64_t igpu_sampler_state_create_address(std::int32_t filter, std::int32_t address, std::int32_t anisotropy)
{
    return igpu::sampler_state_create_address(filter, address, anisotropy);
}

std::int64_t igpu_sampler_state_create_border(std::int32_t filter, std::int32_t anisotropy, std::int32_t border)
{
    return igpu::sampler_state_create_border(filter, anisotropy, border);
}

std::int64_t igpu_sampler_state_create_axes(
    std::int32_t filter,
    std::int32_t address_u,
    std::int32_t address_v,
    std::int32_t address_w,
    std::int32_t anisotropy)
{
    return igpu::sampler_state_create_axes(filter, address_u, address_v, address_w, anisotropy);
}

std::int64_t igpu_sampler_state_create_axes_border(
    std::int32_t filter,
    std::int32_t address_u,
    std::int32_t address_v,
    std::int32_t address_w,
    std::int32_t anisotropy,
    std::int32_t border)
{
    return igpu::sampler_state_create_axes_border(filter, address_u, address_v, address_w, anisotropy, border);
}

std::int64_t igpu_sampler_state_create_axes_range(
    std::int32_t filter,
    std::int32_t address_u,
    std::int32_t address_v,
    std::int32_t address_w,
    std::int32_t anisotropy,
    float level_offset,
    float finest,
    float coarsest)
{
    return igpu::sampler_state_create_axes_range(
        filter, address_u, address_v, address_w, anisotropy, level_offset, finest, coarsest);
}

std::int64_t igpu_sampler_state_create_axes_border_range(
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
    return igpu::sampler_state_create_axes_border_range(
        filter, address_u, address_v, address_w, anisotropy, border, level_offset, finest, coarsest);
}

std::int64_t igpu_sampler_state_create_filters(
    std::int32_t magnification,
    std::int32_t minification,
    std::int32_t mip,
    std::int32_t address_u,
    std::int32_t address_v,
    std::int32_t address_w)
{
    return igpu::sampler_state_create_filters(magnification, minification, mip, address_u, address_v, address_w);
}

std::int64_t igpu_sampler_state_create_filters_border(
    std::int32_t magnification,
    std::int32_t minification,
    std::int32_t mip,
    std::int32_t address_u,
    std::int32_t address_v,
    std::int32_t address_w,
    std::int32_t border)
{
    return igpu::sampler_state_create_filters_border(
        magnification, minification, mip, address_u, address_v, address_w, border);
}

std::int64_t igpu_sampler_state_create_filters_offset(
    std::int32_t magnification,
    std::int32_t minification,
    std::int32_t mip,
    std::int32_t address_u,
    std::int32_t address_v,
    std::int32_t address_w,
    float level_offset)
{
    return igpu::sampler_state_create_filters_offset(
        magnification, minification, mip, address_u, address_v, address_w, level_offset);
}

std::int64_t igpu_sampler_state_create_filters_range(
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
    return igpu::sampler_state_create_filters_range(
        magnification, minification, mip, address_u, address_v, address_w, level_offset, finest, coarsest);
}

std::int64_t igpu_sampler_state_create_filters_border_range(
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
    return igpu::sampler_state_create_filters_border_range(
        magnification, minification, mip, address_u, address_v, address_w, border, level_offset, finest, coarsest);
}

std::int64_t igpu_sampler_state_create_compare(
    std::int32_t compare,
    std::int32_t magnification,
    std::int32_t minification,
    std::int32_t mip,
    std::int32_t address_u,
    std::int32_t address_v,
    std::int32_t address_w,
    std::int32_t anisotropy,
    float level_offset,
    float finest,
    float coarsest)
{
    return igpu::sampler_state_create_compare(
        compare, magnification, minification, mip, address_u, address_v, address_w,
        anisotropy, level_offset, finest, coarsest);
}

bool igpu_state_release(std::uint64_t state)
{
    return igpu::state_release(state);
}

bool igpu_draw_with_state(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    std::int64_t blend_state,
    std::int64_t depth_state,
    std::int64_t raster_state,
    std::int64_t sampler_state)
{
    return igpu::draw(
        vertex_buffer, layout, primitive, first_vertex, vertex_count,
        blend_state, depth_state, raster_state, sampler_state);
}

bool igpu_draw_indexed_with_state(
    std::uint64_t vertex_buffer,
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
    return igpu::draw_indexed(
        vertex_buffer, layout, index_buffer, primitive, first_index, index_count,
        blend_state, depth_state, raster_state, sampler_state);
}

std::int64_t igpu_texture_create(
    std::int32_t kind,
    std::int32_t width,
    std::int32_t height,
    std::int32_t depth,
    std::int32_t format,
    bool render_target,
    bool storage,
    std::int32_t mip_count)
{
    if (mip_count == 1)
    {
        return igpu::texture_create_kind(kind, width, height, depth, format, render_target, storage);
    }
    return igpu::texture_create_mips(kind, width, height, depth, format, storage, mip_count);
}

bool igpu_texture_release(std::uint64_t texture)
{
    return igpu::texture_release(texture);
}

std::int64_t igpu_texture_get_pixel(std::uint64_t texture, std::int32_t x, std::int32_t y)
{
    return igpu::texture_get_pixel(texture, x, y);
}

std::int64_t igpu_texture_create_kind(
    std::int32_t kind,
    std::int32_t width,
    std::int32_t height,
    std::int32_t depth,
    std::int32_t format,
    bool render_target,
    bool storage)
{
    return igpu::texture_create_kind(kind, width, height, depth, format, render_target, storage);
}

std::int64_t igpu_texture_create_mips(
    std::int32_t kind,
    std::int32_t width,
    std::int32_t height,
    std::int32_t depth,
    std::int32_t format,
    bool storage,
    std::int32_t mip_count)
{
    return igpu::texture_create_mips(kind, width, height, depth, format, storage, mip_count);
}

bool igpu_texture_generate_mips(std::uint64_t texture)
{
    return igpu::texture_generate_mips(texture);
}

std::int64_t igpu_texture_read(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip)
{
    return igpu::texture_read_level(texture, x, y, layer, mip);
}

std::int64_t igpu_texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip)
{
    return igpu::texture_read_level(texture, x, y, layer, mip);
}

bool igpu_draw_to_texture_layer(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    std::uint64_t texture,
    std::int32_t layer)
{
    return igpu::draw_to_texture_layer(
        vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, layer);
}

bool igpu_draw_to_texture_level(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    std::uint64_t texture,
    std::int32_t layer,
    std::int32_t mip)
{
    return igpu::draw_to_texture_level(
        vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, layer, mip);
}

bool igpu_dispatch(
    std::int32_t groups_x,
    std::int32_t groups_y,
    std::int32_t groups_z,
    const gm::wire::GMArrayView& kinds,
    const gm::wire::GMArrayView& targets,
    const gm::wire::GMArrayView& layers,
    const gm::wire::GMArrayView& mips)
{
    const std::size_t count = kinds.size();
    if (count < 1 || count > 8 || targets.size() != count || layers.size() != count || mips.size() != count)
    {
        igpu::set_last_error("igpu_dispatch: pass 1 to 8 entries, with equal array lengths");
        return false;
    }
    bool mip_set = false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (layers.as<std::int32_t>(i) != 0)
        {
            igpu::set_last_error("igpu_dispatch: a texture write rejects a non-zero layer");
            return false;
        }
        if (mips.as<std::int32_t>(i) != 0)
        {
            mip_set = true;
        }
    }
    if (!mip_set)
    {
        return igpu::dispatch_writes(groups_x, groups_y, groups_z, kinds, targets);
    }
    if (count == 1 && kinds.as<std::int32_t>(0) == 0)
    {
        return igpu::dispatch_level(
            groups_x, groups_y, groups_z, targets.as<std::uint64_t>(0), mips.as<std::int32_t>(0));
    }
    igpu::set_last_error("igpu_dispatch: a mip level is only accepted for a single texture write");
    return false;
}

bool igpu_dispatch_level(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture, std::int32_t mip)
{
    return igpu::dispatch_level(groups_x, groups_y, groups_z, storage_texture, mip);
}

bool igpu_dispatch_buffer(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_buffer)
{
    return igpu::dispatch_buffer(groups_x, groups_y, groups_z, storage_buffer);
}

bool igpu_dispatch_both(
    std::int32_t groups_x,
    std::int32_t groups_y,
    std::int32_t groups_z,
    std::uint64_t storage_texture,
    std::uint64_t storage_buffer)
{
    return igpu::dispatch_both(groups_x, groups_y, groups_z, storage_texture, storage_buffer);
}

bool igpu_dispatch_writes(
    std::int32_t groups_x,
    std::int32_t groups_y,
    std::int32_t groups_z,
    const gm::wire::GMArrayView& kinds,
    const gm::wire::GMArrayView& targets)
{
    return igpu::dispatch_writes(groups_x, groups_y, groups_z, kinds, targets);
}

std::int64_t igpu_query_create(std::int32_t kind)
{
    return static_cast<std::int64_t>(igpu::query_create(kind));
}

bool igpu_query_begin(std::uint64_t query)
{
    return igpu::query_begin(query);
}

bool igpu_query_end(std::uint64_t query)
{
    return igpu::query_end(query);
}

bool igpu_query_ready(std::uint64_t query)
{
    return igpu::query_ready(query);
}

std::int64_t igpu_query_result(std::uint64_t query)
{
    return igpu::query_result(query);
}

bool igpu_query_release(std::uint64_t query)
{
    return igpu::query_release(query);
}

std::int64_t igpu_timestamp_frequency()
{
    return igpu::timestamp_frequency();
}

std::int64_t igpu_fence_create()
{
    return static_cast<std::int64_t>(igpu::fence_create());
}

bool igpu_fence_signal(std::uint64_t fence)
{
    return igpu::fence_signal(fence);
}

bool igpu_fence_signaled(std::uint64_t fence)
{
    return igpu::fence_signaled(fence);
}

bool igpu_fence_release(std::uint64_t fence)
{
    return igpu::fence_release(fence);
}

bool igpu_draw_to_texture(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    std::uint64_t texture)
{
    return igpu::draw_to_texture(vertex_buffer, layout, primitive, first_vertex, vertex_count, texture);
}

bool igpu_draw_to_render_targets(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    const gm::wire::GMArrayView& targets,
    const gm::wire::GMArrayView& layers,
    const gm::wire::GMArrayView& mips,
    std::int64_t blend_state,
    std::int64_t depth_state,
    std::int64_t raster_state,
    std::int64_t sampler_state)
{
    return igpu::draw_to_render_targets_layer(
        vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, layers, mips,
        blend_state, depth_state, raster_state, sampler_state);
}

bool igpu_draw_to_render_targets_level(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    const gm::wire::GMArrayView& targets,
    const gm::wire::GMArrayView& mips)
{
    return igpu::draw_to_render_targets_level(
        vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, mips);
}

bool igpu_draw_to_render_targets_layer(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    const gm::wire::GMArrayView& targets,
    const gm::wire::GMArrayView& layers,
    const gm::wire::GMArrayView& mips)
{
    return igpu::draw_to_render_targets_layer(
        vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, layers, mips,
        0, 0, 0, 0);
}

bool igpu_draw_sampled(
    std::uint64_t vertex_buffer,
    std::uint64_t layout,
    std::int32_t primitive,
    std::int64_t first_vertex,
    std::int64_t vertex_count,
    std::uint64_t texture,
    std::int64_t blend_state,
    std::int64_t depth_state,
    std::int64_t raster_state,
    std::int64_t sampler_state)
{
    return igpu::draw_sampled(
        vertex_buffer, layout, primitive, first_vertex, vertex_count, texture,
        blend_state, depth_state, raster_state, sampler_state);
}

std::int32_t igpu_get_draw_count()
{
    return igpu::draw_count();
}

std::int32_t igpu_get_draw_restore_failures()
{
    return igpu::draw_restore_failures();
}

bool igpu_is_vertex_buffer_bound(std::uint64_t buffer)
{
    return igpu::is_vertex_buffer_bound(buffer);
}

std::int32_t igpu_shader_block_count(std::int64_t shader)
{
    return igpu::shader_block_count(shader);
}

std::string igpu_shader_block_name(std::int64_t shader, std::int32_t index)
{
    return igpu::shader_block_name(shader, index);
}

std::int32_t igpu_shader_block_size(std::int64_t shader, std::string_view block)
{
    return igpu::shader_block_size(shader, block);
}

std::int32_t igpu_shader_block_slot(std::int64_t shader, std::string_view block)
{
    return igpu::shader_block_slot(shader, block);
}

std::int32_t igpu_shader_member_count(std::int64_t shader, std::string_view block)
{
    return igpu::shader_member_count(shader, block);
}

std::string igpu_shader_member_name(std::int64_t shader, std::string_view block, std::int32_t index)
{
    return igpu::shader_member_name(shader, block, index);
}

std::int32_t igpu_shader_member_offset(std::int64_t shader, std::string_view block, std::string_view member)
{
    return igpu::shader_member_offset(shader, block, member);
}

std::int32_t igpu_shader_member_size(std::int64_t shader, std::string_view block, std::string_view member)
{
    return igpu::shader_member_size(shader, block, member);
}

std::int32_t igpu_shader_member_type(std::int64_t shader, std::string_view block, std::string_view member)
{
    return igpu::shader_member_type(shader, block, member);
}

std::int32_t igpu_shader_member_rows(std::int64_t shader, std::string_view block, std::string_view member)
{
    return igpu::shader_member_rows(shader, block, member);
}

std::int32_t igpu_shader_member_columns(std::int64_t shader, std::string_view block, std::string_view member)
{
    return igpu::shader_member_columns(shader, block, member);
}

std::int32_t igpu_shader_member_elements(std::int64_t shader, std::string_view block, std::string_view member)
{
    return igpu::shader_member_elements(shader, block, member);
}

std::vector<gm_structs::IgpuUniformBlock> igpu_shader_reflect(std::int64_t shader)
{
    igpu::clear_last_error();
    const std::int32_t blocks = igpu::shader_block_count(shader);
    if (!igpu::last_error().empty())
    {
        return {};
    }
    std::vector<gm_structs::IgpuUniformBlock> out;
    out.reserve(static_cast<std::size_t>(blocks));
    for (std::int32_t i = 0; i < blocks; ++i)
    {
        gm_structs::IgpuUniformBlock block;
        block.name = igpu::shader_block_name(shader, i);
        if (!igpu::last_error().empty())
        {
            return {};
        }
        block.size = igpu::shader_block_size(shader, block.name);
        block.slot = igpu::shader_block_slot(shader, block.name);
        const std::int32_t members = igpu::shader_member_count(shader, block.name);
        block.members.reserve(static_cast<std::size_t>(members));
        for (std::int32_t m = 0; m < members; ++m)
        {
            gm_structs::IgpuUniformMember member;
            member.name = igpu::shader_member_name(shader, block.name, m);
            member.offset = igpu::shader_member_offset(shader, block.name, member.name);
            member.size = igpu::shader_member_size(shader, block.name, member.name);
            member.type = igpu::shader_member_type(shader, block.name, member.name);
            member.rows = igpu::shader_member_rows(shader, block.name, member.name);
            member.columns = igpu::shader_member_columns(shader, block.name, member.name);
            member.elements = igpu::shader_member_elements(shader, block.name, member.name);
            block.members.push_back(std::move(member));
        }
        out.push_back(std::move(block));
    }
    return out;
}

bool igpu_uniform_write(std::uint64_t buffer, std::int64_t shader, std::string_view block,
                        std::string_view member, const gm::wire::GMArrayView& values)
{
    return igpu::uniform_write(buffer, shader, block, member, values);
}

bool igpu_uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot)
{
    return igpu::uniform_bind(buffer, stage, slot);
}
