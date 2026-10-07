# 会话交接 — 2026-10-07 — 版本 0.5.0 — OpenGL 顶点颜色已合并

> 先读这一节，再改代码。`HANDOVER.md` 是长文档，开头版本是 `0.5.0`。正文里 9 月的叙述已经过时。本文下半截仍保留 2026-09-23 审计记录。
>
> 比较采样的像素结果还在 Direct3D 11 上：参考值 0.5，左纹素 0.25、右纹素 0.75，小于比较是左黑右红，大于比较对调，线性比较在交界处是红 128。

## 下一会话接着做

停在 `main`。拉取请求 https://github.com/mionumbra/IGPU/pull/3 已合并，合并提交 `0885e67`。写这份交接时 `origin/main` 就是这个提交。顶点颜色已经在 `main` 上，不要重做，也不要再为它开拉取请求。`gl-vertex-colour` 还在，尖端是 `7c0c6e4`，已经包含在 `0885e67` 里。这份文档自己的提交在 `0885e67` 之后，以 `git rev-parse --short HEAD` 为准。

拉取请求 https://github.com/mionumbra/IGPU/pull/2 已经合并，合并提交 `edb0ccd`，非索引 `igpu_draw` 在 `main` 上。不要重做。

顶点颜色只在探针里。布局是 float2 位置再加一个 colour，缓冲步长必须显式写成 12。布局步长传 0 时内部记成 12，缓冲仍要 12。`in_pos` 在属性位置 0，`in_colour` 在属性位置 1。字节序是 R、G、B、A，不交换红蓝。左半边读回红 `255`，右半边是蓝 `16711680`。这一笔探针画的是 12 个顶点；只画前 6 个会让右边保持清屏黑。alpha 0 仍写出红，这次绘制不打开混合。随后用 float2 布局再画，两个采样点是 `0`，颜色属性已经关掉。`igpu_draw_to_render_targets` 从第 6 个顶点起画时，右半边是蓝，左半边保持清屏黑。Windows 的 GameMaker DLL 仍没有 OpenGL 后端。

已知的小缺口，不是下一刀：未知布局句柄的错误写成「the vertex buffer stride does not match the input layout」。调用失败，像素不变。`first_index + index_count` 和 `first_vertex + vertex_count` 在特别大的正数上可能回绕，再被收成 `GLsizei`。Direct3D 有同样的写法。探针里的 13 个索引和 12 个顶点不会走到这里。

下一刀还没定。采样器、混合、深度、多目标、立方体、三维、实例化、间接、计算、统一缓冲、查询都还不要开。ES 2 和 `glsl_es` 的成功编译也还不要做。那些平台上的 GameMaker 运行这台机器证明不了。

`gl-draw` 还在。本地和 `origin/gl-draw` 都是 `31c6982`，那是合并前的尖端，已经包含在 `edb0ccd` 里。

`gl-bind-current` 在合并拉取请求 #1 之后删过，这一会话又在 `fb4a9a5` 上建回来并推送。那是合并提交 `d662fb2` 的第二个父提交，改动已经在 `main` 里。不要为它再开拉取请求，也不要把它当成还没合并的工作。

已经落地的 OpenGL 只在探针里，Windows 的 GameMaker DLL 不含它：

- `igpu_bind_current()` 绑定调用线程上已经 current 的上下文，不创建、不销毁。Windows 这份 GameMaker 构建里它失败，错误是 `igpu_bind_current: this build has no OpenGL backend`。已经 `igpu_init` 的 D3D11 设备不会被这次失败丢掉。
- 探针在这块 AMD Radeon Vega 8 上画出 8×8 纹理，读回 `255 / 16711680 / 16711680`（左上红，右上蓝，左下蓝）。
- 静态 16 位索引缓冲可以创建。`IndexBuffer` 和 `DrawIndexed` 在探针上为 true，写法是 `native || opengl_backend()`。前 6 个索引只把左半边画成红 `255`，右半边保持蓝 `16711680`；从索引 6 起、`index_count = -1` 再把右半边画成红。`igpu_draw_indexed` 画进当前帧缓冲，不清屏。探针用 `gl_color_target_begin` 把 IGPU 纹理绑成当前目标。
- `igpu_draw_to_render_targets` 拒绝 `bind != 1` 的缓冲。索引缓冲的 stride 是 0，不拒绝的话范围检查会放行，`glDrawArrays` 会读过那 24 个字节。

非索引的 `igpu_draw` 已在探针里证明。`GlBackend::draw` 转到 `gl_draw`。它画进调用方已经 current 的帧缓冲，不清屏，不改帧缓冲绑定和视口，也不调用 `UseProgram`。三角形列表的前 6 个顶点只把左半边画成红 `255`，右半边保持清屏黑 `0`。从顶点 6 起、`vertex_count = -1` 再把右半边画成红，左半边仍是红。图元 `6` 和任何一个非 0 的状态句柄都被拒绝，像素保持 `0`。索引缓冲不能当成顶点缓冲。

