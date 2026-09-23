/// @func igpu_init_from_game()
/// @desc Binds IGPU to the D3D11 device that GameMaker is already using.
///       os_get_info() returns a DS Map; on Windows it contains
///       video_d3d11_device / video_d3d11_context / video_d3d11_swapchain
///       pointers which we forward to the native extension.
///       Call this once after the game window exists (e.g. a Create event).
/// @returns {Bool} True if the extension accepted the device.
function igpu_init_from_game()
{
    if (os_type != os_windows)
    {
        show_debug_message("igpu_init_from_game :: IGPU currently only supports Windows");
        return false;
    }

    var _info = os_get_info();
    if (!ds_map_exists(_info, "video_d3d11_device"))
    {
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

    show_debug_message($"igpu_init_from_game :: device={_device} context={_context} swapchain={_swapchain}");

    return igpu_init(_device, _context, _swapchain);
}

/// @func igpu_compile_shader_from_file(_path, _stage, _entry, _target)
/// @desc Reads an HLSL file from disk and compiles it at runtime.
/// @param {String} _path   Path to the .hlsl file (game-relative or absolute).
/// @param {Real}   _stage  IgpuShaderStage constant.
/// @param {String} _entry  Entry point name inside the shader.
/// @param {String} _target Shader profile, e.g. "ps_5_0". Pass "" to auto-select.
/// @returns {Real} Shader handle, or 0 on failure.
function igpu_compile_shader_from_file(_path, _stage, _entry, _target)
{
    if (!file_exists(_path))
    {
        show_debug_message($"igpu_compile_shader_from_file :: file not found: {_path}");
        return 0;
    }

    var _buffer = buffer_load(_path);
    var _source = buffer_read(_buffer, buffer_text);
    buffer_delete(_buffer);

    switch (_stage)
    {
        case IgpuShaderStage.Vertex:
            return igpu_shader_compile_vertex(_source, _entry, _target);

        case IgpuShaderStage.Pixel:
            return igpu_shader_compile_pixel(_source, _entry, _target);

        case IgpuShaderStage.Compute:
            return igpu_shader_compile_compute(_source, _entry, _target);
    }

    show_debug_message($"igpu_compile_shader_from_file :: unknown stage {_stage}");
    return 0;
}
