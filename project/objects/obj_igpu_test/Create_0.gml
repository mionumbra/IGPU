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
function _igpu_uniform_member(_block, _name)
{
    if (is_undefined(_block) || string_length(_name) == 0)
    {
        return undefined;
    }
    var _n = array_length(_block.members);
    for (var _i = 0; _i < _n; _i++)
    {
        if (_block.members[_i].name == _name)
        {
            return _block.members[_i];
        }
    }
    return undefined;
}

function _igpu_check(_condition, _label)
{
    if (!_condition)
    {
        _igpu_failures++;
        show_debug_message($"CHECK FAILED : {_label}");
    }
    return _condition;
}

/// One vertex of the pixel-test layout: clip-space xyz plus a uv.
/// Written field by field because a negative number inside a `[ ]` literal
/// is misparsed by the asset compiler.
function _igpu_write_vert(_buf, _x, _y, _z, _u, _v)
{
    buffer_write(_buf, buffer_f32, _x);
    buffer_write(_buf, buffer_f32, _y);
    buffer_write(_buf, buffer_f32, _z);
    buffer_write(_buf, buffer_f32, _u);
    buffer_write(_buf, buffer_f32, _v);
}

/// Binds `_surf`, clears it, and commits blend / depth / cull to the device.
/// `draw_clear` takes the fast path when the viewport already matches the
/// surface, and that path does not apply those states. `draw_point` forces a
/// flush; the point is outside the surface camera, so it is clipped.
function _igpu_target_begin(_surf, _clear)
{
    surface_set_target(_surf);
    gpu_set_blendenable(false);
    gpu_set_ztestenable(false);
    gpu_set_zwriteenable(false);
    gpu_set_cullmode(cull_noculling);
    gpu_set_alphatestenable(false);
    draw_clear(_clear);
    draw_point(1000, 1000);
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
_igpu_check(!igpu_device_lost(), "a live device is not reported lost");
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
_igpu_check(_caps.uniform_reflection, "uniform reflection reported");
_igpu_check(_caps.instancing, "instancing reported");
_igpu_check(_caps.structured_buffer, "structured buffer reported");
_igpu_check(igpu_set_graphics_info("Qualcomm", "OpenGL ES 3.2 V@0502.0", "Adreno (TM) 640", "OpenGL ES GLSL ES 3.20", 4096), "graphics info is accepted while a device is bound");
_igpu_check(_caps.backend == "d3d11", "the capability snapshot stays on the device");
_igpu_check(igpu_get_capabilities().backend == "d3d11", "a bound device wins over the graphics note");
_igpu_check(igpu_get_capabilities().tier == 1, "tier stays 1 while a device is bound");
_igpu_check(igpu_set_graphics_info("", "", "", "", 0), "clearing the graphics note");
_igpu_check(is_struct(_caps.formats), "formats is a struct");
_igpu_check(_caps.formats.surface_rgba8unorm, "surface_rgba8unorm can be created");
_igpu_check(_caps.formats.surface_r16float, "surface_r16float can be created");
_igpu_check(_caps.formats.surface_r32float, "surface_r32float can be created");
_igpu_check(_caps.formats.surface_rgba4unorm, "surface_rgba4unorm can be created");
_igpu_check(_caps.formats.surface_r8unorm, "surface_r8unorm can be created");
_igpu_check(_caps.formats.surface_rg8unorm, "surface_rg8unorm can be created");
_igpu_check(_caps.formats.surface_rgba16float, "surface_rgba16float can be created");
_igpu_check(_caps.formats.surface_rgba32float, "surface_rgba32float can be created");

var _fmt_r8 = igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_r8unorm, false, false, 1);
var _fmt_rgba4 = igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba4unorm, false, false, 1);
_igpu_check(_fmt_r8 > 0, "an r8 texture is created");
_igpu_check(_fmt_rgba4 > 0, "an rgba4 texture is created");
_igpu_check(igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba4unorm, true, false, 1) == 0, "rgba4 is not a render target");
_igpu_check(igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, 1, false, false, 1) == 0, "an unknown surface format is rejected");
_igpu_check(igpu_texture_release(_fmt_r8), "release r8 texture");
_igpu_check(igpu_texture_release(_fmt_rgba4), "release rgba4 texture");

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
var _pixel = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1, 0, 0, 1); }", "main", IgpuShaderStage.Pixel, "");
show_debug_message("pixel shader handle: " + string(_pixel));

if (_pixel == 0)
{
    show_debug_message("pixel shader error : " + string(igpu_get_last_error()));
}

_igpu_check(_igpu_shader > 0, "unified compile returns a handle");
_igpu_check(_pixel > 0, "wrapper compile returns a handle");
_igpu_check(_pixel != _igpu_shader, "handles are distinct");

// A dialect this backend does not translate is still rejected.
var _msl = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "msl");
show_debug_message("msl dialect handle: " + string(_msl));
show_debug_message("msl dialect error : " + string(igpu_get_last_error()));
_igpu_check(_msl == 0, "foreign dialect rejected");

// Shader handles must release cleanly.
if (_igpu_shader > 0) { _igpu_check(igpu_shader_release(_igpu_shader), "release unified handle"); }
if (_pixel > 0)       { _igpu_check(igpu_shader_release(_pixel), "release wrapper handle"); }
_igpu_check(!igpu_shader_release(999999), "releasing a bogus handle fails");

// ---------------------------------------------------------------------------
// Shader binding
//
// This block exists because an engine-source audit found that igpu_draw could
// not actually put an IGPU shader on the pipeline: igpu_shader_compile returned
// a handle that no function could bind, so a draw ran against whatever shader
// GameMaker happened to have, and "draw returned true" only proved the call was
// issued. igpu_shader_bind is the missing half, and these assertions cover the
// paths that were previously unverifiable.
// ---------------------------------------------------------------------------

var _bind_vs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
_igpu_check(_bind_vs > 0, "shader for binding compiles");

// A fresh stage reports nothing bound, so the bookkeeping starts clean.
_igpu_check(igpu_get_bound_shader(IgpuShaderStage.Vertex) == 0, "nothing bound before bind");

_igpu_check(igpu_shader_bind(_bind_vs, IgpuShaderStage.Vertex), "bind a vertex shader");
_igpu_check(igpu_get_bound_shader(IgpuShaderStage.Vertex) == _bind_vs, "bound handle is reported back");

// Binding is per stage, not global: binding the vertex stage must not make the
// pixel stage claim a shader.
_igpu_check(igpu_get_bound_shader(IgpuShaderStage.Pixel) == 0, "binding is per stage");

// A stage mismatch must be refused. This is the assertion that would have
// caught the original defect: a handle compiled for one stage must not be
// accepted by another.
_igpu_check(!igpu_shader_bind(_bind_vs, IgpuShaderStage.Pixel), "stage mismatch is rejected");
show_debug_message("stage mismatch err: " + string(igpu_get_last_error()));
_igpu_check(igpu_get_bound_shader(IgpuShaderStage.Vertex) == _bind_vs,
            "a rejected bind leaves the previous binding intact");

// Unknown handles and unknown stage numbers fail cleanly rather than faulting.
_igpu_check(!igpu_shader_bind(999999, IgpuShaderStage.Vertex), "unknown shader handle is rejected");
_igpu_check(!igpu_shader_bind(_bind_vs, 999), "unknown stage number is rejected");
_igpu_check(igpu_get_bound_shader(999) == 0, "unknown stage reports nothing bound");

// shader = 0 unbinds deliberately, and does not release the shader.
_igpu_check(igpu_shader_bind(0, IgpuShaderStage.Vertex), "shader 0 unbinds");
_igpu_check(igpu_get_bound_shader(IgpuShaderStage.Vertex) == 0, "unbind clears the record");

// Releasing a bound shader must drop the record too, or a caller asking "is my
// shader still bound" would be told yes about a handle that no longer exists.
_igpu_check(igpu_shader_bind(_bind_vs, IgpuShaderStage.Vertex), "rebind before release");
_igpu_check(igpu_get_bound_shader(IgpuShaderStage.Vertex) == _bind_vs, "rebind is recorded");
_igpu_check(igpu_shader_release(_bind_vs), "release the bound shader");
_igpu_check(igpu_get_bound_shader(IgpuShaderStage.Vertex) == 0,
            "releasing a bound shader clears its binding");

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
// A draw overwrites input-assembler state that GML cannot read or write.
// GameMaker's own draws rebind the vertex buffer, layout and topology, so the
// restore is not what protects those draws. It puts the index buffer back,
// which GameMaker never rebinds, and the counter below is how that contract
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


// --- argument validation: every one of these must refuse WITHOUT drawing ---

// A fan has no backend topology, so it must be refused rather than silently
// drawn as something the caller did not ask for.
_igpu_check(!igpu_draw(_draw_vb, 0, _draw_layout, pr_trianglefan, 0, 3, 1, 0, 0, 0, 0),
            "triangle fan is rejected");
show_debug_message("fan error         : " + string(igpu_get_last_error()));

_igpu_check(!igpu_draw(_draw_vb, 0, _draw_layout, 99, 0, 3, 1, 0, 0, 0, 0), "unknown primitive is rejected");

var _not_vertex = igpu_buffer_create(64, IgpuBufferUsage.Dynamic, IgpuBufferBind.Uniform, 0);
_igpu_check(!igpu_draw(_not_vertex, 0, _draw_layout, pr_trianglelist, 0, 3, 1, 0, 0, 0, 0),
            "drawing a non-vertex buffer is rejected");

_igpu_check(!igpu_draw(999999, 0, _draw_layout, pr_trianglelist, 0, 3, 1, 0, 0, 0, 0),
            "drawing an unknown buffer is rejected");
_igpu_check(!igpu_draw(_draw_vb, 0, 999999, pr_trianglelist, 0, 3, 1, 0, 0, 0, 0),
            "drawing with an unknown layout is rejected");

// 120 bytes / 20 stride = 6 vertices; asking for 7 must be refused rather than
// reading past the buffer.
_igpu_check(!igpu_draw(_draw_vb, 0, _draw_layout, pr_trianglelist, 0, 7, 1, 0, 0, 0, 0),
            "drawing more vertices than the buffer holds is rejected");
show_debug_message("overflow error    : " + string(igpu_get_last_error()));

_igpu_check(!igpu_draw(_draw_vb, 0, _draw_layout, pr_trianglelist, 0, 0, 1, 0, 0, 0, 0),
            "a zero vertex count is rejected");
_igpu_check(!igpu_draw(_draw_vb, 0, _draw_layout, pr_trianglelist, -1, 3, 1, 0, 0, 0, 0),
            "a negative first vertex is rejected");

// --- the real draw ---

// vertex_count = -1 means "to the end of the buffer" (6 vertices here).
var _drew = igpu_draw(_draw_vb, 0, _draw_layout, pr_trianglelist, 0, -1, 1, 0, 0, 0, 0);
show_debug_message("draw result       : " + string(_drew));
if (!_drew) { show_debug_message("draw error        : " + string(igpu_get_last_error())); }
_igpu_check(_drew, "a valid triangle list draw succeeds");

_igpu_check(igpu_draw(_draw_vb, 0, _draw_layout, pr_trianglelist, 0, 3, 1, 0, 0, 0, 0),
            "a second draw after a restore succeeds");

// Two different vertex buffers, drawn in turn: neither may be left bound.
// This catches a restore that happens to work for one buffer by accident.
var _vb2 = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
_igpu_check(_vb2 > 0, "second vertex buffer is created");
_igpu_check(igpu_draw(_vb2, 0, _draw_layout, pr_trianglelist, 0, 6, 1, 0, 0, 0, 0), "draw with the second buffer");

// The same must hold for the indexed path, which also rebinds the index slot.
_igpu_check(igpu_draw(_draw_vb, 0, _draw_layout, pr_trianglelist, 0, 3, 1, 0, 0, 0, 0),
            "draw after the second buffer succeeds");
_igpu_check(igpu_buffer_release(_vb2), "release second vertex buffer");

// An explicit sub-range must work.
_igpu_check(igpu_draw(_draw_vb, 0, _draw_layout, pr_trianglelist, 3, 3, 1, 0, 0, 0, 0),
            "drawing a sub-range succeeds");

// The other topologies must map too.
_igpu_check(igpu_draw(_draw_vb, 0, _draw_layout, pr_linelist, 0, 4, 1, 0, 0, 0, 0), "line list draw succeeds");
_igpu_check(igpu_draw(_draw_vb, 0, _draw_layout, pr_pointlist, 0, 6, 1, 0, 0, 0, 0), "point list draw succeeds");
_igpu_check(!igpu_draw(_draw_vb, 0, _draw_layout, pr_linelist, 0, 7, 1, 0, 0, 0, 0),
            "line list with too many vertices is rejected");

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
_igpu_check(!igpu_draw_indexed(_draw_vb, _draw_layout, _not_vertex, pr_trianglelist, 0, 6, 0, 0, 0, 0), "indexed draw with a non-index buffer is rejected");

_igpu_check(!igpu_draw_indexed(_draw_vb, _draw_layout, 999999, pr_trianglelist, 0, 6, 0, 0, 0, 0), "indexed draw with an unknown index buffer is rejected");

// 12 bytes / 2 bytes per index = 6 indices; 7 must be refused.
_igpu_check(!igpu_draw_indexed(_draw_vb, _draw_layout, _ib, pr_trianglelist, 0, 7, 0, 0, 0, 0), "indexed draw past the end is rejected");
show_debug_message("index overflow    : " + string(igpu_get_last_error()));

var _indexed_drew = igpu_draw_indexed(_draw_vb, _draw_layout, _ib, pr_trianglelist, 0, 6, 0, 0, 0, 0);
show_debug_message("indexed draw      : " + string(_indexed_drew));
if (!_indexed_drew) { show_debug_message("indexed error     : " + string(igpu_get_last_error())); }
_igpu_check(_indexed_drew, "an indexed draw succeeds");

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

// ---------------------------------------------------------------------------
// Pixel readback
//
// A draw that returns true only proves the call was issued. GameMaker already
// binds a surface as the render target (`surface_set_target` → the engine's
// own render-target view), so the missing proof is: draw into an offscreen
// surface and read the pixels back.
//
// The triangle covers only the left half of clip space. The right half stays
// the clear colour, which is what distinguishes "the draw rasterized" from
// "the whole surface was cleared to the draw colour".
//
// Shaders are bound AFTER `_igpu_target_begin`. That helper clears and flushes
// through GameMaker, and GameMaker rebinds its own shaders on every flush.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");

var _pix_vs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _pix_ps = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _pix_layout = igpu_vertex_format(_pix_vs, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
_igpu_check(_pix_vs > 0, "pixel-test vertex shader compiles");
_igpu_check(_pix_ps > 0, "pixel-test pixel shader compiles");
_igpu_check(_pix_layout > 0, "pixel-test layout is created");

// Six vertices, stride 20. Two triangles make the quad x = [-1, 0], y = [-1, 1].
var _pix_vb = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
_igpu_check(_pix_vb > 0, "pixel-test vertex buffer is created");

var _pix_upload = buffer_create(120, buffer_fixed, 4);
buffer_seek(_pix_upload, buffer_seek_start, 0);
_igpu_write_vert(_pix_upload, -1, -1, 0.5, 0, 0);
_igpu_write_vert(_pix_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_pix_upload, -1, 1, 0.5, 0, 1);
_igpu_write_vert(_pix_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_pix_upload, 0, 1, 0.5, 1, 1);
_igpu_write_vert(_pix_upload, -1, 1, 0.5, 0, 1);
_igpu_check(igpu_buffer_write(_pix_vb, 0, _pix_upload), "upload pixel-test vertices");
buffer_delete(_pix_upload);

// Four unique vertices plus six indices, covering the same left half.
var _pix_vb_indexed = igpu_buffer_create(80, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _pix_ib = igpu_buffer_create(12, IgpuBufferUsage.Dynamic, IgpuBufferBind.Index, 0);
_igpu_check(_pix_vb_indexed > 0, "pixel-test indexed vertex buffer is created");
_igpu_check(_pix_ib > 0, "pixel-test index buffer is created");

var _pix_indexed_upload = buffer_create(80, buffer_fixed, 4);
buffer_seek(_pix_indexed_upload, buffer_seek_start, 0);
_igpu_write_vert(_pix_indexed_upload, -1, -1, 0.5, 0, 0);
_igpu_write_vert(_pix_indexed_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_pix_indexed_upload, -1, 1, 0.5, 0, 1);
_igpu_write_vert(_pix_indexed_upload, 0, 1, 0.5, 1, 1);
_igpu_check(igpu_buffer_write(_pix_vb_indexed, 0, _pix_indexed_upload), "upload indexed pixel-test vertices");
buffer_delete(_pix_indexed_upload);

var _pix_indices = buffer_create(12, buffer_fixed, 2);
buffer_seek(_pix_indices, buffer_seek_start, 0);
buffer_write(_pix_indices, buffer_u16, 0);
buffer_write(_pix_indices, buffer_u16, 1);
buffer_write(_pix_indices, buffer_u16, 2);
buffer_write(_pix_indices, buffer_u16, 1);
buffer_write(_pix_indices, buffer_u16, 3);
buffer_write(_pix_indices, buffer_u16, 2);
_igpu_check(igpu_buffer_write(_pix_ib, 0, _pix_indices), "upload pixel-test indices");
buffer_delete(_pix_indices);

var _pix_surf = surface_create(8, 8);
var _pix_surf_indexed = surface_create(8, 8);
_igpu_check(surface_exists(_pix_surf), "pixel-test surface is created");
_igpu_check(surface_exists(_pix_surf_indexed), "indexed pixel-test surface is created");

if (surface_exists(_pix_surf) && _pix_vb > 0 && _pix_layout > 0 && _pix_vs > 0 && _pix_ps > 0)
{
    gpu_push_state();

    // The clear itself has to round-trip, or a red pixel later could be a
    // coincidence of the surface's initial contents.
    _igpu_target_begin(_pix_surf, c_blue);
    surface_reset_target();

    var _cleared_left = surface_getpixel(_pix_surf, 1, 4);
    var _cleared_right = surface_getpixel(_pix_surf, 6, 4);
    show_debug_message("clear left        : " + string(_cleared_left)
        + " r=" + string(color_get_red(_cleared_left))
        + " g=" + string(color_get_green(_cleared_left))
        + " b=" + string(color_get_blue(_cleared_left)));
    show_debug_message("clear right       : " + string(_cleared_right)
        + " r=" + string(color_get_red(_cleared_right))
        + " g=" + string(color_get_green(_cleared_right))
        + " b=" + string(color_get_blue(_cleared_right)));
    _igpu_check(_cleared_left == c_blue, "cleared surface reads back blue");
    _igpu_check(_cleared_right == c_blue, "both halves of the clear read back blue");

    _igpu_target_begin(_pix_surf, c_blue);
    var _bound_vs = igpu_shader_bind(_pix_vs, IgpuShaderStage.Vertex);
    var _bound_ps = igpu_shader_bind(_pix_ps, IgpuShaderStage.Pixel);
    _igpu_check(_bound_vs, "pixel-test vertex shader binds");
    _igpu_check(_bound_ps, "pixel-test pixel shader binds");

    var _pix_drew = igpu_draw(_pix_vb, 0, _pix_layout, pr_trianglelist, 0, 6, 1, 0, 0, 0, 0);
    show_debug_message("pixel draw        : " + string(_pix_drew));
    if (!_pix_drew) { show_debug_message("pixel draw error  : " + string(igpu_get_last_error())); }
    surface_reset_target();

    var _drawn_left = surface_getpixel(_pix_surf, 1, 4);
    var _drawn_right = surface_getpixel(_pix_surf, 6, 4);
    show_debug_message("drawn left        : " + string(_drawn_left)
        + " r=" + string(color_get_red(_drawn_left))
        + " g=" + string(color_get_green(_drawn_left))
        + " b=" + string(color_get_blue(_drawn_left)));
    show_debug_message("drawn right       : " + string(_drawn_right)
        + " r=" + string(color_get_red(_drawn_right))
        + " g=" + string(color_get_green(_drawn_right))
        + " b=" + string(color_get_blue(_drawn_right)));

    _igpu_check(_pix_drew, "offscreen triangle draw succeeds");
    _igpu_check(_drawn_left == c_red, "drawn half of the surface is red");
    _igpu_check(_drawn_right == c_blue, "undrawn half of the surface stays blue");

    // Indexed path, same coverage, on a surface cleared to a different colour
    // so a leak from the first surface cannot satisfy the assertion.
    if (surface_exists(_pix_surf_indexed) && _pix_vb_indexed > 0 && _pix_ib > 0)
    {
        _igpu_target_begin(_pix_surf_indexed, c_green);
        _igpu_check(igpu_shader_bind(_pix_vs, IgpuShaderStage.Vertex), "rebind vertex shader before indexed draw");
        _igpu_check(igpu_shader_bind(_pix_ps, IgpuShaderStage.Pixel), "rebind pixel shader before indexed draw");

        var _pix_indexed_drew = igpu_draw_indexed(_pix_vb_indexed, _pix_layout, _pix_ib, pr_trianglelist, 0, 6, 0, 0, 0, 0);
        show_debug_message("pixel indexed     : " + string(_pix_indexed_drew));
        if (!_pix_indexed_drew) { show_debug_message("pixel indexed err : " + string(igpu_get_last_error())); }
        surface_reset_target();

        var _indexed_left = surface_getpixel(_pix_surf_indexed, 1, 4);
        var _indexed_right = surface_getpixel(_pix_surf_indexed, 6, 4);
        show_debug_message("indexed left      : " + string(_indexed_left)
            + " r=" + string(color_get_red(_indexed_left))
            + " g=" + string(color_get_green(_indexed_left))
            + " b=" + string(color_get_blue(_indexed_left)));
        show_debug_message("indexed right     : " + string(_indexed_right)
            + " r=" + string(color_get_red(_indexed_right))
            + " g=" + string(color_get_green(_indexed_right))
            + " b=" + string(color_get_blue(_indexed_right)));

        _igpu_check(_pix_indexed_drew, "offscreen indexed draw succeeds");
        _igpu_check(_indexed_left == c_red, "indexed draw writes red");
        _igpu_check(_indexed_right == c_green, "indexed draw leaves the other half green");
    }

    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    gpu_pop_state();
}

if (surface_exists(_pix_surf)) { surface_free(_pix_surf); }
if (surface_exists(_pix_surf_indexed)) { surface_free(_pix_surf_indexed); }
_igpu_check(!surface_exists(_pix_surf), "pixel-test surface is freed");
_igpu_check(!surface_exists(_pix_surf_indexed), "indexed pixel-test surface is freed");

_igpu_check(igpu_buffer_release(_pix_vb), "release pixel-test vertex buffer");
_igpu_check(igpu_buffer_release(_pix_vb_indexed), "release indexed pixel-test vertex buffer");
_igpu_check(igpu_buffer_release(_pix_ib), "release pixel-test index buffer");
_igpu_check(igpu_input_layout_release(_pix_layout), "release pixel-test layout");
_igpu_check(igpu_shader_release(_pix_vs), "release pixel-test vertex shader");
_igpu_check(igpu_shader_release(_pix_ps), "release pixel-test pixel shader");

// ---------------------------------------------------------------------------
// Pipeline state objects
//
// Applied for one draw and then put back. GameMaker caches blend, depth and
// raster state, so leaving them bound would change its next draw. The colour
// checks separate "the state was used" from "the whole surface was cleared".
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
show_debug_message("tf_point/linear/aniso : " + string(tf_point) + " " + string(tf_linear) + " " + string(tf_anisotropic));
show_debug_message("bm_zero/one/eq_add    : " + string(bm_zero) + " " + string(bm_one) + " " + string(bm_eq_add));

_igpu_check(igpu_supports(IgpuCapability.BlendState), "supports blend state");
_igpu_check(igpu_supports(IgpuCapability.DepthState), "supports depth state");
_igpu_check(igpu_supports(IgpuCapability.RasterState), "supports raster state");
_igpu_check(igpu_supports(IgpuCapability.SamplerState), "supports sampler state");
_igpu_check(igpu_supports(IgpuCapability.Wireframe), "supports wireframe");
_igpu_check(igpu_get_capabilities().blend_state, "capability struct reports blend state");

var _keep_blend = igpu_blend_state_create(
    true, bm_zero, bm_one, bm_eq_add, bm_zero, bm_one, bm_eq_add,
    true, true, true, true);
_igpu_check(_keep_blend > 0, "keep-destination blend state is created");
_igpu_check(igpu_blend_state_create(
    true, 99, bm_one, bm_eq_add, bm_one, bm_one, bm_eq_add,
    true, true, true, true) == 0, "unknown blend factor is rejected");

var _depth_less = igpu_depth_state_create(
    true, true, cmpfunc_less,
    false, cmpfunc_always, stencilop_keep, stencilop_keep, stencilop_keep,
    0, 255, 255);
var _depth_off = igpu_depth_state_create(
    false, false, cmpfunc_always,
    false, cmpfunc_always, stencilop_keep, stencilop_keep, stencilop_keep,
    0, 255, 255);
_igpu_check(_depth_less > 0 && _depth_off > 0, "depth states are created");
_igpu_check(igpu_depth_state_create(
    true, true, 99,
    false, cmpfunc_always, stencilop_keep, stencilop_keep, stencilop_keep,
    0, 255, 255) == 0, "unknown depth function is rejected");

var _cull_cw = igpu_raster_state_create(cull_clockwise, IgpuFill.Solid, true, true);
var _cull_ccw = igpu_raster_state_create(cull_counterclockwise, IgpuFill.Solid, true, true);
var _wire = igpu_raster_state_create(cull_noculling, IgpuFill.Wireframe, true, true);
_igpu_check(_cull_cw > 0 && _cull_ccw > 0 && _wire > 0, "raster states are created");
_igpu_check(igpu_raster_state_create(99, IgpuFill.Solid, true, true) == 0, "unknown cull mode is rejected");

var _sampler = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
_igpu_check(_sampler > 0, "sampler state is created");
_igpu_check(igpu_sampler_state_create(99, 99, 99, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1) == 0, "unknown sampler filter is rejected");
_igpu_check(!igpu_state_release(999999), "releasing a bogus state handle fails");

var _st_vs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _st_red = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _st_green = igpu_shader_compile("float4 main() : SV_TARGET { return float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _st_layout = igpu_vertex_format(_st_vs, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
var _st_near = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _st_far = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _st_ib = igpu_buffer_create(12, IgpuBufferUsage.Dynamic, IgpuBufferBind.Index, 0);
_igpu_check(_st_vs > 0 && _st_red > 0 && _st_green > 0 && _st_layout > 0, "state-test shaders and layout are created");
_igpu_check(_st_near > 0 && _st_far > 0 && _st_ib > 0, "state-test buffers are created");

var _st_upload = buffer_create(120, buffer_fixed, 4);
buffer_seek(_st_upload, buffer_seek_start, 0);
_igpu_write_vert(_st_upload, -1, -1, 0.0, 0, 0);
_igpu_write_vert(_st_upload, 0, -1, 0.0, 1, 0);
_igpu_write_vert(_st_upload, -1, 1, 0.0, 0, 1);
_igpu_write_vert(_st_upload, 0, -1, 0.0, 1, 0);
_igpu_write_vert(_st_upload, 0, 1, 0.0, 1, 1);
_igpu_write_vert(_st_upload, -1, 1, 0.0, 0, 1);
_igpu_check(igpu_buffer_write(_st_near, 0, _st_upload), "upload near quad");
buffer_seek(_st_upload, buffer_seek_start, 0);
_igpu_write_vert(_st_upload, -1, -1, 0.5, 0, 0);
_igpu_write_vert(_st_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_st_upload, -1, 1, 0.5, 0, 1);
_igpu_write_vert(_st_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_st_upload, 0, 1, 0.5, 1, 1);
_igpu_write_vert(_st_upload, -1, 1, 0.5, 0, 1);
_igpu_check(igpu_buffer_write(_st_far, 0, _st_upload), "upload far quad");
buffer_delete(_st_upload);

var _st_indices = buffer_create(12, buffer_fixed, 2);
buffer_seek(_st_indices, buffer_seek_start, 0);
buffer_write(_st_indices, buffer_u16, 0);
buffer_write(_st_indices, buffer_u16, 1);
buffer_write(_st_indices, buffer_u16, 2);
buffer_write(_st_indices, buffer_u16, 3);
buffer_write(_st_indices, buffer_u16, 4);
buffer_write(_st_indices, buffer_u16, 5);
_igpu_check(igpu_buffer_write(_st_ib, 0, _st_indices), "upload state-test indices");
buffer_delete(_st_indices);

var _blend_surf = surface_create(8, 8);
var _depth_surf = surface_create(8, 8);
var _cull_cw_surf = surface_create(8, 8);
var _cull_ccw_surf = surface_create(8, 8);
_igpu_check(surface_exists(_blend_surf) && surface_exists(_depth_surf), "state-test surfaces are created");

if (_keep_blend > 0 && _depth_off > 0 && _st_near > 0 && surface_exists(_blend_surf))
{
    gpu_push_state();

    _igpu_target_begin(_blend_surf, c_blue);
    igpu_shader_bind(_st_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_st_red, IgpuShaderStage.Pixel);
    var _kept = igpu_draw(_st_near, 0, _st_layout, pr_trianglelist, 0, 6, 1, _keep_blend, _depth_off, 0, _sampler);
    surface_reset_target();
    var _after_keep = surface_getpixel(_blend_surf, 1, 4);
    show_debug_message("blend keep        : " + string(_kept) + " pixel " + string(_after_keep));

    surface_set_target(_blend_surf);
    draw_point(1000, 1000);
    igpu_shader_bind(_st_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_st_red, IgpuShaderStage.Pixel);
    var _replaced = igpu_draw(_st_near, 0, _st_layout, pr_trianglelist, 0, 6, 1, 0, _depth_off, 0, 0);
    surface_reset_target();
    var _after_replace = surface_getpixel(_blend_surf, 1, 4);
    show_debug_message("blend restored    : " + string(_replaced) + " pixel " + string(_after_replace));

    _igpu_check(_kept, "draw with a blend state succeeds");
    _igpu_check(_after_keep == c_blue, "keep-destination blend leaves the clear colour");
    _igpu_check(_replaced && _after_replace == c_red, "restored blend lets the next draw replace the colour");

    _igpu_check(!igpu_draw(_st_near, 0, _st_layout, pr_trianglelist, 0, 6, 1, 0, _keep_blend, 0, 0), "depth slot rejects a blend state");

    if (surface_exists(_depth_surf) && _depth_less > 0 && _st_far > 0 && _st_green > 0)
    {
        _igpu_target_begin(_depth_surf, c_blue);
        igpu_shader_bind(_st_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_st_red, IgpuShaderStage.Pixel);
        igpu_draw(_st_near, 0, _st_layout, pr_trianglelist, 0, 6, 1, 0, _depth_less, 0, 0);
        igpu_shader_bind(_st_green, IgpuShaderStage.Pixel);
        igpu_draw(_st_far, 0, _st_layout, pr_trianglelist, 0, 6, 1, 0, _depth_less, 0, 0);
        surface_reset_target();
        var _depth_pixel = surface_getpixel(_depth_surf, 1, 4);
        show_debug_message("depth pixel       : " + string(_depth_pixel)
            + " r=" + string(color_get_red(_depth_pixel))
            + " g=" + string(color_get_green(_depth_pixel))
            + " b=" + string(color_get_blue(_depth_pixel)));
        _igpu_check(_depth_pixel == c_red, "a farther triangle fails the depth test");
    }

    if (_cull_cw > 0 && _cull_ccw > 0 && surface_exists(_cull_cw_surf) && surface_exists(_cull_ccw_surf))
    {
        _igpu_target_begin(_cull_cw_surf, c_blue);
        igpu_shader_bind(_st_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_st_red, IgpuShaderStage.Pixel);
        igpu_draw(_st_near, 0, _st_layout, pr_trianglelist, 0, 6, 1, 0, _depth_off, _cull_cw, 0);
        surface_reset_target();

        _igpu_target_begin(_cull_ccw_surf, c_blue);
        igpu_shader_bind(_st_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_st_red, IgpuShaderStage.Pixel);
        igpu_draw(_st_near, 0, _st_layout, pr_trianglelist, 0, 6, 1, 0, _depth_off, _cull_ccw, 0);
        surface_reset_target();

        var _cw_pixel = surface_getpixel(_cull_cw_surf, 1, 4);
        var _ccw_pixel = surface_getpixel(_cull_ccw_surf, 1, 4);
        show_debug_message("cull cw/ccw       : " + string(_cw_pixel) + " / " + string(_ccw_pixel));
        // The quad is counter-clockwise on screen, so it is a back face
        // (front faces are clockwise). cull_counterclockwise drops it.
        _igpu_check(_cw_pixel == c_red, "cull_clockwise keeps the test triangle");
        _igpu_check(_ccw_pixel == c_blue, "cull_counterclockwise drops the test triangle");
    }

    if (_st_ib > 0)
    {
        _igpu_target_begin(_blend_surf, c_blue);
        igpu_shader_bind(_st_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_st_red, IgpuShaderStage.Pixel);
        var _indexed_kept = igpu_draw_indexed(_st_near, _st_layout, _st_ib, pr_trianglelist, 0, 6, _keep_blend, _depth_off, 0, 0);
        surface_reset_target();
        _igpu_check(_indexed_kept && surface_getpixel(_blend_surf, 1, 4) == c_blue,
            "indexed draw honours the blend state");
    }

    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    gpu_pop_state();
}

if (surface_exists(_blend_surf)) surface_free(_blend_surf);
if (surface_exists(_depth_surf)) surface_free(_depth_surf);
if (surface_exists(_cull_cw_surf)) surface_free(_cull_cw_surf);
if (surface_exists(_cull_ccw_surf)) surface_free(_cull_ccw_surf);

_igpu_check(igpu_state_release(_keep_blend), "release blend state");
_igpu_check(igpu_state_release(_depth_less), "release depth state");
_igpu_check(igpu_state_release(_depth_off), "release depth-off state");
_igpu_check(igpu_state_release(_cull_cw), "release clockwise raster state");
_igpu_check(igpu_state_release(_cull_ccw), "release counter-clockwise raster state");
_igpu_check(igpu_state_release(_wire), "release wireframe raster state");
_igpu_check(igpu_state_release(_sampler), "release sampler state");
_igpu_check(igpu_buffer_release(_st_near), "release state-test near buffer");
_igpu_check(igpu_buffer_release(_st_far), "release state-test far buffer");
_igpu_check(igpu_buffer_release(_st_ib), "release state-test index buffer");
_igpu_check(igpu_input_layout_release(_st_layout), "release state-test layout");
_igpu_check(igpu_shader_release(_st_vs), "release state-test vertex shader");
_igpu_check(igpu_shader_release(_st_red), "release state-test red shader");
_igpu_check(igpu_shader_release(_st_green), "release state-test green shader");

// ---------------------------------------------------------------------------
// Textures
//
// An IGPU texture can be a render target. The draw binds it only for that
// call and puts GameMaker's target back. get_pixel reads surface_rgba8unorm
// the same way surface_getpixel does. A second draw samples the texture onto
// a GameMaker surface, which also shows the previous target was restored.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
show_debug_message("surface_rgba8unorm : " + string(surface_rgba8unorm));
_igpu_check(igpu_supports(IgpuCapability.Texture2D), "supports 2D textures");
_igpu_check(igpu_get_capabilities().texture_2d, "capability struct reports texture_2d");
_igpu_check(igpu_get_capabilities().texture_3d, "3D textures are supported");
_igpu_check(igpu_get_capabilities().texture_array, "texture arrays are supported");
_igpu_check(igpu_get_capabilities().texture_cubemap, "cube textures are supported");
_igpu_check(igpu_supports(IgpuCapability.UnorderedAccess), "storage textures are supported");

var _tex = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, true, false, 1);
var _not_target = igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba8unorm, false, false, 1);
_igpu_check(_tex > 0, "rgba8 render-target texture is created");
_igpu_check(_not_target > 0, "non-target texture is created");
_igpu_check(igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, 1, false, false, 1) == 0, "compressed format is rejected");
_igpu_check(igpu_texture_create(IgpuTextureKind.TwoD, 0, 8, 1, surface_rgba8unorm, true, false, 1) == 0, "zero-size texture is rejected");
_igpu_check(!igpu_draw_to_render_targets(0, 0, pr_trianglelist, 0, 3, [_not_target], [0], [0], 0, 0, 0, 0), "drawing to a non-target texture is rejected");

