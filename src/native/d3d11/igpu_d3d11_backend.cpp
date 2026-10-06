#include "igpu_d3d11.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../igpu_buffer.h"
#include "../igpu_device.h"
#include "../igpu_error.h"

namespace igpu
{
    namespace
    {
        constexpr std::int32_t kOcclusion = 0;
        constexpr std::int32_t kTimestamp = 1;

        struct Query
        {
            std::int32_t kind = kOcclusion;
            ID3D11Query* occlusion = nullptr;
            ID3D11Query* disjoint = nullptr;
            ID3D11Query* start = nullptr;
            ID3D11Query* end_stamp = nullptr;
            bool begun = false;
            bool ended = false;
        };

        struct Fence
        {
            ID3D11Query* event = nullptr;
            bool signaled = false;
        };

        class D3D11Backend final : public Backend
        {
        public:
            D3D11Backend(ID3D11Device* device, ID3D11DeviceContext* context)
                : device_(device), context_(context)
            {
            }

            ~D3D11Backend() override
            {
                restore_uniforms();
                restore_storage();
                for (auto& [id, query] : queries_)
                {
                    release_query(query);
                }
                for (auto& [id, fence] : fences_)
                {
                    if (fence.event != nullptr)
                    {
                        fence.event->Release();
                    }
                }
            }

            const char* name() const override { return "d3d11"; }

            bool occlusion() const override { return true; }
            bool timestamps() const override { return true; }
            bool fences() const override { return true; }
            std::int64_t timestamp_frequency() const override { return frequency_; }

            std::uint64_t query_create(std::int32_t kind) override
            {
                if (kind != kOcclusion && kind != kTimestamp)
                {
                    set_last_error("igpu_query_create: kind must be IgpuQueryKind.Occlusion or Timestamp");
                    return 0;
                }
                Query query;
                query.kind = kind;
                if (kind == kOcclusion)
                {
                    query.occlusion = make_query(D3D11_QUERY_OCCLUSION);
                    if (query.occlusion == nullptr)
                    {
                        return 0;
                    }
                }
                else
                {
                    query.disjoint = make_query(D3D11_QUERY_TIMESTAMP_DISJOINT);
                    query.start = make_query(D3D11_QUERY_TIMESTAMP);
                    query.end_stamp = make_query(D3D11_QUERY_TIMESTAMP);
                    if (query.disjoint == nullptr || query.start == nullptr || query.end_stamp == nullptr)
                    {
                        release_query(query);
                        return 0;
                    }
                }
                const std::uint64_t id = next_query_++;
                queries_.emplace(id, query);
                return id;
            }

            bool query_begin(std::uint64_t id) override
            {
                auto* query = find_query(id, "igpu_query_begin");
                if (query == nullptr)
                {
                    return false;
                }
                if (query->begun && !query->ended)
                {
                    set_last_error("igpu_query_begin: the query is already open");
                    return false;
                }
                if (query->kind == kOcclusion)
                {
                    context_->Begin(query->occlusion);
                }
                else
                {
                    context_->Begin(query->disjoint);
                    context_->End(query->start);
                }
                query->begun = true;
                query->ended = false;
                return true;
            }

            bool query_end(std::uint64_t id) override
            {
                auto* query = find_query(id, "igpu_query_end");
                if (query == nullptr)
                {
                    return false;
                }
                if (!query->begun || query->ended)
                {
                    set_last_error("igpu_query_end: begin the query first");
                    return false;
                }
                if (query->kind == kOcclusion)
                {
                    context_->End(query->occlusion);
                }
                else
                {
                    context_->End(query->end_stamp);
                    context_->End(query->disjoint);
                }
                query->ended = true;
                return true;
            }

            bool query_ready(std::uint64_t id) override
            {
                auto* query = find_query(id, "igpu_query_ready");
                if (query == nullptr || !query->ended)
                {
                    return false;
                }
                return collect(*query, true) == S_OK;
            }

