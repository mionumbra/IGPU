# 精简 IGPU 公开接口

日期：2026-10-06
状态：待审。审过之前不改 `spec.gmidl`，不改实现。

这份说明不放在 `docs/` 下。仓库根上的 `docs` 是 extgen 生成的文件，不是目录。

## 目的

IGPU 要给 GameMaker Studio 2 Runtime 一套完整的 GPU 能力。运行时的渲染后端主要是 D3D11 和 OpenGL（含 ES）。公开接口按这两个后端都能表达的 GPU 模型来定：一个概念一个函数。

这次只收入口。已经做出来的能力都留。几何、细分、实例化、间接绘制、三维、数组、立方体、多级、查询、时间戳、fence 都还在。

这次不写 OpenGL 后端，不加深度纹理，不加多槽纹理绑定。OpenGL 后端是收口之后的下一阶段。深度纹理和多槽绑定跟在它后面。HTML5/WASM 和主机更晚，见下一节。

## 平台现状（2026-10-06，对照 Runtime 源码）

源码树是 `D:\Users\User\Documents\gml_ext\OpenGM\runtime\GMS2-Runner-Main\VC_Runner`。这是去混淆后的派生树，不是官方发布。下面只写这份树里能对上文件的事实。

extgen 这份 schema 能生成的本机目标是 Windows、Linux、macOS、Android、iOS、tvOS，另外还有 Xbox、PS4、PS5、Switch 的槽位。`config.json` 现在只打开了 Windows。HTML5 不在 extgen 的目标列表里。

现在要定的 API 只覆盖 extgen 的桌面和移动目标：Windows、Linux、macOS、Android、iOS、tvOS。主机槽位先不打开。

已经实现的后端只有 Windows D3D11。`Files\Function\Win32\YoYo_FunctionsM.cpp` 的 `Os_Get_Info` 在 `USE_DX11_0` 下把 `GR_D3D_Device`、`GR_D3D_Context`、`g_SwapChain` 放进 `video_d3d11_device`、`video_d3d11_context`、`video_d3d11_swapchain`。这份 Windows 运行时没有 `wglCreateContext`。

Linux、macOS、Android、iOS、tvOS 的 `os_get_info` 不交上下文指针，只交 `glGetString` 那类字符串。上下文在游戏步进的那条线程上已经是 current，扩展函数跟着 GML 跑的时候可以直接用：

- Android：`DemoGLSurfaceView.java` 用 EGL10 建上下文，ES 2 走 `setEGLContextClientVersion(2)`，否则是 ES 1。`DemoRenderer.onDrawFrame` 调用 `RunnerJNILib.Process`。Java 的 `OsGetInfo` 只放 `Build` 字段，GL 字符串由 C++ 的 `AddGraphicsInfo` 补上。
- iOS / tvOS：`ES2Renderer.m` 建 `kEAGLRenderingAPIOpenGLES2`。`EAGLView.h` 定义了 `USE_METAL`，Metal 只把 OpenGL 画完的纹理显示出来。`renderTheScene` 先 `setCurrentContext`，再 `iPad_Process()`。`MTLDevice` 没有交给 GML。
- Linux：`TFormM.cpp` 用 `glXCreateContext` 建旧式 GLX 上下文，不请求版本。`glXMakeCurrent` 之后同线程进入 `MainLoop_Process`。
- macOS：`YYGLView.mm` 请求 `NSOpenGLProfileVersionLegacy`。`performGameStep` 先 `beginRender`，那里 `makeCurrentContext`。

因此 `igpu_init(device, context, swapchain)` 只描述 Windows 的三个指针。GL 平台要另有一个无指针的入口，绑定这条线程上已经 current 的上下文，不创建、不销毁它。这次把入口定下来。GL 后端的实现不在这次收口里。Windows 构建里这个入口返回失败，并写明这份构建没有 OpenGL 后端。

