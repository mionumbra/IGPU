# OpenGL multiple render targets

The GL probe implements the existing render-target array draw API for one through four 2D color targets. Public API/version remains **0.5.0**. The Windows GameMaker DLL remains D3D11.

## Output contract

The target array order selects consecutive fragment output locations:

```glsl
#version 330
layout(location = 0) out vec4 first;
layout(location = 1) out vec4 second;
void main() {
    first = vec4(1, 0, 0, 1);
    second = vec4(0, 1, 0, 1);
}
```

Passing two targets writes red to the first and green to the second. Locations 2 and 3 work the same way for three and four outputs. Use a compatible raw GLSL program; the backend does not rewrite shaders.

Target, layer and mip lists must have equal length from 1 to 4. Each texture must have the render-target role, each layer must be zero, and each mip must exist. Selected dimensions must match, for example an 8x8 texture at mip 1 and a 4x4 texture at mip 0. Repeating the same texture subresource is rejected. Numeric values must be finite, integral and in range before conversion.

All eight existing GL color formats may participate, including mixtures of RGBA8, R16F, R32F, RGBA4, R8, RG8, RGBA16F and RGBA32F. Native framebuffer completeness is checked before drawing. The existing public pixel-read helper supports RGBA8; the probe also validates mixed-format output using native float reads.

Negative vertex count infers the remaining vertices. Overflowing ranges, wrong-role/released buffers, mapped vertex buffers, bad layouts/states and missing programs fail with a diagnostic. Claimed graphics UBO validation is shared with ordinary drawing.

## Caller state and depth

A one-target draw uses that texture's existing framebuffer, retaining its owned depth attachment and the caller's scissor rectangle. Two through four targets use a temporary color-only framebuffer. This matches the existing color-only MRT behavior of the D3D11 path.

Only the draw framebuffer is temporarily bound. Separate read/draw bindings, caller framebuffer attachments and output routing, viewport, VAO and array-buffer binding survive success and failure. Temporary framebuffer names are deleted before returning, including late pipeline-validation and framebuffer-completeness failures.

An explicit blend descriptor applies its existing common settings to the outputs, then restores every native indexed blend enable, function, equation and color write mask, including hardware slots above the public four-output limit. With descriptor handle zero, the caller's indexed blend/mask settings remain active. Texture initialization also preserves indexed masks and separate read/draw framebuffer bindings.

## Capability boundary

`MultipleRenderTargets` and `max_render_targets = 4` require a suitable actual context, at least four color attachments and draw buffers, and all nine optional output/indexed-state functions. Indexed blend support requires desktop GL 4.0 or the relevant extension, in addition to the GL 3.0 framebuffer/indexed-mask facilities. A missing MRT function hides the multiple-target capability and rejects that path; existing one-target draws continue to work.

## Validation evidence

`out/targets-red-run.log` records the old one-target-only failure. `out/targets-state-red-run.log` records independent indexed blend/mask loss and separate framebuffer binding loss before their fixes.

`out/targets-complete-build.log` and `out/targets-complete-run.log` cover actual 2/3/4-output RGB/white pixels with sixteen geometry samples, selected mip isolation, eight mixed formats, strict wire values, range/mapping/role/lifetime errors, independent indexed states, caller framebuffer attachments/routing, native temporary-name deletion, forced allocation/incomplete/late-pipeline failures and all nine optional-entrypoint gates. Ordinary drawing, ordinary sampling and comparison mirrors after MRT retain the expected pixels and sixteen samples. The full probe includes all previous texture, buffer, compute/storage, uniform, instanced/indirect and query/sync/shutdown suites.