            std::int64_t query_result(std::uint64_t id) override
            {
                auto* query = find_query(id, "igpu_query_result");
                if (query == nullptr)
                {
                    return 0;
                }
                if (!query->ended)
                {
                    set_last_error("igpu_query_result: end the query first");
                    return 0;
                }
                // A few flushes are enough for the tiny draws the tests issue.
                // Callers that need a non-blocking poll use igpu_query_ready.
                context_->Flush();
                HRESULT last = S_FALSE;
                for (int attempt = 0; attempt < 1000; ++attempt)
                {
                    last = collect(*query, true);
                    if (last == S_OK)
                    {
                        return query->kind == kOcclusion ? occlusion_result_ : timestamp_result_;
                    }
                    Sleep(1);
                }
                set_last_error("igpu_query_result: the result is not ready (hr=0x" + std::to_string(static_cast<unsigned>(last)) + ")");
                return 0;
            }

            bool query_release(std::uint64_t id) override
            {
                const auto it = queries_.find(id);
                if (it == queries_.end())
                {
                    set_last_error("igpu_query_release: unknown query handle");
                    return false;
                }
                release_query(it->second);
                queries_.erase(it);
                return true;
            }

            std::uint64_t fence_create() override
            {
                Fence fence;
                fence.event = make_query(D3D11_QUERY_EVENT);
                if (fence.event == nullptr)
                {
                    return 0;
                }
                const std::uint64_t id = next_fence_++;
                fences_.emplace(id, fence);
                return id;
            }

            bool fence_signal(std::uint64_t id) override
            {
                auto* fence = find_fence(id, "igpu_fence_signal");
                if (fence == nullptr)
                {
                    return false;
                }
                context_->End(fence->event);
                fence->signaled = true;
                return true;
            }

            bool fence_signaled(std::uint64_t id) override
            {
                auto* fence = find_fence(id, "igpu_fence_signaled");
                if (fence == nullptr || !fence->signaled)
                {
                    return false;
                }
                const HRESULT hr = context_->GetData(fence->event, nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
                return hr == S_OK;
            }

            bool fence_release(std::uint64_t id) override
            {
                const auto it = fences_.find(id);
                if (it == fences_.end())
                {
                    set_last_error("igpu_fence_release: unknown fence handle");
                    return false;
                }
                if (it->second.event != nullptr)
                {
                    it->second.event->Release();
                }
                fences_.erase(it);
                return true;
            }

            bool draw(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                      std::int64_t first_vertex, std::int64_t vertex_count,
                      std::int64_t blend_state, std::int64_t depth_state,
                      std::int64_t raster_state, std::int64_t sampler_state) override
            {
                return d3d11_impl::draw(vertex_buffer, layout, primitive, first_vertex, vertex_count,
                                        blend_state, depth_state, raster_state, sampler_state);
            }

            bool draw_instanced(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout,
                                std::int32_t primitive, std::int64_t first_vertex, std::int64_t vertex_count,
                                std::int64_t instance_count) override
            {
                return d3d11_impl::draw_instanced(
                    vertex_buffer, instance_buffer, layout, primitive, first_vertex, vertex_count, instance_count);
            }

            bool draw_indirect(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout,
                               std::int32_t primitive, std::uint64_t args, std::int64_t args_offset) override
            {
                return d3d11_impl::draw_indirect(
                    vertex_buffer, instance_buffer, layout, primitive, args, args_offset);
            }

            bool draw_patch(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t control_points,
                            std::int64_t first_vertex, std::int64_t vertex_count) override
            {
                return d3d11_impl::draw_patch(vertex_buffer, layout, control_points, first_vertex, vertex_count);
            }

            bool draw_indexed_indirect(std::uint64_t vertex_buffer, std::uint64_t instance_buffer,
                                       std::uint64_t layout, std::uint64_t index_buffer, std::int32_t primitive,
                                       std::uint64_t args, std::int64_t args_offset) override
            {
                return d3d11_impl::draw_indexed_indirect(
                    vertex_buffer, instance_buffer, layout, index_buffer, primitive, args, args_offset);
            }

            bool draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer,
                              std::int32_t primitive, std::int64_t first_index, std::int64_t index_count,
                              std::int64_t blend_state, std::int64_t depth_state,
                              std::int64_t raster_state, std::int64_t sampler_state) override
            {
                return d3d11_impl::draw_indexed(vertex_buffer, layout, index_buffer, primitive,
                                                first_index, index_count, blend_state, depth_state,
                                                raster_state, sampler_state);
            }

