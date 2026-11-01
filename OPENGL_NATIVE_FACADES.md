# OpenGL existing native convenience facades

The native C++ facade headers retain convenience entry points that predate the slim0.5 GML exports. This batch routes those entry points to the same GL implementation as the existing advanced paths. It adds no GML exports; API/version stays0.5.0 and the formal Windows GameMaker DLL stays D3D11.

| Native convenience | Canonical behavior |
| --- | --- |
| Simple sampler with repeat flag | all axes Repeat when true, Clamp when false |
| Address sampler | same supplied Clamp/Repeat/Mirror mode on all axes |
| Axis sampler | separate U/V/W modes |
| Border sampler | supplied opaque GameMaker color, unpacked by the facade |
| Filter sampler | separate point/linear magnification, minification and mip filters |
| Offset/range variants | supplied LOD bias/limits; defaults are bias0, finest0, coarsest maxfloat |
| Simple texture allocation | kind2D, depth1, supplied format/target role, storage false |
| get_pixel/read | default mip0; get_pixel additionally selects layer0 |
| Single-target draw | default layer0/mip0, or supplied layer/mip |
| Per-vertex layout | implicit step0 on every supported element, no instance stream |

Simple filter0 selects point on all filters, filter1 selects linear on all filters, and filter2 selects linear plus the requested anisotropy. Anisotropy is validated in1..16 even when the filter does not use it; only filter2 applies the factor. Anisotropic support and the actual hardware limit use the existing context checks. Missing border colors, invalid enums, invalid numeric ranges and released handles still reject through canonical validation.

Every nonzero sampler applies its complete source-texture descriptor, including default bias0, finest0, maxfloat coarsest, point/linear filters and factor1 on contexts supporting anisotropy. It does not inherit conflicting source LOD/filter values. All previous values are restored after drawing. Sampler0 retains the caller's native parameters.

Border-aware constructors accept Clamp/Repeat/Mirror/Border independently on U/V/W. Ordinary constructors without a supplied color accept Clamp/Repeat/Mirror on each axis. Mixed-axis behavior and real2D/3D evidence are described in OPENGL_SAMPLER_AXES.md. Layouts retain the canonical float2 position and optional color/float2 UV, with the existing supported instance records on the advanced entry point. This bridge does not add other vertex formats.

The three native target helpers reuse shared subresource validation and target drawing. They each cross the diagnostics boundary once: an accepted submission adds one draw, a rejected submission adds none, and native input-state restoration is checked as with advanced draws. Target role, layer/mip bounds, viewport and framebuffer restoration follow OPENGL_TARGETS.md and OPENGL_TEXTURE_KINDS.md. No depth/stencil storage is synthesized.

The internal bytecode-reflection virtual function is D3D-specific and has no GL source caller. GL reflection uses the existing shader-handle metadata and uniform-layout paths described in OPENGL_UNIFORMS.md; a native D3D bytecode blob is not a GL shader-handle representation.

Validation is recorded in this batch's handover: a canonical sampler fixture control, literal sampler pixels/defaults, parameter restoration, native texture/read/layout, selected layer/mip targets, exact diagnostics and rejection cases, followed by every historical WGL suite and the formal DLL build.

Independent review regression evidence: out/native-review-red-run.log records real native/public failures with preseeded minLOD1, maxLOD0, bias1 and linear filters plus factor2. out/native-review-green-run.log verifies literal pixels, actual driver parameters during drawing, source restoration including anisotropy, sampler0 behavior and all historical/core suites after the complete-descriptor fix.
