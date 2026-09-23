# IGPU — 跨平台抽象层设计

> 状态：**阶段 A/B/C 已实现并验证；阶段 D 进行中**
> 目标读者：IGPU 实现者 / 上层扩展调用方
> 依据：`GmlSpec.xml`（runtime-2026.0.0.23）、离线官方手册、`os_get_info()` 全平台键位表
>
> 本文回答一个问题：**在只有 Windows 能真跑的前提下，接口该怎么设计，才能让跨平台不是重写？**
>
> 📌 **现状与操作请看 `HANDOVER.md`**；本文是设计论证，两者互补。

---

## 0. 设计立场

用户原话：

> 尽管我们可能只能支持 Windows，但是我们需要考虑其他平台渲染的，综合思考来决定这个扩展的功能。
> 这是为未来可能的跨平台做准备，**可以不搞，但不能没有**。

这句话直接决定了两条原则：

1. **接口层必须平台无关** —— 因为接口一旦被调用方使用，改起来就是破坏性变更。
2. **实现层可以平台特定** —— 因为只有 Windows 有设备句柄，硬做别的平台是浪费。

所以本文的重点不是"怎么在 OpenGL 上实现 compute"，而是**怎么让 `spec.gmidl` 里没有一个 D3D 术语**。

---

## 1. 核心发现：GameMaker 自己已经给出了答案

在设计之前，我先把 `GmlSpec.xml` 里 GM 自己的 GPU 词汇表拉出来看。**这个发现改变了整个设计方向。**

### 1.1 GM 的格式命名是 DXGI 风格，但平台无关

```
surface_rgba8unorm     surface_rgba16float
surface_r8unorm        surface_rgba32float
surface_rg8unorm       surface_r16float
surface_rgba4unorm     surface_r32float
```

这些名字**直接来自 DXGI 格式枚举**（`DXGI_FORMAT_R8G8B8A8_UNORM` → `surface_rgba8unorm`）。但它们被用在 `surface_create_ext()` / `surface_format_is_supported()` 上 —— **在 macOS/Linux/Android 上同样有效**，由 GM 内部翻译成 GL/GLES 格式。

这是个**极其重要的先例**：GM 自己就是用"DXGI 风格的名字 + 平台无关的语义"来做跨平台的。

### 1.2 GM 的顶点/状态词汇同样是中立的

```
vertex_usage_position | vertex_usage_normal | vertex_usage_texcoord
vertex_usage_tangent  | vertex_usage_binormal | vertex_usage_blendweight
vertex_usage_colour   | vertex_usage_psize | vertex_usage_sample

cmpfunc_less | cmpfunc_greaterequal | cmpfunc_always ...   ← 比较函数
cull_clockwise | cull_counterclockwise | cull_noculling    ← 剔除模式
bm_src_alpha | bm_inv_src_alpha | bm_eq_add ...            ← 混合
```

这些**都不是 D3D 专有概念**，它们在任何图形 API 里都有对应物。

### 1.3 结论

> **GM 已经建立了"平台无关的 GPU 词汇表"，IGPU 应该复用它，而不是发明新词。**

这带来三个具体好处：
- 调用方学习成本低（GML 程序员已经认识 `surface_rgba8unorm`）
- 语义已被 GM 官方验证过是跨平台可行的
- 避免 IGPU 自己定义一套词汇，将来和 GM 冲突

**唯一例外**：着色器编译目标（`vs_5_0`）是纯 D3D 术语，GM 词汇表里**没有**对应物 —— 因为 GM 从不在运行时编译着色器。这是 IGPU 必须自己设计的部分，见 §4。

---

## 2. 早期 API 的跨平台语义审查

> **注**：本节写于阶段 A 之前，当时是 **14 个 API**、编译参数还是 `"vs_5_0"`。
> 这些污染**均已在阶段 A 修复**（现为 **20 个函数**，见 `HANDOVER.md` §4）。
> 保留本节是因为它记录了**判断标准**——新增 API 时应继续用同一把尺子量。