            std::int32_t draw_count() override { return d3d11_impl::draw_count(); }
            std::int32_t draw_restore_failures() override { return d3d11_impl::draw_restore_failures(); }
            bool is_vertex_buffer_bound(std::uint64_t buffer) override
            {
                return d3d11_impl::is_vertex_buffer_bound(buffer);
            }

            std::int64_t blend_state_create(bool enabled, std::int32_t src, std::int32_t dest, std::int32_t equation,
                                            std::int32_t src_alpha, std::int32_t dest_alpha, std::int32_t equation_alpha,
                                            bool write_red, bool write_green, bool write_blue, bool write_alpha) override
            {
                return d3d11_impl::blend_state_create(enabled, src, dest, equation, src_alpha, dest_alpha,
                                                      equation_alpha, write_red, write_green, write_blue, write_alpha);
            }

            std::int64_t depth_state_create(bool depth_test, bool depth_write, std::int32_t depth_func,
                                            bool stencil_enable, std::int32_t stencil_func, std::int32_t stencil_fail,
                                            std::int32_t stencil_depth_fail, std::int32_t stencil_pass,
                                            std::int32_t stencil_ref, std::int32_t stencil_read_mask,
                                            std::int32_t stencil_write_mask) override
            {
                return d3d11_impl::depth_state_create(depth_test, depth_write, depth_func, stencil_enable,
                                                      stencil_func, stencil_fail, stencil_depth_fail, stencil_pass,
                                                      stencil_ref, stencil_read_mask, stencil_write_mask);
            }

            std::int64_t raster_state_create(std::int32_t cull, std::int32_t fill, bool scissor, bool depth_clip) override
            {
                return d3d11_impl::raster_state_create(cull, fill, scissor, depth_clip);
            }

            std::int64_t sampler_state_create(std::int32_t filter, bool repeat, std::int32_t anisotropy) override
            {
                return d3d11_impl::sampler_state_create(filter, repeat, anisotropy);
            }

            std::int64_t sampler_state_create_address(std::int32_t filter, std::int32_t address, std::int32_t anisotropy) override
            {
                return d3d11_impl::sampler_state_create_address(filter, address, anisotropy);
            }

            std::int64_t sampler_state_create_border(std::int32_t filter, std::int32_t anisotropy,
                                                     float red, float green, float blue, float alpha) override
            {
                return d3d11_impl::sampler_state_create_border(filter, anisotropy, red, green, blue, alpha);
            }

            std::int64_t sampler_state_create_axes(std::int32_t filter, std::int32_t address_u, std::int32_t address_v,
                                                   std::int32_t address_w, std::int32_t anisotropy) override
            {
                return d3d11_impl::sampler_state_create_axes(filter, address_u, address_v, address_w, anisotropy);
            }

            std::int64_t sampler_state_create_axes_border(std::int32_t filter, std::int32_t address_u,
                                                          std::int32_t address_v, std::int32_t address_w,
                                                          std::int32_t anisotropy, float red, float green, float blue,
                                                          float alpha) override
            {
                return d3d11_impl::sampler_state_create_axes_border(
                    filter, address_u, address_v, address_w, anisotropy, red, green, blue, alpha);
            }

            std::int64_t sampler_state_create_axes_range(std::int32_t filter, std::int32_t address_u, std::int32_t address_v,
                                                         std::int32_t address_w, std::int32_t anisotropy,
                                                         float level_offset, float finest, float coarsest) override
            {
                return d3d11_impl::sampler_state_create_axes_range(
                    filter, address_u, address_v, address_w, anisotropy, level_offset, finest, coarsest);
            }

