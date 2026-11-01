# GL Geometry and Tessellation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline and one independent whole-branch review.

**Goal:** Complete native geometry/tess stages and patch submission with transactional graphics binding.
**Architecture:** Focused graphics context/switch helpers and shared private draw implementation; extend existing native shader map/program handling.
**Tech Stack:** C++17, desktopGL/WGL, cached CMake Release /MP8 probe and D3D11DLL.
**Spec:** superpowers/specs/2026-11-01-gl-geometry-tessellation.md

## Global Constraints
- Public0.5.0, no yy/yyp changes, formal WindowsDLL remainsD3D11.
- Continuous user authorization, featurebranch/PR/merge exact approvedhead.

## Review Focus
- Native link failure retains last valid program and requested bindings.
- Incomplete tess pairs use valid ordinary pipeline; patchdraw rejects missing stage.
- Active transform feedback and program lifetime cannot yield false-success switches.
- Safe patch ranges/mappings and all caller native state including PATCH_VERTICES.
- New UBO stage namespaces cannot weaken existing0..2 capability gates/lifetime.

### Task1: Native stage compilation/binding and drawing
- [x] Observe public geometry/tess compile/patch RED.
- [x] Add graphics capability helper/optional PatchParameteri, transactional native graphics linking and paired staging.
- [x] Share private draw implementation for safe patch submission and parameter restoration.
- [x] Verify actual geometry/tess pixels/samples, both pair orders, unbind and link rollback.

### Task2: Resources and adversarial state
- [x] Extend perstage UBO bounds/reflection/claims and graphics late-mapping coverage with actual GPU data.
- [x] Add patch range/role/state, optional gating, activefeedback and lifetime/transaction failure regressions.
- [x] Full existing GL probe plus new stage shutdown/rebind.

### Task3: Integrate
- [x] DLL build, guides/handovers/diff verification.
- [ ] One independent whole-batch review and regression-backed fixes for confirmedImportant findings.
- [ ] Push, PR, merge exact approvedhead and verify main tree.
