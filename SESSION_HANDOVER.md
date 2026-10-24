# 会话交接 — 2026-10-24 — 版本 0.5.0 — OpenGL 钳制采样器上的线性放大和线性缩小可以同时生效，级间过滤保持最近点

> 先读这一节，再改代码。`HANDOVER.md` 是长文档，开头版本是 `0.5.0`。正文里 9 月的叙述已经过时。本文下半截仍保留 2026-09-23 审计记录。
>
> 比较采样的像素结果还在 Direct3D 11 上：参考值 0.5，左纹素 0.25、右纹素 0.75，小于比较是左黑右红，大于比较对调，线性比较在交界处是红 128。

## 下一会话接着做

**合并规则，先读这一段。** 功能分支必须推送到 `origin`。进入 `main` 只能通过 GitHub 拉取请求（pull request）：先推送分支，再开拉取请求，批准并合并。探针通过之后就做完这三步，不要留到下一会话。不要在本地 `merge`（包括 `--no-ff` 和快进）之后直接 `push origin main`。交接文档自己的修改也走拉取请求，不要直接推 `main`。已经用拉取请求合并过的旧分支不要再开一次。`9c56d05`（最近点采样）和 `84b40f5`（钳制与重复）是在这条规则写明之前直接推进 `main` 的，不要照做。

`gl-sample-raster` 随拉取请求 #18 进入 `main`，功能提交 `fbd3811`。`gl-depth` 已随拉取请求 #16 进入 `main`，合并提交 `b673d9a`。`gl-scissor` 从拉取请求 #14 之后的 `origin/main`（`47f7f2f`）拉出。`gl-cull` 从拉取请求 #13 之后的 `origin/main`（`61ac3ee`）拉出。`gl-blend` 从拉取请求 #11 之后的 `origin/main`（`19148c1`）拉出。混轴包裹随拉取请求 #11 进入 `main`，功能提交 `13f926a`，合并提交 `19148c1`。不要重做混轴包裹，也不要再为它开拉取请求。混合随拉取请求 #12 进入 `main`，功能提交 `8beb2a2`。采样绘制上的混合随拉取请求 #13 进入 `main`，功能提交 `ea013a7`。`igpu_draw_sampled` 接受混合句柄，只在这一次绘制里生效，返回前恢复。光栅剔除随拉取请求 #14 进入 `main`，功能提交 `410a6e8`。实心填充、剪裁关闭、深度裁剪打开时，正面是顺时针。`cull_clockwise` 丢掉顺时针，`cull_counterclockwise` 丢掉逆时针，`cull_noculling` 关掉剔除。逆时针三角形在 `cull_clockwise` 下是 `255 / 0`，在 `cull_counterclockwise` 下是 `0 / 0`，随后句柄 0 是 `255 / 0`。`cull_noculling` 下是 `255 / 0`。顺时针三角形在 `cull_clockwise` 下是 `0 / 0`，在 `cull_counterclockwise` 下是 `255 / 0`。索引绘制和画进纹理的逆时针加 `cull_counterclockwise` 是 `0 / 0`。句柄记在 CPU 上，和采样器共用编号，不创建 GL 对象。`igpu_draw`、`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 在这一次绘制里改剔除，返回前写回剔除开关、正面方向和剔除面。句柄 0 不改这三项。未知句柄和已经释放的句柄失败，错误含有 `unknown raster handle`，像素不变。负数句柄失败，错误含有 `not a state object`。线框失败，错误含有 `solid`。深度裁剪关闭失败，错误含有 `depth clip`。剪裁随拉取请求 #15 进入 `main`，功能提交 `16388e8`。矩形是调用方已经设好的，这次绘制不改矩形。探针把矩形设成 8×8 目标的左半边。打开剪裁时，满幅三角形是 `255 / 0`，随后句柄 0 是 `255 / 255`。索引绘制和画进纹理也是 `255 / 0`。同一句柄再加 `cull_counterclockwise` 仍是 `0 / 0`。句柄 0 不改剪裁开关：调用方已经打开时，句柄 0 仍是 `255 / 0`。剪裁关闭的光栅句柄在这一次绘制里关掉调用方打开的剪裁，返回前写回，满幅是 `255 / 255`，随后句柄 0 又是 `255 / 0`。`igpu_draw`、`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 都写回剪裁开关。未知句柄、已经释放的句柄仍失败，像素不变。`igpu_draw_sampled` 接受光栅句柄，只在这一次绘制里生效，返回前写回剔除开关、正面方向、剔除面和剪裁开关，不改剪裁矩形。句柄 0 不改这四项。逆时针加 `cull_clockwise` 是 `255 / 0`，加 `cull_counterclockwise` 是 `0 / 0`，随后句柄 0 是 `255 / 0`。顺时针加 `cull_clockwise` 是 `0 / 0`，加 `cull_counterclockwise` 是 `255 / 0`。调用方矩形的左半边把满幅采样收成 `255 / 0`，随后句柄 0 是 `255 / 255`。调用方已经打开剪裁时，句柄 0 仍是 `255 / 0`。剪裁关闭的句柄在这一次绘制里关掉它，满幅是 `255 / 255`，随后句柄 0 又是 `255 / 0`。同一支绘制可以同时带深度、混合和重复采样器：调用方打开大于比较、关掉深度写入和红色写入时，深度关闭、全通道写入、`cull_clockwise` 与 `u = 1.0625` 的重复写成 `255 / 255`，返回后写回；随后句柄 0 是 `16711680 / 16711680`。未知句柄和已经释放的句柄失败，错误含有 `unknown raster handle`，像素不变。负数句柄失败，错误含有 `not a state object`。`RasterState` 在这份 OpenGL 后端上为 true。`Wireframe` 仍只跟着 Direct3D。深度测试和深度写入随拉取请求 #16 进入 `main`。近处窗口深度是 0，远处是 0.5，深度清成 1。小于比较先近处红再远处绿是 `255 / 255`。写入关掉之后，小于比较且写入打开的远处绿是 `65280 / 65280`。深度测试关闭的远处绿是 `65280 / 65280`。句柄 0 的远处绿是 `65280 / 65280`。索引绘制和画进纹理的小于比较是 `255 / 255`。调用方打开测试、比较为大于、写入关掉时，这一次绘制写成 `255 / 255`，返回后写回这三项。未知句柄失败，错误含有 `unknown depth handle`。`igpu_draw_sampled` 接受深度句柄，随拉取请求 #17 进入 `main`，功能提交 `28546cf`。它只在这一次绘制里生效，返回前写回测试开关、比较函数和写入。小于比较先近处红再远处绿是 `255 / 255`。写入关掉之后，小于比较且写入打开的远处绿是 `65280 / 65280`。深度测试关闭的远处绿是 `65280 / 65280`。句柄 0 的远处绿是 `65280 / 65280`。调用方打开测试、比较为大于、写入关掉时，这一次绘制写成 `255 / 255`，返回后写回这三项。未知句柄和已经释放的句柄失败，错误含有 `unknown depth handle`。负数句柄失败，错误含有 `not a state object`。`igpu_draw_sampled` 接受光栅句柄，只在这一次绘制里生效，返回前写回剔除开关、正面方向、剔除面和剪裁开关，不改剪裁矩形。句柄 0 不改这四项。逆时针加 `cull_clockwise` 是 `255 / 0`，加 `cull_counterclockwise` 是 `0 / 0`，随后句柄 0 是 `255 / 0`。顺时针加 `cull_clockwise` 是 `0 / 0`，加 `cull_counterclockwise` 是 `255 / 0`。调用方矩形的左半边把满幅采样收成 `255 / 0`，随后句柄 0 是 `255 / 255`。调用方已经打开剪裁时，句柄 0 仍是 `255 / 0`。剪裁关闭的句柄在这一次绘制里关掉它，满幅是 `255 / 255`，随后句柄 0 又是 `255 / 0`。同一支绘制可以同时带深度、混合和重复采样器：调用方打开大于比较、关掉深度写入和红色写入时，深度关闭、全通道写入、`cull_clockwise` 与 `u = 1.0625` 的重复写成 `255 / 255`，返回后写回；随后句柄 0 是 `16711680 / 16711680`。未知句柄和已经释放的句柄失败，错误含有 `unknown raster handle`，像素不变。负数句柄失败，错误含有 `not a state object`。模板打开失败，错误含有 `stencil`。`DepthState` 在这份 OpenGL 后端上为 true。线框、关掉深度裁剪、模板、各向异性、比较，都先不要做。最近点多级随拉取请求 #19 进入 `main`。2×2 两级、导数 `0.125`：偏移 0 是 `255 / 255`，偏移 4 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。最细级数随拉取请求 #20 进入 `main`。最细 1、偏移 0 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。边框采样器上的最细随拉取请求 #28 进入 `main`，功能提交 `bd184ad`：最细 1、偏移 0 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。最粗级数随拉取请求 #21 进入 `main`。最粗 0、偏移 4 是 `255 / 255`，随后句柄 0 是 `255 / 255`，不限制的偏移 4 仍是 `65280 / 65280`。边框采样器上的最粗随拉取请求 #29 进入 `main`，功能提交 `0843f05`：最粗 0、偏移 4 是 `255 / 255`，随后句柄 0 是 `255 / 255`，随后不限制的偏移 4 是 `65280 / 65280`。级间线性随拉取请求 #22 进入 `main`。2×2 两级、`texture2DLod` 在 `(0.25, 0.5)`、级数 `0.5`：级间最近点是 `65280 / 65280`，级间线性是 `32896 / 32896`（红 128、绿 128），随后句柄 0 是 `65280 / 65280`。级间线性加级数偏移随拉取请求 #23 进入 `main`，功能提交 `4ab6a39`。同一条 `texture2DLod`、级数 `0.5`、偏移 `0.5` 是 `65280 / 65280`，随后句柄 0 是 `65280 / 65280`。最细 1 的级间线性随拉取请求 #24 进入 `main`，功能提交 `b212617`。同一条 `texture2DLod`、级数 `0.5` 是 `65280 / 65280`，紧接着不带最细的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。最粗 0 的级间线性随拉取请求 #25 进入 `main`，功能提交 `339f74d`。同一条 `texture2DLod`、级数 `0.5` 是 `255 / 255`，紧接着不带最粗的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。缩小为线性的级间线性随拉取请求 #26 进入 `main`，功能提交 `df55563`：2×2 两级，第 0 级左红右蓝、第 1 级全绿，`texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `2523225 / 2523225`（红 89、绿 128、蓝 38），随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性随拉取请求 #31 进入 `main`，功能提交 `b7b6c43`：同一张 2×2、`texture2DLod` 级数 `0.5` 是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性加偏移 `0.5` 随拉取请求 #32 进入 `main`，探针提交 `a12f325`：同一张 2×2、`texture2DLod` 级数 `0.5` 是 `65280 / 65280`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性加最细 1 随拉取请求 #33 进入 `main`，探针提交 `4d6cfd7`：同一张 2×2、`texture2DLod` 级数 `0.5` 是 `65280 / 65280`，紧接着不带最细的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性加最粗 0 随拉取请求 #34 进入 `main`，探针提交 `7526d1e`：同一张 2×2、`texture2DLod` 级数 `0.5` 是 `255 / 255`，紧接着不带最粗的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性加线性缩小随拉取请求 #35 进入 `main`，探针提交 `e363bae`：同一张 2×2、`texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `3371865 / 3371865`（红 89、绿 115、蓝 51），紧接着缩小为最近点的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。线性放大加级间线性、缩小为最近点随拉取请求 #36 进入 `main`，探针提交 `5fba67e`：同一张 2×2 两级，左边导数 `0.001` 是 `5046450`（红 178、蓝 77），右边 `texture2DLod` 级数 `0.5` 是 `32896 / 32896`。随后句柄 0 是 `255 / 65280`。边框采样器上的线性放大加级间线性、缩小为最近点随拉取请求 #37 进入 `main`，探针提交 `a582255`：同一张 2×2 两级，绿色边框，左边 `u = 1.0`、导数 `0.001` 是 `8421376`，右边 `texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `32896`。随后句柄 0 是 `16711680 / 65280`。再绑回 `u = 0.4` 的着色器，句柄 0 是 `255 / 65280`。钳制采样器上的线性放大、线性缩小和级间线性随拉取请求 #38 进入 `main`，探针提交 `3a7162c`：同一张 2×2 两级，左边导数 `0.001` 是 `5046450`，右边 `texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `2523225`。随后句柄 0 是 `255 / 65280`。边框采样器上的级数偏移随拉取请求 #27 进入 `main`，功能提交 `b438883`：同一张 2×2、导数 `0.125`、偏移 4 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。边框采样器上的最细级数随拉取请求 #28 进入 `main`，功能提交 `bd184ad`：同一张 2×2、导数 `0.125`、偏移 0、最细 1 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。最粗 0、偏移 4 是 `255 / 255`，随后句柄 0 是 `255 / 255`，随后不限制的偏移 4 是 `65280 / 65280`。边框采样器上的线性缩小随拉取请求 #30 进入 `main`，功能提交 `47f1a4f`：同一张 2×2、`(0.4, 0.5)`，放大最近点、缩小线性是 `255 / 5046450`，随后句柄 0 是 `255 / 255`。级间线性随拉取请求 #31 进入 `main`，功能提交 `b7b6c43`：级数 `0.5` 是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性加偏移 `0.5` 随拉取请求 #32 进入 `main`，探针提交 `a12f325`：同一张 2×2、`texture2DLod` 级数 `0.5` 是 `65280 / 65280`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性加最细 1 随拉取请求 #33 进入 `main`，探针提交 `4d6cfd7`：同一张 2×2、`texture2DLod` 级数 `0.5` 是 `65280 / 65280`，紧接着不带最细的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性加最粗 0 随拉取请求 #34 进入 `main`，探针提交 `7526d1e`：同一张 2×2、`texture2DLod` 级数 `0.5` 是 `255 / 255`，紧接着不带最粗的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性加线性缩小随拉取请求 #35 进入 `main`，探针提交 `e363bae`：同一张 2×2、`texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `3371865 / 3371865`（红 89、绿 115、蓝 51），紧接着缩小为最近点的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。线性放大加级间线性、缩小为最近点随拉取请求 #36 进入 `main`，探针提交 `5fba67e`：同一张 2×2 两级，左边导数 `0.001` 是 `5046450`（红 178、蓝 77），右边 `texture2DLod` 级数 `0.5` 是 `32896 / 32896`。随后句柄 0 是 `255 / 65280`。边框采样器上的线性放大加级间线性、缩小为最近点随拉取请求 #37 进入 `main`，探针提交 `a582255`：同一张 2×2 两级，绿色边框，左边 `u = 1.0`、导数 `0.001` 是 `8421376`，右边 `texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `32896`。随后句柄 0 是 `16711680 / 65280`。再绑回 `u = 0.4` 的着色器，句柄 0 是 `255 / 65280`。钳制采样器上的线性放大、线性缩小和级间线性随拉取请求 #38 进入 `main`，探针提交 `3a7162c`：同一张 2×2 两级，左边导数 `0.001` 是 `5046450`，右边 `texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `2523225`。随后句柄 0 是 `255 / 65280`。边框采样器上的线性放大、线性缩小和级间线性随拉取请求 #39 进入 `main`，探针提交 `8988c06`：绿色边框，左边 `u = 1.0`、导数 `0.001` 是 `8421376`，右边 `texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `2523225`。随后句柄 0 是 `16711680 / 65280`。再绑回 `u = 0.4` 的着色器，句柄 0 是 `255 / 65280`。钳制采样器上的线性放大和线性缩小可以同时生效，级间过滤保持最近点，探针提交 `e3d0a20`：同一张没有多级的 2×2，采样点 `(0.4, 0.5)`，左边导数 `0.001`、右边导数 `1.0` 都是 `5046450`。随后句柄 0 是 `255 / 255`。再下一刀还没定。这份交接自己的提交以 `git rev-parse --short HEAD` 为准。

