# GL Draw Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use executing-plans inline with one independent whole-branch review.

**Goal:** Existing native GL diagnostic facade and verifiable caller input-state restoration, without adding GML exports.
**Architecture:** Focused diagnostics snapshot/record helper wrapping each public backend submission; existing bufferregistry serves binding query.
**Tech Stack:** C++17, desktop GL/WGL, cached CMake Release /MP8 probe and D3D11DLL.
**Spec:** superpowers/specs/2026-11-01-gl-draw-diagnostics.md

## Global Constraints
- Public API0.5.0, no yy/yyp edits, formalWindowsDLL D3D11.
- Continuous authorization, featurePR and exactapprovedheadmerge.
- Processlifetime GL counters; no internal-helper doublecount; no nativeerrorconsumption.

## Review Focus
- Every implementedpublic submission countedonce, failures/compute/helperpasses excluded.
- ActualVAO attribute source vs genericarray binding; invalid/releasedroles quietfalse.
- Snapshot queries valid on older actualcontexts and optionalentrypoint variations.
- Postrestore mismatches measured, including latefailed calls, preserving submiterror.
- Shutdown/rebind handles cannot alias and cumulativeGLtotals persist.

### Task1: Public diagnostics and restoration evidence
- [x] Add focused realGL tests for nativebinding, all publicdrawcounter deltas, failureandauxiliary exclusions and faultinjection; observeRED.
- [x] Add diagnosticsmodule/coregetattrib loader; wrap publicdraws, resolvebinding fromexistingregistry, observeGREEN.
- [x] Extend edge/fault/lifecycle cases and fullhistoricalprobe.

### Task2: Integrate
- [x] DLL, guide/handovers/diffverification.
- [ ] One independentwholebatchreview; regressionbacked Importantfixes.
- [ ] Push/PR/merge exactapprovedhead and verifymaintree.