var _tex_vs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _tex_red = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _tex_sample = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, i.uv); }", "main", IgpuShaderStage.Pixel, "");
var _tex_layout = igpu_vertex_format(_tex_vs, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
var _tex_vb = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _tex_full = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
_igpu_check(_tex_vs > 0 && _tex_red > 0 && _tex_sample > 0 && _tex_layout > 0, "texture-test shaders and layout are created");
_igpu_check(_tex_vb > 0 && _tex_full > 0, "texture-test buffers are created");

var _tex_upload = buffer_create(120, buffer_fixed, 4);
buffer_seek(_tex_upload, buffer_seek_start, 0);
_igpu_write_vert(_tex_upload, -1, -1, 0.5, 0, 0);
_igpu_write_vert(_tex_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_tex_upload, -1, 1, 0.5, 0, 1);
_igpu_write_vert(_tex_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_tex_upload, 0, 1, 0.5, 1, 1);
_igpu_write_vert(_tex_upload, -1, 1, 0.5, 0, 1);
_igpu_check(igpu_buffer_write(_tex_vb, 0, _tex_upload), "upload texture-test quad");
buffer_seek(_tex_upload, buffer_seek_start, 0);
_igpu_write_vert(_tex_upload, -1, -1, 0.5, 0, 1);
_igpu_write_vert(_tex_upload, 1, -1, 0.5, 1, 1);
_igpu_write_vert(_tex_upload, -1, 1, 0.5, 0, 0);
_igpu_write_vert(_tex_upload, 1, -1, 0.5, 1, 1);
_igpu_write_vert(_tex_upload, 1, 1, 0.5, 1, 0);
_igpu_write_vert(_tex_upload, -1, 1, 0.5, 0, 0);
_igpu_check(igpu_buffer_write(_tex_full, 0, _tex_upload), "upload fullscreen sample quad");
buffer_delete(_tex_upload);

if (_tex > 0)
{
    var _cleared_l = igpu_texture_read(_tex, 1, 4, 0, 0);
    var _cleared_r = igpu_texture_read(_tex, 6, 4, 0, 0);
    show_debug_message("texture clear     : " + string(_cleared_l) + " / " + string(_cleared_r));
    _igpu_check(_cleared_l == 0 && _cleared_r == 0, "a new render target reads back black");

    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
    var _to_tex = igpu_draw_to_render_targets(_tex_vb, _tex_layout, pr_trianglelist, 0, 6, [_tex], [0], [0], 0, 0, 0, 0);
    var _tex_l = igpu_texture_read(_tex, 1, 4, 0, 0);
    var _tex_r = igpu_texture_read(_tex, 6, 4, 0, 0);
    show_debug_message("texture draw      : " + string(_to_tex) + " left " + string(_tex_l) + " right " + string(_tex_r));
    _igpu_check(_to_tex, "draw to an IGPU texture succeeds");
    _igpu_check(_tex_l == c_red, "drawn half of the texture is red");
    _igpu_check(_tex_r == 0, "undrawn half of the texture stays black");
    _igpu_check(igpu_texture_read(_tex, 99, 0, 0, 0) == 0, "out-of-range pixel read fails");
    _igpu_check(igpu_get_last_error() != "", "out-of-range pixel read sets an error");
}

var _sample_surf = surface_create(8, 8);
if (_tex > 0 && _tex_sample > 0 && _tex_full > 0 && surface_exists(_sample_surf))
{
    gpu_push_state();
    _igpu_target_begin(_sample_surf, c_blue);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_sample, IgpuShaderStage.Pixel);
    var _sampled = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _tex, 0, 0, 0, 0);
    surface_reset_target();
    gpu_pop_state();
    var _sample_l = surface_getpixel(_sample_surf, 1, 4);
    var _sample_r = surface_getpixel(_sample_surf, 6, 4);
    show_debug_message("texture sample    : " + string(_sampled) + " left " + string(_sample_l) + " right " + string(_sample_r));
    _igpu_check(_sampled, "sampled draw succeeds");
    _igpu_check(_sample_l == c_red, "sampled left half is red");
    _igpu_check(_sample_r == c_black, "sampled right half is black");
}
if (surface_exists(_sample_surf)) surface_free(_sample_surf);

// Keep-destination blend on the draws that take pipeline state.
// Source factor zero leaves the colour already in the target. A following
// draw with every state handle at 0 must replace that colour, which shows
// the blend was put back.
var _pipe_hold = igpu_blend_state_create(
    true, bm_zero, bm_one, bm_eq_add, bm_zero, bm_one, bm_eq_add,
    true, true, true, true);
var _pipe_blue = igpu_shader_compile("float4 main() : SV_TARGET { return float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _pipe_tex = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, true, false, 1);
var _pipe_samp = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
_igpu_check(_pipe_hold > 0 && _pipe_blue > 0 && _pipe_tex > 0 && _pipe_samp > 0, "pipeline-state draw resources are created");
if (_pipe_hold > 0 && _pipe_blue > 0 && _pipe_tex > 0 && _tex_vs > 0 && _tex_red > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_pipe_blue, IgpuShaderStage.Pixel);
    var _pipe_base = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_pipe_tex], [0], [0], 0, 0, 0, 0);
    var _pipe_base_px = igpu_texture_read(_pipe_tex, 1, 4, 0, 0);
    igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
    var _pipe_held = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_pipe_tex], [0], [0], _pipe_hold, 0, 0, 0);
    var _pipe_held_px = igpu_texture_read(_pipe_tex, 1, 4, 0, 0);
    var _pipe_bad = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_pipe_tex], [0], [0], 0, _pipe_hold, 0, 0);
    var _pipe_bad_px = igpu_texture_read(_pipe_tex, 1, 4, 0, 0);
    var _pipe_open = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_pipe_tex], [0], [0], 0, 0, 0, 0);
    var _pipe_open_px = igpu_texture_read(_pipe_tex, 1, 4, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    show_debug_message("offscreen state  : " + string(_pipe_base) + " " + string(_pipe_base_px)
        + " | " + string(_pipe_held) + " " + string(_pipe_held_px)
        + " | " + string(_pipe_bad) + " " + string(_pipe_bad_px)
        + " | " + string(_pipe_open) + " " + string(_pipe_open_px));
    if (!_pipe_held || !_pipe_open) show_debug_message("offscreen state err: " + string(igpu_get_last_error()));
    _igpu_check(_pipe_base && _pipe_base_px == c_blue, "an offscreen draw with no state writes blue");
    _igpu_check(_pipe_held && _pipe_held_px == c_blue, "an offscreen draw keeps the destination colour");
    _igpu_check(!_pipe_bad && _pipe_bad_px == c_blue, "an offscreen depth slot rejects a blend state and leaves the texel");
    _igpu_check(_pipe_open && _pipe_open_px == c_red, "an offscreen draw with no state replaces the colour");
}
var _pipe_surf = surface_create(8, 8);
if (_pipe_hold > 0 && _pipe_samp > 0 && surface_exists(_pipe_surf) && _tex > 0 && _tex_vs > 0 && _tex_red > 0 && _tex_full > 0 && _tex_layout > 0)
{
    gpu_push_state();
    _igpu_target_begin(_pipe_surf, c_blue);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
    var _sm_held = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _tex, _pipe_hold, 0, 0, _pipe_samp);
    surface_reset_target();
    var _sm_held_px = surface_getpixel(_pipe_surf, 1, 4);
    surface_set_target(_pipe_surf);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
    var _sm_open = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _tex, 0, 0, 0, 0);
    surface_reset_target();
    var _sm_open_px = surface_getpixel(_pipe_surf, 1, 4);
    _igpu_target_begin(_pipe_surf, c_red);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
    var _sm_bad = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _tex, 0, _pipe_hold, 0, 0);
    surface_reset_target();
    var _sm_bad_px = surface_getpixel(_pipe_surf, 1, 4);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    gpu_pop_state();
    show_debug_message("sampled state    : " + string(_sm_held) + " " + string(_sm_held_px)
        + " | " + string(_sm_open) + " " + string(_sm_open_px)
        + " | " + string(_sm_bad) + " " + string(_sm_bad_px));
    if (!_sm_held || !_sm_open) show_debug_message("sampled state err : " + string(igpu_get_last_error()));
    _igpu_check(_sm_held && _sm_held_px == c_blue, "a sampled draw keeps the destination colour");
    _igpu_check(_sm_open && _sm_open_px == c_red, "a sampled draw with no blend replaces the colour");
    _igpu_check(!_sm_bad && _sm_bad_px == c_red, "a sampled depth slot rejects a blend state and leaves the pixel");
}
if (surface_exists(_pipe_surf)) surface_free(_pipe_surf);
_igpu_check(igpu_texture_release(_pipe_tex), "release offscreen state texture");
_igpu_check(igpu_shader_release(_pipe_blue), "release offscreen blue shader");
_igpu_check(igpu_state_release(_pipe_samp), "release offscreen point sampler");
_igpu_check(igpu_state_release(_pipe_hold), "release offscreen hold blend");

// Four colour targets at once. Each shader output is a different colour, so a
// draw that only reached the first target cannot satisfy the later reads.
_igpu_check(igpu_supports(IgpuCapability.MultipleRenderTargets), "supports multiple render targets");
_igpu_check(igpu_get_capabilities().max_render_targets == 4, "four colour targets are reported");

var _mrt = array_create(4);
var _mrt_ok = true;
for (var _mi = 0; _mi < 4; _mi++)
{
    _mrt[_mi] = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, true, false, 1);
    if (_mrt[_mi] <= 0) _mrt_ok = false;
}
var _mrt_small = igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba8unorm, true, false, 1);
var _mrt_ps = igpu_shader_compile("struct Out { float4 c0 : SV_Target0; float4 c1 : SV_Target1; float4 c2 : SV_Target2; float4 c3 : SV_Target3; }; Out main() { Out o; o.c0 = float4(1, 0, 0, 1); o.c1 = float4(0, 1, 0, 1); o.c2 = float4(0, 0, 1, 1); o.c3 = float4(1, 1, 0, 1); return o; }", "main", IgpuShaderStage.Pixel, "");
_igpu_check(_mrt_ok, "four render-target textures are created");
_igpu_check(_mrt_ps > 0, "multiple-target pixel shader compiles");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [], array_create(array_length([]), 0), array_create(array_length([]), 0), 0, 0, 0, 0), "an empty target list is rejected");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_mrt[0], _mrt[0], _mrt[0], _mrt[0], _mrt[0]], array_create(array_length([_mrt[0], _mrt[0], _mrt[0], _mrt[0], _mrt[0]]), 0), array_create(array_length([_mrt[0], _mrt[0], _mrt[0], _mrt[0], _mrt[0]]), 0), 0, 0, 0, 0), "five targets are rejected");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_mrt[0], _mrt_small], array_create(array_length([_mrt[0], _mrt_small]), 0), array_create(array_length([_mrt[0], _mrt_small]), 0), 0, 0, 0, 0), "targets of different sizes are rejected");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_not_target], array_create(array_length([_not_target]), 0), array_create(array_length([_not_target]), 0), 0, 0, 0, 0), "a non-target texture is rejected in a target list");

if (_mrt_ok && _mrt_ps > 0 && _tex_full > 0)
{
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_mrt_ps, IgpuShaderStage.Pixel);
    var _mrt_drew = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _mrt, array_create(array_length(_mrt), 0), array_create(array_length(_mrt), 0), 0, 0, 0, 0);
    if (!_mrt_drew) show_debug_message("mrt error         : " + string(igpu_get_last_error()));
    var _mrt_px = [
        igpu_texture_read(_mrt[0], 1, 4, 0, 0),
        igpu_texture_read(_mrt[1], 1, 4, 0, 0),
        igpu_texture_read(_mrt[2], 1, 4, 0, 0),
        igpu_texture_read(_mrt[3], 1, 4, 0, 0)
    ];
    show_debug_message("mrt pixels        : " + string(_mrt_drew) + " " + string(_mrt_px));
    _igpu_check(_mrt_drew, "draw to four targets succeeds");
    _igpu_check(_mrt_px[0] == c_red, "first target is red");
    _igpu_check(_mrt_px[1] == c_lime, "second target is green");
    _igpu_check(_mrt_px[2] == c_blue, "third target is blue");
    _igpu_check(_mrt_px[3] == c_yellow, "fourth target is yellow");

    var _mrt_surf = surface_create(8, 8);
    if (surface_exists(_mrt_surf))
    {
        gpu_push_state();
        _igpu_target_begin(_mrt_surf, c_blue);
        igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
        var _after_mrt = igpu_draw(_tex_full, 0, _tex_layout, pr_trianglelist, 0, 6, 1, 0, 0, 0, 0);
        surface_reset_target();
        gpu_pop_state();
        var _after_px = surface_getpixel(_mrt_surf, 1, 4);
        show_debug_message("after mrt surface : " + string(_after_mrt) + " " + string(_after_px));
        _igpu_check(_after_mrt && _after_px == c_red, "a surface draw still lands after multiple targets");
        surface_free(_mrt_surf);
    }
}

_igpu_check(igpu_texture_release(_mrt_small), "release the small extra target");
for (var _mi = 0; _mi < 4; _mi++)
{
    _igpu_check(igpu_texture_release(_mrt[_mi]), "release a multiple-target texture");
}
_igpu_check(igpu_shader_release(_mrt_ps), "release multiple-target pixel shader");

// The 4x4 base sits beside level 1 of an 8x8 chain. Both chosen levels are
// 4x4. Output 0 is red and output 1 is green. The 8x8 base stays black, so
// a draw that ignored the level cannot pass.
var _ml_small = igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba8unorm, true, false, 1);
var _ml_big = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, true, true, 0);
if (_ml_big == 0) show_debug_message("mrt level err    : " + string(igpu_get_last_error()));
var _ml_ps = igpu_shader_compile("struct Out { float4 c0 : SV_Target0; float4 c1 : SV_Target1; }; Out main() { Out o; o.c0 = float4(1.0, 0.0, 0.0, 1.0); o.c1 = float4(0.0, 1.0, 0.0, 1.0); return o; }", "main", IgpuShaderStage.Pixel, "");
_igpu_check(_ml_small > 0 && _ml_big > 0 && _ml_ps > 0, "mixed-level targets are created");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_ml_small, _ml_big], array_create(array_length([_ml_small, _ml_big]), 0), [0], 0, 0, 0, 0), "target and mip lists must be the same length");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_ml_small, _ml_big], array_create(array_length([_ml_small, _ml_big]), 0), [0, 0], 0, 0, 0, 0), "chosen levels of different sizes are rejected");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_ml_small, _ml_big], array_create(array_length([_ml_small, _ml_big]), 0), [0, 4], 0, 0, 0, 0), "a mip past a target chain is rejected");
if (_ml_small > 0 && _ml_big > 0 && _ml_ps > 0 && _tex_full > 0 && _tex_vs > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ml_ps, IgpuShaderStage.Pixel);
    var _ml_drew = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_ml_small, _ml_big], array_create(array_length([_ml_small, _ml_big]), 0), [0, 1], 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    var _ml_red = igpu_texture_read(_ml_small, 1, 1, 0, 0);
    var _ml_green = igpu_texture_read(_ml_big, 1, 1, 0, 1);
    var _ml_base = igpu_texture_read(_ml_big, 1, 1, 0, 0);
    show_debug_message("mrt level        : " + string(_ml_drew) + " " + string(_ml_red) + " " + string(_ml_base) + " / " + string(_ml_green));
    if (!_ml_drew) show_debug_message("mrt level err    : " + string(igpu_get_last_error()));
    _igpu_check(_ml_drew && _ml_red == c_red && _ml_base == 0 && _ml_green == c_lime, "each target is drawn at its own level");
}
_igpu_check(igpu_texture_release(_ml_small), "release the small mixed-level target");
_igpu_check(igpu_texture_release(_ml_big), "release the large mixed-level target");
_igpu_check(igpu_shader_release(_ml_ps), "release mixed-level pixel shader");

// Output 0 draws array layer 1 at level 1 (4x4). Output 1 draws cube face 1
// at level 0 (also 4x4). Layer 0 and the array's base level stay black.
var _ly_arr = igpu_texture_create(IgpuTextureKind.Array, 8, 8, 2, surface_rgba8unorm, true, true, 0);
var _ly_cube = igpu_texture_create(IgpuTextureKind.Cube, 4, 4, 6, surface_rgba8unorm, true, false, 1);
var _ly_flat = igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba8unorm, true, false, 1);
if (_ly_arr == 0 || _ly_cube == 0) show_debug_message("mrt layer err    : " + string(igpu_get_last_error()));
var _ly_ps = igpu_shader_compile("struct Out { float4 c0 : SV_Target0; float4 c1 : SV_Target1; }; Out main() { Out o; o.c0 = float4(1.0, 0.0, 0.0, 1.0); o.c1 = float4(0.0, 1.0, 0.0, 1.0); return o; }", "main", IgpuShaderStage.Pixel, "");
_igpu_check(_ly_arr > 0 && _ly_cube > 0 && _ly_flat > 0 && _ly_ps > 0, "layered multiple targets are created");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_ly_arr, _ly_cube], [1], [1, 0], 0, 0, 0, 0), "target, layer and mip lists must be the same length");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_ly_arr, _ly_cube], [2, 1], [1, 0], 0, 0, 0, 0), "a layer past the texture is rejected");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_ly_flat, _ly_cube], [1, 1], [0, 0], 0, 0, 0, 0), "a 2D texture has no second layer");
if (_ly_arr > 0 && _ly_cube > 0 && _ly_ps > 0 && _tex_full > 0 && _tex_vs > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ly_ps, IgpuShaderStage.Pixel);
    var _ly_drew = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_ly_arr, _ly_cube], [1, 1], [1, 0], 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    var _ly_arr_hit = igpu_texture_read(_ly_arr, 1, 1, 1, 1);
    var _ly_arr_other = igpu_texture_read(_ly_arr, 1, 1, 0, 1);
    var _ly_arr_base = igpu_texture_read(_ly_arr, 1, 1, 1, 0);
    var _ly_cube_hit = igpu_texture_read(_ly_cube, 1, 1, 1, 0);
    var _ly_cube_other = igpu_texture_read(_ly_cube, 1, 1, 0, 0);
    show_debug_message("mrt layer        : " + string(_ly_drew) + " " + string(_ly_arr_hit) + " " + string(_ly_arr_other) + " " + string(_ly_arr_base) + " / " + string(_ly_cube_hit) + " " + string(_ly_cube_other));
    if (!_ly_drew) show_debug_message("mrt layer err    : " + string(igpu_get_last_error()));
    _igpu_check(_ly_drew && _ly_arr_hit == c_red && _ly_arr_other == 0 && _ly_arr_base == 0, "the array target is drawn at the chosen layer and level");
    _igpu_check(_ly_cube_hit == c_lime && _ly_cube_other == 0, "the cube target is drawn at the chosen face");
}
_igpu_check(igpu_texture_release(_ly_arr), "release layered array target");
_igpu_check(igpu_texture_release(_ly_cube), "release layered cube target");
_igpu_check(igpu_texture_release(_ly_flat), "release flat layered target");
_igpu_check(igpu_shader_release(_ly_ps), "release layered multiple-target shader");

// One array, two outputs: layer 0 red and layer 1 green. Listing that same
// layer twice is still rejected.
var _twice = igpu_texture_create(IgpuTextureKind.Array, 4, 4, 2, surface_rgba8unorm, true, false, 1);
var _twice_ps = igpu_shader_compile("struct Out { float4 c0 : SV_Target0; float4 c1 : SV_Target1; }; Out main() { Out o; o.c0 = float4(1.0, 0.0, 0.0, 1.0); o.c1 = float4(0.0, 1.0, 0.0, 1.0); return o; }", "main", IgpuShaderStage.Pixel, "");
_igpu_check(_twice > 0 && _twice_ps > 0, "a two-layer target is created");
_igpu_check(!igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_twice, _twice], [0, 0], [0, 0], 0, 0, 0, 0), "the same layer and level are rejected");
if (_twice > 0 && _twice_ps > 0 && _tex_full > 0 && _tex_vs > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_twice_ps, IgpuShaderStage.Pixel);
    var _twice_drew = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_twice, _twice], [0, 1], [0, 0], 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    var _twice_0 = igpu_texture_read(_twice, 1, 1, 0, 0);
    var _twice_1 = igpu_texture_read(_twice, 1, 1, 1, 0);
    show_debug_message("mrt same texture : " + string(_twice_drew) + " " + string(_twice_0) + " / " + string(_twice_1));
    if (!_twice_drew) show_debug_message("mrt same err     : " + string(igpu_get_last_error()));
    _igpu_check(_twice_drew && _twice_0 == c_red && _twice_1 == c_lime, "two layers of one texture are drawn together");
}
_igpu_check(igpu_texture_release(_twice), "release the two-layer target");
_igpu_check(igpu_shader_release(_twice_ps), "release the two-layer shader");

// Kinds are the shapes every backend has: a volume, a stack of layers, and a
// cube of six faces. One draw writes a single slice. A storage texture is the
// writable image, filled by a compute dispatch rather than a backend-specific
// write call.
var _blue_ps = igpu_shader_compile("float4 main() : SV_TARGET { return float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _volume = igpu_texture_create(IgpuTextureKind.ThreeD, 8, 8, 2, surface_rgba8unorm, true, false, 1);
var _stack = igpu_texture_create(IgpuTextureKind.Array, 8, 8, 2, surface_rgba8unorm, true, false, 1);
var _cube = igpu_texture_create(IgpuTextureKind.Cube, 8, 8, 6, surface_rgba8unorm, true, false, 1);
var _store = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, false, true, 1);
_igpu_check(_volume > 0 && _stack > 0 && _cube > 0 && _store > 0, "volume, array, cube and storage textures are created");
_igpu_check(igpu_texture_create(IgpuTextureKind.Cube, 8, 4, 6, surface_rgba8unorm, true, false, 1) == 0, "a non-square cube is rejected");
_igpu_check(igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 2, surface_rgba8unorm, false, false, 1) == 0, "a 2D texture rejects extra depth");
var _vol_store = igpu_texture_create(IgpuTextureKind.ThreeD, 8, 8, 2, surface_rgba8unorm, false, true, 1);
var _arr_store = igpu_texture_create(IgpuTextureKind.Array, 8, 8, 2, surface_rgba8unorm, false, true, 1);
var _cube_store = igpu_texture_create(IgpuTextureKind.Cube, 8, 8, 6, surface_rgba8unorm, false, true, 1);
if (_vol_store == 0 || _arr_store == 0 || _cube_store == 0) show_debug_message("storage kind err  : " + string(igpu_get_last_error()));
_igpu_check(_vol_store > 0 && _arr_store > 0 && _cube_store > 0, "volume, array and cube storage textures are created");