当前 `spec.gmidl` 的实现全部基于 D3D11。逐个检查接口层面是否有平台污染：

### 2.1 有污染 —— 必须改

| API | 问题 | 严重度 |
|---|---|---|
| `igpu_shader_compile_vertex/pixel/compute(source, entry, target)` | `target = "vs_5_0"` 是 **D3D 着色器模型字符串**。跨平台时 GL 用 `#version 330`、Metal 用 MSL、WebGL 用 GLSL ES。把 D3D profile 暴露给调用方，等于把 D3D 焊死在接口上。 | 🔴 高 |
| `igpu_init(device, context, swapchain)` | 三个参数是 **D3D11 特有**的（Xbox 是 D3D12 且无 swapchain；GL 平台什么都没有）。接口形状本身编码了 D3D11 的设备模型。 | 🔴 高 |
| `igpu_get_feature_level()` | 返回 `D3D_FEATURE_LEVEL_*`（11_0/11_1/12_0/12_1），**这是 D3D 专有概念**。GL 没有 "feature level"，只有版本号（4.5 / ES 3.1）。 | 🟡 中 |

### 2.2 无污染 —— 可保留

| API | 说明 |
|---|---|
| `igpu_version()` / `igpu_get_last_error()` | 纯字符串，天然无关 |
| `igpu_is_available()` | 纯 bool |
| `igpu_shutdown()` | 无语义 |
| `igpu_shader_release(shader)` | 句柄是 `uint64`，与后端无关 ✅ |
| `igpu_get_adapter_description()` | 字符串。注意 Xbox 返回 `""`、GL 平台可取 `gl_renderer_string` |
| `igpu_get_video_memory()` | `int64`。注意 Xbox 返回 0、GL 平台无此概念 |
| `igpu_get_backbuffer_width/height()` | 平台无关概念 ✅（但实现有 bug，见 §6.3） |

### 2.3 关键判断

**❌ 不要这样做**（把 D3D 泄漏到 GML 层）：
```gmidl
// 错误示范：调用方被迫知道 D3D 设备模型
function igpu_create_buffer(device : gmval, byteWidth : int32, bindFlags : int32) : int64;
```

**✅ 应该这样做**（用 GM 已有的中立词汇）：
```gmidl
// 正确示范：语义清楚，后端自己翻译
function igpu_buffer_create(size : int64, usage : IgpuBufferUsage) : int64;
```

---

## 3. 分层架构（修正版）

HANDOVER §3 提出的 Tier 1/2/3 方向正确，但**边界需要重新划**。原方案的 Tier 2 说"OpenGL 平台降级——能力探测 + 纹理格式查询，渲染走 GM 自有着色器管线"，这个说法有个隐含矛盾：**如果渲染还是走 GM 管线，那 Tier 2 和 Tier 3 的区别是什么？**

重新划分如下：

```
┌─────────────────────────────────────────────────────────────┐
│ Tier 3 — 能力抽象层（所有平台，纯 GML）                        │
│   igpu_get_capabilities() / igpu_supports(feature)          │
│   目的：让上层代码能优雅降级，永远不 crash                     │
│   实现：100% GML，无原生代码，所有平台一致                     │
├─────────────────────────────────────────────────────────────┤
│ Tier 2 — 查询与反射层（所有有 GPU 信息的平台）                 │
│   适配器信息、显存、格式支持、着色器编译能力探测                │
│   数据来源：os_get_info() 的 gl_* / GL_* / video_adapter_*    │
│   实现：各平台少量原生代码，或纯 GML                         │
├─────────────────────────────────────────────────────────────┤
│ Tier 1 — 真实 GPU 后端（仅 Windows / Xbox）                   │
│   运行时着色器编译、compute、MRT、状态对象、查询              │
│   实现：D3D11（Win）/ D3D12（Xbox）                          │
└─────────────────────────────────────────────────────────────┘
```

**关键改进**：Tier 3 是**纯 GML**，不是原生代码。这样它在**所有平台**（含 HTML5）都能工作，真正做到"让上层优雅降级"。原方案把 Tier 3 描述成需要原生实现，会让它在 HTML5 上失效。