ES 2 上下文上，计算着色器、存储缓冲、细分、几何着色器、统一缓冲块和三维纹理不会为 true。立方体贴图在 ES 2 里有。多目标和实例化取决于扩展字符串。`glsl_es` 不带版本号。有 GL 后端时，它表示当前上下文的着色语言。Android、iOS、tvOS 这份源码里那是 GLSL ES 1.00。源码和上下文对不上时编译失败。

### 以后才做

1. HTML5 / WASM（含 GX.games）。扩展是 JavaScript，不能用 extgen，也不能加载现在的 C++ DLL。WebGL 上下文在 wasm 运行时里（`Emscripten\GameMakerM.cpp` 的 `emscripten_webgl_get_current_context`）。要单独写一份同名 API 的 JS 实现。排在本机 extgen 后端之后。
2. 主机：Xbox、PS4、PS5、Switch。排在 HTML5 / WASM 之后。extgen schema 里虽有这些槽位，现在不启用、不实现。这份树的 `Graphics.h` 在 `YYXBOX` 下有 D3D11 / D3D12 全局量，但没有看到把它们放进 `os_get_info` 的代码。不把旧文档里的 `video_d3d12_*` 键当成已经核对过的事实。

成功时公开函数从 105 个变成 59 个。同一张采样器描述、同一种纹理、同一次调度不再各占一串函数。像素读回里已经证明过的颜色和比较结果仍然成立。版本是 `0.4.0`。

仓库里会调用这些函数的只有 `project/objects/obj_igpu_test/Create_0.gml` 和 `project/scripts/IGPU_helpers/IGPU_helpers.gml`。签名可以破坏性地改。不留转发到旧名字的包装。

## 不改的结构

调用链仍是 `spec.gmidl` → extgen 的 `code_gen` → `IGPU_native.cpp` 的薄包装 → `igpu::` 门面 → `Backend` 虚函数 → `d3d11`。选后端的地方仍只有 `bind_device()`。

公开接口、公共头和 GML 里不出现 `d3d`、`dxgi`、`_5_0`、`ID3D`。方言提示仍是 `hlsl`、`glsl`、`glsl_es`、`msl`、`spirv`。枚举和常量继续用 GameMaker 已有的名字：`bm_*`、`cmpfunc_*`、`cull_*`、`tf_*`、`surface_*`，加上已有的 `Igpu*` 枚举。

能力键仍是 35 个。某个能力对应的行为还在，键就保持为 true。拿掉诊断函数不把 `draw_state_restore` 改成 false。收成一个反射函数不把 `uniform_reflection` 改成 false。

设备仍是 AddRef 一次、只 Release 这一次。`reset()` 先放下当前后端。门面在虚函数之前做 `require_device`。虚函数不写默认参数。GameMaker 颜色仍在门面拆成 0 到 1 的红、绿、蓝。做不到的值让这次调用失败。

`Backend` 的虚函数收成和下面的公开函数同一套。D3D11 填一张采样器描述、一条纹理创建、一次调度。不再保留十三个采样器虚函数，也不在 C++ 里留旧入口的转发。

## 留下的 59 个函数

下面没列出的现有函数删除。参数全部必填。extgen 不接受 `double`，级数用 `float`。

### 原样留下（46）

生命周期：`igpu_init`、`igpu_shutdown`、`igpu_version`、`igpu_is_available`、`igpu_device_lost`。

设备信息：`igpu_get_feature_level`、`igpu_get_adapter_description`、`igpu_get_video_memory`、`igpu_get_backbuffer_width`、`igpu_get_backbuffer_height`。

能力：`igpu_get_capabilities`、`igpu_supports`、`igpu_get_shader_dialect`、`igpu_set_graphics_info`。

着色器：`igpu_shader_compile`、`igpu_shader_release`、`igpu_shader_bind`、`igpu_get_bound_shader`、`igpu_get_last_error`。

