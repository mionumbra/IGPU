# IGPU — 交接文档

> 最后更新：2026-09-23
> 状态：**阶段 A 完成并验证通过**（跨平台 API 抽象层 + 能力查询层已跑通）
> 版本：`0.3.0`
> Git：已是仓库，`main` 分支，工作区干净

---

## 1. 项目目标

为 GameMaker Studio 2 (GMS2) Runtime 提供**完整高级 GPU 功能**的原生扩展，并考虑**跨全平台**。

用户对跨平台的明确定义（**这是本项目的核心约束，务必理解**）：

> 尽管我们可能只能支持 Windows，但是我们需要考虑其他平台渲染的，综合思考来决定这个扩展的功能。
> 这是为未来可能的跨平台做准备，**可以不搞，但不能没有**。

含义拆解：

1. **接口层必须平台无关** —— 接口一旦被调用方使用，改动就是破坏性变更
2. **实现层可以平台特定** —— 只有 Windows 有设备句柄，硬做别的平台是浪费
3. **能力必须可查** —— 非 Windows 平台调用 IGPU 要能优雅降级，而非崩溃

因此本项目的重点**不是**"怎么在 OpenGL 上实现 compute"，而是**怎么让 `spec.gmidl` 里没有一个 D3D 术语**。

GameMaker 自身不暴露的能力（经 `GmlSpec.xml` 三重验证缺失）：

| 缺失能力 | 说明 |
|---|---|
| 运行时着色器编译 | GM 着色器仅能在 IDE 编辑、构建期离线编译，无 `shader_compile`/`shader_load` |
| Compute shader / UAV / structured buffer | 完全没有 |
| 完整 MRT（每目标独立状态） | GM 有 `surface_set_target_ext` (0–3)，但仅颜色、共享单个深度、HTML5 不支持 |
| 状态对象（depth-stencil/rasterizer/sampler/blend state） | 只有全局 `gpu_set_*` 开关，无句柄式不可变状态对象 |
| 查询 / 时间戳 / fence / 遮挡 | 完全没有 |
| 几何 / 细分着色器 | GM 只有 vertex + fragment |
| 3D 纹理 / 纹理数组 / cubemap 控制 | 全 2D 纹理页 |
| 间接绘制 / 实例化属性 / 多流顶点 | 只有单流、无实例化 |
| 与自定义管线互传 GPU 资源 | 无桥接，仅 `buffer_get_address` 提供 CPU 内存指针 |

---

## 2. 架构决策

### 2.1 借用，而非创建

**GameMaker 拥有 D3D11 设备。IGPU 借用它，绝不 Release。**

```
GML: os_get_info()  →  DS Map（含 video_d3d11_device / _context / _swapchain 指针）
      ↓
GML: igpu_init_from_game()  →  igpu_init(device, context, swapchain)
      ↓
C++: igpu::bind_device(ID3D11Device*, ID3D11DeviceContext*, IDXGISwapChain*)
      ↓
后续所有操作基于借用的句柄
```

`igpu_device.cpp` 的 `DeviceState::reset()` 显式**不**释放 device/context/swapchain（注释已标注）。**这个约束守得住，整个项目就不会有生命周期事故。**

### 2.2 为什么不能用其它方式拿设备

- **runner interface 没有图形槽位**：`code_gen/core/GMExtUtils.h` 的 `GMS2RunnerInterface`（~99 个函数指针）无任何 device/context/HWND/graphics 访问器。
- **ExtensionCore 只是序列化框架**：不提供任何 GPU 句柄。
- **唯一官方途径**：GML 层 `os_get_info()`（Windows → `video_d3d11_*`；Xbox → `video_d3d12_*`）或 `window_handle()`（HWND，仅窗口用）。

### 2.3 三层架构

```
┌─────────────────────────────────────────────────────────────┐
│ Tier 3 — 能力抽象层（所有平台）                                │
│   igpu_get_capabilities() / igpu_supports()                 │
│   让上层代码能优雅降级，永远不 crash                            │
├─────────────────────────────────────────────────────────────┤
│ Tier 2 — 查询与反射层（所有有 GPU 信息的平台）                  │
│   适配器信息、显存、格式支持、着色器编译能力探测                 │
├─────────────────────────────────────────────────────────────┤
│ Tier 1 — 真实 GPU 后端（仅 Windows / Xbox）                    │
│   运行时着色器编译、compute、MRT、状态对象、查询                 │
└─────────────────────────────────────────────────────────────┘
```

