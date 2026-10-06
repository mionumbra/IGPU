#pragma once

#include <d3d11.h>

namespace igpu
{
    namespace d3d11_impl
    {
        // Direct3D views for one draw. Not part of the backend-neutral API.
        struct ResolvedDrawState
        {
            ID3D11BlendState* blend = nullptr;
            ID3D11DepthStencilState* depth = nullptr;
            UINT stencil_ref = 0;
            ID3D11RasterizerState* raster = nullptr;
            ID3D11SamplerState* sampler = nullptr;
            bool any = false;
        };

        bool resolve_draw_states(
            const char* entry,
            std::int64_t blend,
            std::int64_t depth,
            std::int64_t raster,
            std::int64_t sampler,
            ResolvedDrawState& out);

        class OutputStateGuard
        {
        public:
            OutputStateGuard(ID3D11DeviceContext* context, const ResolvedDrawState& states);
            ~OutputStateGuard();

            OutputStateGuard(const OutputStateGuard&) = delete;
            OutputStateGuard& operator=(const OutputStateGuard&) = delete;

            bool restore();

        private:
            ID3D11DeviceContext* context_ = nullptr;
            bool active_ = false;
            bool restored_ = false;

            bool touch_blend_ = false;
            bool touch_depth_ = false;
            bool touch_raster_ = false;
            bool touch_sampler_ = false;

            ID3D11BlendState* blend_ = nullptr;
            FLOAT blend_factor_[4] = {};
            UINT sample_mask_ = 0xffffffff;
            ID3D11DepthStencilState* depth_ = nullptr;
            UINT stencil_ref_ = 0;
            ID3D11RasterizerState* raster_ = nullptr;
            ID3D11SamplerState* sampler_ = nullptr;
        };
    }
}
