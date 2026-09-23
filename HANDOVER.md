# IGPU — 交接文档

> 最后更新：2026-09-23
> 状态：**第一阶段完成并验证通过**（Windows D3D11 端到端链路已跑通）
> 注意：当前目录**尚不是 git 仓库**（无 `.git`），`.gitignore` 已备好。

---

## 1. 项目目标

为 GameMaker Studio 2 (GMS2) Runtime 提供**完整高级 GPU 功能**的原生扩展，并考虑**跨全平台**。

GameMaker 自身不暴露的能力（经 `GmlSpec.xml` / `fnames` / 官方手册三重验证缺失）：

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

## 2. 核心架构决策：借用，而非创建

**GameMaker 拥有 D3D11 设备。IGPU 借用它，绝不 Release。**

数据流：

```
GML: os_get_info()  →  DS Map（含 video_d3d11_device / _context / _swapchain 指针）
      ↓
GML: igpu_init_from_game()  →  igpu_init(device, context, swapchain)
      ↓
C++: igpu::bind_device(ID3D11Device*, ID3D11DeviceContext*, IDXGISwapChain*)
      ↓
后续所有操作基于借用的句柄
```

`igpu_device.cpp:35` — `DeviceState::reset()` 显式**不**释放 device/context/swapchain（注释已标注）。

### 为什么不能用其它方式拿设备

- **runner interface 没有图形槽位**：`code_gen/core/GMExtUtils.h` 的 `GMExS2RunnerInterface`（~99 个函数指针）无任何 device/context/HWND/graphics 访问器。
- **ExtensionCore 只是序列化框架**：`ExtensionCore.yy` 声明 `"files":[]`，不提供任何 GPU 句柄。
- **唯一官方途径**：GML 层 `os_get_info()`（Windows → `video_d3d11_*`；Xbox → `video_d3d12_*`）或 `window_handle()`（HWND，仅窗口用）。

---

## 3. 跨平台现实（**重要：决定了整体设计**）

> 这是本项目的硬约束。**只有 Windows 和 Xbox 暴露真实设备指针。**

| 平台 | 图形 API | 设备句柄可得性 |
|---|---|---|
| **Windows** | **D3D11** | ✅ `os_get_info()` → `video_d3d11_device/context/swapchain` |
| **Xbox One / Series** | **D3D12** | ✅ `os_get_info()` → `video_d3d12_cmdqueue/cmdlist/currentrt`（无 swapchain） |
| macOS | OpenGL | ❌ 仅 `gl_vendor/version/renderer_string` |
| Linux | OpenGL | ❌ 同 macOS |
| Android | OpenGL ES | ❌ 仅 `GL_*` 字符串 |
| iOS / tvOS | OpenGL ES | ❌ 仅「含 OpenGL 信息的额外键」 |
| Switch | OpenGL | ❌ `os_get_info()` 返回 `-1` |
| HTML5 / GX.games | WebGL | ❌ 返回 `-1`；且扩展只能用 `.js` |
| PS4 / PS5 | PSSL | ❌ 仅显示信息键，无设备 |

**推论**：在这些平台上，"完整高级 GPU 功能"物理上不可行——拿不到 `HGLRC`/`EGLContext`/`MTLDevice`，且扩展无法链接进 runtime 的图形上下文。因此 IGPU 必然是**分层**架构：

- **Tier 1（Windows / Xbox）**：真 GPU 后端，全部高级功能。
- **Tier 2（OpenGL/GLES 平台）**：降级——能力探测、纹理格式/采样器查询，渲染走 GM 自有着色器管线。
- **Tier 3（所有平台）**：纯 GML 能力查询抽象层，让上层代码优雅降级。

当前第一阶段**只实现了 Tier 1 的 Windows 子集**。

---

## 4. 仓库结构