---

## 4. 着色器抽象（最难的部分）

这是全项目**最关键的设计决策**，因为：
- GM 完全不暴露运行时着色器编译 → 这是 IGPU 的核心价值
- 但它也是**最难跨平台**的：每个后端吃不同的着色器语言
- 而它现在已经被 `"vs_5_0"` 污染了

### 4.1 问题

```gmidl
function igpu_shader_compile_vertex(source : string, entry : string, target : string) : int64;
//                                                              ^^^^^^
//                                      调用方必须传 "vs_5_0" —— 纯 D3D 概念
```

如果将来加 Vulkan 后端，调用方要改成传 `"vert"`（SPIR-V 入口）或根本不同的参数。**这是破坏性变更。**

### 4.2 设计：分离"阶段"与"目标"

**阶段（stage）是平台无关的** —— 任何图形 API 都有 VS/PS/CS：

```gmidl
enum IgpuShaderStage
{
    Vertex      = 0,   // 顶点
    Pixel       = 1,   // 像素/片元（GL 叫 fragment，同一概念）
    Compute     = 2,   // 计算
    Geometry    = 3,   // 几何（D3D 有，GL 有，Metal/WebGL 无）
    Hull        = 4,   // 曲面细分控制（D3D）/ tess control（GL）
    Domain      = 5,   // 曲面细分评估 / tess eval
    Mesh        = 6,   // 网格着色器（D3D12 / Vulkan，GL 无）
    Amplification = 7  // 放大着色器（D3D12 称 amplification，Vulkan 称 task）
}
```

**注意**：新增的 3-7 是**接口占位**。Windows D3D11 只支持 0/1/3/4/5，Xbox D3D12 支持全部，GL 平台按版本部分支持。枚举里先列出来，用 `igpu_supports()` 查询，**这正是"可以不搞，但不能没有"的体现**。

**目标（target）降级为可选提示**，不再要求调用方填 D3D profile：

```gmidl
function igpu_shader_compile(
    source : string,
    entry  : string,
    stage  : IgpuShaderStage,
    [type_hint = `string`] dialect = ""   // 可选，"" = 自动
) : int64;
```

- `dialect = ""` → 后端按 `stage` 自动选（Windows → `vs_5_0`/`ps_5_0`/`cs_5_0`）
- `dialect = "hlsl"` / `"glsl"` / `"msl"` / `"spirv"` → 显式指定源语言（高级用法）

这样：
- **现在**：`igpu_shader_compile(src, "main", IgpuShaderStage.Vertex)` — 干净，无需知道 D3D
- **将来**：加 GL 后端时，接口不变，只是后端换翻译器

### 4.3 三阶段迁移路径

| 阶段 | 动作 | 破坏性 |
|---|---|---|
| **现在** | 保留 `igpu_shader_compile_vertex/pixel/compute` 三个函数（兼容），**新增**统一的 `igpu_shader_compile` | 无 |
| **过渡** | 三个旧函数标记 deprecated，内部转调新函数 | 无 |
| **未来** | 移除旧函数，或永久保留为便捷包装 | 有（可选） |

**建议**：现在就用新接口，旧三个函数保留为**便捷包装**（内部 `return igpu_shader_compile(src, entry, IgpuShaderStage.Vertex)`）。因为当前只有测试对象在用，改造成本几乎为零 —— **这是重构的最佳时机**。

### 4.4 各后端着色器语言映射

| 平台 | 语言 | 编译方式 | 可得性 |
|---|---|---|---|
| Windows (D3D11) | HLSL | `D3DCompile` (d3dcompiler_47) | ✅ 已实现 |
| Xbox (D3D12) | HLSL | `D3DCompile` / 预编译 DXIL | ⚠️ 需验证 |
| macOS (GL) | GLSL | 无运行时编译（无 `HGLRC`） | ❌ 不可行 |
| macOS (未来 Metal) | MSL | 需 `MTLDevice` | ❌ 拿不到 |
| Linux/Android (GL/GLES) | GLSL | 无运行时编译（无 `EGLContext`） | ❌ 不可行 |
| HTML5 (WebGL) | GLSL ES | 扩展只能用 `.js` | ❌ 不可行 |