计划在 `superpowers/plans/2026-10-10-gl-address.md`。更早的最近点采样计划在 `superpowers/plans/2026-10-10-gl-sample.md`。两份都已经证明，不要重做：

最近点纹理采样只在探针里。`igpu_draw_sampled` 把 `g_textures` 里的 rgba8 绑到当前二维纹理，把当前程序的 `uniform sampler2D igpu_tex` 设为 0，画进调用方已经 current 的帧缓冲。不清屏，不改帧缓冲、视口和程序。返回前恢复 `GL_TEXTURE_BINDING_2D`。过滤是创建时设下的最近点，包裹是钳制到边缘。不创建采样器对象，不调用 `glActiveTexture`。属性 0 是 `in_pos`，属性 1 在采样程序里是 `in_uv`。位置加纹理坐标的缓冲步长必须显式写成 16。步长 0 仍是位置缓冲。

源纹理左红 `255`、右蓝 `16711680`。采样四边形的前 3 个顶点只让目标左边变成 `255`，右边保持 `0`。从第 3 个顶点、`vertex_count = -1` 再画，右边变成 `16711680`。常数 `u = 0.484375` 的全屏四边形两边都是 `255`。源纹理在这些绘制之后不变。

被采样的纹理若是当前颜色附件，调用失败，错误含有 `the texture is the current color target`，源像素不变。没有 `igpu_tex` 的程序失败，错误含有 `no sampler uniform 'igpu_tex'`，目标保持 `0`。句柄 0 仍是那次最近点采样。纹理句柄 0 失败，错误含有 `unknown texture handle`。`igpu_buffer_create(96, 0, 1, 0)` 仍是步长 8，配上纹理坐标布局失败，错误含有 `stride does not match`。

最近点采样器只在探针里，已经进入 `main`。`igpu_sampler_state_create` 接受三轴相同的钳制或重复。放大和缩小各自可以是最近点或线性。级间过滤可以是线性，缩小可以是最近点或线性。最细可以大于 0。最粗可以低于不限制的哨兵。级数偏移可以不是 0。缩小为线性的级间线性读回 `2523225 / 2523225`。各向异性是 1，比较是 0。句柄记在 CPU 上，不创建 GL 采样器对象。`igpu_draw_sampled` 在这一次绘制里把源纹理的 S、T 包裹设成对应值。放大为线性时，把 `GL_TEXTURE_MAG_FILTER` 设成 `GL_LINEAR`。没有多级时，缩小为线性把 `GL_TEXTURE_MIN_FILTER` 设成 `GL_LINEAR`。级间线性再加线性缩小设成 `GL_LINEAR_MIPMAP_LINEAR`。返回前写回绘制前的包裹、放大过滤和缩小过滤。句柄 0 不改包裹，也不改过滤。不调用 `glActiveTexture`，不调用 `glGenerateMipmap`。

`u = 1.0625` 在重复下两边都是红 `255`，在钳制采样器下两边都是蓝 `16711680`。随后句柄 0 仍是蓝，重复没有留下来。`u = -0.0625` 在重复下是蓝，在钳制下是红。句柄 0、`u = 0.484375` 两边仍是红 `255`。

`u = 0.5` 在线性放大、钳制下两边都是 `8388736`（红 128、蓝 128）。`u = 1.5` 在线性放大加重复下也是 `8388736`。同一个 `u = 1.5` 在线性放大加钳制下是边缘纹素的蓝 `16711680`。随后句柄 0、`u = 0.484375` 两边仍是红 `255`，`u = 1.0625` 两边仍是蓝 `16711680`。线性过滤和重复都没有留下来。

级间线性随拉取请求 #22 进入 `main`，级数 `0.5` 是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。偏移 `0.5` 的级间线性随拉取请求 #23 进入 `main`，功能提交 `4ab6a39`，读回 `65280 / 65280`。最细 1 的级间线性读回 `65280 / 65280`，紧接着不带最细的级间线性是 `32896 / 32896`。最粗 0 的级间线性是 `255 / 255`，紧接着不带最粗的级间线性是 `32896 / 32896`。缩小为线性的级间线性随拉取请求 #26 进入 `main`，功能提交 `df55563`：2×2 两级，第 0 级左红右蓝、第 1 级全绿，`texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `2523225 / 2523225`（红 89、绿 128、蓝 38），随后句柄 0 是 `65280 / 65280`。边框采样器上的线性缩小随拉取请求 #30 进入 `main`，功能提交 `47f1a4f`：放大最近点、缩小线性是 `255 / 5046450`，随后句柄 0 是 `255 / 255`。非边框的线性缩小在探针里。源纹理是 2×2，左列红 `255`，右列蓝 `16711680`。探针着色器是 `#version 120`，启用 `GL_ARB_shader_texture_lod`，用 `texture2DGradARB` 在 `(0.4, 0.5)` 采样。左边导数 `0.001` 走放大，右边导数 `1.0` 走缩小。放大最近点、缩小线性时读回 `255 / 5046450`（红 178、蓝 77）。放大线性、缩小最近点时读回 `5046450 / 255`。随后句柄 0 两边都是 `255`。钳制、重复和镜像可以按轴不同。横纹上 U 重复、V 钳制、W 镜像、`u = 1.0625` 是红 `255`。竖纹下红上蓝，V 重复、`v = 1.0625` 是红 `255`。U 镜像、`u = 1.6` 是红 `255`。V 钳制、`v = -0.625` 是红 `255`。W 重复不改变 T。线性放大加 U 镜像、`u = 1.5` 是 `8388736`。随后句柄 0 在竖纹 `v = 1.0625` 是蓝 `16711680`，在横纹 `u = 1.6` 是蓝 `16711680`，在 `u = 0.484375` 是红 `255`。轴值不是这三种时失败，错误含有 `clamp, repeat, or mirror`。镜像在探针里。`u = 1.6` 的最近点镜像是红 `255`，同一条 `u` 上的重复和钳制是蓝 `16711680`。`u = -0.625` 的镜像是蓝 `16711680`。线性放大加镜像、`u = 1.5` 是 `8388736`。随后句柄 0 在 `u = 1.6` 仍是蓝 `16711680`，在 `u = 0.484375` 仍是红 `255`。三轴不全是边框时失败，错误含有 `border on every axis`。边框寻址在探针里。`u = 1.6` 的绿色边框是 `65280`，`u = -0.0625` 也是 `65280`。同一条 `u = 1.6` 上，红色边框是 `255`，重复和钳制是 `16711680`。线性放大加绿色边框、`u = 1.0` 是 `8421376`。随后句柄 0 在 `u = 1.6` 仍是 `16711680`，在 `u = 0.484375` 仍是 `255`。比较和各向异性大于 1 仍失败，错误含有 `does not implement`。级数偏移在探针里。2×2 源有两级，第 0 级全红 `255`，第 1 级全绿 `65280`。导数 `0.125` 的 `texture2DGradARB`：偏移 0 是 `255 / 255`，偏移 4 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。最细级数随拉取请求 #20 进入 `main`。最细 1、偏移 0 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。边框采样器上的最细随拉取请求 #28 进入 `main`，功能提交 `bd184ad`：最细 1、偏移 0 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。最粗级数随拉取请求 #21 进入 `main`。最粗 0、偏移 4 是 `255 / 255`，随后句柄 0 是 `255 / 255`，不限制的偏移 4 仍是 `65280 / 65280`。边框采样器上的最粗随拉取请求 #29 进入 `main`，功能提交 `0843f05`：最粗 0、偏移 4 是 `255 / 255`，随后句柄 0 是 `255 / 255`，随后不限制的偏移 4 是 `65280 / 65280`。边框采样器上的级数偏移随拉取请求 #27 进入 `main`，功能提交 `b438883`：同一张 2×2、导数 `0.125`、偏移 4 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。未知句柄和已经释放的句柄失败，错误含有 `unknown sampler handle`，像素不变。`igpu_draw`、`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 拿到活着的采样器仍失败，错误含有 `draws with no extra state`。被采样的纹理若是当前颜色附件，镜像采样器和边框采样器也同样失败。`SamplerState` 在这份 OpenGL 后端上为 true。Windows 的 GameMaker DLL 仍没有 OpenGL 后端，那里 `SamplerState` 仍只跟着 Direct3D。