            std::int64_t sampler_state_create_axes_border_range(std::int32_t filter, std::int32_t address_u,
                                                                std::int32_t address_v, std::int32_t address_w,
                                                                std::int32_t anisotropy, float red, float green, float blue,
                                                                float alpha, float level_offset, float finest,
                                                                float coarsest) override
            {
                return d3d11_impl::sampler_state_create_axes_border_range(
                    filter, address_u, address_v, address_w, anisotropy, red, green, blue, alpha, level_offset, finest,
                    coarsest);
            }

            std::int64_t sampler_state_create_filters(std::int32_t magnification, std::int32_t minification, std::int32_t mip,
                                                      std::int32_t address_u, std::int32_t address_v, std::int32_t address_w) override
            {
                return d3d11_impl::sampler_state_create_filters(
                    magnification, minification, mip, address_u, address_v, address_w);
            }

            std::int64_t sampler_state_create_filters_border(std::int32_t magnification, std::int32_t minification,
                                                             std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                             std::int32_t address_w, float red, float green, float blue,
                                                             float alpha) override
            {
                return d3d11_impl::sampler_state_create_filters_border(
                    magnification, minification, mip, address_u, address_v, address_w, red, green, blue, alpha);
            }

            std::int64_t sampler_state_create_filters_offset(std::int32_t magnification, std::int32_t minification,
                                                             std::int32_t mip, std::int32_t address_u,
                                                             std::int32_t address_v, std::int32_t address_w,
                                                             float level_offset) override
            {
                return d3d11_impl::sampler_state_create_filters_offset(
                    magnification, minification, mip, address_u, address_v, address_w, level_offset);
            }

            std::int64_t sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                            std::int32_t mip, std::int32_t address_u,
                                                            std::int32_t address_v, std::int32_t address_w,
                                                            float level_offset, float finest, float coarsest) override
            {
                return d3d11_impl::sampler_state_create_filters_range(
                    magnification, minification, mip, address_u, address_v, address_w, level_offset, finest, coarsest);
            }

            std::int64_t sampler_state_create_filters_border_range(std::int32_t magnification, std::int32_t minification,
                                                                   std::int32_t mip, std::int32_t address_u,
                                                                   std::int32_t address_v, std::int32_t address_w, float red,
                                                                   float green, float blue, float alpha, float level_offset,
                                                                   float finest, float coarsest) override
            {
                return d3d11_impl::sampler_state_create_filters_border_range(
                    magnification, minification, mip, address_u, address_v, address_w, red, green, blue, alpha, level_offset,
                    finest, coarsest);
            }

            std::int64_t sampler_state_create_compare(std::int32_t compare, std::int32_t magnification,
                                                      std::int32_t minification, std::int32_t mip, std::int32_t address_u,
                                                      std::int32_t address_v, std::int32_t address_w) override
            {
                return d3d11_impl::sampler_state_create_compare(
                    compare, magnification, minification, mip, address_u, address_v, address_w);
            }

            bool state_release(std::uint64_t handle) override
            {
                return d3d11_impl::state_release(handle);
            }

            std::int64_t texture_create(std::int32_t width, std::int32_t height, std::int32_t format, bool render_target) override
            {
                return d3d11_impl::texture_create(width, height, format, render_target);
            }

            std::int64_t texture_create_kind(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                             std::int32_t format, bool render_target, bool storage) override
            {
                return d3d11_impl::texture_create_kind(kind, width, height, depth, format, render_target, storage);
            }

            std::int64_t texture_create_mips(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth,
                                             std::int32_t format, bool storage, std::int32_t mip_count) override
            {
                return d3d11_impl::texture_create_mips(kind, width, height, depth, format, storage, mip_count);
            }

            bool texture_generate_mips(std::uint64_t texture) override
            {
                return d3d11_impl::texture_generate_mips(texture);
            }

            bool texture_release(std::uint64_t texture) override
            {
                return d3d11_impl::texture_release(texture);
            }

            std::int64_t texture_get_pixel(std::uint64_t texture, std::int32_t x, std::int32_t y) override
            {
                return d3d11_impl::texture_get_pixel(texture, x, y);
            }

            std::int64_t texture_read(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer) override
            {
                return d3d11_impl::texture_read(texture, x, y, layer);
            }