资源释放和缓冲：`igpu_input_layout_release`、`igpu_buffer_create`、`igpu_buffer_write`、`igpu_buffer_read`、`igpu_buffer_resize`、`igpu_buffer_size`、`igpu_buffer_release`、`igpu_storage_bind`。

常量写入：`igpu_uniform_write`、`igpu_uniform_bind`。

状态：`igpu_blend_state_create`、`igpu_depth_state_create`、`igpu_raster_state_create`、`igpu_state_release`。

纹理与同步：`igpu_texture_generate_mips`、`igpu_texture_release`、`igpu_query_create`、`igpu_query_begin`、`igpu_query_end`、`igpu_query_ready`、`igpu_query_result`、`igpu_query_release`、`igpu_timestamp_frequency`、`igpu_fence_create`、`igpu_fence_signal`、`igpu_fence_signaled`、`igpu_fence_release`。

这些函数的参数和现在相同。

### 收成一个的函数（13）

`igpu_input_layout_create(shader, usage, type, step, element_count, vertex_stride, instance_stride)`

`step[i]` 是 `IgpuVertexStep`。`Vertex` 每顶点走一格，`Instance` 每实例走一格。`vertex_stride` 或 `instance_stride` 为 0 或负数时，该缓冲按元素紧凑排列。没有 `Instance` 元素时 `instance_stride` 传 0。`step` 的长度必须等于 `element_count`。

`igpu_sampler_state_create(magnification, minification, mip, address_u, address_v, address_w, anisotropy, border, compare, level_offset, finest, coarsest)`

`igpu_texture_create(kind, width, height, depth, format, render_target, storage, mip_count)`

`igpu_texture_read(texture, x, y, layer, mip)`

`igpu_dispatch(groups_x, groups_y, groups_z, kinds, targets, layers, mips)`

`igpu_shader_reflect(shader) : gmval`

`igpu_draw(vertex_buffer, instance_buffer, layout, primitive, first_vertex, vertex_count, instance_count, blend_state, depth_state, raster_state, sampler_state)`

`igpu_draw_indexed(vertex_buffer, layout, index_buffer, primitive, first_index, index_count, blend_state, depth_state, raster_state, sampler_state)`

`igpu_draw_indirect(vertex_buffer, instance_buffer, layout, primitive, args, args_offset, blend_state, depth_state, raster_state, sampler_state)`

`igpu_draw_indexed_indirect(vertex_buffer, instance_buffer, layout, index_buffer, primitive, args, args_offset, blend_state, depth_state, raster_state, sampler_state)`

`igpu_draw_patch(vertex_buffer, layout, control_points, first_vertex, vertex_count, blend_state, depth_state, raster_state, sampler_state)`

`igpu_draw_to_render_targets(vertex_buffer, layout, primitive, first_vertex, vertex_count, targets, layers, mips, blend_state, depth_state, raster_state, sampler_state)`

`igpu_draw_sampled(vertex_buffer, layout, primitive, first_vertex, vertex_count, texture, blend_state, depth_state, raster_state, sampler_state)`

索引绘制不加实例参数。现在没有索引实例化这条能力，这次不加。

新增参数沿用现有绘制函数的类型。状态句柄、`instance_count`、`first_*` 和 `*_count` 用 `int64` 提示。枚举、层、级数和数组长度用 `int32`。三个并行数组用 `array`。

## 哨兵和校验

采样器：