**残酷的现实**：§4.4 表明，运行时着色器编译**在非 Windows/Xbox 平台上根本无法实现**，因为我们拿不到图形上下文。

**但这不改变接口设计**。因为：
1. `igpu_shader_compile` 在那些平台返回 `0` 并设错误信息
2. `igpu_supports(IgpuCapability.ShaderCompileRuntime)` 返回 `false`
3. 调用方据此走 GM 自有着色器管线

**接口统一，能力可查，降级优雅** —— 这就是"可以不搞，但不能没有"。

---

## 5. `igpu_get_capabilities()` 契约（Tier 3）

这是让跨平台可行的**机制核心**。

### 5.1 为什么用 struct 而不是多个 bool 函数

```gml
// ❌ 不好：调用方要问 20 次，且新增能力要加函数
if (igpu_supports_compute()) { ... }
if (igpu_supports_mrt()) { ... }

// ✅ 好：一次拿到全部，缓存起来，新增能力不破坏接口
var _caps = igpu_get_capabilities();
if (_caps.compute) { ... }
```

### 5.2 返回结构（GML struct）

```gml
{
    // ---- 后端标识 ----
    backend        : "d3d11",     // "d3d11" | "d3d12" | "opengl" | "gles" | "webgl" | "none"
    tier           : 1,           // 1 | 2 | 3
    device_name    : "...",       // 适配器描述
    shader_dialect : "hlsl",      // 后端原生着色器语言

    // ---- 着色器能力 ----
    shader_stages  : ["vertex", "pixel", "compute"],   // 支持的阶段
    compute        : true,
    geometry       : false,
    tessellation   : false,
    mesh_shader    : false,
    runtime_compile: true,        // 能否运行时编译着色器
    spirv          : false,

    // ---- 资源能力 ----
    max_texture_2d : 16384,
    max_texture_3d : 2048,
    texture_3d     : true,
    texture_array  : true,
    texture_cubemap: true,
    max_render_targets : 8,       // MRT 上限
    max_vertex_streams : 16,
    max_compute_threads: [1024, 1024, 64],
    structured_buffer  : true,
    uav                : true,

    // ---- 特性能力 ----
    instancing     : true,
    indirect_draw  : true,
    queries        : true,
    timestamps     : true,
    occlusion_query: true,
    fence          : true,
    conservative_raster : false,
    wireframe      : true,

    // ---- 格式支持（复用 GM 词汇表）----
    formats        : { "surface_rgba16float": true, "surface_r32float": true, ... }
}
```

### 5.3 实现策略

| Tier | 实现方式 |
|---|---|
| **Tier 1 (Win)** | 原生：`D3D11_FEATURE_DATA_*` 查询 + `CheckFormatSupport` + `D3D11_REQ_*` 常量 |
| **Tier 2 (GL)** | 原生少量：解析 `os_get_info()` 的 `GL_*` 字符串 + `glGetIntegerv` 若可达 |
| **Tier 3 (所有)** | 纯 GML：`os_type` 判断 + 保守默认值（全 false，安全降级） |

**关键设计**：Tier 3 是**兜底**。在没有原生实现的平台上，`igpu_get_capabilities()` 返回一个"全 false"的结构，调用方自然降级。**这保证了 HTML5 上调用不 crash。**

---

## 6. 能力映射表

各平台能力对照（✅ 可行 / ⚠️ 受限 / ❌ 不可行）：

