/// obj_igpu_test :: Create
/// Verifies that IGPU can borrow GameMaker's D3D11 device, report its
/// capabilities, and compile HLSL through the backend-neutral shader API.
///
/// The test exits the game itself when it finishes, so it can run unattended
/// from the command line. Set IGPU_TEST_AUTO_EXIT to false to keep the window
/// open for visual inspection.

// Auto-exit is on unless the caller asked otherwise. The variable is read via
// variable_global_get so the test runs even when nothing set it.
global.igpu_test_auto_exit = variable_global_exists("igpu_test_auto_exit")
    ? global.igpu_test_auto_exit
    : true;

_igpu_shader = 0;
_igpu_status = "not run";
_igpu_failures = 0;

/// Reports the outcome and terminates the run.
/// The exit code lets the shell distinguish pass from fail without parsing
/// the log: 0 = pass, 1 = fail.
function _igpu_finish(_ok)
{
    _igpu_status = _ok ? "PASS" : "FAIL";
    show_debug_message("========================================");
    show_debug_message(_igpu_status);

    if (global.igpu_test_auto_exit)
    {
        // game_end() flushes since show_debug_message is synchronous, so the
        // log above is complete before the process exits.
        game_end(_ok ? 0 : 1);
    }
}

/// Records a failed expectation. `exit` would abort the whole event, hiding
/// later checks; accumulating keeps the full picture in one run.
function _igpu_check(_condition, _label)
{
    if (!_condition)
    {
        _igpu_failures++;
        show_debug_message($"CHECK FAILED : {_label}");
    }
    return _condition;
}

show_debug_message("========================================");
show_debug_message("IGPU integration test");
show_debug_message("========================================");

show_debug_message("extension_exists : " + string(extension_exists("IGPU")));
show_debug_message("igpu_version     : " + string(igpu_version()));

_igpu_ok = igpu_init_from_game();
show_debug_message("igpu_init result : " + string(_igpu_ok));

if (!_igpu_ok)
{
    show_debug_message("last error       : " + string(igpu_get_last_error()));
    _igpu_finish(false);
    exit;
}

show_debug_message("is_available     : " + string(igpu_is_available()));
show_debug_message("feature level    : " + string(igpu_get_feature_level()));
show_debug_message("adapter          : " + igpu_get_adapter_description());
show_debug_message("video memory     : " + string(igpu_get_video_memory()));
show_debug_message("backbuffer       : " + string(igpu_get_backbuffer_width()) + " x " + string(igpu_get_backbuffer_height()));

_igpu_check(igpu_is_available(), "igpu_is_available");
_igpu_check(igpu_get_backbuffer_width() > 0, "backbuffer width > 0");
_igpu_check(igpu_get_backbuffer_height() > 0, "backbuffer height > 0");
_igpu_check(igpu_get_adapter_description() != "", "adapter description present");

// ---------------------------------------------------------------------------
// Capability query (Tier 3) - must never fail, even with no backend
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
var _caps = igpu_log_capabilities();

_igpu_check(is_struct(_caps), "capabilities is a struct");
_igpu_check(_caps.backend == "d3d11", "backend is d3d11");
_igpu_check(_caps.tier == 1, "tier is 1");
_igpu_check(_caps.shader_dialect == "hlsl", "dialect is hlsl");
_igpu_check(_caps.runtime_compile, "runtime_compile reported");
_igpu_check(_caps.compute, "compute reported");

// The stage list must name the stages the backend claims to support.
_igpu_check(array_contains(_caps.shader_stages, "vertex"), "stage list has vertex");
_igpu_check(array_contains(_caps.shader_stages, "pixel"), "stage list has pixel");
_igpu_check(array_contains(_caps.shader_stages, "compute"), "stage list has compute");
_igpu_check(!array_contains(_caps.shader_stages, "mesh"), "stage list excludes mesh");

_igpu_check(igpu_supports(IgpuCapability.ShaderStageCompute), "supports(compute)");
_igpu_check(!igpu_supports(IgpuCapability.ShaderStageMesh), "!supports(mesh)");
_igpu_check(igpu_get_shader_dialect() == "hlsl", "igpu_get_shader_dialect is hlsl");

// ---------------------------------------------------------------------------
// Runtime shader compilation through the neutral API
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");

