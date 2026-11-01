# GL mixed border sampler implementation

Use executing-plans inline, TDD, one independent whole-branch review. Spec: superpowers/specs/2026-11-01-gl-mixed-border-samplers.md.

## Review Focus
- Canonical border constructor validates each axis0..3 and stores actual U/V/W, not hardcoded3; ordinary no-color constructor remains0..2.
- Border color must apply when V or W is the only Border axis; source S/T/R/color/filter/LOD and caller state restore on every submission.
- Public full and native convenience constructors converge on same canonical semantics; comparison/invalid modes still reject without consuming a handle.
- Literal 2D pixel identity and linear edge interpolation; true3D W-only positive/negative outside/interior with16 GPU samples. Array layer selection/cube native sampling not reinterpreted.
- Replace old unsupported-mixture negative fixtures with actually invalid mode4; retain allocation-order checks and every historical/core test.

## Tasks
- [x] Add mixed 2D/3D regressions and observe actual RED before changing behavior.
- [x] Relax border-aware axis validation/store actual modes and use any-axis border color; observe GREEN.
- [x] Full probe/DLL/handover/diff verification, guide updates, commit and featurePR.
- [ ] Independent review, actual regression-backed Important fixes, exact approved-head merge and main-tree verification.
