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
    enum class IgpuVertexStep : std::int64_t
    {
        Vertex = 0,
        Instance = 1
    };

    enum class IgpuUniformType : std::int64_t
    {
        Float = 0,
        Int = 1,
        Uint = 2,
        Bool = 3,
        Struct = 4,
        Unknown = 5
    };

    enum class IgpuAddressMode : std::int64_t
    {
        Clamp = 0,
        Repeat = 1,
        Mirror = 2,
        Border = 3
    };

    enum class IgpuWriteTarget : std::int64_t
    {
        Texture = 0,
        Buffer = 1
    };

    enum class IgpuQueryKind : std::int64_t
    {
        Occlusion = 0,
        Timestamp = 1
    };

    enum class IgpuTextureKind : std::int64_t
    {
        TwoD = 0,
        ThreeD = 1,
        Array = 2,
        Cube = 3
    };

    enum class IgpuFill : std::int64_t
    {
        Solid = 0,
        Wireframe = 1
    };

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
        Storage = 8,
        Indirect = 16
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

}


namespace gm_structs
{
    struct IgpuUniformMember;
    struct IgpuUniformBlock;

    struct IgpuUniformMember
    {
        std::string name;
        std::int32_t offset;
        std::int32_t size;
        std::int32_t type;
        std::int32_t rows;
        std::int32_t columns;
        std::int32_t elements;
    };

    struct IgpuUniformBlock
    {
        std::string name;
        std::int32_t size;
        std::int32_t slot;
        std::vector<gm_structs::IgpuUniformMember> members;
    };

}

namespace gm::wire::codec
{
    template<>
    inline void writeValue<gm_structs::IgpuUniformMember>(gm::byteio::IByteWriter& _buf, const gm_structs::IgpuUniformMember& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.name);
        gm::wire::codec::writeValue(_buf, obj.offset);
        gm::wire::codec::writeValue(_buf, obj.size);
        gm::wire::codec::writeValue(_buf, obj.type);
        gm::wire::codec::writeValue(_buf, obj.rows);
        gm::wire::codec::writeValue(_buf, obj.columns);
        gm::wire::codec::writeValue(_buf, obj.elements);
    }

    template<>
    inline gm_structs::IgpuUniformMember readValue<gm_structs::IgpuUniformMember>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::IgpuUniformMember obj;
        obj.name = gm::wire::codec::readValue<std::string>(_buf);
        obj.offset = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.size = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.type = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.rows = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.columns = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.elements = gm::wire::codec::readValue<std::int32_t>(_buf);
        return obj;
    }

    template<>
    inline void writeValue<gm_structs::IgpuUniformBlock>(gm::byteio::IByteWriter& _buf, const gm_structs::IgpuUniformBlock& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.name);
        gm::wire::codec::writeValue(_buf, obj.size);
        gm::wire::codec::writeValue(_buf, obj.slot);
        gm::wire::codec::writeValue(_buf, obj.members);
    }

    template<>
    inline gm_structs::IgpuUniformBlock readValue<gm_structs::IgpuUniformBlock>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::IgpuUniformBlock obj;
        obj.name = gm::wire::codec::readValue<std::string>(_buf);
        obj.size = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.slot = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.members = gm::wire::codec::readVector<gm_structs::IgpuUniformMember>(_buf);
        return obj;
    }

}

namespace gm::wire::details
{
    template<>
    struct gm_struct_traits<gm_structs::IgpuUniformMember>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 0;
    };

    template<>
    struct gm_struct_traits<gm_structs::IgpuUniformBlock>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 1;
    };

}

