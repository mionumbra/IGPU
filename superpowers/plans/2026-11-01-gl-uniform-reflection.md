# GL Uniform Buffers and Reflection Implementation Plan

> For agentic workers: use executing-plans inline with one final whole-branch reviewer. Track steps in the per-batch ledger. User already authorized continuous feature PR merge; do not ask again.

Goal: complete existing uniform reflection/member-write/binding flow in one tested batch.
Architecture: cached GL shader metadata, independent focused uniform module and backend-neutral shared lookups. Native subrange uploads preserve member neighbors. Raw GLSL physical binding stage*8+slot.
Tech Stack: C++17, native WGL/OpenGL, existing CMake Release probe, D3D11 DLL regression build.
Spec: superpowers/specs/2026-11-01-gl-uniform-reflection.md; move to superpowers/specs with plan on the new branch after PR47 merge.
Global constraints: version0.5.0, existing public functions, no manual yy/yyp, Windows DLL D3D11, feature branch -> PR -> merge; reuse out/build/win-x64-release.
Review Focus: legacy120 shaders with lazy separable reflection; compile/link transaction separation; vec3/array/matrix neighbor writes; late external mappings; persistent indexed range restoration and Dynamic GPU byte consistency.

## Task1: observed RED -> core uniform compute GREEN

Files: tests/gl_probe/uniform_tests.cpp/.h; src/CMakeLists.txt; tests/gl_probe/main.cpp; src/native/gl/igpu_gl_uniform.cpp/.h; GL loader/backend/draw; shared Backend/reflection interfaces; D3D backend fallback.

- [x] Branch gl-uniform-reflection from merged PR47. Save spec/plan and ledger using existing superpowers directories.
- [x] New test compiles stage2 source below, shader_reflect expects Data size16 logical slot0 member values uint/type2 offset0 size16 rows1 columns4 elements0. Existing code must fail unknown shader; build/run and inspect named failure.
```glsl
#version 430
layout(local_size_x=1) in;
layout(std140,binding=16) uniform Data { uvec4 values; };
layout(std430,binding=0) buffer Output { uint result[]; };
void main() { result[gl_GlobalInvocationID.x]=values[gl_GlobalInvocationID.x]; }
```
- [x] Add optional ProgramParameteri/GetActiveUniformBlockiv/Name/GetActiveUniformsiv loader fields. Query native UBO limits and gate hooks uniform_buffer_supported/uniform_reflection_supported (defaultfalse).
- [x] Add Backend::shader_uniforms(handle) and uniform_buffer_bytes(handle,entry). D3D uses its existing maps; GL resolves its own objects. Shared metadata helper returns UniformLayout pointer; replace entry->uniforms with layout throughout queries. uniform_write asks backend for Uniform role/bytes before packing.
- [x] Implement lazy reflection by temporary program (PROGRAM_SEPARABLE=true), attach one shader/link, read blocks/member descriptors, delete temporary program without UseProgram. Cache only success and preserve compile even if reflection link fails.
- [x] Implement GL UBO claim binding/native range save+restore using physical stage*8+slot; helper resolves Uniform4 metadata and mappings. Add native subrange backend patch function; existing writes/resizes retain GPU consistency.
- [x] Test public create16 usageDynamic bindUniform, uniform_write values[7,10,13,16], uniform_bind(stage2,slot0), dispatch4 to Storage output and readback literal words. Run full probe GREEN.

## Task2: complete layouts, restoration, lifecycle and failure behavior

- [x] Extend internal UniformMember with actual array/matrix stride and pack with defaults preserving D3D behavior. Pin vec3+float neighbor, float array, array[1], column/row mat2x3 and matrices-in-array with literal hand-derived GPU expectations.
- [x] Render actual vertex/pixel inputs using stage bindings0 and8. Dispatch with graphics program current. Compare cached reflection before/after graphics relink without changing current program.
- [x] Validate unknown/released/wrong-role/small/mapped, stage/slot limits, mismatched values, out-of-stage GLSL binding, unsupported type. Add guards before any GPU writes. Revalidate claimed stage2 inputs before dispatch and stage0/1 before draw.
- [x] Capture external UBO range/name/offset/size and generic binding. Verify replace/unbind/release restores original rather than preceding IGPU claim. Verify zero/partial/grow/shrink GPU through real shader reads. Cover shutdown restore/delete/context rebind and missing optional capabilities.
- [x] Build/run full probe; inspect output and errors. Fix failures through observed reproductions only.

## Task3: reviewed and merged capability batch

- [x] Document UBO raw GLSL mapping, supported reflection shape and constraints; update latest handovers.
- [x] Build full Windows IGPU DLL; run tools/verify_handover.ps1 and staged diff check. Stage untracked files.
- [ ] Commit tested implementation, request one read-only whole-branch review while preparing next batch read-only. Address concrete Important/Critical findings with regression proof.
- [ ] Push feature, PR with exact reviewed head, merge, confirm MERGED, fetch/fast-forward main and compare tested/main tree. Continue next capability batch under existing authorization.

Rulings: GPU subrange upload removes the unnecessary GL CPU shadow; native ordinary single-stage linking follows failed separable reflection to preserve existing shader source. See spec and execution ledger for observed RED/GREEN evidence.