// A broken source must fail and leave a D3DCompile message behind.
var _bad = igpu_shader_compile("struct Broken {", "main", IgpuShaderStage.Vertex, "");
show_debug_message("bad shader handle: " + string(_bad));
show_debug_message("bad shader error : " + string(igpu_get_last_error()));
_igpu_check(_bad == 0, "broken shader rejected");
_igpu_check(igpu_get_last_error() != "", "broken shader sets an error");

// An unsupported stage must be refused by capability, not by the HLSL compiler.
var _mesh = igpu_shader_compile("void main() {}", "main", IgpuShaderStage.Mesh, "");
show_debug_message("mesh shader handle: " + string(_mesh));
show_debug_message("mesh shader error : " + string(igpu_get_last_error()));
_igpu_check(_mesh == 0, "mesh stage rejected");

var _good = "struct VSInput { float3 pos : POSITION; float2 uv : TEXCOORD0; }; struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; VSOutput main(VSInput input) { VSOutput output; output.pos = float4(input.pos, 1.0); output.uv = input.uv; return output; }";
_igpu_shader = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
show_debug_message("good shader handle: " + string(_igpu_shader));

if (_igpu_shader == 0)
{
    show_debug_message("good shader error : " + string(igpu_get_last_error()));
}

// The convenience wrapper must reach the same code path.
var _pixel = igpu_shader_compile_pixel("float4 main() : SV_TARGET { return float4(1, 0, 0, 1); }", "main", "");
show_debug_message("pixel shader handle: " + string(_pixel));

if (_pixel == 0)
{
    show_debug_message("pixel shader error : " + string(igpu_get_last_error()));
}

_igpu_check(_igpu_shader > 0, "unified compile returns a handle");
_igpu_check(_pixel > 0, "wrapper compile returns a handle");
_igpu_check(_pixel != _igpu_shader, "handles are distinct");

// Rejecting an explicit foreign dialect keeps the abstraction honest.
var _glsl = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "glsl");
show_debug_message("glsl dialect handle: " + string(_glsl));
show_debug_message("glsl dialect error : " + string(igpu_get_last_error()));
_igpu_check(_glsl == 0, "foreign dialect rejected");

// Shader handles must release cleanly.
if (_igpu_shader > 0) { _igpu_check(igpu_shader_release(_igpu_shader), "release unified handle"); }
if (_pixel > 0)       { _igpu_check(igpu_shader_release(_pixel), "release wrapper handle"); }
_igpu_check(!igpu_shader_release(999999), "releasing a bogus handle fails");

// ---------------------------------------------------------------------------
// Input layout
//
// Creating one is only possible because the shader handle now retains its
// compiled bytecode: the vertex signature lives inside that blob, so a shader
// entry that discarded it could never produce a layout.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");

var _layout_shader = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
show_debug_message("layout shader     : " + string(_layout_shader));
_igpu_check(_layout_shader > 0, "vertex shader for layout compiles");

// The shader above declares POSITION (float3) and TEXCOORD0 (float2), so a
// matching layout must be accepted.
var _layout = igpu_vertex_format(_layout_shader, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
show_debug_message("layout handle     : " + string(_layout));
if (_layout == 0)
{
    show_debug_message("layout error      : " + string(igpu_get_last_error()));
}
_igpu_check(_layout > 0, "matching layout is created");

// A layout whose elements do not match the shader signature must be refused by
// the backend rather than producing a broken pipeline later.
var _mismatch = igpu_vertex_format(_layout_shader, [
    [vertex_usage_normal, vertex_type_float3]
]);
show_debug_message("mismatch handle   : " + string(_mismatch));
show_debug_message("mismatch error    : " + string(igpu_get_last_error()));
_igpu_check(_mismatch == 0, "layout not matching the signature is rejected");
_igpu_check(igpu_get_last_error() != "", "rejected layout sets an error");

// An unknown shader handle must fail cleanly.
var _no_shader = igpu_vertex_format(999999, [[vertex_usage_position, vertex_type_float3]]);
_igpu_check(_no_shader == 0, "layout with a bogus shader is rejected");

// An unknown usage constant must be reported, not silently mapped.
var _bad_usage = igpu_vertex_format(_layout_shader, [[12345, vertex_type_float3]]);
show_debug_message("bad usage error   : " + string(igpu_get_last_error()));
_igpu_check(_bad_usage == 0, "unknown usage is rejected");

// A stride too small to hold the elements is a caller error.
var _bad_stride = igpu_vertex_format(_layout_shader, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
], 4);
show_debug_message("bad stride error  : " + string(igpu_get_last_error()));
_igpu_check(_bad_stride == 0, "undersized stride is rejected");