镜像寻址随拉取请求 #8 进入 `main`。边框寻址随拉取请求 #9 进入 `main`，功能提交 `38687b8`。线性缩小随拉取请求 #10 进入 `main`，功能提交 `8fe079a`，合并提交 `fb136fd`。混轴包裹随拉取请求 #11 进入 `main`，功能提交 `13f926a`，合并提交 `19148c1`。进入 `main` 只能通过拉取请求。

混合随拉取请求 #12 进入 `main`，功能提交 `8beb2a2`。`igpu_blend_state_create` 接受 `bm_zero` 到 `bm_src_alpha_sat`（1–11），方程接受 `bm_eq_*`（1–5）。颜色因子放到 alpha 槽时，和 Direct3D 一样改成对应的 alpha 因子。句柄记在 CPU 上，和采样器共用编号，不创建 GL 对象。`igpu_draw`、`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 在这一次绘制里按记录打开或关掉混合，设置分开的因子和方程，并套上四个通道的写掩码，返回前写回。句柄 0 不改混合开关、因子、方程和写掩码。未知句柄和已经释放的句柄失败，错误含有 `unknown blend handle`，像素不变。负数句柄失败，错误含有 `not a state object`。当时深度句柄仍必须是 0。未知光栅句柄失败，错误含有 `unknown raster handle`。`igpu_draw_sampled` 和上面三支绘制一样套用混合，返回前写回。采样器可以同时非 0，包裹和过滤也写回。三角带上的假句柄 1 现在失败，错误含有 `unknown blend handle`。采样绘制上的假句柄 1 也失败，错误含有 `unknown blend handle`。`BlendState` 在这份 OpenGL 后端上为 true。Windows 的 GameMaker DLL 仍没有 OpenGL 后端，那里 `BlendState` 仍只跟着 Direct3D。

不透明蓝是 `16711680`。src-alpha（因子 5、6，相加），红且 alpha 0 仍是 `16711680`。同一混合，红且 alpha 255 是 `255`。随后句柄 0、蓝且 alpha 0 是 `16711680`，混合开关回到关闭。源红、目标蓝、因子都是 one、方程 max 是 `16711935`。随后句柄 0、红且 alpha 0 是 `255`。索引绘制和画进纹理的 alpha 0 src-alpha 都保持 `16711680`。混合关闭且红通道不写，黑底上画红是 `0`。随后句柄 0 画红是 `255`。采样绘制的源纹理 alpha 由采样着色器原样写出。目标先是不透明蓝 `16711680`。src-alpha 下，alpha 0 的红源仍是 `16711680`，alpha 255 的红源写成 `255`。随后句柄 0 再采样 alpha 0 的红源，写成 `255`，混合开关回到关闭。同一支采样绘制可以同时带重复采样器：`u = 1.0625` 是红 `255`。随后句柄 0 在同一条 `u` 上是蓝 `16711680`，重复没有留下来。未知句柄、负数句柄和已经释放的句柄失败，像素不变。

深度测试和深度写入随拉取请求 #16 进入 `main`。近处窗口深度是 0，远处是 0.5，深度清成 1。小于比较先近处红再远处绿是 `255 / 255`。写入关掉之后，小于比较且写入打开的远处绿是 `65280 / 65280`。深度测试关闭的远处绿是 `65280 / 65280`。句柄 0 的远处绿是 `65280 / 65280`。索引绘制和画进纹理的小于比较是 `255 / 255`。调用方打开测试、比较为大于、写入关掉时，这一次绘制写成 `255 / 255`，返回后写回这三项。未知句柄失败，错误含有 `unknown depth handle`。`igpu_draw_sampled` 接受深度句柄，随拉取请求 #17 进入 `main`，功能提交 `28546cf`。它只在这一次绘制里生效，返回前写回测试开关、比较函数和写入。小于比较先近处红再远处绿是 `255 / 255`。写入关掉之后，小于比较且写入打开的远处绿是 `65280 / 65280`。深度测试关闭的远处绿是 `65280 / 65280`。句柄 0 的远处绿是 `65280 / 65280`。调用方打开测试、比较为大于、写入关掉时，这一次绘制写成 `255 / 255`，返回后写回这三项。未知句柄和已经释放的句柄失败，错误含有 `unknown depth handle`。负数句柄失败，错误含有 `not a state object`。`igpu_draw_sampled` 接受光栅句柄，只在这一次绘制里生效，返回前写回剔除开关、正面方向、剔除面和剪裁开关，不改剪裁矩形。句柄 0 不改这四项。逆时针加 `cull_clockwise` 是 `255 / 0`，加 `cull_counterclockwise` 是 `0 / 0`，随后句柄 0 是 `255 / 0`。顺时针加 `cull_clockwise` 是 `0 / 0`，加 `cull_counterclockwise` 是 `255 / 0`。调用方矩形的左半边把满幅采样收成 `255 / 0`，随后句柄 0 是 `255 / 255`。调用方已经打开剪裁时，句柄 0 仍是 `255 / 0`。剪裁关闭的句柄在这一次绘制里关掉它，满幅是 `255 / 255`，随后句柄 0 又是 `255 / 0`。同一支绘制可以同时带深度、混合和重复采样器：调用方打开大于比较、关掉深度写入和红色写入时，深度关闭、全通道写入、`cull_clockwise` 与 `u = 1.0625` 的重复写成 `255 / 255`，返回后写回；随后句柄 0 是 `16711680 / 16711680`。未知句柄和已经释放的句柄失败，错误含有 `unknown raster handle`，像素不变。负数句柄失败，错误含有 `not a state object`。模板打开失败，错误含有 `stencil`。`DepthState` 在这份 OpenGL 后端上为 true。线框、关掉深度裁剪、模板、各向异性、比较，都先不要做。ES 2 和 `glsl_es` 的成功编译也还不要做。边框采样器上的线性放大加级间线性随拉取请求 #37 进入 `main`，探针提交 `a582255`。钳制采样器上的线性放大、线性缩小和级间线性随拉取请求 #38 进入 `main`，探针提交 `3a7162c`：同一张 2×2 两级，左边导数 `0.001` 是 `5046450`，右边 `texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `2523225`。随后句柄 0 是 `255 / 65280`。边框采样器上的线性放大、线性缩小和级间线性随拉取请求 #39 进入 `main`，探针提交 `8988c06`：绿色边框，左边 `u = 1.0`、导数 `0.001` 是 `8421376`，右边 `texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 是 `2523225`。随后句柄 0 是 `16711680 / 65280`。再绑回 `u = 0.4` 的着色器，句柄 0 是 `255 / 65280`。钳制采样器上的线性放大和线性缩小可以同时生效，级间过滤保持最近点，探针提交 `e3d0a20`：同一张没有多级的 2×2，采样点 `(0.4, 0.5)`，左边导数 `0.001`、右边导数 `1.0` 都是 `5046450`。随后句柄 0 是 `255 / 255`。再下一刀还没定。那些平台上的 GameMaker 运行这台机器证明不了。纹理单元保持调用方原来的值；探针的默认单元是 0。

更早的 OpenGL 也在 `main` 上，不要重做，也不要再开拉取请求。拉取请求 https://github.com/mionumbra/IGPU/pull/5 的合并提交是 `332e1a8`，只写了「必须走拉取请求」这条规则。拉取请求 https://github.com/mionumbra/IGPU/pull/4 的合并提交是 `83afeb8`，图元 1、2、3、5。`gl-primitives` 尖端 `3e0a1f2` 已经包含在里面。拉取请求 https://github.com/mionumbra/IGPU/pull/3 的合并提交是 `0885e67`，顶点颜色。`gl-vertex-colour` 尖端 `7c0c6e4`。拉取请求 https://github.com/mionumbra/IGPU/pull/2 的合并提交是 `edb0ccd`，非索引 `igpu_draw`。`gl-draw` 尖端 `31c6982`。`gl-bind-current` 尖端 `fb4a9a5` 已经包含在 `main` 里。

顶点颜色只在探针里。布局是 float2 位置再加一个 colour，缓冲步长必须显式写成 12。布局步长传 0 时内部记成 12，缓冲仍要 12。`in_pos` 在属性位置 0，`in_colour` 在属性位置 1。字节序是 R、G、B、A，不交换红蓝。左半边读回红 `255`，右半边是蓝 `16711680`。这一笔探针画的是 12 个顶点；只画前 6 个会让右边保持清屏黑。alpha 0 仍写出红，这次绘制不打开混合。随后用 float2 布局再画，两个采样点是 `0`，颜色属性已经关掉。`igpu_draw_to_render_targets` 从第 6 个顶点起画时，右半边是蓝，左半边保持清屏黑。Windows 的 GameMaker DLL 仍没有 OpenGL 后端。

图元 1、2、3、5 只在探针里。点列表放在纹素中心，点大小和线宽保持 1，邻纹素是 `0`。线列表两段之间的 `(3,4)` 是 `0`，`(1,4)` 和 `(6,4)` 是 `255`。线带先画一段，右边保持 `0`，再从下一个顶点画，右边变成 `255`。索引线带一次画 3 个顶点，左右都是 `255`；若被当成 `GL_LINES`，第三顶点会被丢下，右边保持 `0`。三角带的采样点在第二条三角形里：四顶点若被当成 `GL_TRIANGLES`，左边保持 `0`。实际读回是左 `255`、右 `0`，随后右边也是 `255`。图元 0 和 7 拒绝，错误含有 `does not draw this primitive`。图元 6 仍拒绝，错误含有 `trianglefan`。深度和采样器句柄仍拒绝，错误含有 `draws with no extra state`，像素不变。未知光栅句柄拒绝，错误含有 `unknown raster handle`。未知混合句柄拒绝，错误含有 `unknown blend handle`。`igpu_draw` 和 `igpu_draw_indexed` 不改帧缓冲、视口和程序。`igpu_draw_to_render_targets` 会恢复帧缓冲和视口。Windows 的 GameMaker DLL 仍没有 OpenGL 后端。

已知的小缺口，不是下一刀：未知布局句柄的错误写成「the vertex buffer stride does not match the input layout」。调用失败，像素不变。`first_index + index_count` 和 `first_vertex + vertex_count` 在特别大的正数上可能回绕，再被收成 `GLsizei`。Direct3D 有同样的写法。探针里的 13 个索引和 12 个顶点不会走到这里。非索引线带的两段各是 2 个顶点，`GL_LINES` 和 `GL_LINE_STRIP` 对这两段画出来一样。区分它们的是索引路径上一次画 3 个顶点。不要为了这个再开一刀。

已经落地的 OpenGL 只在探针里，Windows 的 GameMaker DLL 不含它：

