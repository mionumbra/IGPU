/// obj_igpu_test :: Create
/// Verifies that IGPU can borrow GameMaker's D3D11 device and compile HLSL.

_igpu_shader = 0;
_igpu_status = "not run";

show_debug_message("========================================");
show_debug_message("IGPU integration test");
show_debug_message("========================================");

show_debug_message("extension_exists : " + string(extension_exists("IGPU")));
show_debug_message("igpu_version     : " + string(igpu_version()));

var _info = os_get_info();
show_debug_message("os_get_info type : " + typeof(_info));
show_debug_message("has device key   : " + string(ds_map_exists(_info, "video_d3d11_device")));

_igpu_ok = igpu_init_from_game();
show_debug_message("igpu_init result : " + string(_igpu_ok));

if (!_igpu_ok)
{
    _igpu_status = "FAIL - igpu_init_from_game";
    show_debug_message(_igpu_status);
    exit;
}

show_debug_message("is_available     : " + string(igpu_is_available()));
show_debug_message("feature level    : " + string(igpu_get_feature_level()));
show_debug_message("adapter          : " + igpu_get_adapter_description());
show_debug_message("video memory     : " + string(igpu_get_video_memory()));
show_debug_message("backbuffer       : " + string(igpu_get_backbuffer_width()) + " x " + string(igpu_get_backbuffer_height()));

var _bad = igpu_shader_compile_vertex("struct Broken {", "main", "vs_5_0");
show_debug_message("bad shader handle: " + string(_bad));
show_debug_message("bad shader error : " + string(igpu_get_last_error()));

var _good = "struct VSInput { float3 pos : POSITION; float2 uv : TEXCOORD0; }; struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; VSOutput main(VSInput input) { VSOutput output; output.pos = float4(input.pos, 1.0); output.uv = input.uv; return output; }";
_igpu_shader = igpu_shader_compile_vertex(_good, "main", "vs_5_0");
show_debug_message("good shader handle: " + string(_igpu_shader));

if (_igpu_shader == 0)
{
    _igpu_status = "FAIL - shader compile";
    show_debug_message("good shader error : " + string(igpu_get_last_error()));
    exit;
}

_igpu_status = "PASS";
show_debug_message(_igpu_status);
show_debug_message("========================================");
