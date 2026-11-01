# GL mixed border sampler axes

Complete the existing per-axis sampler descriptor: a constructor supplied with a border color accepts each U/V/W axis independently as Clamp/Repeat/Mirror/Border. Store all requested modes and apply/restore border color when any axis is Border. Constructors without a color continue to reject Border; comparison plus Border stays unsupported. Keep native hardware anisotropy and existing numeric validation. No API/export/version0.5.0/generated-resource or yy/yyp changes; formal Windows DLL remains D3D11.

Reuse the canonical sampled path and existing real-GPU fixtures. Verify public full/native convenience paths, U/V-only border and clamp/repeat/mirror coexistence, 2D ignored-W semantics, real3D W border for positive/negative outside coordinates, linear border interpolation, mip and optional anisotropic cases, interior samples, invalid enums and exact native source/caller restoration. Do not invent array-layer wrapping or change native cube semantics. Existing default, all-border and comparison behavior stays covered by historical suites.

Standing continuous authorization covers this bounded implementation and featurePR/independent-review/approved-head merge. Prepare/test the whole batch; do not repeat user confirmation gates.
