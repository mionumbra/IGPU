# GL native facade bridge implementation

Use executing-plans inline and one fresh independent whole-branch reviewer. Standing continuous authorization applies; do not ask again. Spec: superpowers/specs/2026-11-01-gl-native-compatibility.md.

## Review Focus
- No guessed defaults: compare existing D3D convenience constructors. Anisotropy on point/linear is validated but not applied; filter2 maps to three linear filters plus its factor.
- No accidental exports or alteration of GML facade routing. Native bytecode reflection is deliberately outside GL shader-handle reflection.
- All three convenience draws use canonical target validation and exactly one diagnostic boundary. Default layer/mip0, selected layer/mip routing and invalid negative values are tested.
- No source parameter, input, framebuffer or viewport leak on accepted/rejected paths; layout step0 retains canonical supported format validation.

## Tasks
- [x] Add real native facade sampler/resource/draw/layout regressions and observe actual RED before implementation.
- [x] Bridge wrappers to shared GL paths with native defaults/validation and observe GREEN.
- [x] Extend literal pixels, defaults, layers/mips, counters, invalid/released/state cases and run all historical suites.
- [ ] Build formal DLL, update guides/handovers, run handover/diff checks, commit and create feature PR.
- [ ] One independent review; address confirmed Important with actual RED/GREEN and reviewer follow-up; merge exact approved head and verify main tree.
