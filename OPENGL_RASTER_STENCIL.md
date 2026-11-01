# OpenGL raster and stencil states

The existing raster/depth APIs now cover solid/wireframe fill, depth clipping/clamping and stencil behavior on desktop GL. The Wireframe capability is enabled for the bound desktop GL backend. API/version remains0.5.0, with no new exports or generatedresource changes; the formal Windows GameMaker DLL remains D3D11.

## Raster state

Fill0 selects native FILL; fill1 selects LINE. Other values are rejected. Explicit descriptors apply the requested fill, cull and scissor settings and temporarily use the existing clockwise-front convention. The caller's polygon modes, front/cull settings, scissor enable and supported depth-clamp enable are restored after drawing. The scissor rectangle remains the caller's.

A depth_clip=true descriptor disables native depth clamp for its draw. depth_clip=false enables it and requires actual GL3.2 or ARB_depth_clamp support. Unsupported contexts reject that descriptor before creating a state object; they are never queried or modified with the unsupported clamp enum. State0 preserves the caller's native clip/clamp setting.

Polygon restoration handles compatibility contexts that have different front/back modes. Equal modes restore with FRONT_AND_BACK, which is also valid in core profiles. Separate face calls are used only when the saved native modes differ. This avoids invalid face arguments in core contexts.

## Stencil state

The depth descriptor's stencil flag, compare function, reference, read/write masks and fail/depth-fail/pass operations now apply during its draw. The supplied descriptor applies identically to front and back, matching the existing D3D descriptor contract. Both native faces are saved and restored independently, preserving asymmetric caller functions, references, masks and operations.

| Public operation | Native effect |
| --- | --- |
| 1 Keep | retain the stored value |
| 2 Zero | write zero |
| 3 Replace | write the reference |
| 4 Increment saturate | stop at the largest stored value |
| 5 Decrement saturate | stop at zero |
| 6 Invert | invert stored bits |
| 7 Increment wrap | wrap the largest value to zero |
| 8 Decrement wrap | wrap zero to the largest value |

All eight existing compare values and the existing reference/mask0..255 validation are retained. Stencil operates when depth testing is disabled; stencil fail and depth fail select their respective operations. An explicit descriptor with stencil disabled temporarily disables caller stencil testing; state0 retains caller behavior.

Stencil effects require storage in the current native DRAW framebuffer. This batch adds no automatic depth/stencil attachments or texture API. Existing color-only MRT/layered targets retain that storage contract. A single2D target retains a native depth/stencil attachment already supplied by the host. Tests attach a real borrowed packed D24S8 renderbuffer; color-only multi-target tests verify colors and complete caller-state restoration.

Ordinary, indexed, instanced, indirect, sampled and target drawing all reuse the shared save/apply/restore helpers. Invalid descriptors/handles and late blend failures leave caller state intact.

## Verification

out/raster-red-run.log records the old backend rejecting wireframe/depthclipfalse and stencil-enabled descriptors. out/raster-expanded-run.log verifies the complete historical probe and new real-state tests. The final combined4.1core evidence is out/raster-core-context-run.log, followed by the final integration run documented in the handover.

Coverage includes actual filled/wireframe pixel/sample differences, asymmetric polygon restoration, out-of-range Z clipping/clamping, state0 and caller enable restoration, all8 stencil operations and comparisons, saturate/wrap boundaries, read/write masks, stencil/depth-fail bytes, both triangle windings, advanced drawing, sampled/single-target/MRT state, and late failures. A real GL4.1core context also exercises polygon/clamp/asymmetric stencil restoration and reads stencil byte91, together with the diagnostic core-profile regressions. Every prior GL suite is retained. Formal DLL evidence: out/raster-dll-build.log.