> **注意**：原方案把 Tier 3 描述成需要原生实现，会导致它在 HTML5 上失效。**Tier 3 应该是纯 GML 可用的**，这样才真正实现"所有平台优雅降级"。

### 2.4 跨平台现实（硬约束）

> **只有 Windows 和 Xbox 暴露真实设备指针。**

| 平台 | 图形 API | 设备句柄可得性 |
|---|---|---|
| **Windows** | **D3D11** | ✅ `os_get_info()` → `video_d3d11_device/context/swapchain` |
| **Xbox One / Series** | **D3D12** | ✅ `video_d3d12_cmdqueue/cmdlist/currentrt`（**无 swapchain**） |
| macOS / Linux | OpenGL | ❌ 仅 `gl_vendor/version/renderer_string` |
| Android | OpenGL ES | ❌ 仅 `GL_*` 字符串 |
| iOS / tvOS | OpenGL ES | ❌ 仅「含 OpenGL 信息的额外键」 |
| Switch | OpenGL | ❌ `os_get_info()` 返回 `-1`（**非 map**） |
| HTML5 / GX.games | WebGL | ❌ 返回 `-1`（**非 map**）；扩展只能用 `.js` |
| PS4 / PS5 | PSSL | ❌ 仅显示信息键，无设备 |

**推论**：运行时着色器编译在非 Windows/Xbox 上**物理上不可行**——拿不到 `HGLRC`/`EGLContext`/`MTLDevice`。

但这**不改变接口设计**：那些平台上 `igpu_shader_compile` 返回 0 + 设错误，`igpu_supports(ShaderCompileRuntime)` 返回 false，调用方据此走 GM 管线。**接口统一，能力可查，降级优雅。**

---

## 3. 仓库结构

```
IGPU/
├── spec.gmidl                 ← GMIDL API 定义（唯一需要手改的接口文件）
├── DESIGN.md                  ← **跨平台抽象层设计文档（必读）**
├── HANDOVER.md                ← 本文件
├── config.json                ← extgen 配置
├── extgen.schema.json         ← 自动生成，勿手改
├── CMakeLists.txt             ← extgen 生成，勿手改
├── CMakePresets.json          ← extgen 每次重新生成会覆盖！勿手改
├── CMakeUserPresets.json      ← 本机 VS2026 预设（.gitignore 已忽略）
├── code_gen/                  ← extgen 生成的桥接代码，勿手改
├── docs                       ← extgen 生成的文档数据（是文件，非目录）
├── src/
│   ├── CMakeLists.txt         ← 手写：源文件选择 + D3D 链接
│   └── native/                ← **唯一手写实现目录**
│       ├── IGPU_native.cpp        ← API 实现 + D3DCompile 逻辑
│       ├── IGPU_native.h          ← 只有一行 include
│       ├── igpu_device.h/.cpp     ← 设备状态、借用句柄管理
│       ├── igpu_capabilities.h/.cpp ← **能力查询层（Tier 3）**
│       └── igpu_error.h/.cpp      ← 错误信息 + UTF-16→UTF-8
├── third_party/               ← 第三方集成点（当前 discord SDK 残留，inert）
└── project/                   ← GameMaker 工程
    ├── IGPU.yyp
    ├── AGENTS.md              ← **必读：.yy/.yyp 编辑规则 + GML 约定**
    ├── extensions/IGPU/       ← IGPU.ext(0字节占位) + IGPU.yy + IGPU.dll(构建产物)
    └── scripts/IGPU_helpers/  ← **手写** GML 辅助函数
```

---

## 4. 当前 API 清单（spec.gmidl，v0.3.0）

### 生命周期
- `igpu_init(device : gmval, context : gmval, swapchain : gmval) : bool`
- `igpu_shutdown() : unit`
- `igpu_version() : string` → `"0.3.0"`
- `igpu_is_available() : bool`

### 设备信息
- `igpu_get_feature_level() : int32`
- `igpu_get_adapter_description() : string`
- `igpu_get_video_memory() : int64`
- `igpu_get_backbuffer_width() : int32`
- `igpu_get_backbuffer_height() : int32`

### 能力查询（Tier 3）
- `igpu_get_capabilities() : gmval` → **struct，19 个键，永不失败**
- `igpu_supports(capability : int32) : bool`
- `igpu_get_shader_dialect() : string` → `"hlsl"` / `""`

