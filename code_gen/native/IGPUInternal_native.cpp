// ##### extgen :: Auto-generated file do not edit!! #####

#include "IGPUInternal_native.h"
#include "IGPUInternal_exports.h"

using namespace gm_structs;
using namespace gm::wire::codec;

static std::queue<gm::wire::GMBuffer> __buffer_queue;

// Internal function used for queueing buffers to native code
GMEXPORT double __EXT_NATIVE__IGPU_queue_buffer(char* __arg_buffer, double __arg_buffer_length)
{
    gm::wire::GMBuffer __buff{__arg_buffer, static_cast<uint64_t>(__arg_buffer_length)};
    __buffer_queue.push(__buff);

    return 1.0;
}

GMEXPORT double __EXT_NATIVE__igpu_init(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: Any
    gm::wire::GMValue device = gm::wire::codec::readValue<gm::wire::GMValue>(__br);

    // field: context, type: Any
    gm::wire::GMValue context = gm::wire::codec::readValue<gm::wire::GMValue>(__br);

    // field: swapchain, type: Any
    gm::wire::GMValue swapchain = gm::wire::codec::readValue<gm::wire::GMValue>(__br);

    auto&& __result = igpu_init(device, context, swapchain);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_bind_current()
{
    auto&& __result = igpu_bind_current();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_shutdown()
{
    igpu_shutdown();
    return 0;
}

GMEXPORT char* __EXT_NATIVE__igpu_version()
{
    static std::string __result;
    __result = igpu_version();
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__igpu_is_available()
{
    auto&& __result = igpu_is_available();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_device_lost()
{
    auto&& __result = igpu_device_lost();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_get_feature_level()
{
    auto&& __result = igpu_get_feature_level();
    return static_cast<double>(__result);
}

GMEXPORT char* __EXT_NATIVE__igpu_get_adapter_description()
{
    static std::string __result;
    __result = igpu_get_adapter_description();
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__igpu_get_video_memory(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = igpu_get_video_memory();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_get_backbuffer_width()
{
    auto&& __result = igpu_get_backbuffer_width();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_get_backbuffer_height()
{
    auto&& __result = igpu_get_backbuffer_height();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_get_capabilities(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = igpu_get_capabilities();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Any
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_supports(double capability)
{
    auto&& __result = igpu_supports(static_cast<std::int32_t>(capability));
    return static_cast<double>(__result);
}

GMEXPORT char* __EXT_NATIVE__igpu_get_shader_dialect()
{
    static std::string __result;
    __result = igpu_get_shader_dialect();
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__igpu_set_graphics_info(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: vendor, type: String
    std::string_view vendor = gm::wire::codec::readValue<std::string_view>(__br);

    // field: version, type: String
    std::string_view version = gm::wire::codec::readValue<std::string_view>(__br);

    // field: renderer, type: String
    std::string_view renderer = gm::wire::codec::readValue<std::string_view>(__br);

    // field: shading_language, type: String
    std::string_view shading_language = gm::wire::codec::readValue<std::string_view>(__br);

    // field: max_texture_size, type: Int32
    std::int32_t max_texture_size = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_set_graphics_info(vendor, version, renderer, shading_language, max_texture_size);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_shader_compile(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: source, type: String
    std::string_view source = gm::wire::codec::readValue<std::string_view>(__br);

    // field: entry, type: String
    std::string_view entry = gm::wire::codec::readValue<std::string_view>(__br);

    // field: stage, type: Int32
    std::int32_t stage = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: dialect, type: String
    std::string_view dialect = gm::wire::codec::readValue<std::string_view>(__br);

    auto&& __result = igpu_shader_compile(source, entry, stage, dialect);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_shader_release(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: shader, type: UInt64
    std::uint64_t shader = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_shader_release(shader);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_shader_bind(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: shader, type: Int64
    std::int64_t shader = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: stage, type: Int32
    std::int32_t stage = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_shader_bind(shader, stage);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_get_bound_shader(double stage, char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = igpu_get_bound_shader(static_cast<std::int32_t>(stage));
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT char* __EXT_NATIVE__igpu_get_last_error()
{
    static std::string __result;
    __result = igpu_get_last_error();
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__igpu_input_layout_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: shader, type: Int64
    std::int64_t shader = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: usage, type: AnyArray
    gm::wire::GMArrayView usage = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: type, type: AnyArray
    gm::wire::GMArrayView type = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: step, type: AnyArray
    gm::wire::GMArrayView step = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: element_count, type: Int32
    std::int32_t element_count = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: vertex_stride, type: Int32
    std::int32_t vertex_stride = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: instance_stride, type: Int32
    std::int32_t instance_stride = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_input_layout_create(shader, usage, type, step, element_count, vertex_stride, instance_stride);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_input_layout_release(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: layout, type: UInt64
    std::uint64_t layout = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_input_layout_release(layout);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_buffer_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: size, type: Int64
    std::int64_t size = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: usage, type: Int32
    std::int32_t usage = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: bind, type: Int32
    std::int32_t bind = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stride, type: Int32
    std::int32_t stride = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_buffer_create(size, usage, bind, stride);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_buffer_write(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: offset, type: Int64
    std::int64_t offset = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    auto&& __result = igpu_buffer_write(buffer, offset, data);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_buffer_resize(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: size, type: Int64
    std::int64_t size = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_buffer_resize(buffer, size);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_buffer_read(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: offset, type: Int64
    std::int64_t offset = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: dest, type: Buffer
    gm::wire::GMBuffer dest = __buffer_queue.front();
    __buffer_queue.pop();

    auto&& __result = igpu_buffer_read(buffer, offset, dest);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_buffer_size(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_buffer_size(buffer);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_buffer_release(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_buffer_release(buffer);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_storage_bind(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: stage, type: Int32
    std::int32_t stage = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: slot, type: Int32
    std::int32_t slot = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_storage_bind(buffer, stage, slot);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_shader_reflect(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: shader, type: Int64
    std::int64_t shader = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_shader_reflect(shader);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: struct IgpuUniformBlock[]
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_uniform_write(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: shader, type: Int64
    std::int64_t shader = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: block, type: String
    std::string_view block = gm::wire::codec::readValue<std::string_view>(__br);

    // field: member, type: String
    std::string_view member = gm::wire::codec::readValue<std::string_view>(__br);

    // field: values, type: AnyArray
    gm::wire::GMArrayView values = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    auto&& __result = igpu_uniform_write(buffer, shader, block, member, values);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_uniform_bind(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: stage, type: Int32
    std::int32_t stage = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: slot, type: Int32
    std::int32_t slot = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_uniform_bind(buffer, stage, slot);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_draw(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: vertex_buffer, type: UInt64
    std::uint64_t vertex_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: instance_buffer, type: UInt64
    std::uint64_t instance_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: layout, type: UInt64
    std::uint64_t layout = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: primitive, type: Int32
    std::int32_t primitive = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: first_vertex, type: Int64
    std::int64_t first_vertex = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: vertex_count, type: Int64
    std::int64_t vertex_count = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: instance_count, type: Int64
    std::int64_t instance_count = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: blend_state, type: Int64
    std::int64_t blend_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: depth_state, type: Int64
    std::int64_t depth_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: raster_state, type: Int64
    std::int64_t raster_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: sampler_state, type: Int64
    std::int64_t sampler_state = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_draw(vertex_buffer, instance_buffer, layout, primitive, first_vertex, vertex_count, instance_count, blend_state, depth_state, raster_state, sampler_state);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_draw_indexed(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: vertex_buffer, type: UInt64
    std::uint64_t vertex_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: layout, type: UInt64
    std::uint64_t layout = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: index_buffer, type: UInt64
    std::uint64_t index_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: primitive, type: Int32
    std::int32_t primitive = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: first_index, type: Int64
    std::int64_t first_index = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: index_count, type: Int64
    std::int64_t index_count = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: blend_state, type: Int64
    std::int64_t blend_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: depth_state, type: Int64
    std::int64_t depth_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: raster_state, type: Int64
    std::int64_t raster_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: sampler_state, type: Int64
    std::int64_t sampler_state = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_draw_indexed(vertex_buffer, layout, index_buffer, primitive, first_index, index_count, blend_state, depth_state, raster_state, sampler_state);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_draw_indirect(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: vertex_buffer, type: UInt64
    std::uint64_t vertex_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: instance_buffer, type: UInt64
    std::uint64_t instance_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: layout, type: UInt64
    std::uint64_t layout = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: primitive, type: Int32
    std::int32_t primitive = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: args, type: UInt64
    std::uint64_t args = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: args_offset, type: Int64
    std::int64_t args_offset = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: blend_state, type: Int64
    std::int64_t blend_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: depth_state, type: Int64
    std::int64_t depth_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: raster_state, type: Int64
    std::int64_t raster_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: sampler_state, type: Int64
    std::int64_t sampler_state = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_draw_indirect(vertex_buffer, instance_buffer, layout, primitive, args, args_offset, blend_state, depth_state, raster_state, sampler_state);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_draw_patch(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: vertex_buffer, type: UInt64
    std::uint64_t vertex_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: layout, type: UInt64
    std::uint64_t layout = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: control_points, type: Int32
    std::int32_t control_points = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: first_vertex, type: Int64
    std::int64_t first_vertex = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: vertex_count, type: Int64
    std::int64_t vertex_count = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: blend_state, type: Int64
    std::int64_t blend_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: depth_state, type: Int64
    std::int64_t depth_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: raster_state, type: Int64
    std::int64_t raster_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: sampler_state, type: Int64
    std::int64_t sampler_state = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_draw_patch(vertex_buffer, layout, control_points, first_vertex, vertex_count, blend_state, depth_state, raster_state, sampler_state);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_draw_indexed_indirect(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: vertex_buffer, type: UInt64
    std::uint64_t vertex_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: instance_buffer, type: UInt64
    std::uint64_t instance_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: layout, type: UInt64
    std::uint64_t layout = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: index_buffer, type: UInt64
    std::uint64_t index_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: primitive, type: Int32
    std::int32_t primitive = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: args, type: UInt64
    std::uint64_t args = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: args_offset, type: Int64
    std::int64_t args_offset = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: blend_state, type: Int64
    std::int64_t blend_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: depth_state, type: Int64
    std::int64_t depth_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: raster_state, type: Int64
    std::int64_t raster_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: sampler_state, type: Int64
    std::int64_t sampler_state = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_draw_indexed_indirect(vertex_buffer, instance_buffer, layout, index_buffer, primitive, args, args_offset, blend_state, depth_state, raster_state, sampler_state);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_blend_state_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: enabled, type: Bool
    bool enabled = gm::wire::codec::readValue<bool>(__br);

    // field: src, type: Int32
    std::int32_t src = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: dest, type: Int32
    std::int32_t dest = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: equation, type: Int32
    std::int32_t equation = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: src_alpha, type: Int32
    std::int32_t src_alpha = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: dest_alpha, type: Int32
    std::int32_t dest_alpha = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: equation_alpha, type: Int32
    std::int32_t equation_alpha = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: write_red, type: Bool
    bool write_red = gm::wire::codec::readValue<bool>(__br);

    // field: write_green, type: Bool
    bool write_green = gm::wire::codec::readValue<bool>(__br);

    // field: write_blue, type: Bool
    bool write_blue = gm::wire::codec::readValue<bool>(__br);

    // field: write_alpha, type: Bool
    bool write_alpha = gm::wire::codec::readValue<bool>(__br);

    auto&& __result = igpu_blend_state_create(enabled, src, dest, equation, src_alpha, dest_alpha, equation_alpha, write_red, write_green, write_blue, write_alpha);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_depth_state_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: depth_test, type: Bool
    bool depth_test = gm::wire::codec::readValue<bool>(__br);

    // field: depth_write, type: Bool
    bool depth_write = gm::wire::codec::readValue<bool>(__br);

    // field: depth_func, type: Int32
    std::int32_t depth_func = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stencil_enable, type: Bool
    bool stencil_enable = gm::wire::codec::readValue<bool>(__br);

    // field: stencil_func, type: Int32
    std::int32_t stencil_func = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stencil_fail, type: Int32
    std::int32_t stencil_fail = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stencil_depth_fail, type: Int32
    std::int32_t stencil_depth_fail = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stencil_pass, type: Int32
    std::int32_t stencil_pass = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stencil_ref, type: Int32
    std::int32_t stencil_ref = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stencil_read_mask, type: Int32
    std::int32_t stencil_read_mask = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stencil_write_mask, type: Int32
    std::int32_t stencil_write_mask = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_depth_state_create(depth_test, depth_write, depth_func, stencil_enable, stencil_func, stencil_fail, stencil_depth_fail, stencil_pass, stencil_ref, stencil_read_mask, stencil_write_mask);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_raster_state_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: cull, type: Int32
    std::int32_t cull = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: fill, type: Int32
    std::int32_t fill = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: scissor, type: Bool
    bool scissor = gm::wire::codec::readValue<bool>(__br);

    // field: depth_clip, type: Bool
    bool depth_clip = gm::wire::codec::readValue<bool>(__br);

    auto&& __result = igpu_raster_state_create(cull, fill, scissor, depth_clip);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_sampler_state_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: magnification, type: Int32
    std::int32_t magnification = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: minification, type: Int32
    std::int32_t minification = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: mip, type: Int32
    std::int32_t mip = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: address_u, type: Int32
    std::int32_t address_u = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: address_v, type: Int32
    std::int32_t address_v = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: address_w, type: Int32
    std::int32_t address_w = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: anisotropy, type: Int32
    std::int32_t anisotropy = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: border, type: Int32
    std::int32_t border = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: compare, type: Int32
    std::int32_t compare = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: level_offset, type: Float32
    float level_offset = gm::wire::codec::readValue<float>(__br);

    // field: finest, type: Float32
    float finest = gm::wire::codec::readValue<float>(__br);

    // field: coarsest, type: Float32
    float coarsest = gm::wire::codec::readValue<float>(__br);

    auto&& __result = igpu_sampler_state_create(magnification, minification, mip, address_u, address_v, address_w, anisotropy, border, compare, level_offset, finest, coarsest);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_state_release(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: state, type: UInt64
    std::uint64_t state = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_state_release(state);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_texture_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: kind, type: Int32
    std::int32_t kind = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: width, type: Int32
    std::int32_t width = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: height, type: Int32
    std::int32_t height = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: depth, type: Int32
    std::int32_t depth = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: format, type: Int32
    std::int32_t format = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: render_target, type: Bool
    bool render_target = gm::wire::codec::readValue<bool>(__br);

    // field: storage, type: Bool
    bool storage = gm::wire::codec::readValue<bool>(__br);

    // field: mip_count, type: Int32
    std::int32_t mip_count = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_texture_create(kind, width, height, depth, format, render_target, storage, mip_count);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_texture_generate_mips(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: texture, type: UInt64
    std::uint64_t texture = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_texture_generate_mips(texture);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_texture_read(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: texture, type: UInt64
    std::uint64_t texture = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: x, type: Int32
    std::int32_t x = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: y, type: Int32
    std::int32_t y = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: layer, type: Int32
    std::int32_t layer = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: mip, type: Int32
    std::int32_t mip = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_texture_read(texture, x, y, layer, mip);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_texture_release(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: texture, type: UInt64
    std::uint64_t texture = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_texture_release(texture);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_dispatch(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: groups_x, type: Int32
    std::int32_t groups_x = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: groups_y, type: Int32
    std::int32_t groups_y = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: groups_z, type: Int32
    std::int32_t groups_z = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: kinds, type: AnyArray
    gm::wire::GMArrayView kinds = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: targets, type: AnyArray
    gm::wire::GMArrayView targets = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: layers, type: AnyArray
    gm::wire::GMArrayView layers = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: mips, type: AnyArray
    gm::wire::GMArrayView mips = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    auto&& __result = igpu_dispatch(groups_x, groups_y, groups_z, kinds, targets, layers, mips);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_query_create(double kind, char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = igpu_query_create(static_cast<std::int32_t>(kind));
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_query_begin(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: query, type: UInt64
    std::uint64_t query = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_query_begin(query);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_query_end(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: query, type: UInt64
    std::uint64_t query = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_query_end(query);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_query_ready(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: query, type: UInt64
    std::uint64_t query = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_query_ready(query);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_query_result(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: query, type: UInt64
    std::uint64_t query = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_query_result(query);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_query_release(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: query, type: UInt64
    std::uint64_t query = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_query_release(query);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_timestamp_frequency(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = igpu_timestamp_frequency();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_fence_create(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = igpu_fence_create();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_fence_signal(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: fence, type: UInt64
    std::uint64_t fence = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_fence_signal(fence);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_fence_signaled(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: fence, type: UInt64
    std::uint64_t fence = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_fence_signaled(fence);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_fence_release(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: fence, type: UInt64
    std::uint64_t fence = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_fence_release(fence);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_draw_to_render_targets(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: vertex_buffer, type: UInt64
    std::uint64_t vertex_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: layout, type: UInt64
    std::uint64_t layout = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: primitive, type: Int32
    std::int32_t primitive = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: first_vertex, type: Int64
    std::int64_t first_vertex = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: vertex_count, type: Int64
    std::int64_t vertex_count = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: targets, type: AnyArray
    gm::wire::GMArrayView targets = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: layers, type: AnyArray
    gm::wire::GMArrayView layers = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: mips, type: AnyArray
    gm::wire::GMArrayView mips = gm::wire::codec::readValue<gm::wire::GMArrayView>(__br);

    // field: blend_state, type: Int64
    std::int64_t blend_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: depth_state, type: Int64
    std::int64_t depth_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: raster_state, type: Int64
    std::int64_t raster_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: sampler_state, type: Int64
    std::int64_t sampler_state = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_draw_to_render_targets(vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, layers, mips, blend_state, depth_state, raster_state, sampler_state);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_draw_sampled(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: vertex_buffer, type: UInt64
    std::uint64_t vertex_buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: layout, type: UInt64
    std::uint64_t layout = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: primitive, type: Int32
    std::int32_t primitive = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: first_vertex, type: Int64
    std::int64_t first_vertex = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: vertex_count, type: Int64
    std::int64_t vertex_count = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: texture, type: UInt64
    std::uint64_t texture = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: blend_state, type: Int64
    std::int64_t blend_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: depth_state, type: Int64
    std::int64_t depth_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: raster_state, type: Int64
    std::int64_t raster_state = gm::wire::codec::readValue<std::int64_t>(__br);

    // field: sampler_state, type: Int64
    std::int64_t sampler_state = gm::wire::codec::readValue<std::int64_t>(__br);

    auto&& __result = igpu_draw_sampled(vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, blend_state, depth_state, raster_state, sampler_state);
    return static_cast<double>(__result);
}