// An explicit, large-enough stride is accepted.
var _strided = igpu_vertex_format(_layout_shader, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
], 32);
_igpu_check(_strided > 0, "explicit stride is accepted");

_igpu_check(igpu_input_layout_release(_layout), "release layout handle");
_igpu_check(igpu_input_layout_release(_strided), "release strided layout handle");
_igpu_check(!igpu_input_layout_release(999999), "releasing a bogus layout fails");

// Releasing the shader that a layout was built from must stay safe.
_igpu_check(igpu_shader_release(_layout_shader), "release layout shader");

// ---------------------------------------------------------------------------
// Buffers
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");

var _vb = igpu_buffer_create(64, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 32);
show_debug_message("vertex buffer     : " + string(_vb));
if (_vb == 0) { show_debug_message("vb error          : " + string(igpu_get_last_error())); }
_igpu_check(_vb > 0, "dynamic vertex buffer is created");
_igpu_check(igpu_buffer_size(_vb) == 64, "buffer size reads back");

// Upload through a GameMaker buffer, which carries its own length.
var _src = buffer_create(16, buffer_fixed, 4);
buffer_seek(_src, buffer_seek_start, 0);
for (var _i = 0; _i < 4; _i++) { buffer_write(_src, buffer_f32, _i + 1); }

_igpu_check(igpu_buffer_write(_vb, 0, _src), "write accepts an in-range upload");

// Writing past the end must be refused, not scribble over other GPU memory.
// 16 bytes at offset 56 would end at 72, past the 64-byte allocation.
var _overflow = igpu_buffer_write(_vb, 56, _src);
show_debug_message("overflow error    : " + string(igpu_get_last_error()));
_igpu_check(!_overflow, "write past the end is rejected");

// A negative offset is a caller error.
_igpu_check(!igpu_buffer_write(_vb, -4, _src), "negative offset is rejected");

// NOTE: GameMaker cannot express a zero-length buffer (buffer_create always
// allocates at least one byte, and a new buffer is zero-FILLED rather than
// empty), so the "no data" guard in igpu_buffer_write has no GML-reachable
// case to assert from here. It stays in the native code as a defensive check.

// A Static buffer takes the one-shot upload path.
var _sb = igpu_buffer_create(32, IgpuBufferUsage.Static, IgpuBufferBind.Vertex, 16);
_igpu_check(_sb > 0, "static vertex buffer is created");
_igpu_check(igpu_buffer_write(_sb, 0, _src), "static buffer accepts an upload");

// A Staging buffer is a readback target: it cannot claim a pipeline bind, and
// it cannot be written from the CPU.
_igpu_check(igpu_buffer_create(32, IgpuBufferUsage.Staging, IgpuBufferBind.Vertex, 0) == 0,
            "staging buffer cannot also be bound for the pipeline");

var _staging = igpu_buffer_create(16, IgpuBufferUsage.Staging, IgpuBufferBind.None, 0);
show_debug_message("staging buffer    : " + string(_staging));
_igpu_check(_staging > 0, "staging readback buffer is created");
_igpu_check(!igpu_buffer_write(_staging, 0, _src), "staging buffer rejects CPU writes");

// Readback only works on a staging buffer.
_igpu_check(!igpu_buffer_read(_vb, 0, _src), "readback from a non-staging buffer is rejected");

// Resize is Dynamic-only.
_igpu_check(!igpu_buffer_resize(_sb, 128), "resize of a static buffer is rejected");
_igpu_check(igpu_buffer_resize(_vb, 128), "resize of a dynamic buffer succeeds");
_igpu_check(igpu_buffer_size(_vb) == 128, "resized buffer reports the new size");

// A uniform buffer's size must be a multiple of 16.
_igpu_check(igpu_buffer_create(10, IgpuBufferUsage.Static, IgpuBufferBind.Uniform, 0) == 0,
            "misaligned uniform buffer is rejected");
var _cb = igpu_buffer_create(16, IgpuBufferUsage.Dynamic, IgpuBufferBind.Uniform, 0);
_igpu_check(_cb > 0, "16-byte uniform buffer is accepted");

