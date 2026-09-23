/// @func igpu_init_from_game()
/// @desc Binds IGPU to the graphics device that GameMaker is already using.
///       os_get_info() returns a DS Map; on Windows it contains
///       video_d3d11_device / video_d3d11_context / video_d3d11_swapchain
///       pointers which we forward to the native extension.
///       Call this once after the game window exists (e.g. a Create event).
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
