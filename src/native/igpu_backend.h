#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "igpu_reflect.h"

namespace gm
{
    namespace wire
    {
        struct GMArrayView;
        struct GMBuffer;
    }
}

namespace igpu
{
    // The GML entry points talk to this. A backend is chosen once, when the
    // device is bound. Adding a backend means a new subclass and a new branch
    // at that one choice. Draw, texture, pipeline state, query, timestamp and
    // fence calls do not name a graphics API. So do shader compile, buffers,
    // input layouts, uniform-block reflection and binding a uniform buffer.
    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual const char* name() const = 0;

        virtual bool occlusion() const = 0;
        virtual bool timestamps() const = 0;
        virtual bool fences() const = 0;

        // Ticks per second. 0 until a timestamp query has completed.
        virtual std::int64_t timestamp_frequency() const = 0;

        // kind: 0 occlusion (samples that passed), 1 timestamp (tick delta).
        virtual std::uint64_t query_create(std::int32_t kind) = 0;
        virtual bool query_begin(std::uint64_t query) = 0;
        virtual bool query_end(std::uint64_t query) = 0;
        virtual bool query_ready(std::uint64_t query) = 0;
        virtual std::int64_t query_result(std::uint64_t query) = 0;
        virtual bool query_release(std::uint64_t query) = 0;

        virtual std::uint64_t fence_create() = 0;
        virtual bool fence_signal(std::uint64_t fence) = 0;
        virtual bool fence_signaled(std::uint64_t fence) = 0;
        virtual bool fence_release(std::uint64_t fence) = 0;

