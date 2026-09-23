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
        Compute = 2
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
std::int64_t igpu_shader_compile_vertex(std::string_view source, std::string_view entry, std::string_view target);
std::int64_t igpu_shader_compile_pixel(std::string_view source, std::string_view entry, std::string_view target);
std::int64_t igpu_shader_compile_compute(std::string_view source, std::string_view entry, std::string_view target);
bool igpu_shader_release(std::uint64_t shader);
std::string igpu_get_last_error();
