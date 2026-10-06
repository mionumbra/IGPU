#include "igpu_device.h"

#include <dxgi1_2.h>

#include <Windows.h>

#include <cstdio>
#include <string>

#include "igpu_backend.h"
#include "igpu_error.h"
#include "d3d11/igpu_d3d11.h"

namespace igpu
{
    void DeviceState::reset()
    {
        // Queries hold device objects, so the backend goes before the device.
        set_active_backend(nullptr);
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
            if (entry.shader_view != nullptr)
            {
                entry.shader_view->Release();
            }
            if (entry.unordered_view != nullptr)
            {
                entry.unordered_view->Release();
            }
            if (entry.object != nullptr)
            {
                entry.object->Release();
            }
        }
        buffers.clear();
        next_buffer_id = 1;

        for (auto& [id, entry] : states)
        {
            if (entry.object != nullptr)
            {
                entry.object->Release();
            }
        }
        states.clear();
        next_state_id = 1;

        for (auto& [id, entry] : textures)
        {
            if (entry.rtv != nullptr)
            {
                entry.rtv->Release();
            }
            if (entry.srv != nullptr)
            {
                entry.srv->Release();
            }
            if (entry.uav != nullptr)
            {
                entry.uav->Release();
            }
            if (entry.texture != nullptr)
            {
                entry.texture->Release();
            }
        }
        textures.clear();
        next_texture_id = 1;

        // GameMaker owns the original reference. IGPU holds one extra reference
        // taken in bind_device(), and this releases only that extra reference.
        // The extra reference is what keeps the object alive after GameMaker
        // drops its own during device-loss recovery, long enough to ask why.
        if (swapchain != nullptr)
        {
            swapchain->Release();
        }
        if (context != nullptr)
        {
            context->Release();
        }
        if (device != nullptr)
        {
            device->Release();
        }
        device = nullptr;
        context = nullptr;
        swapchain = nullptr;

        adapter_desc = {};
        adapter_desc_valid = false;
        feature_level = D3D_FEATURE_LEVEL_11_0;
        backbuffer_width = 0;
        backbuffer_height = 0;
        initialised = false;
        device_lost = false;
        device_lost_detail.clear();
        renderer_name.clear();
        gl_dialect.clear();
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

    DeviceState::StateEntry* find_state(std::uint64_t handle)
    {
        auto& states = state().states;
        const auto it = states.find(handle);
        return it == states.end() ? nullptr : &it->second;
    }

    DeviceState::TextureEntry* find_texture(std::uint64_t handle)
    {
        auto& textures = state().textures;
        const auto it = textures.find(handle);
        return it == textures.end() ? nullptr : &it->second;
    }

    namespace
    {
        std::string removal_detail(HRESULT hr)
        {
            const char* why = "unusable";
            if (hr == DXGI_ERROR_DEVICE_REMOVED)
            {
                why = "removed";
            }
            else if (hr == DXGI_ERROR_DEVICE_RESET)
            {
                why = "reset";
            }
            else if (hr == DXGI_ERROR_DEVICE_HUNG)
            {
                why = "hung";
            }
            else if (hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
            {
                why = "broken by a driver fault";
            }

            char hex[16] = {};
            std::snprintf(
                hex,
                sizeof(hex),
                "0x%08lX",
                static_cast<unsigned long>(static_cast<unsigned>(hr)));
            return std::string(why) + " (" + hex + ")";
        }
    }

    void refresh_device_status()
    {
        auto& s = state();
        if (!s.initialised || s.device == nullptr)
        {
            return;
        }

        const HRESULT hr = s.device->GetDeviceRemovedReason();
        if (hr == S_OK)
        {
            return;
        }

        // Drop handles created on this device before releasing our reference.
        // Their Release() calls are still valid; using them is not.
        const std::string detail = removal_detail(hr);
        s.reset();
        s.device_lost = true;
        s.device_lost_detail = detail;
        set_last_error(
            "the graphics device was " + detail + "; call igpu_init() again");
    }

    bool device_was_removed()
    {
        refresh_device_status();
        return state().device_lost;
    }

    bool require_device(const char* entry)
    {
        auto& s = state();
        if (s.initialised && s.device != nullptr)
        {
            refresh_device_status();
        }

        if (s.device_lost)
        {
            const std::string detail = s.device_lost_detail.empty()
                ? std::string("removed")
                : s.device_lost_detail;
            set_last_error(
                std::string(entry) + ": the graphics device was " + detail +
                "; call igpu_init() again");
            return false;
        }

        if (!s.initialised || active_backend() == nullptr)
        {
            set_last_error(std::string(entry) + ": call igpu_init() first");
            return false;
        }

        return true;
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

        // One extra reference each. reset() releases exactly these.
        s.device->AddRef();
        s.context->AddRef();
        if (s.swapchain != nullptr)
        {
            s.swapchain->AddRef();
        }

        s.feature_level = s.device->GetFeatureLevel();
        s.initialised = true;
        // Concrete backends are chosen only here.
        set_active_backend(make_d3d11_backend(s.device, s.context));

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
