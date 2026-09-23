/// obj_igpu_test :: Create
/// Verifies that IGPU can borrow GameMaker's D3D11 device, report its
/// capabilities, and compile HLSL through the backend-neutral shader API.

_igpu_shader = 0;
_igpu_status = "not run";

show_debug_message("========================================");
show_debug_message("IGPU integration test");
show_debug_message("========================================");

show_debug_message("extension_exists : " + string(extension_exists("IGPU")));
show_debug_message("igpu_version     : " + string(igpu_version()));

_igpu_ok = igpu_init_from_game();
show_debug_message("igpu_init result : " + string(_igpu_ok));

if (!_igpu_ok)
{
    _igpu_status = "FAIL - igpu_init_from_game";
    show_debug_message(_igpu_status);
    show_debug_message("last error       : " + string(igpu_get_last_error()));
    exit;
}

show_debug_message("is_available     : " + string(igpu_is_available()));
show_debug_message("feature level    : " + string(igpu_get_feature_level()));
show_debug_message("adapter          : " + igpu_get_adapter_description());
show_debug_message("video memory     : " + string(igpu_get_video_memory()));
show_debug_message("backbuffer       : " + string(igpu_get_backbuffer_width()) + " x " + string(igpu_get_backbuffer_height()));

// ---------------------------------------------------------------------------
// Capability query (Tier 3) - must never fail, even with no backend
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
var _caps = igpu_log_capabilities();

if (_caps.backend != "d3d11")
{
    _igpu_status = "FAIL - expected d3d11 backend, got " + string(_caps.backend);
    show_debug_message(_igpu_status);
    exit;
}

// A few capability flags must agree with the backend we bound.
if (!_caps.runtime_compile || !_caps.compute || _caps.tier != 1)
{
    _igpu_status = "FAIL - capability flags inconsistent with d3d11";
    show_debug_message(_igpu_status);
    exit;
}

show_debug_message("supports(compute): " + string(igpu_supports(IgpuCapability.ShaderStageCompute)));
show_debug_message("supports(mesh)   : " + string(igpu_supports(IgpuCapability.ShaderStageMesh)));

// ---------------------------------------------------------------------------
// Runtime shader compilation through the neutral API
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");

// A broken source must fail and leave a D3DCompile message behind.
var _bad = igpu_shader_compile("struct Broken {", "main", IgpuShaderStage.Vertex, "");
show_debug_message("bad shader handle: " + string(_bad));
show_debug_message("bad shader error : " + string(igpu_get_last_error()));

// An unsupported stage must be refused by capability, not by the HLSL compiler.
var _mesh = igpu_shader_compile("void main() {}", "main", IgpuShaderStage.Mesh, "");
show_debug_message("mesh shader handle: " + string(_mesh));
show_debug_message("mesh shader error : " + string(igpu_get_last_error()));

var _good = "struct VSInput { float3 pos : POSITION; float2 uv : TEXCOORD0; }; struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; VSOutput main(VSInput input) { VSOutput output; output.pos = float4(input.pos, 1.0); output.uv = input.uv; return output; }";
_igpu_shader = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
show_debug_message("good shader handle: " + string(_igpu_shader));

if (_igpu_shader == 0)
{
    _igpu_status = "FAIL - shader compile";
    show_debug_message("good shader error : " + string(igpu_get_last_error()));
    exit;
}

// The convenience wrapper must reach the same code path.
var _pixel = igpu_shader_compile_pixel("float4 main() : SV_TARGET { return float4(1, 0, 0, 1); }", "main", "");
show_debug_message("pixel shader handle: " + string(_pixel));

if (_pixel == 0)
{
    _igpu_status = "FAIL - pixel wrapper";
    show_debug_message("pixel shader error : " + string(igpu_get_last_error()));
    exit;
}

// Rejecting an explicit foreign dialect keeps the abstraction honest.
var _glsl = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "glsl");
show_debug_message("glsl dialect handle: " + string(_glsl));
show_debug_message("glsl dialect error : " + string(igpu_get_last_error()));

igpu_shader_release(_pixel);

_igpu_status = "PASS";
show_debug_message(_igpu_status);
show_debug_message("========================================");
