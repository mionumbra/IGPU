// ##### extgen :: Auto-generated file do not edit!! #####

// #####################################################################
// # Macros
// #####################################################################

// #####################################################################
// # Enums
// #####################################################################

enum IgpuVertexStep
{
    Vertex = 0,
    Instance = 1
}

enum IgpuUniformType
{
    Float = 0,
    Int = 1,
    Uint = 2,
    Bool = 3,
    Struct = 4,
    Unknown = 5
}

enum IgpuAddressMode
{
    Clamp = 0,
    Repeat = 1,
    Mirror = 2,
    Border = 3
}

enum IgpuWriteTarget
{
    Texture = 0,
    Buffer = 1
}

enum IgpuQueryKind
{
    Occlusion = 0,
    Timestamp = 1
}

enum IgpuTextureKind
{
    TwoD = 0,
    ThreeD = 1,
    Array = 2,
    Cube = 3
}

enum IgpuFill
{
    Solid = 0,
    Wireframe = 1
}

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

enum IgpuBufferUsage
{
    Static = 0,
    Dynamic = 1,
    Staging = 2
}

enum IgpuBufferBind
{
    None = 0,
    Vertex = 1,
    Index = 2,
    Uniform = 4,
    Storage = 8,
    Indirect = 16
}

enum IgpuPrimitive
{
    PointList = 1,
    LineList = 2,
    LineStrip = 3,
    TriangleList = 4,
    TriangleStrip = 5,
    TriangleFan = 6
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
}

// #####################################################################
// # Constructors
// #####################################################################

/**
 * @returns {Struct.IgpuUniformMember}
 */
function IgpuUniformMember() constructor
{
    /**
     * Internally generated hash for quick validation
     * @ignore
     */
    static __uid = 2351395888;

    self.name = undefined;
    self.offset = undefined;
    self.size = undefined;
    self.type = undefined;
    self.rows = undefined;
    self.columns = undefined;
    self.elements = undefined;

}

/**
 * @returns {Struct.IgpuUniformBlock}
 */
function IgpuUniformBlock() constructor
{
    /**
     * Internally generated hash for quick validation
     * @ignore
     */
    static __uid = 12738947;

    self.name = undefined;
    self.size = undefined;
    self.slot = undefined;
    self.members = undefined;

}

// #####################################################################
// # Codecs
// #####################################################################

/**
 * @func __IgpuUniformMember_encode(_inst, _buffer, _offset, _where)
 * @param {Struct.IgpuUniformMember} _inst
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @param {String} _where
 * @ignore
 */
function __IgpuUniformMember_encode(_inst, _buffer, _offset, _where = _GMFUNCTION_)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);
    with (_inst)
    {
        // field: name, type: String
        if (!is_string(self.name)) show_error($"{_where} :: self.name expected string", true);
        buffer_write(_buffer, buffer_u32, string_byte_length(self.name));
        buffer_write(_buffer, buffer_string, self.name);

        // field: offset, type: Int32
        if (!is_numeric(self.offset)) show_error($"{_where} :: self.offset expected number", true);
        buffer_write(_buffer, buffer_s32, self.offset);

        // field: size, type: Int32
        if (!is_numeric(self.size)) show_error($"{_where} :: self.size expected number", true);
        buffer_write(_buffer, buffer_s32, self.size);

        // field: type, type: Int32
        if (!is_numeric(self.type)) show_error($"{_where} :: self.type expected number", true);
        buffer_write(_buffer, buffer_s32, self.type);

        // field: rows, type: Int32
        if (!is_numeric(self.rows)) show_error($"{_where} :: self.rows expected number", true);
        buffer_write(_buffer, buffer_s32, self.rows);

        // field: columns, type: Int32
        if (!is_numeric(self.columns)) show_error($"{_where} :: self.columns expected number", true);
        buffer_write(_buffer, buffer_s32, self.columns);

        // field: elements, type: Int32
        if (!is_numeric(self.elements)) show_error($"{_where} :: self.elements expected number", true);
        buffer_write(_buffer, buffer_s32, self.elements);

    }
}

/**
 * @func __IgpuUniformMember_decode(_buffer, _offset)
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @returns {Struct.IgpuUniformMember}
 * @ignore
 */
function __IgpuUniformMember_decode(_buffer, _offset)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);

    _inst = new IgpuUniformMember();
    with (_inst)
    {
        // field: name, type: String
        buffer_read(_buffer, buffer_u32);
        self.name = buffer_read(_buffer, buffer_string);

        // field: offset, type: Int32
        self.offset = buffer_read(_buffer, buffer_s32);

        // field: size, type: Int32
        self.size = buffer_read(_buffer, buffer_s32);

        // field: type, type: Int32
        self.type = buffer_read(_buffer, buffer_s32);

        // field: rows, type: Int32
        self.rows = buffer_read(_buffer, buffer_s32);

        // field: columns, type: Int32
        self.columns = buffer_read(_buffer, buffer_s32);

        // field: elements, type: Int32
        self.elements = buffer_read(_buffer, buffer_s32);

    }

    return _inst;
}

/**
 * @func __IgpuUniformBlock_encode(_inst, _buffer, _offset, _where)
 * @param {Struct.IgpuUniformBlock} _inst
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @param {String} _where
 * @ignore
 */
function __IgpuUniformBlock_encode(_inst, _buffer, _offset, _where = _GMFUNCTION_)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);
    with (_inst)
    {
        // field: name, type: String
        if (!is_string(self.name)) show_error($"{_where} :: self.name expected string", true);
        buffer_write(_buffer, buffer_u32, string_byte_length(self.name));
        buffer_write(_buffer, buffer_string, self.name);

        // field: size, type: Int32
        if (!is_numeric(self.size)) show_error($"{_where} :: self.size expected number", true);
        buffer_write(_buffer, buffer_s32, self.size);

        // field: slot, type: Int32
        if (!is_numeric(self.slot)) show_error($"{_where} :: self.slot expected number", true);
        buffer_write(_buffer, buffer_s32, self.slot);

        // field: members, type: struct IgpuUniformMember[]
        if (!is_array(self.members)) show_error($"{_where} :: self.members expected array", true);
        var __length__ = array_length(self.members);
        buffer_write(_buffer, buffer_u32, __length__);
        for (var _i = 0; _i < __length__; ++_i)
        {
            if (self.members[_i].__uid != 2351395888) show_error($"{_where} :: self.members[_i] expected IgpuUniformMember", true);
            __IgpuUniformMember_encode(self.members[_i], _buffer, buffer_tell(_buffer), _where);
        }

    }
}