        // No default arguments: a virtual call uses the static type's defaults,
        // and the public igpu:: overloads already supply those.
        virtual bool draw(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                          std::int64_t first_vertex, std::int64_t vertex_count,
                          std::int64_t blend_state, std::int64_t depth_state,
                          std::int64_t raster_state, std::int64_t sampler_state) = 0;
        virtual bool draw_instanced(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout,
                                    std::int32_t primitive, std::int64_t first_vertex, std::int64_t vertex_count,
                                    std::int64_t instance_count) = 0;
        virtual bool draw_indirect(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout,
                                   std::int32_t primitive, std::uint64_t args, std::int64_t args_offset) = 0;
        virtual bool draw_patch(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t control_points,
                                std::int64_t first_vertex, std::int64_t vertex_count) = 0;
        virtual bool draw_indexed_indirect(std::uint64_t vertex_buffer, std::uint64_t instance_buffer,
                                           std::uint64_t layout, std::uint64_t index_buffer, std::int32_t primitive,
                                           std::uint64_t args, std::int64_t args_offset) = 0;
        virtual bool draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer,
                                  std::int32_t primitive, std::int64_t first_index, std::int64_t index_count,
                                  std::int64_t blend_state, std::int64_t depth_state,
                                  std::int64_t raster_state, std::int64_t sampler_state) = 0;
        virtual std::int32_t draw_count() = 0;
        virtual std::int32_t draw_restore_failures() = 0;
        virtual bool is_vertex_buffer_bound(std::uint64_t buffer) = 0;

        virtual std::int64_t blend_state_create(bool enabled, std::int32_t src, std::int32_t dest, std::int32_t equation,
                                                std::int32_t src_alpha, std::int32_t dest_alpha, std::int32_t equation_alpha,
                                                bool write_red, bool write_green, bool write_blue, bool write_alpha) = 0;
        virtual std::int64_t depth_state_create(bool depth_test, bool depth_write, std::int32_t depth_func,
                                                bool stencil_enable, std::int32_t stencil_func, std::int32_t stencil_fail,
                                                std::int32_t stencil_depth_fail, std::int32_t stencil_pass,
                                                std::int32_t stencil_ref, std::int32_t stencil_read_mask,
                                                std::int32_t stencil_write_mask) = 0;
        virtual std::int64_t raster_state_create(std::int32_t cull, std::int32_t fill, bool scissor, bool depth_clip) = 0;
        virtual std::int64_t sampler_state_create(std::int32_t filter, bool repeat, std::int32_t anisotropy) = 0;
        virtual std::int64_t sampler_state_create_address(std::int32_t filter, std::int32_t address, std::int32_t anisotropy) = 0;
        // Channels are 0..1. The public call passes a GameMaker colour; the
        // facade converts it. Alpha is 1 because that colour has no alpha.
        virtual std::int64_t sampler_state_create_border(std::int32_t filter, std::int32_t anisotropy,
                                                         float red, float green, float blue, float alpha) = 0;
        virtual std::int64_t sampler_state_create_axes(std::int32_t filter, std::int32_t address_u, std::int32_t address_v,
                                                       std::int32_t address_w, std::int32_t anisotropy) = 0;
        virtual std::int64_t sampler_state_create_axes_border(std::int32_t filter, std::int32_t address_u,
                                                              std::int32_t address_v, std::int32_t address_w,
                                                              std::int32_t anisotropy, float red, float green, float blue,
                                                              float alpha) = 0;
        // `finest` and `coarsest` are level indices. The facade has already
        // rejected a non-finite value and a finest above coarsest.
        virtual std::int64_t sampler_state_create_axes_range(std::int32_t filter, std::int32_t address_u,
                                                             std::int32_t address_v, std::int32_t address_w,
                                                             std::int32_t anisotropy, float level_offset, float finest,
                                                             float coarsest) = 0;
        // Channels are 0..1. The facade unpacked the GameMaker colour and
        // rejected a non-finite level or a finest above coarsest.
        virtual std::int64_t sampler_state_create_axes_border_range(std::int32_t filter, std::int32_t address_u,
                                                                    std::int32_t address_v, std::int32_t address_w,
                                                                    std::int32_t anisotropy, float red, float green,
                                                                    float blue, float alpha, float level_offset,
                                                                    float finest, float coarsest) = 0;
        virtual std::int64_t sampler_state_create_filters(std::int32_t magnification, std::int32_t minification,
                                                          std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                          std::int32_t address_w) = 0;
        virtual std::int64_t sampler_state_create_filters_border(std::int32_t magnification, std::int32_t minification,
                                                                 std::int32_t mip, std::int32_t address_u,
                                                                 std::int32_t address_v, std::int32_t address_w,
                                                                 float red, float green, float blue, float alpha) = 0;
        // `level_offset` is a number of levels. Positive is coarser. The facade
        // has already rejected a non-finite value.
        virtual std::int64_t sampler_state_create_filters_offset(std::int32_t magnification, std::int32_t minification,
                                                                 std::int32_t mip, std::int32_t address_u,
                                                                 std::int32_t address_v, std::int32_t address_w,
                                                                 float level_offset) = 0;
        // `finest` and `coarsest` are level indices. The facade has already
        // rejected a non-finite value and a finest above coarsest.
        virtual std::int64_t sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                                std::int32_t mip, std::int32_t address_u,
                                                                std::int32_t address_v, std::int32_t address_w,
                                                                float level_offset, float finest, float coarsest) = 0;
        // Channels are 0..1. The facade unpacked the GameMaker colour and
        // rejected a non-finite level or a finest above coarsest.
        virtual std::int64_t sampler_state_create_filters_border_range(std::int32_t magnification,
                                                                       std::int32_t minification, std::int32_t mip,
                                                                       std::int32_t address_u, std::int32_t address_v,
                                                                       std::int32_t address_w, float red, float green,
                                                                       float blue, float alpha, float level_offset,
                                                                       float finest, float coarsest) = 0;
        // `compare` is a cmpfunc_* value. The facade has already required a device.
        virtual std::int64_t sampler_state_create_compare(std::int32_t compare, std::int32_t magnification,
                                                          std::int32_t minification, std::int32_t mip,
                                                          std::int32_t address_u, std::int32_t address_v,
                                                          std::int32_t address_w) = 0;
        virtual bool state_release(std::uint64_t handle) = 0;

        virtual std::int64_t texture_create(std::int32_t width, std::int32_t height, std::int32_t format, bool render_target) = 0;
        virtual std::int64_t texture_create_kind(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                                 std::int32_t format, bool render_target, bool storage) = 0;
        virtual std::int64_t texture_create_mips(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                                 std::int32_t format, bool storage, std::int32_t mip_count) = 0;
        virtual bool texture_generate_mips(std::uint64_t texture) = 0;
        virtual bool texture_release(std::uint64_t texture) = 0;
        virtual std::int64_t texture_get_pixel(std::uint64_t texture, std::int32_t x, std::int32_t y) = 0;
        virtual std::int64_t texture_read(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer) = 0;
        virtual std::int64_t texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip) = 0;
        virtual bool draw_to_texture(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                     std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture) = 0;
        virtual bool draw_to_texture_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                           std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                                           std::int32_t layer) = 0;
        virtual bool draw_to_texture_level(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                           std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                                           std::int32_t layer, std::int32_t mip) = 0;
        virtual bool dispatch(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture) = 0;
        virtual bool dispatch_level(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture, std::int32_t mip) = 0;
        virtual bool dispatch_buffer(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_buffer) = 0;
        virtual bool dispatch_both(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                                   std::uint64_t storage_texture, std::uint64_t storage_buffer) = 0;
        virtual bool dispatch_writes(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                                     const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets) = 0;
        virtual bool draw_to_render_targets(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                            std::int64_t first_vertex, std::int64_t vertex_count,
                                            const gm::wire::GMArrayView& targets) = 0;
        virtual bool draw_to_render_targets_level(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                                  std::int64_t first_vertex, std::int64_t vertex_count,
                                                  const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& mips) = 0;
        virtual bool draw_to_render_targets_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                                  std::int64_t first_vertex, std::int64_t vertex_count,
                                                  const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers,
                                                  const gm::wire::GMArrayView& mips) = 0;
        virtual bool draw_sampled(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                  std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                                  std::int64_t sampler) = 0;

        // Fills `out` from compiled shader bytecode. True with an empty layout
        // when the shader has no uniform blocks.
        virtual bool reflect_uniforms(const void* bytecode, std::size_t size, UniformLayout& out) = 0;

        // Binds a uniform buffer to `slot` for `stage`. Buffer 0 restores the
        // buffer that was on that slot before the first bind.
        virtual bool uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot) = 0;

        virtual std::int64_t shader_compile(std::string_view source, std::string_view entry,
                                            std::int32_t stage, std::string_view dialect) = 0;
        virtual bool shader_release(std::uint64_t shader) = 0;
        virtual bool shader_bind(std::int64_t shader, std::int32_t stage) = 0;
        virtual std::int64_t get_bound_shader(std::int32_t stage) = 0;

        virtual std::int64_t buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind,
                                           std::int32_t stride) = 0;
        virtual bool buffer_write(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& data) = 0;
        virtual bool buffer_resize(std::uint64_t buffer, std::int64_t size) = 0;
        virtual bool buffer_read(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& dest) = 0;
        virtual std::int64_t buffer_size(std::uint64_t buffer) = 0;
        virtual bool buffer_release(std::uint64_t buffer) = 0;
        virtual bool storage_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot) = 0;
        virtual bool buffer_patch(std::uint64_t buffer, std::int64_t offset, const void* data,
                                  std::size_t size, const char* entry) = 0;

        virtual std::int64_t input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                                 const gm::wire::GMArrayView& type, std::int32_t element_count,
                                                 std::int32_t stride) = 0;
        virtual std::int64_t input_layout_create_step(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                                      const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step,
                                                      std::int32_t element_count, std::int32_t vertex_stride,
                                                      std::int32_t instance_stride) = 0;
        virtual bool input_layout_release(std::uint64_t layout) = 0;

        // True when a texture of this GameMaker surface_* format can be created.
        // A render target may still be rejected; that is a separate constraint.
        virtual bool texture_format(std::int32_t format) const = 0;
    };

    Backend* active_backend();
    void set_active_backend(std::unique_ptr<Backend> backend);
}
