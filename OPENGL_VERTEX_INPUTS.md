# OpenGL vertex input descriptors

The existing input-layout API now describes individual elements instead of recognizing a few record strides. All nine GameMaker usages and six existing types use the same vertex/instance binding path. The API and version remain0.5.0. The Windows GameMaker DLL remains D3D11; desktop GL validation uses the real WGL probe.

| Type | Bytes | Shader values |
| --- | ---: | --- |
| float1 / float2 / float3 / float4 | 4 / 8 / 12 / 16 | Floating scalar/vector; normal GL component expansion applies |
| colour | 4 | Unsigned RGBA bytes normalized to0..1 |
| ubyte4 | 4 | Four raw integer bytes0..255 through integer attributes |

UByte4 requires actual GL3.0/EXT_gpu_shader4 support and its integer-pointer entry point. Instance steps require the existing instancing capability. Unsupported combinations reject before allocating a layout. A layout requires a live vertex-shader handle; linked live input types are checked when drawing. Floating/normalized data and integer shader inputs cannot be mixed. Matrix/array inputs spanning multiple locations require separate scalar/vector shader inputs instead.

Elements are packed independently into the vertex stream (step0) and instance stream (step1), in array order within each stream. Each usage receives an index in first-seen order across the complete element list. Stride0 or negative selects the packed byte count; an explicit positive stride can add end padding but must cover its stream and be at most2048 bytes. The number of elements must fit all three arrays and GL_MAX_VERTEX_ATTRIBS. A layout with no instance elements accepts no positive instance stride. An instance layout requires the corresponding vertex-role buffer on an instanced/indirect draw.

| GameMaker usage | GLSL base name |
| --- | --- |
| position | in_pos |
| colour | in_colour |
| normal | in_normal |
| texcoord | in_uv |
| blendweight | in_blendweight |
| blendindices | in_blendindices |
| psize | in_psize |
| tangent | in_tangent |
| binormal | in_binormal |

Choose the base name or suffix0 for the first element of each usage, and suffix1,2,... for subsequent elements. Linked locations are respected, including explicit layout(location=N). Colour and UV can be active simultaneously. Custom names retain the legacy fallback to their element-index location; use explicit locations for custom GLSL identifiers. Inputs omitted from a layout use native current attribute values. Compatibility submissions preserve current values opaquely through the native current-state stack, including float, signed/unsigned integer and double kinds; they reject before drawing if the caller has filled that stack. Core contexts follow their native specification. GL4.6 guarantees drawing leaves current values unchanged; the older4.1core specification permits a location supplied by an enabled array to become undefined after drawing. Before subsequently using that location as a constant input on older core, callers must explicitly set its current value again. IGPU does not guess current kinds from array descriptors or shader inputs. The legacy native vertex-buffer query continues to inspect attribute0.

For example, usages[position,colour,texcoord,texcoord], types[float3,colour,float2,float2], steps[0,0,0,1] pack a24-byte vertex stream and an8-byte instance stream. The shader can use:

```glsl
#version 330
in vec3 in_pos;
in vec4 in_colour;
in vec2 in_uv;
in vec2 in_uv1;
out vec4 colour;
out vec2 uv;
void main() {
    gl_Position = vec4(in_pos + vec3(in_uv1, 0.0), 1.0);
    colour = in_colour;
    uv = in_uv;
}
```

Ordinary, indexed, sampled, render-target, instanced, indirect, indexed-indirect and patch submissions share descriptor resolution/binding. Each uses a private VAO, clears all attributes/divisors it enabled, and restores the caller VAO and buffer bindings. Diagnostics inspect every native attribute slot; core VAO0 remains query-safe and GL4.1 does not receive newer LONG-token queries.

Evidence: out/vertex-red-run.log fails on the original typed/padded descriptor limits after a passing float2 control. out/vertex-green-run.log and out/vertex-complete-run.log pass sixteen pixel positions per image for all formats/usages, reordered and repeated inputs, independent stream offsets, every affected draw family, rejection counters, and caller attributes5/6 plus foreign byte91. The suite also runs in the actual4.1core context. Additional diagnostic RED/GREEN evidence is recorded in SESSION_HANDOVER.md.

Current-value review evidence: out/vertex-current-red-run.log observes lost opaque constants, red rather than green omission pixels, and false acceptance with a full caller stack. out/vertex-current-green-run.log verifies matching-kind fixture queries for float/int/uint/double despite all-float array descriptors, sixteen green omission pixels, full-stack rejection, and balanced late sampled-uniform failure. Actual4.1core verifies an untouched omitted constant and explicit reseeding after strict legacy clobber. Production diagnostics do not query unknown current kinds. Normative scope: Khronos GL4.1 core section2.8; GL4.6 core section10.4/10.5 and compatibility current-state attribute group.

Forward-compatible contexts: GL3.0 and later inspect the native CONTEXT_FLAGS before any deprecated stack operation. A forward-compatible context skips the legacy current guard even before profile masks exist. The focused probe requests3.0 forward compatibility; this driver returns4.6 flags1, so a clearly reported3.0 version-metadata model exercises that old-version branch over the real forward context. out/vertex-forward-observed-red-run.log records the two unwanted legacy queries; out/vertex-forward-green-run.log requires zero, successful GLSL130/nonzero-VAO drawing, sixteen green pixels and a restored borrowed context. This is strict modeled3.0 coverage, not a claim of an actual3.0 driver run. Unknown current kinds on older forward contexts use the same explicit-reseeding boundary as older core.
