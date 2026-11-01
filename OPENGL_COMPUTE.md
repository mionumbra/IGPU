# OpenGL compute and storage resources

This is the existing IGPU 0.5.0 API on the GL backend. The WGL probe builds GL; the Windows GameMaker DLL still builds D3D11. Public APIs and GameMaker resource formats are unchanged.

## Capabilities and resources

`ShaderStageCompute` and `UnorderedAccess` require GL 4.3 or the complete compute/SSBO/image extension set, transform-feedback active/paused state queries, all required dispatch/binding/barrier/query entry points, and at least eight compute SSBO blocks and image units. Each group count is in `1..min(65535, hardware limit)`.

`StructuredBuffer` additionally requires 32 SSBO bindings and eight storage blocks in each of the vertex, pixel and compute stages. These conservative gates cover the complete binding contract. GL UBOs and uniform reflection are documented in `OPENGL_UNIFORMS.md`.

Storage buffers use the existing single Storage role and stride/size restrictions. Combining Storage with Vertex, Index or Indirect remains invalid. Storage images are 2D, depth 1, in RGBA8, R8, RG8, R16F, R32F, RGBA16F or RGBA32F. RGBA4 remains valid for ordinary textures and is rejected for storage images.

## Raw GLSL binding contract

GLSL uses a shared SSBO binding namespace. IGPU reserves separate physical bindings for persistent storage inputs and temporary dispatch targets. Raw GLSL must use these explicit bindings; IGPU does not rewrite shader source.

| Resource | Public stage / slot | GLSL binding |
| --- | --- | --- |
| Dispatch write entry | Entry index 0..7 | 0..7 in its image or SSBO namespace |
| Vertex storage input | Stage 0, slot 0..7 | 8..15 |
| Pixel storage input | Stage 1, slot 0..7 | 16..23 |
| Compute storage input | Stage 2, slot 0..7 | 24..31 |
| Geometry storage input | Stage 3, slot 0..7 | 32..39 |
| Hull/TCS storage input | Stage 4, slot 0..7 | 40..47 |
| Domain/TES storage input | Stage 5, slot 0..7 | 48..55 |

The input formula is `8 + stage * 8 + slot`. For example, compute reading public storage slot 0 and writing a buffer at dispatch entry 0 uses:

```glsl
#version 430
layout(local_size_x = 1) in;
layout(std430, binding = 24) readonly buffer Input { uint src[]; };
layout(std430, binding = 0) writeonly buffer Output { uint dst[]; };
void main() {
    uint i = gl_GlobalInvocationID.x;
    dst[i] = src[i] * 2u;
}
```

`igpu_storage_bind` persists until replaced, unbound with buffer 0, the owned buffer is released, or IGPU shuts down. Unbinding restores the caller's previous indexed buffer, offset and size. Supported stages are vertex 0, pixel 1, compute 2, geometry 3, hull/TCS 4 and domain/TES 5, with slots 0..7. Each stage3..5 claim additionally requires the actual graphics stage, eight storage blocks for that stage, and its particular physical binding to fit the native global limit. The existing StructuredBuffer capability and stages0..2 gates are retained, so support for ordinary storage inputs does not promise every higher-stage slot. Readonly declarations are the shader author's responsibility. A compute input cannot also be a dispatch write target; graphics input claims may refer to a buffer being written by compute.

## Dispatch and program lifetime

Stage 2 compiles and links into a separate compute program. Binding, unbinding or releasing compute shaders preserves the current graphics program. A failed compute link preserves the previous shader and executable. Dispatch temporarily uses compute and restores the caller's program. Active unpaused transform feedback must first be paused or ended; dispatch then preserves its paused capture state. A current program marked for deletion must first be unbound, since switching away would destroy it and prevent restoration.

Dispatch arrays contain 1..8 entries. Kind 0 is an image, kind 1 a Storage buffer; entry position determines binding even with mixed kinds. Thus `[image, buffer, image]` uses image bindings 0 and 2 plus SSBO binding 1. Duplicate targets of the same kind are rejected. A single image may select a nonzero mip; mixed nonzero mips and nonzero layers retain the existing facade's rejection rules.

Images use read/write access, supporting imageLoad/imageStore in-place updates. A full memory barrier follows each dispatch so later dispatches, vertex/pixel storage fetches, sampling and CPU readback observe writes. Shaders remain responsible for keeping their own accesses inside the resource and selected mip.

Every target and active compute input is validated before installing temporary write bindings. Unknown, released, wrong-role or externally mapped buffers are rejected. Inputs are revalidated at dispatch because a caller can map them after claiming a slot. Every graphics submission likewise revalidates persistent vertex/pixel/geometry/hull/domain inputs before drawing, including sampled and patch paths. A buffer mapped after binding is rejected without drawing or changing the caller state. Compute-only input claims do not block graphics; dispatch alias rules are unchanged.

Dispatch restores all six image fields (name, level, layered flag, layer, access, format), touched SSBO indexed buffer/ranges, generic SSBO binding and current program. Persistent storage operations preserve the generic binding. Shutdown restores persistent caller claims before deleting resources and preserves the borrowed GL context.

## Verification

The Release WGL probe checks GPU words, asymmetric 3D groups, local sizes, chained reads, seven image formats at base and nonzero mips, untouched levels, mixed entries, eight writable buffers and images, read/modify/write, and stage input bindings. It renders 16 actual samples using vertex/pixel SSBO reads and immediately samples a compute-written image. It compares caller image fields and all 32 indexed ranges, covers validation and seven optional-entrypoint failures, and verifies native program/shader/buffer/image deletion and context rebinding.

Logs: `out/compute-complete-build.log`, `out/compute-complete-run.log`, `out/compute-dll-build.log`. The prior texture comparison, buffer, instanced/indirect and query/fence suites run in the same probe.

The graphics storage regression renders sixteen samples for geometry/hull/domain at both slot0 and slot7 and inspects native SSBO bindings, replacements, borrowed ranges, generic state and foreign byte91. It checks late mappings for higher stages, every graphics family for Vertex, Pixel rejection, and a mapped Compute-only input that still allows graphics. Shutdown restores higher-stage SSBO ranges before deleting owned storage buffers. Feature REDs are out/storage-complete-red-run.log; full GREEN is out/storage-green-final-run.log.
