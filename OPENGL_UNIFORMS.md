# OpenGL uniform buffers and reflection

The existing IGPU 0.5.0 shader reflection, uniform member write and uniform binding APIs now work on the GL backend in the real WGL probe. The Windows GameMaker DLL continues to use D3D11, with its existing reflection and buffer handling. Public APIs, generated resources and version are unchanged.

## Native reflection

Reflection queries use the backend's own shader metadata. GL links a temporary single-stage program on first query, reads native block/member offsets and strides, caches successful results and deletes the temporary program. It first tries separable linking, then ordinary single-stage linking for vertex shaders without a separable interface. Geometry, hull/TCS and domain/TES additionally fall back to a native companion-stage link when a standalone link cannot supply the required pipeline interface. The temporary companion stages do not rewrite caller source. The current graphics/compute program is preserved. Shader compilation does not require reflection linking to succeed, so compilation/linking remain separate operations. Legacy GLSL120 shaders with no uniform blocks reflect an empty list.

Queries expose active native blocks and scalar, vector, matrix and array members, with actual byte offsets, spans, scalar kind, dimensions and array element counts. Struct members are exposed as native leaf paths such as `s[0].v`; a block-name prefix is removed. Scalar/vector arrays use the member name without a terminal `[0]`, including arrays of one element. Double scalar/vector/matrix metadata retains its native size and dimensions with Unknown scalar kind; writes to unsupported kinds are rejected.

Member spans stop after the final component. For example, a vec3 at offset 0 has size 12, so writing it preserves a scalar at offset 12. Matrix orientation and array/matrix strides come from the driver; the common packer retains its existing D3D defaults when a backend supplies no stride metadata. GL writes use native subrange uploads, preserving other GPU bytes. No duplicate CPU uniform shadow is needed on GL. Existing zero initialization and GPU-preserving Dynamic resizing apply.

## Raw GLSL UBO bindings

UBOs use a namespace separate from storage SSBOs. Raw GLSL must specify physical UBO binding `stage * 8 + slot`; reflection reports the public logical slot. No shader text is rewritten.

| Public stage | Public slots | GLSL UBO bindings |
| --- | --- | --- |
| Vertex 0 | 0..7 | 0..7 |
| Pixel 1 | 0..7 | 8..15 |
| Compute 2 | 0..7 | 16..23 |
| Geometry 3 | 0..7 | 24..31 |
| Hull / TCS 4 | 0..7 | 32..39 |
| Domain / TES 5 | 0..7 | 40..47 |

For a compute UBO on public slot 0:

```glsl
#version 430
layout(local_size_x = 1) in;
layout(std140, binding = 16) uniform Data { uvec4 values; };
layout(std430, binding = 0) buffer Output { uint result[]; };
void main() {
    uint i = gl_GlobalInvocationID.x;
    result[i] = values[i];
}
```

GL Uniform buffers accept positive sizes in multiples of four bytes, including packed blocks of 4, 8 and 12 bytes. Allocate the reflected native size directly; D3D retains its existing 16-byte allocation rule.

The existing API flow is: reflect `Data`, create a Uniform buffer of its reflected block size, write member `values`, bind that buffer to compute stage 2 slot 0, then dispatch to a Storage output. The reflected `Data.slot` is 0. The SSBO output uses dispatch entry 0 in its independent storage namespace.

Blocks outside their stage's UBO namespace fail reflection with a diagnostic. Use different block names for independent vertex and pixel blocks; native GL linking rules still apply when block definitions are shared across stages.

## Lifetime, validation and capabilities

Uniform binding persists until replaced, unbound with buffer 0, released or shut down. Each claim retains the caller's original indexed buffer, offset and size; replacing a claim does not replace that saved caller state. Every operation preserves the generic UBO binding. Unbind/release/shutdown restores the original indexed ranges before deleting owned buffers.

Writes require a valid Uniform buffer large enough for the reflected block, a supported member kind and the exact number of components. Unknown/released/wrong-role buffers, missing blocks/members, unsupported stages/slots and externally mapped buffers fail before mutation. Draw and dispatch recheck claimed input mappings because an external caller may map a buffer after binding it. Raw shader access bounds remain the author's responsibility.

UniformBuffer and UniformReflection capabilities retain their existing vertex/pixel/compute gate: a suitable context, all eight optional reflection/binding/query entrypoints, at least 24 UBO bindings, at least eight UBO blocks in each supported original stage and a positive native block limit. Geometry/hull/domain binding additionally requires that stage to be supported, at least eight blocks for that stage, and the requested physical binding to fit the actual context limit. A new stage capability does not promise every UBO namespace is available. Graphics draws recheck claimed inputs in stages 0, 1, 3, 4 and 5; compute stage 2 remains independent. Binding an entire buffer larger than that block limit is rejected. The independent GL storage binding mapping is documented in `OPENGL_COMPUTE.md`.

## Verification

The complete Release probe validates literal compute GPU words, std140 offsets/spans, both matrix orders, arrays and matrix arrays, packed native strides, struct-array leaves, double metadata/rejection, zero/partial uploads, grow/shrink/regrow, eight compute blocks summing to 36, public vertex/pixel reflection, Static/Dynamic writes and 16 samples with pixel128/64/32. It also checks all 24 indexed ranges, generic/current-program preservation, late mappings, missing entrypoints, legacy compatibility and shutdown cleanup/rebinding, along with every prior GL regression.

Logs: `out/uniform-red-run.log`, `out/uniform-core-run.log`, `out/uniform-double-red-run.log`, `out/uniform-complete-build.log`, `out/uniform-complete-run.log`, `out/uniform-dll-build.log`.

New graphics-stage verification: out/graphics-complete-run.log covers GS24/TCS32/TES40 reflection, writes, real pixels, late mappings and shutdown/rebind range restoration. See OPENGL_GRAPHICS_STAGES.md.
