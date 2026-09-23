#pragma once

#include <d3d11.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace igpu
{
    struct DeviceState
    {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        IDXGISwapChain* swapchain = nullptr;

        bool initialised = false;

        std::unordered_map<std::uint64_t, ID3D11DeviceChild*> shaders;
        std::uint64_t next_shader_id = 1;

        DXGI_ADAPTER_DESC adapter_desc{};
        bool adapter_desc_valid = false;

        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

        std::int32_t backbuffer_width = 0;
        std::int32_t backbuffer_height = 0;

        void reset();
    };

    DeviceState& state();

    bool bind_device(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain);
    void release_all();

    void refresh_backbuffer_size();
    void refresh_adapter_info();
}
