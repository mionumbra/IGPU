# GL Layered Textures Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline and one fresh-context whole-branch review.

**Goal:** Complete3D/array/cube creation through draw/read/sample/compute paths.
**Architecture:** Extract focused GL texture module, reuse shared subresource attachment resolution and native-target binding guards.
**Tech Stack:** C++17, OpenGL/WGL, CMake Release, existing D3D11 DLL.
**Spec:** superpowers/specs/2026-11-01-gl-layered-textures.md

## Global Constraints
- Public version0.5.0 and no yy/yyp edits; formal DLL remains D3D11.
- Reuse build cache, feature branch/PR/merge; standing authorization covers continuous execution.

## Review Focus
-3D depth shrink versus fixed array/cube layers and cube face order.
-Caller native-target texture bindings, indexed masks and independent FBO/PBO state.
-MRT aliases identify exact layer/mip and private FBO cleanup on failure.
-Compute layered binding agrees with native shader image type and restores all fields.
-Unsupported optional/context paths retain2D behavior and honest capabilities.

### Task1: Texture lifetime, layers and target attachment
- [x] Observe public non2D creation RED.
- [x] Extract texture module and generalize allocation/zero/read/generate/release with native limits and strict subresources.
- [x] Extend MRT to layer-aware attachments and duplicate identity; verify all kinds/layers/mips and cross-kind pixels.

### Task2: Native-target sampling and compute
- [x] Observe typed sample/layered image/W addressing REDs.
- [x] Use native targets and sampler types throughout ordinary sample state; retain2D comparison boundary.
- [x] Persist samplerW and bind compute non2D images layered; add kind capability gates.
- [x] Verify typed GPU samples, W addressing, whole-layer writes, guards, exact state and cleanup.

### Task3: Complete batch
- [x] Full GL probe, DLL build, handover/diff verification; update guides and handovers.
- [ ] One independent whole-batch review, fix confirmed Important findings with regression evidence.
- [ ] Push feature branch, PR and merge exact approved head; verify main tree.
