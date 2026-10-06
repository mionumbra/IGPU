#pragma once

#include <cstdint>

namespace igpu
{
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
        bool write_alpha);

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
        std::int32_t stencil_write_mask);

    std::int64_t raster_state_create(
        std::int32_t cull,
        std::int32_t fill,
        bool scissor,
        bool depth_clip);

    std::int64_t sampler_state_create(
        std::int32_t filter,
        bool repeat,
        std::int32_t anisotropy);

    std::int64_t sampler_state_create_address(
        std::int32_t filter,
        std::int32_t address,
        std::int32_t anisotropy);

    // `border` is an opaque GameMaker colour. The facade turns it into
    // 0..1 channels before the backend sees it.
    std::int64_t sampler_state_create_border(
        std::int32_t filter,
        std::int32_t anisotropy,
        std::int32_t border);

    std::int64_t sampler_state_create_axes(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy);

    std::int64_t sampler_state_create_axes_border(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        std::int32_t border);

    // `level_offset`, `finest` and `coarsest` match the filter-range sampler.
    // The facade rejects a non-finite value and a finest above coarsest.
    std::int64_t sampler_state_create_axes_range(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        float level_offset,
        float finest,
        float coarsest);

    // `border` is a GameMaker colour. The facade unpacks it before the backend.
    std::int64_t sampler_state_create_axes_border_range(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        std::int32_t border,
        float level_offset,
        float finest,
        float coarsest);

    std::int64_t sampler_state_create_filters(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w);

    std::int64_t sampler_state_create_filters_border(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t border);

    // `level_offset` is a number of levels added to the one chosen from the
    // sample's size. Positive is coarser. The facade rejects non-finite values.
    std::int64_t sampler_state_create_filters_offset(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        float level_offset);

    // `finest` and `coarsest` bound the level after `level_offset` is applied.
    // The facade rejects a non-finite value and a finest above coarsest.
    std::int64_t sampler_state_create_filters_range(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        float level_offset,
        float finest,
        float coarsest);

    // `border` is a GameMaker colour. The facade unpacks it before the backend.
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
        float coarsest);

    // `compare` is a cmpfunc_* value. The sample is 1 or 0, then blended by
    // the three filters. Border addressing is rejected.
    // One sampler. compare 0 is ordinary. coarsest < 0 means no upper limit.
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
        float coarsest);

    std::int64_t sampler_state_create_compare(
        std::int32_t compare,
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w);

    bool state_release(std::uint64_t handle);
}
