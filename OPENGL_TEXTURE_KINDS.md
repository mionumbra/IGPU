# OpenGL 3D, array and cube textures

The existing texture APIs now cover 2D, 3D, 2D array and cubemap resources in the GL probe. Public API/version remains **0.5.0**; the formal Windows GameMaker DLL remains D3D11.

| Kind | Native target | Depth argument | Mip layers | Ordinary sampler |
| --- | --- | --- | --- | --- |
| 0 | TEXTURE_2D | 1 | 1 | sampler2D |
| 1 | TEXTURE_3D | volume depth | shrinks each mip | sampler3D |
| 2 | TEXTURE_2D_ARRAY | array length | fixed | sampler2DArray |
| 3 | TEXTURE_CUBE_MAP | 6, square dimensions | six faces | samplerCube |

Cube face indices follow native order: +X, -X, +Y, -Y, +Z, -Z. A full 3D mip chain uses the largest of width, height and depth. Arrays and cubes use width/height for chain length. Native context dimension/layer limits are checked before allocation, alongside the existing public width/height bounds and format/storage rules. Every allocated subresource receives the existing initial color (zero RGB, alpha one).

## Subresource drawing and reading

The existing target/layer/mip lists may select individual 3D slices, array layers or cube faces. MRT can mix kinds or use different layers of the same texture when selected dimensions match. Duplicate identity is the exact texture/layer/mip. The 3D layer bound follows the selected mip depth. Array and cube layer counts stay fixed.

Layered draws use private color-only FBOs. Existing one-target 2D drawing keeps its owned depth attachment/scissor behavior. Caller read/draw FBOs, target routing, viewport and indexed blend/mask state follow the MRT contract in `OPENGL_TARGETS.md`.

RGBA8 pixel reads now select any valid subresource through a private read FBO. Mip/layer/pixel bounds are validated before dimension calculation. Reads preserve separate FBO bindings, PACK parameters and caller pixel-pack buffer bindings/bytes. Generating mips uses the native texture target and restores caller binding plus source base/max-level parameters.

## Sampling

Use the matching ordinary sampler type in raw GLSL. Existing U/V/filter/LOD/anisotropy settings apply to the native target, and W address mode is retained and applied/restored for non-2D resources. Three-dimensional clamp, repeat and mirror behavior is verified with positive and negative W coordinates. Cube sampling uses direction vectors; array sampling uses a layer coordinate.

Sampled drawing temporarily uses texture unit zero, preserving the caller's active unit and that unit's native-target binding. Native parameters are restored after the draw. Wrong sampler/resource kinds fail before drawing. Comparison sampling retains the existing 2D R16F/R32F `sampler2DShadow` contract; non-2D comparison is rejected.

## Compute writes

Non-2D storage images bind with `layered = true`, so the shader addresses the whole selected mip. Use `image3D`, `image2DArray` or `imageCube` matching the texture. A cube image uses integer face indices, while cube sampling uses directions. Existing dispatch slots, writable formats, barriers and image-state restoration stay shared.

The public dispatch layer list remains zero: shader coordinates choose the slice/face. A nonzero mip is accepted for a single texture output, as in the existing API. The probe verifies all slices/faces at base and nonbase mips and exact restoration of all six native image-binding fields.

## Capabilities and lifetime

Texture3D and TextureArray require the suitable actual context/limits plus optional TexImage3D and FramebufferTextureLayer entrypoints. Cubemaps use the existing 2D face operations and have their own context/size gate. Missing optional layered functions hide only 3D/array capability and creation; ordinary 2D/cube paths remain available. Release and shutdown delete every owned native texture, FBO and comparison mirror while preserving foreign bindings and the borrowed WGL context.

## Verification

Observed REDs: `out/layered-red-run.log` (old 2D-only creation); `out/layered-expanded-red-run.log` (layer MRT, typed samplers, compute layered writes and capabilities); `out/layered-w-red-run.log` (positive/negative W Repeat selected the clamp slice).

Final full WGL probe: `out/layered-complete-build.log` and `out/layered-complete-run.log`, ending PASS. Coverage includes zero initialization, selected mip isolation, cross-kind/same-texture-layer MRT, all six native cube directions, sixteen geometry samples, source mip generation, depth-driven chains, typed sampler rejection, active texture unit3, native limits, PACK/PBO preservation, optional gates, image fields and native release/shutdown/rebind, plus every prior GL suite. MSVC source compilation now uses `/MP8` in addition to CMake project parallelism.
