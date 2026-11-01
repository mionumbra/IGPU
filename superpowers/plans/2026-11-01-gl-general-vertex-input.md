# GL general vertex input implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline with one independent whole-branch reviewer.

**Goal:** Complete the existing GameMaker usage/type/step schema with real typed GL input descriptors shared by every draw path.
**Architecture:** Isolate descriptor parsing, GLSL attribute resolution and private-VAO binding in native/gl/igpu_gl_vertex.cpp/.h. Draw objects retain stream strides and typed element vectors; layout creation validates live vertex shader handles in the backend. Existing GL wrappers/diagnostics and canonical draws remain the submission boundaries.
**Tech Stack:** C++17, desktop GL2+/optional integer attributes and instancing, WGL real-GPU probe, cached CMake VS2022 Release.
**Spec:** superpowers/specs/2026-11-01-gl-general-vertex-input.md

## Global constraints
- API/export/version0.5.0; formal Windows DLL D3D11; no yy/yyp or new public functions/capability keys.
- Six existing formats and nine usages; raw UByte4 requires actual integer attribute support. Step1 requires existing instancing capability.
- Stream strides zero/negative mean packed; positive padding allowed up to2048, not below packed size.
- Preserve caller GL VAO/buffers/higher attributes/FBO/viewport, old explicit-location custom shaders and all historical suites.
- Standing user authorization covers implementation, featurePR, independent review and exact-approved-head merge.

## Review focus
- Reordered elements, repeated usage indices and colour+UV coexistence must bind distinct linked locations rather than overwrite attribute1.
- Independent offsets for interleaved vertex/instance descriptors, padded/tightly packed strides, all formats and integer normalization.
- Every draw route uses the same binder, clears all private attributes/divisors, and never changes caller VAO attributes including slots5/6.
- Invalid arrays/counts/enums/steps/strides/shaders and unsupported features reject before handles/GPU submissions; draw rejections preserve counters and source buffers.
- Core VAO0 must remain query-safe, with GL4.1 avoiding4.3 LONG tokens; run the general suite inside the actual4.1core context too.

## Task1: Real descriptor/input regressions and RED
**Files:** tests/gl_probe/vertex_layout_tests.cpp/.h, tests/gl_probe/main.cpp, tests/gl_probe/diagnostics_tests.cpp, src/CMakeLists.txt.
**Produces:** bool run_gl_vertex_layout_tests(); uses existing IGPU native entry points and linked GL3.3 shaders, no new production symbols in the RED tests.
- [x] Build a real8x8 target fixture, literal16sample pixel checks, and caller VAO slots5/6 sentinels. A canonical float2/stride8 control must pass.
- [x] Exercise float1/2/3/4/Colour/UByte4 values, float1/3/4 positions, nine semantic names, reordered fields and repeated UVs with explicit nonordinal locations. Inspect exact integer-vs-normalized pixels.
- [x] Exercise direct/indexed/sampled/target and instanced/indirect/indexed-indirect paths, multiple fields per stream and packed/negative/padded strides. Add invalid count/array/enum/step/stride/shader and missing-instance/counter checks.
- [x] Run cmake --build out/build/win-x64-release --config Release --target igpu_gl_probe --parallel8 then src/Release/igpu_gl_probe.exe; redirect out/vertex-red-{build,run}.log. Require actual feature-related FAIL, not compilation errors. Keep all historical suites enabled.

## Task2: Shared parser/binder and GREEN
**Files:** src/native/gl/igpu_gl_vertex.cpp/.h, igpu_gl_draw.cpp, igpu_gl_backend.cpp, igpu_gl_loader.cpp/.h, igpu_gl_diagnostics.cpp/.h.
**Consumes/produces:** GlVertexElement {usage,index,components,type,normalized,integer,step,offset}; GlVertexLayout {vertex_stride,instance_stride,elements}; gl_vertex_layout_parse(usage,type,step,count,vstride,istride,out) -> bool; gl_vertex_layout_resolve(layout,program,out) -> bool; gl_vertex_layout_bind(layout,resolved,vertex_name,instance_name); gl_vertex_layout_unbind(const GlVertexBindings& resolved).
- [x] Parse integer arrays transactionally, compute separate per-stream cursors, validate count/limits/enums/strides and gate integer/instance features. Validate live stage0 shader in both backend layout facades. Accept vertex buffer stride1..2048, keeping default8 and all other bind validation.
- [x] Load GetAttribLocation/GetActiveAttrib and optional VertexAttribIPointer. Resolve canonical names/unindexed or indexed suffix, retain custom ordinal fallback; validate bound live shader scalar/vector types and reject overlapping/multilocation descriptors before mutating GL state. Remove forced colour/UV aliasing at link.
- [x] Replace all four direct bind sites plus the instanced record special case with shared descriptor resolution/binding; reset every used private-VAO attribute/divisor. Expand diagnostics snapshots to the actual context attribute count, retaining actual version/extension query guards.
- [x] Build/run complete probe out/vertex-green-{build,run}.log, inspect exit0/PASS, pixels/state/counters and actual4.1core results. Fix concrete failures without weakening expected pixels or historical tests.

## Task3: Ship the validated batch
**Files:** OPENGL_VERTEX_INPUTS.md, HANDOVER.md, SESSION_HANDOVER.md and spec/plan updates.
- [ ] Build formal IGPU DLL, run tools/verify_handover.ps1 and git diff --check; retain exact commands/logs. Commit/push feature branch and draftPR against the landed dependency main.
- [ ] Dispatch one fresh independent whole-branch reviewer over exact base/head. Confirmed Important findings get actual RED/GREEN and same-reviewer follow-up. No outstanding Important may merge.
- [ ] Merge exact approved head under existing authorization, ff original main and compare approved/merged trees; rebuild/copy formal DLL if main changed. Preserve reusable managed worktree/cache and report any rulings/deferred minors.
