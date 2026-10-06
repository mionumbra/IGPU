/// @func igpu_init_from_game()
/// @desc Binds IGPU to the graphics device that GameMaker is already using.
///       os_get_info() returns a DS Map; on Windows it contains
///       video_d3d11_device / video_d3d11_context / video_d3d11_swapchain
///       pointers which we forward to the native extension.
///       Call this once after the game window exists (e.g. a Create event).
///       If igpu_device_lost() becomes true, call it again: the previous
///       handles were created on the old device and are no longer valid.
///
///       NOTE: only Windows exposes real device pointers. Xbox returns
///       video_d3d12_* instead, every OpenGL platform returns driver strings
///       only, and HTML5 / Switch return -1 rather than a map at all. This
///       function therefore fails cleanly everywhere except Windows; use
///       igpu_get_capabilities() to branch instead of assuming.
/// @returns {Bool} True if the extension accepted the device.
function igpu_init_from_game()
{
    if (os_type != os_windows)
    {
        show_debug_message("igpu_init_from_game :: IGPU has no native backend on this platform");
        return false;
    }

    var _info = os_get_info();

    // os_get_info() returns -1 (not a DS Map) on HTML5 and Switch. Guard before
    // touching it so those targets degrade instead of erroring.
    if (!is_real(_info) && !ds_map_exists(_info, "video_d3d11_device"))
    {
        // Always release the map: GameMaker does not clear it for us.
        ds_map_destroy(_info);
        show_debug_message("igpu_init_from_game :: os_get_info() has no D3D11 device (non-DX11 target?)");
        return false;
    }

    var _device = _info[? "video_d3d11_device"];
    var _context = _info[? "video_d3d11_context"];
    var _swapchain = _info[? "video_d3d11_swapchain"];

    if (is_undefined(_swapchain))
    {
        _swapchain = 0;
    }

    var _result = igpu_init(_device, _context, _swapchain);

    // os_get_info() maps are never freed automatically - leaking one per call.
    ds_map_destroy(_info);

    return _result;
}

/// @func igpu_log_capabilities()
/// @desc Prints the capability struct. Useful for confirming what a given
///       machine/backend actually supports before writing backend-specific code.
/// @returns {Struct} The capability struct, for callers that want to keep it.
function igpu_log_capabilities()
{
    var _caps = igpu_get_capabilities();

    show_debug_message($"igpu :: backend={_caps.backend} tier={_caps.tier} dialect={_caps.shader_dialect}");
    show_debug_message($"igpu :: device={_caps.device_name}");
    show_debug_message($"igpu :: stages={_caps.shader_stages}");
    show_debug_message($"igpu :: runtime_compile={_caps.runtime_compile} compute={_caps.compute} geometry={_caps.geometry} tess={_caps.tessellation}");
    show_debug_message($"igpu :: texture3d={_caps.texture_3d} array={_caps.texture_array} cubemap={_caps.texture_cubemap} uav={_caps.uav} mrt={_caps.max_render_targets}");
    show_debug_message($"igpu :: rgba8={_caps.formats.surface_rgba8unorm} rgba4={_caps.formats.surface_rgba4unorm} r8={_caps.formats.surface_r8unorm}");

    return _caps;
}

/// @func igpu_compile_shader_from_file(_path, _stage, _entry)
/// @desc Reads an HLSL file from disk and compiles it at runtime.
///       The shader stage is backend-neutral; no profile string is passed -
///       the backend picks the right one (see igpu_shader_compile).
/// @param {String} _path   Path to the .hlsl file (game-relative or absolute).
/// @param {Real}   _stage  IgpuShaderStage constant.
/// @param {String} _entry  Entry point name inside the shader.
/// @returns {Real} Shader handle, or 0 on failure.
function igpu_compile_shader_from_file(_path, _stage, _entry)
{
    if (!file_exists(_path))
    {
        show_debug_message($"igpu_compile_shader_from_file :: file not found: {_path}");
        return 0;
    }

    var _buffer = buffer_load(_path);
    var _source = buffer_read(_buffer, buffer_text);
    buffer_delete(_buffer);

    return igpu_shader_compile(_source, _entry, _stage, "");
}

