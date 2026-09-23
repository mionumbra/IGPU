// ##### extgen :: Auto-generated file do not edit!! #####

#include "IGPUInternal_native.h"
#include "IGPUInternal_exports.h"

using namespace gm_structs;
using namespace gm::wire::codec;

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

GMEXPORT char* __EXT_NATIVE__igpu_get_last_error()
{
    static std::string __result;
    __result = igpu_get_last_error();
    return (char*)__result.c_str();
}