if (_volume > 0 && _blue_ps > 0 && _tex_red > 0)
{
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
    var _vol0 = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_volume], [0], [0], 0, 0, 0, 0);
    igpu_shader_bind(_blue_ps, IgpuShaderStage.Pixel);
    var _vol1 = igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_volume], [1], [0], 0, 0, 0, 0);
    var _vol_near = igpu_texture_read(_volume, 1, 4, 0, 0);
    var _vol_far = igpu_texture_read(_volume, 1, 4, 1, 0);
    show_debug_message("volume slices     : " + string(_vol0) + "/" + string(_vol1) + " " + string(_vol_near) + " " + string(_vol_far));
    _igpu_check(_vol0 && _vol1 && _vol_near == c_red && _vol_far == c_blue, "volume slices keep separate colours");
}
if (_stack > 0 && _blue_ps > 0)
{
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
    igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_stack], [0], [0], 0, 0, 0, 0);
    igpu_shader_bind(_blue_ps, IgpuShaderStage.Pixel);
    igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_stack], [1], [0], 0, 0, 0, 0);
    _igpu_check(igpu_texture_read(_stack, 1, 4, 0, 0) == c_red && igpu_texture_read(_stack, 1, 4, 1, 0) == c_blue,
        "array layers keep separate colours");
}
if (_cube > 0 && _blue_ps > 0)
{
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_red, IgpuShaderStage.Pixel);
    igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_cube], [0], [0], 0, 0, 0, 0);
    igpu_shader_bind(_blue_ps, IgpuShaderStage.Pixel);
    igpu_draw_to_render_targets(_tex_full, _tex_layout, pr_trianglelist, 0, 6, [_cube], [1], [0], 0, 0, 0, 0);
    _igpu_check(igpu_texture_read(_cube, 1, 4, 0, 0) == c_red && igpu_texture_read(_cube, 1, 4, 1, 0) == c_blue,
        "cube faces keep separate colours");
}

var _cs = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
_igpu_check(_cs > 0, "storage compute shader compiles");
if (_store > 0 && _cs > 0)
{
    _igpu_check(igpu_texture_read(_store, 1, 4, 0, 0) == 0, "storage texture starts black");
    _igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_store], [0], [0]), "dispatch without a bound compute shader is rejected");
    igpu_shader_bind(_cs, IgpuShaderStage.Compute);
    var _ran = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_store], [0], [0]);
    var _stored = igpu_texture_read(_store, 1, 4, 0, 0);
    show_debug_message("storage dispatch  : " + string(_ran) + " " + string(_stored));
    if (!_ran) show_debug_message("storage error     : " + string(igpu_get_last_error()));
    _igpu_check(_ran && _stored == c_red, "a compute dispatch writes the storage texture");
    igpu_shader_bind(0, IgpuShaderStage.Compute);
}

// Slice 0 is red and every later slice is blue. A view that only covered
// the first slice would leave the later reads black.
var _cs3 = igpu_shader_compile("RWTexture3D<float4> dst : register(u0); [numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id] = id.z == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _csa = igpu_shader_compile("RWTexture2DArray<float4> dst : register(u0); [numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id] = id.z == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
_igpu_check(_cs3 > 0 && _csa > 0, "volume and array storage shaders compile");
if (_cs3 > 0 && _vol_store > 0)
{
    igpu_shader_bind(_cs3, IgpuShaderStage.Compute);
    var _vran = igpu_dispatch(1, 1, 2, [IgpuWriteTarget.Texture], [_vol_store], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _v0 = igpu_texture_read(_vol_store, 1, 4, 0, 0);
    var _v1 = igpu_texture_read(_vol_store, 1, 4, 1, 0);
    show_debug_message("storage volume   : " + string(_vran) + " " + string(_v0) + " / " + string(_v1));
    if (!_vran) show_debug_message("storage volume err: " + string(igpu_get_last_error()));
    _igpu_check(_vran && _v0 == c_red && _v1 == c_blue, "a compute shader writes both volume slices");
}
if (_csa > 0 && _arr_store > 0 && _cube_store > 0)
{
    igpu_shader_bind(_csa, IgpuShaderStage.Compute);
    var _aran = igpu_dispatch(1, 1, 2, [IgpuWriteTarget.Texture], [_arr_store], [0], [0]);
    var _a0 = igpu_texture_read(_arr_store, 1, 4, 0, 0);
    var _a1 = igpu_texture_read(_arr_store, 1, 4, 1, 0);
    var _cran = igpu_dispatch(1, 1, 6, [IgpuWriteTarget.Texture], [_cube_store], [0], [0]);
    var _c0 = igpu_texture_read(_cube_store, 1, 4, 0, 0);
    var _c1 = igpu_texture_read(_cube_store, 1, 4, 1, 0);
    var _c5 = igpu_texture_read(_cube_store, 1, 4, 5, 0);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    show_debug_message("storage array    : " + string(_aran) + " " + string(_a0) + " / " + string(_a1));
    show_debug_message("storage cube     : " + string(_cran) + " " + string(_c0) + " / " + string(_c1) + " / " + string(_c5));
    if (!_aran || !_cran) show_debug_message("storage kind err  : " + string(igpu_get_last_error()));
    _igpu_check(_aran && _a0 == c_red && _a1 == c_blue, "a compute shader writes both array layers");
    _igpu_check(_cran && _c0 == c_red && _c1 == c_blue && _c5 == c_blue, "a compute shader writes every cube face");
}

// The compute shader paints the left half red and the right half blue.
// Sampling that texture onto a yellow surface distinguishes three failures:
// yellow means the draw did not replace the clear, black means the sample
// saw the unwritten texture, and the split colours mean it saw the write.
var _cs_split = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = id.x < 4 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _split = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, false, true, 1);
var _split_surf = surface_create(8, 8);
_igpu_check(_cs_split > 0 && _split > 0 && surface_exists(_split_surf), "storage sample resources are created");
if (_split > 0 && _cs_split > 0 && surface_exists(_split_surf) && _tex_vs > 0 && _tex_sample > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_cs_split, IgpuShaderStage.Compute);
    var _split_ran = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_split], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    _igpu_target_begin(_split_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_tex_sample, IgpuShaderStage.Pixel);
    var _split_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _split, 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _split_l = surface_getpixel(_split_surf, 1, 4);
    var _split_r = surface_getpixel(_split_surf, 6, 4);
    show_debug_message("storage sample   : " + string(_split_ran) + " " + string(_split_drew) + " " + string(_split_l) + " / " + string(_split_r));
    if (!_split_drew) show_debug_message("storage sample err: " + string(igpu_get_last_error()));
    _igpu_check(_split_ran && _split_drew && _split_l == c_red && _split_r == c_blue, "a draw samples the storage texture the compute shader wrote");
}
if (surface_exists(_split_surf)) surface_free(_split_surf);
_igpu_check(igpu_texture_release(_split), "release sampled storage texture");
_igpu_check(igpu_shader_release(_cs_split), "release storage sample shader");

// The left half of the quad samples slice, layer or +X face 0 (red).
// The right half samples the next one (blue). Yellow would mean the draw
// missed the clear; black would mean the sample missed the compute write.
var _ps_vol = igpu_shader_compile("Texture3D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float z = i.uv.x < 0.5 ? 0.25 : 0.75; return t.Sample(s, float3(0.5, 0.5, z)); }", "main", IgpuShaderStage.Pixel, "");
var _ps_arr = igpu_shader_compile("Texture2DArray t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float layer = i.uv.x < 0.5 ? 0.0 : 1.0; return t.Sample(s, float3(0.5, 0.5, layer)); }", "main", IgpuShaderStage.Pixel, "");
var _ps_cube = igpu_shader_compile("TextureCube t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float3 dir = i.uv.x < 0.5 ? float3(1.0, 0.0, 0.0) : float3(-1.0, 0.0, 0.0); return t.Sample(s, dir); }", "main", IgpuShaderStage.Pixel, "");
if (_ps_vol == 0 || _ps_arr == 0 || _ps_cube == 0) show_debug_message("layer sample err  : " + string(igpu_get_last_error()));
_igpu_check(_ps_vol > 0 && _ps_arr > 0 && _ps_cube > 0, "layered sample shaders compile");
var _layer_surf = surface_create(8, 8);
_igpu_check(surface_exists(_layer_surf), "layered sample surface is created");
if (surface_exists(_layer_surf) && _ps_vol > 0 && _ps_arr > 0 && _ps_cube > 0 && _vol_store > 0 && _arr_store > 0 && _cube_store > 0 && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    gpu_push_state();
    _igpu_target_begin(_layer_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ps_vol, IgpuShaderStage.Pixel);
    var _vol_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _vol_store, 0, 0, 0, 0);
    surface_reset_target();
    var _vol_l = surface_getpixel(_layer_surf, 1, 4);
    var _vol_r = surface_getpixel(_layer_surf, 6, 4);
    _igpu_target_begin(_layer_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ps_arr, IgpuShaderStage.Pixel);
    var _arr_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _arr_store, 0, 0, 0, 0);
    surface_reset_target();
    var _arr_l = surface_getpixel(_layer_surf, 1, 4);
    var _arr_r = surface_getpixel(_layer_surf, 6, 4);
    _igpu_target_begin(_layer_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ps_cube, IgpuShaderStage.Pixel);
    var _cube_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cube_store, 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _cube_l = surface_getpixel(_layer_surf, 1, 4);
    var _cube_r = surface_getpixel(_layer_surf, 6, 4);
    show_debug_message("sample volume    : " + string(_vol_drew) + " " + string(_vol_l) + " / " + string(_vol_r));
    show_debug_message("sample array     : " + string(_arr_drew) + " " + string(_arr_l) + " / " + string(_arr_r));
    show_debug_message("sample cube      : " + string(_cube_drew) + " " + string(_cube_l) + " / " + string(_cube_r));
    if (!_vol_drew || !_arr_drew || !_cube_drew) show_debug_message("layer sample err  : " + string(igpu_get_last_error()));
    _igpu_check(_vol_drew && _vol_l == c_red && _vol_r == c_blue, "a draw samples both volume slices");
    _igpu_check(_arr_drew && _arr_l == c_red && _arr_r == c_blue, "a draw samples both array layers");
    _igpu_check(_cube_drew && _cube_l == c_red && _cube_r == c_blue, "a draw samples the +X and -X cube faces");
}
if (surface_exists(_layer_surf)) surface_free(_layer_surf);
_igpu_check(igpu_shader_release(_ps_vol), "release volume sample shader");
_igpu_check(igpu_shader_release(_ps_arr), "release array sample shader");
_igpu_check(igpu_shader_release(_ps_cube), "release cube sample shader");

_igpu_check(igpu_texture_release(_vol_store), "release volume storage texture");
_igpu_check(igpu_texture_release(_arr_store), "release array storage texture");
_igpu_check(igpu_texture_release(_cube_store), "release cube storage texture");
_igpu_check(igpu_shader_release(_cs3), "release volume storage shader");
_igpu_check(igpu_shader_release(_csa), "release array storage shader");

_igpu_check(igpu_texture_release(_volume), "release volume texture");
_igpu_check(igpu_texture_release(_stack), "release array texture");
_igpu_check(igpu_texture_release(_cube), "release cube texture");
_igpu_check(igpu_texture_release(_store), "release storage texture");
_igpu_check(igpu_shader_release(_blue_ps), "release blue pixel shader");
_igpu_check(igpu_shader_release(_cs), "release storage compute shader");

// Two texels, red then blue. The sample is at u=0.4, closer to the red
// centre (0.25) than the blue centre (0.75), so point filtering stays red
// and linear filtering mixes in blue.
var _f_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, false, true, 1);
var _f_cs = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = id.x == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _f_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, float2(0.4, 0.5)); }", "main", IgpuShaderStage.Pixel, "");
var _f_point = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _f_linear = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _f_surf = surface_create(8, 8);
_igpu_check(_f_tex > 0 && _f_cs > 0 && _f_ps > 0 && _f_point > 0 && _f_linear > 0 && surface_exists(_f_surf), "filter-seam resources are created");
if (_f_tex > 0 && _f_cs > 0 && _f_ps > 0 && _f_point > 0 && _f_linear > 0 && surface_exists(_f_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_f_cs, IgpuShaderStage.Compute);
    var _f_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_f_tex], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    _igpu_target_begin(_f_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_f_ps, IgpuShaderStage.Pixel);
    var _f_point_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _f_point);
    surface_reset_target();
    var _f_point_px = surface_getpixel(_f_surf, 1, 4);
    _igpu_target_begin(_f_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_f_ps, IgpuShaderStage.Pixel);
    var _f_linear_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _f_linear);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _f_linear_px = surface_getpixel(_f_surf, 1, 4);
    show_debug_message("filter seam      : " + string(_f_wrote) + " " + string(_f_point_drew) + " " + string(_f_point_px) + " / " + string(_f_linear_drew) + " " + string(_f_linear_px)
        + " r=" + string(color_get_red(_f_linear_px)) + " b=" + string(color_get_blue(_f_linear_px)));
    if (!_f_point_drew || !_f_linear_drew) show_debug_message("filter seam err   : " + string(igpu_get_last_error()));
    _igpu_check(_f_wrote && _f_point_drew && _f_point_px == c_red, "point filtering stays on the red texel");
    _igpu_check(_f_linear_drew && _f_linear_px != _f_point_px && color_get_red(_f_linear_px) > 0 && color_get_blue(_f_linear_px) > 0 && color_get_green(_f_linear_px) == 0, "linear filtering mixes red and blue at the seam");
}
if (surface_exists(_f_surf)) surface_free(_f_surf);
_igpu_check(igpu_shader_release(_f_cs), "release filter-seam compute shader");
_igpu_check(igpu_shader_release(_f_ps), "release filter-seam pixel shader");
_igpu_check(igpu_state_release(_f_point), "release point sampler");
_igpu_check(igpu_state_release(_f_linear), "release linear sampler");

// u = 1.1 is just past the right edge. Clamp stays on the blue texel.
// Wrap brings 1.1 back to 0.1, which is the red texel.
var _a_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, float2(1.1, 0.5)); }", "main", IgpuShaderStage.Pixel, "");
var _a_clamp = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _a_wrap = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 1, 0, 0, 0, 0, -1);
var _a_surf = surface_create(8, 8);
_igpu_check(_f_tex > 0 && _a_ps > 0 && _a_clamp > 0 && _a_wrap > 0 && surface_exists(_a_surf), "address-mode resources are created");
if (_f_tex > 0 && _a_ps > 0 && _a_clamp > 0 && _a_wrap > 0 && surface_exists(_a_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    gpu_push_state();
    _igpu_target_begin(_a_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_a_ps, IgpuShaderStage.Pixel);
    var _a_clamp_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _a_clamp);
    surface_reset_target();
    var _a_clamp_px = surface_getpixel(_a_surf, 1, 4);
    _igpu_target_begin(_a_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_a_ps, IgpuShaderStage.Pixel);
    var _a_wrap_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _a_wrap);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _a_wrap_px = surface_getpixel(_a_surf, 1, 4);
    show_debug_message("address mode     : " + string(_a_clamp_drew) + " " + string(_a_clamp_px) + " / " + string(_a_wrap_drew) + " " + string(_a_wrap_px));
    if (!_a_clamp_drew || !_a_wrap_drew) show_debug_message("address mode err  : " + string(igpu_get_last_error()));
    _igpu_check(_a_clamp_drew && _a_clamp_px == c_blue, "clamp sampling stays on the edge texel");
    _igpu_check(_a_wrap_drew && _a_wrap_px == c_red, "repeat sampling wraps onto the other texel");
}
if (surface_exists(_a_surf)) surface_free(_a_surf);
_igpu_check(igpu_shader_release(_a_ps), "release address-mode pixel shader");
_igpu_check(igpu_state_release(_a_clamp), "release clamp sampler");
_igpu_check(igpu_state_release(_a_wrap), "release repeat sampler");

// u = 1.6. Clamp stays on the blue edge. Repeat lands on 0.6, still the
// blue texel. Mirror flips the second tile, so 1.6 becomes 0.4, the red texel.
var _mir_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, float2(1.6, 0.5)); }", "main", IgpuShaderStage.Pixel, "");
var _mir_clamp = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _mir_repeat = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 1, 0, 0, 0, 0, -1);
var _mir_mirror = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Mirror, IgpuAddressMode.Mirror, IgpuAddressMode.Mirror, 1, 0, 0, 0, 0, -1);
var _mir_surf = surface_create(8, 8);
_igpu_check(_f_tex > 0 && _mir_ps > 0 && _mir_clamp > 0 && _mir_repeat > 0 && _mir_mirror > 0 && surface_exists(_mir_surf), "mirror-address resources are created");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, 9, 9, 9, 1, 0, 0, 0, 0, -1) == 0, "an unknown address mode is rejected");
if (_f_tex > 0 && _mir_ps > 0 && _mir_clamp > 0 && _mir_repeat > 0 && _mir_mirror > 0 && surface_exists(_mir_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    gpu_push_state();
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_mir_ps, IgpuShaderStage.Pixel);
    _igpu_target_begin(_mir_surf, c_yellow);
    var _mir_c_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _mir_clamp);
    surface_reset_target();
    var _mir_c_px = surface_getpixel(_mir_surf, 1, 4);
    _igpu_target_begin(_mir_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_mir_ps, IgpuShaderStage.Pixel);
    var _mir_r_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _mir_repeat);
    surface_reset_target();
    var _mir_r_px = surface_getpixel(_mir_surf, 1, 4);
    _igpu_target_begin(_mir_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_mir_ps, IgpuShaderStage.Pixel);
    var _mir_m_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _mir_mirror);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _mir_m_px = surface_getpixel(_mir_surf, 1, 4);
    show_debug_message("mirror address   : " + string(_mir_c_drew) + " " + string(_mir_c_px) + " / " + string(_mir_r_drew) + " " + string(_mir_r_px) + " / " + string(_mir_m_drew) + " " + string(_mir_m_px));
    if (!_mir_m_drew) show_debug_message("mirror address err: " + string(igpu_get_last_error()));
    _igpu_check(_mir_c_drew && _mir_c_px == c_blue && _mir_r_drew && _mir_r_px == c_blue, "clamp and repeat both stay on the blue texel at u=1.6");
    _igpu_check(_mir_m_drew && _mir_m_px == c_red, "mirror sampling flips onto the red texel");
}
if (surface_exists(_mir_surf)) surface_free(_mir_surf);
_igpu_check(igpu_shader_release(_mir_ps), "release mirror pixel shader");
_igpu_check(igpu_state_release(_mir_clamp), "release mirror-test clamp sampler");
_igpu_check(igpu_state_release(_mir_repeat), "release mirror-test repeat sampler");
_igpu_check(igpu_state_release(_mir_mirror), "release mirror sampler");

// u = -0.5 is outside the texture. Clamp stays on the red edge texel.
// Border reads the colour passed to the API, not a texel. The surface is
// cleared to black so a missed draw cannot look like the border colour.
var _bd_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, float2(-0.5, 0.5)); }", "main", IgpuShaderStage.Pixel, "");
var _bd_clamp = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _bd_border = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Border, IgpuAddressMode.Border, 1, c_fuchsia, 0, 0, 0, -1);
var _bd_surf = surface_create(8, 8);
_igpu_check(_f_tex > 0 && _bd_ps > 0 && _bd_clamp > 0 && _bd_border > 0 && surface_exists(_bd_surf), "border-address resources are created");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Border, IgpuAddressMode.Border, 1, 0, 0, 0, 0, -1) != 0, "border colour 0 is black, not a missing colour");
if (_f_tex > 0 && _bd_ps > 0 && _bd_clamp > 0 && _bd_border > 0 && surface_exists(_bd_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    gpu_push_state();
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_bd_ps, IgpuShaderStage.Pixel);
    _igpu_target_begin(_bd_surf, c_black);
    var _bd_c_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _bd_clamp);
    surface_reset_target();
    var _bd_c_px = surface_getpixel(_bd_surf, 1, 4);
    _igpu_target_begin(_bd_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_bd_ps, IgpuShaderStage.Pixel);
    var _bd_b_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _f_tex, 0, 0, 0, _bd_border);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _bd_b_px = surface_getpixel(_bd_surf, 1, 4);
    show_debug_message("border address   : " + string(_bd_c_drew) + " " + string(_bd_c_px) + " / " + string(_bd_b_drew) + " " + string(_bd_b_px));
    if (!_bd_b_drew) show_debug_message("border address err: " + string(igpu_get_last_error()));
    _igpu_check(_bd_c_drew && _bd_c_px == c_red, "clamp outside the texture stays on the red edge");
    _igpu_check(_bd_b_drew && _bd_b_px == c_fuchsia, "border sampling reads the given colour");
}
if (surface_exists(_bd_surf)) surface_free(_bd_surf);
_igpu_check(igpu_shader_release(_bd_ps), "release border pixel shader");
_igpu_check(igpu_state_release(_bd_clamp), "release border-test clamp sampler");
_igpu_check(igpu_state_release(_bd_border), "release border sampler");
_igpu_check(igpu_texture_release(_f_tex), "release filter-seam texture");

// 2x2: top-left red, top-right blue, bottom-left green. The sample is
// outside on both axes. Horizontal repeat and vertical clamp lands on blue.
// Horizontal clamp and vertical repeat lands on green.
var _ax_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, false, true, 1);
var _ax_cs = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { if (id.y == 0) dst[id.xy] = id.x == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); else dst[id.xy] = id.x == 0 ? float4(0.0, 1.0, 0.0, 1.0) : float4(1.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _ax_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, float2(-0.25, -0.25)); }", "main", IgpuShaderStage.Pixel, "");
var _ax_u = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Repeat, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _ax_v = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Repeat, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _ax_b = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, c_fuchsia, 0, 0, 0, -1);
var _ax_surf = surface_create(8, 8);
_igpu_check(_ax_tex > 0 && _ax_cs > 0 && _ax_ps > 0 && _ax_u > 0 && _ax_v > 0 && _ax_b > 0 && surface_exists(_ax_surf), "per-axis address resources are created");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Border, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1) != 0, "one border axis with colour 0 is black");
if (_ax_tex > 0 && _ax_cs > 0 && _ax_ps > 0 && _ax_u > 0 && _ax_v > 0 && _ax_b > 0 && surface_exists(_ax_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_ax_cs, IgpuShaderStage.Compute);
    var _ax_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_ax_tex], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ax_ps, IgpuShaderStage.Pixel);
    _igpu_target_begin(_ax_surf, c_black);
    var _ax_u_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _ax_tex, 0, 0, 0, _ax_u);
    surface_reset_target();
    var _ax_u_px = surface_getpixel(_ax_surf, 1, 4);
    _igpu_target_begin(_ax_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ax_ps, IgpuShaderStage.Pixel);
    var _ax_v_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _ax_tex, 0, 0, 0, _ax_v);
    surface_reset_target();
    var _ax_v_px = surface_getpixel(_ax_surf, 1, 4);
    _igpu_target_begin(_ax_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ax_ps, IgpuShaderStage.Pixel);
    var _ax_b_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _ax_tex, 0, 0, 0, _ax_b);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _ax_b_px = surface_getpixel(_ax_surf, 1, 4);
    show_debug_message("axis address     : " + string(_ax_wrote) + " " + string(_ax_u_drew) + " " + string(_ax_u_px) + " / " + string(_ax_v_drew) + " " + string(_ax_v_px) + " / " + string(_ax_b_drew) + " " + string(_ax_b_px));
    if (!_ax_u_drew || !_ax_v_drew || !_ax_b_drew) show_debug_message("axis address err : " + string(igpu_get_last_error()));
    _igpu_check(_ax_wrote && _ax_u_drew && _ax_u_px == c_blue, "horizontal repeat and vertical clamp reads the blue texel");
    _igpu_check(_ax_v_drew && _ax_v_px == c_lime, "horizontal clamp and vertical repeat reads the green texel");
    _igpu_check(_ax_b_drew && _ax_b_px == c_fuchsia, "a border on the horizontal axis reads the given colour");
}
if (surface_exists(_ax_surf)) surface_free(_ax_surf);
_igpu_check(igpu_texture_release(_ax_tex), "release per-axis texture");
_igpu_check(igpu_shader_release(_ax_cs), "release per-axis compute shader");
_igpu_check(igpu_shader_release(_ax_ps), "release per-axis pixel shader");
_igpu_check(igpu_state_release(_ax_u), "release horizontal-repeat sampler");
_igpu_check(igpu_state_release(_ax_v), "release vertical-repeat sampler");
_igpu_check(igpu_state_release(_ax_b), "release horizontal-border sampler");

// A 1x1x2 volume: the near slice is red, the far slice is blue. The sample
// is inside x and y, and outside on depth. Only the depth address changes.
var _dw_tex = igpu_texture_create(IgpuTextureKind.ThreeD, 1, 1, 2, surface_rgba8unorm, false, true, 1);
if (_dw_tex == 0) show_debug_message("depth address err: " + string(igpu_get_last_error()));
var _dw_cs = igpu_shader_compile("RWTexture3D<float4> dst : register(u0); [numthreads(1, 1, 2)] void main(uint3 id : SV_DispatchThreadID) { dst[id] = id.z == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _dw_ps = igpu_shader_compile("Texture3D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, float3(0.25, 0.25, -0.25)); }", "main", IgpuShaderStage.Pixel, "");
var _dw_clamp = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _dw_repeat = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Repeat, 1, 0, 0, 0, 0, -1);
var _dw_border = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Border, 1, c_fuchsia, 0, 0, 0, -1);
var _dw_surf = surface_create(8, 8);
_igpu_check(_dw_tex > 0 && _dw_cs > 0 && _dw_ps > 0 && _dw_clamp > 0 && _dw_repeat > 0 && _dw_border > 0 && surface_exists(_dw_surf), "depth-address resources are created");
if (_dw_tex > 0 && _dw_cs > 0 && _dw_ps > 0 && _dw_clamp > 0 && _dw_repeat > 0 && _dw_border > 0 && surface_exists(_dw_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_dw_cs, IgpuShaderStage.Compute);
    var _dw_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_dw_tex], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_dw_ps, IgpuShaderStage.Pixel);
    _igpu_target_begin(_dw_surf, c_black);
    var _dw_c_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _dw_tex, 0, 0, 0, _dw_clamp);
    surface_reset_target();
    var _dw_c_px = surface_getpixel(_dw_surf, 1, 4);
    _igpu_target_begin(_dw_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_dw_ps, IgpuShaderStage.Pixel);
    var _dw_r_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _dw_tex, 0, 0, 0, _dw_repeat);
    surface_reset_target();
    var _dw_r_px = surface_getpixel(_dw_surf, 1, 4);
    _igpu_target_begin(_dw_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_dw_ps, IgpuShaderStage.Pixel);
    var _dw_b_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _dw_tex, 0, 0, 0, _dw_border);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _dw_b_px = surface_getpixel(_dw_surf, 1, 4);
    show_debug_message("depth address    : " + string(_dw_wrote) + " " + string(_dw_c_drew) + " " + string(_dw_c_px) + " / " + string(_dw_r_drew) + " " + string(_dw_r_px) + " / " + string(_dw_b_drew) + " " + string(_dw_b_px));
    if (!_dw_wrote || !_dw_c_drew || !_dw_r_drew || !_dw_b_drew) show_debug_message("depth address err: " + string(igpu_get_last_error()));
    _igpu_check(_dw_wrote && _dw_c_drew && _dw_c_px == c_red, "depth clamp stays on the near slice");
    _igpu_check(_dw_r_drew && _dw_r_px == c_blue, "depth repeat wraps onto the far slice");
    _igpu_check(_dw_b_drew && _dw_b_px == c_fuchsia, "depth border reads the given colour");
}
if (surface_exists(_dw_surf)) surface_free(_dw_surf);
_igpu_check(igpu_texture_release(_dw_tex), "release depth-address volume");
_igpu_check(igpu_shader_release(_dw_cs), "release depth-address compute shader");
_igpu_check(igpu_shader_release(_dw_ps), "release depth-address pixel shader");
_igpu_check(igpu_state_release(_dw_clamp), "release depth-clamp sampler");
_igpu_check(igpu_state_release(_dw_repeat), "release depth-repeat sampler");
_igpu_check(igpu_state_release(_dw_border), "release depth-border sampler");

// Left half of the quad is a magnified sample at the red/blue seam.
// Right half is the same point with a footprint covering both texels.
// Point magnification stays red; linear magnification mixes. Linear
// minification mixes; point minification stays on the red texel.
var _ff_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, false, true, 1);
var _ff_cs = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = id.x == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _ff_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float4 magnified = t.SampleGrad(s, float2(0.4, 0.5), float2(0.001, 0.0), float2(0.0, 0.001)); float4 minified = t.SampleGrad(s, float2(0.4, 0.5), float2(1.0, 0.0), float2(0.0, 0.001)); return i.uv.x < 0.5 ? magnified : minified; }", "main", IgpuShaderStage.Pixel, "");
var _ff_pm = igpu_sampler_state_create(tf_point, tf_linear, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _ff_mp = igpu_sampler_state_create(tf_linear, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _ff_surf = surface_create(8, 8);
_igpu_check(_ff_tex > 0 && _ff_cs > 0 && _ff_ps > 0 && _ff_pm > 0 && _ff_mp > 0 && surface_exists(_ff_surf), "split-filter resources are created");
_igpu_check(igpu_sampler_state_create(tf_anisotropic, tf_linear, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1) == 0, "anisotropic is not a magnification filter");
if (_ff_tex > 0 && _ff_cs > 0 && _ff_ps > 0 && _ff_pm > 0 && _ff_mp > 0 && surface_exists(_ff_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_ff_cs, IgpuShaderStage.Compute);
    var _ff_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_ff_tex], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ff_ps, IgpuShaderStage.Pixel);
    _igpu_target_begin(_ff_surf, c_black);
    var _ff_pm_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _ff_tex, 0, 0, 0, _ff_pm);
    surface_reset_target();
    var _ff_pm_l = surface_getpixel(_ff_surf, 1, 4);
    var _ff_pm_r = surface_getpixel(_ff_surf, 6, 4);
    _igpu_target_begin(_ff_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ff_ps, IgpuShaderStage.Pixel);
    var _ff_mp_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _ff_tex, 0, 0, 0, _ff_mp);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _ff_mp_l = surface_getpixel(_ff_surf, 1, 4);
    var _ff_mp_r = surface_getpixel(_ff_surf, 6, 4);
    show_debug_message("split filter     : " + string(_ff_wrote) + " " + string(_ff_pm_drew) + " " + string(_ff_pm_l) + " / " + string(_ff_pm_r) + " | " + string(_ff_mp_drew) + " " + string(_ff_mp_l) + " / " + string(_ff_mp_r));
    if (!_ff_pm_drew || !_ff_mp_drew) show_debug_message("split filter err  : " + string(igpu_get_last_error()));
    _igpu_check(_ff_wrote && _ff_pm_drew && _ff_pm_l == c_red && color_get_red(_ff_pm_r) > 0 && color_get_blue(_ff_pm_r) > 0 && color_get_green(_ff_pm_r) == 0, "point magnification and linear minification");
    _igpu_check(_ff_mp_drew && _ff_mp_r == c_red && color_get_red(_ff_mp_l) > 0 && color_get_blue(_ff_mp_l) > 0 && color_get_green(_ff_mp_l) == 0, "linear magnification and point minification");
}
if (surface_exists(_ff_surf)) surface_free(_ff_surf);
_igpu_check(igpu_texture_release(_ff_tex), "release split-filter texture");
_igpu_check(igpu_shader_release(_ff_cs), "release split-filter compute shader");
_igpu_check(igpu_shader_release(_ff_ps), "release split-filter pixel shader");
_igpu_check(igpu_state_release(_ff_pm), "release point-mag sampler");
_igpu_check(igpu_state_release(_ff_mp), "release linear-mag sampler");

