// ##### extgen :: Auto-generated file do not edit!! #####

// #####################################################################
// # Macros
// #####################################################################

// #####################################################################
// # Enums
// #####################################################################

enum IgpuFeatureLevel
{
    Unknown = 0,
    Level_11_0 = 1,
    Level_11_1 = 2,
    Level_12_0 = 3,
    Level_12_1 = 4
}

enum IgpuShaderStage
{
    Vertex = 0,
    Pixel = 1,
    Compute = 2
}

// #####################################################################
// # Constructors
// #####################################################################

// #####################################################################
// # Codecs
// #####################################################################

// #####################################################################
// # Functions
// #####################################################################

/**
 * @param {Any} _device
 * @param {Any} _context
 * @param {Any} _swapchain
 * @returns {Bool}
 */
function igpu_init(_device, _context, _swapchain)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: Any

    __ext_core_buffer_marshal_value(__args_buffer__, _device);

    // param: _context, type: Any

    __ext_core_buffer_marshal_value(__args_buffer__, _context);

    // param: _swapchain, type: Any

    __ext_core_buffer_marshal_value(__args_buffer__, _swapchain);

    var __return_value__ = __igpu_init(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function igpu_shutdown (no wrapper is required)


// Skipping function igpu_version (no wrapper is required)


// Skipping function igpu_is_available (no wrapper is required)


// Skipping function igpu_get_feature_level (no wrapper is required)


// Skipping function igpu_get_adapter_description (no wrapper is required)


/**
 * @returns {Real}
 */
function igpu_get_video_memory()
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_get_video_memory(buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

// Skipping function igpu_get_backbuffer_width (no wrapper is required)


// Skipping function igpu_get_backbuffer_height (no wrapper is required)


/**
 * @param {String} _source
 * @param {String} _entry
 * @param {String} _target
 * @returns {Real}
 */
function igpu_shader_compile_vertex(_source, _entry, _target)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _source, type: String
    if (!is_string(_source)) show_error($"{_GMFUNCTION_} :: _source expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_source));
    buffer_write(__args_buffer__, buffer_string, _source);

    // param: _entry, type: String
    if (!is_string(_entry)) show_error($"{_GMFUNCTION_} :: _entry expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_entry));
    buffer_write(__args_buffer__, buffer_string, _entry);

    // param: _target, type: String
    if (!is_string(_target)) show_error($"{_GMFUNCTION_} :: _target expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_target));
    buffer_write(__args_buffer__, buffer_string, _target);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_shader_compile_vertex(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {String} _source
 * @param {String} _entry
 * @param {String} _target
 * @returns {Real}
 */
function igpu_shader_compile_pixel(_source, _entry, _target)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _source, type: String
    if (!is_string(_source)) show_error($"{_GMFUNCTION_} :: _source expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_source));
    buffer_write(__args_buffer__, buffer_string, _source);

    // param: _entry, type: String
    if (!is_string(_entry)) show_error($"{_GMFUNCTION_} :: _entry expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_entry));
    buffer_write(__args_buffer__, buffer_string, _entry);

    // param: _target, type: String
    if (!is_string(_target)) show_error($"{_GMFUNCTION_} :: _target expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_target));
    buffer_write(__args_buffer__, buffer_string, _target);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_shader_compile_pixel(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {String} _source
 * @param {String} _entry
 * @param {String} _target
 * @returns {Real}
 */
function igpu_shader_compile_compute(_source, _entry, _target)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _source, type: String
    if (!is_string(_source)) show_error($"{_GMFUNCTION_} :: _source expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_source));
    buffer_write(__args_buffer__, buffer_string, _source);

    // param: _entry, type: String
    if (!is_string(_entry)) show_error($"{_GMFUNCTION_} :: _entry expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_entry));
    buffer_write(__args_buffer__, buffer_string, _entry);

    // param: _target, type: String
    if (!is_string(_target)) show_error($"{_GMFUNCTION_} :: _target expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_target));
    buffer_write(__args_buffer__, buffer_string, _target);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_shader_compile_compute(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _shader
 * @returns {Bool}
 */
function igpu_shader_release(_shader)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _shader, type: UInt64
    if (!is_numeric(_shader)) show_error($"{_GMFUNCTION_} :: _shader expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _shader);

    var __return_value__ = __igpu_shader_release(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function igpu_get_last_error (no wrapper is required)


/// @ignore
function __IGPU_get_decoders()
{
    static __decoders__ = [];
    return __decoders__;
}
/// @ignore
function __IGPU_is_available()
{
    static __available__ = extension_exists("IGPU");
    return __available__;
}