### 运行时着色器编译
- `igpu_shader_compile(source, entry, stage : int32, dialect : string = "") : int64` ← **统一入口**
- `igpu_shader_compile_vertex/pixel/compute(source, entry, dialect = "") : int64` ← 便捷包装
- `igpu_shader_release([type_hint = \`uint64\`] shader) : bool`
- `igpu_get_last_error() : string`

### 枚举
- `IgpuFeatureLevel { Unknown, Level_11_0, Level_11_1, Level_12_0, Level_12_1 }`
- `IgpuShaderStage { Vertex=0, Pixel=1, Compute=2, Geometry=3, Hull=4, Domain=5, Mesh=6, Amplification=7 }`
- `IgpuCapability { None=0, ShaderCompileRuntime=1, ShaderStage*=2-7, Texture3D=20…MultipleRenderTargets=25, Instancing=40…Wireframe=46, AdapterInfo=60…BackbufferSize=62 }`

### `igpu_get_capabilities()` 返回的 19 个键

```
backend, tier, device_name, shader_dialect,
shader_stages, runtime_compile, compute, geometry, tessellation, mesh_shader,
texture_3d, texture_array, texture_cubemap, structured_buffer, uav, max_render_targets,
instancing, indirect_draw, queries
```

**所有键永远存在**，调用方可无条件读取。非 Windows 平台返回 `backend="none"`, `tier=3`, 其余全 false。

---

## 5. 构建与运行

### 环境（本机已验证）

