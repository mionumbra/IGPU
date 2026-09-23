// ##### extgen :: Auto-generated file do not edit!! #####

#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include "core/GMExtWire.h"

namespace gm_consts
{
}


namespace gm_enums
{
    enum class IgpuFeatureLevel : std::int64_t
    {
        Unknown = 0,
        Level_11_0 = 1,
        Level_11_1 = 2,
        Level_12_0 = 3,
        Level_12_1 = 4
    };

    enum class IgpuShaderStage : std::int64_t
    {
        Vertex = 0,
        Pixel = 1,
        Compute = 2,
        Geometry = 3,
        Hull = 4,
        Domain = 5,
        Mesh = 6,
        Amplification = 7
    };

    enum class IgpuCapability : std::int64_t
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
        Instancing = 40,
        IndirectDraw = 41,
        Queries = 42,
        Timestamps = 43,
        OcclusionQuery = 44,
        Fence = 45,
        Wireframe = 46,
        AdapterInfo = 60,
        VideoMemory = 61,
        BackbufferSize = 62
    };

}


namespace gm_structs
{

}

namespace gm::wire::codec
{
}

namespace gm::wire::details
{
}

bool igpu_init(const gm::wire::GMValue& device, const gm::wire::GMValue& context, const gm::wire::GMValue& swapchain);
void igpu_shutdown();
std::string igpu_version();
bool igpu_is_available();
std::int32_t igpu_get_feature_level();
std::string igpu_get_adapter_description();
std::int64_t igpu_get_video_memory();
std::int32_t igpu_get_backbuffer_width();
std::int32_t igpu_get_backbuffer_height();
gm::wire::DataStream igpu_get_capabilities();
bool igpu_supports(std::int32_t capability);
std::string igpu_get_shader_dialect();
std::int64_t igpu_shader_compile(std::string_view source, std::string_view entry, std::int32_t stage, std::string_view dialect);
std::int64_t igpu_shader_compile_vertex(std::string_view source, std::string_view entry, std::string_view dialect);
std::int64_t igpu_shader_compile_pixel(std::string_view source, std::string_view entry, std::string_view dialect);
std::int64_t igpu_shader_compile_compute(std::string_view source, std::string_view entry, std::string_view dialect);
bool igpu_shader_release(std::uint64_t shader);
std::string igpu_get_last_error();
