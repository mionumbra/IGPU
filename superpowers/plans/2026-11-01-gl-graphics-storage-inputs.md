# GL graphics storage input implementation plan

Use executing-plans inline, TDD, one independent whole-branch review. Spec: superpowers/specs/2026-11-01-gl-graphics-storage-inputs.md. API/version0.5.0, no exports/generated resources/yy/yyp; formal Windows DLL D3D11.

## Review focus
- Existing stages0..2 retain bindings8..31 and StructuredBuffer capability; stage3..5 independently gate real per-stage8block/global-binding limits before touching reserved indices32..55.
- Every draw route including sampled and patches validates graphics claims0,1,3..5, excludes compute2, and rejects externally mapped input without counters or caller state mutation.
- Full previous indexed name/start/size survives replacement/unbind/release/shutdown, with generic SSBO and foreign byte91 unchanged.
- Actual GS and pairedHull/Domain read correct readonly data through slot0/7; no synthetic format/name remapping.

## Tasks
- [x] Add real geometry/tessellation SSBO/late-map/shutdown regressions to tests/gl_probe/graphics_stage_tests.cpp, observe feature RED and a separate existing Vertex/Pixel late-map RED before production changes.
- [x] Extend igpu_gl_compute.cpp stage limit storage to6 and safe conditional queries, gl_storage_bind stage/slot capability gates, expose gl_storage_validate_inputs(first,last,entry) in igpu_gl_compute.h and invoke for graphics0,1,3..5 from both shared draw acceptance and sampled draw validation. Preserve compute dispatch logic and claim restoration.
- [ ] Run complete probe/DLL/handover/diff checks; document GLSL mappings and limits in OPENGL_COMPUTE.md/OPENGL_GRAPHICS_STAGES.md/handovers, commit/push featurePR. Integrate vertex-input approved main without losing either change and rerun combined checks before review.
- [ ] One independent whole-batch reviewer; real RED/GREEN for confirmed Important findings; exact-approved-head merge and original main tree/DLL verification under standing authorization.