- `igpu_bind_current()` 绑定调用线程上已经 current 的上下文，不创建、不销毁。Windows 这份 GameMaker 构建里它失败，错误是 `igpu_bind_current: this build has no OpenGL backend`。已经 `igpu_init` 的 D3D11 设备不会被这次失败丢掉。
- 探针在这块 AMD Radeon Vega 8 上画出 8×8 纹理，读回 `255 / 16711680 / 16711680`（左上红，右上蓝，左下蓝）。
- 静态 16 位索引缓冲可以创建。`IndexBuffer` 和 `DrawIndexed` 在探针上为 true，写法是 `native || opengl_backend()`。前 6 个索引只把左半边画成红 `255`，右半边保持蓝 `16711680`；从索引 6 起、`index_count = -1` 再把右半边画成红。`igpu_draw_indexed` 画进当前帧缓冲，不清屏。探针用 `gl_color_target_begin` 把 IGPU 纹理绑成当前目标。
- `igpu_draw_to_render_targets` 拒绝 `bind != 1` 的缓冲。索引缓冲的 stride 是 0，不拒绝的话范围检查会放行，`glDrawArrays` 会读过那 24 个字节。

非索引的 `igpu_draw` 已在探针里证明。`GlBackend::draw` 转到 `gl_draw`。它画进调用方已经 current 的帧缓冲，不清屏，不改帧缓冲绑定和视口，也不调用 `UseProgram`。三角形列表的前 6 个顶点只把左半边画成红 `255`，右半边保持清屏黑 `0`。从顶点 6 起、`vertex_count = -1` 再把右半边画成红，左半边仍是红。图元 `6` 和任何一个非 0 的状态句柄都被拒绝，像素保持 `0`。索引缓冲不能当成顶点缓冲。

Git Bash 在 `D:\Program Files\Git`。`bash.exe` 是 `D:\Program Files\Git\bin\bash.exe`，PowerShell 的 `PATH` 里没有它。需要 bash 时用这个路径，不要当成没安装。

`git fetch`、`git push`、`git pull` 和 `gh` 在这台机器上必须走系统代理。Git 不读 Windows 的系统代理。每次从 `HKCU:\Software\Microsoft\Windows\CurrentVersion\Internet Settings` 读取 `ProxyEnable` 和 `ProxyServer`。代理没开或没有地址时停下来说明，不要裸连。只在这一次命令上加 `git -c http.proxy=http://<地址>`，并给这次进程设 `HTTP_PROXY` 和 `HTTPS_PROXY`。不要写进 `git config`，端口会变。2026-10-12 读到 `ProxyEnable = 1`，`ProxyServer = 127.0.0.1:7897`。

动手前先经系统代理在 `main` 上拉取，再跑下面三件事，确认树还是绿的：