| 能力 | D3D11 (Win) | D3D12 (Xbox) | OpenGL (mac/Linux) | GLES (Android/iOS) | WebGL (HTML5) |
|---|---|---|---|---|---|
| **设备句柄可得** | ✅ 三个指针 | ✅ 三个指针（无 swapchain） | ❌ 仅字符串 | ❌ 仅字符串 | ❌ 返回 -1 |
| 适配器信息 | ✅ DXGI | ⚠️ 仅 description，其余为 0 | ✅ `gl_renderer_string` | ✅ `GL_RENDERER` | ✅ `gl_renderer_string` |
| 显存查询 | ✅ | ❌ 返回 0 | ❌ | ❌ | ❌ |
| **运行时着色器编译** | ✅ | ⚠️ 待验证 | ❌ 无 context | ❌ 无 context | ❌ 仅 .js |
| Compute shader | ✅ | ✅ | ❌ | ❌ | ❌ |
| 几何/细分着色器 | ✅ | ✅ | ⚠️ GL 3.2+ | ❌ | ❌ |
| Mesh shader | ❌ | ✅ | ❌ | ❌ | ❌ |
| MRT | ✅ | ✅ | ⚠️ GL 2.0+ | ⚠️ 部分 | ⚠️ WEBGL_draw_buffers |
| 3D 纹理 | ✅ | ✅ | ✅ | ✅ | ⚠️ WebGL2 |
| 纹理数组 | ✅ | ✅ | ✅ | ✅ | ⚠️ WebGL2 |
| 实例化 | ✅ | ✅ | ⚠️ GL 3.1+ | ⚠️ GLES 3.0+ | ⚠️ ANGLE_instanced_arrays |
| 间接绘制 | ✅ | ✅ | ⚠️ GL 4.0+ | ⚠️ GLES 3.1+ | ❌ |
| 查询/时间戳 | ✅ | ✅ | ⚠️ 部分 | ⚠️ 部分 | ❌ 需 EXT |
| 结构化缓冲 | ✅ | ✅ | ⚠️ GL 4.3+ | ⚠️ GLES 3.1+ | ❌ |
| UAV | ✅ | ✅ | ❌ | ❌ | ❌ |

> **读法**：这张表的重点是**右下角大片红色**。它证明"HANDOVER 说只有 Windows/Xbox 能做高级功能"是**正确的**，跨平台设计的目的不是让红色变绿，而是**让调用方知道哪里是红的**。

---

## 7. 需要修复的现有问题

> **状态（已核实）**：7.1 / 7.2 / 7.3 / 7.5 在阶段 C **均已修复**；
> 7.4 **不是 bug**（平台如此），已由能力层如实上报。
> 本节保留原始分析作为记录。

设计审查过程中发现的实现问题（与跨平台无关，但应一并修）：

### 7.1 `refresh_backbuffer_size()` 用了陈旧的尺寸 ✅ 已修复

原实现用 `GetDesc()`，返回的是 **swapchain 创建时**的尺寸；窗口拉伸或切全屏后
backbuffer 会变，但那里永远返回旧值。

**修复**：已改为优先用 `GetDesc1()`（DXGI 1.1+），取不到再回退到 `GetDesc()`。
见 `igpu_device.cpp` 的 `refresh_backbuffer_size()`。

### 7.2 `igpu_init_from_game()` 未销毁 DS Map ✅ 已修复

`os_get_info()` 返回的 DS Map **不会自动释放**，手册明确警告要用 `ds_map_destroy()`。
原实现每次调用泄漏一个 map。

**修复**：`IGPU_helpers.gml` 的两个返回分支**都已**补上 `ds_map_destroy()`。

### 7.3 `igpu_init_from_game()` 未防御 `-1` 返回值 ✅ 已修复

手册（已核实）：

> **HTML5** Returns `-1`.
> **Nintendo Switch** Returns `-1`.

在 HTML5/Switch 上 `_info` 是 `-1`（**不是 map**），对 `-1` 调 `ds_map_exists()`
行为未定义。

**修复**：现有代码是 `if (!is_real(_info) && !ds_map_exists(...))` ——
用 `is_real()` 先挡掉 `-1`，且该分支内也调用了 `ds_map_destroy()`
（`-1` 上调用是安全的，因为此时必然不是 map）。

### 7.4 Xbox 上的返回值语义 —— **不是 bug**

手册（已核实）：

