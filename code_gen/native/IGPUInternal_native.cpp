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

GMEXPORT double __EXT_NATIVE__igpu_shader_compile_vertex(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: source, type: String
    std::string_view source = gm::wire::codec::readValue<std::string_view>(__br);

    // field: entry, type: String
    std::string_view entry = gm::wire::codec::readValue<std::string_view>(__br);

    // field: dialect, type: String
    std::string_view dialect = gm::wire::codec::readValue<std::string_view>(__br);

    auto&& __result = igpu_shader_compile_vertex(source, entry, dialect);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_shader_compile_pixel(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: source, type: String
    std::string_view source = gm::wire::codec::readValue<std::string_view>(__br);

    // field: entry, type: String
    std::string_view entry = gm::wire::codec::readValue<std::string_view>(__br);

    // field: dialect, type: String
    std::string_view dialect = gm::wire::codec::readValue<std::string_view>(__br);

    auto&& __result = igpu_shader_compile_pixel(source, entry, dialect);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: Int64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__igpu_shader_compile_compute(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: source, type: String
    std::string_view source = gm::wire::codec::readValue<std::string_view>(__br);

    // field: entry, type: String
    std::string_view entry = gm::wire::codec::readValue<std::string_view>(__br);

    // field: dialect, type: String
    std::string_view dialect = gm::wire::codec::readValue<std::string_view>(__br);

    auto&& __result = igpu_shader_compile_compute(source, entry, dialect);
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

    // field: element_count, type: Int32
    std::int32_t element_count = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: stride, type: Int32
    std::int32_t stride = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = igpu_input_layout_create(shader, usage, type, element_count, stride);
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

GMEXPORT double __EXT_NATIVE__igpu_draw(char* __arg_buffer, double __arg_buffer_length)
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

    auto&& __result = igpu_draw(vertex_buffer, layout, primitive, first_vertex, vertex_count);
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

    auto&& __result = igpu_draw_indexed(vertex_buffer, layout, index_buffer, primitive, first_index, index_count);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_get_draw_count()
{
    auto&& __result = igpu_get_draw_count();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_get_draw_restore_failures()
{
    auto&& __result = igpu_get_draw_restore_failures();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__igpu_is_vertex_buffer_bound(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: buffer, type: UInt64
    std::uint64_t buffer = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = igpu_is_vertex_buffer_bound(buffer);
    return static_cast<double>(__result);
}