```
pwsh -File tools\verify_handover.ps1
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

探针退出码 0，并打印这一百七十行和 `PASS`：

```
pixels       : 255 / 16711680 / 16711680
indexed pixels: 255 / 16711680
indexed tail  : 255 / 255
draw pixels   : 255 / 0
draw tail     : 255 / 255
colour pixels  : 255 / 16711680
colour tail    : 0 / 0
colour alpha   : 255
colour indexed : 255 / 16711680
colour target  : 0 / 16711680
strip pixels  : 255 / 0
strip tail    : 255 / 255
strip indexed : 255 / 0
strip index tail: 255 / 255
strip target  : 255 / 0
point pixels  : 255 / 0
point tail    : 255 / 255
point indexed : 255 / 0
line pixels   : 255 / 0 / 255
line strip    : 255 / 0
line strip tail: 255 / 255
line indexed  : 255 / 255
line target   : 255 / 0
sample source : 255 / 16711680
sample pixels : 255 / 0
sample tail   : 255 / 16711680
sample nearest: 255 / 255
wrap repeat  : 255 / 255
wrap clamp   : 16711680 / 16711680
wrap restored: 16711680 / 16711680
wrap negative: 16711680 / 16711680
wrap edge    : 255 / 255
wrap nearest : 255 / 255
linear seam  : 8388736 / 8388736
linear repeat: 8388736 / 8388736
linear edge  : 16711680 / 16711680
linear point : 255 / 255
linear wrap  : 16711680 / 16711680
mirror nearest: 255 / 255
mirror repeat : 16711680 / 16711680
mirror clamp  : 16711680 / 16711680
mirror negative: 16711680 / 16711680
mirror point  : 255 / 255
mirror linear : 8388736 / 8388736
mirror restore: 16711680 / 16711680
mirror plain  : 255 / 255
axes repeat  : 255 / 255
axes tall    : 255 / 255
axes mirror  : 255 / 255
axes low     : 255 / 255
axes wide    : 255 / 255
axes linear  : 8388736 / 8388736
axes restore : 16711680 / 16711680
axes wrap    : 16711680 / 16711680
axes plain   : 255 / 255
border outside: 65280 / 65280
border negative: 65280 / 65280
border red    : 255 / 255
border clamp  : 16711680 / 16711680
border repeat : 16711680 / 16711680
border linear : 8421376 / 8421376
border restore: 16711680 / 16711680
border plain  : 255 / 255
minify linear : 255 / 5046450
minify swap   : 5046450 / 255
minify plain  : 255 / 255
minify both   : 5046450 / 5046450
minify both0  : 255 / 255
border min   : 255 / 5046450
border min0  : 255 / 255
lod hold     : 255 / 255
lod coarse   : 65280 / 65280
lod plain    : 255 / 255
lod finest   : 65280 / 65280
lod fine0    : 255 / 255
lod cap      : 255 / 255
lod cap0     : 255 / 255
lod uncap    : 65280 / 65280
border lod   : 65280 / 65280
border lod0  : 255 / 255
border fine  : 65280 / 65280
border fine0 : 255 / 255
border cap   : 255 / 255
border cap0  : 255 / 255
border uncap : 65280 / 65280
mip point    : 65280 / 65280
mip fine     : 65280 / 65280
mip cap      : 255 / 255
mip linear   : 32896 / 32896
mip bias     : 65280 / 65280
mip plain    : 65280 / 65280
border mip   : 32896 / 32896
border mip0  : 65280 / 65280
border mbias : 65280 / 65280
border mbias0: 65280 / 65280
border mfine : 65280 / 65280
border mfree : 32896 / 32896
border mfine0: 65280 / 65280
border mcap  : 255 / 255
border mopen : 32896 / 32896
border mcap0 : 65280 / 65280
mip tri      : 2523225 / 2523225
border mtri  : 3371865 / 3371865
border mnear : 32896 / 32896
border mtri0 : 65280 / 65280
mag mip      : 5046450 / 32896
mag mip0     : 255 / 65280
tri mag      : 5046450 / 2523225
tri mag0     : 255 / 65280
border magm  : 8421376 / 32896
border mag0  : 16711680 / 65280
border mag1  : 255 / 65280
border trim  : 8421376 / 2523225
border trim0 : 16711680 / 65280
border trim1 : 255 / 65280
blend base    : 16711680 / 16711680
blend hold    : 16711680 / 16711680
blend replace : 255 / 255
blend plain   : 16711680 / 16711680
blend max     : 16711935 / 16711935
blend max off : 255 / 255
blend indexed : 16711680 / 16711680
blend target  : 16711680 / 16711680
blend mask    : 0 / 0
blend unmask  : 255 / 255
sample blend0 : 16711680 / 16711680
sample blend1 : 255 / 255
sample plain  : 255 / 255
sample repeat : 255 / 255
sample clamped: 16711680 / 16711680
cull keep    : 255 / 0
cull drop    : 0 / 0
cull plain   : 255 / 0
cull none    : 255 / 0
cull front   : 0 / 0
cull back    : 255 / 0
cull indexed : 0 / 0
cull target  : 0 / 0
scissor clip  : 255 / 0
scissor plain : 255 / 255
scissor index : 255 / 0
scissor target: 255 / 0
scissor drop  : 0 / 0
scissor held  : 255 / 0
scissor widen : 255 / 255
scissor back  : 255 / 0
depth less   : 255 / 255
depth write  : 65280 / 65280
depth off    : 65280 / 65280
depth plain  : 65280 / 65280
depth index  : 255 / 255
depth target : 255 / 255
depth held   : 255 / 255
sample depth : 255 / 255
sample dwrite: 65280 / 65280
sample doff  : 65280 / 65280
sample d0    : 65280 / 65280
sample dheld : 255 / 255
sample ckeep : 255 / 0
sample cdrop : 0 / 0
sample c0    : 255 / 0
sample cfront: 0 / 0
sample cback : 255 / 0
sample clip  : 255 / 0
sample s0    : 255 / 255
sample sheld : 255 / 0
sample swide : 255 / 255
sample sback : 255 / 0
sample crep  : 255 / 255
sample cr0   : 16711680 / 16711680
```

可执行文件在 `src\Release` 下，不在预设目录的 `Release` 根上。

改了 DLL 才会再跑 GameMaker。`gm-cli`（`compile`、`run`、`manual`，以及 `npx @gamemaker/gm-cli`）在这台机器上一般必须走系统代理才能用。这是平时的用法。Node 不读 Windows 的系统代理。每次调用前从注册表取出当前代理，写进这次进程，再用 `node --use-env-proxy`。代理没开或没有地址时停下来说明，不要裸跑。端口以注册表为准。2026-10-12 读到 `ProxyEnable = 1`，`ProxyServer = 127.0.0.1:7897`。Git 和 `gh` 用同一条代理，做法在上面「Git Bash」后面那一节，不要裸连。

```pwsh
$s = Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Internet Settings'
if ($s.ProxyEnable -ne 1 -or -not $s.ProxyServer) { throw 'system proxy is off; gm-cli on this machine needs it' }
$proxy = [string]$s.ProxyServer
if ($proxy -notmatch '://') { $proxy = "http://$proxy" }
$env:HTTP_PROXY = $proxy
$env:HTTPS_PROXY = $proxy
$env:NODE_USE_ENV_PROXY = '1'
$env:NO_PROXY = (($s.ProxyOverride -split ';') | Where-Object { $_ -and $_ -ne '<local>' }) -join ','
```

在 `project\` 下：

```
cmake --build --preset win-x64-release-vs18 --target IGPU
node --use-env-proxy "D:\node.js\node_cache\_npx\166e0ec5f4c2d768\node_modules\@gamemaker\gm-cli\dist\cli.js" run --no-errors-only
```

预期 `checks failed : 0`，`igpu_version` 为 `0.5.0`，`feature level` 为 `2`（`Level_11_1`）。已知无害输出：`Failed to load Options from local_settings.json`。

## 这一版是什么

`0.4.0` 把公开函数从 105 个收到 59 个。`0.5.0` 加上 `igpu_bind_current()`，公开函数 60 个。Windows D3D11 集成测试 `checks failed : 0`，当前检查项 **878** 项。`igpu_version()` 返回 `0.5.0`。

`igpu_init(device, context, swapchain)` 仍是三个 `gmval` 指针。不要把指针塞进 GMIDL 类的 `gmval` 字段：本机 extgen `v1.d8c68bd` 会把那种字段生成成 `DataStream`，而 `readValue<DataStream>` 编不过。反射用的是具体字段的类 `IgpuUniformMember` / `IgpuUniformBlock`。

生成物只由 extgen 重写：`code_gen/`、`project/scripts/IGPU_API/IGPU_API.gml`、`project/extensions/IGPU/IGPU.yy`、根上的 `docs` 文件。不要手改它们。

## OpenGL 第一刀

这个后端给 Windows 以外的 GameMaker 运行时用。GMS2 Runtime 在 Windows 上走 Direct3D 11。不要为了让 Windows 的 `gm-cli` 跑到 GL 而加 ANGLE，也不要把 GL 塞进 `igpu_init`。

两条证据的职责：

1. **产品路径。** Linux、macOS、Android、iOS、tvOS 上，游戏步进和 GL 上下文在同一条线程，扩展函数跑的时候上下文已经 current。`igpu_bind_current()` 只绑定那个上下文，不创建、不销毁。`config.json` 仍只打开 Windows。那些平台上的 GameMaker 运行还没做。
2. **探针。** `tests/gl_probe` 用 `wglCreateContext`（不请求版本）建隐藏窗口上的上下文，`wglMakeCurrent` 之后调用和 GML 相同的 `igpu_*`，`igpu_shutdown()` 之后才 `wglDeleteContext`。这些 WGL 调用只在 `tests/gl_probe/wgl_host.cpp`。

Windows GameMaker DLL 编译时不定义 `IGPU_HAS_OPENGL`。`igpu_bind_current()` 返回 false，`igpu_get_last_error()` 是 `igpu_bind_current: this build has no OpenGL backend`。`igpu_init` 之后再调也失败，D3D11 设备还在。GML 里这 5 个检查已经在 `Create_0.gml`。

HTML5 / WASM（含 GX.games）仍排在本机后端之后，要单独写 JavaScript。主机排在那之后。extgen schema 里的主机槽位现在不启用。

### 已经核对过的平台事实

Runtime 源码在 `D:\Users\User\Documents\gml_ext\OpenGM\runtime\GMS2-Runner-Main\VC_Runner`。这是去混淆派生树，不是官方发布。`config.json` 现在只打开了 Windows。

`os_get_info` 交设备指针的，这份源码里只有 Windows D3D11：`video_d3d11_device`、`video_d3d11_context`、`video_d3d11_swapchain`。这份 Windows 运行时没有 WGL，也没有 `wglCreateContext`。

Linux、macOS、Android、iOS、tvOS 不交指针。游戏步进和 GL 上下文在同一条线程上，扩展函数跟着 GML 跑的时候上下文已经 current：

- Android：`DemoGLSurfaceView.java` 用 EGL10。ES 2 走 `setEGLContextClientVersion(2)`，否则是 ES 1。`onDrawFrame` 里调用 `RunnerJNILib.Process`。
- iOS / tvOS：`ES2Renderer.m` 建 `kEAGLRenderingAPIOpenGLES2`。`renderTheScene` 先 `setCurrentContext`，再 `iPad_Process()`。`MTLDevice` 没有交给 GML。Metal 只显示 OpenGL 画完的纹理。
- Linux：`TFormM.cpp` 用 `glXCreateContext` 建旧式 GLX，不请求版本。`glXMakeCurrent` 之后同线程进入 `MainLoop_Process`。
- macOS：`YYGLView.mm` 请求 `NSOpenGLProfileVersionLegacy`。`performGameStep` 先 `beginRender`，那里 `makeCurrentContext`。

因此产品入口是 `igpu_bind_current()`。公开函数 60 个。Windows 的 GameMaker 构建里这个入口失败，并写明这份构建没有 OpenGL 后端。

ES 2 上，计算着色器、存储缓冲、细分、几何着色器、统一缓冲块和三维纹理不会为 true。立方体贴图在 ES 2 里有。多目标和实例化取决于扩展字符串。`glsl_es` 不带版本号。源码和上下文对不上时编译失败。这块桌面探针上的 `glsl_es` 已经会失败，错误是 `igpu_shader_compile: dialect 'glsl_es' does not match this OpenGL context`。成功编译 `glsl_es` 要等真有 ES 上下文。

这台 Windows 机器上的 `gm-cli` 只能证明 D3D11。探针不能冒充那些平台的运行时。不要把自建上下文写成 Windows 产品功能。不要引入 ANGLE。

### 这一版已经补上的公开契约

实例化、间接绘制、面片、画进纹理和 `igpu_draw_sampled` 与 `igpu_draw`、`igpu_draw_indexed` 一样。非零的混合、深度、光栅和采样器只在这一次绘制里生效，返回前恢复。比较采样接受级数偏移、最细、最粗，以及线性过滤上的各向异性。`SampleCmp` 看偏移。这块 Direct3D 11 设备上 `SampleCmpLevelZero` 也看偏移，最细和最粗仍然把级数留在范围内。边框色 `0` 是黑色，不是「没给颜色」。比较值 `0` 是普通采样。比较和边框寻址仍然互斥。这块编译器的 `Texture2D` 没有 `SampleCmpGrad`，偏移证明用的是带真实纹理坐标导数的 `SampleCmp`。

## 必须守住的结构

这是整个扩展的规则，不是某一条功能的规则。

- 功能分支必须推送到 `origin`，并且只通过 GitHub 拉取请求合并进 `main`：推送、开拉取请求、批准并合并。探针通过之后就做完，不要留到下一会话。不要本地合并后直接推 `main`。`9c56d05` 和 `84b40f5` 是例外，不要再这么做。
- 公开接口是跨后端的。`spec.gmidl`、公共头和 GML 里不出现 d3d、dxgi、hlsl、`_5_0`、ID3D。方言提示字符串 `hlsl` / `glsl` / `glsl_es` / `msl` / `spirv` 除外。复用 GameMaker 已有的名字：`bm_*`、`cmpfunc_*`、`cull_*`、`tf_*`、`surface_*`、`IgpuAddressMode`。
- 调用链是 `spec.gmidl` → extgen 生成的 `code_gen` → `src/native/IGPU_native.cpp` 的薄包装 → `igpu::` 门面（`igpu_gpu.cpp`）→ `Backend` 虚函数。选具体后端的地方有两处，都在绑定设备时：`bind_device()` 装 `make_d3d11_backend`；`bind_current_context()` 只在定义了 `IGPU_HAS_OPENGL` 的探针里装 `GlBackend`。Direct3D 的实现仍在 `d3d11/igpu_d3d11_backend.cpp` 的 `namespace igpu::d3d11_impl`。
- 门面在虚函数调用之前执行 `require_device`。虚函数不写默认参数。公共头可以有默认参数。
- GameMaker 颜色是 BGR（`c_red` 是 255，`c_lime` 是 65280，`c_blue` 是 16711680，`c_fuchsia` 是 16711935）。拆通道在 `igpu_gpu.cpp` 的 `gm_colour_channels()`，后端只收到 0 到 1 的红、绿、蓝。后端做不到的值让这次调用失败，不要改成别的值。
- `d3d11_impl` 内部的无限定调用必须解析到 `d3d11_impl`，否则会递归回门面。
- MSVC 按代码页 936 读 cpp。中文注释里的字节 `0x5C` 会吃掉下一行。编译进 MSVC 的 cpp 注释只用 ASCII。C4819 是已知噪声。
- 不要手改 `.yy`、`.yyp`、`code_gen`。改了 `spec.gmidl` 的签名就跑 `D:\GM-ExtensionGenerator\extgen.exe --config config.json`。注释改动可以不跑，签名改动必须跑，而且生成的 `IGPUInternal_native.h` 必须和 `IGPU_native.cpp` 的包装一致。
- 能力只有真有 API 才为 true。键数保持 35。键清单代码块里不要写 `surface_*`。文档里不要为无关列表写「N 个键」，验证脚本会把那句话当成键数。
- 断言数是 `_igpu_check(` 的调用点，去掉函数定义那一行。文档里的「当前检查项 **N 项**」必须落在验证脚本打印的区间里。
- GML 数组字面量里不要写裸负数。数组里的句柄和枚举经常以 `uint64` 到达，读取时接受 `uint64`、`int32` 和 `double`。
- OpenGL 第一刀只在探针里。Windows 的 GameMaker DLL 没有这份后端。产品代码用调用线程上已经 current 的上下文，不创建、不销毁它。`wglCreateContext` / `wglDeleteContext` / `wglMakeCurrent` 只在 `tests/gl_probe`。不要引入 ANGLE。不要手改生成文件来传递指针。DLL 的源文件 GLOB 保持 `native/*.cpp` 和 `native/d3d11/*.cpp`，不要收进 `native/gl/`。
- 借用 GameMaker 的设备：AddRef 一次，只 Release 这一次。`reset()` 先 `set_active_backend(nullptr)`。
- `gm-cli`、`git fetch`、`git push`、`git pull` 和 `gh` 在这台机器上必须走系统代理。每次从 `HKCU:\Software\Microsoft\Windows\CurrentVersion\Internet Settings` 读取 `ProxyEnable` 和 `ProxyServer`。Node 用 `node --use-env-proxy`，并设好 `HTTP_PROXY`、`HTTPS_PROXY`、`NO_PROXY` 和 `NODE_USE_ENV_PROXY=1`。Git 只在这一次命令上加 `git -c http.proxy=http://<地址>`，不要写进 `git config`。代理没开就停。不要把端口写死，不要裸连。做法在本文开头「Git Bash」后面，以及「改了 DLL 才会再跑 GameMaker」那一节。

## 当前数字

| 项 | 值 |
|---|---|
| 版本 | 0.5.0 |
| 检查 | **878** 项，`checks failed: 0` |
| spec 函数 | 60 |
| 能力键 | 35 |
| `tools\verify_handover.ps1` | exit 0 |
| D3D11 测试 GPU | AMD Radeon Vega 8，`igpu_get_feature_level()` 为 `2`（`Level_11_1`） |
| GL 探针 | 同一块 GPU，`GL_VERSION` 为 `4.6.0 Compatibility Profile Context 26.5.2.260413` |
| GL 像素 | `255 / 16711680 / 16711680` |
| GL 索引像素 | `255 / 16711680`，随后右半边也是 `255` |
| GL 非索引像素 | `255 / 0`，随后右半边也是 `255` |
| GL 顶点颜色 | `255 / 16711680`；随后 float2 绘制是 `0 / 0`；alpha 0 仍是 `255`；从第 6 个顶点画进纹理时是 `0 / 16711680` |
| GL 图元 1/2/3/5 | 三角带 `255 / 0`，随后右边也是 `255`。点是单个纹素 `255`，邻居是 `0`。线列表是 `255 / 0 / 255`。线带随后右边也是 `255` |
| GL 纹理采样 | 源 `255 / 16711680`。前 3 个顶点是 `255 / 0`，尾巴是 `255 / 16711680`。常数 `u = 0.484375` 是 `255 / 255` |
| GL 包裹 | 重复 `u = 1.0625` 为 `255 / 255`，钳制和句柄 0 为 `16711680 / 16711680` |
| GL 线性放大 | `u = 0.5` 钳制是 `8388736 / 8388736`。`u = 1.5` 重复是同一混合，钳制是 `16711680 / 16711680`。随后句柄 0 在 `u = 0.484375` 仍是 `255 / 255`。加上级间线性、缩小仍是最近点时，左边导数 `0.001` 是 `5046450`，右边级数 `0.5` 是 `32896`。随后句柄 0 是 `255 / 65280`。边框采样器上同一组合随拉取请求 #37 进入 `main`，探针提交 `a582255`：绿色边框、左边 `u = 1.0` 是 `8421376`，右边级数 `0.5` 是 `32896`。随后句柄 0 是 `16711680 / 65280`，再绑回 `u = 0.4` 是 `255 / 65280`。钳制采样器上放大、缩小和级间都是线性时，左边导数 `0.001` 是 `5046450`，右边级数 `0.5` 是 `2523225`。随后句柄 0 是 `255 / 65280`。边框采样器上放大、缩小和级间都是线性时，绿色边框、左边 `u = 1.0` 是 `8421376`，右边级数 `0.5` 是 `2523225`。随后句柄 0 是 `16711680 / 65280`，再绑回 `u = 0.4` 是 `255 / 65280`。 |
| GL 镜像寻址 | `u = 1.6` 最近点镜像是 `255 / 255`，重复和钳制是 `16711680 / 16711680`。`u = -0.625` 镜像是 `16711680 / 16711680`。`u = 1.5` 线性镜像是 `8388736 / 8388736`。随后句柄 0 在 `u = 1.6` 是 `16711680 / 16711680`，在 `u = 0.484375` 是 `255 / 255` |
| GL 边框寻址 | `u = 1.6` 绿色边框是 `65280 / 65280`，`u = -0.0625` 也是 `65280 / 65280`。红色边框在 `u = 1.6` 是 `255 / 255`，重复和钳制是 `16711680 / 16711680`。`u = 1.0` 线性绿色边框是 `8421376 / 8421376`。随后句柄 0 在 `u = 1.6` 是 `16711680 / 16711680`，在 `u = 0.484375` 是 `255 / 255` |
| GL 线性缩小 | 2×2 源，采样点 `(0.4, 0.5)`。放大最近点、缩小线性是 `255 / 5046450`。放大线性、缩小最近点是 `5046450 / 255`。随后句柄 0 是 `255 / 255`。放大和缩小都是线性、级间保持最近点时两边都是 `5046450`。随后句柄 0 是 `255 / 255`。级间线性见下一行。边框采样器上放大最近点、缩小线性是 `255 / 5046450`，随后句柄 0 是 `255 / 255`。 |
| GL 最细级数 | 2×2 两级、导数 `0.125`、偏移 0。最细 1 是 `65280 / 65280`。随后句柄 0 是 `255 / 255`。边框最细 1 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。级间最近点。级间线性见下一行。 |
| GL 级间线性 | 2×2 两级、`texture2DLod` 级数 `0.5`。级间最近点是 `65280 / 65280`。级间线性是 `32896 / 32896`。随后句柄 0 是 `65280 / 65280`。最细 1 是 `65280 / 65280`，随后不带最细的级间线性是 `32896 / 32896`。最粗 0 是 `255 / 255`，随后不带最粗的级间线性是 `32896 / 32896`。缩小为线性是 `2523225 / 2523225`，随后句柄 0 是 `65280 / 65280`。边框采样器上的级间线性随拉取请求 #31 进入 `main`：级数 `0.5` 是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。偏移 `0.5` 随拉取请求 #32 进入 `main`，探针提交 `a12f325`：级数 `0.5` 是 `65280 / 65280`，随后句柄 0 是 `65280 / 65280`。最细 1 随拉取请求 #33 进入 `main`，探针提交 `4d6cfd7`：级数 `0.5` 是 `65280 / 65280`，紧接着不带最细的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。最粗 0 随拉取请求 #34 进入 `main`，探针提交 `7526d1e`：级数 `0.5` 是 `255 / 255`，紧接着不带最粗的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。线性缩小随拉取请求 #35 进入 `main`，探针提交 `e363bae`：`(0.4, 0.5)`、级数 `0.5` 是 `3371865 / 3371865`，紧接着缩小为最近点的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。线性放大加级间线性、缩小为最近点随拉取请求 #36 进入 `main`，探针提交 `5fba67e`：左边导数 `0.001` 是 `5046450`，右边级数 `0.5` 是 `32896`。随后句柄 0 是 `255 / 65280`。边框采样器上同一组合随拉取请求 #37 进入 `main`，探针提交 `a582255`：绿色边框、左边 `u = 1.0` 是 `8421376`，右边级数 `0.5` 是 `32896`。随后句柄 0 是 `16711680 / 65280`，再绑回 `u = 0.4` 是 `255 / 65280`。钳制采样器上放大、缩小和级间都是线性时，左边导数 `0.001` 是 `5046450`，右边级数 `0.5` 是 `2523225`。随后句柄 0 是 `255 / 65280`。边框采样器上放大、缩小和级间都是线性时，绿色边框、左边 `u = 1.0` 是 `8421376`，右边级数 `0.5` 是 `2523225`。随后句柄 0 是 `16711680 / 65280`，再绑回 `u = 0.4` 是 `255 / 65280`。 |
| GL 最粗级数 | 同一张 2×2、导数 `0.125`。偏移 4、最粗 0 是 `255 / 255`。随后句柄 0 是 `255 / 255`。不限制的偏移 4 仍是 `65280 / 65280`。边框最粗 0、偏移 4 是 `255 / 255`，随后句柄 0 是 `255 / 255`，随后不限制的偏移 4 是 `65280 / 65280`。 |
| GL 边框级数 | 同一张 2×2、导数 `0.125`。三轴边框、缩小和级间都是最近点、偏移 4 是 `65280 / 65280`。随后句柄 0 是 `255 / 255`。最细 1、偏移 0 是 `65280 / 65280`，随后句柄 0 是 `255 / 255`。最粗 0、偏移 4 是 `255 / 255`，随后句柄 0 是 `255 / 255`，随后不限制的偏移 4 是 `65280 / 65280`。放大最近点、缩小线性是 `255 / 5046450`，随后句柄 0 是 `255 / 255`。级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。偏移 `0.5` 是 `65280 / 65280`，随后句柄 0 是 `65280 / 65280`。最细 1 是 `65280 / 65280`，紧接着不带最细的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。最粗 0 是 `255 / 255`，紧接着不带最粗的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。缩小为线性是 `3371865 / 3371865`，紧接着缩小为最近点的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。线性放大、绿色边框、左边 `u = 1.0` 是 `8421376`，右边级数 `0.5` 是 `32896`。随后句柄 0 是 `16711680 / 65280`，再绑回 `u = 0.4` 是 `255 / 65280`。钳制采样器上放大、缩小和级间都是线性时，左边导数 `0.001` 是 `5046450`，右边级数 `0.5` 是 `2523225`。随后句柄 0 是 `255 / 65280`。边框采样器上放大、缩小和级间都是线性时，绿色边框、左边 `u = 1.0` 是 `8421376`，右边级数 `0.5` 是 `2523225`。随后句柄 0 是 `16711680 / 65280`，再绑回 `u = 0.4` 是 `255 / 65280`。 |
| GL 混轴包裹 | U、V、W 可以不同，各自是钳制、重复或镜像。横纹 `u = 1.0625` 的 U 重复是 `255 / 255`。竖纹 `v = 1.0625` 的 V 重复是 `255 / 255`。`u = 1.6` 的 U 镜像是 `255 / 255`。`u = 1.5` 的线性 U 镜像是 `8388736 / 8388736`。随后句柄 0 在 `v = 1.0625` 是 `16711680 / 16711680`，在 `u = 1.6` 是 `16711680 / 16711680`，在 `u = 0.484375` 是 `255 / 255`。 |
| GL 混合 | src-alpha、alpha 0 保持 `16711680`。alpha 255 写成 `255`。随后句柄 0、alpha 0 的蓝是 `16711680`。max 是 `16711935`，随后句柄 0 写成 `255`。索引和画进纹理的 alpha 0 保持 `16711680`。红通道不写时是 `0`，随后句柄 0 是 `255`。采样绘制上，alpha 0 的红源保持 `16711680`，alpha 255 写成 `255`，随后句柄 0 写成 `255`。重复采样器加 src-alpha、`u = 1.0625` 是 `255`，随后句柄 0 是 `16711680`。 |
| GL 深度 | 小于比较先近后远是 `255 / 255`。写入关掉之后远处绿是 `65280 / 65280`。测试关闭和句柄 0 也是 `65280 / 65280`。索引和画进纹理的小于比较是 `255 / 255`。模板仍拒绝。 |
| GL 采样光栅 | `igpu_draw_sampled` 接受光栅句柄，返回前写回剔除和剪裁开关，不改剪裁矩形。句柄 0 不改这四项。逆时针加 `cull_clockwise` 是 `255 / 0`，加 `cull_counterclockwise` 是 `0 / 0`，随后句柄 0 是 `255 / 0`。顺时针加 `cull_clockwise` 是 `0 / 0`，加 `cull_counterclockwise` 是 `255 / 0`。左半边剪裁把满幅采样收成 `255 / 0`，随后句柄 0 是 `255 / 255`。调用方已经打开剪裁时，句柄 0 仍是 `255 / 0`。剪裁关闭的句柄写成 `255 / 255`，随后句柄 0 是 `255 / 0`。同时带深度、混合和重复采样器时，`u = 1.0625` 是 `255 / 255`，随后句柄 0 是 `16711680 / 16711680`。功能提交 `fbd3811`，拉取请求 #18。 |
| Git | 写这份交接时 `origin/main` 是 `19148c1`（拉取请求 #11）。混轴包裹的功能提交是 `13f926a`，已经进入 `main`。混合随拉取请求 #12 进入 `main`，功能提交 `8beb2a2`。`gl-blend` 从 `19148c1` 拉出。线性缩小的功能提交是 `8fe079a`。拉取请求 #10 的合并提交是 `fb136fd`。边框寻址随拉取请求 #9 进入 `main`，功能提交 `38687b8`。镜像寻址随拉取请求 #8 进入 `main`，功能提交是 `b57cc88`。线性放大的提交是 `9e4325f`。钳制与重复的合并提交是 `84b40f5`。`gl-address` 在 `95867cd`。最近点采样的合并提交是 `9c56d05`。`gl-sample` 在 `b871e87`。拉取请求 #4 的合并提交是 `83afeb8`。`gl-primitives` 在 `3e0a1f2`。拉取请求 #3 是 `0885e67`。`gl-vertex-colour` 在 `7c0c6e4`。拉取请求 #2 是 `edb0ccd`。`gl-draw` 在 `31c6982`。`gl-bind-current` 在 `fb4a9a5`。 |

构建：在仓库根目录 `cmake --build --preset win-x64-release-vs18 --target IGPU`。DLL 会拷到 `project\extensions\IGPU\IGPU.dll`。

集成测试：在 `project\` 下执行。先按上面「改了 DLL 才会再跑 GameMaker」那段套上系统代理，再用：

```
node --use-env-proxy "D:\node.js\node_cache\_npx\166e0ec5f4c2d768\node_modules\@gamemaker\gm-cli\dist\cli.js" run --no-errors-only
```

已知无害输出：`Failed to load Options from local_settings.json`。

探针：

```
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

## 探针里已经有的 OpenGL

文件：

- `tests/gl_probe/wgl_host.cpp` 拥有窗口和 `HGLRC`。`gl_probe_forget()` 只把句柄置空，不删除。
- `src/native/gl/igpu_gl_loader.cpp` 用 `wglGetProcAddress` 装 GL 2.0 入口。`<GL/gl.h>` 之前要先 include `<Windows.h>`。
- `src/native/gl/igpu_gl_backend.cpp` 是 `GlBackend`。`draw_indexed` 已经转到 `gl_draw_indexed`。`draw` 转到 `gl_draw`。`draw_sampled` 转到 `gl_draw_sampled`。没实现的虚函数返回 0 或 false，错误以 `: the opengl backend does not implement this call yet` 结尾。
- `src/native/gl/igpu_gl_draw.cpp` 管缓冲、布局、纹理、绘制、读回。
- `src/CMakeLists.txt` 把核心源、`d3d11`、`gl` 和 `code_gen` 编进 `igpu_gl_probe`，并定义 `IGPU_HAS_OPENGL`、`NOMINMAX`、`WIN32_LEAN_AND_MEAN`。DLL 目标没有这些。

已经为 true 的能力：`ShaderCompileRuntime`、`ShaderStageVertex`、`ShaderStagePixel`、`Texture2D`、`InputLayout`、`VertexBuffer`、`IndexBuffer`、`Draw`、`DrawIndexed`、`DrawStateRestore`、`SamplerState`、`BlendState`、`RasterState`、`DepthState`。`formats.surface_rgba8unorm` 为 true，另外七个格式为 false。`igpu_get_feature_level()` 在 GL 上是 `0`（`Unknown`）。没有 DXGI 适配器时，显存和后缓冲尺寸是 0，设备名来自 `GL_RENDERER`。方言是 `glsl`。

着色器是 `#version 120`。入口必须是 `main`。空方言和 `"glsl"` 可以编译。`"hlsl"` 被拒绝。顶点阶段和像素阶段都绑上之后才链成一个程序，属性 0 叫 `in_pos`。属性 1 在颜色程序里是 `in_colour`，在采样程序里是 `in_uv`。不声明 `in_colour` 的程序仍然能链上。阶段绑错会失败，原来的绑定还在。`shader_bind(0, stage)` 解绑该阶段。

绘制接受图元 `1`、`2`、`3`、`4`、`5`（点列表、线列表、线带、三角形列表、三角带），并且只接受一个目标、layer 0、mip 0。活着的深度句柄在 `igpu_draw`、`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 这一次绘制里生效，返回前写回深度测试开关、比较函数和深度写入。句柄 0 不改这三项。模板打开失败，错误含有 `stencil`。`igpu_draw_sampled` 接受深度句柄，随拉取请求 #17 进入 `main`，功能提交 `28546cf`，返回前写回测试开关、比较函数和写入。句柄 0 不改这三项。实心剔除句柄可以非 0。混合句柄 0 不改混合状态。活着的混合句柄在 `igpu_draw`、`igpu_draw_indexed`、`igpu_draw_to_render_targets` 和 `igpu_draw_sampled` 这一次绘制里生效，返回前写回开关、分开的因子、方程和四个通道的写掩码。前三支绘制的采样器句柄也必须是 0。`igpu_draw_sampled` 可以带一个三轴同为钳制、重复、镜像或边框的采样器。放大和缩小各自是最近点或线性。级间线性在缩小是最近点或线性时成立。最细可以大于 0，最粗可以低于不限制的哨兵，级数偏移可以不是 0。缩小是最近点时这一次绘制把缩小过滤设成 `GL_NEAREST_MIPMAP_LINEAR`，缩小是线性时设成 `GL_LINEAR_MIPMAP_LINEAR`。最细大于 0 时还把 `GL_TEXTURE_MIN_LOD` 设成该值。最粗低于不限制的哨兵时还把 `GL_TEXTURE_MAX_LOD` 设成该值。级数偏移不是 0 时把 `GL_TEXTURE_LOD_BIAS` 设成偏移。返回前写回这些值。缩小也是线性的级间线性读回 `2523225 / 2523225`。非边框采样器的最细可以大于 0，最粗可以低于不限制的哨兵。这一次绘制把 `GL_TEXTURE_MIN_LOD` 或 `GL_TEXTURE_MAX_LOD` 设成该值，并把缩小过滤改成对应的多级最近点或线性，返回前写回。句柄 0 不改最细和最粗。边框采样器的级数偏移可以不是 0。这一次绘制把缩小过滤设成 `GL_NEAREST_MIPMAP_NEAREST`，并把 `GL_TEXTURE_LOD_BIAS` 设成偏移，返回前写回。偏移 4 读回 `65280 / 65280`，随后句柄 0 是 `255 / 255`。边框采样器的最细可以大于 0。这一次绘制把 `GL_TEXTURE_MIN_LOD` 设成最细，返回前写回。最细 1、偏移 0 读回 `65280 / 65280`，随后句柄 0 是 `255 / 255`。边框采样器的最粗可以低于不限制的哨兵。这一次绘制把 `GL_TEXTURE_MAX_LOD` 设成最粗，返回前写回。最粗 0、偏移 4 读回 `255 / 255`，随后句柄 0 是 `255 / 255`，随后不限制的偏移 4 是 `65280 / 65280`。边框采样器的缩小可以是线性。没有级数选择时这一次绘制把 `GL_TEXTURE_MIN_FILTER` 设成 `GL_LINEAR`，返回前写回。放大最近点、缩小线性读回 `255 / 5046450`，随后句柄 0 是 `255 / 255`。边框采样器的级间线性把缩小过滤设成 `GL_NEAREST_MIPMAP_LINEAR` 或 `GL_LINEAR_MIPMAP_LINEAR`，返回前写回。级数 `0.5` 读回 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。偏移 `0.5` 把 `GL_TEXTURE_LOD_BIAS` 设成 `0.5`，读回 `65280 / 65280`，随后句柄 0 是 `65280 / 65280`。最细 1 把 `GL_TEXTURE_MIN_LOD` 设成 `1`，读回 `65280 / 65280`，紧接着不带最细的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。最粗 0 把 `GL_TEXTURE_MAX_LOD` 设成 `0`，读回 `255 / 255`，紧接着不带最粗的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。缩小为线性时把缩小过滤设成 `GL_LINEAR_MIPMAP_LINEAR`，读回 `3371865 / 3371865`，紧接着缩小为最近点的级间线性是 `32896 / 32896`，随后句柄 0 是 `65280 / 65280`。线性放大加级间线性、缩小为最近点时，这一次绘制把 `GL_TEXTURE_MAG_FILTER` 设成 `GL_LINEAR`，把 `GL_TEXTURE_MIN_FILTER` 设成 `GL_NEAREST_MIPMAP_LINEAR`，返回前写回。左边导数 `0.001` 读回 `5046450`，右边 `texture2DLod` 级数 `0.5` 读回 `32896`。随后句柄 0 是 `255 / 65280`。边框采样器把这两项设成同样的值，并写入绿色边框。左边 `u = 1.0`、导数 `0.001` 读回 `8421376`，右边 `(0.4, 0.5)`、级数 `0.5` 读回 `32896`。随后句柄 0 是 `16711680 / 65280`。再绑回 `u = 0.4` 的着色器，句柄 0 是 `255 / 65280`。线性放大、线性缩小和级间线性同时打开时，这一次绘制把 `GL_TEXTURE_MAG_FILTER` 设成 `GL_LINEAR`，把 `GL_TEXTURE_MIN_FILTER` 设成 `GL_LINEAR_MIPMAP_LINEAR`，返回前写回。左边导数 `0.001` 读回 `5046450`，右边 `texture2DLod` 在 `(0.4, 0.5)`、级数 `0.5` 读回 `2523225`。随后句柄 0 是 `255 / 65280`。边框色只在三轴都是边框时写入这一次绘制，返回前写回。句柄 0 不改包裹、边框色，也不改放大过滤或缩小过滤。图元 `6` 是扇形，拒绝且不改像素。图元 `0` 和 `7` 拒绝，错误含有 `does not draw this primitive`。非采样绘制上的非零采样器拒绝，错误含有 `draws with no extra state`。未知深度句柄拒绝，错误含有 `unknown depth handle`。未知光栅句柄拒绝，错误含有 `unknown raster handle`。实心剔除句柄在 `igpu_draw`、`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 这一次绘制里生效，返回前写回剔除开关、正面方向和剔除面。句柄 0 不改这三项。线框和关掉深度裁剪仍拒绝。剪裁打开的句柄在这一次绘制里打开 `GL_SCISSOR_TEST`，不改剪裁矩形，返回前写回开关。剪裁关闭的句柄在这一次绘制里关掉测试，返回前写回。句柄 0 不改剪裁开关。`igpu_draw_sampled` 接受光栅句柄，返回前写回剔除开关、正面方向、剔除面和剪裁开关，不改剪裁矩形。句柄 0 不改这四项。未知句柄和已经释放的句柄失败，错误含有 `unknown raster handle`。负数句柄失败，错误含有 `not a state object`。未知混合句柄拒绝，错误含有 `unknown blend handle`。`igpu_draw_sampled` 的活混合在这一次绘制里生效，返回前写回。未知混合句柄拒绝，错误含有 `unknown blend handle`。点大小和线宽保持 1。`igpu_draw` 与 `igpu_draw_indexed` 不改帧缓冲、视口和程序。`igpu_draw_to_render_targets` 会恢复帧缓冲和视口。布局接受一个 `float2` 位置（usage `1`，type `2`，step `0`，stride 0 或 8），也接受 float2 后面接一个 4 字节颜色（usage `2`，type `5`），缓冲步长必须显式写成 12，也接受 float2 位置后面接 float2 纹理坐标（usage `4`，type `2`），缓冲步长必须显式写成 16。步长 0 仍是位置缓冲。`igpu_draw_sampled` 已从 `not_yet` 转成 `gl_draw_sampled`。绘制按这个步长绑属性 0 和属性 1，返回前都关掉。其它布局和步长仍拒绝。顶点缓冲是静态的，一次写满。索引缓冲也是静态的，16 位，`bind` 为 2，stride 为 0，长度是 2 的倍数。`igpu_draw` 和 `igpu_draw_indexed` 都画进当前帧缓冲，不清屏。探针用 `gl_color_target_begin` 把 IGPU 纹理绑成当前目标，画完再还原。读回把 RGBA 收成 `r | (g << 8) | (b << 16)`，丢掉 alpha。`gl_row = height - 1 - y`，所以 `y = 0` 是裁剪空间的上方。画进纹理的 `igpu_draw_to_render_targets` 返回前恢复 `GL_FRAMEBUFFER_BINDING` 和 `GL_VIEWPORT`，不解开调用方绑好的程序。`igpu_draw` 和 `igpu_draw_indexed` 不动帧缓冲、视口和程序；它们恢复顶点数组和数组缓冲。索引绘制另外恢复元素数组缓冲。

`supports()` 里的局部量 `native` 等于 `d3d11_backend()`。交接脚本把裸的 `return native` 当成这份构建上的恒 true。OpenGL 多出来的 true 写成 `native || opengl_backend()`。不要把 `native` 改回「任意后端」，否则 GL 会继承计算、三维纹理和查询。

`require_device` 认「已初始化且有后端」。D3D 移除检查仍只在 `device != nullptr` 时调用 `GetDeviceRemovedReason`。

### 这块驱动上踩过的两处

- 兼容上下文也要先 `glBindVertexArray`。不绑的话 `glDrawArrays` 不写像素，也不报错。
- 新建的 rgba8 图像要先 `glClear` 一次。不清的话随后的绘制会被丢掉。纹理创建里清成黑。绘制本身不清，所以第二笔可以留住第一笔的颜色。

## 调用链上刚加过的采样器

都走上面的 Backend 链。Direct3D 的过滤、寻址、各向异性和比较只出现在 `d3d11_impl`。

- 分开过滤的偏移和范围：`igpu_sampler_state_create_filters_offset`、`igpu_sampler_state_create_filters_range`
- 分轴和各向异性的范围：`igpu_sampler_state_create_axes_range`
- 带边框色的范围：`igpu_sampler_state_create_filters_border_range`、`igpu_sampler_state_create_axes_border_range`。颜色在门面拆开。
- 比较：`igpu_sampler_state_create_compare`。通过是 1，不通过是 0。

各向异性足迹很宽时会把边框色掺进旁边的纹素。边框色加级数的像素证明因此用了点采样；16 倍各向异性带边框色和级数范围可以建出来。

---

## 0. 三十秒版本（2026-09-23 审计，已过期）

下面到文末是审计会话的原始记录。断言 152、工作区干净、能力位恒 false 都已过期。当前数字以上面的表为准。

用户给了 **GMS2 引擎去混淆源码**，要求"根据源码重新认真审视我们的扩展"。

审下来了：**常量映射这类细节全部正确，但有三处结构性偏差**，其中两处已修。
最重要的发现不是新 bug，而是**"我们以为已经验证过的东西其实没有"** ——
`igpu_draw` 当时连着色器都绑不上，而测试从未察觉。

| 门禁 | 状态 |
|---|---|
| `pwsh -File tools\verify_handover.ps1` | ✅ exit 0（**10 个检查组**，共 26 条 `Check`） |
| `cmake --build --preset win-x64-release-vs18 --clean-first` | ✅ exit 0，非 C4819 警告 **0** 条 |
| `gm-cli run --no-errors-only` | ✅ `checks failed: 0`，**152** 项断言，exit 0 |

**HEAD `add3cf2`，分支 `main`，工作区干净。**
（`add3cf2` 是写完本文件时的提交；**再提交一次这个数字就会过期**。
以 `git rev-parse --short HEAD` 为准 —— 自检脚本第 8 组会核对它。
注意本文件自身的提交 `25fc643` 及之后都在 `add3cf2` 之上。）
⚠️ **没有配置 git remote —— 所有提交只存在本地。**

---

## 1. 现状快照（接手前请自行复验）

```
仓库      D:\Users\User\Documents\gml_ext\IGPU
HEAD      add3cf2   (main)
版本      0.3.0（API 有新增，版本号未提升）
断言      152 个 _igpu_check 调用点（定义行另计 1）
能力表    恒 true 13 项 / 恒 false 12 项
```

### 本次会话的 5 个提交

| 提交 | 内容 |
|---|---|
| `6006794` | 引擎源码审计 + 修正 11 项虚报能力 + 新增第 3b 组检查 |
| `f848570` | HANDOVER 顶部指向审计报告，修正"绘制"措辞 |
| `c148506` | **新增 `igpu_shader_bind`** —— 补上审计发现的断链 |
| `53d5657` | 写会话交接，记录检查器自己是怎么写错的 |
| `add3cf2` | 修正第 3b 组的计数显示（24 → 实际检查数） |

---

## 2. 外部依赖（不在仓库里，接手前确认仍在）

| 依赖 | 路径 | 用途 |
|---|---|---|
| extgen | `D:\GM-ExtensionGenerator\extgen.exe` | **改 `spec.gmidl` 后必须手动跑** |
| 引擎源码 | `D:\Users\User\Documents\gml_ext\OpenGM\` | 审计依据（去混淆，非官方） |
| gm-cli | `D:\node.js\node_cache\_npx\166e0ec5f4c2d768\...\gm-cli\dist\cli.js` | 跑集成测试 |
| YoYo.lib 符号 | `...\runtime-2026.0.0.23\yyc\Win32\lib\x64\YoYo.lib` | 符号层交叉验证 |
| Git Bash | `D:\Program Files\Git\bin\bash.exe` | PowerShell 的 `PATH` 里没有 bash。需要时用这个路径 |

> `OpenGM` 是**去混淆的派生源码**，不是官方发布。本次结论都用
> `YoYo.lib` 符号做了交叉验证，两者一致 —— 但**不要把源码当稳定 API**。

---

## 3. ⚠️ 最容易再踩的坑：extgen 必须手动跑

**改了 `spec.gmidl` 之后，不跑 extgen 就不会生成绑定代码，
而 `cmake --build` 照样成功** —— 因为还没有人引用新函数，编译器不会报错。

```pwsh
extgen --config config.json          # 必须
Select-String -Path code_gen\native\IGPUInternal_native.h -Pattern 'igpu_shader_bind'   # 验证
```

`code_gen/` 是**输入**（被 cmake GLOB），不是构建产物。
**"build 成功"不能证明新 API 存在** —— 本次就是靠 grep 生成文件才发现的。

---

## 4. 本次会话的实质产出

### 4.1 审计报告 `tools/engine_audit.md`

逐条把 `spec.gmidl` / `src/native/*` 的设计主张拿去和引擎真源码对质。

**✅ 验证为正确（不要动）**

- `os_get_info()` 确实给出 `GR_D3D_Device` / `GR_D3D_Context` / `g_SwapChain`
  （`YoYo_FunctionsM.cpp:519-538`）
- `pr_*` 1–6 与引擎 `ePrimType` 完全一致（`Graphics.h:789-807`）
- `vertex_usage_*` 1–9 / `vertex_type_*` 1–6 与 `yyVU*` / `yyVT*` 完全一致
  （`Vertex_Class.h:10-46`）
- 语义名拼写与 `g_VertexUsageStrings[]` 一致（`ShaderM.cpp:1336-1354`）
- `Static` → `D3D11_USAGE_DEFAULT` 与引擎 `VertexBuffer::Init` 同构
- 引擎**从不调用 `D3DCompile`**（全仓库零匹配）→ IGPU 运行时编译是**增量能力**

**常量映射的最强证据是运行时反证**，不是读源码：测试 L177 断言"匹配的布局
被接受"，若 `vertex_usage_position` 差一位就会变成 `COLOR` 而**必定被拒**。
L186 的反向断言证明该校验真的有鉴别力。两个审计子代理都把这条列为
"无法确定"，**现已确定：一致**。

**❌ 已修的两处**

1. **11 个能力虚报 true 却无对应 API**（Instancing / Queries / Fence /
   Texture3D / TextureArray / TextureCubemap / MultipleRenderTargets /
   IndirectDraw / Timestamps / OcclusionQuery / Wireframe）。
   调用方看到 `igpu_supports(Instancing) == true` 会去调一个**不存在的**函数。
   违反 `spec.gmidl:38-41` 自己写的核心约束。已全部改 `false`。
2. **`igpu_draw` 绑不上着色器** —— 见 §4.2。

**⚠️ 未解决的三处**

1. **像素级验证仍然缺失**（**下一优先级**）
2. **设备丢失无恢复路径**：`HandleDeviceLost()` 会释放并重建 device/context
   （`Graphics_DisplayM.cpp:1225-1258`），IGPU 仍持旧指针 → use-after-free 风险
3. ✅ **`IaStateGuard` 的文档理由**已按这里的结论改写（实现未动）。
   引擎每次绘制都无条件重设顶点缓冲、布局和拓扑，且不缓存
   （`StateManagerM.h:27-28`，"not included yet"）。
   “防止污染 GM 绘制”是错的。保留实现的理由是索引缓冲、绘制之间的设备一致性，
   以及将来引擎若缓存输入装配。见 `HANDOVER.md` §7.15。

### 4.2 新增 `igpu_shader_bind` / `igpu_get_bound_shader`

**审计最严重的发现**：`igpu_draw` 的注释说"调用方负责设置着色器"，
但 spec 里**没有任何函数能绑着色器** —— `igpu_shader_compile` 的句柄
传不进 GM 的 `shader_set()`。引擎绑定着色器只有一处
（`VertexBuilderM.cpp:816/822`）且只认自己的对象。
测试**从未调用 `shader_set`**，所以"draw 返回 true"只证明调用发出去了。

已补上，实现要点：
- `ShaderEntry` 记录编译时 stage，绑定时**校验匹配** → 类型错误在绑定点报错
- `shader = 0` **显式解绑**（非错误），不释放着色器
- **释放着色器会清掉绑定记录** → 否则"还绑着吗"会对已释放句柄答"是"
- `reset()` 同步清空，避免 shutdown/re-init 后残留

**有意不自动恢复**（与 IA 状态不同）：着色器是调用方主动设定的状态，
不是绘制的副作用；每次绘制后重绑会**对抗调用方自己的切换**，
且没有 GML 接口能读回"之前绑的是谁"。代价是 GM 自己的绘制**仍会覆盖**
IGPU 的绑定 → 应在需要的 `igpu_draw` **紧前**绑定。这条已写进 spec 注释。

新增 13 项断言（136 → 152），**经过金丝雀验证**：把 stage 校验改成
`if (false)` → `CHECK FAILED : stage mismatch is rejected`、exit 1。

---

## 5. 本次会话的教训（写给接手者，避免重蹈）

### 5.1 检查器自己会骗人 —— 我自己犯了两次

写第 3b 组时：
1. `$capsCpp` 是**路径**不是文本 → 喂给 `[regex]::Matches` 得 0 匹配 →
   每个能力都 `continue` → **检查恒过**
2. 循环变量取名 `$root`，**覆盖了脚本的仓库根路径** → 后面 4 组检查静默失效

**两个 bug 叠加，脚本照样打印"全部一致"** —— 而当时有 11 项虚报
且 4 组检查已坏。已加自检（"能解析出 N 个返回分支"）并改名 `$apiRoot`。

### 5.2 金丝雀"没红"有两种可能

给第 2 组做金丝雀时前两次"通过"**都是假象**：该组要求 API 名写成
**反引号+括号**（`` `name()` ``），我插入的是裸名字，**根本没进被检查集合**。

**教训**：金丝雀没红，要么检查器坏了，**要么你的突变没生效**。
必须确认突变**真的落到被检查范围内**（本次靠计数从 38 变 39 才判定生效）。

### 5.3 数字要报"实际检查了几个"

第 3b 组原先打印"共 24 个恒 true 能力"，那是**映射表大小**；
实际只有 13 个是 `native`，另 11 个已改 false 被跳过。
数字虚高会让接手者以为覆盖更广。已改为报实际值，**并断言它非零**
（若全部非 native，主检查会**空过**——正是这组要防的事）。

### 5.4 前一份交接文档的可信度是"打折"来的

我此前多份报告都说"draw 链路已跑通"，那是**基于测试通过**的判断。
用引擎源码一看，"跑通"的含义比文档暗示的弱得多。
**本次最大收获不是发现新 bug，而是发现"我们以为验证过的其实没有"。**

---

## 6. 接手后建议的第一步

按优先级：

1. ✅ **像素级验证** —— 已完成。离屏 surface + `surface_getpixel`，
   左半边 `c_red`、右半边保持清屏色。见 `HANDOVER.md` §6 第五批。
2. ✅ **设备丢失检测** —— `igpu_device_lost()`。发现移除、重置、挂起或驱动故障后置错并丢掉旧句柄。不自动重建。
3. **修正 §4.1 第 3 条的文档措辞**（实现不动）。**这是当前的第一项。**

> 顺带：`surface_set_target_ext(stage, id, depth_id)` **已是 GML 内置函数**，
> `MAX_MRTS = 4`（`Graphics.h:8`）—— §9 第 7 项（MRT）**可能不需要新 API**。

---

## 7. 交接物清单

| 文件 | 说明 |
|---|---|
| `HANDOVER.md` | 长期交接文档，**§0.0 是本次会话摘要** |
| `tools/engine_audit.md` | 引擎源码审计报告（本次核心产出） |
| `tools/verify_handover.ps1` | 自检脚本，10 组 / 26 条检查，**改文档或代码后必须跑** |
| `tools/yoyo_lib_symbols.md` | `YoYo.lib` 符号层调研（上次会话产出） |
| `spec.gmidl` | API 契约，**新增 2 个函数（共 33 个）** |
| `src/native/IGPU_native.cpp` | 绑定实现（`igpu_shader_bind` 等） |
| `project/objects/obj_igpu_test/Create_0.gml` | 集成测试，152 项断言 |

---

## 8. 诚实声明：没做到的事

1. **当时没有像素级证据** —— 本会话结束时还没有。后续已补上，见 `HANDOVER.md` §6 第五批
2. **设备丢失行为是源码推断**，未做运行时插桩验证引用计数
3. **`OpenGM` 是去混淆派生源码，不是官方** —— 结论已用 `YoYo.lib`
   符号交叉验证（一致），但源码反映的是"某个 GM 版本"，未必是
   runtime-2026.0.0.23 的逐字对应
4. **DX12 路径未深挖** —— IGPU 目前只针对 DX11
5. **`desc[32]` 越界**（`ShaderM.cpp:1388`）是引擎侧潜在问题，未验证能否触发
