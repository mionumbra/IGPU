#include "igpu_device.h"

#include <dxgi.h>

#include <Windows.h>

#include <cstdio>

#include "igpu_error.h"

namespace
{
    void igpu_debug_trace(const char* text)
    {
        ::OutputDebugStringA(text);
        ::OutputDebugStringA("\n");

        std::FILE* file = nullptr;
        if (fopen_s(&file, "igpu_native_trace.txt", "a") == 0 && file != nullptr)
        {
            std::fputs(text, file);
            std::fputc('\n', file);
            std::fclose(file);
        }
    }
}

namespace igpu
{
    void DeviceState::reset()
    {
        for (auto& [id, shader] : shaders)
        {
            if (shader != nullptr)
            {
                shader->Release();
            }
        }
        shaders.clear();
        next_shader_id = 1;

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

    bool bind_device(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain)
    {
        char trace[256] = {};
        std::snprintf(trace, sizeof(trace),
            "bind_device: device=%p context=%p swapchain=%p",
            static_cast<void*>(device),
            static_cast<void*>(context),
            static_cast<void*>(swapchain));
        igpu_debug_trace(trace);

        auto& s = state();
        s.reset();

        if (device == nullptr || context == nullptr)
        {
            set_last_error("igpu_init: device and context pointers must be non-null");
            igpu_debug_trace("bind_device: FAILED (null device or context)");
            return false;
        }

        s.device = device;
        s.context = context;
        s.swapchain = swapchain;

        s.feature_level = s.device->GetFeatureLevel();
        s.initialised = true;

        refresh_adapter_info();
        refresh_backbuffer_size();

        {
            char success[256] = {};
            std::snprintf(success, sizeof(success),
                "bind_device: OK featureLevel=0x%X backbuffer=%dx%d",
                static_cast<unsigned>(s.feature_level),
                s.backbuffer_width,
                s.backbuffer_height);
            igpu_debug_trace(success);
        }

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

        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(s.swapchain->GetDesc(&desc)))
        {
            return;
        }

        s.backbuffer_width = static_cast<std::int32_t>(desc.BufferDesc.Width);
        s.backbuffer_height = static_cast<std::int32_t>(desc.BufferDesc.Height);
    }
}
