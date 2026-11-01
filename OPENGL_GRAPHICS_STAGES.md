# OpenGL geometry and tessellation stages

The real WGL probe now supports native GLSL geometry, hull and domain shaders through the existing IGPU 0.5.0 APIs. Hull maps to tessellation control (TCS); domain maps to tessellation evaluation (TES). The formal Windows GameMaker DLL remains D3D11. Public API/version and generated GameMaker resources are unchanged.

| Public stage | Native GLSL stage | UBO namespace |
| --- | --- | --- |
| Geometry 3 | geometry shader | 24..31 |
| Hull 4 | tessellation control shader | 32..39 |
| Domain 5 | tessellation evaluation shader | 40..47 |

## Compilation and graphics binding

Use raw GLSL appropriate to the actual context, including the native stage layout declarations. Shader source is not rewritten. Mesh stage 6 and amplification stage 7 remain unsupported.

Graphics binding links a candidate program before replacing the last valid program and requested bindings. A failed link, incompatible interface or program allocation leaves the previous program, bindings and rendered result intact. Compute continues to use its separate program.

TCS and TES may be bound in either order. Until both exist, the requested stage is retained but both tessellation stages are omitted from the native graphics program, so the ordinary VS/PS pipeline (with an optional GS) remains usable. Completing the pair enables tessellation. Unbinding or releasing either removes the native pair and retains the other requested stage. Native GLSL interface and block-name rules still apply across the linked stages.

Program changes are rejected while transform feedback is active and unpaused, before the current program or requested bindings change. Paused capture permits explicit replacement; finish the capture before submitting unrelated graphics work.

## Patch drawing

Use the existing patch draw API after binding both hull and domain shaders. The current native program must be the IGPU-owned program successfully linked with the TCS/TES pair. The backend records executable stage metadata immediately after its own successful links; later attach/detach operations do not replace an installed executable. Owned tessellation continues to draw after detaching shader objects. External programs are rejected for patch/new-stage drawing even if their current attachments contain TCS/TES. Existing caller-owned ordinary VS/PS programs remain supported; external advanced-stage programs and externally relinking an IGPU-owned program are outside this contract.

Control points must be 1..32 and fit the native maximum. Vertex counts must contain complete patches. A negative count infers the remaining vertex range. Buffer/layout roles, safe first/count bounds, mapped inputs, state handles and late UBO mappings follow the shared draw validation. The private patch submission mode preserves the public ordinary primitive enum.

Every submission family rejects ordinary primitives while the owned tessellation pair is executable, and rejects geometry input mismatches before native drawing. Geometry checks group points, lines/line strips and triangles/strips/fans; for patches the input family follows native TES mode and point mode. This validation covers direct, indexed, instanced, indirect, MRT and sampled draws.

Patch submission saves and restores the caller's PATCH_VERTICES, together with the existing vertex input, target and draw-state restoration. Ordinary draws remain available after removing an incomplete tessellation pair.

## Uniform blocks

The physical binding formula remains stage * 8 + public slot. Each new stage requires native support, at least eight blocks for that stage and an available physical binding. Thus a context may expose the stage while rejecting an unavailable UBO slot. Existing vertex/pixel/compute UBO capability requirements remain unchanged.

Reflection first tries native separable and single-stage linking. For geometry/TCS/TES that require another native stage for linking, a temporary companion vertex shader (and a TES for TCS) supplies the link context. Caller source is unchanged, successful metadata is cached, and temporary native objects are deleted. Writes, claim restoration and mapped-input guards reuse the existing UBO path. See OPENGL_UNIFORMS.md for the full contract.

Storage SSBO bindings retain their existing stages 0..2 contract; additional graphics-stage SSBO namespaces are not exposed by this batch.

## Actual capability and lifetime

Geometry requires GL 3.2 plus queryable transform-feedback state (GL 4.0 or ARB_transform_feedback2). Tessellation requires GL 4.0 or ARB_tessellation_shader, the same feedback query support, PatchParameteri and a positive native patch maximum. Missing optional patch support disables tessellation without disabling ordinary drawing or geometry.

Release uses transactional graphics relinking before deleting a shader. Shutdown clears UBO claims and reflection caches, deletes owned programs/shaders/buffers, and preserves foreign bindings and the borrowed WGL context. If the current program is the backend's owned graphics program and program switching is allowed, shutdown unbinds it before deletion so attached shaders are freed immediately. An active unpaused capture retains native deferred deletion semantics.

## Verification

Observed REDs: out/graphics-red-run.log (unsupported stages), out/graphics-expanded-red-run.log (new-stage reflection/binding), out/graphics-native-program-red-run.log (external ordinary native program accepted as patches), and out/graphics-shutdown-red-run.log (current program kept deleted shaders alive).

The complete WGL run is out/graphics-complete-build.log and out/graphics-complete-run.log, ending PASS. It verifies a geometry-expanded quad and tessellated triangle with sixteen samples and real pixels; both pair orders and removal; geometry24/hull32/domain40 UBO reflection, writes, pixel effects and late mappings; transactional native failures; active/paused transform feedback; caller PATCH_VERTICES=7; invalid patch counts/ranges/states; optional entrypoints; external native program rejection; and shutdown native deletion, indexed UBO restoration and fresh rebind. Every prior GL suite also passes. The formal DLL build is out/graphics-dll-build.log.

Independent-review regressions: out/graphics-review-red-run.log observes attachment/executable differences and incompatible primitive false-success. out/graphics-review-green-build.log and out/graphics-review-green-run.log validate the fix, both detach/attach ownership cases and every submission family, plus the complete historical probe.