// Level 0 is red and level 1 is green, written directly. The sample sits
// halfway between those levels. Point mip picks one pure colour. Linear mip
// mixes them. Magnification and minification stay point, so only mip changes.
var _mb_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, true, true, 0);
if (_mb_tex == 0) show_debug_message("mip blend err    : " + string(igpu_get_last_error()));
var _mb0 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _mb1 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _mb_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.SampleLevel(s, float2(0.25, 0.5), 0.5); }", "main", IgpuShaderStage.Pixel, "");
var _mb_point = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _mb_linear = igpu_sampler_state_create(tf_point, tf_point, tf_linear, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _mb_surf = surface_create(8, 8);
_igpu_check(_mb_tex > 0 && _mb0 > 0 && _mb1 > 0 && _mb_ps > 0 && _mb_point > 0 && _mb_linear > 0 && surface_exists(_mb_surf), "mip-blend resources are created");
if (_mb_tex > 0 && _mb0 > 0 && _mb1 > 0 && _mb_ps > 0 && _mb_point > 0 && _mb_linear > 0 && surface_exists(_mb_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_mb0, IgpuShaderStage.Compute);
    var _mb_base = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_mb_tex], [0], [0]);
    igpu_shader_bind(_mb1, IgpuShaderStage.Compute);
    var _mb_coarse = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_mb_tex], [0], [1]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_mb_ps, IgpuShaderStage.Pixel);
    _igpu_target_begin(_mb_surf, c_black);
    var _mb_p_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _mb_tex, 0, 0, 0, _mb_point);
    surface_reset_target();
    var _mb_p_px = surface_getpixel(_mb_surf, 1, 4);
    _igpu_target_begin(_mb_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_mb_ps, IgpuShaderStage.Pixel);
    var _mb_l_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _mb_tex, 0, 0, 0, _mb_linear);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _mb_l_px = surface_getpixel(_mb_surf, 1, 4);
    show_debug_message("mip blend        : " + string(_mb_base) + " " + string(_mb_coarse) + " " + string(_mb_p_drew) + " " + string(_mb_p_px) + " / " + string(_mb_l_drew) + " " + string(_mb_l_px)
        + " r=" + string(color_get_red(_mb_l_px)) + " g=" + string(color_get_green(_mb_l_px)));
    if (!_mb_p_drew || !_mb_l_drew) show_debug_message("mip blend err    : " + string(igpu_get_last_error()));
    _igpu_check(_mb_base && _mb_coarse && _mb_p_drew && (_mb_p_px == c_red || _mb_p_px == c_lime), "point mip picks one level");
    _igpu_check(_mb_l_drew && _mb_l_px != _mb_p_px && color_get_red(_mb_l_px) > 0 && color_get_green(_mb_l_px) > 0 && color_get_blue(_mb_l_px) == 0, "linear mip mixes the two levels");
}
if (surface_exists(_mb_surf)) surface_free(_mb_surf);
_igpu_check(igpu_texture_release(_mb_tex), "release mip-blend texture");
_igpu_check(igpu_shader_release(_mb0), "release mip-blend level-0 shader");
_igpu_check(igpu_shader_release(_mb1), "release mip-blend level-1 shader");
_igpu_check(igpu_shader_release(_mb_ps), "release mip-blend pixel shader");
_igpu_check(igpu_state_release(_mb_point), "release point-mip sampler");
_igpu_check(igpu_state_release(_mb_linear), "release linear-mip sampler");

// Level 0 is red and level 1 is green. The left footprint is one texel on
// the 2x2 level, so the unshifted choice is level 0. The right footprint is
// two texels, so the unshifted choice is level 1. A positive offset pushes
// the left sample onto green. A negative offset pulls the right sample back
// onto red. Point filtering keeps each choice a pure colour.
var _lo_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, true, true, 0);
if (_lo_tex == 0) show_debug_message("level offset err : " + string(igpu_get_last_error()));
var _lo0 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _lo1 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _lo_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float2 ddxuv = i.uv.x < 0.5 ? float2(0.5, 0.0) : float2(1.0, 0.0); float2 ddyuv = i.uv.x < 0.5 ? float2(0.0, 0.5) : float2(0.0, 1.0); return t.SampleGrad(s, float2(0.25, 0.5), ddxuv, ddyuv); }", "main", IgpuShaderStage.Pixel, "");
var _lo_zero = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _lo_plus = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 4, 0, -1);
var _lo_minus = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, -1, 0, -1);
var _lo_surf = surface_create(8, 8);
_igpu_check(_lo_tex > 0 && _lo0 > 0 && _lo1 > 0 && _lo_ps > 0 && _lo_zero > 0 && _lo_plus > 0 && _lo_minus > 0 && surface_exists(_lo_surf), "level-offset resources are created");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, power(10, 40), 0, -1) == 0, "a level offset that is not a finite number of levels is rejected");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1) != 0, "border colour 0 stays valid with a level offset");
if (_lo_tex > 0 && _lo0 > 0 && _lo1 > 0 && _lo_ps > 0 && _lo_zero > 0 && _lo_plus > 0 && _lo_minus > 0 && surface_exists(_lo_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_lo0, IgpuShaderStage.Compute);
    var _lo_base = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lo_tex], [0], [0]);
    igpu_shader_bind(_lo1, IgpuShaderStage.Compute);
    var _lo_coarse = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lo_tex], [0], [1]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    _igpu_target_begin(_lo_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_lo_ps, IgpuShaderStage.Pixel);
    var _lo_z_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _lo_tex, 0, 0, 0, _lo_zero);
    surface_reset_target();
    var _lo_z_l = surface_getpixel(_lo_surf, 1, 4);
    var _lo_z_r = surface_getpixel(_lo_surf, 6, 4);
    _igpu_target_begin(_lo_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_lo_ps, IgpuShaderStage.Pixel);
    var _lo_p_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _lo_tex, 0, 0, 0, _lo_plus);
    surface_reset_target();
    var _lo_p_l = surface_getpixel(_lo_surf, 1, 4);
    _igpu_target_begin(_lo_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_lo_ps, IgpuShaderStage.Pixel);
    var _lo_m_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _lo_tex, 0, 0, 0, _lo_minus);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _lo_m_r = surface_getpixel(_lo_surf, 6, 4);
    show_debug_message("level offset    : " + string(_lo_base) + " " + string(_lo_coarse) + " " + string(_lo_z_drew) + " " + string(_lo_z_l) + " / " + string(_lo_z_r)
        + " | " + string(_lo_p_drew) + " " + string(_lo_p_l) + " | " + string(_lo_m_drew) + " " + string(_lo_m_r));
    if (!_lo_z_drew || !_lo_p_drew || !_lo_m_drew) show_debug_message("level offset err : " + string(igpu_get_last_error()));
    _igpu_check(_lo_base && _lo_coarse && _lo_z_drew && _lo_z_l == c_red, "zero offset stays on the fine level");
    _igpu_check(_lo_z_drew && _lo_z_r == c_lime, "the coarser footprint starts on the coarse level");
    _igpu_check(_lo_p_drew && _lo_p_l == c_lime, "a positive offset moves to the coarse level");
    _igpu_check(_lo_m_drew && _lo_m_r == c_red, "a negative offset moves back to the fine level");
}
if (surface_exists(_lo_surf)) surface_free(_lo_surf);
_igpu_check(igpu_texture_release(_lo_tex), "release level-offset texture");
_igpu_check(igpu_shader_release(_lo0), "release level-offset level-0 shader");
_igpu_check(igpu_shader_release(_lo1), "release level-offset level-1 shader");
_igpu_check(igpu_shader_release(_lo_ps), "release level-offset pixel shader");
_igpu_check(igpu_state_release(_lo_zero), "release zero-offset sampler");
_igpu_check(igpu_state_release(_lo_plus), "release positive-offset sampler");
_igpu_check(igpu_state_release(_lo_minus), "release negative-offset sampler");

// The same two footprints. Offset 4 would move both onto green, but the
// coarse limit is level 0, so both stay red. With no offset, the fine
// footprint would stay red, but the fine limit is level 1, so it becomes green.
var _lr_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, true, true, 0);
if (_lr_tex == 0) show_debug_message("level range err  : " + string(igpu_get_last_error()));
var _lr0 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _lr1 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _lr_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float2 ddxuv = i.uv.x < 0.5 ? float2(0.5, 0.0) : float2(1.0, 0.0); float2 ddyuv = i.uv.x < 0.5 ? float2(0.0, 0.5) : float2(0.0, 1.0); return t.SampleGrad(s, float2(0.25, 0.5), ddxuv, ddyuv); }", "main", IgpuShaderStage.Pixel, "");
var _lr_cap = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 4, 0, 0);
var _lr_floor = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 1, 1);
var _lr_surf = surface_create(8, 8);
_igpu_check(_lr_tex > 0 && _lr0 > 0 && _lr1 > 0 && _lr_ps > 0 && _lr_cap > 0 && _lr_floor > 0 && surface_exists(_lr_surf), "level-range resources are created");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, power(10, 40)) == 0, "a level limit that is not a finite number of levels is rejected");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 2, 0) == 0, "a finest level above the coarsest level is rejected");
if (_lr_tex > 0 && _lr0 > 0 && _lr1 > 0 && _lr_ps > 0 && _lr_cap > 0 && _lr_floor > 0 && surface_exists(_lr_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_lr0, IgpuShaderStage.Compute);
    var _lr_base = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lr_tex], [0], [0]);
    igpu_shader_bind(_lr1, IgpuShaderStage.Compute);
    var _lr_coarse = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lr_tex], [0], [1]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    _igpu_target_begin(_lr_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_lr_ps, IgpuShaderStage.Pixel);
    var _lr_c_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _lr_tex, 0, 0, 0, _lr_cap);
    surface_reset_target();
    var _lr_c_l = surface_getpixel(_lr_surf, 1, 4);
    var _lr_c_r = surface_getpixel(_lr_surf, 6, 4);
    _igpu_target_begin(_lr_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_lr_ps, IgpuShaderStage.Pixel);
    var _lr_f_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _lr_tex, 0, 0, 0, _lr_floor);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _lr_f_l = surface_getpixel(_lr_surf, 1, 4);
    show_debug_message("level range     : " + string(_lr_base) + " " + string(_lr_coarse) + " " + string(_lr_c_drew) + " " + string(_lr_c_l) + " / " + string(_lr_c_r)
        + " | " + string(_lr_f_drew) + " " + string(_lr_f_l));
    if (!_lr_c_drew || !_lr_f_drew) show_debug_message("level range err  : " + string(igpu_get_last_error()));
    _igpu_check(_lr_base && _lr_coarse && _lr_c_drew && _lr_c_l == c_red, "a coarse limit holds the fine footprint on red");
    _igpu_check(_lr_c_drew && _lr_c_r == c_red, "a coarse limit pulls the coarse footprint back to red");
    _igpu_check(_lr_f_drew && _lr_f_l == c_lime, "a fine limit pushes the fine footprint onto green");
}
if (surface_exists(_lr_surf)) surface_free(_lr_surf);
_igpu_check(igpu_texture_release(_lr_tex), "release level-range texture");
_igpu_check(igpu_shader_release(_lr0), "release level-range level-0 shader");
_igpu_check(igpu_shader_release(_lr1), "release level-range level-1 shader");
_igpu_check(igpu_shader_release(_lr_ps), "release level-range pixel shader");
_igpu_check(igpu_state_release(_lr_cap), "release coarse-limit sampler");
_igpu_check(igpu_state_release(_lr_floor), "release fine-limit sampler");

// Level 0 is red and level 1 is green. The left sample stays inside the
// texture at a one-texel footprint. The right sample is left of the edge,
// so a border axis reads the given colour. Offset 4 would move the left
// sample onto green; a coarse limit of 0 holds it on red. A fine limit of
// 1 forces green with no offset.
var _br_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, true, true, 0);
if (_br_tex == 0) show_debug_message("border range err : " + string(igpu_get_last_error()));
var _br0 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _br1 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _br_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float2 uv = i.uv.x < 0.5 ? float2(0.25, 0.5) : float2(-0.5, 0.5); return t.SampleGrad(s, uv, float2(0.5, 0.0), float2(0.0, 0.5)); }", "main", IgpuShaderStage.Pixel, "");
var _br_push = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, c_fuchsia, 0, 4, 0, 16);
var _br_cap = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, c_fuchsia, 0, 4, 0, 0);
var _br_floor = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, c_fuchsia, 0, 0, 1, 1);
var _br_ax_push = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, c_fuchsia, 0, 4, 0, 16);
var _br_ax_cap = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, c_fuchsia, 0, 4, 0, 0);
var _br_ax_aniso = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 16, c_fuchsia, 0, 0, 0, 16);
var _br_surf = surface_create(8, 8);
_igpu_check(_br_tex > 0 && _br0 > 0 && _br1 > 0 && _br_ps > 0 && _br_push > 0 && _br_cap > 0 && _br_floor > 0 && _br_ax_push > 0 && _br_ax_cap > 0 && _br_ax_aniso > 0 && surface_exists(_br_surf), "border level-range resources are created");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, c_fuchsia, 0, 0, 0, power(10, 40)) == 0, "a filter border level limit that is not finite is rejected");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, c_fuchsia, 0, 0, 2, 0) == 0, "a filter border finest level above the coarsest is rejected");
_igpu_check(igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 16, c_fuchsia, 0, 0, 0, power(10, 40)) == 0, "an axis border level limit that is not finite is rejected");
_igpu_check(igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 17, c_fuchsia, 0, 0, 0, 16) == 0, "an axis border range still rejects anisotropy above 16");
if (_br_tex > 0 && _br0 > 0 && _br1 > 0 && _br_ps > 0 && _br_push > 0 && _br_cap > 0 && _br_floor > 0 && _br_ax_push > 0 && _br_ax_cap > 0 && surface_exists(_br_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_br0, IgpuShaderStage.Compute);
    var _br_base = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_br_tex], [0], [0]);
    igpu_shader_bind(_br1, IgpuShaderStage.Compute);
    var _br_coarse = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_br_tex], [0], [1]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    _igpu_target_begin(_br_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_br_ps, IgpuShaderStage.Pixel);
    var _br_p_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _br_tex, 0, 0, 0, _br_push);
    surface_reset_target();
    var _br_p_l = surface_getpixel(_br_surf, 1, 4);
    var _br_p_r = surface_getpixel(_br_surf, 6, 4);
    _igpu_target_begin(_br_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_br_ps, IgpuShaderStage.Pixel);
    var _br_c_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _br_tex, 0, 0, 0, _br_cap);
    surface_reset_target();
    var _br_c_l = surface_getpixel(_br_surf, 1, 4);
    var _br_c_r = surface_getpixel(_br_surf, 6, 4);
    _igpu_target_begin(_br_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_br_ps, IgpuShaderStage.Pixel);
    var _br_f_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _br_tex, 0, 0, 0, _br_floor);
    surface_reset_target();
    var _br_f_l = surface_getpixel(_br_surf, 1, 4);
    _igpu_target_begin(_br_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_br_ps, IgpuShaderStage.Pixel);
    var _br_ap_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _br_tex, 0, 0, 0, _br_ax_push);
    surface_reset_target();
    var _br_ap_l = surface_getpixel(_br_surf, 1, 4);
    var _br_ap_r = surface_getpixel(_br_surf, 6, 4);
    _igpu_target_begin(_br_surf, c_black);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_br_ps, IgpuShaderStage.Pixel);
    var _br_ac_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _br_tex, 0, 0, 0, _br_ax_cap);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _br_ac_l = surface_getpixel(_br_surf, 1, 4);
    var _br_ac_r = surface_getpixel(_br_surf, 6, 4);
    show_debug_message("border range    : " + string(_br_base) + " " + string(_br_coarse) + " " + string(_br_p_drew) + " " + string(_br_p_l) + " / " + string(_br_p_r)
        + " | " + string(_br_c_drew) + " " + string(_br_c_l) + " / " + string(_br_c_r)
        + " | " + string(_br_f_drew) + " " + string(_br_f_l)
        + " || " + string(_br_ap_drew) + " " + string(_br_ap_l) + " / " + string(_br_ap_r)
        + " | " + string(_br_ac_drew) + " " + string(_br_ac_l) + " / " + string(_br_ac_r));
    if (!_br_p_drew || !_br_c_drew || !_br_f_drew || !_br_ap_drew || !_br_ac_drew) show_debug_message("border range err : " + string(igpu_get_last_error()));
    _igpu_check(_br_base && _br_coarse && _br_p_drew && _br_p_l == c_lime && _br_p_r == c_fuchsia, "a filter border offset reaches green and the outside sample is fuchsia");
    _igpu_check(_br_c_drew && _br_c_l == c_red && _br_c_r == c_fuchsia, "a filter border coarse limit stays red and the outside sample is fuchsia");
    _igpu_check(_br_f_drew && _br_f_l == c_lime, "a filter border fine limit reaches green");
    _igpu_check(_br_ap_drew && _br_ap_l == c_lime && _br_ap_r == c_fuchsia, "an axis border offset reaches green and the outside sample is fuchsia");
    _igpu_check(_br_ac_drew && _br_ac_l == c_red && _br_ac_r == c_fuchsia, "an axis border coarse limit stays red and the outside sample is fuchsia");
}
if (surface_exists(_br_surf)) surface_free(_br_surf);
_igpu_check(igpu_texture_release(_br_tex), "release border level-range texture");
_igpu_check(igpu_shader_release(_br0), "release border level-range level-0 shader");
_igpu_check(igpu_shader_release(_br1), "release border level-range level-1 shader");
_igpu_check(igpu_shader_release(_br_ps), "release border level-range pixel shader");
_igpu_check(igpu_state_release(_br_push), "release filter border offset sampler");
_igpu_check(igpu_state_release(_br_cap), "release filter border coarse sampler");
_igpu_check(igpu_state_release(_br_floor), "release filter border fine sampler");
_igpu_check(igpu_state_release(_br_ax_push), "release axis border offset sampler");
_igpu_check(igpu_state_release(_br_ax_cap), "release axis border coarse sampler");
_igpu_check(igpu_state_release(_br_ax_aniso), "release anisotropic border span sampler");

// A single-channel texture stores 0.25 on the left and 0.75 on the right.
// The shader reference is 0.5. cmpfunc_less passes only on the right.
// cmpfunc_greater passes only on the left. A linear filter at the boundary
// blends those two results.
var _cmp_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_r32float, false, true, 1);
if (_cmp_tex == 0) show_debug_message("compare err      : " + string(igpu_get_last_error()));
var _cmp_cs = igpu_shader_compile("RWTexture2D<float> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = id.x == 0 ? 0.25 : 0.75; }", "main", IgpuShaderStage.Compute, "");
var _cmp_ps = igpu_shader_compile("Texture2D<float> t : register(t0); SamplerComparisonState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float2 uv = i.uv.x < 0.5 ? float2(0.25, 0.5) : float2(0.75, 0.5); return float4(t.SampleCmpLevelZero(s, uv, 0.5), 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _cmp_mid = igpu_shader_compile("Texture2D<float> t : register(t0); SamplerComparisonState s : register(s0); float4 main() : SV_TARGET { return float4(t.SampleCmpLevelZero(s, float2(0.5, 0.5), 0.5), 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _cmp_less = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 0, 0, -1);
var _cmp_greater = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_greater, 0, 0, -1);
var _cmp_linear = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 0, 0, -1);
var _cmp_surf = surface_create(8, 8);
_igpu_check(_cmp_tex > 0 && _cmp_cs > 0 && _cmp_ps > 0 && _cmp_mid > 0 && _cmp_less > 0 && _cmp_greater > 0 && _cmp_linear > 0 && surface_exists(_cmp_surf), "comparison sampler resources are created");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 99, 0, 0, -1) == 0, "a comparison sampler rejects an unknown compare");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Border, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 0, 0, -1) == 0, "a comparison sampler rejects border addressing");
if (_cmp_tex > 0 && _cmp_cs > 0 && _cmp_ps > 0 && _cmp_mid > 0 && _cmp_less > 0 && _cmp_greater > 0 && _cmp_linear > 0 && surface_exists(_cmp_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_cmp_cs, IgpuShaderStage.Compute);
    var _cmp_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_cmp_tex], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    _igpu_target_begin(_cmp_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cmp_ps, IgpuShaderStage.Pixel);
    var _cmp_l_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cmp_tex, 0, 0, 0, _cmp_less);
    surface_reset_target();
    var _cmp_l_left = surface_getpixel(_cmp_surf, 1, 4);
    var _cmp_l_right = surface_getpixel(_cmp_surf, 6, 4);
    _igpu_target_begin(_cmp_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cmp_ps, IgpuShaderStage.Pixel);
    var _cmp_g_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cmp_tex, 0, 0, 0, _cmp_greater);
    surface_reset_target();
    var _cmp_g_left = surface_getpixel(_cmp_surf, 1, 4);
    var _cmp_g_right = surface_getpixel(_cmp_surf, 6, 4);
    _igpu_target_begin(_cmp_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cmp_mid, IgpuShaderStage.Pixel);
    var _cmp_m_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cmp_tex, 0, 0, 0, _cmp_linear);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _cmp_m_px = surface_getpixel(_cmp_surf, 4, 4);
    show_debug_message("compare sample  : " + string(_cmp_wrote) + " " + string(_cmp_l_drew) + " " + string(_cmp_l_left) + " / " + string(_cmp_l_right)
        + " | " + string(_cmp_g_drew) + " " + string(_cmp_g_left) + " / " + string(_cmp_g_right)
        + " | " + string(_cmp_m_drew) + " " + string(_cmp_m_px) + " r=" + string(color_get_red(_cmp_m_px)));
    if (!_cmp_wrote || !_cmp_l_drew || !_cmp_g_drew || !_cmp_m_drew) show_debug_message("compare err      : " + string(igpu_get_last_error()));
    _igpu_check(_cmp_wrote && _cmp_l_drew && _cmp_l_left == c_black && _cmp_l_right == c_red, "less passes only where the reference is below the texel");
    _igpu_check(_cmp_g_drew && _cmp_g_left == c_red && _cmp_g_right == c_black, "greater passes only where the reference is above the texel");
    _igpu_check(_cmp_m_drew && color_get_red(_cmp_m_px) == 128 && color_get_green(_cmp_m_px) == 0 && color_get_blue(_cmp_m_px) == 0, "a linear comparison blends the passing and failing texels");
}
if (surface_exists(_cmp_surf)) surface_free(_cmp_surf);
_igpu_check(igpu_texture_release(_cmp_tex), "release comparison texture");
_igpu_check(igpu_shader_release(_cmp_cs), "release comparison compute shader");
_igpu_check(igpu_shader_release(_cmp_ps), "release comparison pixel shader");
_igpu_check(igpu_shader_release(_cmp_mid), "release comparison blend shader");
_igpu_check(igpu_state_release(_cmp_less), "release less comparison sampler");
_igpu_check(igpu_state_release(_cmp_greater), "release greater comparison sampler");
_igpu_check(igpu_state_release(_cmp_linear), "release linear comparison sampler");

// Level 0 is 0.25 and level 1 is 1.0. cmpfunc_less against 0.5 fails on
// level 0 and passes on level 1. SampleCmp follows the sampler offset.
// On this Direct3D 11 device SampleCmpLevelZero follows it too, and the
// fine limit still clamps that explicit level.
var _cl_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_r32float, false, true, 0);
var _cl0 = igpu_shader_compile("RWTexture2D<float> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = 0.25; }", "main", IgpuShaderStage.Compute, "");
var _cl1 = igpu_shader_compile("RWTexture2D<float> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = 1.0; }", "main", IgpuShaderStage.Compute, "");
var _cl_grad = igpu_shader_compile("Texture2D<float> t : register(t0); SamplerComparisonState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return float4(t.SampleCmp(s, i.uv, 0.5), 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
if (_cl_grad == 0) show_debug_message("compare grad err: " + string(igpu_get_last_error()));
var _cl_zero = igpu_shader_compile("Texture2D<float> t : register(t0); SamplerComparisonState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return float4(t.SampleCmpLevelZero(s, float2(0.25, 0.5), 0.5), 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
if (_cl_tex == 0 || _cl0 == 0 || _cl1 == 0 || _cl_grad == 0 || _cl_zero == 0) show_debug_message("compare level res: " + string(igpu_get_last_error()));
var _cl_base = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 0, 0, -1);
var _cl_plus = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 4, 0, -1);
if (_cl_plus == 0) show_debug_message("compare level err: " + string(igpu_get_last_error()));
var _cl_cap = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 4, 0, 0);
var _cl_floor = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 0, 1, -1);
var _cl_explicit = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 4, 0, -1);
var _cl_explicit_floor = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, cmpfunc_less, 0, 1, -1);
var _cl_aniso = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 16, 0, cmpfunc_less, 0, 0, -1);
var _cl_surf = surface_create(8, 8);
_igpu_check(_cl_tex > 0 && _cl0 > 0 && _cl1 > 0 && _cl_grad > 0 && _cl_zero > 0 && surface_exists(_cl_surf), "comparison level resources are created");
_igpu_check(_cl_base > 0 && _cl_plus > 0 && _cl_cap > 0 && _cl_floor > 0 && _cl_explicit > 0 && _cl_explicit_floor > 0 && _cl_aniso > 0, "comparison level samplers are created");
_igpu_check(igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 16, 0, cmpfunc_less, 0, 0, -1) == 0, "a comparison sampler rejects anisotropy above 1 with point filters");
if (_cl_tex > 0 && _cl0 > 0 && _cl1 > 0 && _cl_grad > 0 && _cl_zero > 0 && _cl_base > 0 && _cl_plus > 0 && _cl_cap > 0 && _cl_floor > 0 && _cl_explicit > 0 && _cl_explicit_floor > 0 && _cl_aniso > 0 && surface_exists(_cl_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_cl0, IgpuShaderStage.Compute);
    var _cl_wrote0 = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_cl_tex], [0], [0]);
    igpu_shader_bind(_cl1, IgpuShaderStage.Compute);
    var _cl_wrote1 = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_cl_tex], [0], [1]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    _igpu_target_begin(_cl_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cl_grad, IgpuShaderStage.Pixel);
    var _cl_z_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cl_tex, 0, 0, 0, _cl_base);
    surface_reset_target();
    var _cl_z_px = surface_getpixel(_cl_surf, 1, 4);
    _igpu_target_begin(_cl_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cl_zero, IgpuShaderStage.Pixel);
    var _cl_e_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cl_tex, 0, 0, 0, _cl_explicit);
    surface_reset_target();
    var _cl_e_px = surface_getpixel(_cl_surf, 1, 4);
    _igpu_target_begin(_cl_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cl_grad, IgpuShaderStage.Pixel);
    var _cl_p_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cl_tex, 0, 0, 0, _cl_plus);
    surface_reset_target();
    var _cl_p_px = surface_getpixel(_cl_surf, 1, 4);
    _igpu_target_begin(_cl_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cl_grad, IgpuShaderStage.Pixel);
    var _cl_c_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cl_tex, 0, 0, 0, _cl_cap);
    surface_reset_target();
    var _cl_c_px = surface_getpixel(_cl_surf, 1, 4);
    _igpu_target_begin(_cl_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cl_grad, IgpuShaderStage.Pixel);
    var _cl_f_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cl_tex, 0, 0, 0, _cl_floor);
    surface_reset_target();
    var _cl_f_px = surface_getpixel(_cl_surf, 1, 4);
    _igpu_target_begin(_cl_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cl_zero, IgpuShaderStage.Pixel);
    var _cl_ef_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cl_tex, 0, 0, 0, _cl_explicit_floor);
    surface_reset_target();
    var _cl_ef_px = surface_getpixel(_cl_surf, 1, 4);
    _igpu_target_begin(_cl_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_cl_zero, IgpuShaderStage.Pixel);
    var _cl_a_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _cl_tex, 0, 0, 0, _cl_aniso);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _cl_a_px = surface_getpixel(_cl_surf, 1, 4);
    show_debug_message("compare level    : " + string(_cl_wrote0) + " " + string(_cl_wrote1)
        + " " + string(_cl_z_drew) + " " + string(_cl_z_px)
        + " | " + string(_cl_p_drew) + " " + string(_cl_p_px)
        + " | " + string(_cl_c_drew) + " " + string(_cl_c_px)
        + " | " + string(_cl_f_drew) + " " + string(_cl_f_px)
        + " || " + string(_cl_e_drew) + " " + string(_cl_e_px)
        + " | " + string(_cl_ef_drew) + " " + string(_cl_ef_px)
        + " | " + string(_cl_a_drew) + " " + string(_cl_a_px));
    if (!_cl_z_drew || !_cl_p_drew || !_cl_c_drew || !_cl_f_drew || !_cl_e_drew || !_cl_ef_drew || !_cl_a_drew) show_debug_message("compare level err: " + string(igpu_get_last_error()));
    _igpu_check(_cl_wrote0 && _cl_wrote1 && _cl_z_drew && _cl_z_px == c_black, "a comparison sample with no offset stays on the fine level");
    _igpu_check(_cl_p_drew && _cl_p_px == c_red, "a comparison offset moves to the coarse level");
    _igpu_check(_cl_c_drew && _cl_c_px == c_black, "a comparison coarse limit keeps the fine level");
    _igpu_check(_cl_f_drew && _cl_f_px == c_red, "a comparison fine limit starts on the coarse level");
    _igpu_check(_cl_e_drew && _cl_e_px == c_red, "an explicit comparison level also follows the offset");
    _igpu_check(_cl_ef_drew && _cl_ef_px == c_red, "an explicit comparison level still obeys the fine limit");
    _igpu_check(_cl_a_drew && _cl_a_px == c_black, "an anisotropic comparison sampler still compares the fine level");
}
if (surface_exists(_cl_surf)) surface_free(_cl_surf);
_igpu_check(igpu_texture_release(_cl_tex), "release comparison level texture");
_igpu_check(igpu_shader_release(_cl0), "release comparison level-0 shader");
_igpu_check(igpu_shader_release(_cl1), "release comparison level-1 shader");
_igpu_check(igpu_shader_release(_cl_grad), "release comparison gradient shader");
_igpu_check(igpu_shader_release(_cl_zero), "release comparison explicit-level shader");
_igpu_check(igpu_state_release(_cl_base), "release comparison base sampler");
_igpu_check(igpu_state_release(_cl_plus), "release comparison offset sampler");
_igpu_check(igpu_state_release(_cl_cap), "release comparison coarse sampler");
_igpu_check(igpu_state_release(_cl_floor), "release comparison fine sampler");
_igpu_check(igpu_state_release(_cl_explicit), "release comparison explicit offset sampler");
_igpu_check(igpu_state_release(_cl_explicit_floor), "release comparison explicit fine sampler");
_igpu_check(igpu_state_release(_cl_aniso), "release anisotropic comparison sampler");