bool igpu_init(const gm::wire::GMValue& device, const gm::wire::GMValue& context, const gm::wire::GMValue& swapchain);
bool igpu_bind_current();
void igpu_shutdown();
std::string igpu_version();
bool igpu_is_available();
bool igpu_device_lost();
std::int32_t igpu_get_feature_level();
std::string igpu_get_adapter_description();
std::int64_t igpu_get_video_memory();
std::int32_t igpu_get_backbuffer_width();
std::int32_t igpu_get_backbuffer_height();
gm::wire::DataStream igpu_get_capabilities();
bool igpu_supports(std::int32_t capability);
std::string igpu_get_shader_dialect();
bool igpu_set_graphics_info(std::string_view vendor, std::string_view version, std::string_view renderer, std::string_view shading_language, std::int32_t max_texture_size);
std::int64_t igpu_shader_compile(std::string_view source, std::string_view entry, std::int32_t stage, std::string_view dialect);
bool igpu_shader_release(std::uint64_t shader);
bool igpu_shader_bind(std::int64_t shader, std::int32_t stage);
std::int64_t igpu_get_bound_shader(std::int32_t stage);
std::string igpu_get_last_error();
std::int64_t igpu_input_layout_create(std::int64_t shader, const gm::wire::GMArrayView& usage, const gm::wire::GMArrayView& type, const gm::wire::GMArrayView& step, std::int32_t element_count, std::int32_t vertex_stride, std::int32_t instance_stride);
bool igpu_input_layout_release(std::uint64_t layout);
std::int64_t igpu_buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind, std::int32_t stride);
bool igpu_buffer_write(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer data);
bool igpu_buffer_resize(std::uint64_t buffer, std::int64_t size);
bool igpu_buffer_read(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer dest);
std::int64_t igpu_buffer_size(std::uint64_t buffer);
bool igpu_buffer_release(std::uint64_t buffer);
bool igpu_storage_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot);
std::vector<gm_structs::IgpuUniformBlock> igpu_shader_reflect(std::int64_t shader);
bool igpu_uniform_write(std::uint64_t buffer, std::int64_t shader, std::string_view block, std::string_view member, const gm::wire::GMArrayView& values);
bool igpu_uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot);
bool igpu_draw(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout, std::int32_t primitive, std::int64_t first_vertex, std::int64_t vertex_count, std::int64_t instance_count, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state);
bool igpu_draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer, std::int32_t primitive, std::int64_t first_index, std::int64_t index_count, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state);
bool igpu_draw_indirect(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout, std::int32_t primitive, std::uint64_t args, std::int64_t args_offset, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state);
bool igpu_draw_patch(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t control_points, std::int64_t first_vertex, std::int64_t vertex_count, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state);
bool igpu_draw_indexed_indirect(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout, std::uint64_t index_buffer, std::int32_t primitive, std::uint64_t args, std::int64_t args_offset, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state);
std::int64_t igpu_blend_state_create(bool enabled, std::int32_t src, std::int32_t dest, std::int32_t equation, std::int32_t src_alpha, std::int32_t dest_alpha, std::int32_t equation_alpha, bool write_red, bool write_green, bool write_blue, bool write_alpha);
std::int64_t igpu_depth_state_create(bool depth_test, bool depth_write, std::int32_t depth_func, bool stencil_enable, std::int32_t stencil_func, std::int32_t stencil_fail, std::int32_t stencil_depth_fail, std::int32_t stencil_pass, std::int32_t stencil_ref, std::int32_t stencil_read_mask, std::int32_t stencil_write_mask);
std::int64_t igpu_raster_state_create(std::int32_t cull, std::int32_t fill, bool scissor, bool depth_clip);
std::int64_t igpu_sampler_state_create(std::int32_t magnification, std::int32_t minification, std::int32_t mip, std::int32_t address_u, std::int32_t address_v, std::int32_t address_w, std::int32_t anisotropy, std::int32_t border, std::int32_t compare, float level_offset, float finest, float coarsest);
bool igpu_state_release(std::uint64_t state);
std::int64_t igpu_texture_create(std::int32_t kind, std::int32_t width, std::int32_t height, std::int32_t depth, std::int32_t format, bool render_target, bool storage, std::int32_t mip_count);
bool igpu_texture_generate_mips(std::uint64_t texture);
std::int64_t igpu_texture_read(std::uint64_t texture, std::int32_t x, std::int32_t y, std::int32_t layer, std::int32_t mip);
bool igpu_texture_release(std::uint64_t texture);
bool igpu_dispatch(std::int32_t groups_x, std::int32_t groups_y, std::int32_t groups_z, const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers, const gm::wire::GMArrayView& mips);
std::int64_t igpu_query_create(std::int32_t kind);
bool igpu_query_begin(std::uint64_t query);
bool igpu_query_end(std::uint64_t query);
bool igpu_query_ready(std::uint64_t query);
std::int64_t igpu_query_result(std::uint64_t query);
bool igpu_query_release(std::uint64_t query);
std::int64_t igpu_timestamp_frequency();
std::int64_t igpu_fence_create();
bool igpu_fence_signal(std::uint64_t fence);
bool igpu_fence_signaled(std::uint64_t fence);
bool igpu_fence_release(std::uint64_t fence);
bool igpu_draw_to_render_targets(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive, std::int64_t first_vertex, std::int64_t vertex_count, const gm::wire::GMArrayView& targets, const gm::wire::GMArrayView& layers, const gm::wire::GMArrayView& mips, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state);
bool igpu_draw_sampled(std::uint64_t vertex_buffer, std::uint64_t layout, std::int32_t primitive, std::int64_t first_vertex, std::int64_t vertex_count, std::uint64_t texture, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state);
