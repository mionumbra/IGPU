#include "igpu_device.h"

#include <dxgi1_2.h>

#include <Windows.h>

#include "igpu_error.h"

namespace igpu
{
    void DeviceState::reset()
    {
        for (auto& [id, entry] : shaders)
        {
            if (entry.object != nullptr)
            {
                entry.object->Release();
            }
            if (entry.bytecode != nullptr)
            {
                entry.bytecode->Release();
            }
        }
        shaders.clear();
        next_shader_id = 1;

        // The bound-shader records point at handles that no longer exist once
        // the map above is cleared, so they must go with it. Leaving them would
        // make igpu_get_bound_shader() report a stale handle after a
        // shutdown/re-init cycle.
        bound_shaders.clear();

        for (auto& [id, layout] : input_layouts)
        {
            if (layout != nullptr)
            {
                layout->Release();
            }
        }
        input_layouts.clear();
        next_input_layout_id = 1;

        for (auto& [id, entry] : buffers)
        {
            if (entry.object != nullptr)
            {
                entry.object->Release();
            }
        }
        buffers.clear();
        next_buffer_id = 1;

        // device, context and swapchain are borrowed from GameMaker: never release.
        device = nullptr;
        context = nullptr;
        swapchain = nullptr;

        adapter_desc = {};
        adapter_desc_valid = false;
        feature_level = D3D_FEATURE_LEVEL_11_0;
        backbuffer_width = 0;
        backbuffer_height = 0;
        initialised = false;
    }

    DeviceState& state()
    {
        static DeviceState instance;
        return instance;
    }

    DeviceState::ShaderEntry* find_shader(std::uint64_t handle)
    {
        auto& shaders = state().shaders;
        const auto it = shaders.find(handle);
        return it == shaders.end() ? nullptr : &it->second;
    }

    DeviceState::BufferEntry* find_buffer(std::uint64_t handle)
    {
        auto& buffers = state().buffers;
        const auto it = buffers.find(handle);
        return it == buffers.end() ? nullptr : &it->second;
    }

    bool bind_device(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain)
    {
        auto& s = state();
        s.reset();

        if (device == nullptr || context == nullptr)
        {
            set_last_error("igpu_init: device and context pointers must be non-null");
            return false;
        }

        s.device = device;
        s.context = context;
        s.swapchain = swapchain;

        s.feature_level = s.device->GetFeatureLevel();
        s.initialised = true;

        refresh_adapter_info();
        refresh_backbuffer_size();

        clear_last_error();
        return true;
    }

    void release_all()
    {
        state().reset();
    }

    void refresh_adapter_info()
    {
        auto& s = state();
        s.adapter_desc_valid = false;
        if (s.device == nullptr)
        {
            return;
        }

        IDXGIDevice* dxgi_device = nullptr;
        if (FAILED(s.device->QueryInterface(__uuidof(IDXGIDevice),
                                            reinterpret_cast<void**>(&dxgi_device))) ||
            dxgi_device == nullptr)
        {
            return;
        }

        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(dxgi_device->GetAdapter(&adapter)) && adapter != nullptr)
        {
            if (SUCCEEDED(adapter->GetDesc(&s.adapter_desc)))
            {
                s.adapter_desc_valid = true;
            }
            adapter->Release();
        }

        dxgi_device->Release();
    }

    void refresh_backbuffer_size()
    {
        auto& s = state();
        s.backbuffer_width = 0;
        s.backbuffer_height = 0;

        if (s.swapchain == nullptr)
        {
            return;
        }

        // Prefer GetDesc1: DXGI_SWAP_CHAIN_DESC1 carries the *current* buffer
        // size, whereas the older GetDesc() reports the size the swapchain was
        // created with - which goes stale as soon as the window is resized or
        // the game switches to fullscreen.
        IDXGISwapChain1* swapchain1 = nullptr;
        if (SUCCEEDED(s.swapchain->QueryInterface(
                __uuidof(IDXGISwapChain1),
                reinterpret_cast<void**>(&swapchain1))) &&
            swapchain1 != nullptr)
        {
            DXGI_SWAP_CHAIN_DESC1 desc1{};
            if (SUCCEEDED(swapchain1->GetDesc1(&desc1)))
            {
                s.backbuffer_width = static_cast<std::int32_t>(desc1.Width);
                s.backbuffer_height = static_cast<std::int32_t>(desc1.Height);
            }
            swapchain1->Release();

            if (s.backbuffer_width > 0 && s.backbuffer_height > 0)
            {
                return;
            }
        }

        // Fallback for a swapchain that is not IDXGISwapChain1 (DXGI 1.0).
        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(s.swapchain->GetDesc(&desc)))
        {
            return;
        }

        s.backbuffer_width = static_cast<std::int32_t>(desc.BufferDesc.Width);
        s.backbuffer_height = static_cast<std::int32_t>(desc.BufferDesc.Height);
    }
}