            std::int64_t texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip) override
            {
                return d3d11_impl::texture_read_level(texture, x, y, layer, mip);
            }

            bool draw_to_texture(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                 std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture) override
            {
                return d3d11_impl::draw_to_texture(vertex_buffer, layout, primitive, first_vertex, vertex_count, texture);
            }

            bool draw_to_texture_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                       std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                                       std::int32_t layer) override
            {
                return d3d11_impl::draw_to_texture_layer(vertex_buffer, layout, primitive, first_vertex, vertex_count,
                                                         texture, layer);
            }

            bool draw_to_texture_level(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                       std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                                       std::int32_t layer, std::int32_t mip) override
            {
                return d3d11_impl::draw_to_texture_level(vertex_buffer, layout, primitive, first_vertex, vertex_count,
                                                         texture, layer, mip);
            }

            bool dispatch(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture) override
            {
                return d3d11_impl::dispatch(groups_x, groups_y, groups_z, storage_texture);
            }

            bool dispatch_level(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture, std::int32_t mip) override
            {
                return d3d11_impl::dispatch_level(groups_x, groups_y, groups_z, storage_texture, mip);
            }

            bool dispatch_buffer(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_buffer) override
            {
                return d3d11_impl::dispatch_buffer(groups_x, groups_y, groups_z, storage_buffer);
            }

            bool dispatch_both(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                               std::uint64_t storage_texture, std::uint64_t storage_buffer) override
            {
                return d3d11_impl::dispatch_both(groups_x, groups_y, groups_z, storage_texture, storage_buffer);
            }

            bool dispatch_writes(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z,
                                 const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets) override
            {
                return d3d11_impl::dispatch_writes(groups_x, groups_y, groups_z, kinds, targets);
            }

            bool draw_to_render_targets(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                        std::int64_t first_vertex, std::int64_t vertex_count,
                                        const gm::wire::GMArrayView& targets) override
            {
                return d3d11_impl::draw_to_render_targets(vertex_buffer, layout, primitive, first_vertex, vertex_count, targets);
            }

            bool draw_to_render_targets_level(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                              std::int64_t first_vertex, std::int64_t vertex_count,
                                              const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& mips) override
            {
                return d3d11_impl::draw_to_render_targets_level(
                    vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, mips);
            }

            bool draw_to_render_targets_layer(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                                              std::int64_t first_vertex, std::int64_t vertex_count,
                                              const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers,
                                              const gm::wire::GMArrayView& mips) override
            {
                return d3d11_impl::draw_to_render_targets_layer(
                    vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, layers, mips);
            }

            bool draw_sampled(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive,
                              std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture,
                              std::int64_t sampler) override
            {
                return d3d11_impl::draw_sampled(vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, sampler);
            }

            bool reflect_uniforms(const void* bytecode, std::size_t size, UniformLayout& out) override
            {
                return d3d11_impl::reflect_uniforms(bytecode, size, out);
            }

            bool uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot) override
            {
                if (stage < 0 || stage > 5)
                {
                    set_last_error("igpu_uniform_bind: stage is not one this backend can bind a uniform buffer to");
                    return false;
                }
                if (slot < 0 || slot >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
                {
                    set_last_error("igpu_uniform_bind: slot is outside the range this backend can bind");
                    return false;
                }

                const auto existing = std::find_if(
                    claimed_uniforms_.begin(), claimed_uniforms_.end(),
                    [&](const ClaimedUniform& claimed) {
                        return claimed.stage == stage && claimed.slot == slot;
                    });

                if (buffer == 0)
                {
                    if (existing == claimed_uniforms_.end())
                    {
                        return true;
                    }
                    set_constant_buffer(stage, static_cast<UINT>(slot), existing->previous);
                    if (existing->previous != nullptr)
                    {
                        existing->previous->Release();
                    }
                    claimed_uniforms_.erase(existing);
                    return true;
                }

                const auto* entry = find_buffer(buffer);
                if (entry == nullptr || entry->object == nullptr)
                {
                    set_last_error("igpu_uniform_bind: unknown buffer handle");
                    return false;
                }
                if ((entry->bind & static_cast<std::int32_t>(BufferBind::Uniform)) == 0)
                {
                    set_last_error("igpu_uniform_bind: the buffer was not created with IgpuBufferBind.Uniform");
                    return false;
                }

                if (existing == claimed_uniforms_.end())
                {
                    ID3D11Buffer* previous = nullptr;
                    get_constant_buffer(stage, static_cast<UINT>(slot), &previous);
                    claimed_uniforms_.push_back(ClaimedUniform{ stage, slot, previous });
                }
                set_constant_buffer(stage, static_cast<UINT>(slot), entry->object);
                return true;
            }

            bool storage_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot) override
            {
                if (stage < 0 || stage > 5)
                {
                    set_last_error("igpu_storage_bind: stage is not one this backend can bind a storage buffer to");
                    return false;
                }
                if (slot < 0 || slot >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
                {
                    set_last_error("igpu_storage_bind: slot is outside the range this backend can bind");
                    return false;
                }

                const auto existing = std::find_if(
                    claimed_storage_.begin(), claimed_storage_.end(),
                    [&](const ClaimedStorage& claimed) {
                        return claimed.stage == stage && claimed.slot == slot;
                    });

                if (buffer == 0)
                {
                    if (existing == claimed_storage_.end())
                    {
                        return true;
                    }
                    set_shader_resource(stage, static_cast<UINT>(slot), existing->previous);
                    if (existing->previous != nullptr)
                    {
                        existing->previous->Release();
                    }
                    claimed_storage_.erase(existing);
                    return true;
                }

                auto* entry = find_buffer(buffer);
                if (entry == nullptr || entry->shader_view == nullptr)
                {
                    set_last_error("igpu_storage_bind: the buffer was not created with IgpuBufferBind.Storage");
                    return false;
                }
                if (existing == claimed_storage_.end())
                {
                    ID3D11ShaderResourceView* previous = nullptr;
                    get_shader_resource(stage, static_cast<UINT>(slot), &previous);
                    claimed_storage_.push_back(ClaimedStorage{ stage, slot, previous });
                }
                set_shader_resource(stage, static_cast<UINT>(slot), entry->shader_view);
                return true;
            }

            std::int64_t shader_compile(std::string_view source, std::string_view entry,
                                        std::int32_t stage, std::string_view dialect) override
            {
                return d3d11_impl::shader_compile(source, entry, stage, dialect);
            }
            bool shader_release(std::uint64_t shader) override { return d3d11_impl::shader_release(shader); }
            bool shader_bind(std::int64_t shader, std::int32_t stage) override
            {
                return d3d11_impl::shader_bind(shader, stage);
            }
            std::int64_t get_bound_shader(std::int32_t stage) override { return d3d11_impl::get_bound_shader(stage); }

            std::int64_t buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride) override
            {
                return d3d11_impl::buffer_create(size, usage, bind, stride);
            }
            bool buffer_write(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& data) override
            {
                return d3d11_impl::buffer_write(buffer, offset, data);
            }
            bool buffer_resize(std::uint64_t buffer, std::int64_t size) override
            {
                return d3d11_impl::buffer_resize(buffer, size);
            }
            bool buffer_read(std::uint64_t buffer, std::int64_t offset, const gm::wire::GMBuffer& dest) override
            {
                return d3d11_impl::buffer_read(buffer, offset, dest);
            }
            std::int64_t buffer_size(std::uint64_t buffer) override { return d3d11_impl::buffer_size(buffer); }
            bool buffer_release(std::uint64_t buffer) override { return d3d11_impl::buffer_release(buffer); }
            bool buffer_patch(std::uint64_t buffer, std::int64_t offset, const void* data,
                              std::size_t size, const char* entry) override
            {
                return d3d11_impl::buffer_patch(buffer, offset, data, size, entry);
            }

            std::int64_t input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                             const gm::wire::GMArrayView& type, std::int32_t element_count,
                                             std::int32_t stride) override
            {
                return d3d11_impl::input_layout_create(shader, usage, type, element_count, stride);
            }

            std::int64_t input_layout_create_step(std::int64_t shader, const gm::wire::GMArrayView& usage,
                                                  const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step,
                                                  std::int32_t element_count, std::int32_t vertex_stride,
                                                  std::int32_t instance_stride) override
            {
                return d3d11_impl::input_layout_create_step(
                    shader, usage, type, step, element_count, vertex_stride, instance_stride);
            }
            bool input_layout_release(std::uint64_t layout) override
            {
                return d3d11_impl::input_layout_release(layout);
            }

            bool texture_format(std::int32_t format) const override
            {
                return d3d11_impl::texture_format(format);
            }

        private:
            ID3D11Query* make_query(D3D11_QUERY type)
            {
                D3D11_QUERY_DESC desc{};
                desc.Query = type;
                desc.MiscFlags = 0;
                ID3D11Query* query = nullptr;
                if (FAILED(device_->CreateQuery(&desc, &query)) || query == nullptr)
                {
                    set_last_error("igpu_query_create: the device rejected this query");
                    return nullptr;
                }
                return query;
            }

            static void release_query(Query& query)
            {
                if (query.occlusion != nullptr) query.occlusion->Release();
                if (query.disjoint != nullptr) query.disjoint->Release();
                if (query.start != nullptr) query.start->Release();
                if (query.end_stamp != nullptr) query.end_stamp->Release();
                query.occlusion = nullptr;
                query.disjoint = nullptr;
                query.start = nullptr;
                query.end_stamp = nullptr;
            }

            Query* find_query(std::uint64_t id, const char* entry)
            {
                const auto it = queries_.find(id);
                if (it == queries_.end())
                {
                    set_last_error(std::string(entry) + ": unknown query handle");
                    return nullptr;
                }
                return &it->second;
            }

            Fence* find_fence(std::uint64_t id, const char* entry)
            {
                const auto it = fences_.find(id);
                if (it == fences_.end())
                {
                    set_last_error(std::string(entry) + ": unknown fence handle");
                    return nullptr;
                }
                return &it->second;
            }

            // S_OK means the numbers below were filled in. S_FALSE means not yet.
            HRESULT collect(Query& query, bool no_flush)
            {
                const UINT flags = no_flush ? D3D11_ASYNC_GETDATA_DONOTFLUSH : 0;
                if (query.kind == kOcclusion)
                {
                    UINT64 samples = 0;
                    const HRESULT hr = context_->GetData(query.occlusion, &samples, sizeof(samples), flags);
                    if (hr == S_OK)
                    {
                        occlusion_result_ = static_cast<std::int64_t>(samples);
                    }
                    return hr;
                }

                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
                const HRESULT disjoint_hr = context_->GetData(query.disjoint, &disjoint, sizeof(disjoint), flags);
                if (disjoint_hr != S_OK)
                {
                    return disjoint_hr;
                }
                UINT64 start = 0;
                UINT64 end = 0;
                if (context_->GetData(query.start, &start, sizeof(start), flags) != S_OK)
                {
                    return S_FALSE;
                }
                if (context_->GetData(query.end_stamp, &end, sizeof(end), flags) != S_OK)
                {
                    return S_FALSE;
                }
                if (disjoint.Disjoint)
                {
                    set_last_error("igpu_query_result: the timestamp was interrupted and has no value");
                    timestamp_result_ = 0;
                    return S_OK;
                }
                frequency_ = static_cast<std::int64_t>(disjoint.Frequency);
                timestamp_result_ = static_cast<std::int64_t>(end - start);
                return S_OK;
            }

            struct ClaimedUniform
            {
                std::int32_t stage = 0;
                std::int32_t slot = 0;
                ID3D11Buffer* previous = nullptr;
            };

            void get_constant_buffer(std::int32_t stage, UINT slot, ID3D11Buffer** out)
            {
                switch (stage)
                {
                case 0: context_->VSGetConstantBuffers(slot, 1, out); break;
                case 1: context_->PSGetConstantBuffers(slot, 1, out); break;
                case 2: context_->CSGetConstantBuffers(slot, 1, out); break;
                case 3: context_->GSGetConstantBuffers(slot, 1, out); break;
                case 4: context_->HSGetConstantBuffers(slot, 1, out); break;
                case 5: context_->DSGetConstantBuffers(slot, 1, out); break;
                default: break;
                }
            }

            void set_constant_buffer(std::int32_t stage, UINT slot, ID3D11Buffer* buffer)
            {
                switch (stage)
                {
                case 0: context_->VSSetConstantBuffers(slot, 1, &buffer); break;
                case 1: context_->PSSetConstantBuffers(slot, 1, &buffer); break;
                case 2: context_->CSSetConstantBuffers(slot, 1, &buffer); break;
                case 3: context_->GSSetConstantBuffers(slot, 1, &buffer); break;
                case 4: context_->HSSetConstantBuffers(slot, 1, &buffer); break;
                case 5: context_->DSSetConstantBuffers(slot, 1, &buffer); break;
                default: break;
                }
            }

            struct ClaimedStorage
            {
                std::int32_t stage = 0;
                std::int32_t slot = 0;
                ID3D11ShaderResourceView* previous = nullptr;
            };

            void get_shader_resource(std::int32_t stage, UINT slot, ID3D11ShaderResourceView** out)
            {
                switch (stage)
                {
                case 0: context_->VSGetShaderResources(slot, 1, out); break;
                case 1: context_->PSGetShaderResources(slot, 1, out); break;
                case 2: context_->CSGetShaderResources(slot, 1, out); break;
                case 3: context_->GSGetShaderResources(slot, 1, out); break;
                case 4: context_->HSGetShaderResources(slot, 1, out); break;
                case 5: context_->DSGetShaderResources(slot, 1, out); break;
                default: break;
                }
            }

            void set_shader_resource(std::int32_t stage, UINT slot, ID3D11ShaderResourceView* view)
            {
                switch (stage)
                {
                case 0: context_->VSSetShaderResources(slot, 1, &view); break;
                case 1: context_->PSSetShaderResources(slot, 1, &view); break;
                case 2: context_->CSSetShaderResources(slot, 1, &view); break;
                case 3: context_->GSSetShaderResources(slot, 1, &view); break;
                case 4: context_->HSSetShaderResources(slot, 1, &view); break;
                case 5: context_->DSSetShaderResources(slot, 1, &view); break;
                default: break;
                }
            }

            void restore_storage()
            {
                for (auto& claimed : claimed_storage_)
                {
                    if (context_ != nullptr)
                    {
                        set_shader_resource(claimed.stage, static_cast<UINT>(claimed.slot), claimed.previous);
                    }
                    if (claimed.previous != nullptr)
                    {
                        claimed.previous->Release();
                        claimed.previous = nullptr;
                    }
                }
                claimed_storage_.clear();
            }

            void restore_uniforms()
            {
                for (auto& claimed : claimed_uniforms_)
                {
                    if (context_ != nullptr)
                    {
                        set_constant_buffer(claimed.stage, static_cast<UINT>(claimed.slot), claimed.previous);
                    }
                    if (claimed.previous != nullptr)
                    {
                        claimed.previous->Release();
                        claimed.previous = nullptr;
                    }
                }
                claimed_uniforms_.clear();
            }

            ID3D11Device* device_ = nullptr;
            ID3D11DeviceContext* context_ = nullptr;
            std::vector<ClaimedUniform> claimed_uniforms_;
            std::vector<ClaimedStorage> claimed_storage_;
            std::unordered_map<std::uint64_t, Query> queries_;
            std::unordered_map<std::uint64_t, Fence> fences_;
            std::uint64_t next_query_ = 1;
            std::uint64_t next_fence_ = 1;
            std::int64_t frequency_ = 0;
            std::int64_t occlusion_result_ = 0;
            std::int64_t timestamp_result_ = 0;
        };
    }

    std::unique_ptr<Backend> make_d3d11_backend(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        return std::make_unique<D3D11Backend>(device, context);
    }
}