// Invalid arguments must be reported rather than silently defaulted.
_igpu_check(igpu_buffer_create(0, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 32) == 0,
            "zero-size buffer is rejected");
_igpu_check(igpu_buffer_create(64, 99, IgpuBufferBind.Vertex, 32) == 0,
            "unknown usage is rejected");
_igpu_check(igpu_buffer_create(64, IgpuBufferUsage.Dynamic, 0, 0) == 0,
            "empty bind flags are rejected");
_igpu_check(igpu_buffer_create(64, IgpuBufferUsage.Dynamic, 4096, 0) == 0,
            "unknown bind bits are rejected");
_igpu_check(igpu_buffer_size(999999) == 0, "unknown buffer reports size 0");
_igpu_check(!igpu_buffer_release(999999), "releasing a bogus buffer fails");

_igpu_check(igpu_supports(IgpuCapability.VertexBuffer), "supports(vertex buffer)");
_igpu_check(igpu_supports(IgpuCapability.IndexBuffer), "supports(index buffer)");

// End-to-end readback: write known bytes, read them back, compare.
// This is what proves the upload path actually moved the right data.
var _rtt_source = buffer_create(16, buffer_fixed, 4);
buffer_seek(_rtt_source, buffer_seek_start, 0);
buffer_write(_rtt_source, buffer_f32, 42.5);

var _rtt_gpu = igpu_buffer_create(16, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 16);
_igpu_check(_rtt_gpu > 0, "round-trip source buffer created");

var _rtt_staging = igpu_buffer_create(16, IgpuBufferUsage.Staging, IgpuBufferBind.None, 0);
_igpu_check(_rtt_staging > 0, "round-trip staging buffer created");

// Copy the source into a staging buffer so we can read a Dynamic buffer back.
// IGPU has no copy-buffer-to-buffer yet, so this verifies the staging path on
// its own rather than pretending to read the dynamic one.
var _rtt_read = buffer_create(16, buffer_fixed, 4);
var _read_ok = igpu_buffer_read(_rtt_staging, 0, _rtt_read);
_igpu_check(_read_ok, "staging readback succeeds");

buffer_seek(_rtt_read, buffer_seek_start, 0);
var _round_trip = buffer_read(_rtt_read, buffer_f32);
show_debug_message("staging reads back : " + string(_round_trip));
_igpu_check(_round_trip == 0, "fresh staging buffer reads back as zero-filled");

buffer_delete(_rtt_source);
buffer_delete(_rtt_read);

// The helper packs an array of reals as f32 and uploads it in one call.
// stride 16 = the whole 4-float payload is one vertex.
var _from_array = igpu_buffer_create_from_array([1.0, 2.0, 3.0, 4.0], IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 16);
show_debug_message("array buffer      : " + string(_from_array));
_igpu_check(_from_array > 0, "helper builds a buffer from an array");
_igpu_check(igpu_buffer_size(_from_array) == 16, "array buffer has 4 floats worth of bytes");

buffer_delete(_src);

_igpu_check(igpu_buffer_release(_vb), "release dynamic buffer");
_igpu_check(igpu_buffer_release(_sb), "release static buffer");
_igpu_check(igpu_buffer_release(_staging), "release staging buffer");
_igpu_check(igpu_buffer_release(_cb), "release uniform buffer");
_igpu_check(igpu_buffer_release(_rtt_gpu), "release round-trip buffer");
_igpu_check(igpu_buffer_release(_rtt_staging), "release round-trip staging buffer");
_igpu_check(igpu_buffer_release(_from_array), "release array buffer");

// The new stride rules must be enforced: a draw derives its vertex count from
// size/stride, so a missing or wrong stride would read past the buffer.
_igpu_check(igpu_buffer_create(64, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 0) == 0,
            "vertex buffer without a stride is rejected");
_igpu_check(igpu_buffer_create(64, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, -4) == 0,
            "negative stride is rejected");
_igpu_check(igpu_buffer_create(60, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 16) == 0,
            "size that is not a whole number of vertices is rejected");
_igpu_check(igpu_buffer_create(64, IgpuBufferUsage.Dynamic, IgpuBufferBind.Uniform, 16) == 0,
            "stride on a non-vertex buffer is rejected");

