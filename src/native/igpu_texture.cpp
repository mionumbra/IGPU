#include "igpu_texture.h"

#include <d3d11.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "d3d11/igpu_d3d11.h"
#include "igpu_device.h"
#include "igpu_draw.h"
#include "igpu_error.h"

namespace igpu
{
namespace d3d11_impl
{
    namespace
    {
        // surface_* constants match the engine's eTextureFormat values.
        bool format_to_dxgi(std::int32_t format, DXGI_FORMAT& out, bool render_target)
        {
            switch (format)
            {
            case 6:  out = DXGI_FORMAT_R8G8B8A8_UNORM; break;       // surface_rgba8unorm
            case 9:  out = DXGI_FORMAT_R16_FLOAT; break;            // surface_r16float
            case 10: out = DXGI_FORMAT_R32_FLOAT; break;            // surface_r32float
            case 11: out = DXGI_FORMAT_B4G4R4A4_UNORM; break;       // surface_rgba4unorm
            case 12: out = DXGI_FORMAT_R8_UNORM; break;             // surface_r8unorm
            case 13: out = DXGI_FORMAT_R8G8_UNORM; break;           // surface_rg8unorm
            case 14: out = DXGI_FORMAT_R16G16B16A16_FLOAT; break;   // surface_rgba16float
            case 15: out = DXGI_FORMAT_R32G32B32A32_FLOAT; break;   // surface_rgba32float
            default: return false;
            }
            // B4G4R4A4 cannot be a render target on D3D11.
            if (render_target && format == 11)
            {
                return false;
            }
            return true;
        }

        // Four colour targets, the same limit as the engine.
        constexpr UINT kMaxColorTargets = 4;

        // Saves every bound colour target plus the viewport and scissor, then
        // points them at `targets`. Setting fewer than four unbinds the rest.
        class TargetGuard
        {
        public:
            TargetGuard(ID3D11DeviceContext* context, ID3D11RenderTargetView* const* targets, UINT count, std::int32_t width, std::int32_t height)
                : context_(context)
            {
                if (context_ == nullptr || targets == nullptr || count == 0 || count > kMaxColorTargets)
                {
                    return;
                }
                context_->OMGetRenderTargets(kMaxColorTargets, old_rt_, &old_ds_);
                viewport_count_ = 1;
                context_->RSGetViewports(&viewport_count_, &old_viewport_);
                scissor_count_ = 1;
                context_->RSGetScissorRects(&scissor_count_, &old_scissor_);

                context_->OMSetRenderTargets(count, targets, nullptr);

                D3D11_VIEWPORT viewport{};
                viewport.TopLeftX = 0.0f;
                viewport.TopLeftY = 0.0f;
                viewport.Width = static_cast<float>(width);
                viewport.Height = static_cast<float>(height);
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                context_->RSSetViewports(1, &viewport);

                D3D11_RECT scissor{};
                scissor.left = 0;
                scissor.top = 0;
                scissor.right = width;
                scissor.bottom = height;
                context_->RSSetScissorRects(1, &scissor);
                active_ = true;
            }

            ~TargetGuard()
            {
                restore();
            }

            void restore()
            {
                if (!active_ || context_ == nullptr)
                {
                    return;
                }
                active_ = false;
                context_->OMSetRenderTargets(kMaxColorTargets, old_rt_, old_ds_);
                if (viewport_count_ > 0)
                {
                    context_->RSSetViewports(viewport_count_, &old_viewport_);
                }
                if (scissor_count_ > 0)
                {
                    context_->RSSetScissorRects(scissor_count_, &old_scissor_);
                }
                for (UINT i = 0; i < kMaxColorTargets; ++i)
                {
                    if (old_rt_[i] != nullptr)
                    {
                        old_rt_[i]->Release();
                        old_rt_[i] = nullptr;
                    }
                }
                if (old_ds_ != nullptr)
                {
                    old_ds_->Release();
                    old_ds_ = nullptr;
                }
            }

        private:
            ID3D11DeviceContext* context_ = nullptr;
            bool active_ = false;
            ID3D11RenderTargetView* old_rt_[kMaxColorTargets] = {};
            ID3D11DepthStencilView* old_ds_ = nullptr;
            UINT viewport_count_ = 0;
            D3D11_VIEWPORT old_viewport_{};
            UINT scissor_count_ = 0;
            D3D11_RECT old_scissor_{};
        };
    }

    namespace
    {
        std::int32_t bytes_per_pixel(std::int32_t format)
        {
            switch (format)
            {
            case 9: case 11: case 13: return 2;
            case 12: return 1;
            case 14: return 8;
            case 15: return 16;
            default: return 4;
            }
        }

        // Levels from the base size down to 1. A volume shrinks its depth too.
        // An array or a cube shrinks each layer, not the layer count.
        std::int32_t full_mip_count(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth)
        {
            std::int32_t edge = width > height ? width : height;
            if (kind == 1 && depth > edge)
            {
                edge = depth;
            }
            std::int32_t levels = 1;
            while (edge > 1)
            {
                edge >>= 1;
                ++levels;
            }
            return levels;
        }

        void zero_subresource(ID3D11DeviceContext* context, ID3D11Resource* texture, UINT subresource,
                              std::int32_t width, std::int32_t height, std::int32_t depth, std::int32_t format)
        {
            const std::int32_t bpp = bytes_per_pixel(format);
            const std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                                      static_cast<std::size_t>(depth) * static_cast<std::size_t>(bpp);
            std::vector<unsigned char> zeros(bytes, 0);
            context->UpdateSubresource(
                texture, subresource, nullptr, zeros.data(),
                static_cast<UINT>(width * bpp),
                static_cast<UINT>(width * height * bpp));
        }

