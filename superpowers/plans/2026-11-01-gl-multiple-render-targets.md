# OpenGL Multiple Render Targets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline, with one independent whole-branch review.

**Goal:** Complete the existing OpenGL render-target array path for one through four outputs.
**Architecture:** Focused target resolution/FBO guard, shared draw path, indexed blend/mask preservation.
**Tech Stack:** C++17, desktop OpenGL/WGL, existing CMake Release probe and D3D11 DLL.
**Spec:** superpowers/specs/2026-11-01-gl-multiple-render-targets.md

## Global Constraints
- Public API/version remains 0.5.0; no yy/yyp edits.
- Windows GameMaker DLL remains D3D11; exercise GL in the real WGL probe.
- Reuse the existing checkout/build cache and use a feature PR.
- Continuous authorization: implement, test, review, push and merge without repeated prompts.

## Review Focus
- Separate READ/DRAW FBOs and caller FBO routing/attachments survive success and failure.
- Indexed masks and blend fields above public slot four survive global descriptor calls and allocation.
- Huge, fractional, nonfinite wire values fail before integer conversion or resource mutation.
- Single-target owned depth and caller scissor behavior remain compatible.
- Every temporary FBO is deleted and mapped input/range errors issue no draw.

### Task 1: MRT output and bounded target resolution
- [x] Add tests/gl_probe/target_tests.cpp/.h and integrate CMake/main; observe two-output RED on existing backend.
- [x] Add src/native/gl/igpu_gl_targets.cpp/.h and minimal draw-resource accessors; route all backend wrappers.
- [x] Validate strict arrays, roles, compatible mip dimensions and safe vertex counts; preserve single-target depth/scissor using its FBO.
- [x] Observe real 2/3/4-output/mip/format GPU GREEN and invalid-input state preservation.

### Task 2: Indexed state and honest capability
- [x] Add failing indexed blend/write-mask preservation and capability regressions.
- [x] Add optional loader fields and focused color-state helper; wire Backend capability and actual context/limits.
- [x] Preserve every native indexed slot for explicit blend descriptors and texture initialization.
- [x] Verify independent caller FBO routing/attachments and success/late-failure cleanup.

### Task 3: Integrate whole batch
- [x] Run full Release GL probe, Windows DLL build, handover verifier and diff checks.
- [x] Document behavior/limits and observed evidence in OPENGL_TARGETS.md and handovers.
- [ ] Commit, obtain one independent whole-batch review, reproduce/fix concrete Important findings and confirm final head.
- [ ] Push feature branch, create PR and merge the approved head; verify main tree matches tested tree.