- cmake 4.4.3、Visual Studio **18** 2026 Community（`D:\Program Files\Microsoft Visual Studio\18\Community`）、MSVC 14.51
- Windows SDK 10.0.26100.0（`D:\Windows Kits\10\`，**不在**默认 `Program Files (x86)`）
- extgen v1.d8c68bd，位于 **`D:\GM-ExtensionGenerator\extgen.exe`**（不在工作区内！）
- gm-cli v2.3.0，缓存于 `D:\node.js\node_cache\_npx\166e0ec5f4c2d768\node_modules\@gamemaker\gm-cli\dist\cli.js`

### 构建

```pwsh
# 注意：用 -vs18 后缀的预设（本机无 VS2022/v143）
cmake --preset win-x64-release-vs18
cmake --build --preset win-x64-release-vs18
```

产物 `IGPU.dll` 会自动拷贝到 `project/extensions/IGPU/`。

### 修改 API 后必须重新生成

```pwsh
& "D:\GM-ExtensionGenerator\extgen.exe" --config "D:\Users\User\Documents\gml_ext\IGPU\config.json"
```

每次改 `spec.gmidl` 或 `config.json` 都要重跑。成功输出 `[extgen] Success [x]`。

### 编译 & 运行游戏

```pwsh
# 在 project/ 目录
node "D:\node.js\node_cache\_npx\166e0ec5f4c2d768\node_modules\@gamemaker\gm-cli\dist\cli.js" compile --errors-only
node "D:\node.js\node_cache\_npx\166e0ec5f4c2d768\node_modules\@gamemaker\gm-cli\dist\cli.js" run --no-errors-only
```

> `run` 默认只显示错误；要看到 `show_debug_message` 输出**必须**加 `--no-errors-only`。
> 若 `npx` 因沙箱写缓存失败（EPERM），直接用上面的 `node` + 缓存路径。

### 测试自动退出

`obj_igpu_test` 会**自己结束游戏**，无需手动关窗口：

- 全部通过 → 输出 `PASS`，退出码 **0**
- 有失败 → 输出 `FAIL` + `checks failed : N`，退出码 **1**

想肉眼看渲染效果：设 `global.igpu_test_auto_exit = false`。

### 查文档（**优先用它，别凭记忆写 GML**）

```pwsh
node "...\gm-cli\dist\cli.js" manual read <函数名>
```

本机权威资料（**不需要联网**）：
- `GmlSpec.xml`：`%LOCALAPPDATA%\GameMakerCLI\cache\runtimes-gms2\runtime-2026.0.0.23\GmlSpec.xml`
- 离线手册：`C:\ProgramData\GameMakerStudio2-LTS2026\Manual\GMS2-Robohelp-en.zip`（173 MB）

---

## 6. 验证证据（阶段 A）

`gm-cli run --no-errors-only` 实测输出：

```
IGPU integration test
igpu_version     : 0.3.0
igpu_init result : 1
is_available     : 1
feature level    : 2          ← D3D_FEATURE_LEVEL_11_1
adapter          : AMD Radeon(TM) Vega 8 Graphics
video memory     : 2132373504
backbuffer       : 1366 x 768
igpu :: backend=d3d11 tier=1 dialect=hlsl
igpu :: device=AMD Radeon(TM) Vega 8 Graphics
igpu :: stages=[ "vertex","pixel","compute","geometry","tessellation" ]
igpu :: runtime_compile=1 compute=1 geometry=1 tess=1
igpu :: texture3d=1 array=1 cubemap=1 uav=1 mrt=8
supports(compute): 1
supports(mesh)   : 0
bad shader error : igpu_shader_compile_vertex failed (hr=0x80004005): ... error X3000: syntax error
mesh shader error : igpu_shader_compile: stage 'mesh' is not supported by the 'd3d11' backend (check igpu_supports)
good shader handle: 1
pixel shader handle: 2
glsl dialect error : igpu_shader_compile: dialect 'glsl' is not supported by the 'd3d11' backend (it compiles 'hlsl')
checks failed    : 0
PASS
Game exited
```

验证了：设备借用、能力结构体解码、阶段列表、**能力拒绝路径优于编译器报错**、统一/包装两条编译路径、dialect 校验。

---

## 7. 踩过的坑（**务必读，容易重蹈**）

### 7.1 指针参数不能用 `uint64`

- **错误做法**：`function igpu_init([type_hint = \`uint64\`] device, ...)`
- **原因**：extgen 为 `uint64` 生成 `if (!is_numeric(_x)) show_error(...)`。而 GM 的指针类型（`ptr`）**不是 numeric**，`is_numeric(ptr)` 为 false。
- **正确做法**：用 **`gmval`**（映射到 `gm::wire::GMValue`），走 `__ext_core_buffer_marshal_value`，类型为 `Any`，不做检查。

### 7.2 不要用 `GMValue::as<void*>()`

会触发 `coerceScalar<T>` 对所有标量类型实例化，其中 `static_cast<void*>(uint8_t)` 非法 → 一堆 `C2440`。
**正确做法**：直接读原始字节：
```cpp
if (value.kind() != gm::wire::GMKind::Pointer) return nullptr;
return reinterpret_cast<void*>(gm::byteio::readLe<std::uintptr_t>(value.data()));
```

### 7.3 `bind_device` 签名要直接收指针

不要 `uint64` 收进来再 `reinterpret_cast` 回指针。直接 `ID3D11Device*`。

### 7.4 `CMakePresets.json` 每次 extgen 会覆盖

本机只有 VS **18** 2026。`CMakePresets.json` 硬编码 `"Visual Studio 17 2022"` + toolset `v143`。
**解法**：写 `CMakeUserPresets.json`（`.gitignore` 已忽略、extgen 不碰），定义 `win-x64-*-vs18` 预设。
注意：
- user preset **不能与生成预设同名**（会 `Duplicate preset` 报错），故加 `-vs18` 后缀。
- **不要设 `toolset`**：本机 toolset 目录是 `v180`，写 `v144` 会 MSB8020。
- 用户预设若 `inherits: base-vs`，会继承到 `toolset: v143` → 失败。故 `vs18-base` **不继承** `base-vs`。

### 7.5 extgen 不支持顶层 `struct` 声明

已实测：在 `spec.gmidl` 里写 `struct Foo { ... }` 会报 `NoViableAltException`。

**后果**：返回复合类型只能用 `gmval`，C++ 侧手工编码 wire 格式。见 §7.6。

### 7.6 ⚠️ Wire 格式三大静默陷阱（**最危险，不报错只出垃圾数据**）

这三个都是实测出来的，且**不会编译报错**，运行时才表现为数据错误。

#### (a) `StructStream` 不能按值当 `DataStream` 返回

```cpp
// ❌ 错误：虚函数 writeTo() 被切片，Struct 头（tag + 条目数）整个丢失
gm::wire::DataStream build() { gm::wire::StructStream s; ...; return s; }
// 实测：缓冲区首字节变成 11（String）而非 255（Struct），GML 解析成乱码
```

`DataStream` 的 `buffer` 是 **protected**，`operator<<` 会给每个值**前置一个 kind 字节**，因此没有公开方式注入已编码字节。

**正确做法**：用派生类把字节灌进基类自己的 buffer（这样按值返回基类型时不丢）：
```cpp
class SeedableDataStream final : public gm::wire::DataStream {
public:
    void append(const std::byte* data, std::size_t size) {
        auto& target = getBuffer();          // protected，派生类可访问
        target.insert(target.end(), data, data + size);
    }
};
```
先 `StructStream::writeTo(writer)` 得到正确字节（它用 `codec::writeValue` 写 **原始** tag/count），再 append 进去。

#### (b) 字符串值必须显式传 `std::string_view`

```cpp
caps.add("backend", backend_name());              // ❌ 编码成数字，实测得到 1
caps.add("backend", std::string_view(...));       // ✅
```
`const char*` 会绑到 `addKeyValue` 的 `const T&` 模板（`T = char[N]`），`getGMValueType` 判定异常。

#### (c) `ArrayStream::push("字面量")` 会绑到 **`bool` 重载**

```cpp
stages.push("vertex");                    // ❌ 指针→bool 是标准转换，比 string_view 的用户定义转换优先
stages.push(std::string_view("vertex"));  // ✅
```
实测：五个字符串变成五个 `true`，整个结构体后续字段全部错位。

### 7.7 `igpu_get_last_error` 等「无包装」函数

extgen 对简单标量/字符串返回的函数**不生成 GML 包装**，直接在 `.yy` 注册为外部函数。这些可直接从 GML 调用（如 `igpu_version()`、`igpu_supports()`、`igpu_get_shader_dialect()`）。

### 7.8 扩展文件命名

- `IGPU.yy` 的 `files[0].filename = "IGPU.ext"`，实际加载的是 `ProxyFiles` 中映射的 `IGPU.dll`。
- 运行日志出现 `LoadLibraryW("IGPU.ext") failed ... File doesn't exist.` **是正常的**，不影响功能（`.ext` 是 0 字节占位）。

### 7.9 `os_get_info()` 返回 DS Map，不是 struct

- `typeof()` 显示为 `ref`。用 `ds_map_exists(info, "key")` 和 `info[? "key"]`，**不要**用 `variable_struct_*`。
- **必须 `ds_map_destroy()`** —— 手册明确警告 GM 不会自动清理。
- **HTML5 / Switch 返回 `-1`（非 map）**，需防御。

### 7.10 `@"..."` verbatim 字符串

GML 的 `@"` 后**紧跟换行**会导致 `invalid token`。多行 HLSL 用字符串拼接或单行。

### 7.11 `.yy`/`.yyp` 绝不可手改

必须用 `gamemaker-resource-tool` MCP。工程约定见 `project/AGENTS.md`。

### 7.12 沙箱/工具链陷阱（本机环境）

- `git` 因目录属主是 `Administrators` 报 `dubious ownership`，已加 `safe.directory` 到全局配置。
- `extgen.exe`、`cl.exe`、`msbuild`、`node` 均在工作区**外**，沙箱默认拒绝执行，需提权。
- MSBuild / gm-cli 需要 piped `cmd.exe`，沙箱下报 `EPERM` / `spawn EPERM`，同样需提权。
- PowerShell 里重定向 `2>&1` 会让某些 `.exe` 包装器报 `StandardErrorEncoding` 错误；用 `Out-File` 或直接不加重定向。

---

## 8. 已知问题 / 待清理

1. **`third_party` 是 discord SDK 模板残留**：`third_party/CMakeLists.txt` 是 extgen 自带模板，默认全 OFF，因此**当前是 inert 的**，但配置时会打印 `SDK_ROOT = .../discord_social_sdk`。可在 `config.json` 设 `build.cmake.useThirdParty = false`。
2. **测试对象仍在生产工程里**：`obj_igpu_test` + `Room` 中的实例。生产前应移除或改为独立示例工程。
3. **无 `igpu_shutdown` 自动调用**：需在游戏结束/房间切换时手动调用，否则着色器句柄泄漏。
4. **`.gmcache/` 留本地**（用户决定）：有自保护 `.gitignore`（内容 `*`），不进仓库。
5. **Xbox D3D12 后端暂不做**（用户决定）：只保留接口（`IgpuFeatureLevel.Level_12_*` 等），不实现。

---

## 9. 下一步

详细设计见 **`DESIGN.md`**。按优先级：

### 阶段 D — 补全 Tier 1（Windows）

按依赖顺序：

1. **⚠️ 先改 shader 句柄结构保留 `ID3DBlob`** ← **关键前置**
   当前 `DeviceState::shaders` 只存 `ID3D11DeviceChild*`，**丢弃了编译产物 blob**。
   而 `CreateInputLayout` **必须**用 blob 里的 signature。没有它，编译出的顶点着色器无法真正绘制。
   → 建议 `shaders` 改为 `struct ShaderEntry { ID3D11DeviceChild* object; ID3D11Blob* bytecode; }`
2. **常量缓冲区 / 顶点缓冲区**（`ID3D11Buffer`）+ 更新/映射
3. **输入布局**（`ID3D11InputLayout`）+ 顶点格式定义
4. **绘制调用**（`Draw` / `DrawIndexed` / 实例化）
5. **渲染状态对象**（depth-stencil / rasterizer / blend / sampler state）
6. **MRT**（多 `ID3D11RenderTargetView`）
7. **纹理 / SRV / RTV / UAV**（含 3D / array / cubemap）
8. **查询 / 时间戳 / fence**

> 新增 API 时**同步更新** `igpu_capabilities.cpp` 的 `supports()` 与 `build_capabilities()`（**注意 `kEntryCount` 要跟着改**）。

### 优先级 2
- [ ] `igpu_get_capabilities()` 增加 `formats` 子结构（复用 GM 的 `surface_*` 词汇表）
- [ ] Tier 2 后端抽象（OpenGL/GLES 能力探测，先不实现渲染）

### 设计约束（务必遵守）
- **接口零 D3D 术语** —— GML 层面不得出现 `d3d`/`dxgi`/`_5_0`/`ID3D` 字样
- **复用 GM 词汇表** —— GM 已有平台无关的 GPU 词汇（`surface_rgba8unorm`、`vertex_usage_*`、`cmpfunc_*`、`cull_*`），**不要另发明**
- **能力可查，降级优雅** —— 非 Windows 平台调用 IGPU 必须返回失败而非 crash
- **不与 GM 状态机打架** —— 改变全局管线状态后应恢复（`gpu_get_state`/`gpu_set_state`）
- **句柄模式统一** —— 延续 `unordered_map<uint64, 资源*>` + 自增 ID

---

## 10. 关键命令速查

```pwsh
# 重新生成绑定（改 spec.gmidl / config.json 后）
& "D:\GM-ExtensionGenerator\extgen.exe" --config "D:\Users\User\Documents\gml_ext\IGPU\config.json"

# 构建
cmake --preset win-x64-release-vs18
cmake --build --preset win-x64-release-vs18

# 编译游戏 / 运行（看 debug 输出必须 --no-errors-only）
$cli = "D:\node.js\node_cache\_npx\166e0ec5f4c2d768\node_modules\@gamemaker\gm-cli\dist\cli.js"
node $cli compile --errors-only
node $cli run --no-errors-only      # PASS 退出码 0，FAIL 退出码 1

# 查 GML 文档
node $cli manual read <fn>

# Git
git log --oneline
git status --short
```

---

## 11. 参考资料

- extgen CLI 文档：https://github.com/YoYoGames/GM-ExtensionGenerator/wiki/user_cli_docs
- **GMIDL 类型系统（最重要）**：https://github.com/YoYoGames/GM-ExtensionGenerator/wiki/user_gmidl_docs
- 实现工作流：https://github.com/YoYoGames/GM-ExtensionGenerator/wiki/user_impl_workflow
- gm-cli：https://github.com/YoYoGames/gm-cli
- 官方扩展示例：https://github.com/YoYoGames/GMEXT-Steamworks
- 参考实现：GMD3D11（blueburncz）、shader_replace_unsafe（YAL）、gm82dx9 / gm82angle（GM82Project）
- 本机手册：`C:\ProgramData\GameMakerStudio2-LTS2026\Manual\GMS2-Robohelp-en.zip`
- 本机 GmlSpec：`%LOCALAPPDATA%\GameMakerCLI\cache\runtimes-gms2\runtime-2026.0.0.23\GmlSpec.xml`

> **注意**：本环境**没有公网出口**（`github.com` 等解析到非公网 IP），上述链接仅作离线参考。需要的资料优先查本机 GmlSpec 和离线手册。
