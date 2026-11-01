# GL Raster and Stencil Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use executing-plans inline, then one independent whole-branch review.

**Goal:** Existing raster/depth descriptors cover wireframe, depthclamp and native stencil with exact callerstate restoration.
**Architecture:** Extend focused existing RasterObject/DepthObject and shared save/apply/restore helpers; add a dedicated real-WGL state suite.
**Tech Stack:** C++17, desktopGL/WGL, cached CMake Release /MP8, formalD3D11DLL.
**Spec:** superpowers/specs/2026-11-01-gl-raster-stencil.md

## Global Constraints
- API0.5.0, no yy/yyp edits or extraexports, formalWindowsDLL D3D11.
- Samepublicdescriptor forbothstencilfaces; preserve caller's independentfront/back state.
- Nativeactualcontext depthclampgate; no newcapabilitykey; no automaticdepth/stencilstorage oncolor-onlytargets.
- Standingcontinuousauthorization, featurePR/onefreshreview/exactapprovedheadmerge.

## Review Focus
- Core/compatibility polygonrestore: asymmetricfaces onlywhere nativepermits; no invalidGLquery onoldercontexts.
- Depthclamp handle0vs explicitcliptruefalse andcaller enable restoration.
- All8stencilops, clampvswrapboundaries, compare/ref/read/write masks andasymmetricfront/backrestoration.
- Depth-disabled stencil stilloperates;stencfail/zfail/explicitdisable remain distinguishablewith realpixels/bytes.
- Every existingdraw family sharesnewstate semantics; latefailure mustnotleakstate, color-onlyFBOcontractunchanged.

### Task1: Raster coverage
- [ ] Add realwireframe/depthclip state regressions andobserveRED.
- [ ] Implement nativepolygon/depthclamp state/capability and exactrestoration; observeGREEN.

### Task2: Stencil coverage
- [ ] Add realpackedD24S8stencil operations/compare/masks/bothfaces regressions andobserveRED.
- [ ] Implement core2separateface save/apply/restore; verify allsubmissionfamilies andinvalidcases.
- [ ] CompletehistoricalGLprobe.

### Task3: Integrate
- [ ] DLL, docs/handovers/diffverification.
- [ ] Onefreshwholebranchreview andregression-backed Importantfixes.
- [ ] Push/PR/merge exactapprovedhead andverifymaintree.

Existing main.cpp regression fixtures currently assert wireframe/depthclipfalse/stencilenable rejection at approx4675/4680/5076 and Wireframe=false at4851. At new-state branch, replace obsolete unsupported-feature assertions with invalidfill/invalidstencil-op validation, and require the newly implemented Wireframe capability. Keep older cull/depth/sample expectations intact. New tests cover the accepted descriptors in the dedicated suite.