> the `video_adapter_*` and `udid` keys are `0` (except for `video_adapter_description` which is an empty string `""`).

所以在 Xbox 上：
- `igpu_get_adapter_description()` → `""`
- `igpu_get_video_memory()` → `0`

**这是平台如此，不是缺陷。** 但调用方需要知道 —— 这正是 `igpu_get_capabilities()`
要解决的问题（Xbox 上 `AdapterInfo` / `VideoMemory` 仍会报 true，因为 Xbox 确实
有适配器，只是 GM 不上报数值；见 `igpu_capabilities.cpp` 的注释）。

### 7.5 `igpu_debug_trace()` 写文件到 CWD ✅ 已移除

原实现往 CWD 写 `igpu_native_trace.txt` 并调 `OutputDebugString`。
**已从代码中移除**（`grep igpu_debug_trace` 无结果）。

---

## 8. 实施路线

按"接口优先"原则排序：

### 阶段 A — 接口定型（不改行为，零风险）
1. ✅ 本文档
2. ✅ 重构 `spec.gmidl`：
   - 新增 `igpu_shader_compile(source, entry, stage, dialect="")` 统一接口
   - 旧三个函数改为便捷包装
   - 扩展 `IgpuShaderStage` 枚举到 8 个阶段
   - 新增 `IgpuCapability` 枚举
3. ✅ 重跑 extgen，确认生成物正常

### 阶段 B — 能力层（Tier 3）
4. ✅ 实现 `igpu_get_capabilities()` — Windows 走原生查询，其他平台纯 GML 兜底
5. ✅ 实现 `igpu_supports(capability)` 便捷函数

### 阶段 C — 修复现有缺陷
6. ✅ §7.1 backbuffer 尺寸（改用 `GetDesc1()`）
7. ✅ §7.2 DS Map 泄漏（补 `ds_map_destroy()`）
8. ✅ §7.3 `-1` 返回值防御
9. ✅ §7.5 移除 debug trace

### 阶段 D — 补全 Tier 1（Windows）
10. ✅ **shader 句柄保留 `ID3DBlob`** ← 见下方说明
11. ✅ **输入布局**（`ID3D11InputLayout`）+ 顶点格式
12. ✅ 缓冲区（`ID3D11Buffer`）+ 上传 / 读回
13. ⬜ 绘制调用 + 实例化
14. ⬜ 状态对象（depth-stencil / rasterizer / blend / sampler）
15. ⬜ MRT
16. ⬜ 纹理 / SRV / RTV / UAV
17. ⬜ 查询 / 时间戳 / fence
18. ⬜ 常量缓冲区反射（`D3DReflect`），自动打包 `cbuffer` 布局

> **关于第 10 步（已完成）**：`DeviceState::shaders` 原先只存 `ID3D11DeviceChild*`，
> **丢弃了 `ID3DBlob`**。而 `CreateInputLayout` 必须用编译产物里的 signature，
> 所以这一步是输入布局（以及后续所有绘制）的**硬前置**。
>
> 现结构：`ShaderEntry { ID3D11DeviceChild* object; ID3DBlob* bytecode; }`。
> 注意 blob 有三个所有权出口（编译失败 / 建对象失败 / 成功移交），改动时别漏。

---

## 9. 设计约束（务必遵守）

1. **绝不 Release GM 的设备** —— `DeviceState::reset()` 已正确注释，继续保持。
2. **接口零 D3D 术语** —— 审查任何新 API，GML 层面不得出现 `d3d`/`dxgi`/`_5_0`/`ID3D` 等字样。
3. **复用 GM 词汇表** —— 格式、usage、比较函数等，用 GM 已有的中立常量名。
4. **能力可查，降级优雅** —— 任何非 Windows 平台调用 IGPU 必须返回失败而非 crash。
5. **不与 GM 状态机打架** —— 改变全局管线状态后应恢复（GM 有 `gpu_get_state`/`gpu_set_state`）。
6. **句柄模式统一** —— 延续 `unordered_map<uint64, 资源*>` + 自增 ID。