动手前先在 `main` 上拉取，再跑下面三件事，确认树还是绿的：

```
pwsh -File tools\verify_handover.ps1
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

探针退出码 0，并打印这十行和 `PASS`：

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
```

可执行文件在 `src\Release` 下，不在预设目录的 `Release` 根上。

改了 DLL 才会再跑 GameMaker。在 `project\` 下：

```
cmake --build --preset win-x64-release-vs18 --target IGPU
node "D:\node.js\node_cache\_npx\166e0ec5f4c2d768\node_modules\@gamemaker\gm-cli\dist\cli.js" run --no-errors-only
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
| Git | 拉取请求 #3 的合并提交是 `0885e67`。写这份交接时 `origin/main` 就是它。`gl-vertex-colour` 在 `7c0c6e4`。拉取请求 #2 的合并提交是 `edb0ccd`。`gl-draw` 在 `31c6982`。`gl-bind-current` 在 `fb4a9a5`。 |

构建：在仓库根目录 `cmake --build --preset win-x64-release-vs18 --target IGPU`。DLL 会拷到 `project\extensions\IGPU\IGPU.dll`。

集成测试：在 `project\` 下执行

```
node "D:\node.js\node_cache\_npx\166e0ec5f4c2d768\node_modules\@gamemaker\gm-cli\dist\cli.js" run --no-errors-only
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
- `src/native/gl/igpu_gl_backend.cpp` 是 `GlBackend`。`draw_indexed` 已经转到 `gl_draw_indexed`。`draw` 转到 `gl_draw`。没实现的虚函数返回 0 或 false，错误以 `: the opengl backend does not implement this call yet` 结尾。
- `src/native/gl/igpu_gl_draw.cpp` 管缓冲、布局、纹理、绘制、读回。
- `src/CMakeLists.txt` 把核心源、`d3d11`、`gl` 和 `code_gen` 编进 `igpu_gl_probe`，并定义 `IGPU_HAS_OPENGL`、`NOMINMAX`、`WIN32_LEAN_AND_MEAN`。DLL 目标没有这些。

已经为 true 的能力：`ShaderCompileRuntime`、`ShaderStageVertex`、`ShaderStagePixel`、`Texture2D`、`InputLayout`、`VertexBuffer`、`IndexBuffer`、`Draw`、`DrawIndexed`、`DrawStateRestore`。`formats.surface_rgba8unorm` 为 true，另外七个格式为 false。`igpu_get_feature_level()` 在 GL 上是 `0`（`Unknown`）。没有 DXGI 适配器时，显存和后缓冲尺寸是 0，设备名来自 `GL_RENDERER`。方言是 `glsl`。

着色器是 `#version 120`。入口必须是 `main`。空方言和 `"glsl"` 可以编译。`"hlsl"` 被拒绝。顶点阶段和像素阶段都绑上之后才链成一个程序，属性 0 叫 `in_pos`，属性 1 叫 `in_colour`。不声明 `in_colour` 的程序仍然能链上。阶段绑错会失败，原来的绑定还在。`shader_bind(0, stage)` 解绑该阶段。

绘制只接受一个目标、layer 0、mip 0、四个状态句柄都是 0、图元 `4`（三角形列表）。图元 `6` 是扇形，拒绝且不改像素。布局接受一个 `float2` 位置（usage `1`，type `2`，step `0`，stride 0 或 8），也接受 float2 后面接一个 4 字节颜色（usage `2`，type `5`），缓冲步长必须显式写成 12。绘制按这个步长绑属性 0 和属性 1，返回前都关掉。其它布局和步长仍拒绝。顶点缓冲是静态的，一次写满。索引缓冲也是静态的，16 位，`bind` 为 2，stride 为 0，长度是 2 的倍数。`igpu_draw` 和 `igpu_draw_indexed` 都画进当前帧缓冲，不清屏。探针用 `gl_color_target_begin` 把 IGPU 纹理绑成当前目标，画完再还原。读回把 RGBA 收成 `r | (g << 8) | (b << 16)`，丢掉 alpha。`gl_row = height - 1 - y`，所以 `y = 0` 是裁剪空间的上方。画进纹理的 `igpu_draw_to_render_targets` 返回前恢复 `GL_FRAMEBUFFER_BINDING` 和 `GL_VIEWPORT`，不解开调用方绑好的程序。`igpu_draw` 和 `igpu_draw_indexed` 不动帧缓冲、视口和程序；它们恢复顶点数组和数组缓冲。索引绘制另外恢复元素数组缓冲。

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