- `magnification`、`minification`、`mip` 只能是 `tf_point` 或 `tf_linear`。
- `anisotropy` 是 1 到 16。大于 1 时三种过滤必须全是 `tf_linear`，否则失败。大于 1 表示各向异性过滤。D3D11 的各向异性就是线性过滤加各向异性倍数，这样写两边都能实现。
- 三轴寻址是 `IgpuAddressMode`。没有任何一轴是 `Border` 时，`border` 忽略，传 0 即可。有一轴是 `Border` 时，`border` 是不透明的 GameMaker 颜色，门面拆成通道再交给后端。
- `compare` 为 0 表示普通采样。1 到 8 是 `cmpfunc_*`（`cmpfunc_never` 是 1）。其它值失败。比较和边框寻址同时出现则失败。通过是 1，不通过是 0。
- `level_offset` 必须是有限数。正数往更粗的级走。着色器自己写明级数时不加这个偏移，这是采样指令的差异，采样器上仍然记下偏移。
- `finest` 必须是有限数并且大于等于 0，0 是原图。`coarsest` 小于 0 表示不限制最粗。`coarsest` 大于等于 0 时必须有限，并且大于等于 `finest`。先加偏移，再留在这两级之间。着色器自己写明的级数不加偏移，但仍留在这个范围里。

纹理：

- `kind` 与 `depth` 的规则不变。立方体必须是正方形，`depth` 为 6，面序 +X、−X、+Y、−Y、+Z、−Z。
- `mip_count` 为 1 是只有一级。为 0 时分配到 1×1 的整条链。其它值必须在 1 和整条链长度之间。
- 多于一级的纹理可以当渲染目标，和现在一样。`render_target` 为 true 的单级纹理也可以。
- `storage` 为 true 且后端没有可写图像时，调用失败。
- `igpu_texture_read` 只读 `surface_rgba8unorm`，丢掉 alpha。`layer` 和 `mip` 的范围规则与现在的按级读回相同。

调度：

- `kinds`、`targets`、`layers`、`mips` 等长，长度 1 到 8。`kinds[i]` 是 `IgpuWriteTarget`。槽 i 就是这一项。
- 缓冲项的 `layers[i]` 和 `mips[i]` 必须是 0。纹理项的 `mips[i]` 选择级数，视图覆盖这一级的全部分片，由着色器选择写哪一层或哪一面。这就是现在的 `igpu_dispatch_level`。纹理项的 `layers[i]` 必须是 0，这次不加「只绑定一层」的写入视图。
- 同一张纹理或同一个缓冲不能出现两次。用过的槽在返回前恢复。

绘制：

- 四个状态句柄为 0 时不动该阶段。种类不对则失败，不留下半套状态。
- `instance_buffer` 为 0 且 `instance_count` 为 1 时就是原来的单次绘制。`instance_count` 小于 1 失败。
- `igpu_draw_sampled` 把纹理和采样器绑到槽 0，返回前恢复。`texture` 为 0 失败。
- `igpu_draw_to_render_targets` 的 `targets`、`layers`、`mips` 等长，1 到 4 张。层、级数、同一张纹理能否重复的规则与现在的按层按级多目标相同。画一张二维纹理的第 0 级时，`targets` 放这张纹理，`layers` 和 `mips` 放 0。
- `pr_trianglefan` 仍然拒绝。输入装配、混合、深度、光栅、采样器都在返回前恢复。间接绘制和面片也恢复这四类状态。

反射：

`igpu_shader_reflect` 成功时返回：

```
{
    blocks: [
        {
            name: "",
            size: 0,
            slot: 0,
            members: [
                {
                    name: "",
                    offset: 0,
                    size: 0,
                    type: 0,
                    rows: 0,
                    columns: 0,
                    elements: 0
                }
            ]
        }
    ]
}
```

`size`、`offset`、`type`、`rows`、`columns`、`elements` 与现在的逐项查询相同。没有请求槽时 `slot` 为 −1。没有常量块时 `blocks` 是空数组，并且 `igpu_get_last_error()` 为空。失败时 `blocks` 也是空数组，同时设下错误。编码走和 `igpu_get_capabilities()` 一样的结构体路径：字符串用 `string_view`，不把 `StructStream` 按值当成 `DataStream` 返回。

`igpu_uniform_write` 和 `igpu_uniform_bind` 的打包、列主序、`float` 数组步长不变。调用方用反射出来的块名和成员名。