/**
 * @func __IgpuUniformBlock_decode(_buffer, _offset)
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @returns {Struct.IgpuUniformBlock}
 * @ignore
 */
function __IgpuUniformBlock_decode(_buffer, _offset)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);

    _inst = new IgpuUniformBlock();
    with (_inst)
    {
        // field: name, type: String
        buffer_read(_buffer, buffer_u32);
        self.name = buffer_read(_buffer, buffer_string);

        // field: size, type: Int32
        self.size = buffer_read(_buffer, buffer_s32);

        // field: slot, type: Int32
        self.slot = buffer_read(_buffer, buffer_s32);

        // field: members, type: struct IgpuUniformMember[]
        var __length__ = buffer_read(_buffer, buffer_u32);
        self.members = array_create(__length__);
        for (var _i = 0; _i < __length__; ++_i)
        {
            self.members[_i] = __IgpuUniformMember_decode(_buffer, buffer_tell(_buffer));
        }

    }

    return _inst;
}

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


// Skipping function igpu_device_lost (no wrapper is required)


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
 * @param {String} _vendor
 * @param {String} _version
 * @param {String} _renderer
 * @param {String} _shading_language
 * @param {Real} _max_texture_size
 * @returns {Bool}
 */
function igpu_set_graphics_info(_vendor, _version, _renderer, _shading_language, _max_texture_size)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _vendor, type: String
    if (!is_string(_vendor)) show_error($"{_GMFUNCTION_} :: _vendor expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_vendor));
    buffer_write(__args_buffer__, buffer_string, _vendor);

    // param: _version, type: String
    if (!is_string(_version)) show_error($"{_GMFUNCTION_} :: _version expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_version));
    buffer_write(__args_buffer__, buffer_string, _version);

    // param: _renderer, type: String
    if (!is_string(_renderer)) show_error($"{_GMFUNCTION_} :: _renderer expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_renderer));
    buffer_write(__args_buffer__, buffer_string, _renderer);

    // param: _shading_language, type: String
    if (!is_string(_shading_language)) show_error($"{_GMFUNCTION_} :: _shading_language expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_shading_language));
    buffer_write(__args_buffer__, buffer_string, _shading_language);

    // param: _max_texture_size, type: Int32
    if (!is_numeric(_max_texture_size)) show_error($"{_GMFUNCTION_} :: _max_texture_size expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _max_texture_size);

    var __return_value__ = __igpu_set_graphics_info(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

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

/**
 * @param {Real} _shader
 * @param {Real} _stage
 * @returns {Bool}
 */
function igpu_shader_bind(_shader, _stage)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _shader, type: Int64
    if (!is_numeric(_shader)) show_error($"{_GMFUNCTION_} :: _shader expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _shader);

    // param: _stage, type: Int32
    if (!is_numeric(_stage)) show_error($"{_GMFUNCTION_} :: _stage expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stage);

    var __return_value__ = __igpu_shader_bind(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _stage
 * @returns {Real}
 */
function igpu_get_bound_shader(_stage)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_get_bound_shader(_stage, buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

// Skipping function igpu_get_last_error (no wrapper is required)


/**
 * @param {Real} _shader
 * @param {Array} _usage
 * @param {Array} _type
 * @param {Array} _step
 * @param {Real} _element_count
 * @param {Real} _vertex_stride
 * @param {Real} _instance_stride
 * @returns {Real}
 */
function igpu_input_layout_create(_shader, _usage, _type, _step, _element_count, _vertex_stride, _instance_stride)
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

    // param: _step, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _step);

    // param: _element_count, type: Int32
    if (!is_numeric(_element_count)) show_error($"{_GMFUNCTION_} :: _element_count expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _element_count);

    // param: _vertex_stride, type: Int32
    if (!is_numeric(_vertex_stride)) show_error($"{_GMFUNCTION_} :: _vertex_stride expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _vertex_stride);

    // param: _instance_stride, type: Int32
    if (!is_numeric(_instance_stride)) show_error($"{_GMFUNCTION_} :: _instance_stride expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _instance_stride);

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

/**
 * @param {Real} _size
 * @param {Real} _usage
 * @param {Real} _bind
 * @param {Real} _stride
 * @returns {Real}
 */
function igpu_buffer_create(_size, _usage, _bind, _stride)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _size, type: Int64
    if (!is_numeric(_size)) show_error($"{_GMFUNCTION_} :: _size expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _size);

    // param: _usage, type: Int32
    if (!is_numeric(_usage)) show_error($"{_GMFUNCTION_} :: _usage expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _usage);

    // param: _bind, type: Int32
    if (!is_numeric(_bind)) show_error($"{_GMFUNCTION_} :: _bind expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _bind);

    // param: _stride, type: Int32
    if (!is_numeric(_stride)) show_error($"{_GMFUNCTION_} :: _stride expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stride);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_buffer_create(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _buffer
 * @param {Real} _offset
 * @param {Id.Buffer} _data
 * @returns {Bool}
 */
function igpu_buffer_write(_buffer, _offset, _data)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _buffer, type: UInt64
    if (!is_numeric(_buffer)) show_error($"{_GMFUNCTION_} :: _buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _buffer);

    // param: _offset, type: Int64
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _offset);

    // param: _data, type: Buffer
    if (!buffer_exists(_data)) show_error($"{_GMFUNCTION_} :: _data expected Id.Buffer", true);
    __IGPU_queue_buffer(buffer_get_address(_data), buffer_get_size(_data));

    var __return_value__ = __igpu_buffer_write(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _buffer
 * @param {Real} _size
 * @returns {Bool}
 */
function igpu_buffer_resize(_buffer, _size)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _buffer, type: UInt64
    if (!is_numeric(_buffer)) show_error($"{_GMFUNCTION_} :: _buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _buffer);

    // param: _size, type: Int64
    if (!is_numeric(_size)) show_error($"{_GMFUNCTION_} :: _size expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _size);

    var __return_value__ = __igpu_buffer_resize(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _buffer
 * @param {Real} _offset
 * @param {Id.Buffer} _dest
 * @returns {Bool}
 */
function igpu_buffer_read(_buffer, _offset, _dest)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _buffer, type: UInt64
    if (!is_numeric(_buffer)) show_error($"{_GMFUNCTION_} :: _buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _buffer);

    // param: _offset, type: Int64
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _offset);

    // param: _dest, type: Buffer
    if (!buffer_exists(_dest)) show_error($"{_GMFUNCTION_} :: _dest expected Id.Buffer", true);
    __IGPU_queue_buffer(buffer_get_address(_dest), buffer_get_size(_dest));

    var __return_value__ = __igpu_buffer_read(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _buffer
 * @returns {Real}
 */
function igpu_buffer_size(_buffer)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _buffer, type: UInt64
    if (!is_numeric(_buffer)) show_error($"{_GMFUNCTION_} :: _buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _buffer);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_buffer_size(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _buffer
 * @returns {Bool}
 */
function igpu_buffer_release(_buffer)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _buffer, type: UInt64
    if (!is_numeric(_buffer)) show_error($"{_GMFUNCTION_} :: _buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _buffer);

    var __return_value__ = __igpu_buffer_release(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _buffer
 * @param {Real} _stage
 * @param {Real} _slot
 * @returns {Bool}
 */
function igpu_storage_bind(_buffer, _stage, _slot)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _buffer, type: UInt64
    if (!is_numeric(_buffer)) show_error($"{_GMFUNCTION_} :: _buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _buffer);

    // param: _stage, type: Int32
    if (!is_numeric(_stage)) show_error($"{_GMFUNCTION_} :: _stage expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stage);

    // param: _slot, type: Int32
    if (!is_numeric(_slot)) show_error($"{_GMFUNCTION_} :: _slot expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _slot);

    var __return_value__ = __igpu_storage_bind(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _shader
 * @returns {Array[Struct.IgpuUniformBlock]}
 */
function igpu_shader_reflect(_shader)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _shader, type: Int64
    if (!is_numeric(_shader)) show_error($"{_GMFUNCTION_} :: _shader expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _shader);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_shader_reflect(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    var __length__ = buffer_read(__ret_buffer__, buffer_u32);
    __result__ = array_create(__length__);
    for (var _i = 0; _i < __length__; ++_i)
    {
        __result__[_i] = __IgpuUniformBlock_decode(__ret_buffer__, buffer_tell(__ret_buffer__));
    }
    return __result__;
}

/**
 * @param {Real} _buffer
 * @param {Real} _shader
 * @param {String} _block
 * @param {String} _member
 * @param {Array} _values
 * @returns {Bool}
 */
function igpu_uniform_write(_buffer, _shader, _block, _member, _values)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _buffer, type: UInt64
    if (!is_numeric(_buffer)) show_error($"{_GMFUNCTION_} :: _buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _buffer);

    // param: _shader, type: Int64
    if (!is_numeric(_shader)) show_error($"{_GMFUNCTION_} :: _shader expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _shader);

    // param: _block, type: String
    if (!is_string(_block)) show_error($"{_GMFUNCTION_} :: _block expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_block));
    buffer_write(__args_buffer__, buffer_string, _block);

    // param: _member, type: String
    if (!is_string(_member)) show_error($"{_GMFUNCTION_} :: _member expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_member));
    buffer_write(__args_buffer__, buffer_string, _member);

    // param: _values, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _values);

    var __return_value__ = __igpu_uniform_write(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _buffer
 * @param {Real} _stage
 * @param {Real} _slot
 * @returns {Bool}
 */
function igpu_uniform_bind(_buffer, _stage, _slot)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _buffer, type: UInt64
    if (!is_numeric(_buffer)) show_error($"{_GMFUNCTION_} :: _buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _buffer);

    // param: _stage, type: Int32
    if (!is_numeric(_stage)) show_error($"{_GMFUNCTION_} :: _stage expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stage);

    // param: _slot, type: Int32
    if (!is_numeric(_slot)) show_error($"{_GMFUNCTION_} :: _slot expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _slot);

    var __return_value__ = __igpu_uniform_bind(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _vertex_buffer
 * @param {Real} _instance_buffer
 * @param {Real} _layout
 * @param {Real} _primitive
 * @param {Real} _first_vertex
 * @param {Real} _vertex_count
 * @param {Real} _instance_count
 * @param {Real} _blend_state
 * @param {Real} _depth_state
 * @param {Real} _raster_state
 * @param {Real} _sampler_state
 * @returns {Bool}
 */
function igpu_draw(_vertex_buffer, _instance_buffer, _layout, _primitive, _first_vertex, _vertex_count, _instance_count, _blend_state, _depth_state, _raster_state, _sampler_state)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _vertex_buffer, type: UInt64
    if (!is_numeric(_vertex_buffer)) show_error($"{_GMFUNCTION_} :: _vertex_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_buffer);

    // param: _instance_buffer, type: UInt64
    if (!is_numeric(_instance_buffer)) show_error($"{_GMFUNCTION_} :: _instance_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _instance_buffer);

    // param: _layout, type: UInt64
    if (!is_numeric(_layout)) show_error($"{_GMFUNCTION_} :: _layout expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _layout);

    // param: _primitive, type: Int32
    if (!is_numeric(_primitive)) show_error($"{_GMFUNCTION_} :: _primitive expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _primitive);

    // param: _first_vertex, type: Int64
    if (!is_numeric(_first_vertex)) show_error($"{_GMFUNCTION_} :: _first_vertex expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _first_vertex);

    // param: _vertex_count, type: Int64
    if (!is_numeric(_vertex_count)) show_error($"{_GMFUNCTION_} :: _vertex_count expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_count);

    // param: _instance_count, type: Int64
    if (!is_numeric(_instance_count)) show_error($"{_GMFUNCTION_} :: _instance_count expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _instance_count);

    // param: _blend_state, type: Int64
    if (!is_numeric(_blend_state)) show_error($"{_GMFUNCTION_} :: _blend_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _blend_state);

    // param: _depth_state, type: Int64
    if (!is_numeric(_depth_state)) show_error($"{_GMFUNCTION_} :: _depth_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _depth_state);

    // param: _raster_state, type: Int64
    if (!is_numeric(_raster_state)) show_error($"{_GMFUNCTION_} :: _raster_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _raster_state);

    // param: _sampler_state, type: Int64
    if (!is_numeric(_sampler_state)) show_error($"{_GMFUNCTION_} :: _sampler_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _sampler_state);

    var __return_value__ = __igpu_draw(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _vertex_buffer
 * @param {Real} _layout
 * @param {Real} _index_buffer
 * @param {Real} _primitive
 * @param {Real} _first_index
 * @param {Real} _index_count
 * @param {Real} _blend_state
 * @param {Real} _depth_state
 * @param {Real} _raster_state
 * @param {Real} _sampler_state
 * @returns {Bool}
 */
function igpu_draw_indexed(_vertex_buffer, _layout, _index_buffer, _primitive, _first_index, _index_count, _blend_state, _depth_state, _raster_state, _sampler_state)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _vertex_buffer, type: UInt64
    if (!is_numeric(_vertex_buffer)) show_error($"{_GMFUNCTION_} :: _vertex_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_buffer);

    // param: _layout, type: UInt64
    if (!is_numeric(_layout)) show_error($"{_GMFUNCTION_} :: _layout expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _layout);

    // param: _index_buffer, type: UInt64
    if (!is_numeric(_index_buffer)) show_error($"{_GMFUNCTION_} :: _index_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _index_buffer);

    // param: _primitive, type: Int32
    if (!is_numeric(_primitive)) show_error($"{_GMFUNCTION_} :: _primitive expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _primitive);

    // param: _first_index, type: Int64
    if (!is_numeric(_first_index)) show_error($"{_GMFUNCTION_} :: _first_index expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _first_index);

    // param: _index_count, type: Int64
    if (!is_numeric(_index_count)) show_error($"{_GMFUNCTION_} :: _index_count expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _index_count);

    // param: _blend_state, type: Int64
    if (!is_numeric(_blend_state)) show_error($"{_GMFUNCTION_} :: _blend_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _blend_state);

    // param: _depth_state, type: Int64
    if (!is_numeric(_depth_state)) show_error($"{_GMFUNCTION_} :: _depth_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _depth_state);

    // param: _raster_state, type: Int64
    if (!is_numeric(_raster_state)) show_error($"{_GMFUNCTION_} :: _raster_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _raster_state);

    // param: _sampler_state, type: Int64
    if (!is_numeric(_sampler_state)) show_error($"{_GMFUNCTION_} :: _sampler_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _sampler_state);

    var __return_value__ = __igpu_draw_indexed(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _vertex_buffer
 * @param {Real} _instance_buffer
 * @param {Real} _layout
 * @param {Real} _primitive
 * @param {Real} _args
 * @param {Real} _args_offset
 * @param {Real} _blend_state
 * @param {Real} _depth_state
 * @param {Real} _raster_state
 * @param {Real} _sampler_state
 * @returns {Bool}
 */
function igpu_draw_indirect(_vertex_buffer, _instance_buffer, _layout, _primitive, _args, _args_offset, _blend_state, _depth_state, _raster_state, _sampler_state)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _vertex_buffer, type: UInt64
    if (!is_numeric(_vertex_buffer)) show_error($"{_GMFUNCTION_} :: _vertex_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_buffer);

    // param: _instance_buffer, type: UInt64
    if (!is_numeric(_instance_buffer)) show_error($"{_GMFUNCTION_} :: _instance_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _instance_buffer);

    // param: _layout, type: UInt64
    if (!is_numeric(_layout)) show_error($"{_GMFUNCTION_} :: _layout expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _layout);

    // param: _primitive, type: Int32
    if (!is_numeric(_primitive)) show_error($"{_GMFUNCTION_} :: _primitive expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _primitive);

    // param: _args, type: UInt64
    if (!is_numeric(_args)) show_error($"{_GMFUNCTION_} :: _args expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _args);

    // param: _args_offset, type: Int64
    if (!is_numeric(_args_offset)) show_error($"{_GMFUNCTION_} :: _args_offset expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _args_offset);

    // param: _blend_state, type: Int64
    if (!is_numeric(_blend_state)) show_error($"{_GMFUNCTION_} :: _blend_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _blend_state);

    // param: _depth_state, type: Int64
    if (!is_numeric(_depth_state)) show_error($"{_GMFUNCTION_} :: _depth_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _depth_state);

    // param: _raster_state, type: Int64
    if (!is_numeric(_raster_state)) show_error($"{_GMFUNCTION_} :: _raster_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _raster_state);

    // param: _sampler_state, type: Int64
    if (!is_numeric(_sampler_state)) show_error($"{_GMFUNCTION_} :: _sampler_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _sampler_state);

    var __return_value__ = __igpu_draw_indirect(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _vertex_buffer
 * @param {Real} _layout
 * @param {Real} _control_points
 * @param {Real} _first_vertex
 * @param {Real} _vertex_count
 * @param {Real} _blend_state
 * @param {Real} _depth_state
 * @param {Real} _raster_state
 * @param {Real} _sampler_state
 * @returns {Bool}
 */
function igpu_draw_patch(_vertex_buffer, _layout, _control_points, _first_vertex, _vertex_count, _blend_state, _depth_state, _raster_state, _sampler_state)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _vertex_buffer, type: UInt64
    if (!is_numeric(_vertex_buffer)) show_error($"{_GMFUNCTION_} :: _vertex_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_buffer);

    // param: _layout, type: UInt64
    if (!is_numeric(_layout)) show_error($"{_GMFUNCTION_} :: _layout expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _layout);

    // param: _control_points, type: Int32
    if (!is_numeric(_control_points)) show_error($"{_GMFUNCTION_} :: _control_points expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _control_points);

    // param: _first_vertex, type: Int64
    if (!is_numeric(_first_vertex)) show_error($"{_GMFUNCTION_} :: _first_vertex expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _first_vertex);

    // param: _vertex_count, type: Int64
    if (!is_numeric(_vertex_count)) show_error($"{_GMFUNCTION_} :: _vertex_count expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_count);

    // param: _blend_state, type: Int64
    if (!is_numeric(_blend_state)) show_error($"{_GMFUNCTION_} :: _blend_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _blend_state);

    // param: _depth_state, type: Int64
    if (!is_numeric(_depth_state)) show_error($"{_GMFUNCTION_} :: _depth_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _depth_state);

    // param: _raster_state, type: Int64
    if (!is_numeric(_raster_state)) show_error($"{_GMFUNCTION_} :: _raster_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _raster_state);

    // param: _sampler_state, type: Int64
    if (!is_numeric(_sampler_state)) show_error($"{_GMFUNCTION_} :: _sampler_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _sampler_state);

    var __return_value__ = __igpu_draw_patch(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _vertex_buffer
 * @param {Real} _instance_buffer
 * @param {Real} _layout
 * @param {Real} _index_buffer
 * @param {Real} _primitive
 * @param {Real} _args
 * @param {Real} _args_offset
 * @param {Real} _blend_state
 * @param {Real} _depth_state
 * @param {Real} _raster_state
 * @param {Real} _sampler_state
 * @returns {Bool}
 */
function igpu_draw_indexed_indirect(_vertex_buffer, _instance_buffer, _layout, _index_buffer, _primitive, _args, _args_offset, _blend_state, _depth_state, _raster_state, _sampler_state)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _vertex_buffer, type: UInt64
    if (!is_numeric(_vertex_buffer)) show_error($"{_GMFUNCTION_} :: _vertex_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_buffer);

    // param: _instance_buffer, type: UInt64
    if (!is_numeric(_instance_buffer)) show_error($"{_GMFUNCTION_} :: _instance_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _instance_buffer);

    // param: _layout, type: UInt64
    if (!is_numeric(_layout)) show_error($"{_GMFUNCTION_} :: _layout expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _layout);

    // param: _index_buffer, type: UInt64
    if (!is_numeric(_index_buffer)) show_error($"{_GMFUNCTION_} :: _index_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _index_buffer);

    // param: _primitive, type: Int32
    if (!is_numeric(_primitive)) show_error($"{_GMFUNCTION_} :: _primitive expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _primitive);

    // param: _args, type: UInt64
    if (!is_numeric(_args)) show_error($"{_GMFUNCTION_} :: _args expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _args);

    // param: _args_offset, type: Int64
    if (!is_numeric(_args_offset)) show_error($"{_GMFUNCTION_} :: _args_offset expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _args_offset);

    // param: _blend_state, type: Int64
    if (!is_numeric(_blend_state)) show_error($"{_GMFUNCTION_} :: _blend_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _blend_state);

    // param: _depth_state, type: Int64
    if (!is_numeric(_depth_state)) show_error($"{_GMFUNCTION_} :: _depth_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _depth_state);

    // param: _raster_state, type: Int64
    if (!is_numeric(_raster_state)) show_error($"{_GMFUNCTION_} :: _raster_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _raster_state);

    // param: _sampler_state, type: Int64
    if (!is_numeric(_sampler_state)) show_error($"{_GMFUNCTION_} :: _sampler_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _sampler_state);

    var __return_value__ = __igpu_draw_indexed_indirect(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Bool} _enabled
 * @param {Real} _src
 * @param {Real} _dest
 * @param {Real} _equation
 * @param {Real} _src_alpha
 * @param {Real} _dest_alpha
 * @param {Real} _equation_alpha
 * @param {Bool} _write_red
 * @param {Bool} _write_green
 * @param {Bool} _write_blue
 * @param {Bool} _write_alpha
 * @returns {Real}
 */
function igpu_blend_state_create(_enabled, _src, _dest, _equation, _src_alpha, _dest_alpha, _equation_alpha, _write_red, _write_green, _write_blue, _write_alpha)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _enabled, type: Bool
    if (!is_bool(_enabled)) show_error($"{_GMFUNCTION_} :: _enabled expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _enabled);

    // param: _src, type: Int32
    if (!is_numeric(_src)) show_error($"{_GMFUNCTION_} :: _src expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _src);

    // param: _dest, type: Int32
    if (!is_numeric(_dest)) show_error($"{_GMFUNCTION_} :: _dest expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _dest);

    // param: _equation, type: Int32
    if (!is_numeric(_equation)) show_error($"{_GMFUNCTION_} :: _equation expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _equation);

    // param: _src_alpha, type: Int32
    if (!is_numeric(_src_alpha)) show_error($"{_GMFUNCTION_} :: _src_alpha expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _src_alpha);

    // param: _dest_alpha, type: Int32
    if (!is_numeric(_dest_alpha)) show_error($"{_GMFUNCTION_} :: _dest_alpha expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _dest_alpha);

    // param: _equation_alpha, type: Int32
    if (!is_numeric(_equation_alpha)) show_error($"{_GMFUNCTION_} :: _equation_alpha expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _equation_alpha);

    // param: _write_red, type: Bool
    if (!is_bool(_write_red)) show_error($"{_GMFUNCTION_} :: _write_red expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _write_red);

    // param: _write_green, type: Bool
    if (!is_bool(_write_green)) show_error($"{_GMFUNCTION_} :: _write_green expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _write_green);

    // param: _write_blue, type: Bool
    if (!is_bool(_write_blue)) show_error($"{_GMFUNCTION_} :: _write_blue expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _write_blue);

    // param: _write_alpha, type: Bool
    if (!is_bool(_write_alpha)) show_error($"{_GMFUNCTION_} :: _write_alpha expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _write_alpha);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_blend_state_create(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Bool} _depth_test
 * @param {Bool} _depth_write
 * @param {Real} _depth_func
 * @param {Bool} _stencil_enable
 * @param {Real} _stencil_func
 * @param {Real} _stencil_fail
 * @param {Real} _stencil_depth_fail
 * @param {Real} _stencil_pass
 * @param {Real} _stencil_ref
 * @param {Real} _stencil_read_mask
 * @param {Real} _stencil_write_mask
 * @returns {Real}
 */
function igpu_depth_state_create(_depth_test, _depth_write, _depth_func, _stencil_enable, _stencil_func, _stencil_fail, _stencil_depth_fail, _stencil_pass, _stencil_ref, _stencil_read_mask, _stencil_write_mask)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _depth_test, type: Bool
    if (!is_bool(_depth_test)) show_error($"{_GMFUNCTION_} :: _depth_test expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _depth_test);

    // param: _depth_write, type: Bool
    if (!is_bool(_depth_write)) show_error($"{_GMFUNCTION_} :: _depth_write expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _depth_write);

    // param: _depth_func, type: Int32
    if (!is_numeric(_depth_func)) show_error($"{_GMFUNCTION_} :: _depth_func expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _depth_func);

    // param: _stencil_enable, type: Bool
    if (!is_bool(_stencil_enable)) show_error($"{_GMFUNCTION_} :: _stencil_enable expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _stencil_enable);

    // param: _stencil_func, type: Int32
    if (!is_numeric(_stencil_func)) show_error($"{_GMFUNCTION_} :: _stencil_func expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stencil_func);

    // param: _stencil_fail, type: Int32
    if (!is_numeric(_stencil_fail)) show_error($"{_GMFUNCTION_} :: _stencil_fail expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stencil_fail);

    // param: _stencil_depth_fail, type: Int32
    if (!is_numeric(_stencil_depth_fail)) show_error($"{_GMFUNCTION_} :: _stencil_depth_fail expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stencil_depth_fail);

    // param: _stencil_pass, type: Int32
    if (!is_numeric(_stencil_pass)) show_error($"{_GMFUNCTION_} :: _stencil_pass expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stencil_pass);

    // param: _stencil_ref, type: Int32
    if (!is_numeric(_stencil_ref)) show_error($"{_GMFUNCTION_} :: _stencil_ref expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stencil_ref);

    // param: _stencil_read_mask, type: Int32
    if (!is_numeric(_stencil_read_mask)) show_error($"{_GMFUNCTION_} :: _stencil_read_mask expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stencil_read_mask);

    // param: _stencil_write_mask, type: Int32
    if (!is_numeric(_stencil_write_mask)) show_error($"{_GMFUNCTION_} :: _stencil_write_mask expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _stencil_write_mask);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_depth_state_create(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _cull
 * @param {Real} _fill
 * @param {Bool} _scissor
 * @param {Bool} _depth_clip
 * @returns {Real}
 */
function igpu_raster_state_create(_cull, _fill, _scissor, _depth_clip)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _cull, type: Int32
    if (!is_numeric(_cull)) show_error($"{_GMFUNCTION_} :: _cull expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _cull);

    // param: _fill, type: Int32
    if (!is_numeric(_fill)) show_error($"{_GMFUNCTION_} :: _fill expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _fill);

    // param: _scissor, type: Bool
    if (!is_bool(_scissor)) show_error($"{_GMFUNCTION_} :: _scissor expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _scissor);

    // param: _depth_clip, type: Bool
    if (!is_bool(_depth_clip)) show_error($"{_GMFUNCTION_} :: _depth_clip expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _depth_clip);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_raster_state_create(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _magnification
 * @param {Real} _minification
 * @param {Real} _mip
 * @param {Real} _address_u
 * @param {Real} _address_v
 * @param {Real} _address_w
 * @param {Real} _anisotropy
 * @param {Real} _border
 * @param {Real} _compare
 * @param {Real} _level_offset
 * @param {Real} _finest
 * @param {Real} _coarsest
 * @returns {Real}
 */
function igpu_sampler_state_create(_magnification, _minification, _mip, _address_u, _address_v, _address_w, _anisotropy, _border, _compare, _level_offset, _finest, _coarsest)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _magnification, type: Int32
    if (!is_numeric(_magnification)) show_error($"{_GMFUNCTION_} :: _magnification expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _magnification);

    // param: _minification, type: Int32
    if (!is_numeric(_minification)) show_error($"{_GMFUNCTION_} :: _minification expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _minification);

    // param: _mip, type: Int32
    if (!is_numeric(_mip)) show_error($"{_GMFUNCTION_} :: _mip expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _mip);

    // param: _address_u, type: Int32
    if (!is_numeric(_address_u)) show_error($"{_GMFUNCTION_} :: _address_u expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _address_u);

    // param: _address_v, type: Int32
    if (!is_numeric(_address_v)) show_error($"{_GMFUNCTION_} :: _address_v expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _address_v);

    // param: _address_w, type: Int32
    if (!is_numeric(_address_w)) show_error($"{_GMFUNCTION_} :: _address_w expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _address_w);

    // param: _anisotropy, type: Int32
    if (!is_numeric(_anisotropy)) show_error($"{_GMFUNCTION_} :: _anisotropy expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _anisotropy);

    // param: _border, type: Int32
    if (!is_numeric(_border)) show_error($"{_GMFUNCTION_} :: _border expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _border);

    // param: _compare, type: Int32
    if (!is_numeric(_compare)) show_error($"{_GMFUNCTION_} :: _compare expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _compare);

    // param: _level_offset, type: Float32
    if (!is_numeric(_level_offset)) show_error($"{_GMFUNCTION_} :: _level_offset expected number", true);
    buffer_write(__args_buffer__, buffer_f32, _level_offset);

    // param: _finest, type: Float32
    if (!is_numeric(_finest)) show_error($"{_GMFUNCTION_} :: _finest expected number", true);
    buffer_write(__args_buffer__, buffer_f32, _finest);

    // param: _coarsest, type: Float32
    if (!is_numeric(_coarsest)) show_error($"{_GMFUNCTION_} :: _coarsest expected number", true);
    buffer_write(__args_buffer__, buffer_f32, _coarsest);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_sampler_state_create(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _state
 * @returns {Bool}
 */
function igpu_state_release(_state)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _state, type: UInt64
    if (!is_numeric(_state)) show_error($"{_GMFUNCTION_} :: _state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _state);

    var __return_value__ = __igpu_state_release(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _kind
 * @param {Real} _width
 * @param {Real} _height
 * @param {Real} _depth
 * @param {Real} _format
 * @param {Bool} _render_target
 * @param {Bool} _storage
 * @param {Real} _mip_count
 * @returns {Real}
 */
function igpu_texture_create(_kind, _width, _height, _depth, _format, _render_target, _storage, _mip_count)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _kind, type: Int32
    if (!is_numeric(_kind)) show_error($"{_GMFUNCTION_} :: _kind expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _kind);

    // param: _width, type: Int32
    if (!is_numeric(_width)) show_error($"{_GMFUNCTION_} :: _width expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _width);

    // param: _height, type: Int32
    if (!is_numeric(_height)) show_error($"{_GMFUNCTION_} :: _height expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _height);

    // param: _depth, type: Int32
    if (!is_numeric(_depth)) show_error($"{_GMFUNCTION_} :: _depth expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _depth);

    // param: _format, type: Int32
    if (!is_numeric(_format)) show_error($"{_GMFUNCTION_} :: _format expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _format);

    // param: _render_target, type: Bool
    if (!is_bool(_render_target)) show_error($"{_GMFUNCTION_} :: _render_target expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _render_target);

    // param: _storage, type: Bool
    if (!is_bool(_storage)) show_error($"{_GMFUNCTION_} :: _storage expected bool", true);
    buffer_write(__args_buffer__, buffer_bool, _storage);

    // param: _mip_count, type: Int32
    if (!is_numeric(_mip_count)) show_error($"{_GMFUNCTION_} :: _mip_count expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _mip_count);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_texture_create(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _texture
 * @returns {Bool}
 */
function igpu_texture_generate_mips(_texture)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _texture, type: UInt64
    if (!is_numeric(_texture)) show_error($"{_GMFUNCTION_} :: _texture expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _texture);

    var __return_value__ = __igpu_texture_generate_mips(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _texture
 * @param {Real} _x
 * @param {Real} _y
 * @param {Real} _layer
 * @param {Real} _mip
 * @returns {Real}
 */
function igpu_texture_read(_texture, _x, _y, _layer, _mip)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _texture, type: UInt64
    if (!is_numeric(_texture)) show_error($"{_GMFUNCTION_} :: _texture expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _texture);

    // param: _x, type: Int32
    if (!is_numeric(_x)) show_error($"{_GMFUNCTION_} :: _x expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _x);

    // param: _y, type: Int32
    if (!is_numeric(_y)) show_error($"{_GMFUNCTION_} :: _y expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _y);

    // param: _layer, type: Int32
    if (!is_numeric(_layer)) show_error($"{_GMFUNCTION_} :: _layer expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _layer);

    // param: _mip, type: Int32
    if (!is_numeric(_mip)) show_error($"{_GMFUNCTION_} :: _mip expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _mip);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_texture_read(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _texture
 * @returns {Bool}
 */
function igpu_texture_release(_texture)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _texture, type: UInt64
    if (!is_numeric(_texture)) show_error($"{_GMFUNCTION_} :: _texture expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _texture);

    var __return_value__ = __igpu_texture_release(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _groups_x
 * @param {Real} _groups_y
 * @param {Real} _groups_z
 * @param {Array} _kinds
 * @param {Array} _targets
 * @param {Array} _layers
 * @param {Array} _mips
 * @returns {Bool}
 */
function igpu_dispatch(_groups_x, _groups_y, _groups_z, _kinds, _targets, _layers, _mips)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _groups_x, type: Int32
    if (!is_numeric(_groups_x)) show_error($"{_GMFUNCTION_} :: _groups_x expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _groups_x);

    // param: _groups_y, type: Int32
    if (!is_numeric(_groups_y)) show_error($"{_GMFUNCTION_} :: _groups_y expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _groups_y);

    // param: _groups_z, type: Int32
    if (!is_numeric(_groups_z)) show_error($"{_GMFUNCTION_} :: _groups_z expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _groups_z);

    // param: _kinds, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _kinds);

    // param: _targets, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _targets);

    // param: _layers, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _layers);

    // param: _mips, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _mips);

    var __return_value__ = __igpu_dispatch(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _kind
 * @returns {Real}
 */
function igpu_query_create(_kind)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_query_create(_kind, buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _query
 * @returns {Bool}
 */
function igpu_query_begin(_query)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _query, type: UInt64
    if (!is_numeric(_query)) show_error($"{_GMFUNCTION_} :: _query expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _query);

    var __return_value__ = __igpu_query_begin(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _query
 * @returns {Bool}
 */
function igpu_query_end(_query)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _query, type: UInt64
    if (!is_numeric(_query)) show_error($"{_GMFUNCTION_} :: _query expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _query);

    var __return_value__ = __igpu_query_end(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _query
 * @returns {Bool}
 */
function igpu_query_ready(_query)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _query, type: UInt64
    if (!is_numeric(_query)) show_error($"{_GMFUNCTION_} :: _query expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _query);

    var __return_value__ = __igpu_query_ready(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _query
 * @returns {Real}
 */
function igpu_query_result(_query)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _query, type: UInt64
    if (!is_numeric(_query)) show_error($"{_GMFUNCTION_} :: _query expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _query);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_query_result(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _query
 * @returns {Bool}
 */
function igpu_query_release(_query)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _query, type: UInt64
    if (!is_numeric(_query)) show_error($"{_GMFUNCTION_} :: _query expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _query);

    var __return_value__ = __igpu_query_release(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @returns {Real}
 */
function igpu_timestamp_frequency()
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_timestamp_frequency(buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @returns {Real}
 */
function igpu_fence_create()
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __igpu_fence_create(buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _fence
 * @returns {Bool}
 */
function igpu_fence_signal(_fence)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _fence, type: UInt64
    if (!is_numeric(_fence)) show_error($"{_GMFUNCTION_} :: _fence expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _fence);

    var __return_value__ = __igpu_fence_signal(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _fence
 * @returns {Bool}
 */
function igpu_fence_signaled(_fence)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _fence, type: UInt64
    if (!is_numeric(_fence)) show_error($"{_GMFUNCTION_} :: _fence expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _fence);

    var __return_value__ = __igpu_fence_signaled(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _fence
 * @returns {Bool}
 */
function igpu_fence_release(_fence)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _fence, type: UInt64
    if (!is_numeric(_fence)) show_error($"{_GMFUNCTION_} :: _fence expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _fence);

    var __return_value__ = __igpu_fence_release(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _vertex_buffer
 * @param {Real} _layout
 * @param {Real} _primitive
 * @param {Real} _first_vertex
 * @param {Real} _vertex_count
 * @param {Array} _targets
 * @param {Array} _layers
 * @param {Array} _mips
 * @param {Real} _blend_state
 * @param {Real} _depth_state
 * @param {Real} _raster_state
 * @param {Real} _sampler_state
 * @returns {Bool}
 */
function igpu_draw_to_render_targets(_vertex_buffer, _layout, _primitive, _first_vertex, _vertex_count, _targets, _layers, _mips, _blend_state, _depth_state, _raster_state, _sampler_state)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _vertex_buffer, type: UInt64
    if (!is_numeric(_vertex_buffer)) show_error($"{_GMFUNCTION_} :: _vertex_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_buffer);

    // param: _layout, type: UInt64
    if (!is_numeric(_layout)) show_error($"{_GMFUNCTION_} :: _layout expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _layout);

    // param: _primitive, type: Int32
    if (!is_numeric(_primitive)) show_error($"{_GMFUNCTION_} :: _primitive expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _primitive);

    // param: _first_vertex, type: Int64
    if (!is_numeric(_first_vertex)) show_error($"{_GMFUNCTION_} :: _first_vertex expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _first_vertex);

    // param: _vertex_count, type: Int64
    if (!is_numeric(_vertex_count)) show_error($"{_GMFUNCTION_} :: _vertex_count expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_count);

    // param: _targets, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _targets);

    // param: _layers, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _layers);

    // param: _mips, type: AnyArray

    __ext_core_buffer_marshal_value(__args_buffer__, _mips);

    // param: _blend_state, type: Int64
    if (!is_numeric(_blend_state)) show_error($"{_GMFUNCTION_} :: _blend_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _blend_state);

    // param: _depth_state, type: Int64
    if (!is_numeric(_depth_state)) show_error($"{_GMFUNCTION_} :: _depth_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _depth_state);

    // param: _raster_state, type: Int64
    if (!is_numeric(_raster_state)) show_error($"{_GMFUNCTION_} :: _raster_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _raster_state);

    // param: _sampler_state, type: Int64
    if (!is_numeric(_sampler_state)) show_error($"{_GMFUNCTION_} :: _sampler_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _sampler_state);

    var __return_value__ = __igpu_draw_to_render_targets(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _vertex_buffer
 * @param {Real} _layout
 * @param {Real} _primitive
 * @param {Real} _first_vertex
 * @param {Real} _vertex_count
 * @param {Real} _texture
 * @param {Real} _blend_state
 * @param {Real} _depth_state
 * @param {Real} _raster_state
 * @param {Real} _sampler_state
 * @returns {Bool}
 */
function igpu_draw_sampled(_vertex_buffer, _layout, _primitive, _first_vertex, _vertex_count, _texture, _blend_state, _depth_state, _raster_state, _sampler_state)
{
    var __available__ = __IGPU_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _vertex_buffer, type: UInt64
    if (!is_numeric(_vertex_buffer)) show_error($"{_GMFUNCTION_} :: _vertex_buffer expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_buffer);

    // param: _layout, type: UInt64
    if (!is_numeric(_layout)) show_error($"{_GMFUNCTION_} :: _layout expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _layout);

    // param: _primitive, type: Int32
    if (!is_numeric(_primitive)) show_error($"{_GMFUNCTION_} :: _primitive expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _primitive);

    // param: _first_vertex, type: Int64
    if (!is_numeric(_first_vertex)) show_error($"{_GMFUNCTION_} :: _first_vertex expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _first_vertex);

    // param: _vertex_count, type: Int64
    if (!is_numeric(_vertex_count)) show_error($"{_GMFUNCTION_} :: _vertex_count expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _vertex_count);

    // param: _texture, type: UInt64
    if (!is_numeric(_texture)) show_error($"{_GMFUNCTION_} :: _texture expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _texture);

    // param: _blend_state, type: Int64
    if (!is_numeric(_blend_state)) show_error($"{_GMFUNCTION_} :: _blend_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _blend_state);

    // param: _depth_state, type: Int64
    if (!is_numeric(_depth_state)) show_error($"{_GMFUNCTION_} :: _depth_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _depth_state);

    // param: _raster_state, type: Int64
    if (!is_numeric(_raster_state)) show_error($"{_GMFUNCTION_} :: _raster_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _raster_state);

    // param: _sampler_state, type: Int64
    if (!is_numeric(_sampler_state)) show_error($"{_GMFUNCTION_} :: _sampler_state expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _sampler_state);

    var __return_value__ = __igpu_draw_sampled(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/// @ignore
function __IGPU_get_decoders()
{
    static __decoders__ = [
        __IgpuUniformMember_decode,
        __IgpuUniformBlock_decode
    ];
    return __decoders__;
}
/// @ignore
function __IGPU_is_available()
{
    static __available__ = extension_exists("IGPU");
    return __available__;
}
