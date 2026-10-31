# OpenGL multiple render targets

## Intent and constraints
Continue the existing GL backend in complete capability batches under the user's standing development/push/PR/merge authorization. Support the existing 1..4 target API with actual pixels, equal selected mip dimensions, duplicate-subresource rejection and exact caller-state restoration. Public API/version remains 0.5.0; no yy/yyp edits. Windows GameMaker DLL remains D3D11; exercise GL in the real WGL probe. Reuse the existing checkout/build cache and use a feature PR.

## Design
A focused targets module resolves strict numeric arrays, validates texture roles/subresources and vertex bounds, and temporarily binds only DRAW_FRAMEBUFFER. One target uses its existing FBO and retains its owned depth attachment and caller scissor rectangle. Multiple targets use a private color-only FBO with consecutive output locations. Invoke existing gl_draw after overflow-safe range/mapping checks so pipeline validation remains shared. Restore the draw FBO/viewport and delete temporary FBOs on every exit; read FBO, caller attachments and draw routing are preserved.

A focused indexed color-state helper saves every native draw-buffer slot when explicit blend descriptors use global GL setters. Texture initialization similarly preserves every indexed write mask. Shadow copying does not change blending or color masks in the current source, so it needs no unrelated edit. Optional MRT functions/actual context/limits gate the capability; missing MRT support preserves ordinary single-target drawing.

## Acceptance
GPU output for 2/3/4 distinct colors, selected compatible mips and mixed color formats; geometry samples counted once. Test descriptor blend/masks and every native indexed slot including slots above four. Compare separate read/draw bindings, attachments/routing, viewport, VAO/array buffer and indexed states around success and late failure. Reject malformed arrays, fractional/nonfinite/out-of-range numbers, duplicates, unknown/released/non-target textures, dimensions/mips/layers, invalid states, mapped inputs and overflowing draw ranges. Keep prior single-target depth/scissor and all previous texture/compute/uniform/instancing/query regressions passing. Complete capability-entrypoint gates, resource cleanup, DLL build and handover validation; one independent whole-batch review before PR merge.