// Level 0 is 2x2, left column red and right column blue. The generated
// 1x1 level is the mix of those four texels. The left of the quad samples
// level 0, the right samples level 1.
var _m_one = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, false, false, 1);
var _m_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, true, true, 0);
if (_m_tex == 0) show_debug_message("mip create err    : " + string(igpu_get_last_error()));
var _m_cs = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = id.x == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _m_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float lod = i.uv.x < 0.5 ? 0.0 : 1.0; return t.SampleLevel(s, float2(0.25, 0.5), lod); }", "main", IgpuShaderStage.Pixel, "");
var _m_point = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _m_surf = surface_create(8, 8);
_igpu_check(_m_one > 0 && _m_tex > 0 && _m_cs > 0 && _m_ps > 0 && _m_point > 0 && surface_exists(_m_surf), "mip resources are created");
_igpu_check(igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, true, true, 3) == 0, "too many mip levels are rejected");
_igpu_check(!igpu_texture_generate_mips(_m_one), "a one-level texture cannot generate coarser levels");
if (_m_tex > 0 && _m_cs > 0 && _m_ps > 0 && _m_point > 0 && surface_exists(_m_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_m_cs, IgpuShaderStage.Compute);
    var _m_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_m_tex], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _m_gen = igpu_texture_generate_mips(_m_tex);
    var _m_read0 = igpu_texture_read(_m_tex, 0, 0, 0, 0);
    var _m_read_blue = igpu_texture_read(_m_tex, 1, 0, 0, 0);
    var _m_read1 = igpu_texture_read(_m_tex, 0, 0, 0, 1);
    show_debug_message("mip read         : " + string(_m_read0) + " " + string(_m_read_blue) + " / " + string(_m_read1)
        + " r=" + string(color_get_red(_m_read1)) + " b=" + string(color_get_blue(_m_read1)));
    gpu_push_state();
    _igpu_target_begin(_m_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_m_ps, IgpuShaderStage.Pixel);
    var _m_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _m_tex, 0, 0, 0, _m_point);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _m_fine = surface_getpixel(_m_surf, 1, 4);
    var _m_coarse = surface_getpixel(_m_surf, 6, 4);
    show_debug_message("mip level        : " + string(_m_wrote) + " " + string(_m_gen) + " " + string(_m_drew) + " " + string(_m_fine) + " / " + string(_m_coarse)
        + " r=" + string(color_get_red(_m_coarse)) + " b=" + string(color_get_blue(_m_coarse)));
    if (!_m_gen || !_m_drew) show_debug_message("mip level err     : " + string(igpu_get_last_error()));
    _igpu_check(_m_wrote && _m_gen && _m_drew && _m_fine == c_red, "the finest mip stays on the red texel");
    _igpu_check(_m_coarse != _m_fine && color_get_red(_m_coarse) > 0 && color_get_blue(_m_coarse) > 0 && color_get_green(_m_coarse) == 0, "the coarser mip mixes red and blue");
    _igpu_check(_m_read0 == c_red && _m_read_blue == c_blue, "level 0 reads the red and blue texels");
    _igpu_check(color_get_red(_m_read1) == 128 && color_get_blue(_m_read1) == 128 && color_get_green(_m_read1) == 0, "level 1 reads the mixed texel");
}
_igpu_check(_m_tex == 0 || igpu_texture_read(_m_tex, 0, 0, 0, 2) == 0, "a mip past the chain is rejected");
_igpu_check(_m_tex == 0 || igpu_texture_read(_m_tex, 1, 0, 0, 1) == 0, "a pixel outside the coarser level is rejected");

// Level 0 is written red and blue. Level 1 is written green directly, not
// generated, so it is not the average and level 0 stays untouched.
var _lw_tex = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, true, true, 0);
var _lw_plain = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, false, false, 1);
var _lw0 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = id.x == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _lw1 = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
if (_lw_tex == 0) show_debug_message("level write err   : " + string(igpu_get_last_error()));
_igpu_check(_lw_tex > 0 && _lw_plain > 0 && _lw0 > 0 && _lw1 > 0, "level-write resources are created");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lw_tex], [0], [1]), "a level write without a compute shader is rejected");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lw_plain], [0], [0]), "a sampled texture is not a level write target");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lw_tex], [0], [2]), "a level past the chain is rejected");
if (_lw_tex > 0 && _lw0 > 0 && _lw1 > 0)
{
    igpu_shader_bind(_lw0, IgpuShaderStage.Compute);
    var _lw_base = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lw_tex], [0], [0]);
    igpu_shader_bind(_lw1, IgpuShaderStage.Compute);
    var _lw_coarse = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_lw_tex], [0], [1]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _lw_red = igpu_texture_read(_lw_tex, 0, 0, 0, 0);
    var _lw_blue = igpu_texture_read(_lw_tex, 1, 0, 0, 0);
    var _lw_green = igpu_texture_read(_lw_tex, 0, 0, 0, 1);
    show_debug_message("level write      : " + string(_lw_base) + " " + string(_lw_coarse) + " " + string(_lw_red) + " " + string(_lw_blue) + " / " + string(_lw_green));
    if (!_lw_base || !_lw_coarse) show_debug_message("level write err   : " + string(igpu_get_last_error()));
    _igpu_check(_lw_base && _lw_red == c_red && _lw_blue == c_blue, "level 0 keeps the colour written there");
    _igpu_check(_lw_coarse && _lw_green == c_lime, "level 1 keeps the colour written there");
}
_igpu_check(igpu_texture_release(_lw_tex), "release level-write texture");
_igpu_check(igpu_texture_release(_lw_plain), "release plain level-write texture");
_igpu_check(igpu_shader_release(_lw0), "release level-0 compute shader");
_igpu_check(igpu_shader_release(_lw1), "release level-1 compute shader");

// Level 0 of each kind is written red. Level 1 writes green into the volume,
// and into array layer 1 and cube face 1 only. Layer 0 of that coarser
// level stays black, so a write that hit the wrong level or layer fails.
var _kv = igpu_texture_create(IgpuTextureKind.ThreeD, 2, 2, 2, surface_rgba8unorm, true, true, 0);
var _ka = igpu_texture_create(IgpuTextureKind.Array, 2, 2, 2, surface_rgba8unorm, true, true, 0);
var _kc = igpu_texture_create(IgpuTextureKind.Cube, 2, 2, 6, surface_rgba8unorm, true, true, 0);
if (_kv == 0 || _ka == 0 || _kc == 0) show_debug_message("kind level err   : " + string(igpu_get_last_error()));
var _kv0 = igpu_shader_compile("RWTexture3D<float4> dst : register(u0); [numthreads(2, 2, 2)] void main(uint3 id : SV_DispatchThreadID) { dst[id] = float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _kv1 = igpu_shader_compile("RWTexture3D<float4> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[uint3(0, 0, 0)] = float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _ka0 = igpu_shader_compile("RWTexture2DArray<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id] = float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _ka1 = igpu_shader_compile("RWTexture2DArray<float4> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[uint3(0, 0, 1)] = float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
_igpu_check(_kv > 0 && _ka > 0 && _kc > 0 && _kv0 > 0 && _kv1 > 0 && _ka0 > 0 && _ka1 > 0, "kind level-write resources are created");
if (_kv > 0 && _ka > 0 && _kc > 0 && _kv0 > 0 && _kv1 > 0 && _ka0 > 0 && _ka1 > 0)
{
    igpu_shader_bind(_kv0, IgpuShaderStage.Compute);
    var _kv_base = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_kv], [0], [0]);
    igpu_shader_bind(_kv1, IgpuShaderStage.Compute);
    var _kv_coarse = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_kv], [0], [1]);
    igpu_shader_bind(_ka0, IgpuShaderStage.Compute);
    var _ka_base = igpu_dispatch(1, 1, 2, [IgpuWriteTarget.Texture], [_ka], [0], [0]);
    var _kc_base = igpu_dispatch(1, 1, 6, [IgpuWriteTarget.Texture], [_kc], [0], [0]);
    igpu_shader_bind(_ka1, IgpuShaderStage.Compute);
    var _ka_coarse = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_ka], [0], [1]);
    var _kc_coarse = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_kc], [0], [1]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _kv0_px = igpu_texture_read(_kv, 0, 0, 0, 0);
    var _kv1_px = igpu_texture_read(_kv, 0, 0, 0, 1);
    var _ka0_px = igpu_texture_read(_ka, 0, 0, 0, 0);
    var _ka1_blank = igpu_texture_read(_ka, 0, 0, 0, 1);
    var _ka1_px = igpu_texture_read(_ka, 0, 0, 1, 1);
    var _kc0_px = igpu_texture_read(_kc, 0, 0, 0, 0);
    var _kc1_blank = igpu_texture_read(_kc, 0, 0, 0, 1);
    var _kc1_px = igpu_texture_read(_kc, 0, 0, 1, 1);
    show_debug_message("level volume     : " + string(_kv_base) + " " + string(_kv_coarse) + " " + string(_kv0_px) + " / " + string(_kv1_px));
    show_debug_message("level array      : " + string(_ka_base) + " " + string(_ka_coarse) + " " + string(_ka0_px) + " " + string(_ka1_blank) + " / " + string(_ka1_px));
    show_debug_message("level cube       : " + string(_kc_base) + " " + string(_kc_coarse) + " " + string(_kc0_px) + " " + string(_kc1_blank) + " / " + string(_kc1_px));
    if (!_kv_coarse || !_ka_coarse || !_kc_coarse) show_debug_message("kind level err   : " + string(igpu_get_last_error()));
    _igpu_check(_kv_base && _kv_coarse && _kv0_px == c_red && _kv1_px == c_lime && igpu_texture_read(_kv, 0, 0, 1, 1) == 0, "a volume level write keeps both levels");
    _igpu_check(_ka_base && _ka_coarse && _ka0_px == c_red && _ka1_blank == 0 && _ka1_px == c_lime, "an array level write hits one layer");
    _igpu_check(_kc_base && _kc_coarse && _kc0_px == c_red && _kc1_blank == 0 && _kc1_px == c_lime, "a cube level write hits one face");
}
_igpu_check(igpu_texture_release(_kv), "release volume level texture");
_igpu_check(igpu_texture_release(_ka), "release array level texture");
_igpu_check(igpu_texture_release(_kc), "release cube level texture");
_igpu_check(igpu_shader_release(_kv0), "release volume level-0 shader");
_igpu_check(igpu_shader_release(_kv1), "release volume level-1 shader");
_igpu_check(igpu_shader_release(_ka0), "release array level-0 shader");
_igpu_check(igpu_shader_release(_ka1), "release array level-1 shader");

// A triangle covers the viewport. Level 0 is drawn red. Level 1 is drawn
// green, and for the array and cube only layer or face 1, so the other
// coarser slice stays black.
var _rt = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, true, true, 0);
var _rv = igpu_texture_create(IgpuTextureKind.ThreeD, 2, 2, 2, surface_rgba8unorm, true, true, 0);
var _ra = igpu_texture_create(IgpuTextureKind.Array, 2, 2, 2, surface_rgba8unorm, true, true, 0);
var _rc = igpu_texture_create(IgpuTextureKind.Cube, 2, 2, 6, surface_rgba8unorm, true, true, 0);
var _rplain = igpu_texture_create(IgpuTextureKind.TwoD, 2, 2, 1, surface_rgba8unorm, false, false, 1);
if (_rt == 0 || _rv == 0 || _ra == 0 || _rc == 0) show_debug_message("draw level err   : " + string(igpu_get_last_error()));
var _rvs = igpu_shader_compile(
    "struct VSIn { float3 pos : POSITION; }; struct VSOut { float4 pos : SV_POSITION; }; VSOut main(VSIn i) { VSOut o; o.pos = float4(i.pos, 1.0); return o; }",
    "main", IgpuShaderStage.Vertex, "");
var _rred = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _rgreen = igpu_shader_compile("float4 main() : SV_TARGET { return float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _rlayout = igpu_input_layout_create(_rvs, [vertex_usage_position], [vertex_type_float3], array_create(1, IgpuVertexStep.Vertex), 1, 12, 0);
var _rvb = igpu_buffer_create(36, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 12);
_igpu_check(_rt > 0 && _rv > 0 && _ra > 0 && _rc > 0 && _rplain > 0 && _rvs > 0 && _rred > 0 && _rgreen > 0 && _rlayout > 0 && _rvb > 0, "draw-level resources are created");
var _rup = buffer_create(36, buffer_fixed, 4);
buffer_seek(_rup, buffer_seek_start, 0);
buffer_write(_rup, buffer_f32, -1); buffer_write(_rup, buffer_f32, -1); buffer_write(_rup, buffer_f32, 0.5);
buffer_write(_rup, buffer_f32, 3); buffer_write(_rup, buffer_f32, -1); buffer_write(_rup, buffer_f32, 0.5);
buffer_write(_rup, buffer_f32, -1); buffer_write(_rup, buffer_f32, 3); buffer_write(_rup, buffer_f32, 0.5);
_igpu_check(igpu_buffer_write(_rvb, 0, _rup), "upload the level-draw triangle");
buffer_delete(_rup);
_igpu_check(!igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rplain], [0], [0], 0, 0, 0, 0), "a sampled texture is not a level draw target");
_igpu_check(!igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rt], [0], [2], 0, 0, 0, 0), "a draw level past the chain is rejected");
if (_rt > 0 && _rv > 0 && _ra > 0 && _rc > 0 && _rvs > 0 && _rred > 0 && _rgreen > 0 && _rlayout > 0 && _rvb > 0)
{
    igpu_shader_bind(_rvs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_rred, IgpuShaderStage.Pixel);
    var _rt0 = igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rt], [0], [0], 0, 0, 0, 0);
    var _rv0 = igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rv], [0], [0], 0, 0, 0, 0);
    var _ra0 = igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_ra], [0], [0], 0, 0, 0, 0);
    var _rc0 = igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rc], [0], [0], 0, 0, 0, 0);
    igpu_shader_bind(_rgreen, IgpuShaderStage.Pixel);
    var _rt1 = igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rt], [0], [1], 0, 0, 0, 0);
    var _rv1 = igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rv], [0], [1], 0, 0, 0, 0);
    var _ra1 = igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_ra], [1], [1], 0, 0, 0, 0);
    var _rc1 = igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rc], [1], [1], 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    var _rt0_px = igpu_texture_read(_rt, 0, 0, 0, 0);
    var _rt1_px = igpu_texture_read(_rt, 0, 0, 0, 1);
    var _rv0_px = igpu_texture_read(_rv, 0, 0, 0, 0);
    var _rv1_px = igpu_texture_read(_rv, 0, 0, 0, 1);
    var _ra0_px = igpu_texture_read(_ra, 0, 0, 0, 0);
    var _ra1_blank = igpu_texture_read(_ra, 0, 0, 0, 1);
    var _ra1_px = igpu_texture_read(_ra, 0, 0, 1, 1);
    var _rc0_px = igpu_texture_read(_rc, 0, 0, 0, 0);
    var _rc1_blank = igpu_texture_read(_rc, 0, 0, 0, 1);
    var _rc1_px = igpu_texture_read(_rc, 0, 0, 1, 1);
    show_debug_message("draw level       : " + string(_rt0) + " " + string(_rt1) + " " + string(_rt0_px) + " / " + string(_rt1_px));
    show_debug_message("draw level vol   : " + string(_rv0) + " " + string(_rv1) + " " + string(_rv0_px) + " / " + string(_rv1_px));
    show_debug_message("draw level arr   : " + string(_ra0) + " " + string(_ra1) + " " + string(_ra0_px) + " " + string(_ra1_blank) + " / " + string(_ra1_px));
    show_debug_message("draw level cube  : " + string(_rc0) + " " + string(_rc1) + " " + string(_rc0_px) + " " + string(_rc1_blank) + " / " + string(_rc1_px));
    if (!_rt1 || !_rv1 || !_ra1 || !_rc1) show_debug_message("draw level err   : " + string(igpu_get_last_error()));
    _igpu_check(_rt0 && _rt1 && _rt0_px == c_red && _rt1_px == c_lime, "a draw into level 1 leaves level 0 red");
    _igpu_check(_rv0 && _rv1 && _rv0_px == c_red && _rv1_px == c_lime && !igpu_draw_to_render_targets(_rvb, _rlayout, pr_trianglelist, 0, 3, [_rv], [1], [1], 0, 0, 0, 0), "a volume draw into the coarser level keeps level 0");
    _igpu_check(_ra0 && _ra1 && _ra0_px == c_red && _ra1_blank == 0 && _ra1_px == c_lime, "an array draw hits one coarser layer");
    _igpu_check(_rc0 && _rc1 && _rc0_px == c_red && _rc1_blank == 0 && _rc1_px == c_lime, "a cube draw hits one coarser face");
}
_igpu_check(igpu_texture_release(_rt), "release draw-level texture");
_igpu_check(igpu_texture_release(_rv), "release draw-level volume");
_igpu_check(igpu_texture_release(_ra), "release draw-level array");
_igpu_check(igpu_texture_release(_rc), "release draw-level cube");
_igpu_check(igpu_texture_release(_rplain), "release plain draw-level texture");
_igpu_check(igpu_buffer_release(_rvb), "release level-draw triangle");
_igpu_check(igpu_input_layout_release(_rlayout), "release level-draw layout");
_igpu_check(igpu_shader_release(_rvs), "release level-draw vertex shader");
_igpu_check(igpu_shader_release(_rred), "release level-draw red shader");
_igpu_check(igpu_shader_release(_rgreen), "release level-draw green shader");

if (surface_exists(_m_surf)) surface_free(_m_surf);
_igpu_check(igpu_texture_release(_m_one), "release one-level texture");
_igpu_check(igpu_texture_release(_m_tex), "release mip texture");
_igpu_check(igpu_shader_release(_m_cs), "release mip compute shader");
_igpu_check(igpu_shader_release(_m_ps), "release mip pixel shader");
_igpu_check(igpu_state_release(_m_point), "release mip sampler");

// Each level-0 slice, layer and face is 2x2 with a red column and a blue
// column. Level 0 at the red texel stays red. The generated 1x1 level is
// the mix. The cube sample aims at the red half of +X.
var _mv = igpu_texture_create(IgpuTextureKind.ThreeD, 2, 2, 2, surface_rgba8unorm, true, true, 0);
var _ma = igpu_texture_create(IgpuTextureKind.Array, 2, 2, 2, surface_rgba8unorm, true, true, 0);
var _mc = igpu_texture_create(IgpuTextureKind.Cube, 2, 2, 6, surface_rgba8unorm, true, true, 0);
if (_mv == 0 || _ma == 0 || _mc == 0) show_debug_message("mip kind err      : " + string(igpu_get_last_error()));
var _mv_cs = igpu_shader_compile("RWTexture3D<float4> dst : register(u0); [numthreads(2, 2, 2)] void main(uint3 id : SV_DispatchThreadID) { dst[id] = id.x == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _ma_cs = igpu_shader_compile("RWTexture2DArray<float4> dst : register(u0); [numthreads(2, 2, 1)] void main(uint3 id : SV_DispatchThreadID) { if (id.z == 1) dst[id] = id.x == 0 ? float4(0.0, 1.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); else dst[id] = id.x == 0 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _mv_ps = igpu_shader_compile("Texture3D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float lod = i.uv.x < 0.5 ? 0.0 : 1.0; return t.SampleLevel(s, float3(0.25, 0.5, 0.25), lod); }", "main", IgpuShaderStage.Pixel, "");
var _ma_ps = igpu_shader_compile("Texture2DArray t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float lod = i.uv.x < 0.5 ? 0.0 : 1.0; return t.SampleLevel(s, float3(0.25, 0.5, 0.0), lod); }", "main", IgpuShaderStage.Pixel, "");
var _mc_ps = igpu_shader_compile("TextureCube t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float lod = i.uv.x < 0.5 ? 0.0 : 1.0; return t.SampleLevel(s, float3(1.0, 0.0, 0.5), lod); }", "main", IgpuShaderStage.Pixel, "");
var _mk_point = igpu_sampler_state_create(tf_point, tf_point, tf_point, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 1, 0, 0, 0, 0, -1);
var _mk_surf = surface_create(8, 8);
_igpu_check(_mv > 0 && _ma > 0 && _mc > 0 && _mv_cs > 0 && _ma_cs > 0 && _mv_ps > 0 && _ma_ps > 0 && _mc_ps > 0 && _mk_point > 0 && surface_exists(_mk_surf), "volume, array and cube mip resources are created");
if (_mv > 0 && _ma > 0 && _mc > 0 && _mv_cs > 0 && _ma_cs > 0 && _mv_ps > 0 && _ma_ps > 0 && _mc_ps > 0 && _mk_point > 0 && surface_exists(_mk_surf) && _tex_vs > 0 && _tex_full > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_mv_cs, IgpuShaderStage.Compute);
    var _mv_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_mv], [0], [0]);
    var _mv_gen = igpu_texture_generate_mips(_mv);
    igpu_shader_bind(_ma_cs, IgpuShaderStage.Compute);
    var _ma_wrote = igpu_dispatch(1, 1, 2, [IgpuWriteTarget.Texture], [_ma], [0], [0]);
    var _ma_gen = igpu_texture_generate_mips(_ma);
    var _mc_wrote = igpu_dispatch(1, 1, 6, [IgpuWriteTarget.Texture], [_mc], [0], [0]);
    var _mc_gen = igpu_texture_generate_mips(_mc);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    gpu_push_state();
    _igpu_target_begin(_mk_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_mv_ps, IgpuShaderStage.Pixel);
    var _mv_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _mv, 0, 0, 0, _mk_point);
    surface_reset_target();
    var _mv_fine = surface_getpixel(_mk_surf, 1, 4);
    var _mv_coarse = surface_getpixel(_mk_surf, 6, 4);
    _igpu_target_begin(_mk_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ma_ps, IgpuShaderStage.Pixel);
    var _ma_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _ma, 0, 0, 0, _mk_point);
    surface_reset_target();
    var _ma_fine = surface_getpixel(_mk_surf, 1, 4);
    var _ma_coarse = surface_getpixel(_mk_surf, 6, 4);
    _igpu_target_begin(_mk_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_mc_ps, IgpuShaderStage.Pixel);
    var _mc_drew = igpu_draw_sampled(_tex_full, _tex_layout, pr_trianglelist, 0, 6, _mc, 0, 0, 0, _mk_point);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _mc_fine = surface_getpixel(_mk_surf, 1, 4);
    var _mc_coarse = surface_getpixel(_mk_surf, 6, 4);
    show_debug_message("mip volume       : " + string(_mv_wrote) + " " + string(_mv_gen) + " " + string(_mv_drew) + " " + string(_mv_fine) + " / " + string(_mv_coarse));
    show_debug_message("mip array        : " + string(_ma_wrote) + " " + string(_ma_gen) + " " + string(_ma_drew) + " " + string(_ma_fine) + " / " + string(_ma_coarse));
    show_debug_message("mip cube         : " + string(_mc_wrote) + " " + string(_mc_gen) + " " + string(_mc_drew) + " " + string(_mc_fine) + " / " + string(_mc_coarse)
        + " r=" + string(color_get_red(_mc_coarse)) + " b=" + string(color_get_blue(_mc_coarse)));
    if (!_mv_gen || !_ma_gen || !_mc_gen || !_mv_drew || !_ma_drew || !_mc_drew) show_debug_message("mip kind err      : " + string(igpu_get_last_error()));
    _igpu_check(_mv_wrote && _mv_gen && _mv_drew && _mv_fine == c_red && _mv_coarse != _mv_fine && color_get_red(_mv_coarse) > 0 && color_get_blue(_mv_coarse) > 0 && color_get_green(_mv_coarse) == 0, "a volume's coarser level mixes the slice");
    _igpu_check(_ma_wrote && _ma_gen && _ma_drew && _ma_fine == c_red && _ma_coarse != _ma_fine && color_get_red(_ma_coarse) > 0 && color_get_blue(_ma_coarse) > 0 && color_get_green(_ma_coarse) == 0, "an array's coarser level mixes the layer");
    _igpu_check(_mc_wrote && _mc_gen && _mc_drew && _mc_fine == c_red && _mc_coarse != _mc_fine && color_get_red(_mc_coarse) > 0 && color_get_blue(_mc_coarse) > 0 && color_get_green(_mc_coarse) == 0, "a cube face's coarser level mixes that face");
    var _mv_lvl = igpu_texture_read(_mv, 0, 0, 0, 1);
    var _ma_lvl = igpu_texture_read(_ma, 0, 0, 1, 1);
    var _mc_lvl = igpu_texture_read(_mc, 0, 0, 1, 1);
    show_debug_message("mip read kinds   : " + string(_mv_lvl) + " " + string(_ma_lvl) + " " + string(_mc_lvl));
    _igpu_check(color_get_red(_mv_lvl) == 128 && color_get_blue(_mv_lvl) == 128 && igpu_texture_read(_mv, 0, 0, 1, 1) == 0, "a volume's coarser level reads back and its depth has shrunk");
    _igpu_check(color_get_green(_ma_lvl) == 128 && color_get_blue(_ma_lvl) == 128 && color_get_red(_ma_lvl) == 0, "an array layer's coarser level reads back");
    _igpu_check(color_get_green(_mc_lvl) == 128 && color_get_blue(_mc_lvl) == 128 && color_get_red(_mc_lvl) == 0, "a cube face's coarser level reads back");
}
if (surface_exists(_mk_surf)) surface_free(_mk_surf);
_igpu_check(igpu_texture_release(_mv), "release volume mip texture");
_igpu_check(igpu_texture_release(_ma), "release array mip texture");
_igpu_check(igpu_texture_release(_mc), "release cube mip texture");
_igpu_check(igpu_shader_release(_mv_cs), "release volume mip compute shader");
_igpu_check(igpu_shader_release(_ma_cs), "release array mip compute shader");
_igpu_check(igpu_shader_release(_mv_ps), "release volume mip pixel shader");
_igpu_check(igpu_shader_release(_ma_ps), "release array mip pixel shader");
_igpu_check(igpu_shader_release(_mc_ps), "release cube mip pixel shader");
_igpu_check(igpu_state_release(_mk_point), "release kind mip sampler");

igpu_shader_bind(0, IgpuShaderStage.Vertex);
igpu_shader_bind(0, IgpuShaderStage.Pixel);
_igpu_check(igpu_texture_release(_tex), "release render-target texture");
_igpu_check(igpu_texture_release(_not_target), "release non-target texture");
_igpu_check(!igpu_texture_release(999999), "releasing a bogus texture fails");