/// @func igpu_vertex_format(_shader, _elements)
/// @desc Builds an input layout for a compiled vertex shader from a compact
///       element list, so callers do not have to maintain two parallel arrays.
///
///       Each entry is [usage, type] using GameMaker's own constants, e.g.
///           igpu_vertex_format(_vs, [
///               [vertex_usage_position, vertex_type_float3],
///               [vertex_usage_texcoord, vertex_type_float2]
///           ]);
///       which mirrors vertex_format_begin() + vertex_format_add_position_3d()
///       and friends.
///
///       The vertex stride is derived automatically from the element sizes
///       (tightly packed). Pass _stride explicitly if your data is interleaved
///       with padding.
/// @param {Real}   _shader  Vertex shader handle from igpu_shader_compile.
/// @param {Array}  _elements  Array of [usage, type] pairs.
/// @param {Real}   _stride  Optional byte stride; -1 (default) means auto.
/// @returns {Real} Layout handle, or 0 on failure.
function igpu_vertex_format(_shader, _elements, _stride = -1)
{
    var _count = array_length(_elements);
    if (_count <= 0)
    {
        show_debug_message("igpu_vertex_format :: element list is empty");
        return 0;
    }

    var _usage = array_create(_count, 0);
    var _type = array_create(_count, 0);

    for (var _i = 0; _i < _count; _i++)
    {
        var _element = _elements[_i];
        if (!is_array(_element) || array_length(_element) < 2)
        {
            show_debug_message($"igpu_vertex_format :: element {_i} is not a [usage, type] pair");
            return 0;
        }

        _usage[_i] = _element[0];
        _type[_i] = _element[1];
    }

    var _step = array_create(_count, IgpuVertexStep.Vertex);
    return igpu_input_layout_create(_shader, _usage, _type, _step, _count, _stride, 0);
}

/// @func igpu_buffer_upload(_buffer, _data, _offset)
/// @desc Uploads a GameMaker buffer into a GPU buffer.
///       This is a thin wrapper over igpu_buffer_write() that exists to make
///       the argument order read naturally; the native function is what
///       carries the bounds checking.
/// @param {Real} _buffer  IGPU buffer handle.
/// @param {Id.Buffer} _data  GameMaker buffer holding the bytes to upload.
/// @param {Real} _offset  Byte offset in the GPU buffer; 0 by default.
/// @returns {Bool} True on success.
function igpu_buffer_upload(_buffer, _data, _offset = 0)
{
    var _ok = igpu_buffer_write(_buffer, _offset, _data);
    if (!_ok)
    {
        show_debug_message($"igpu_buffer_upload :: {igpu_get_last_error()}");
    }
    return _ok;
}

/// @func igpu_buffer_create_from_array(_values, _usage, _bind)
/// @desc Builds a GPU buffer directly from a GML array of reals, packing each
///       element as a 32-bit float.
///
///       This is the shortest path for feeding computed data (positions,
///       indices, constants) to the GPU without hand-managing a GameMaker
///       buffer and its fifo/text position.
///
///       NOTE: every value becomes one 4-byte float, so this suits numeric
///       arrays only. For interleaved vertex structs, fill a buffer yourself
///       and use igpu_buffer_upload() so the layout matches your format.
/// @param {Array} _values  Array of reals.
/// @param {Real}  _usage   IgpuBufferUsage constant.
/// @param {Real}  _bind    IgpuBufferBind constant (or OR of several).
/// @param {Real}  _stride  Bytes per vertex; only for a Vertex buffer, where
///                         it must be a positive multiple of 4 (this helper
///                         packs each value as one float). 0 otherwise.
/// @returns {Real} Buffer handle, or 0 on failure.
function igpu_buffer_create_from_array(_values, _usage, _bind, _stride = 0)
{
    var _count = array_length(_values);
    if (_count <= 0)
    {
        show_debug_message("igpu_buffer_create_from_array :: array is empty");
        return 0;
    }

    var _handle = igpu_buffer_create(_count * 4, _usage, _bind, _stride);
    if (_handle == 0)
    {
        show_debug_message($"igpu_buffer_create_from_array :: {igpu_get_last_error()}");
        return 0;
    }

    // buffer_fixed + buffer_fast keeps the writes tightly packed with no
    // alignment padding, which is exactly what the GPU expects.
    var _staging = buffer_create(_count * 4, buffer_fixed, 4);
    buffer_seek(_staging, buffer_seek_start, 0);

    for (var _i = 0; _i < _count; _i++)
    {
        buffer_write(_staging, buffer_f32, _values[_i]);
    }

    var _ok = igpu_buffer_write(_handle, 0, _staging);
    buffer_delete(_staging);

    if (!_ok)
    {
        show_debug_message($"igpu_buffer_create_from_array :: {igpu_get_last_error()}");
        igpu_buffer_release(_handle);
        return 0;
    }

    return _handle;
}

/// @func igpu_draw_buffer(_buffer, _layout, _primitive, _first, _count)
/// @desc Draws a vertex buffer, logging the reason if the draw is refused.
///       A thin wrapper over igpu_draw() that makes failures visible; the
///       state save/restore happens inside the native call and needs no
///       matching begin/end from the caller.
/// @param {Real} _buffer     Vertex buffer handle (with IgpuBufferBind.Vertex).
/// @param {Real} _layout     Input layout handle.
/// @param {Real} _primitive  pr_* constant.
/// @param {Real} _first      First vertex; 0 by default.
/// @param {Real} _count      Vertex count; -1 (default) means to the end.
/// @returns {Bool} True if the draw was issued.
function igpu_draw_buffer(_buffer, _layout, _primitive, _first = 0, _count = -1)
{
    var _ok = igpu_draw(_buffer, 0, _layout, _primitive, _first, _count, 1, 0, 0, 0, 0);
    if (!_ok)
    {
        show_debug_message($"igpu_draw_buffer :: {igpu_get_last_error()}");
    }
    return _ok;
}
