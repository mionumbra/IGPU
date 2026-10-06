#pragma once

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "igpu_reflect.h"

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

            // Uniform blocks read back when the shader was compiled. Empty when
            // the shader declares none. Queries never re-read the bytecode.
            UniformLayout uniforms;
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

            // Bytes per vertex, or bytes per structure for a storage buffer.
            // 0 when the buffer has neither of those uses.
            std::int32_t stride = 0;

            // Shader view of a structured storage buffer. Null for other binds.
            ID3D11ShaderResourceView* shader_view = nullptr;
            // Writable view used by a compute shader. Null for other binds.
            ID3D11UnorderedAccessView* unordered_view = nullptr;

            // CPU copy of a uniform buffer. Member writes patch this and then
            // upload the whole block; the GPU buffer starts zeroed to match.
            std::vector<std::byte> shadow;
        };

        std::unordered_map<std::uint64_t, BufferEntry> buffers;
        std::uint64_t next_buffer_id = 1;

        // Immutable pipeline states. `object` is a blend, depth-stencil,
        // rasteriser or sampler state. `stencil_ref` is only meaningful for
        // a depth state; the depth-stencil object itself cannot store it.
        enum class StateKind : std::int32_t
        {
            Blend = 1,
            Depth = 2,
            Raster = 3,
            Sampler = 4
        };

        struct StateEntry
        {
            StateKind kind = StateKind::Blend;
            ID3D11DeviceChild* object = nullptr;
            std::uint32_t stencil_ref = 0;
        };

        std::unordered_map<std::uint64_t, StateEntry> states;
        std::uint64_t next_state_id = 1;

        // texture is a 2D resource or a 3D resource. rtv covers the whole
        // resource only for a plain 2D render target; layered draws make a
        // one-slice view on the spot. uav is set only for a storage texture.
        struct TextureEntry
        {
            ID3D11Resource* texture = nullptr;
            ID3D11ShaderResourceView* srv = nullptr;
            ID3D11RenderTargetView* rtv = nullptr;
            ID3D11UnorderedAccessView* uav = nullptr;
            std::int32_t kind = 0;
            std::int32_t width = 0;
            std::int32_t height = 0;
            std::int32_t depth = 1;
            std::int32_t format = 0;
            bool target = false;
            std::int32_t mips = 1;
        };

        std::unordered_map<std::uint64_t, TextureEntry> textures;
        std::uint64_t next_texture_id = 1;

        DXGI_ADAPTER_DESC adapter_desc{};
        bool adapter_desc_valid = false;

        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

        std::int32_t backbuffer_width = 0;
        std::int32_t backbuffer_height = 0;

        // Set when GetDeviceRemovedReason reports the borrowed device is dead.
        // Cleared by a later successful igpu_init, and by a clean shutdown.
        // Stays set after the pointers are dropped, so a caller can tell loss
        // apart from "never initialised".
        bool device_lost = false;

        // Phrase recorded with device_lost, so a later call can repeat the
        // reason after the original error string has been cleared.
        std::string device_lost_detail;

        void reset();
    };

    // If the borrowed device has been removed or reset, drop every handle that
    // was created on it and record the error. Does nothing when no device is
    // bound. Safe to call often; a healthy device is left untouched, including
    // the last-error string.
    void refresh_device_status();

    bool device_was_removed();

    // False when there is no live device. Sets last error, naming `entry`.
    bool require_device(const char* entry);

    DeviceState& state();

    // Shader lookup helper. Returns nullptr for an unknown or already-released
    // handle, so callers can validate without reaching into the map.
    DeviceState::ShaderEntry* find_shader(std::uint64_t handle);

    // Buffer lookup helper, same contract as find_shader.
    DeviceState::BufferEntry* find_buffer(std::uint64_t handle);

    DeviceState::StateEntry* find_state(std::uint64_t handle);
    DeviceState::TextureEntry* find_texture(std::uint64_t handle);

    bool bind_device(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain);
    void release_all();

    void refresh_backbuffer_size();
    void refresh_adapter_info();
}