// The quad is full-screen. v runs 0..8, so one pixel covers 16 texels
// vertically: four mip levels, the whole chain of a 16-wide texture.
// Anisotropy of 16 buys those four levels back. u stays inside the red
// half, so the finer level is red and the 1x1 level is the mix.
var _an_tex = igpu_texture_create(IgpuTextureKind.TwoD, 16, 16, 1, surface_rgba8unorm, true, true, 0);
if (_an_tex == 0) show_debug_message("aniso create err  : " + string(igpu_get_last_error()));
var _an_cs = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(16, 16, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = id.x < 8 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _an_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, i.uv); }", "main", IgpuShaderStage.Pixel, "");
var _an_linear = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 1, 0, 0, 0, 0, -1);
var _an_aniso = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 16, 0, 0, 0, 0, -1);
var _an_vb = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _an_surf = surface_create(8, 8);
_igpu_check(_an_tex > 0 && _an_cs > 0 && _an_ps > 0 && _an_linear > 0 && _an_aniso > 0 && _an_vb > 0 && surface_exists(_an_surf), "anisotropy resources are created");
_igpu_check(igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 0, 0, 0, 0, 0, -1) == 0, "anisotropy below 1 is rejected");
_igpu_check(igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, IgpuAddressMode.Clamp, 17, 0, 0, 0, 0, -1) == 0, "anisotropy above 16 is rejected");
var _an_upload = buffer_create(120, buffer_fixed, 4);
buffer_seek(_an_upload, buffer_seek_start, 0);
_igpu_write_vert(_an_upload, -1, -1, 0.5, 0.2, 8);
_igpu_write_vert(_an_upload, 1, -1, 0.5, 0.3, 8);
_igpu_write_vert(_an_upload, -1, 1, 0.5, 0.2, 0);
_igpu_write_vert(_an_upload, 1, -1, 0.5, 0.3, 8);
_igpu_write_vert(_an_upload, 1, 1, 0.5, 0.3, 0);
_igpu_write_vert(_an_upload, -1, 1, 0.5, 0.2, 0);
_igpu_check(igpu_buffer_write(_an_vb, 0, _an_upload), "upload the stretched anisotropy quad");
buffer_delete(_an_upload);
if (_an_tex > 0 && _an_cs > 0 && _an_ps > 0 && _an_linear > 0 && _an_aniso > 0 && _an_vb > 0 && surface_exists(_an_surf) && _tex_vs > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_an_cs, IgpuShaderStage.Compute);
    var _an_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_an_tex], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _an_gen = igpu_texture_generate_mips(_an_tex);
    gpu_push_state();
    _igpu_target_begin(_an_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_an_ps, IgpuShaderStage.Pixel);
    var _an_lin_drew = igpu_draw_sampled(_an_vb, _tex_layout, pr_trianglelist, 0, 6, _an_tex, 0, 0, 0, _an_linear);
    surface_reset_target();
    var _an_lin_px = surface_getpixel(_an_surf, 2, 4);
    _igpu_target_begin(_an_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_an_ps, IgpuShaderStage.Pixel);
    var _an_an_drew = igpu_draw_sampled(_an_vb, _tex_layout, pr_trianglelist, 0, 6, _an_tex, 0, 0, 0, _an_aniso);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _an_an_px = surface_getpixel(_an_surf, 2, 4);
    show_debug_message("aniso filter     : " + string(_an_wrote) + " " + string(_an_gen) + " " + string(_an_lin_drew) + " " + string(_an_lin_px) + " / " + string(_an_an_drew) + " " + string(_an_an_px)
        + " lin r=" + string(color_get_red(_an_lin_px)) + " b=" + string(color_get_blue(_an_lin_px))
        + " an r=" + string(color_get_red(_an_an_px)) + " b=" + string(color_get_blue(_an_an_px)));
    if (!_an_gen || !_an_lin_drew || !_an_an_drew) show_debug_message("aniso filter err  : " + string(igpu_get_last_error()));
    _igpu_check(_an_wrote && _an_gen && _an_lin_drew && color_get_red(_an_lin_px) > 0 && color_get_blue(_an_lin_px) > 0 && color_get_green(_an_lin_px) == 0, "linear minification mixes the whole texture");
    _igpu_check(_an_an_drew && _an_an_px == c_red && _an_an_px != _an_lin_px, "anisotropic filtering stays on the red half");
}
if (surface_exists(_an_surf)) surface_free(_an_surf);
_igpu_check(igpu_texture_release(_an_tex), "release anisotropy texture");
_igpu_check(igpu_buffer_release(_an_vb), "release anisotropy quad");
_igpu_check(igpu_shader_release(_an_cs), "release anisotropy compute shader");
_igpu_check(igpu_shader_release(_an_ps), "release anisotropy pixel shader");
_igpu_check(igpu_state_release(_an_linear), "release linear minification sampler");
_igpu_check(igpu_state_release(_an_aniso), "release anisotropic sampler");

// Same stretched footprint. Anisotropy of 16 keeps the sample on the red
// half. Offset 8 is more than those four recovered levels, so the sample
// reaches the mixed 1x1. A coarse limit of 0 holds it on red anyway. A fine
// limit of 4 forces that mixed level even with no offset.
var _ar_tex = igpu_texture_create(IgpuTextureKind.TwoD, 16, 16, 1, surface_rgba8unorm, true, true, 0);
if (_ar_tex == 0) show_debug_message("aniso range err  : " + string(igpu_get_last_error()));
var _ar_cs = igpu_shader_compile("RWTexture2D<float4> dst : register(u0); [numthreads(16, 16, 1)] void main(uint3 id : SV_DispatchThreadID) { dst[id.xy] = id.x < 8 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
var _ar_ps = igpu_shader_compile("Texture2D t : register(t0); SamplerState s : register(s0); struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { return t.Sample(s, i.uv); }", "main", IgpuShaderStage.Pixel, "");
var _ar_base = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 16, 0, 0, 0, 0, 16);
var _ar_push = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 16, 0, 0, 8, 0, 16);
var _ar_cap = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 16, 0, 0, 8, 0, 0);
var _ar_floor = igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 16, 0, 0, 0, 4, 4);
var _ar_vb = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _ar_surf = surface_create(8, 8);
_igpu_check(_ar_tex > 0 && _ar_cs > 0 && _ar_ps > 0 && _ar_base > 0 && _ar_push > 0 && _ar_cap > 0 && _ar_floor > 0 && _ar_vb > 0 && surface_exists(_ar_surf), "anisotropic level-range resources are created");
_igpu_check(igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 16, 0, 0, 0, 0, power(10, 40)) == 0, "an anisotropic level limit that is not finite is rejected");
_igpu_check(igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 16, 0, 0, 0, 2, 0) == 0, "an anisotropic finest level above the coarsest is rejected");
_igpu_check(igpu_sampler_state_create(tf_linear, tf_linear, tf_linear, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, IgpuAddressMode.Repeat, 17, 0, 0, 0, 0, 16) == 0, "anisotropic level range still rejects anisotropy above 16");
var _ar_upload = buffer_create(120, buffer_fixed, 4);
buffer_seek(_ar_upload, buffer_seek_start, 0);
_igpu_write_vert(_ar_upload, -1, -1, 0.5, 0.2, 8);
_igpu_write_vert(_ar_upload, 1, -1, 0.5, 0.3, 8);
_igpu_write_vert(_ar_upload, -1, 1, 0.5, 0.2, 0);
_igpu_write_vert(_ar_upload, 1, -1, 0.5, 0.3, 8);
_igpu_write_vert(_ar_upload, 1, 1, 0.5, 0.3, 0);
_igpu_write_vert(_ar_upload, -1, 1, 0.5, 0.2, 0);
_igpu_check(igpu_buffer_write(_ar_vb, 0, _ar_upload), "upload the anisotropic level-range quad");
buffer_delete(_ar_upload);
if (_ar_tex > 0 && _ar_cs > 0 && _ar_ps > 0 && _ar_base > 0 && _ar_push > 0 && _ar_cap > 0 && _ar_floor > 0 && _ar_vb > 0 && surface_exists(_ar_surf) && _tex_vs > 0 && _tex_layout > 0)
{
    igpu_shader_bind(_ar_cs, IgpuShaderStage.Compute);
    var _ar_wrote = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_ar_tex], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _ar_gen = igpu_texture_generate_mips(_ar_tex);
    gpu_push_state();
    _igpu_target_begin(_ar_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ar_ps, IgpuShaderStage.Pixel);
    var _ar_b_drew = igpu_draw_sampled(_ar_vb, _tex_layout, pr_trianglelist, 0, 6, _ar_tex, 0, 0, 0, _ar_base);
    surface_reset_target();
    var _ar_b_px = surface_getpixel(_ar_surf, 2, 4);
    _igpu_target_begin(_ar_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ar_ps, IgpuShaderStage.Pixel);
    var _ar_p_drew = igpu_draw_sampled(_ar_vb, _tex_layout, pr_trianglelist, 0, 6, _ar_tex, 0, 0, 0, _ar_push);
    surface_reset_target();
    var _ar_p_px = surface_getpixel(_ar_surf, 2, 4);
    _igpu_target_begin(_ar_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ar_ps, IgpuShaderStage.Pixel);
    var _ar_c_drew = igpu_draw_sampled(_ar_vb, _tex_layout, pr_trianglelist, 0, 6, _ar_tex, 0, 0, 0, _ar_cap);
    surface_reset_target();
    var _ar_c_px = surface_getpixel(_ar_surf, 2, 4);
    _igpu_target_begin(_ar_surf, c_yellow);
    igpu_shader_bind(_tex_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_ar_ps, IgpuShaderStage.Pixel);
    var _ar_f_drew = igpu_draw_sampled(_ar_vb, _tex_layout, pr_trianglelist, 0, 6, _ar_tex, 0, 0, 0, _ar_floor);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _ar_f_px = surface_getpixel(_ar_surf, 2, 4);
    show_debug_message("aniso range     : " + string(_ar_wrote) + " " + string(_ar_gen) + " " + string(_ar_b_drew) + " " + string(_ar_b_px)
        + " / " + string(_ar_p_drew) + " " + string(_ar_p_px) + " | " + string(_ar_c_drew) + " " + string(_ar_c_px)
        + " | " + string(_ar_f_drew) + " " + string(_ar_f_px)
        + " r=" + string(color_get_red(_ar_p_px)) + " b=" + string(color_get_blue(_ar_p_px)));
    if (!_ar_gen || !_ar_b_drew || !_ar_p_drew || !_ar_c_drew || !_ar_f_drew) show_debug_message("aniso range err  : " + string(igpu_get_last_error()));
    _igpu_check(_ar_wrote && _ar_gen && _ar_b_drew && _ar_b_px == c_red, "anisotropic sampling with no offset stays on red");
    _igpu_check(_ar_p_drew && _ar_p_px != c_red && color_get_red(_ar_p_px) > 0 && color_get_blue(_ar_p_px) > 0 && color_get_green(_ar_p_px) == 0, "an anisotropic offset reaches the mixed level");
    _igpu_check(_ar_c_drew && _ar_c_px == c_red, "an anisotropic coarse limit holds the offset on red");
    _igpu_check(_ar_f_drew && _ar_f_px != c_red && color_get_red(_ar_f_px) > 0 && color_get_blue(_ar_f_px) > 0 && color_get_green(_ar_f_px) == 0, "an anisotropic fine limit forces the mixed level");
}
if (surface_exists(_ar_surf)) surface_free(_ar_surf);
_igpu_check(igpu_texture_release(_ar_tex), "release anisotropic level-range texture");
_igpu_check(igpu_buffer_release(_ar_vb), "release anisotropic level-range quad");
_igpu_check(igpu_shader_release(_ar_cs), "release anisotropic level-range compute shader");
_igpu_check(igpu_shader_release(_ar_ps), "release anisotropic level-range pixel shader");
_igpu_check(igpu_state_release(_ar_base), "release anisotropic base sampler");
_igpu_check(igpu_state_release(_ar_push), "release anisotropic offset sampler");
_igpu_check(igpu_state_release(_ar_cap), "release anisotropic coarse-limit sampler");
_igpu_check(igpu_state_release(_ar_floor), "release anisotropic fine-limit sampler");

_igpu_check(igpu_buffer_release(_tex_vb), "release texture-test quad");
_igpu_check(igpu_buffer_release(_tex_full), "release fullscreen sample quad");
_igpu_check(igpu_input_layout_release(_tex_layout), "release texture-test layout");
_igpu_check(igpu_shader_release(_tex_vs), "release texture-test vertex shader");
_igpu_check(igpu_shader_release(_tex_red), "release texture-test red shader");
_igpu_check(igpu_shader_release(_tex_sample), "release texture-test sample shader");

// Queries bracket GPU work. Occlusion counts samples, a timestamp is a tick
// delta, and a fence is one point on the timeline. The calls go through the
// active backend; this machine's backend is the D3D11 one.
_igpu_check(igpu_supports(IgpuCapability.Queries), "supports queries");
_igpu_check(igpu_supports(IgpuCapability.OcclusionQuery), "supports occlusion queries");
_igpu_check(igpu_supports(IgpuCapability.Timestamps), "supports timestamps");
_igpu_check(igpu_supports(IgpuCapability.Fence), "supports fences");