        bool make_slice_view(ID3D11Device* device, const DeviceState::TextureEntry& entry, std::int32_t layer, std::int32_t mip, ID3D11RenderTargetView** out)
        {
            DXGI_FORMAT dxgi{};
            if (!format_to_dxgi(entry.format, dxgi, true))
            {
                return false;
            }
            D3D11_RENDER_TARGET_VIEW_DESC desc{};
            desc.Format = dxgi;
            if (entry.kind == 1)
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                desc.Texture3D.MipSlice = static_cast<UINT>(mip);
                desc.Texture3D.FirstWSlice = static_cast<UINT>(layer);
                desc.Texture3D.WSize = 1;
            }
            else if (entry.kind == 0)
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = static_cast<UINT>(mip);
            }
            else
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = static_cast<UINT>(mip);
                desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(layer);
                desc.Texture2DArray.ArraySize = 1;
            }
            return SUCCEEDED(device->CreateRenderTargetView(entry.texture, &desc, out)) && *out != nullptr;
        }

        // Array elements from GML literals arrive as int32, double, or uint64.
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

    std::int64_t texture_create_kind(
        std::int32_t kind,
        std::int32_t width,
        std::int32_t height,
        std::int32_t depth,
        std::int32_t format,
        bool render_target,
        bool storage,
        std::int32_t mip_count)
    {
        clear_last_error();
        if (!require_device("igpu_texture_create_kind"))
        {
            return 0;
        }
        if (kind < 0 || kind > 3)
        {
            set_last_error("igpu_texture_create_kind: kind must be an IgpuTextureKind value");
            return 0;
        }
        if (width <= 0 || height <= 0 || width > 16384 || height > 16384 || depth <= 0)
        {
            set_last_error("igpu_texture_create_kind: width and height must be in 1..16384, and depth must be positive");
            return 0;
        }
        if (kind == 0 && depth != 1)
        {
            set_last_error("igpu_texture_create_kind: a 2D texture has depth 1");
            return 0;
        }
        if (kind == 3 && (depth != 6 || width != height))
        {
            set_last_error("igpu_texture_create_kind: a cube texture is square and has 6 faces");
            return 0;
        }
        if (storage && state().feature_level < D3D_FEATURE_LEVEL_11_0)
        {
            set_last_error("igpu_texture_create_kind: this device cannot write storage textures");
            return 0;
        }

        const std::int32_t chain = full_mip_count(kind, width, height, depth);
        std::int32_t mips = 1;
        if (mip_count == 0)
        {
            mips = chain;
        }
        else if (mip_count < 1 || mip_count > chain)
        {
            set_last_error("igpu_texture_create_mips: mip count must be 0 for the full chain, or between 1 and that chain");
            return 0;
        }
        else
        {
            mips = mip_count;
        }
        const bool chain_mips = mips > 1;

        DXGI_FORMAT dxgi{};
        if (!format_to_dxgi(format, dxgi, render_target || chain_mips))
        {
            set_last_error(chain_mips
                ? "igpu_texture_create_mips: this format cannot have coarser levels"
                : "igpu_texture_create_kind: format must be a surface_* colour format the backend can create");
            return 0;
        }

        auto& s = state();
        ID3D11Resource* texture = nullptr;
        const UINT bind = D3D11_BIND_SHADER_RESOURCE |
                          ((render_target || chain_mips) ? D3D11_BIND_RENDER_TARGET : 0) |
                          (storage ? D3D11_BIND_UNORDERED_ACCESS : 0);
        const UINT generate = chain_mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
        HRESULT created = E_FAIL;
        if (kind == 1)
        {
            D3D11_TEXTURE3D_DESC desc{};
            desc.Width = static_cast<UINT>(width);
            desc.Height = static_cast<UINT>(height);
            desc.Depth = static_cast<UINT>(depth);
            desc.MipLevels = static_cast<UINT>(mips);
            desc.Format = dxgi;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = bind;
            desc.MiscFlags = generate;
            ID3D11Texture3D* volume = nullptr;
            created = s.device->CreateTexture3D(&desc, nullptr, &volume);
            texture = volume;
        }
        else
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = static_cast<UINT>(width);
            desc.Height = static_cast<UINT>(height);
            desc.MipLevels = static_cast<UINT>(mips);
            desc.ArraySize = static_cast<UINT>(kind == 0 ? 1 : depth);
            desc.Format = dxgi;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = bind;
            if (kind == 3)
            {
                desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | generate;
            }
            else
            {
                desc.MiscFlags = generate;
            }
            ID3D11Texture2D* image = nullptr;
            created = s.device->CreateTexture2D(&desc, nullptr, &image);
            texture = image;
        }
        if (FAILED(created) || texture == nullptr)
        {
            set_last_error("igpu_texture_create_kind: the device rejected this texture");
            return 0;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = dxgi;
        if (kind == 1)
        {
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            srv_desc.Texture3D.MipLevels = static_cast<UINT>(mips);
        }
        else if (kind == 2)
        {
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            srv_desc.Texture2DArray.MipLevels = static_cast<UINT>(mips);
            srv_desc.Texture2DArray.ArraySize = static_cast<UINT>(depth);
        }
        else if (kind == 3)
        {
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            srv_desc.TextureCube.MipLevels = static_cast<UINT>(mips);
        }
        else
        {
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = static_cast<UINT>(mips);
        }
        ID3D11ShaderResourceView* srv = nullptr;
        if (FAILED(s.device->CreateShaderResourceView(texture, &srv_desc, &srv)) || srv == nullptr)
        {
            texture->Release();
            set_last_error("igpu_texture_create_kind: could not create a shader view");
            return 0;
        }

        ID3D11UnorderedAccessView* uav = nullptr;
        if (storage)
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
            uav_desc.Format = dxgi;
            if (kind == 1)
            {
                uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                uav_desc.Texture3D.MipSlice = 0;
                uav_desc.Texture3D.FirstWSlice = 0;
                uav_desc.Texture3D.WSize = static_cast<UINT>(depth);
            }
            else if (kind == 2 || kind == 3)
            {
                // A cube has no separate writable-image shape. Face 0 is the
                // first layer, in the same order reads use.
                uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                uav_desc.Texture2DArray.MipSlice = 0;
                uav_desc.Texture2DArray.FirstArraySlice = 0;
                uav_desc.Texture2DArray.ArraySize = static_cast<UINT>(depth);
            }
            else
            {
                uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                uav_desc.Texture2D.MipSlice = 0;
            }
            if (FAILED(s.device->CreateUnorderedAccessView(texture, &uav_desc, &uav)) || uav == nullptr)
            {
                srv->Release();
                texture->Release();
                set_last_error("igpu_texture_create_kind: could not create a storage view");
                return 0;
            }
        }

        ID3D11RenderTargetView* rtv = nullptr;
        if (render_target && kind == 0)
        {
            if (!make_slice_view(s.device, DeviceState::TextureEntry{ texture, srv, nullptr, uav, kind, width, height, depth, format }, 0, 0, &rtv))
            {
                if (uav != nullptr) uav->Release();
                srv->Release();
                texture->Release();
                set_last_error("igpu_texture_create_kind: could not create a render-target view");
                return 0;
            }
        }

        const int layers = (kind == 1) ? 1 : depth;
        for (int layer = 0; layer < layers; ++layer)
        {
            for (int mip = 0; mip < mips; ++mip)
            {
                const std::int32_t mip_width = std::max(1, width >> mip);
                const std::int32_t mip_height = std::max(1, height >> mip);
                const std::int32_t mip_depth = (kind == 1) ? std::max(1, depth >> mip) : 1;
                const UINT subresource = static_cast<UINT>(mip + layer * mips);
                zero_subresource(s.context, texture, subresource, mip_width, mip_height, mip_depth, format);
            }
        }

        const std::uint64_t id = s.next_texture_id++;
        // A mip chain is allocated as a render target so a draw can aim at one
        // level. Storage is still allowed on the same resource.
        const bool drawable = render_target || chain_mips;
        s.textures.emplace(id, DeviceState::TextureEntry{ texture, srv, rtv, uav, kind, width, height, depth, format, drawable, mips });
        return static_cast<std::int64_t>(id);
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
        return texture_create_kind(kind, width, height, depth, format, false, storage, mip_count);
    }

    bool texture_generate_mips(std::uint64_t texture)
    {
        clear_last_error();
        if (!require_device("igpu_texture_generate_mips"))
        {
            return false;
        }
        auto* entry = find_texture(texture);
        if (entry == nullptr || entry->srv == nullptr)
        {
            set_last_error("igpu_texture_generate_mips: unknown texture handle");
            return false;
        }
        if (entry->mips < 2)
        {
            set_last_error("igpu_texture_generate_mips: the texture has only one level");
            return false;
        }
        state().context->GenerateMips(entry->srv);
        return true;
    }

    std::int64_t texture_create(std::int32_t width, std::int32_t height, std::int32_t format, bool render_target)
    {
        return texture_create_kind(0, width, height, 1, format, render_target, false);
    }

    bool texture_release(std::uint64_t texture)
    {
        clear_last_error();
        auto& s = state();
        const auto it = s.textures.find(texture);
        if (it == s.textures.end())
        {
            set_last_error("igpu_texture_release: unknown texture handle");
            return false;
        }
        if (it->second.rtv != nullptr)
        {
            it->second.rtv->Release();
        }
        if (it->second.srv != nullptr)
        {
            it->second.srv->Release();
        }
        if (it->second.uav != nullptr)
        {
            it->second.uav->Release();
        }
        if (it->second.texture != nullptr)
        {
            it->second.texture->Release();
        }
        s.textures.erase(it);
        return true;
    }

    std::int64_t texture_read_level(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip)
    {
        clear_last_error();
        const char* const api = "igpu_texture_read_level";
        if (!require_device(api))
        {
            return 0;
        }
        auto* entry = find_texture(texture);
        if (entry == nullptr)
        {
            set_last_error(std::string(api) + ": unknown texture handle");
            return 0;
        }
        if (entry->format != 6)
        {
            set_last_error(std::string(api) + ": only surface_rgba8unorm can be read back as a colour");
            return 0;
        }
        if (mip < 0 || mip >= entry->mips)
        {
            set_last_error(std::string(api) + ": mip level is outside the texture");
            return 0;
        }
        const std::int32_t level_width = std::max(1, entry->width >> mip);
        const std::int32_t level_height = std::max(1, entry->height >> mip);
        // A volume's depth halves with the level. Layers and faces do not.
        const std::int32_t level_depth = (entry->kind == 1) ? std::max(1, entry->depth >> mip) : entry->depth;
        if (layer < 0 || layer >= level_depth || x < 0 || y < 0 || x >= level_width || y >= level_height)
        {
            set_last_error(std::string(api) + ": pixel is outside that level");
            return 0;
        }

        const bool volume = entry->kind == 1;
        const UINT subresource = volume
            ? static_cast<UINT>(mip)
            : static_cast<UINT>(mip + layer * entry->mips);
        D3D11_BOX box{};
        box.left = static_cast<UINT>(x);
        box.right = box.left + 1;
        box.top = static_cast<UINT>(y);
        box.bottom = box.top + 1;
        box.front = volume ? static_cast<UINT>(layer) : 0u;
        box.back = box.front + 1;

        auto& s = state();
        ID3D11Resource* staging = nullptr;
        if (volume)
        {
            D3D11_TEXTURE3D_DESC staging_desc{};
            staging_desc.Width = 1;
            staging_desc.Height = 1;
            staging_desc.Depth = 1;
            staging_desc.MipLevels = 1;
            staging_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            staging_desc.Usage = D3D11_USAGE_STAGING;
            staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ID3D11Texture3D* volume_staging = nullptr;
            if (FAILED(s.device->CreateTexture3D(&staging_desc, nullptr, &volume_staging)) || volume_staging == nullptr)
            {
                set_last_error(std::string(api) + ": could not create a staging texture");
                return 0;
            }
            staging = volume_staging;
        }
        else
        {
            D3D11_TEXTURE2D_DESC staging_desc{};
            staging_desc.Width = 1;
            staging_desc.Height = 1;
            staging_desc.MipLevels = 1;
            staging_desc.ArraySize = 1;
            staging_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            staging_desc.SampleDesc.Count = 1;
            staging_desc.Usage = D3D11_USAGE_STAGING;
            staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ID3D11Texture2D* image_staging = nullptr;
            if (FAILED(s.device->CreateTexture2D(&staging_desc, nullptr, &image_staging)) || image_staging == nullptr)
            {
                set_last_error(std::string(api) + ": could not create a staging texture");
                return 0;
            }
            staging = image_staging;
        }

        s.context->CopySubresourceRegion(staging, 0, 0, 0, 0, entry->texture, subresource, &box);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(s.context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)) || mapped.pData == nullptr)
        {
            staging->Release();
            set_last_error(std::string(api) + ": could not map the pixel");
            return 0;
        }
        const auto pixel = *static_cast<const std::uint32_t*>(mapped.pData);
        s.context->Unmap(staging, 0);
        staging->Release();
        return static_cast<std::int64_t>(pixel & 0x00ffffffu);
    }

    std::int64_t texture_read(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer)
    {
        const std::int64_t pixel = texture_read_level(texture, x, y, layer, 0);
        if (!last_error().empty() && last_error().rfind("igpu_texture_read_level:", 0) == 0)
        {
            set_last_error("igpu_texture_read:" + last_error().substr(std::string("igpu_texture_read_level").size()));
        }
        return pixel;
    }

    std::int64_t texture_get_pixel(std::uint64_t texture, std::int32_t x, std::int32_t y)
    {
        const auto pixel = texture_read(texture, x, y, 0);
        if (!last_error().empty() && last_error().rfind("igpu_texture_read:", 0) == 0)
        {
            set_last_error("igpu_texture_get_pixel:" + last_error().substr(std::string("igpu_texture_read").size()));
        }
        return pixel;
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
        const char* const api = "igpu_draw_to_texture_level";
        if (!require_device(api))
        {
            return false;
        }
        auto* entry = find_texture(texture);
        if (entry == nullptr)
        {
            set_last_error(std::string(api) + ": unknown texture handle");
            return false;
        }
        if (!entry->target)
        {
            set_last_error(std::string(api) + ": the texture was not created as a render target");
            return false;
        }
        if (mip < 0 || mip >= entry->mips)
        {
            set_last_error(std::string(api) + ": mip level is outside the texture");
            return false;
        }
        const std::int32_t level_depth = (entry->kind == 1) ? std::max(1, entry->depth >> mip) : entry->depth;
        if (layer < 0 || layer >= level_depth)
        {
            set_last_error(std::string(api) + ": layer is outside that level");
            return false;
        }

        ID3D11RenderTargetView* view = entry->rtv;
        ID3D11RenderTargetView* owned = nullptr;
        if (view == nullptr || layer != 0 || mip != 0 || entry->kind != 0)
        {
            if (!make_slice_view(state().device, *entry, layer, mip, &owned))
            {
                set_last_error(std::string(api) + ": could not target that level");
                return false;
            }
            view = owned;
        }

        const std::int32_t level_width = std::max(1, entry->width >> mip);
        const std::int32_t level_height = std::max(1, entry->height >> mip);
        ID3D11RenderTargetView* views[1] = { view };
        TargetGuard guard(state().context, views, 1, level_width, level_height);
        const bool ok = draw(vertex_buffer, layout, primitive, first_vertex, vertex_count);
        guard.restore();
        if (owned != nullptr)
        {
            owned->Release();
        }
        return ok;
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
        const bool ok = draw_to_texture_level(
            vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, layer, 0);
        if (!last_error().empty() && last_error().rfind("igpu_draw_to_texture_level:", 0) == 0)
        {
            set_last_error("igpu_draw_to_texture_layer:" + last_error().substr(std::string("igpu_draw_to_texture_level").size()));
        }
        return ok;
    }

    bool draw_to_texture(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        std::uint64_t texture)
    {
        return draw_to_texture_layer(vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, 0);
    }

    bool dispatch(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_texture)
    {
        clear_last_error();
        if (!require_device("igpu_dispatch"))
        {
            return false;
        }
        if (groups_x <= 0 || groups_y <= 0 || groups_z <= 0 ||
            groups_x > 65535 || groups_y > 65535 || groups_z > 65535)
        {
            set_last_error("igpu_dispatch: group counts must be in 1..65535");
            return false;
        }
        auto* entry = find_texture(storage_texture);
        if (entry == nullptr || entry->uav == nullptr)
        {
            set_last_error("igpu_dispatch: the texture is not a storage texture");
            return false;
        }
        // IgpuShaderStage.Compute. The bound record is IGPU's own bookkeeping.
        const auto bound = state().bound_shaders.find(2);
        if (bound == state().bound_shaders.end() || bound->second == 0)
        {
            set_last_error("igpu_dispatch: bind a compute shader first");
            return false;
        }

        auto* context = state().context;
        ID3D11UnorderedAccessView* previous = nullptr;
        context->CSGetUnorderedAccessViews(0, 1, &previous);
        ID3D11UnorderedAccessView* view = entry->uav;
        context->CSSetUnorderedAccessViews(0, 1, &view, nullptr);
        context->Dispatch(static_cast<UINT>(groups_x), static_cast<UINT>(groups_y), static_cast<UINT>(groups_z));
        context->CSSetUnorderedAccessViews(0, 1, &previous, nullptr);
        if (previous != nullptr)
        {
            previous->Release();
        }
        return true;
    }

    bool dispatch_level(
        std::int32_t groups_x,
        std::int32_t groups_y,
        std::int32_t groups_z,
        std::uint64_t storage_texture,
        std::int32_t mip)
    {
        clear_last_error();
        if (!require_device("igpu_dispatch_level"))
        {
            return false;
        }
        if (groups_x <= 0 || groups_y <= 0 || groups_z <= 0 ||
            groups_x > 65535 || groups_y > 65535 || groups_z > 65535)
        {
            set_last_error("igpu_dispatch_level: group counts must be in 1..65535");
            return false;
        }
        auto* entry = find_texture(storage_texture);
        if (entry == nullptr || entry->uav == nullptr)
        {
            set_last_error("igpu_dispatch_level: the texture is not a storage texture");
            return false;
        }
        if (mip < 0 || mip >= entry->mips)
        {
            set_last_error("igpu_dispatch_level: mip level is outside the texture");
            return false;
        }
        const auto bound = state().bound_shaders.find(2);
        if (bound == state().bound_shaders.end() || bound->second == 0)
        {
            set_last_error("igpu_dispatch_level: bind a compute shader first");
            return false;
        }

        // The shader sees this level as the whole image. Level 0 reuses the
        // view made at creation. A coarser level gets a view of that level
        // only, released before return.
        ID3D11UnorderedAccessView* view = entry->uav;
        bool owned = false;
        if (mip != 0)
        {
            DXGI_FORMAT dxgi{};
            if (!format_to_dxgi(entry->format, dxgi, false))
            {
                set_last_error("igpu_dispatch_level: this format cannot be written");
                return false;
            }
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = dxgi;
            if (entry->kind == 1)
            {
                desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                desc.Texture3D.MipSlice = static_cast<UINT>(mip);
                desc.Texture3D.FirstWSlice = 0;
                desc.Texture3D.WSize = static_cast<UINT>(std::max(1, entry->depth >> mip));
            }
            else if (entry->kind == 2 || entry->kind == 3)
            {
                desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = static_cast<UINT>(mip);
                desc.Texture2DArray.FirstArraySlice = 0;
                desc.Texture2DArray.ArraySize = static_cast<UINT>(entry->depth);
            }
            else
            {
                desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = static_cast<UINT>(mip);
            }
            if (FAILED(state().device->CreateUnorderedAccessView(entry->texture, &desc, &view)) || view == nullptr)
            {
                set_last_error("igpu_dispatch_level: could not write that level");
                return false;
            }
            owned = true;
        }

        auto* context = state().context;
        ID3D11UnorderedAccessView* previous = nullptr;
        context->CSGetUnorderedAccessViews(0, 1, &previous);
        context->CSSetUnorderedAccessViews(0, 1, &view, nullptr);
        context->Dispatch(static_cast<UINT>(groups_x), static_cast<UINT>(groups_y), static_cast<UINT>(groups_z));
        context->CSSetUnorderedAccessViews(0, 1, &previous, nullptr);
        if (previous != nullptr)
        {
            previous->Release();
        }
        if (owned)
        {
            view->Release();
        }
        return true;
    }

    bool dispatch_buffer(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, std::uint64_t storage_buffer)
    {
        clear_last_error();
        if (!require_device("igpu_dispatch_buffer"))
        {
            return false;
        }
        if (groups_x <= 0 || groups_y <= 0 || groups_z <= 0 ||
            groups_x > 65535 || groups_y > 65535 || groups_z > 65535)
        {
            set_last_error("igpu_dispatch_buffer: group counts must be in 1..65535");
            return false;
        }
        auto* entry = find_buffer(storage_buffer);
        if (entry == nullptr || entry->unordered_view == nullptr)
        {
            set_last_error("igpu_dispatch_buffer: the buffer is not a writable storage buffer");
            return false;
        }
        const auto bound = state().bound_shaders.find(2);
        if (bound == state().bound_shaders.end() || bound->second == 0)
        {
            set_last_error("igpu_dispatch_buffer: bind a compute shader first");
            return false;
        }

        auto* context = state().context;
        ID3D11UnorderedAccessView* previous = nullptr;
        context->CSGetUnorderedAccessViews(0, 1, &previous);
        ID3D11UnorderedAccessView* view = entry->unordered_view;
        context->CSSetUnorderedAccessViews(0, 1, &view, nullptr);
        context->Dispatch(static_cast<UINT>(groups_x), static_cast<UINT>(groups_y), static_cast<UINT>(groups_z));
        context->CSSetUnorderedAccessViews(0, 1, &previous, nullptr);
        if (previous != nullptr)
        {
            previous->Release();
        }
        return true;
    }

    bool dispatch_both(
        std::int32_t groups_x,
        std::int32_t groups_y,
        std::int32_t groups_z,
        std::uint64_t storage_texture,
        std::uint64_t storage_buffer)
    {
        clear_last_error();
        if (!require_device("igpu_dispatch_both"))
        {
            return false;
        }
        if (groups_x <= 0 || groups_y <= 0 || groups_z <= 0 ||
            groups_x > 65535 || groups_y > 65535 || groups_z > 65535)
        {
            set_last_error("igpu_dispatch_both: group counts must be in 1..65535");
            return false;
        }
        auto* texture = find_texture(storage_texture);
        if (texture == nullptr || texture->uav == nullptr)
        {
            set_last_error("igpu_dispatch_both: the texture is not a storage texture");
            return false;
        }
        auto* buffer = find_buffer(storage_buffer);
        if (buffer == nullptr || buffer->unordered_view == nullptr)
        {
            set_last_error("igpu_dispatch_both: the buffer is not a writable storage buffer");
            return false;
        }
        const auto bound = state().bound_shaders.find(2);
        if (bound == state().bound_shaders.end() || bound->second == 0)
        {
            set_last_error("igpu_dispatch_both: bind a compute shader first");
            return false;
        }

        // Slot 0 is the texture and slot 1 is the buffer. One table, so a
        // backend with separate image and storage-buffer tables can use the
        // same numbers. Both slots are put back before return.
        auto* context = state().context;
        ID3D11UnorderedAccessView* previous[2] = {};
        context->CSGetUnorderedAccessViews(0, 2, previous);
        ID3D11UnorderedAccessView* views[2] = { texture->uav, buffer->unordered_view };
        context->CSSetUnorderedAccessViews(0, 2, views, nullptr);
        context->Dispatch(static_cast<UINT>(groups_x), static_cast<UINT>(groups_y), static_cast<UINT>(groups_z));
        context->CSSetUnorderedAccessViews(0, 2, previous, nullptr);
        for (ID3D11UnorderedAccessView* view : previous)
        {
            if (view != nullptr)
            {
                view->Release();
            }
        }
        return true;
    }

    bool dispatch_writes(
        std::int32_t groups_x,
        std::int32_t groups_y,
        std::int32_t groups_z,
        const gm::wire::GMArrayView& kinds,
        const gm::wire::GMArrayView& targets)
    {
        clear_last_error();
        if (!require_device("igpu_dispatch_writes"))
        {
            return false;
        }
        if (groups_x <= 0 || groups_y <= 0 || groups_z <= 0 ||
            groups_x > 65535 || groups_y > 65535 || groups_z > 65535)
        {
            set_last_error("igpu_dispatch_writes: group counts must be in 1..65535");
            return false;
        }
        if (kinds.size() != targets.size())
        {
            set_last_error("igpu_dispatch_writes: the two lists must be the same length");
            return false;
        }
        // Eight is the writable-slot count of a compute shader on this tier.
        // A longer list cannot be expressed on the shared slot table.
        const auto count = kinds.size();
        if (count < 1 || count > D3D11_PS_CS_UAV_REGISTER_COUNT)
        {
            set_last_error("igpu_dispatch_writes: pass 1 to 8 write targets");
            return false;
        }

        // IgpuWriteTarget.Texture = 0, Buffer = 1. Slot i is entry i.
        // Texture ids and buffer ids are separate, so a duplicate is per kind.
        ID3D11UnorderedAccessView* views[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
        std::uint64_t seen_texture[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
        std::uint64_t seen_buffer[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
        std::size_t texture_count = 0;
        std::size_t buffer_count = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            std::int64_t kind = 0;
            if (!read_i64(kinds[i], kind))
            {
                set_last_error("igpu_dispatch_writes: a kind entry is not a number");
                return false;
            }
            std::int64_t handle = 0;
            if (!read_i64(targets[i], handle) || handle <= 0)
            {
                set_last_error("igpu_dispatch_writes: a target entry is not a handle");
                return false;
            }
            const auto id = static_cast<std::uint64_t>(handle);
            if (kind == 0)
            {
                for (std::size_t earlier = 0; earlier < texture_count; ++earlier)
                {
                    if (seen_texture[earlier] == id)
                    {
                        set_last_error("igpu_dispatch_writes: the same storage texture is listed twice");
                        return false;
                    }
                }
                auto* entry = find_texture(id);
                if (entry == nullptr || entry->uav == nullptr)
                {
                    set_last_error("igpu_dispatch_writes: the texture is not a storage texture");
                    return false;
                }
                seen_texture[texture_count++] = id;
                views[i] = entry->uav;
            }
            else if (kind == 1)
            {
                for (std::size_t earlier = 0; earlier < buffer_count; ++earlier)
                {
                    if (seen_buffer[earlier] == id)
                    {
                        set_last_error("igpu_dispatch_writes: the same storage buffer is listed twice");
                        return false;
                    }
                }
                auto* entry = find_buffer(id);
                if (entry == nullptr || entry->unordered_view == nullptr)
                {
                    set_last_error("igpu_dispatch_writes: the buffer is not a writable storage buffer");
                    return false;
                }
                seen_buffer[buffer_count++] = id;
                views[i] = entry->unordered_view;
            }
            else
            {
                set_last_error("igpu_dispatch_writes: unknown write target kind");
                return false;
            }
        }

        const auto bound = state().bound_shaders.find(2);
        if (bound == state().bound_shaders.end() || bound->second == 0)
        {
            set_last_error("igpu_dispatch_writes: bind a compute shader first");
            return false;
        }

        auto* context = state().context;
        ID3D11UnorderedAccessView* previous[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
        context->CSGetUnorderedAccessViews(0, static_cast<UINT>(count), previous);
        context->CSSetUnorderedAccessViews(0, static_cast<UINT>(count), views, nullptr);
        context->Dispatch(static_cast<UINT>(groups_x), static_cast<UINT>(groups_y), static_cast<UINT>(groups_z));
        context->CSSetUnorderedAccessViews(0, static_cast<UINT>(count), previous, nullptr);
        for (std::size_t i = 0; i < count; ++i)
        {
            if (previous[i] != nullptr)
            {
                previous[i]->Release();
            }
        }
        return true;
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
        if (!require_device("igpu_draw_to_render_targets"))
        {
            return false;
        }

        const auto count = targets.size();
        if (count < 1 || count > 4)
        {
            set_last_error("igpu_draw_to_render_targets: pass 1 to 4 render-target textures");
            return false;
        }

        ID3D11RenderTargetView* views[4] = {};
        std::int64_t seen[4] = {};
        std::int32_t width = 0;
        std::int32_t height = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto value = targets[i];
            std::int64_t handle = 0;
            if (value.is<std::uint64_t>())
            {
                handle = static_cast<std::int64_t>(value.as<std::uint64_t>());
            }
            else if (value.is<std::int32_t>())
            {
                handle = value.as<std::int32_t>();
            }
            else if (value.is<double>())
            {
                handle = static_cast<std::int64_t>(value.as<double>());
            }
            else
            {
                set_last_error("igpu_draw_to_render_targets: a target entry is not a texture handle");
                return false;
            }
            if (handle <= 0)
            {
                set_last_error("igpu_draw_to_render_targets: a target entry is not a texture handle");
                return false;
            }
            for (std::size_t earlier = 0; earlier < i; ++earlier)
            {
                if (seen[earlier] == handle)
                {
                    set_last_error("igpu_draw_to_render_targets: the same texture is listed twice");
                    return false;
                }
            }
            seen[i] = handle;

            auto* entry = find_texture(static_cast<std::uint64_t>(handle));
            if (entry == nullptr)
            {
                set_last_error("igpu_draw_to_render_targets: unknown texture handle");
                return false;
            }
            if (entry->rtv == nullptr)
            {
                set_last_error("igpu_draw_to_render_targets: a texture was not created as a render target");
                return false;
            }
            if (i == 0)
            {
                width = entry->width;
                height = entry->height;
            }
            else if (entry->width != width || entry->height != height)
            {
                set_last_error("igpu_draw_to_render_targets: every target must be the same size");
                return false;
            }
            views[i] = entry->rtv;
        }

        TargetGuard guard(state().context, views, static_cast<UINT>(count), width, height);
        const bool ok = draw(vertex_buffer, layout, primitive, first_vertex, vertex_count);
        guard.restore();
        return ok;
    }

    // layers == nullptr means every target uses layer 0.
    bool draw_targets_chosen(
        std::uint64_t vertex_buffer,
        std::uint64_t layout,
        std::int32_t primitive,
        std::int64_t first_vertex,
        std::int64_t vertex_count,
        const gm::wire::GMArrayView& targets,
        const gm::wire::GMArrayView* layers,
        const gm::wire::GMArrayView& mips,
        const char* api,
        std::int64_t blend_state,
        std::int64_t depth_state,
        std::int64_t raster_state,
        std::int64_t sampler_state)
    {
        clear_last_error();
        if (!require_device(api))
        {
            return false;
        }
        if (targets.size() != mips.size() || (layers != nullptr && layers->size() != targets.size()))
        {
            set_last_error(std::string(api) + ": the lists must be the same length");
            return false;
        }
        const auto count = targets.size();
        if (count < 1 || count > 4)
        {
            set_last_error(std::string(api) + ": pass 1 to 4 render-target textures");
            return false;
        }

        ID3D11RenderTargetView* views[4] = {};
        bool owned[4] = {};
        auto release_owned = [&]()
        {
            for (int slot = 0; slot < 4; ++slot)
            {
                if (owned[slot] && views[slot] != nullptr)
                {
                    views[slot]->Release();
                    views[slot] = nullptr;
                }
            }
        };

        // The same texture may be listed twice when the layer or the level
        // differs. The same layer of the same level may not.
        std::int64_t seen_handle[4] = {};
        std::int32_t seen_layer[4] = {};
        std::int32_t seen_mip[4] = {};
        std::int32_t width = 0;
        std::int32_t height = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            std::int64_t handle = 0;
            if (!read_i64(targets[i], handle) || handle <= 0)
            {
                release_owned();
                set_last_error(std::string(api) + ": a target entry is not a texture handle");
                return false;
            }
            std::int64_t mip_value = 0;
            if (!read_i64(mips[i], mip_value))
            {
                release_owned();
                set_last_error(std::string(api) + ": a mip entry is not a number");
                return false;
            }
            std::int64_t layer_value = 0;
            if (layers != nullptr && !read_i64((*layers)[i], layer_value))
            {
                release_owned();
                set_last_error(std::string(api) + ": a layer entry is not a number");
                return false;
            }
            const auto listed_layer = static_cast<std::int32_t>(layer_value);
            const auto listed_mip = static_cast<std::int32_t>(mip_value);
            for (std::size_t earlier = 0; earlier < i; ++earlier)
            {
                if (seen_handle[earlier] == handle && seen_layer[earlier] == listed_layer && seen_mip[earlier] == listed_mip)
                {
                    release_owned();
                    set_last_error(std::string(api) + ": the same layer and level are listed twice");
                    return false;
                }
            }
            seen_handle[i] = handle;
            seen_layer[i] = listed_layer;
            seen_mip[i] = listed_mip;

            auto* entry = find_texture(static_cast<std::uint64_t>(handle));
            if (entry == nullptr)
            {
                release_owned();
                set_last_error(std::string(api) + ": unknown texture handle");
                return false;
            }
            if (!entry->target)
            {
                release_owned();
                set_last_error(std::string(api) + ": a texture was not created as a render target");
                return false;
            }
            if (mip_value < 0 || mip_value >= entry->mips)
            {
                release_owned();
                set_last_error(std::string(api) + ": mip level is outside the texture");
                return false;
            }
            const auto mip = static_cast<std::int32_t>(mip_value);
            const auto layer = static_cast<std::int32_t>(layer_value);
            const std::int32_t level_depth = (entry->kind == 1) ? std::max(1, entry->depth >> mip) : entry->depth;
            if (layer < 0 || layer >= level_depth)
            {
                release_owned();
                set_last_error(std::string(api) + ": layer is outside that level");
                return false;
            }
            const std::int32_t level_width = std::max(1, entry->width >> mip);
            const std::int32_t level_height = std::max(1, entry->height >> mip);
            if (i == 0)
            {
                width = level_width;
                height = level_height;
            }
            else if (level_width != width || level_height != height)
            {
                release_owned();
                set_last_error(std::string(api) + ": every chosen level must be the same size");
                return false;
            }

            if (mip == 0 && layer == 0 && entry->kind == 0 && entry->rtv != nullptr)
            {
                views[i] = entry->rtv;
            }
            else
            {
                if (!make_slice_view(state().device, *entry, layer, mip, &views[i]))
                {
                    release_owned();
                    set_last_error(std::string(api) + ": could not target that level");
                    return false;
                }
                owned[i] = true;
            }
        }

        TargetGuard guard(state().context, views, static_cast<UINT>(count), width, height);
        const bool ok = draw(vertex_buffer, layout, primitive, first_vertex, vertex_count,
            blend_state, depth_state, raster_state, sampler_state);
        guard.restore();
        release_owned();
        return ok;
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
        return draw_targets_chosen(
            vertex_buffer, layout, primitive, first_vertex, vertex_count,
            targets, nullptr, mips, "igpu_draw_to_render_targets_level", 0, 0, 0, 0);
    }

    bool draw_to_render_targets_layer(
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
        return draw_targets_chosen(
            vertex_buffer, layout, primitive, first_vertex, vertex_count,
            targets, &layers, mips, "igpu_draw_to_render_targets_layer",
            blend_state, depth_state, raster_state, sampler_state);
    }

    bool draw_sampled(
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
        clear_last_error();
        if (!require_device("igpu_draw_sampled"))
        {
            return false;
        }
        auto* entry = find_texture(texture);
        if (entry == nullptr || entry->srv == nullptr)
        {
            set_last_error("igpu_draw_sampled: unknown texture handle");
            return false;
        }

        auto* context = state().context;
        ID3D11ShaderResourceView* previous = nullptr;
        context->PSGetShaderResources(0, 1, &previous);
        ID3D11ShaderResourceView* bound = entry->srv;
        context->PSSetShaderResources(0, 1, &bound);

        const bool ok = draw(
            vertex_buffer, layout, primitive, first_vertex, vertex_count,
            blend_state, depth_state, raster_state, sampler_state);

        context->PSSetShaderResources(0, 1, &previous);
        if (previous != nullptr)
        {
            previous->Release();
        }
        return ok;
    }

    bool texture_format(std::int32_t format)
    {
        DXGI_FORMAT dxgi{};
        if (!format_to_dxgi(format, dxgi, false))
        {
            return false;
        }
        auto* device = state().device;
        if (device == nullptr)
        {
            return false;
        }
        UINT support = 0;
        if (FAILED(device->CheckFormatSupport(dxgi, &support)))
        {
            return false;
        }
        return (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
    }
}
}
