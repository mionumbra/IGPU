#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <string>
#include <vector>

#include "core/GMExtWire.h"

namespace igpu
{
    // A DataStream that can be seeded with bytes which are already in wire
    // format.
    //
    // Needed because DataStream's buffer is protected and its operator<<
    // prepends a kind byte to every value, so there is no public way to inject
    // an already-encoded payload. The capability struct is built by
    // StructStream, whose header (Struct tag + entry count) is emitted as RAW
    // bytes and must not be re-tagged.
    //
    // The bytes go into the DataStream base's own buffer - reachable from a
    // derived class - so they survive being returned by value as the base
    // type, which would slice any extra member state.
    class SeedableDataStream final : public gm::wire::DataStream
    {
    public:
        SeedableDataStream() = default;

        void append(const std::byte* data, std::size_t size)
        {
            auto& target = getBuffer();
            target.insert(target.end(), data, data + size);
        }
    };

    // Backend-neutral capability flags. Values mirror the IgpuCapability enum
    // in spec.gmidl and must stay in sync with it.
    enum class Capability : std::int32_t
    {
        None = 0,

        ShaderCompileRuntime = 1,
        ShaderStageVertex = 2,
        ShaderStagePixel = 3,
        ShaderStageCompute = 4,
        ShaderStageGeometry = 5,
        ShaderStageTessellation = 6,
        ShaderStageMesh = 7,

        Texture3D = 20,
        TextureArray = 21,
        TextureCubemap = 22,
        StructuredBuffer = 23,
        UnorderedAccess = 24,
        MultipleRenderTargets = 25,
        Texture2D = 26,

        Instancing = 40,
        IndirectDraw = 41,
        Queries = 42,
        Timestamps = 43,
        OcclusionQuery = 44,
        Fence = 45,
        Wireframe = 46,

        InputLayout = 47,
        VertexBuffer = 48,
        IndexBuffer = 49,
        UniformBuffer = 50,
        BufferResize = 51,
        BufferReadback = 52,

        Draw = 53,
        DrawIndexed = 54,
        DrawStateRestore = 55,

        BlendState = 56,
        DepthState = 57,
        RasterState = 58,
        SamplerState = 59,

        AdapterInfo = 60,
        VideoMemory = 61,
        BackbufferSize = 62,

        UniformReflection = 63
    };

    // Backend identifiers reported to GML. Deliberately stringly-typed so a new
    // backend can be added without touching the wire format.
    const char* backend_name();
    const char* shader_dialect();

    // Records os_get_info() graphics strings for platforms with no device.
    // Empty version clears the note. A bound device still wins in the
    // capability struct.
    bool set_graphics_info(std::string_view vendor, std::string_view version, std::string_view renderer,
                           std::string_view shading_language, std::int32_t max_texture_size);
    const char* probed_backend();
    const char* probed_dialect();
    std::string probed_device_name();
    bool probed_format(std::int32_t format);

    bool supports(Capability capability);

    // Builds the struct handed back to GML by igpu_get_capabilities(). Every
    // field is always present, so callers can read any key unconditionally.
    //
    // Returns DataStream because that is the type extgen generated for the
    // `gmval` return; the struct tag and entry count are written as raw bytes.
    gm::wire::DataStream build_capabilities();
}