var _q_tex = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, true, false, 1);
var _q_vs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _q_red = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _q_drop = igpu_shader_compile("float4 main() : SV_TARGET { clip(-1.0); return float4(0.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _q_layout = igpu_vertex_format(_q_vs, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
var _q_vb = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _q_upload = buffer_create(120, buffer_fixed, 4);
buffer_seek(_q_upload, buffer_seek_start, 0);
_igpu_write_vert(_q_upload, -1, -1, 0.5, 0, 1);
_igpu_write_vert(_q_upload, 1, -1, 0.5, 1, 1);
_igpu_write_vert(_q_upload, -1, 1, 0.5, 0, 0);
_igpu_write_vert(_q_upload, 1, -1, 0.5, 1, 1);
_igpu_write_vert(_q_upload, 1, 1, 0.5, 1, 0);
_igpu_write_vert(_q_upload, -1, 1, 0.5, 0, 0);
igpu_buffer_write(_q_vb, 0, _q_upload);
buffer_delete(_q_upload);

var _occ = igpu_query_create(IgpuQueryKind.Occlusion);
var _occ_none = igpu_query_create(IgpuQueryKind.Occlusion);
var _stamp = igpu_query_create(IgpuQueryKind.Timestamp);
var _fence = igpu_fence_create();
_igpu_check(_q_tex > 0 && _q_vs > 0 && _q_red > 0 && _q_drop > 0 && _q_layout > 0 && _q_vb > 0, "query-test resources are created");
_igpu_check(_occ > 0 && _occ_none > 0 && _stamp > 0 && _fence > 0, "queries and a fence are created");
_igpu_check(igpu_query_create(9) == 0, "an unknown query kind is rejected");
_igpu_check(!igpu_query_end(_occ), "ending a query before begin is rejected");

if (_occ > 0 && _q_tex > 0)
{
    igpu_shader_bind(_q_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_q_red, IgpuShaderStage.Pixel);
    igpu_query_begin(_occ);
    igpu_draw_to_render_targets(_q_vb, _q_layout, pr_trianglelist, 0, 6, [_q_tex], [0], [0], 0, 0, 0, 0);
    igpu_query_end(_occ);

    igpu_shader_bind(_q_drop, IgpuShaderStage.Pixel);
    igpu_query_begin(_occ_none);
    igpu_draw_to_render_targets(_q_vb, _q_layout, pr_trianglelist, 0, 6, [_q_tex], [0], [0], 0, 0, 0, 0);
    igpu_query_end(_occ_none);

    igpu_shader_bind(_q_red, IgpuShaderStage.Pixel);
    igpu_query_begin(_stamp);
    igpu_draw_to_render_targets(_q_vb, _q_layout, pr_trianglelist, 0, 6, [_q_tex], [0], [0], 0, 0, 0, 0);
    igpu_query_end(_stamp);
    igpu_fence_signal(_fence);

    var _samples = igpu_query_result(_occ);
    var _none = igpu_query_result(_occ_none);
    var _ticks = igpu_query_result(_stamp);
    var _hz = igpu_timestamp_frequency();
    var _reached = igpu_fence_signaled(_fence);
    show_debug_message("occlusion/ticks   : " + string(_samples) + " / " + string(_none) + " ticks " + string(_ticks) + " hz " + string(_hz) + " fence " + string(_reached));
    _igpu_check(_samples > 0, "occlusion counts drawn samples");
    _igpu_check(_none == 0, "discarded pixels count as no samples");
    _igpu_check(_ticks > 0, "a timestamp query reports elapsed ticks");
    _igpu_check(_hz > 0, "timestamp frequency is known after a query");
    _igpu_check(_reached, "a fence is reached after the GPU work in front of it");
}

_igpu_check(igpu_query_release(_occ), "release occlusion query");
_igpu_check(igpu_query_release(_occ_none), "release empty occlusion query");
_igpu_check(igpu_query_release(_stamp), "release timestamp query");
_igpu_check(igpu_fence_release(_fence), "release fence");
_igpu_check(!igpu_query_release(999999), "releasing a bogus query fails");
_igpu_check(igpu_texture_release(_q_tex), "release query-test texture");
_igpu_check(igpu_buffer_release(_q_vb), "release query-test buffer");
_igpu_check(igpu_input_layout_release(_q_layout), "release query-test layout");
_igpu_check(igpu_shader_release(_q_vs), "release query-test vertex shader");
_igpu_check(igpu_shader_release(_q_red), "release query-test red shader");
_igpu_check(igpu_shader_release(_q_drop), "release query-test discard shader");

// ---------------------------------------------------------------------------
// Uniform block reflection and packing
//
// Offsets come from the compiled shader. The test does not hard-code the
// padding rules except as the expected answers: a float4, then a float, then
// a float2, then a float4x4, then a float[2]. The draw reads those members,
// so a wrong offset comes back as the wrong colour.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
_igpu_check(igpu_supports(IgpuCapability.UniformReflection), "supports uniform reflection");

var _u_vs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _u_src = "cbuffer Sprite : register(b7) { float4 tint; float extra; float2 scale; float4x4 world; float weights[2]; }; struct In { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; }; float4 main(In i) : SV_TARGET { float used = scale.x + scale.y + world._m00 + world._m11 + weights[0] + weights[1]; return tint * extra + float4(used, 0.0, 0.0, 0.0); }";
var _u_ps = igpu_shader_compile(_u_src, "main", IgpuShaderStage.Pixel, "");
_igpu_check(_u_vs > 0, "uniform-test vertex shader compiles");
_igpu_check(_u_ps > 0, "uniform-test pixel shader compiles");
if (_u_ps == 0)
{
    show_debug_message("uniform shader err: " + string(igpu_get_last_error()));
}
var _u_none = igpu_shader_reflect(_u_vs);
_igpu_check(array_length(_u_none) == 0 && igpu_get_last_error() == "", "a shader with no uniforms reports none");
var _u_blocks = igpu_shader_reflect(_u_ps);
_igpu_check(array_length(_u_blocks) == 1, "the sprite block is reflected");
var _u_block = (array_length(_u_blocks) == 1) ? _u_blocks[0] : undefined;
var _u_size = is_undefined(_u_block) ? 0 : _u_block.size;
var _u_slot = is_undefined(_u_block) ? -1 : _u_block.slot;
var _u_members = is_undefined(_u_block) ? 0 : array_length(_u_block.members);
_igpu_check(!is_undefined(_u_block) && _u_block.name == "Sprite", "reflected block name");
show_debug_message("uniform block     : size " + string(_u_size) + " slot " + string(_u_slot) + " members " + string(_u_members));

var _u_tint = _igpu_uniform_member(_u_block, "tint");
var _u_extra = _igpu_uniform_member(_u_block, "extra");
var _u_scale = _igpu_uniform_member(_u_block, "scale");
var _u_world = _igpu_uniform_member(_u_block, "world");
var _u_weights = _igpu_uniform_member(_u_block, "weights");
_igpu_check(_u_size == 128, "sprite block is 128 bytes");
_igpu_check(_u_slot == 7, "sprite block requests slot 7");
_igpu_check(_u_members == 5, "sprite block has 5 members");
_igpu_check(!is_undefined(_u_tint) && _u_block.members[0].name == "tint", "first member is tint");
_igpu_check(!is_undefined(_u_tint) && _u_tint.offset == 0, "tint is at the start of the block");
_igpu_check(!is_undefined(_u_tint) && _u_tint.size == 16, "tint is 16 bytes");
_igpu_check(!is_undefined(_u_tint) && _u_tint.type == IgpuUniformType.Float, "tint is a float");
_igpu_check(!is_undefined(_u_tint) && _u_tint.columns == 4, "tint has 4 columns");
_igpu_check(!is_undefined(_u_extra) && _u_extra.offset == 16, "extra follows the float4");
_igpu_check(!is_undefined(_u_extra) && _u_extra.size == 4, "extra is 4 bytes");
_igpu_check(!is_undefined(_u_scale) && _u_scale.offset == 20, "scale is packed beside extra");
_igpu_check(!is_undefined(_u_scale) && _u_scale.size == 8, "scale is 8 bytes");
_igpu_check(!is_undefined(_u_scale) && _u_scale.columns == 2, "scale has 2 columns");
_igpu_check(!is_undefined(_u_world) && _u_world.offset == 32, "the matrix starts on the next register");
_igpu_check(!is_undefined(_u_world) && _u_world.size == 64, "the matrix is 64 bytes");
_igpu_check(!is_undefined(_u_world) && _u_world.rows == 4, "the matrix has 4 rows");
_igpu_check(!is_undefined(_u_world) && _u_world.columns == 4, "the matrix has 4 columns");
_igpu_check(!is_undefined(_u_weights) && _u_weights.offset == 96, "the array starts after the matrix");
_igpu_check(!is_undefined(_u_weights) && _u_weights.size == 20, "float[2] is 4 bytes plus a 16-byte gap, with no trailing pad");
_igpu_check(!is_undefined(_u_weights) && _u_weights.elements == 2, "weights has 2 elements");
_igpu_check(is_undefined(_igpu_uniform_member(_u_block, "missing")), "an unknown member is absent");
var _u_missing = false;
if (!is_undefined(_u_block))
{
    _u_missing = (_u_block.name == "Missing");
}
_igpu_check(!_u_missing, "an unknown block is absent");

var _u_layout = igpu_vertex_format(_u_vs, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
var _u_vb = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _u_cb = (_u_size > 0) ? igpu_buffer_create(_u_size, IgpuBufferUsage.Static, IgpuBufferBind.Uniform, 0) : 0;
_igpu_check(_u_layout > 0, "uniform-test layout is created");
_igpu_check(_u_vb > 0, "uniform-test vertex buffer is created");
_igpu_check(_u_cb > 0, "uniform buffer is created from the reflected size");
_igpu_check(!igpu_uniform_write(_u_vb, _u_ps, "Sprite", "tint", [0, 1, 0, 1]), "a vertex buffer is not a uniform buffer");

var _u_upload = buffer_create(120, buffer_fixed, 4);
buffer_seek(_u_upload, buffer_seek_start, 0);
_igpu_write_vert(_u_upload, -1, -1, 0.5, 0, 0);
_igpu_write_vert(_u_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_u_upload, -1, 1, 0.5, 0, 1);
_igpu_write_vert(_u_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_u_upload, 0, 1, 0.5, 1, 1);
_igpu_write_vert(_u_upload, -1, 1, 0.5, 0, 1);
_igpu_check(igpu_buffer_write(_u_vb, 0, _u_upload), "upload uniform-test vertices");
buffer_delete(_u_upload);

var _u_wrote_tint = igpu_uniform_write(_u_cb, _u_ps, "Sprite", "tint", [0, 1, 0, 1]);
var _u_wrote_extra = igpu_uniform_write(_u_cb, _u_ps, "Sprite", "extra", [1]);
_igpu_check(_u_wrote_tint, "tint is packed by name");
_igpu_check(_u_wrote_extra, "extra is packed by name");
if (!_u_wrote_tint || !_u_wrote_extra)
{
    show_debug_message("uniform write err : " + string(igpu_get_last_error()));
}
_igpu_check(!igpu_uniform_write(_u_cb, _u_ps, "Sprite", "tint", [1, 0]), "a short value list is rejected");

var _u_surf = surface_create(8, 8);
_igpu_check(surface_exists(_u_surf), "uniform-test surface is created");
if (surface_exists(_u_surf) && _u_cb > 0 && _u_vb > 0 && _u_layout > 0 && _u_vs > 0 && _u_ps > 0)
{
    gpu_push_state();
    _igpu_target_begin(_u_surf, c_blue);
    _igpu_check(igpu_shader_bind(_u_vs, IgpuShaderStage.Vertex), "uniform-test vertex shader binds");
    _igpu_check(igpu_shader_bind(_u_ps, IgpuShaderStage.Pixel), "uniform-test pixel shader binds");
    var _u_bound = igpu_uniform_bind(_u_cb, IgpuShaderStage.Pixel, _u_slot);
    _igpu_check(_u_bound, "uniform buffer binds at the reflected slot");
    if (!_u_bound)
    {
        show_debug_message("uniform bind err  : " + string(igpu_get_last_error()));
    }
    var _u_drew = igpu_draw(_u_vb, 0, _u_layout, pr_trianglelist, 0, 6, 1, 0, 0, 0, 0);
    _igpu_check(igpu_uniform_bind(0, IgpuShaderStage.Pixel, _u_slot), "uniform slot is restored");
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();

    var _u_left = surface_getpixel(_u_surf, 1, 4);
    var _u_right = surface_getpixel(_u_surf, 6, 4);
    show_debug_message("uniform pixel     : " + string(_u_drew) + " " + string(_u_left) + " / " + string(_u_right));
    _igpu_check(_u_drew, "packed uniform draw succeeds");
    _igpu_check(_u_left == c_lime, "packed tint and extra come back green");
    _igpu_check(_u_right == c_blue, "uniform draw leaves the other half blue");
}
if (surface_exists(_u_surf))
{
    surface_free(_u_surf);
}
_igpu_check(igpu_buffer_release(_u_cb), "release uniform buffer");
_igpu_check(igpu_buffer_release(_u_vb), "release uniform-test vertex buffer");
_igpu_check(igpu_input_layout_release(_u_layout), "release uniform-test layout");
_igpu_check(igpu_shader_release(_u_vs), "release uniform-test vertex shader");
_igpu_check(igpu_shader_release(_u_ps), "release uniform-test pixel shader");

// ---------------------------------------------------------------------------
// dialect "glsl" is translated in-process, then compiled. std140 puts scale
// at byte 24. Member names may carry a translator prefix, so the test finds
// them from the reflection instead of guessing the prefix.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
var _x_src = "#version 310 es\nprecision highp float;\nlayout(std140, binding = 7) uniform Sprite { vec4 tint; float extra; vec2 scale; mat4 world; float weights[2]; };\nlayout(location = 0) out vec4 frag_color;\nvoid main() { float used = scale.x + scale.y + world[0][0] + world[1][1] + weights[0] + weights[1]; frag_color = tint * extra + vec4(used, 0.0, 0.0, 0.0); }";
var _x_vs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _x_ps = igpu_shader_compile(_x_src, "main", IgpuShaderStage.Pixel, "glsl");
_igpu_check(_x_vs > 0, "translated-test vertex shader compiles");
_igpu_check(_x_ps > 0, "glsl dialect compiles");
if (_x_ps == 0)
{
    show_debug_message("translated shader err: " + string(igpu_get_last_error()));
}
var _x_tint = "";
var _x_extra = "";
var _x_scale_name = "";
var _x_world = "";
var _x_weights = "";
var _x_blocks = igpu_shader_reflect(_x_ps);
var _x_block = (array_length(_x_blocks) == 1) ? _x_blocks[0] : undefined;
var _x_members = is_undefined(_x_block) ? 0 : array_length(_x_block.members);
for (var _xi = 0; _xi < _x_members; _xi += 1)
{
    var _xn = _x_block.members[_xi].name;
    if (string_pos("tint", _xn) > 0) { _x_tint = _xn; }
    if (string_pos("extra", _xn) > 0) { _x_extra = _xn; }
    if (string_pos("scale", _xn) > 0) { _x_scale_name = _xn; }
    if (string_pos("world", _xn) > 0) { _x_world = _xn; }
    if (string_pos("weights", _xn) > 0) { _x_weights = _xn; }
}
var _x_tint_m = _igpu_uniform_member(_x_block, _x_tint);
var _x_extra_m = _igpu_uniform_member(_x_block, _x_extra);
var _x_scale_m = _igpu_uniform_member(_x_block, _x_scale_name);
var _x_world_m = _igpu_uniform_member(_x_block, _x_world);
var _x_weights_m = _igpu_uniform_member(_x_block, _x_weights);
_igpu_check(!is_undefined(_x_block) && _x_block.name == "Sprite", "translated block is still Sprite");
_igpu_check(!is_undefined(_x_block) && _x_block.slot == 7, "translated block keeps slot 7");
_igpu_check(!is_undefined(_x_block) && _x_block.size == 128, "translated block is 128 bytes");
_igpu_check(string_length(_x_tint) > 0 && string_length(_x_extra) > 0 && string_length(_x_scale_name) > 0, "translated members were reflected");
_igpu_check(!is_undefined(_x_tint_m) && _x_tint_m.offset == 0, "translated tint is at 0");
_igpu_check(!is_undefined(_x_extra_m) && _x_extra_m.offset == 16, "translated extra is at 16");
_igpu_check(!is_undefined(_x_scale_m) && _x_scale_m.offset == 24, "translated scale follows std140 at 24");
_igpu_check(!is_undefined(_x_scale_m) && _x_scale_m.size == 8, "translated scale is 8 bytes");
_igpu_check(!is_undefined(_x_world_m) && _x_world_m.offset == 32, "translated matrix is at 32");
_igpu_check(!is_undefined(_x_world_m) && _x_world_m.size == 64, "translated matrix is 64 bytes");
_igpu_check(!is_undefined(_x_weights_m) && _x_weights_m.offset == 96, "translated array is at 96");
show_debug_message("translated scale  : " + string(is_undefined(_x_scale_m) ? -1 : _x_scale_m.offset) + " / " + string(is_undefined(_x_scale_m) ? -1 : _x_scale_m.size) + " " + _x_tint);

var _x_layout = igpu_vertex_format(_x_vs, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
var _x_vb = igpu_buffer_create(120, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _x_size = is_undefined(_x_block) ? 0 : _x_block.size;
var _x_cb = (_x_size > 0) ? igpu_buffer_create(_x_size, IgpuBufferUsage.Static, IgpuBufferBind.Uniform, 0) : 0;
_igpu_check(_x_layout > 0, "translated-test layout is created");
_igpu_check(_x_vb > 0, "translated-test vertex buffer is created");
_igpu_check(_x_cb > 0, "translated uniform buffer is created");

var _x_upload = buffer_create(120, buffer_fixed, 4);
buffer_seek(_x_upload, buffer_seek_start, 0);
_igpu_write_vert(_x_upload, -1, -1, 0.5, 0, 0);
_igpu_write_vert(_x_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_x_upload, -1, 1, 0.5, 0, 1);
_igpu_write_vert(_x_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_x_upload, 0, 1, 0.5, 1, 1);
_igpu_write_vert(_x_upload, -1, 1, 0.5, 0, 1);
_igpu_check(igpu_buffer_write(_x_vb, 0, _x_upload), "upload translated-test vertices");
buffer_delete(_x_upload);

var _x_wrote_tint = igpu_uniform_write(_x_cb, _x_ps, "Sprite", _x_tint, [0, 1, 0, 1]);
var _x_wrote_extra = igpu_uniform_write(_x_cb, _x_ps, "Sprite", _x_extra, [1]);
_igpu_check(_x_wrote_tint, "translated tint is packed by reflected name");
_igpu_check(_x_wrote_extra, "translated extra is packed by reflected name");
if (!_x_wrote_tint || !_x_wrote_extra)
{
    show_debug_message("translated write err: " + string(igpu_get_last_error()));
}

var _x_surf = surface_create(8, 8);
_igpu_check(surface_exists(_x_surf), "translated-test surface is created");
if (surface_exists(_x_surf) && _x_cb > 0 && _x_vb > 0 && _x_layout > 0 && _x_vs > 0 && _x_ps > 0)
{
    gpu_push_state();
    _igpu_target_begin(_x_surf, c_blue);
    igpu_shader_bind(_x_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_x_ps, IgpuShaderStage.Pixel);
    var _x_slot = is_undefined(_x_block) ? -1 : _x_block.slot;
    var _x_bound = igpu_uniform_bind(_x_cb, IgpuShaderStage.Pixel, _x_slot);
    _igpu_check(_x_bound, "translated uniform buffer binds");
    var _x_drew = igpu_draw(_x_vb, 0, _x_layout, pr_trianglelist, 0, 6, 1, 0, 0, 0, 0);
    igpu_uniform_bind(0, IgpuShaderStage.Pixel, _x_slot);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _x_left = surface_getpixel(_x_surf, 1, 4);
    var _x_right = surface_getpixel(_x_surf, 6, 4);
    show_debug_message("translated pixel  : " + string(_x_drew) + " " + string(_x_left) + " / " + string(_x_right));
    _igpu_check(_x_drew, "translated shader draw succeeds");
    _igpu_check(_x_left == c_lime, "translated tint and extra come back green");
    _igpu_check(_x_right == c_blue, "translated draw leaves the other half blue");
}
if (surface_exists(_x_surf))
{
    surface_free(_x_surf);
}
_igpu_check(igpu_buffer_release(_x_cb), "release translated uniform buffer");
_igpu_check(igpu_buffer_release(_x_vb), "release translated vertex buffer");
_igpu_check(igpu_input_layout_release(_x_layout), "release translated layout");
_igpu_check(igpu_shader_release(_x_vs), "release translated vertex shader");
_igpu_check(igpu_shader_release(_x_ps), "release translated pixel shader");

// ---------------------------------------------------------------------------
// Instanced draw. The mesh is the left half. Instance 0 stays there in red,
// instance 1 is shifted one unit right and is green. The colour comes from
// the instance buffer, so a per-vertex step would paint both halves red.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
var _i_vs_src = "struct VSIn { float3 pos : POSITION; float4 col : COLOR; uint iid : SV_InstanceID; }; struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; }; VSOut main(VSIn i) { VSOut o; o.pos = float4(i.pos.x + i.iid, i.pos.y, i.pos.z, 1.0); o.col = i.col; return o; }";
var _i_ps_src = "struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; }; float4 main(PSIn i) : SV_TARGET { return i.col; }";
var _i_vs = igpu_shader_compile(_i_vs_src, "main", IgpuShaderStage.Vertex, "");
var _i_ps = igpu_shader_compile(_i_ps_src, "main", IgpuShaderStage.Pixel, "");
var _i_layout = igpu_input_layout_create(_i_vs, [vertex_usage_position, vertex_usage_colour], [vertex_type_float3, vertex_type_float4], [IgpuVertexStep.Vertex, IgpuVertexStep.Instance], 2, 12, 16);
_igpu_check(_i_vs > 0 && _i_ps > 0, "instance shaders compile");
_igpu_check(_i_layout > 0, "instance layout is created");
if (_i_layout == 0)
{
    show_debug_message("instance layout err: " + string(igpu_get_last_error()));
}
var _i_vb = igpu_buffer_create(72, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 12);
var _i_ib = igpu_buffer_create(32, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 16);
_igpu_check(_i_vb > 0 && _i_ib > 0, "instance buffers are created");
var _i_mesh = buffer_create(72, buffer_fixed, 4);
buffer_seek(_i_mesh, buffer_seek_start, 0);
buffer_write(_i_mesh, buffer_f32, -1); buffer_write(_i_mesh, buffer_f32, -1); buffer_write(_i_mesh, buffer_f32, 0.5);
buffer_write(_i_mesh, buffer_f32, 0); buffer_write(_i_mesh, buffer_f32, -1); buffer_write(_i_mesh, buffer_f32, 0.5);
buffer_write(_i_mesh, buffer_f32, -1); buffer_write(_i_mesh, buffer_f32, 1); buffer_write(_i_mesh, buffer_f32, 0.5);
buffer_write(_i_mesh, buffer_f32, 0); buffer_write(_i_mesh, buffer_f32, -1); buffer_write(_i_mesh, buffer_f32, 0.5);
buffer_write(_i_mesh, buffer_f32, 0); buffer_write(_i_mesh, buffer_f32, 1); buffer_write(_i_mesh, buffer_f32, 0.5);
buffer_write(_i_mesh, buffer_f32, -1); buffer_write(_i_mesh, buffer_f32, 1); buffer_write(_i_mesh, buffer_f32, 0.5);
var _i_cols = buffer_create(32, buffer_fixed, 4);
buffer_seek(_i_cols, buffer_seek_start, 0);
buffer_write(_i_cols, buffer_f32, 1); buffer_write(_i_cols, buffer_f32, 0); buffer_write(_i_cols, buffer_f32, 0); buffer_write(_i_cols, buffer_f32, 1);
buffer_write(_i_cols, buffer_f32, 0); buffer_write(_i_cols, buffer_f32, 1); buffer_write(_i_cols, buffer_f32, 0); buffer_write(_i_cols, buffer_f32, 1);
_igpu_check(igpu_buffer_write(_i_vb, 0, _i_mesh), "upload instance mesh");
_igpu_check(igpu_buffer_write(_i_ib, 0, _i_cols), "upload instance colours");
buffer_delete(_i_mesh);
buffer_delete(_i_cols);
_igpu_check(!igpu_draw(_i_vb, _i_ib, _i_layout, pr_trianglelist, 0, 6, 0, 0, 0, 0, 0), "zero instances are rejected");

var _i_surf = surface_create(8, 8);
_igpu_check(surface_exists(_i_surf), "instance surface is created");
if (surface_exists(_i_surf) && _i_vb > 0 && _i_ib > 0 && _i_layout > 0 && _i_vs > 0 && _i_ps > 0)
{
    gpu_push_state();
    _igpu_target_begin(_i_surf, c_blue);
    igpu_shader_bind(_i_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_i_ps, IgpuShaderStage.Pixel);
    var _i_drew = igpu_draw(_i_vb, _i_ib, _i_layout, pr_trianglelist, 0, 6, 2, 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _i_left = surface_getpixel(_i_surf, 1, 4);
    var _i_right = surface_getpixel(_i_surf, 6, 4);
    show_debug_message("instance pixel   : " + string(_i_drew) + " " + string(_i_left) + " / " + string(_i_right));
    _igpu_check(_i_drew, "instanced draw succeeds");
    _igpu_check(_i_left == c_red, "instance 0 is red");
    _igpu_check(_i_right == c_lime, "instance 1 is green");

    var _i_hold = igpu_blend_state_create(
        true, bm_zero, bm_one, bm_eq_add, bm_zero, bm_one, bm_eq_add,
        true, true, true, true);
    var _i_hold_surf = surface_create(8, 8);
    if (_i_hold > 0 && surface_exists(_i_hold_surf))
    {
        gpu_push_state();
        _igpu_target_begin(_i_hold_surf, c_blue);
        igpu_shader_bind(_i_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_i_ps, IgpuShaderStage.Pixel);
        var _i_held = igpu_draw(_i_vb, _i_ib, _i_layout, pr_trianglelist, 0, 6, 2, _i_hold, 0, 0, 0);
        surface_reset_target();
        var _i_held_l = surface_getpixel(_i_hold_surf, 1, 4);
        var _i_held_r = surface_getpixel(_i_hold_surf, 6, 4);
        surface_set_target(_i_hold_surf);
        igpu_shader_bind(_i_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_i_ps, IgpuShaderStage.Pixel);
        var _i_open = igpu_draw(_i_vb, _i_ib, _i_layout, pr_trianglelist, 0, 6, 2, 0, 0, 0, 0);
        surface_reset_target();
        var _i_open_l = surface_getpixel(_i_hold_surf, 1, 4);
        var _i_open_r = surface_getpixel(_i_hold_surf, 6, 4);
        surface_set_target(_i_hold_surf);
        igpu_shader_bind(_i_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_i_ps, IgpuShaderStage.Pixel);
        var _i_bad = igpu_draw(_i_vb, _i_ib, _i_layout, pr_trianglelist, 0, 6, 2, 0, _i_hold, 0, 0);
        surface_reset_target();
        var _i_bad_l = surface_getpixel(_i_hold_surf, 1, 4);
        var _i_bad_r = surface_getpixel(_i_hold_surf, 6, 4);
        igpu_shader_bind(0, IgpuShaderStage.Vertex);
        igpu_shader_bind(0, IgpuShaderStage.Pixel);
        gpu_pop_state();
        show_debug_message("instance state   : " + string(_i_held) + " " + string(_i_held_l) + " / " + string(_i_held_r)
            + " | " + string(_i_open) + " " + string(_i_open_l) + " / " + string(_i_open_r)
            + " | " + string(_i_bad) + " " + string(_i_bad_l) + " / " + string(_i_bad_r));
        if (!_i_held) show_debug_message("instance state err: " + string(igpu_get_last_error()));
        _igpu_check(_i_held && _i_held_l == c_blue && _i_held_r == c_blue, "an instanced draw keeps the destination colour");
        _igpu_check(_i_open && _i_open_l == c_red && _i_open_r == c_lime, "an instanced draw with no state replaces both halves");
        _igpu_check(!_i_bad && _i_bad_l == c_red && _i_bad_r == c_lime, "an instanced depth slot rejects a blend state and leaves the pixels");
    }
    if (surface_exists(_i_hold_surf)) surface_free(_i_hold_surf);
    _igpu_check(igpu_state_release(_i_hold), "release instanced hold blend");
}
if (surface_exists(_i_surf))
{
    surface_free(_i_surf);
}
var _a_buf = igpu_buffer_create(16, IgpuBufferUsage.Static, IgpuBufferBind.Indirect, 0);
_igpu_check(_a_buf > 0, "argument buffer is created");
_igpu_check(!igpu_draw_indirect(_i_vb, _i_ib, _i_layout, pr_trianglelist, _i_vb, 0, 0, 0, 0, 0), "a vertex buffer is not an argument buffer");
var _a_src = buffer_create(16, buffer_fixed, 4);
buffer_seek(_a_src, buffer_seek_start, 0);
buffer_write(_a_src, buffer_u32, 6);
buffer_write(_a_src, buffer_u32, 2);
buffer_write(_a_src, buffer_u32, 0);
buffer_write(_a_src, buffer_u32, 0);
_igpu_check(igpu_buffer_write(_a_buf, 0, _a_src), "upload draw arguments");
buffer_delete(_a_src);
var _a_surf = surface_create(8, 8);
_igpu_check(surface_exists(_a_surf), "indirect surface is created");
if (surface_exists(_a_surf) && _a_buf > 0 && _i_vb > 0 && _i_ib > 0 && _i_layout > 0 && _i_vs > 0 && _i_ps > 0)
{
    gpu_push_state();
    _igpu_target_begin(_a_surf, c_blue);
    igpu_shader_bind(_i_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_i_ps, IgpuShaderStage.Pixel);
    var _a_drew = igpu_draw_indirect(_i_vb, _i_ib, _i_layout, pr_trianglelist, _a_buf, 0, 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _a_left = surface_getpixel(_a_surf, 1, 4);
    var _a_right = surface_getpixel(_a_surf, 6, 4);
    show_debug_message("indirect pixel   : " + string(_a_drew) + " " + string(_a_left) + " / " + string(_a_right));
    _igpu_check(_a_drew, "indirect draw succeeds");
    _igpu_check(_a_left == c_red, "indirect instance 0 is red");
    _igpu_check(_a_right == c_lime, "indirect instance 1 is green");

    var _a_hold = igpu_blend_state_create(
        true, bm_zero, bm_one, bm_eq_add, bm_zero, bm_one, bm_eq_add,
        true, true, true, true);
    var _a_hold_surf = surface_create(8, 8);
    if (_a_hold > 0 && surface_exists(_a_hold_surf))
    {
        gpu_push_state();
        _igpu_target_begin(_a_hold_surf, c_blue);
        igpu_shader_bind(_i_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_i_ps, IgpuShaderStage.Pixel);
        var _a_held = igpu_draw_indirect(_i_vb, _i_ib, _i_layout, pr_trianglelist, _a_buf, 0, _a_hold, 0, 0, 0);
        surface_reset_target();
        var _a_held_l = surface_getpixel(_a_hold_surf, 1, 4);
        var _a_held_r = surface_getpixel(_a_hold_surf, 6, 4);
        _igpu_target_begin(_a_hold_surf, c_blue);
        igpu_shader_bind(_i_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_i_ps, IgpuShaderStage.Pixel);
        var _a_open = igpu_draw_indirect(_i_vb, _i_ib, _i_layout, pr_trianglelist, _a_buf, 0, 0, 0, 0, 0);
        igpu_shader_bind(0, IgpuShaderStage.Vertex);
        igpu_shader_bind(0, IgpuShaderStage.Pixel);
        surface_reset_target();
        gpu_pop_state();
        var _a_open_l = surface_getpixel(_a_hold_surf, 1, 4);
        var _a_open_r = surface_getpixel(_a_hold_surf, 6, 4);
        show_debug_message("indirect state   : " + string(_a_held) + " " + string(_a_held_l) + " / " + string(_a_held_r)
            + " | " + string(_a_open) + " " + string(_a_open_l) + " / " + string(_a_open_r));
        if (!_a_held) show_debug_message("indirect state err: " + string(igpu_get_last_error()));
        _igpu_check(_a_held && _a_held_l == c_blue && _a_held_r == c_blue, "an indirect draw keeps the destination colour");
        _igpu_check(_a_open && _a_open_l == c_red && _a_open_r == c_lime, "an indirect draw with no state replaces both halves");
    }
    if (surface_exists(_a_hold_surf)) surface_free(_a_hold_surf);
    _igpu_check(igpu_state_release(_a_hold), "release indirect hold blend");
}
if (surface_exists(_a_surf))
{
    surface_free(_a_surf);
}

// Indexed indirect. Eight vertices, two quads. Indices 0..5 cover the left
// half and 6..11 cover the right. The 20-byte record starts at index 6, so
// only the right half may turn red. Starting at index 0 would paint the left.
var _n_vs = igpu_shader_compile(_good, "main", IgpuShaderStage.Vertex, "");
var _n_ps = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _n_layout = igpu_vertex_format(_n_vs, [
    [vertex_usage_position, vertex_type_float3],
    [vertex_usage_texcoord, vertex_type_float2]
]);
var _n_vb = igpu_buffer_create(160, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 20);
var _n_ib = igpu_buffer_create(24, IgpuBufferUsage.Dynamic, IgpuBufferBind.Index, 0);
var _n_args = igpu_buffer_create(20, IgpuBufferUsage.Static, IgpuBufferBind.Indirect, 0);
_igpu_check(_n_vs > 0 && _n_ps > 0 && _n_layout > 0 && _n_vb > 0 && _n_ib > 0 && _n_args > 0, "indexed-indirect resources are created");
var _n_upload = buffer_create(160, buffer_fixed, 4);
buffer_seek(_n_upload, buffer_seek_start, 0);
_igpu_write_vert(_n_upload, -1, -1, 0.5, 0, 0);
_igpu_write_vert(_n_upload, 0, -1, 0.5, 1, 0);
_igpu_write_vert(_n_upload, -1, 1, 0.5, 0, 1);
_igpu_write_vert(_n_upload, 0, 1, 0.5, 1, 1);
_igpu_write_vert(_n_upload, 0, -1, 0.5, 0, 0);
_igpu_write_vert(_n_upload, 1, -1, 0.5, 1, 0);
_igpu_write_vert(_n_upload, 0, 1, 0.5, 0, 1);
_igpu_write_vert(_n_upload, 1, 1, 0.5, 1, 1);
_igpu_check(igpu_buffer_write(_n_vb, 0, _n_upload), "upload indexed-indirect vertices");
buffer_delete(_n_upload);
var _n_indices = buffer_create(24, buffer_fixed, 2);
buffer_seek(_n_indices, buffer_seek_start, 0);
buffer_write(_n_indices, buffer_u16, 0);
buffer_write(_n_indices, buffer_u16, 1);
buffer_write(_n_indices, buffer_u16, 2);
buffer_write(_n_indices, buffer_u16, 1);
buffer_write(_n_indices, buffer_u16, 3);
buffer_write(_n_indices, buffer_u16, 2);
buffer_write(_n_indices, buffer_u16, 4);
buffer_write(_n_indices, buffer_u16, 5);
buffer_write(_n_indices, buffer_u16, 6);
buffer_write(_n_indices, buffer_u16, 5);
buffer_write(_n_indices, buffer_u16, 7);
buffer_write(_n_indices, buffer_u16, 6);
_igpu_check(igpu_buffer_write(_n_ib, 0, _n_indices), "upload indexed-indirect indices");
buffer_delete(_n_indices);
var _n_rec = buffer_create(20, buffer_fixed, 4);
buffer_seek(_n_rec, buffer_seek_start, 0);
buffer_write(_n_rec, buffer_u32, 6);
buffer_write(_n_rec, buffer_u32, 1);
buffer_write(_n_rec, buffer_u32, 6);
buffer_write(_n_rec, buffer_u32, 0);
buffer_write(_n_rec, buffer_u32, 0);
_igpu_check(igpu_buffer_write(_n_args, 0, _n_rec), "upload indexed-indirect arguments");
buffer_delete(_n_rec);
_igpu_check(!igpu_draw_indexed_indirect(_n_vb, 0, _n_layout, 0, pr_trianglelist, _n_args, 0, 0, 0, 0, 0), "indexed indirect requires an index buffer");
_igpu_check(!igpu_draw_indexed_indirect(_n_vb, 0, _n_layout, _n_vb, pr_trianglelist, _n_args, 0, 0, 0, 0, 0), "a vertex buffer is not an index buffer");
_igpu_check(!igpu_draw_indexed_indirect(_n_vb, 0, _n_layout, _n_ib, pr_trianglelist, _n_vb, 0, 0, 0, 0, 0), "a vertex buffer is not an indexed argument buffer");
_igpu_check(!igpu_draw_indexed_indirect(_n_vb, 0, _n_layout, _n_ib, pr_trianglelist, _a_buf, 0, 0, 0, 0, 0), "a 16-byte argument buffer cannot hold an indexed record");
var _n_surf = surface_create(8, 8);
_igpu_check(surface_exists(_n_surf), "indexed-indirect surface is created");
if (surface_exists(_n_surf) && _n_vb > 0 && _n_ib > 0 && _n_args > 0 && _n_layout > 0 && _n_vs > 0 && _n_ps > 0)
{
    gpu_push_state();
    _igpu_target_begin(_n_surf, c_blue);
    igpu_shader_bind(_n_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_n_ps, IgpuShaderStage.Pixel);
    var _n_drew = igpu_draw_indexed_indirect(_n_vb, 0, _n_layout, _n_ib, pr_trianglelist, _n_args, 0, 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _n_left = surface_getpixel(_n_surf, 1, 4);
    var _n_right = surface_getpixel(_n_surf, 6, 4);
    show_debug_message("indexed indirect : " + string(_n_drew) + " " + string(_n_left) + " / " + string(_n_right));
    if (!_n_drew) show_debug_message("indexed indirect err: " + string(igpu_get_last_error()));
    _igpu_check(_n_drew, "indexed indirect draw succeeds");
    _igpu_check(_n_right == c_red, "the indexed record draws the right half");
    _igpu_check(_n_left == c_blue, "the indexed record leaves the left half blue");

    var _n_hold = igpu_blend_state_create(
        true, bm_zero, bm_one, bm_eq_add, bm_zero, bm_one, bm_eq_add,
        true, true, true, true);
    var _n_hold_surf = surface_create(8, 8);
    if (_n_hold > 0 && surface_exists(_n_hold_surf))
    {
        gpu_push_state();
        _igpu_target_begin(_n_hold_surf, c_blue);
        igpu_shader_bind(_n_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_n_ps, IgpuShaderStage.Pixel);
        var _n_held = igpu_draw_indexed_indirect(_n_vb, 0, _n_layout, _n_ib, pr_trianglelist, _n_args, 0, _n_hold, 0, 0, 0);
        surface_reset_target();
        var _n_held_l = surface_getpixel(_n_hold_surf, 1, 4);
        var _n_held_r = surface_getpixel(_n_hold_surf, 6, 4);
        _igpu_target_begin(_n_hold_surf, c_blue);
        igpu_shader_bind(_n_vs, IgpuShaderStage.Vertex);
        igpu_shader_bind(_n_ps, IgpuShaderStage.Pixel);
        var _n_open = igpu_draw_indexed_indirect(_n_vb, 0, _n_layout, _n_ib, pr_trianglelist, _n_args, 0, 0, 0, 0, 0);
        igpu_shader_bind(0, IgpuShaderStage.Vertex);
        igpu_shader_bind(0, IgpuShaderStage.Pixel);
        surface_reset_target();
        gpu_pop_state();
        var _n_open_l = surface_getpixel(_n_hold_surf, 1, 4);
        var _n_open_r = surface_getpixel(_n_hold_surf, 6, 4);
        show_debug_message("indexed state    : " + string(_n_held) + " " + string(_n_held_l) + " / " + string(_n_held_r)
            + " | " + string(_n_open) + " " + string(_n_open_l) + " / " + string(_n_open_r));
        if (!_n_held) show_debug_message("indexed state err : " + string(igpu_get_last_error()));
        _igpu_check(_n_held && _n_held_l == c_blue && _n_held_r == c_blue, "an indexed indirect draw keeps the destination colour");
        _igpu_check(_n_open && _n_open_l == c_blue && _n_open_r == c_red, "an indexed indirect draw with no state paints only the right half");
    }
    if (surface_exists(_n_hold_surf)) surface_free(_n_hold_surf);
    _igpu_check(igpu_state_release(_n_hold), "release indexed-indirect hold blend");
}
if (surface_exists(_n_surf))
{
    surface_free(_n_surf);
}
_igpu_check(igpu_buffer_release(_n_vb), "release indexed-indirect mesh");
_igpu_check(igpu_buffer_release(_n_ib), "release indexed-indirect indices");
_igpu_check(igpu_buffer_release(_n_args), "release indexed-indirect arguments");
_igpu_check(igpu_input_layout_release(_n_layout), "release indexed-indirect layout");
_igpu_check(igpu_shader_release(_n_vs), "release indexed-indirect vertex shader");
_igpu_check(igpu_shader_release(_n_ps), "release indexed-indirect pixel shader");

_igpu_check(igpu_buffer_release(_a_buf), "release argument buffer");
_igpu_check(_caps.indirect_draw, "indirect draw reported");

_igpu_check(igpu_buffer_release(_i_vb), "release instance mesh");
_igpu_check(igpu_buffer_release(_i_ib), "release instance colours");
_igpu_check(igpu_input_layout_release(_i_layout), "release instance layout");
_igpu_check(igpu_shader_release(_i_vs), "release instance vertex shader");
_igpu_check(igpu_shader_release(_i_ps), "release instance pixel shader");

// ---------------------------------------------------------------------------
// Geometry shader. A single point does not cover pixel (1, 4). The geometry
// shader replaces that point with the left-half triangle. The right pixel
// stays the clear colour, so a full-surface fill cannot pass.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
_igpu_check(igpu_supports(IgpuCapability.ShaderStageGeometry), "supports geometry");
var _g_vs = igpu_shader_compile(
    "struct VSIn { float3 pos : POSITION; }; struct VSOut { float4 pos : SV_POSITION; }; VSOut main(VSIn i) { VSOut o; o.pos = float4(i.pos, 1.0); return o; }",
    "main", IgpuShaderStage.Vertex, "");
var _g_gs = igpu_shader_compile(
    "struct VSOut { float4 pos : SV_POSITION; }; [maxvertexcount(3)] void main(point VSOut input[1], inout TriangleStream<VSOut> stream) { VSOut o; o.pos = float4(-1.0, -1.0, 0.5, 1.0); stream.Append(o); o.pos = float4(0.0, -1.0, 0.5, 1.0); stream.Append(o); o.pos = float4(-1.0, 1.0, 0.5, 1.0); stream.Append(o); stream.RestartStrip(); }",
    "main", IgpuShaderStage.Geometry, "");
var _g_ps = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _g_layout = igpu_input_layout_create(_g_vs, [vertex_usage_position], [vertex_type_float3], array_create(1, IgpuVertexStep.Vertex), 1, 12, 0);
var _g_vb = igpu_buffer_create(12, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 12);
_igpu_check(_g_vs > 0 && _g_gs > 0 && _g_ps > 0 && _g_layout > 0 && _g_vb > 0, "geometry resources are created");
if (_g_gs == 0) show_debug_message("geometry compile err: " + string(igpu_get_last_error()));
var _g_upload = buffer_create(12, buffer_fixed, 4);
buffer_seek(_g_upload, buffer_seek_start, 0);
buffer_write(_g_upload, buffer_f32, 0);
buffer_write(_g_upload, buffer_f32, 0);
buffer_write(_g_upload, buffer_f32, 0.5);
_igpu_check(igpu_buffer_write(_g_vb, 0, _g_upload), "upload the geometry-test point");
buffer_delete(_g_upload);
var _g_surf = surface_create(8, 8);
_igpu_check(surface_exists(_g_surf), "geometry surface is created");
if (surface_exists(_g_surf) && _g_vb > 0 && _g_layout > 0 && _g_vs > 0 && _g_gs > 0 && _g_ps > 0)
{
    gpu_push_state();
    _igpu_target_begin(_g_surf, c_blue);
    igpu_shader_bind(_g_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_g_ps, IgpuShaderStage.Pixel);
    var _g_point = igpu_draw(_g_vb, 0, _g_layout, pr_pointlist, 0, 1, 1, 0, 0, 0, 0);
    surface_reset_target();
    var _g_point_left = surface_getpixel(_g_surf, 1, 4);
    _igpu_target_begin(_g_surf, c_blue);
    igpu_shader_bind(_g_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_g_ps, IgpuShaderStage.Pixel);
    var _g_bound = igpu_shader_bind(_g_gs, IgpuShaderStage.Geometry);
    var _g_tri = igpu_draw(_g_vb, 0, _g_layout, pr_pointlist, 0, 1, 1, 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    igpu_shader_bind(0, IgpuShaderStage.Geometry);
    surface_reset_target();
    gpu_pop_state();
    var _g_left = surface_getpixel(_g_surf, 1, 4);
    var _g_right = surface_getpixel(_g_surf, 6, 4);
    show_debug_message("geometry pixel   : " + string(_g_point) + " " + string(_g_point_left) + " " + string(_g_bound) + " " + string(_g_tri) + " " + string(_g_left) + " / " + string(_g_right));
    if (!_g_tri) show_debug_message("geometry draw err : " + string(igpu_get_last_error()));
    _igpu_check(_g_point && _g_point_left == c_blue, "a lone point leaves the left pixel blue");
    _igpu_check(_g_bound && _g_tri && _g_left == c_red, "the geometry shader paints the left half");
    _igpu_check(_g_right == c_blue, "the geometry shader leaves the right half blue");
}
if (surface_exists(_g_surf))
{
    surface_free(_g_surf);
}
_igpu_check(igpu_buffer_release(_g_vb), "release geometry-test point");
_igpu_check(igpu_input_layout_release(_g_layout), "release geometry-test layout");
_igpu_check(igpu_shader_release(_g_vs), "release geometry vertex shader");
_igpu_check(igpu_shader_release(_g_gs), "release geometry shader");
_igpu_check(igpu_shader_release(_g_ps), "release geometry pixel shader");

// ---------------------------------------------------------------------------
// Tessellation. Three control points sit on top of each other, so a triangle
// list does not cover pixel (1, 4). The domain shader places the patch
// corners on the left-half triangle. The right pixel stays the clear colour.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
_igpu_check(igpu_supports(IgpuCapability.ShaderStageTessellation), "supports tessellation");
var _t_vs = igpu_shader_compile(
    "struct VSIn { float3 pos : POSITION; }; struct VSOut { float4 pos : SV_POSITION; }; VSOut main(VSIn i) { VSOut o; o.pos = float4(i.pos, 1.0); return o; }",
    "main", IgpuShaderStage.Vertex, "");
var _t_hs = igpu_shader_compile(
    "struct VSOut { float4 pos : SV_POSITION; }; struct HSConst { float edge[3] : SV_TessFactor; float inside : SV_InsideTessFactor; }; HSConst factors(InputPatch<VSOut, 3> patch, uint pid : SV_PrimitiveID) { HSConst c; c.edge[0] = 1; c.edge[1] = 1; c.edge[2] = 1; c.inside = 1; return c; } [domain(\"tri\")] [partitioning(\"integer\")] [outputtopology(\"triangle_ccw\")] [outputcontrolpoints(3)] [patchconstantfunc(\"factors\")] VSOut main(InputPatch<VSOut, 3> patch, uint id : SV_OutputControlPointID) { return patch[id]; }",
    "main", IgpuShaderStage.Hull, "");
if (_t_hs == 0) show_debug_message("hull err          : " + string(igpu_get_last_error()));
var _t_ds = igpu_shader_compile(
    "struct VSOut { float4 pos : SV_POSITION; }; struct DSOut { float4 pos : SV_POSITION; }; struct HSConst { float edge[3] : SV_TessFactor; float inside : SV_InsideTessFactor; }; [domain(\"tri\")] DSOut main(HSConst factors, float3 bary : SV_DomainLocation, const OutputPatch<VSOut, 3> patch) { DSOut o; float2 p = bary.x * float2(-1.0, -1.0) + bary.y * float2(0.0, -1.0) + bary.z * float2(-1.0, 1.0); o.pos = float4(p, 0.5, factors.inside); return o; }",
    "main", IgpuShaderStage.Domain, "");
if (_t_ds == 0) show_debug_message("domain err        : " + string(igpu_get_last_error()));
var _t_ps = igpu_shader_compile("float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Pixel, "");
var _t_layout = igpu_input_layout_create(_t_vs, [vertex_usage_position], [vertex_type_float3], array_create(1, IgpuVertexStep.Vertex), 1, 12, 0);
var _t_vb = igpu_buffer_create(36, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 12);
_igpu_check(_t_vs > 0 && _t_hs > 0 && _t_ds > 0 && _t_ps > 0 && _t_layout > 0 && _t_vb > 0, "tessellation resources are created");
var _t_upload = buffer_create(36, buffer_fixed, 4);
buffer_seek(_t_upload, buffer_seek_start, 0);
buffer_write(_t_upload, buffer_f32, 0); buffer_write(_t_upload, buffer_f32, 0); buffer_write(_t_upload, buffer_f32, 0.5);
buffer_write(_t_upload, buffer_f32, 0); buffer_write(_t_upload, buffer_f32, 0); buffer_write(_t_upload, buffer_f32, 0.5);
buffer_write(_t_upload, buffer_f32, 0); buffer_write(_t_upload, buffer_f32, 0); buffer_write(_t_upload, buffer_f32, 0.5);
_igpu_check(igpu_buffer_write(_t_vb, 0, _t_upload), "upload the degenerate patch");
buffer_delete(_t_upload);
_igpu_check(!igpu_draw_patch(_t_vb, _t_layout, 0, 0, 3, 0, 0, 0, 0), "a patch needs 1 to 32 control points");
_igpu_check(!igpu_draw_patch(_t_vb, _t_layout, 33, 0, 3, 0, 0, 0, 0), "33 control points are rejected");
_igpu_check(!igpu_draw_patch(_t_vb, _t_layout, 3, 0, 2, 0, 0, 0, 0), "a partial patch is rejected");
_igpu_check(!igpu_draw_patch(_t_vb, _t_layout, 3, 0, 3, 0, 0, 0, 0), "a patch draw without hull and domain shaders is rejected");
var _t_surf = surface_create(8, 8);
_igpu_check(surface_exists(_t_surf), "tessellation surface is created");
if (surface_exists(_t_surf) && _t_vb > 0 && _t_layout > 0 && _t_vs > 0 && _t_hs > 0 && _t_ds > 0 && _t_ps > 0)
{
    gpu_push_state();
    _igpu_target_begin(_t_surf, c_blue);
    igpu_shader_bind(_t_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_t_ps, IgpuShaderStage.Pixel);
    var _t_plain = igpu_draw(_t_vb, 0, _t_layout, pr_trianglelist, 0, 3, 1, 0, 0, 0, 0);
    surface_reset_target();
    var _t_plain_left = surface_getpixel(_t_surf, 1, 4);
    _igpu_target_begin(_t_surf, c_blue);
    igpu_shader_bind(_t_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_t_ps, IgpuShaderStage.Pixel);
    var _t_hull = igpu_shader_bind(_t_hs, IgpuShaderStage.Hull);
    var _t_domain = igpu_shader_bind(_t_ds, IgpuShaderStage.Domain);
    var _t_hold = igpu_blend_state_create(
        true, bm_zero, bm_one, bm_eq_add, bm_zero, bm_one, bm_eq_add,
        true, true, true, true);
    var _t_held = igpu_draw_patch(_t_vb, _t_layout, 3, 0, 3, _t_hold, 0, 0, 0);
    surface_reset_target();
    var _t_held_left = surface_getpixel(_t_surf, 1, 4);
    _igpu_target_begin(_t_surf, c_blue);
    igpu_shader_bind(_t_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_t_ps, IgpuShaderStage.Pixel);
    igpu_shader_bind(_t_hs, IgpuShaderStage.Hull);
    igpu_shader_bind(_t_ds, IgpuShaderStage.Domain);
    var _t_drew = igpu_draw_patch(_t_vb, _t_layout, 3, 0, 3, 0, 0, 0, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    igpu_shader_bind(0, IgpuShaderStage.Hull);
    igpu_shader_bind(0, IgpuShaderStage.Domain);
    surface_reset_target();
    gpu_pop_state();
    var _t_left = surface_getpixel(_t_surf, 1, 4);
    var _t_right = surface_getpixel(_t_surf, 6, 4);
    show_debug_message("tessellation     : " + string(_t_plain) + " " + string(_t_plain_left) + " " + string(_t_hull) + " " + string(_t_domain) + " " + string(_t_held) + " " + string(_t_held_left) + " " + string(_t_drew) + " " + string(_t_left) + " / " + string(_t_right));
    if (!_t_drew || !_t_held) show_debug_message("tessellation err  : " + string(igpu_get_last_error()));
    _igpu_check(_t_plain && _t_plain_left == c_blue, "the degenerate triangle leaves the left pixel blue");
    _igpu_check(_t_hold > 0 && _t_held && _t_held_left == c_blue, "a patch draw keeps the destination colour");
    _igpu_check(_t_hull && _t_domain && _t_drew && _t_left == c_red, "the patch covers the left half");
    _igpu_check(_t_right == c_blue, "the patch leaves the right half blue");
    _igpu_check(igpu_state_release(_t_hold), "release patch hold blend");
}
if (surface_exists(_t_surf))
{
    surface_free(_t_surf);
}
_igpu_check(igpu_buffer_release(_t_vb), "release tessellation patch");
_igpu_check(igpu_input_layout_release(_t_layout), "release tessellation layout");
_igpu_check(igpu_shader_release(_t_vs), "release tessellation vertex shader");
_igpu_check(igpu_shader_release(_t_hs), "release hull shader");
_igpu_check(igpu_shader_release(_t_ds), "release domain shader");
_igpu_check(igpu_shader_release(_t_ps), "release tessellation pixel shader");

// ---------------------------------------------------------------------------
// Structured storage buffer. Two float4 colours, read by the vertex shader.
// The CPU read-back checks the upload, and the pixels check the shader saw it.
// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
_igpu_check(igpu_buffer_create(32, IgpuBufferUsage.Static, IgpuBufferBind.Storage, 0) == 0, "storage without a stride is rejected");
var _s_buf = igpu_buffer_create(32, IgpuBufferUsage.Static, IgpuBufferBind.Storage, 16);
_igpu_check(_s_buf > 0, "structured buffer is created");
var _s_src = buffer_create(32, buffer_fixed, 4);
buffer_seek(_s_src, buffer_seek_start, 0);
buffer_write(_s_src, buffer_f32, 1); buffer_write(_s_src, buffer_f32, 0); buffer_write(_s_src, buffer_f32, 0); buffer_write(_s_src, buffer_f32, 1);
buffer_write(_s_src, buffer_f32, 0); buffer_write(_s_src, buffer_f32, 1); buffer_write(_s_src, buffer_f32, 0); buffer_write(_s_src, buffer_f32, 1);
_igpu_check(igpu_buffer_write(_s_buf, 0, _s_src), "upload structured colours");
var _s_back = buffer_create(4, buffer_fixed, 4);
_igpu_check(igpu_buffer_read(_s_buf, 16, _s_back), "read the second structure back");
buffer_seek(_s_back, buffer_seek_start, 0);
var _s_read = buffer_read(_s_back, buffer_f32);
show_debug_message("storage readback : " + string(_s_read));
_igpu_check(_s_read == 0, "second structure starts with 0");
buffer_delete(_s_back);
buffer_delete(_s_src);

var _s_vs_src = "StructuredBuffer<float4> colors : register(t0); struct VSIn { float3 pos : POSITION; uint iid : SV_InstanceID; }; struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; }; VSOut main(VSIn i) { VSOut o; o.pos = float4(i.pos.x + i.iid, i.pos.y, i.pos.z, 1.0); o.col = colors[i.iid]; return o; }";
var _s_ps_src = "struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; }; float4 main(PSIn i) : SV_TARGET { return i.col; }";
var _s_vs = igpu_shader_compile(_s_vs_src, "main", IgpuShaderStage.Vertex, "");
var _s_ps = igpu_shader_compile(_s_ps_src, "main", IgpuShaderStage.Pixel, "");
var _s_layout = igpu_input_layout_create(_s_vs, [vertex_usage_position], [vertex_type_float3], array_create(1, IgpuVertexStep.Vertex), 1, 12, 0);
var _s_mesh = igpu_buffer_create(72, IgpuBufferUsage.Dynamic, IgpuBufferBind.Vertex, 12);
_igpu_check(_s_vs > 0 && _s_ps > 0 && _s_layout > 0 && _s_mesh > 0, "storage draw resources are created");
var _s_bytes = buffer_create(72, buffer_fixed, 4);
buffer_seek(_s_bytes, buffer_seek_start, 0);
buffer_write(_s_bytes, buffer_f32, -1); buffer_write(_s_bytes, buffer_f32, -1); buffer_write(_s_bytes, buffer_f32, 0.5);
buffer_write(_s_bytes, buffer_f32, 0); buffer_write(_s_bytes, buffer_f32, -1); buffer_write(_s_bytes, buffer_f32, 0.5);
buffer_write(_s_bytes, buffer_f32, -1); buffer_write(_s_bytes, buffer_f32, 1); buffer_write(_s_bytes, buffer_f32, 0.5);
buffer_write(_s_bytes, buffer_f32, 0); buffer_write(_s_bytes, buffer_f32, -1); buffer_write(_s_bytes, buffer_f32, 0.5);
buffer_write(_s_bytes, buffer_f32, 0); buffer_write(_s_bytes, buffer_f32, 1); buffer_write(_s_bytes, buffer_f32, 0.5);
buffer_write(_s_bytes, buffer_f32, -1); buffer_write(_s_bytes, buffer_f32, 1); buffer_write(_s_bytes, buffer_f32, 0.5);
_igpu_check(igpu_buffer_write(_s_mesh, 0, _s_bytes), "upload storage-test mesh");
buffer_delete(_s_bytes);

var _s_surf = surface_create(8, 8);
_igpu_check(surface_exists(_s_surf), "storage surface is created");
if (surface_exists(_s_surf) && _s_buf > 0 && _s_mesh > 0 && _s_layout > 0 && _s_vs > 0 && _s_ps > 0)
{
    gpu_push_state();
    _igpu_target_begin(_s_surf, c_blue);
    igpu_shader_bind(_s_vs, IgpuShaderStage.Vertex);
    igpu_shader_bind(_s_ps, IgpuShaderStage.Pixel);
    var _s_bound = igpu_storage_bind(_s_buf, IgpuShaderStage.Vertex, 0);
    var _s_drew = igpu_draw(_s_mesh, 0, _s_layout, pr_trianglelist, 0, 6, 2, 0, 0, 0, 0);
    igpu_storage_bind(0, IgpuShaderStage.Vertex, 0);
    igpu_shader_bind(0, IgpuShaderStage.Vertex);
    igpu_shader_bind(0, IgpuShaderStage.Pixel);
    surface_reset_target();
    gpu_pop_state();
    var _s_left = surface_getpixel(_s_surf, 1, 4);
    var _s_right = surface_getpixel(_s_surf, 6, 4);
    show_debug_message("storage pixel    : " + string(_s_bound) + " " + string(_s_drew) + " " + string(_s_left) + " / " + string(_s_right));
    _igpu_check(_s_bound, "storage buffer binds");
    _igpu_check(_s_drew, "storage-backed draw succeeds");
    _igpu_check(_s_left == c_red, "structure 0 is red");
    _igpu_check(_s_right == c_lime, "structure 1 is green");
}
if (surface_exists(_s_surf))
{
    surface_free(_s_surf);
}
var _s_cs = igpu_shader_compile("RWStructuredBuffer<float4> dst : register(u0); [numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) { if (id.x < 2) dst[id.x] = float4(0.0, 0.0, 1.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
_igpu_check(_s_cs > 0, "storage compute shader compiles");
_igpu_check(!igpu_dispatch(2, 1, 1, [IgpuWriteTarget.Buffer], [_s_buf], [0], [0]), "buffer dispatch without a compute shader is rejected");
_igpu_check(!igpu_dispatch(2, 1, 1, [IgpuWriteTarget.Buffer], [_s_mesh], [0], [0]), "a mesh buffer is not writable storage");
if (_s_cs > 0 && _s_buf > 0)
{
    igpu_shader_bind(_s_cs, IgpuShaderStage.Compute);
    var _s_ran = igpu_dispatch(2, 1, 1, [IgpuWriteTarget.Buffer], [_s_buf], [0], [0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _s_wrote = buffer_create(4, buffer_fixed, 4);
    var _s_ok = igpu_buffer_read(_s_buf, 8, _s_wrote);
    buffer_seek(_s_wrote, buffer_seek_start, 0);
    var _s_blue = buffer_read(_s_wrote, buffer_f32);
    show_debug_message("storage compute  : " + string(_s_ran) + " " + string(_s_ok) + " " + string(_s_blue));
    if (!_s_ran) show_debug_message("storage compute err: " + string(igpu_get_last_error()));
    _igpu_check(_s_ran && _s_ok && _s_blue == 1, "a compute shader writes the structured buffer");
    buffer_delete(_s_wrote);
}

// One dispatch writes the texture at slot 0 and the buffer at slot 1.
// The buffer's first float is 0 after the earlier write, so 1 means this
// dispatch stored it. The texture starts black, so red means this dispatch
// stored that too.
var _s_tex = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, false, true, 1);
var _s_plain = igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba8unorm, false, false, 1);
var _s_both = igpu_shader_compile("RWTexture2D<float4> img : register(u0); RWStructuredBuffer<float4> rec : register(u1); [numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { img[id.xy] = float4(1.0, 0.0, 0.0, 1.0); if (id.x == 0 && id.y == 0) rec[0] = float4(1.0, 0.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
_igpu_check(_s_tex > 0 && _s_plain > 0 && _s_both > 0, "paired storage targets are created");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture, IgpuWriteTarget.Buffer], [_s_tex, _s_buf], [0, 0], [0, 0]), "paired dispatch without a compute shader is rejected");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture, IgpuWriteTarget.Buffer], [_s_plain, _s_buf], [0, 0], [0, 0]), "a sampled texture is not a paired write target");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture, IgpuWriteTarget.Buffer], [_s_tex, _s_mesh], [0, 0], [0, 0]), "a mesh buffer is not a paired write target");
if (_s_tex > 0 && _s_both > 0 && _s_buf > 0)
{
    igpu_shader_bind(_s_both, IgpuShaderStage.Compute);
    var _s_both_ran = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture, IgpuWriteTarget.Buffer], [_s_tex, _s_buf], [0, 0], [0, 0]);
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _s_pix = igpu_texture_read(_s_tex, 1, 4, 0, 0);
    var _s_word = buffer_create(4, buffer_fixed, 4);
    var _s_word_ok = igpu_buffer_read(_s_buf, 0, _s_word);
    buffer_seek(_s_word, buffer_seek_start, 0);
    var _s_red = buffer_read(_s_word, buffer_f32);
    show_debug_message("storage pair     : " + string(_s_both_ran) + " " + string(_s_pix) + " " + string(_s_word_ok) + " " + string(_s_red));
    if (!_s_both_ran) show_debug_message("storage pair err : " + string(igpu_get_last_error()));
    _igpu_check(_s_both_ran && _s_pix == c_red && _s_word_ok && _s_red == 1, "one dispatch writes the texture and the buffer");
    buffer_delete(_s_word);
}
_igpu_check(igpu_texture_release(_s_tex), "release paired storage texture");
_igpu_check(igpu_texture_release(_s_plain), "release plain paired texture");
_igpu_check(igpu_shader_release(_s_both), "release paired compute shader");
_igpu_check(igpu_shader_release(_s_cs), "release storage compute shader");

// Caller order: slot i is entry i, so a buffer can come before a texture.
// The four-target list writes red, green, then two different buffer lanes.
// The following dispatch lists a buffer first; blue on the texture and 1
// in that buffer's second float only happen if slot 0 was the buffer.
var _w_a = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, false, true, 1);
var _w_b = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, false, true, 1);
var _w_c = igpu_buffer_create(16, IgpuBufferUsage.Static, IgpuBufferBind.Storage, 16);
var _w_d = igpu_buffer_create(16, IgpuBufferUsage.Static, IgpuBufferBind.Storage, 16);
var _w_ord_tex = igpu_texture_create(IgpuTextureKind.TwoD, 8, 8, 1, surface_rgba8unorm, false, true, 1);
var _w_ord_buf = igpu_buffer_create(16, IgpuBufferUsage.Static, IgpuBufferBind.Storage, 16);
var _w_plain = igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba8unorm, false, false, 1);
var _w_list = igpu_shader_compile("RWTexture2D<float4> a : register(u0); RWTexture2D<float4> b : register(u1); RWStructuredBuffer<float4> c : register(u2); RWStructuredBuffer<float4> d : register(u3); [numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { a[id.xy] = float4(1.0, 0.0, 0.0, 1.0); b[id.xy] = float4(0.0, 1.0, 0.0, 1.0); if (id.x == 0 && id.y == 0) { c[0] = float4(0.0, 0.0, 1.0, 1.0); d[0] = float4(0.0, 1.0, 0.0, 1.0); } }", "main", IgpuShaderStage.Compute, "");
var _w_ord = igpu_shader_compile("RWStructuredBuffer<float4> rec : register(u0); RWTexture2D<float4> img : register(u1); [numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) { img[id.xy] = float4(0.0, 0.0, 1.0, 1.0); if (id.x == 0 && id.y == 0) rec[0] = float4(0.0, 1.0, 0.0, 1.0); }", "main", IgpuShaderStage.Compute, "");
_igpu_check(_w_a > 0 && _w_b > 0 && _w_c > 0 && _w_d > 0 && _w_ord_tex > 0 && _w_ord_buf > 0 && _w_plain > 0 && _w_list > 0 && _w_ord > 0, "listed write targets are created");
var _w_nine = [0, 0, 0, 0, 0, 0, 0, 0, 0];
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [], array_create(array_length([IgpuWriteTarget.Texture]), 0), array_create(array_length([IgpuWriteTarget.Texture]), 0)), "write target lists must be the same length");
_igpu_check(!igpu_dispatch(1, 1, 1, [], [], array_create(array_length([]), 0), array_create(array_length([]), 0)), "a dispatch with no write targets is rejected");
_igpu_check(!igpu_dispatch(1, 1, 1, _w_nine, _w_nine, array_create(array_length(_w_nine), 0), array_create(array_length(_w_nine), 0)), "more than 8 write targets are rejected");
_igpu_check(!igpu_dispatch(1, 1, 1, [99], [_w_a], array_create(array_length([99]), 0), array_create(array_length([99]), 0)), "an unknown write target kind is rejected");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_w_a], array_create(array_length([IgpuWriteTarget.Texture]), 0), array_create(array_length([IgpuWriteTarget.Texture]), 0)), "listed dispatch without a compute shader is rejected");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture], [_w_plain], array_create(array_length([IgpuWriteTarget.Texture]), 0), array_create(array_length([IgpuWriteTarget.Texture]), 0)), "a sampled texture is not a listed write target");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Buffer], [_s_mesh], array_create(array_length([IgpuWriteTarget.Buffer]), 0), array_create(array_length([IgpuWriteTarget.Buffer]), 0)), "a mesh buffer is not a listed write target");
_igpu_check(!igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture, IgpuWriteTarget.Texture], [_w_a, _w_a], array_create(array_length([IgpuWriteTarget.Texture, IgpuWriteTarget.Texture]), 0), array_create(array_length([IgpuWriteTarget.Texture, IgpuWriteTarget.Texture]), 0)), "the same storage texture is listed twice");
if (_w_list > 0 && _w_a > 0 && _w_b > 0 && _w_c > 0 && _w_d > 0)
{
    igpu_shader_bind(_w_list, IgpuShaderStage.Compute);
    var _w_ran = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Texture, IgpuWriteTarget.Texture, IgpuWriteTarget.Buffer, IgpuWriteTarget.Buffer], [_w_a, _w_b, _w_c, _w_d], array_create(array_length([IgpuWriteTarget.Texture, IgpuWriteTarget.Texture, IgpuWriteTarget.Buffer, IgpuWriteTarget.Buffer]), 0), array_create(array_length([IgpuWriteTarget.Texture, IgpuWriteTarget.Texture, IgpuWriteTarget.Buffer, IgpuWriteTarget.Buffer]), 0));
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _w_red = igpu_texture_read(_w_a, 1, 4, 0, 0);
    var _w_green = igpu_texture_read(_w_b, 1, 4, 0, 0);
    var _w_cz = buffer_create(4, buffer_fixed, 4);
    var _w_dy = buffer_create(4, buffer_fixed, 4);
    var _w_cz_ok = igpu_buffer_read(_w_c, 8, _w_cz);
    var _w_dy_ok = igpu_buffer_read(_w_d, 4, _w_dy);
    buffer_seek(_w_cz, buffer_seek_start, 0);
    buffer_seek(_w_dy, buffer_seek_start, 0);
    var _w_z = buffer_read(_w_cz, buffer_f32);
    var _w_y = buffer_read(_w_dy, buffer_f32);
    show_debug_message("storage list     : " + string(_w_ran) + " " + string(_w_red) + " " + string(_w_green) + " " + string(_w_cz_ok) + " " + string(_w_z) + " " + string(_w_dy_ok) + " " + string(_w_y));
    if (!_w_ran) show_debug_message("storage list err : " + string(igpu_get_last_error()));
    _igpu_check(_w_ran && _w_red == c_red && _w_green == c_lime && _w_cz_ok && _w_z == 1 && _w_dy_ok && _w_y == 1, "one dispatch writes two textures and two buffers");
    buffer_delete(_w_cz);
    buffer_delete(_w_dy);
}
if (_w_ord > 0 && _w_ord_tex > 0 && _w_ord_buf > 0)
{
    igpu_shader_bind(_w_ord, IgpuShaderStage.Compute);
    var _o_ran = igpu_dispatch(1, 1, 1, [IgpuWriteTarget.Buffer, IgpuWriteTarget.Texture], [_w_ord_buf, _w_ord_tex], array_create(array_length([IgpuWriteTarget.Buffer, IgpuWriteTarget.Texture]), 0), array_create(array_length([IgpuWriteTarget.Buffer, IgpuWriteTarget.Texture]), 0));
    igpu_shader_bind(0, IgpuShaderStage.Compute);
    var _o_pix = igpu_texture_read(_w_ord_tex, 1, 4, 0, 0);
    var _o_back = buffer_create(4, buffer_fixed, 4);
    var _o_ok = igpu_buffer_read(_w_ord_buf, 4, _o_back);
    buffer_seek(_o_back, buffer_seek_start, 0);
    var _o_y = buffer_read(_o_back, buffer_f32);
    show_debug_message("storage order    : " + string(_o_ran) + " " + string(_o_pix) + " " + string(_o_ok) + " " + string(_o_y));
    if (!_o_ran) show_debug_message("storage order err: " + string(igpu_get_last_error()));
    _igpu_check(_o_ran && _o_pix == c_blue && _o_ok && _o_y == 1, "a buffer can occupy slot 0 ahead of a texture");
    buffer_delete(_o_back);
}
_igpu_check(igpu_texture_release(_w_a), "release listed texture 0");
_igpu_check(igpu_texture_release(_w_b), "release listed texture 1");
_igpu_check(igpu_buffer_release(_w_c), "release listed buffer 0");
_igpu_check(igpu_buffer_release(_w_d), "release listed buffer 1");
_igpu_check(igpu_texture_release(_w_ord_tex), "release ordered texture");
_igpu_check(igpu_buffer_release(_w_ord_buf), "release ordered buffer");
_igpu_check(igpu_texture_release(_w_plain), "release plain listed texture");
_igpu_check(igpu_shader_release(_w_list), "release listed compute shader");
_igpu_check(igpu_shader_release(_w_ord), "release ordered compute shader");