## 从测试里删掉的入口

这些名字从 `spec.gmidl`、生成代码、门面、`Backend` 和测试里删除，不留同名转发：

- `igpu_shader_compile_vertex`、`igpu_shader_compile_pixel`、`igpu_shader_compile_compute`
- `igpu_input_layout_create_step`
- `igpu_shader_block_count`、`igpu_shader_block_name`、`igpu_shader_block_size`、`igpu_shader_block_slot`
- `igpu_shader_member_count`、`igpu_shader_member_name`、`igpu_shader_member_offset`、`igpu_shader_member_size`、`igpu_shader_member_type`、`igpu_shader_member_rows`、`igpu_shader_member_columns`、`igpu_shader_member_elements`
- 全部 `igpu_sampler_state_create_*` 后缀函数
- `igpu_texture_create_kind`、`igpu_texture_create_mips`、`igpu_texture_get_pixel`、`igpu_texture_read_level`
- `igpu_draw_instanced`、`igpu_draw_with_state`、`igpu_draw_indexed_with_state`
- `igpu_draw_to_texture`、`igpu_draw_to_texture_layer`、`igpu_draw_to_texture_level`
- `igpu_draw_to_render_targets_level`、`igpu_draw_to_render_targets_layer`
- `igpu_dispatch_level`、`igpu_dispatch_buffer`、`igpu_dispatch_both`、`igpu_dispatch_writes`
- `igpu_get_draw_count`、`igpu_get_draw_restore_failures`、`igpu_is_vertex_buffer_bound`

旧的「过滤 + 是否重复 + 各向异性」采样器在测试里改写成三轴寻址、三种过滤和 `anisotropy`。点采样且钳制是三种 `tf_point`、三轴 `Clamp`、`anisotropy` 1。各向异性是三种 `tf_linear` 加上 2 到 16 的倍数。

## 测试和文档

`Create_0.gml` 改成只调用留下的函数。已有的像素期望保持不变：清屏色、半边颜色、多级、各向异性、边框、比较（左黑右红、交界红 128）、几何、细分、间接、查询。只为已删除诊断函数存在的检查删掉。反射检查改读 `igpu_shader_reflect` 的结构体，偏移和大小的期望不变。

集成测试仍是 `project/` 下的 `gm-cli run --no-errors-only`。通过时 `checks failed: 0`，退出码 0。

`HANDOVER.md` 的函数清单、能力说明和「当前检查项 **N 项**」改成收口后的数。`N` 是 `_igpu_check(` 的调用点，去掉函数定义那一行。`SESSION_HANDOVER.md` 开头改成收口已完成，下一阶段再写。`tools/verify_handover.ps1` 继续要求文档里的每个函数都在 spec 里，并且 59 这个数对得上。版本字符串是 `0.4.0`。

`IGPU_helpers.gml` 只做迁移：`igpu_vertex_format` 给每个元素填 `IgpuVertexStep.Vertex`，`instance_stride` 传 0。`igpu_draw_buffer` 传 `instance_buffer` 0、`instance_count` 1、四个状态句柄 0。测试直接调用原生函数，不靠 helper 掩盖签名。

改 `spec.gmidl` 之后跑 `D:\GM-ExtensionGenerator\extgen.exe --config config.json`，再按 `win-x64-release-vs18` 编 `IGPU`。MSVC 编译的 cpp 注释只用 ASCII。

## 不做

- 不实现 OpenGL 或 GLES 后端。
- 不加深度纹理，不加按槽绑定纹理或采样器。`igpu_draw_sampled` 仍只使用槽 0。
- 不加索引实例化，不加新的着色器阶段。
- 不改能力枚举的数值，不改 35 个键的名字。
- 不把测试对象移出工程，不清理 `third_party` 里的 Discord SDK 模板。
- 不升高版本号。
- 用户没有明确要求时不提交。