```
IGPU/
├── spec.gmidl                 ← GMIDL API 定义（唯一需要手改的接口文件）
├── config.json                ← extgen 配置
├── extgen.schema.json         ← 自动生成，勿手改
├── CMakeLists.txt             ← extgen 生成，勿手改
├── CMakePresets.json          ← extgen 每次重新生成会覆盖！勿手改
├── CMakeUserPresets.json      ← 本机 VS2026 预设（.gitignore 已忽略，extgen 不覆盖）
├── code_gen/                  ← extgen 生成的桥接代码，勿手改
│   ├── core/GMExtWire.h/.cpp  ← 序列化 / wire 协议
│   ├── core/GMExtUtils.h/.cpp ← runner interface
│   └── native/IGPUInternal_native.{h,cpp}  ← GML↔C++ 桥接
│   └── native/IGPUInternal_exports.h
├── src/
│   ├── CMakeLists.txt         ← 手写：源文件选择 + D3D 链接
│   └── native/                ← **唯一手写实现目录**
│       ├── IGPU_native.cpp    ← 14 个 API 实现 + D3DCompile 逻辑
│       ├── IGPU_native.h      ← 只有一行 include
│       ├── igpu_device.h/.cpp ← 设备状态、借用句柄管理
│       └── igpu_error.h/.cpp  ← 错误信息 + UTF-16→UTF-8
├── docs/                      ← extgen 生成的文档数据
├── third_party/               ← 第三方集成点（当前含 discord SDK 残留配置）
└── project/                   ← GameMaker 工程
    ├── IGPU.yyp
    ├── AGENTS.md              ← **必读：.yy/.yyp 编辑规则 + GML 约定**
    ├── extensions/
    │   ├── IGPU/              ← IGPU.ext(占位) + IGPU.yy(extgen patch) + IGPU.dll(构建产物)
    │   └── ExtensionCore/     ← 官方导入的运行时
    ├── scripts/
    │   ├── IGPU_API/          ← extgen 生成的类型化包装（勿手改）
    │   ├── IGPU_helpers/      ← **手写** GML 辅助函数
    │   ├── ExtensionCore_api/
    │   └── ExtensionCore_exports/
    ├── objects/obj_igpu_test/ ← 集成测试对象
    └── rooms/Room/            ← 已放置测试实例
```

---

## 5. 当前 API 清单（spec.gmidl）

**生命周期**
- `igpu_init(device : gmval, context : gmval, swapchain : gmval) : bool`
- `igpu_shutdown() : unit`
- `igpu_version() : string` → `"0.2.0"`
- `igpu_is_available() : bool`

**设备信息**
- `igpu_get_feature_level() : int32`（返回 `IgpuFeatureLevel`）
- `igpu_get_adapter_description() : string`
- `igpu_get_video_memory() : int64`
- `igpu_get_backbuffer_width() : int32`
- `igpu_get_backbuffer_height() : int32`

**运行时着色器编译**
- `igpu_shader_compile_vertex(source, entry, target) : int64`（返回句柄，0=失败）
- `igpu_shader_compile_pixel(source, entry, target) : int64`
- `igpu_shader_compile_compute(source, entry, target) : int64`
- `igpu_shader_release([type_hint = \`uint64\`] shader) : bool`
- `igpu_get_last_error() : string`

**枚举**
- `IgpuFeatureLevel { Unknown, Level_11_0, Level_11_1, Level_12_0, Level_12_1 }`
- `IgpuShaderStage { Vertex, Pixel, Compute }`

---

## 6. 构建与运行

### 环境（本机已验证）