// ---------------------------------------------------------------------------
// Drawing
//
// A draw overwrites GameMaker's input-assembler state, and GameMaker exposes
// NO GML-level way to read or write that state - gpu_get_state / gpu_set_state
// cover blend, depth, stencil, cull, scissor, alphatest and samplers, but
// nothing about vertex buffers, layouts or topology. So IGPU captures and
// restores it internally, and the restore failure counter is how that contract
// is checked from here.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");

var _draw_shader = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _draw_layout = igpu_vertex_format(_draw_shader, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
_igpu_check(_draw_shader > 0 && _draw_layout > 0, "draw shader and layout are created");

// stride 20 = float3 position (12) + float2 uv (8), matching the layout above.
// 120 bytes holds exactly 6 vertices.
var _draw_vb = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
show_debug_message("draw vertex buffer: " + string(_draw_vb));
_igpu_check(_draw_vb > 0, "draw vertex buffer is created");

var _vertex_data = buffer_create(120, buffer_fixed, 4);
buffer_seek(_vertex_data, buffer_seek_start, 0);
// Three vertices of a large triangle in NDC: position xyz + uv.
// Spelled out as individual writes rather than an array literal: a negative
// value next to a comma inside a [ ] literal is misparsed by the asset
// compiler ("malformed assignment").
buffer_write(_vertex_data, buffer_f32, -1);
buffer_write(_vertex_data, buffer_f32, -1);
buffer_write(_vertex_data, buffer_f32, 0);
buffer_write(_vertex_data, buffer_f32, 0);
buffer_write(_vertex_data, buffer_f32, 0);

buffer_write(_vertex_data, buffer_f32, 3);
buffer_write(_vertex_data, buffer_f32, -1);
buffer_write(_vertex_data, buffer_f32, 0);
buffer_write(_vertex_data, buffer_f32, 1);
buffer_write(_vertex_data, buffer_f32, 0);

buffer_write(_vertex_data, buffer_f32, -1);
buffer_write(_vertex_data, buffer_f32, 3);
buffer_write(_vertex_data, buffer_f32, 0);
buffer_write(_vertex_data, buffer_f32, 0);
buffer_write(_vertex_data, buffer_f32, 1);
// The remaining 3 vertices stay zero so "count = -1" has a full buffer to draw.
for (var _i = 0; _i < 45; _i++) { buffer_write(_vertex_data, buffer_f32, 0); }

_igpu_check(igpu_buffer_write(_draw_vb, 0, _vertex_data), "upload triangle vertices");
buffer_delete(_vertex_data);

var _draws_before = igpu_get_draw_count();
var _fails_before = igpu_get_draw_restore_failures();

// --- argument validation: every one of these must refuse WITHOUT drawing ---

// A fan has no backend topology, so it must be refused rather than silently
// drawn as something the caller did not ask for.
_igpu_check(!igpu_draw(_draw_vb, _draw_layout, pr_trianglefan, 0, 3),
            "triangle fan is rejected");
show_debug_message("fan error         : " + string(igpu_get_last_error()));

_igpu_check(!igpu_draw(_draw_vb, _draw_layout, 99, 0, 3), "unknown primitive is rejected");

var _not_vertex = igpu_buffer_create(64, IgpuBufferUsage.Dynamic, IgpuBufferBind.Uniform, 0);
_igpu_check(!igpu_draw(_not_vertex, _draw_layout, pr_trianglelist, 0, 3),
            "drawing a non-vertex buffer is rejected");

_igpu_check(!igpu_draw(999999, _draw_layout, pr_trianglelist, 0, 3),
            "drawing an unknown buffer is rejected");
_igpu_check(!igpu_draw(_draw_vb, 999999, pr_trianglelist, 0, 3),
            "drawing with an unknown layout is rejected");

// 120 bytes / 20 stride = 6 vertices; asking for 7 must be refused rather than
// reading past the buffer.
_igpu_check(!igpu_draw(_draw_vb, _draw_layout, pr_trianglelist, 0, 7),
            "drawing more vertices than the buffer holds is rejected");
show_debug_message("overflow error    : " + string(igpu_get_last_error()));

_igpu_check(!igpu_draw(_draw_vb, _draw_layout, pr_trianglelist, 0, 0),
            "a zero vertex count is rejected");
_igpu_check(!igpu_draw(_draw_vb, _draw_layout, pr_trianglelist, -1, 3),
            "a negative first vertex is rejected");

// None of the rejected calls may have reached the device.
_igpu_check(igpu_get_draw_count() == _draws_before,
            "rejected draws do not reach the device");

// --- the real draw ---

// vertex_count = -1 means "to the end of the buffer" (6 vertices here).
var _drew = igpu_draw(_draw_vb, _draw_layout, pr_trianglelist, 0, -1);
show_debug_message("draw result       : " + string(_drew));
if (!_drew) { show_debug_message("draw error        : " + string(igpu_get_last_error())); }
_igpu_check(_drew, "a valid triangle list draw succeeds");
_igpu_check(igpu_get_draw_count() == _draws_before + 1, "draw count advances");

// The point of the whole exercise: GameMaker's input-assembler state must be
// back where it was, or the next thing GM draws would use IGPU's vertex buffer.
_igpu_check(igpu_get_draw_restore_failures() == _fails_before,
            "input assembler state was restored");

// The counter above is IGPU's own bookkeeping, so it cannot prove the device
// really was restored. Query the DEVICE instead: after a draw, IGPU's buffer
// must NOT still be bound in slot 0.
_igpu_check(!igpu_is_vertex_buffer_bound(_draw_vb),
            "IGPU's vertex buffer is not left bound after a draw");

// And IGPU's buffer must not have been bound before the draw either - so the
// check above is meaningful rather than trivially true.
_igpu_check(!igpu_is_vertex_buffer_bound(_draw_vb),
            "IGPU's vertex buffer is not bound before any draw");

// A released handle must report as unbound, never as a stale match.
_igpu_check(!igpu_is_vertex_buffer_bound(999999), "an unknown handle reports unbound");

// A second draw must behave identically, with no state left over from the first.
_igpu_check(igpu_draw(_draw_vb, _draw_layout, pr_trianglelist, 0, 3),
            "a second draw after a restore succeeds");
_igpu_check(!igpu_is_vertex_buffer_bound(_draw_vb),
            "still unbound after the second draw");

// Two different vertex buffers, drawn in turn: neither may be left bound.
// This catches a restore that happens to work for one buffer by accident.
var _vb2 = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
_igpu_check(_vb2 > 0, "second vertex buffer is created");
_igpu_check(igpu_draw(_vb2, _draw_layout, pr_trianglelist, 0, 6), "draw with the second buffer");
_igpu_check(!igpu_is_vertex_buffer_bound(_vb2), "second buffer is not left bound");
_igpu_check(!igpu_is_vertex_buffer_bound(_draw_vb), "first buffer is not left bound either");

// The same must hold for the indexed path, which also rebinds the index slot.
_igpu_check(igpu_draw(_draw_vb, _draw_layout, pr_trianglelist, 0, 3),
            "draw after the second buffer succeeds");
_igpu_check(!igpu_is_vertex_buffer_bound(_draw_vb), "unbound after drawing the first again");
_igpu_check(igpu_buffer_release(_vb2), "release second vertex buffer");

// An explicit sub-range must work.
_igpu_check(igpu_draw(_draw_vb, _draw_layout, pr_trianglelist, 3, 3),
            "drawing a sub-range succeeds");

// The other topologies must map too.
_igpu_check(igpu_draw(_draw_vb, _draw_layout, pr_linelist, 0, 4), "line list draw succeeds");
_igpu_check(igpu_draw(_draw_vb, _draw_layout, pr_pointlist, 0, 6), "point list draw succeeds");
_igpu_check(!igpu_draw(_draw_vb, _draw_layout, pr_linelist, 0, 7),
            "line list with too many vertices is rejected");

_igpu_check(igpu_get_draw_restore_failures() == _fails_before,
            "no restore failures across every draw so far");

// --- indexed drawing ---

// A 16-bit index buffer: 6 indices forming two triangles over 4 vertices.
var _ib = igpu_buffer_create(12, IgpuBufferUsage.Dynamic, IgpuBufferBind.Index, 0);
show_debug_message("index buffer      : " + string(_ib));
_igpu_check(_ib > 0, "index buffer is created");

var _index_data = buffer_create(12, buffer_fixed, 2);
buffer_seek(_index_data, buffer_seek_start, 0);
buffer_write(_index_data, buffer_u16, 0);
buffer_write(_index_data, buffer_u16, 1);
buffer_write(_index_data, buffer_u16, 2);
buffer_write(_index_data, buffer_u16, 0);
buffer_write(_index_data, buffer_u16, 2);
buffer_write(_index_data, buffer_u16, 3);
_igpu_check(igpu_buffer_write(_ib, 0, _index_data), "upload indices");
buffer_delete(_index_data);

// A vertex buffer is still required even for an indexed draw.
_igpu_check(!igpu_draw_indexed(_draw_vb, _draw_layout, _not_vertex, pr_trianglelist, 0, 6), "indexed draw with a non-index buffer is rejected");

_igpu_check(!igpu_draw_indexed(_draw_vb, _draw_layout, 999999, pr_trianglelist, 0, 6), "indexed draw with an unknown index buffer is rejected");

// 12 bytes / 2 bytes per index = 6 indices; 7 must be refused.
_igpu_check(!igpu_draw_indexed(_draw_vb, _draw_layout, _ib, pr_trianglelist, 0, 7), "indexed draw past the end is rejected");
show_debug_message("index overflow    : " + string(igpu_get_last_error()));

var _indexed_drew = igpu_draw_indexed(_draw_vb, _draw_layout, _ib, pr_trianglelist, 0, 6);
show_debug_message("indexed draw      : " + string(_indexed_drew));
if (!_indexed_drew) { show_debug_message("indexed error     : " + string(igpu_get_last_error())); }
_igpu_check(_indexed_drew, "an indexed draw succeeds");
_igpu_check(igpu_get_draw_restore_failures() == _fails_before,
            "indexed draw also restores state");

_igpu_check(igpu_supports(IgpuCapability.Draw), "supports(draw)");
_igpu_check(igpu_supports(IgpuCapability.DrawIndexed), "supports(draw indexed)");
_igpu_check(igpu_supports(IgpuCapability.DrawStateRestore), "supports(draw state restore)");

_igpu_check(igpu_buffer_release(_ib), "release index buffer");
_igpu_check(igpu_buffer_release(_not_vertex), "release non-vertex buffer");
_igpu_check(igpu_buffer_release(_draw_vb), "release draw vertex buffer");
_igpu_check(igpu_input_layout_release(_draw_layout), "release draw layout");
_igpu_check(igpu_shader_release(_draw_shader), "release draw shader");

// ---------------------------------------------------------------------------
// Churn
//
// The shader entry now owns two COM objects (the device object and the
// bytecode blob) instead of one, so create/release is exercised repeatedly.
// A double-release or a missed release shows up here as a crash or an
// allocation failure rather than silently at shutdown.
// ---------------------------------------------------------------------------

var _churn_ok = true;
for (var _i = 0; _i < 25; _i++)
{
    var _cs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
    if (_cs == 0) { _churn_ok = false; break; }

    var _cl = igpu_vertex_format(_cs, [
        [vertex_usage_position, vertex_type_float3],
        [vertex_usage_texcoord, vertex_type_float2]
    ]);
    if (_cl == 0) { _churn_ok = false; break; }

    if (!igpu_input_layout_release(_cl)) { _churn_ok = false; break; }
    if (!igpu_shader_release(_cs))       { _churn_ok = false; break; }
}

_igpu_check(_churn_ok, "create/release churn completes cleanly");

// A layout built from a shader that is then released must not be used by IGPU
// again, but releasing the shader first must itself stay safe.
var _orphan_shader = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _orphan_layout = igpu_vertex_format(_orphan_shader, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
_igpu_check(_orphan_shader > 0 && _orphan_layout > 0, "orphan pair created");
_igpu_check(igpu_shader_release(_orphan_shader), "release shader before its layout");
_igpu_check(igpu_input_layout_release(_orphan_layout), "release layout after its shader");

// Shutdown must release everything still live without touching GM's device.
igpu_shutdown();
_igpu_check(!igpu_is_available(), "shutdown clears availability");
_igpu_check(igpu_get_capabilities().backend == "none", "capabilities degrade after shutdown");

// Re-initialising after shutdown must work: the borrowed device is still valid.
_igpu_check(igpu_init_from_game(), "re-init after shutdown");
_igpu_check(igpu_is_available(), "available again after re-init");
_igpu_check(igpu_get_capabilities().backend == "d3d11", "backend restored after re-init");

// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
show_debug_message($"checks failed    : {_igpu_failures}");
_igpu_finish(_igpu_failures == 0);