---

## 10. 未决问题

1. **`.gmcache` 是否该进仓库？** 当前有 `.gitignore`（内容 `*`）自保护，但仍在磁盘上占 ~700 文件。
2. **Xbox D3D12 后端可行性**：`os_get_info()` 给的是 `cmdqueue`/`cmdlist`/`currentrt`，与 D3D11 的 `device`/`context` 模型差异大，需实测确认能否借用。
3. **`third_party` 清理**：仍是 discord SDK 模板残留，`config.json` 里 `useThirdParty = true`，建议改 `false`。
4. **测试对象去留**：`obj_igpu_test` + `Room` 中的实例，生产前应移出或转为独立示例工程。

---

## 附录 A：`os_get_info()` 全平台键位表（来自离线官方手册，已核实）

| 平台 | 关键键 | 备注 |
|---|---|---|
| **Windows** | `video_d3d11_device` / `_context` / `_swapchain`；`video_adapter_vendorid` / `_deviceid` / `_subsysid` / `_revision` / `_description` / `_dedicatedvideomemory` / `_dedicatedsystemmemory` / `_sharedsystemmemory`；`udid` | 唯一完整可用的平台 |
| **Xbox One/Series** | `video_d3d12_cmdqueue` / `_cmdlist` / `_currentrt`；`device_type` | **无** `video_d3d11_swapchain`；`video_adapter_*` 和 `udid` 均为 `0`（description 为 `""`） |
| **macOS / Ubuntu** | `gl_vendor_string` / `gl_version_string` / `gl_renderer_string`；`udid` | 仅字符串，无设备 |
| **Android** | `GL_VERSION` / `GL_VENDOR` / `GL_RENDERER` / `GL_EXTENSIONS` / `GL_SHADING_LANGUAGE_VERSION` / `GL_MAX_TEXTURE_SIZE`；`android_tv`；`SDK_INT` 等 Build 信息 | 比 macOS 多，仍无设备 |
| **iOS / tvOS** | 设备信息键 + 「Additional keys containing OpenGL graphics info」 | 手册未列全 GL 键名 |
| **GX.games** | `mobile` / `userAgentString` / `gl_vendor_string` / `gl_version_string` / `gl_renderer_string` | 无设备 |
| **HTML5** | **返回 `-1`** | ⚠️ 非 map |
| **Nintendo Switch** | **返回 `-1`** | ⚠️ 非 map |
| **PS4** | `display_safe_area_ratio` / `is_neo_mode` / `enter_button_assign` | 无设备 |
| **PS5** | 上述 + `display_resolution` / `display_dynamic_range` / `display_refresh_rate` | 无设备；HDR 信息对渲染路径有意义 |
| **所有平台**（HTML5 除外） | `is64bit` | — |

## 附录 B：GM 已提供的 GPU 能力（IGPU 不应重复实现）

从 `GmlSpec.xml` 提取，以下能力 GM **已有**，IGPU 无需提供：

- **着色器**：`shader_set` / `shader_get_uniform` / `shader_set_uniform_*` / `shader_is_compiled` / `shader_get_sampler_index`
- **顶点**：`vertex_format_begin` + `vertex_format_add_*` / `vertex_create_buffer*` / `vertex_submit_ext`
- **表面**：`surface_create_ext` / `surface_set_target_ext`（MRT 0-3）/ `surface_format_is_supported`
- **全局状态**：`gpu_set_*` / `gpu_get_*` / `gpu_push_state` / `gpu_pop_state`（含 blend / depth / stencil / cull / scissor / alphatest）
- **矩阵**：`matrix_build*` / `matrix_set` / `matrix_stack_*`
- **纹理**：`texture_get_width/height` / `texture_set_stage` / `sprite_get_texture`

**IGPU 的真正空白**（HANDOVER §1 已列，此处确认）：
运行时着色器编译、compute、完整 MRT、状态对象、查询/时间戳/fence、几何/细分着色器、3D 纹理/数组/cubemap 控制、间接绘制/实例化、跨管线资源共享。