- cmake 4.4.3、Visual Studio **18** 2026 Community（`D:\Program Files\Microsoft Visual Studio\18\Community`）、MSVC 14.51
- Windows SDK 10.0.26100.0（`D:\Windows Kits\10\`，注意不在默认 `Program Files (x86)`）
- extgen v1.d8c68bd（在 PATH）
- gm-cli v2.3.0（`npx @gamemaker/gm-cli`）

### 构建

```pwsh
# 注意：用 -vs18 后缀的预设（本机无 VS2022/v143）
cmake --preset win-x64-release-vs18
cmake --build --preset win-x64-release-vs18
```

产物 `IGPU.dll` 会自动拷贝到 `project/extensions/IGPU/`。

### 修改 API 后必须重新生成

```pwsh
extgen --config "D:\Users\User\Documents\gml_ext\IGPU\config.json"
```

每次改 `spec.gmidl` 或 `config.json` 都要重跑 extgen。

### 编译 & 运行游戏

```pwsh
# 在 project/ 目录
npx --yes @gamemaker/gm-cli compile --errors-only
npx --yes @gamemaker/gm-cli run --no-errors-only   # 必须 --no-errors-only 才显示 show_debug_message
```

> `run` 默认只显示错误；要看到 `show_debug_message` 输出**必须**加 `--no-errors-only`。

### 查文档（**优先用它，别凭记忆写 GML**）

```pwsh
npx --yes @gamemaker/gm-cli manual read <函数名>
```

---

## 7. 验证证据（第一阶段）

`npx @gamemaker/gm-cli run --no-errors-only` 实测输出：

```
extension_exists : 1
igpu_version     : 0.2.0
os_get_info type : ref
has device key   : 1
igpu_init_from_game :: device=0000000002824950 context=0000000002827B28 swapchain=0000000002851680
igpu_init result : 1
feature level    : 2          ← D3D_FEATURE_LEVEL_11_1
adapter          : AMD Radeon(TM) Vega 8 Graphics
video memory     : 2132373504
backbuffer       : 1366 x 768
bad shader handle: 0
bad shader error : ...error X3000: syntax error: unexpected end of file
good shader handle: 1
PASS
```

验证了：真实设备借用、feature level、DXGI 显卡信息、**运行时 HLSL 编译成功**、错误路径正确返回 D3DCompile 错误。

---

## 8. 踩过的坑（**务必读，容易重蹈**）

### 8.1 指针参数不能用 `uint64`

- **错误做法**：`function igpu_init([type_hint = \`uint64\`] device, ...)`
- **原因**：extgen 为 `uint64` 生成 `if (!is_numeric(_x)) show_error(...)`。而 GM 的指针类型（`ptr`）**不是 numeric**，`is_numeric(ptr)` 为 false，直接报错 `_device expected number`。
- **正确做法**：用 **`gmval`**（映射到 C++ `gm::wire::GMValue`），走 `__ext_core_buffer_marshal_value`，类型为 `Any`，不做检查，指针原样传入。

### 8.2 不要用 `GMValue::as<void*>()`

- 会触发 `GMExtWire.h:798` 的 `coerceScalar<T>` 模板对所有标量类型实例化，其中 `static_cast<void*>(uint8_t)` 非法 → 编译报一堆 `C2440`。
- **正确做法**：直接读原始字节：
  ```cpp
  if (value.kind() != gm::wire::GMKind::Pointer) return nullptr;
  return reinterpret_cast<void*>(gm::byteio::readLe<std::uintptr_t>(value.data()));
  ```

### 8.3 `bind_device` 签名要直接收指针

不要 `uint64` 收进来再 `reinterpret_cast` 回指针——多此一举。直接 `ID3D11Device*`。

### 8.4 `CMakePresets.json` 每次 extgen 会覆盖

本机只有 VS **18** 2026，没有 VS2022/v143。`CMakePresets.json` 硬编码 `"Visual Studio 17 2022"` + toolset `v143`。
**解法**：写 `CMakeUserPresets.json`（已被 `.gitignore` 忽略、extgen 不碰），定义 `win-x64-*-vs18` 预设。
注意：
- user preset **不能与生成预设同名**（会 `Duplicate preset` 报错），故加 `-vs18` 后缀。
- **不要设 `toolset`**：本机 toolset 目录是 `v180`，写 `v144` 会 MSB8020。
- 用户预设若 `inherits: base-vs`，会继承到 `toolset: v143` → 失败。故 `vs18-base` **不继承** `base-vs`，自行内联所需 cache 变量。

### 8.5 `igpu_get_last_error` 等「无包装」函数

extgen 对简单标量/字符串返回的函数**不生成 GML 包装**，直接在 `.yy` 注册为外部函数。这些可直接从 GML 调用（如 `igpu_version()`、`igpu_get_last_error()`）。
早期返回 `0` 是因为当时扩展/DLL 尚未正确加载，**不是**函数不存在。

### 8.6 扩展文件命名

- `IGPU.yy` 的 `files[0].filename = "IGPU.ext"`，实际加载的是 `ProxyFiles` 中映射的 `IGPU.dll`。
- 运行日志出现 `LoadLibraryW("IGPU.ext") failed ... File doesn't exist.` **是正常的**，不影响功能（`.ext` 是 0 字节占位，真正的 DLL 经 ProxyFiles 加载）。

### 8.7 `os_get_info()` 返回 DS Map，不是 struct

- `typeof()` 显示为 `ref`。用 `ds_map_exists(info, "key")` 和 `info[? "key"]`，**不要**用 `variable_struct_*`。
- 千万不要忘记 `ds_map_destroy()`（当前 helper 未销毁，属于待改进项）。

### 8.8 `@"..."` verbatim 字符串

GML 的 `@"` 后**紧跟换行**会导致 `invalid token`。多行 HLSL 用字符串拼接或单行。

### 8.9 `.yy`/`.yyp` 绝不可手改

必须用 `gamemaker-resource-tool` MCP。工程约定见 `project/AGENTS.md`。

---

## 9. 已知问题 / 待清理

1. **调试痕迹**：`src/native/igpu_device.cpp` 里有 `igpu_debug_trace()`，会写 `igpu_native_trace.txt` 到 CWD 并 `OutputDebugString`。**正式版应移除或改为可选日志。**
2. **`ds_map_destroy` 缺失**：`igpu_init_from_game()` 拿到 `os_get_info()` 的 map 后未销毁，会泄漏。
3. **测试残留**：`obj_igpu_test` 对象 + `Room` 中的测试实例。生产前应移除或改为独立示例工程。
4. **`third_party` 是 discord SDK 模板残留**：`third_party/CMakeLists.txt` 是 extgen 自带的模板，默认全部选项 OFF，因此**当前是 inert 的**（不链接任何东西），但会在配置时打印 `SDK_ROOT = .../discord_social_sdk`。若不需要第三方库，可在 `config.json` 设 `build.cmake.useThirdParty = false`，或清空该文件。
5. **`igpu_get_last_error` 无包装**：可用但需注意 extgen 行为。
6. **`shader_release` 参数仍是 `uint64`**：因 shader 句柄是真正数值（非指针），此处 `uint64` 正确。
7. **无 `igpu_shutdown` 自动调用**：需在游戏结束/房间切换时手动调用，否则着色器句柄泄漏。

---

## 10. 下一步建议

### 优先级 1：稳固基础
- [ ] 移除调试 trace，改为可配置日志
- [ ] 补 `ds_map_destroy`
- [ ] 决定测试对象去留

### 优先级 2：补齐 Tier 1 核心（Windows）
按依赖顺序：
1. **常量缓冲区 / 顶点缓冲区**（`ID3D11Buffer`）+ 更新/映射
2. **输入布局**（`ID3D11InputLayout`）+ 顶点格式定义
3. **绘制调用**（`Draw` / `DrawIndexed` / 实例化）
4. **渲染状态对象**（depth-stencil / rasterizer / blend / sampler state）
5. **MRT**（多 `ID3D11RenderTargetView`）
6. **纹理 / SRV / RTV / UAV**（含 3D / array / cubemap）
7. **查询 / 时间戳 / fence**

### 优先级 3：Tier 2/3
- [ ] `igpu_get_capabilities()` 跨平台能力查询（返回 struct/map）
- [ ] OpenGL/GLES 后端抽象（先做能力探测，不急于实现渲染）
- [ ] Xbox D3D12 后端（`video_d3d12_*`）

### 设计约束（务必遵守）
- **抽象层要平台无关**：后端可插拔，Tier 1 代码用 `#ifdef OS_WINDOWS` 隔离在 `src/native/`。
- **不要与 GM 的状态机打架**：IGPU 改变全局管线状态后应考虑恢复（GM 有 `gpu_get_state`/`gpu_set_state`）。
- **句柄模式**：延续 `unordered_map<uint64, 资源*>` + 自增 ID 设计（见 `igpu_device.h` 的 `shaders`）。

---

## 11. 关键命令速查

```pwsh
# 重新生成绑定（改 spec.gmidl / config.json 后）
extgen --config "D:\Users\User\Documents\gml_ext\IGPU\config.json"

# 构建
cmake --preset win-x64-release-vs18
cmake --build --preset win-x64-release-vs18

# 编译游戏
npx --yes @gamemaker/gm-cli compile --errors-only

# 运行游戏（看 debug 输出必须 --no-errors-only）
npx --yes @gamemaker/gm-cli run --no-errors-only

# 查 GML 文档
npx --yes @gamemaker/gm-cli manual read <fn>
```

---

## 12. 参考资料

- extgen CLI 文档：https://github.com/YoYoGames/GM-ExtensionGenerator/wiki/user_cli_docs
- **GMIDL 类型系统（最重要）**：https://github.com/YoYoGames/GM-ExtensionGenerator/wiki/user_gmidl_docs
- 实现工作流：https://github.com/YoYoGames/GM-ExtensionGenerator/wiki/user_impl_workflow
- 入门示例：https://github.com/YoYoGames/GM-ExtensionGenerator/wiki/getting_started_sample
- gm-cli：https://github.com/YoYoGames/gm-cli
- 官方扩展示例：https://github.com/YoYoGames/GMEXT-Steamworks
- 参考实现：GMD3D11（blueburncz）、shader_replace_unsafe（YAL）、gm82dx9 / gm82angle（GM82Project）
- 本机手册：`C:\ProgramData\GameMakerStudio2-LTS2026\Manual\GMS2-Robohelp-en.zip`
- 本机 GmlSpec：`%LOCALAPPDATA%\GameMakerCLI\cache\runtimes-gms2\runtime-2026.0.0.23\GmlSpec.xml`
