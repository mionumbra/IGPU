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

    enum class IgpuBufferUsage : std::int64_t
    {
        Static = 0,
        Dynamic = 1,
        Staging = 2
    };

    enum class IgpuBufferBind : std::int64_t
    {
        None = 0,
        Vertex = 1,
        Index = 2,
        Uniform = 4,
        Storage = 8
    };

    enum class IgpuPrimitive : std::int64_t
    {
        PointList = 1,
        LineList = 2,
        LineStrip = 3,
        TriangleList = 4,
        TriangleStrip = 5,
        TriangleFan = 6
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
        InputLayout = 47,
        VertexBuffer = 48,
        IndexBuffer = 49,
        UniformBuffer = 50,
        BufferResize = 51,
        BufferReadback = 52,
        Draw = 53,
        DrawIndexed = 54,
        DrawStateRestore = 55,
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
std::int64_t igpu_input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage, const gm::wire::GMArrayView& type, std::int32_t element_count, std::int32_t stride);
bool igpu_input_layout_release(std::uint64_t layout);
std::int64_t igpu_buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride);
bool igpu_buffer_write(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer data);
bool igpu_buffer_resize(std::uint64_t buffer, std::int64_t size);
bool igpu_buffer_read(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer dest);
std::int64_t igpu_buffer_size(std::uint64_t buffer);
bool igpu_buffer_release(std::uint64_t buffer);
bool igpu_draw(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive, std::int64_t first_vertex, std::int64_t vertex_count);
bool igpu_draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer, std::int32_t primitive, std::int64_t first_index, std::int64_t index_count);
std::int32_t igpu_get_draw_count();
std::int32_t igpu_get_draw_restore_failures();
bool igpu_is_vertex_buffer_bound(std::uint64_t buffer);
