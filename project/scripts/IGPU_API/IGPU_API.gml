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
    Compute = 2,
    Geometry = 3,
    Hull = 4,
    Domain = 5,
    Mesh = 6,
    Amplification = 7
}

enum IgpuCapability
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
    AdapterInfo = 60,
    VideoMemory = 61,
    BackbufferSize = 62
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
 * @returns {Any}
 */
function igpu_get_capabilities()
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __decoders__ = __IGPU_get_decoders();

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_get_capabilities(buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = __ext_core_buffer_unmarshal_value(__ret_buffer__, __decoders__);
    return __result__;
}

// Skipping function igpu_supports (no wrapper is required)


// Skipping function igpu_get_shader_dialect (no wrapper is required)


/**
 * @param {String} _source
 * @param {String} _entry
 * @param {Real} _stage
 * @param {String} _dialect
 * @returns {Real}
 */
function igpu_shader_compile(_source, _entry, _stage, _dialect)
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

    // param: _stage, type: Int32
    if (!is_numeric(_stage)) show_error($"{_GMFUNCTION_} :: _stage expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stage);

    // param: _dialect, type: String
    if (!is_string(_dialect)) show_error($"{_GMFUNCTION_} :: _dialect expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_dialect));
    buffer_write(__args_buffer__, buffer_string, _dialect);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_shader_compile(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {String} _source
 * @param {String} _entry
 * @param {String} _dialect
 * @returns {Real}
 */
function igpu_shader_compile_vertex(_source, _entry, _dialect)
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

    // param: _dialect, type: String
    if (!is_string(_dialect)) show_error($"{_GMFUNCTION_} :: _dialect expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_dialect));
    buffer_write(__args_buffer__, buffer_string, _dialect);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_shader_compile_vertex(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {String} _source
 * @param {String} _entry
 * @param {String} _dialect
 * @returns {Real}
 */
function igpu_shader_compile_pixel(_source, _entry, _dialect)
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

    // param: _dialect, type: String
    if (!is_string(_dialect)) show_error($"{_GMFUNCTION_} :: _dialect expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_dialect));
    buffer_write(__args_buffer__, buffer_string, _dialect);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_shader_compile_pixel(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {String} _source
 * @param {String} _entry
 * @param {String} _dialect
 * @returns {Real}
 */
function igpu_shader_compile_compute(_source, _entry, _dialect)
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

    // param: _dialect, type: String
    if (!is_string(_dialect)) show_error($"{_GMFUNCTION_} :: _dialect expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_dialect));
    buffer_write(__args_buffer__, buffer_string, _dialect);

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


/**
 * @param {Real} _shader
 * @param {Array} _usage
 * @param {Array} _type
 * @param {Real} _element_count
 * @param {Real} _stride
 * @returns {Real}
 */
function igpu_input_layout_create(_shader, _usage, _type, _element_count, _stride)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _shader, type: Int64
    if (!is_numeric(_shader)) show_error($"{_GMFUNCTION_} :: _shader expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _shader);

    // param: _usage, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _usage);

    // param: _type, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _type);

    // param: _element_count, type: Int32
    if (!is_numeric(_element_count)) show_error($"{_GMFUNCTION_} :: _element_count expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _element_count);

    // param: _stride, type: Int32
    if (!is_numeric(_stride)) show_error($"{_GMFUNCTION_} :: _stride expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stride);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_input_layout_create(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _layout
 * @returns {Bool}
 */
function igpu_input_layout_release(_layout)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _layout, type: UInt64
    if (!is_numeric(_layout)) show_error($"{_GMFUNCTION_} :: _layout expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _layout);

    var __return_value__ = __igpu_input_layout_release(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

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