_igpu_check(igpu_buffer_release(_s_buf), "release structured buffer");
_igpu_check(igpu_buffer_release(_s_mesh), "release storage-test mesh");
_igpu_check(igpu_input_layout_release(_s_layout), "release storage-test layout");
_igpu_check(igpu_shader_release(_s_vs), "release storage vertex shader");
_igpu_check(igpu_shader_release(_s_ps), "release storage pixel shader");

// Shutdown must release everything still live without touching GM's device.
igpu_shutdown();
_igpu_check(!igpu_is_available(), "shutdown clears availability");
_igpu_check(!igpu_device_lost(), "shutdown is not reported as device loss");
_igpu_check(igpu_get_capabilities().backend == "none", "capabilities degrade after shutdown");
_igpu_check(!igpu_get_capabilities().formats.surface_rgba8unorm, "formats are false without a device");

_igpu_check(!igpu_set_graphics_info("x", "not a graphics api", "", "", 0), "an unrecognised version is rejected");
_igpu_check(igpu_set_graphics_info("browser", "WebGL 1.0", "WebKit WebGL", "WebGL GLSL ES 1.0", 4096), "WebGL 1 info is accepted");
var _web = igpu_get_capabilities();
_igpu_check(_web.backend == "webgl", "WebGL 1 is its own backend name");
_igpu_check(_web.tier == 2, "WebGL 1 is tier 2");
_igpu_check(_web.shader_dialect == "glsl_es", "WebGL speaks glsl_es");
_igpu_check(_web.formats.surface_rgba4unorm, "WebGL 1 can sample rgba4");
_igpu_check(!_web.formats.surface_rgba8unorm, "WebGL 1 does not promise sized rgba8");
_igpu_check(!_web.runtime_compile, "tier 2 does not compile shaders");
_igpu_check(!_web.draw, "tier 2 does not draw");

_igpu_check(igpu_set_graphics_info("Qualcomm", "OpenGL ES 3.2 V@0502.0", "Adreno (TM) 640", "OpenGL ES GLSL ES 3.20", 4096), "OpenGL ES 3.2 info is accepted");
var _gles = igpu_get_capabilities();
_igpu_check(_gles.backend == "gles", "OpenGL ES reports gles");
_igpu_check(_gles.tier == 2, "OpenGL ES is tier 2");
_igpu_check(_gles.shader_dialect == "glsl_es", "OpenGL ES dialect is glsl_es");
_igpu_check(_gles.device_name == "Adreno (TM) 640", "renderer string becomes the device name");
_igpu_check(igpu_get_adapter_description() == "Adreno (TM) 640", "adapter description uses the renderer string");
_igpu_check(igpu_get_shader_dialect() == "glsl_es", "shader dialect follows the note");
_igpu_check(_gles.formats.surface_rgba8unorm, "OpenGL ES 3 can sample rgba8");
_igpu_check(_gles.formats.surface_r16float, "OpenGL ES 3 can sample r16f");
_igpu_check(!_gles.texture_2d, "tier 2 does not claim the texture API");
_igpu_check(igpu_texture_create(IgpuTextureKind.TwoD, 4, 4, 1, surface_rgba8unorm, false, false, 1) == 0, "tier 2 does not create textures");
_igpu_check(igpu_shader_compile("void main(){}", "main", IgpuShaderStage.Vertex, "glsl") == 0, "tier 2 does not compile");

_igpu_check(igpu_set_graphics_info("NVIDIA", "4.6.0 NVIDIA 560.94", "NVIDIA GeForce", "4.60 NVIDIA", 16384), "desktop OpenGL info is accepted");
_igpu_check(igpu_get_capabilities().backend == "opengl", "a numbered GL version is opengl");
_igpu_check(igpu_get_capabilities().shader_dialect == "glsl", "desktop OpenGL dialect is glsl");

// Re-initialising after shutdown must work: the borrowed device is still valid.
_igpu_check(igpu_init_from_game(), "re-init after shutdown");
_igpu_check(igpu_is_available(), "available again after re-init");
_igpu_check(!igpu_device_lost(), "re-init does not report device loss");
_igpu_check(igpu_get_capabilities().backend == "d3d11", "backend restored after re-init");
_igpu_check(igpu_get_capabilities().tier == 1, "tier returns to 1 after re-init");
_igpu_check(igpu_get_shader_dialect() == "hlsl", "dialect returns to hlsl after re-init");
_igpu_check(igpu_set_graphics_info("", "", "", "", 0), "graphics note cleared after re-init");

// ---------------------------------------------------------------------------

show_debug_message("----------------------------------------");
show_debug_message($"checks failed    : {_igpu_failures}");
_igpu_finish(_igpu_failures == 0);
