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

        // A compiled shader keeps BOTH the live device object and the bytecode
        // blob it was created from. The blob is not redundant: creating an
        // input layout requires the vertex shader's signature, which only
        // exists inside the compiled bytecode. Dropping it would make
        // ID3D11InputLayout impossible to build.
        struct ShaderEntry
        {
            ID3D11DeviceChild* object = nullptr;
            ID3DBlob* bytecode = nullptr;

            // The IgpuShaderStage this was compiled for, recorded so
            // igpu_shader_bind can reject a stage mismatch instead of binding a
            // pixel shader to the vertex stage and letting the backend fail
            // later with a far more opaque message.
            std::int32_t stage = -1;
        };

        std::unordered_map<std::uint64_t, ShaderEntry> shaders;
        std::uint64_t next_shader_id = 1;

        // Which IGPU handle is bound per stage, mirrored from IgpuShaderStage.
        // This is IGPU's own bookkeeping, not a device read-back: the backend
        // can change the real binding behind IGPU's back (it re-binds its own
        // shader on every one of its draws), so this records intent, and
        // igpu_get_bound_shader() documents that distinction rather than
        // pretending to be authoritative.
        std::unordered_map<std::int32_t, std::uint64_t> bound_shaders;

        // Input layouts are keyed by handle like every other resource, and also
        // indexed by the vertex shader they were built against so the same
        // layout can be reused without the caller re-passing it.
        std::unordered_map<std::uint64_t, ID3D11InputLayout*> input_layouts;
        std::uint64_t next_input_layout_id = 1;

        // GPU buffers. The usage and bind flags are recorded alongside the
        // object because later calls must reject operations the buffer was not
        // created for - e.g. resizing a Static buffer, or reading back from one
        // that was not created as Staging.
        struct BufferEntry
        {
            ID3D11Buffer* object = nullptr;
            std::int64_t size = 0;
            std::int32_t usage = 0;
            std::int32_t bind = 0;

            // Bytes per vertex, for a buffer created with IgpuBufferBind.Vertex.
            // Stored because a draw has to derive how many vertices the buffer
            // holds, and only its creator knows the stride. 0 when not vertex.
            std::int32_t stride = 0;
        };

        std::unordered_map<std::uint64_t, BufferEntry> buffers;
        std::uint64_t next_buffer_id = 1;

        DXGI_ADAPTER_DESC adapter_desc{};
        bool adapter_desc_valid = false;

        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

        std::int32_t backbuffer_width = 0;
        std::int32_t backbuffer_height = 0;

        void reset();
    };

    DeviceState& state();

    // Shader lookup helper. Returns nullptr for an unknown or already-released
    // handle, so callers can validate without reaching into the map.
    DeviceState::ShaderEntry* find_shader(std::uint64_t handle);

    // Buffer lookup helper, same contract as find_shader.
    DeviceState::BufferEntry* find_buffer(std::uint64_t handle);

    bool bind_device(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain);
    void release_all();

    void refresh_backbuffer_size();
    void refresh_adapter_info();
}
