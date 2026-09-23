# 用引擎源码重新审视 IGPU —— 审计报告

> 方法:把 `spec.gmidl` / `src/native/*` 的每一条设计主张,拿去和 **GMS2 引擎真源码**
> (`OpenGM` → `runtime/GMS2-Runner-Main/VC_Runner/Files/`)逐条对质。
> 凡是符号层能验证的,都实际链接/运行验证过,不只读源码。
>
> 日期:2026-09-23

---

## 结论摘要

| 项 | 判定 |
|---|---|
| `os_get_info()` 取 device/context/swapchain | ✅ **正确**,引擎内部就是 `GR_D3D_Device` / `GR_D3D_Context` |
| `pr_*` 常量值 1–6 | ✅ **正确**,与 `ePrimType` 完全一致 |
| `vertex_usage_*` / `vertex_type_*` 常量值 | ✅ **正确**,且已被现有测试**运行时反证**(见 §2) |
| 语义名拼写(POSITION/TEXCOORD…) | ✅ **正确**,与引擎 `g_VertexUsageStrings[]` 一致 |
| `Static` → `D3D11_USAGE_DEFAULT` | ✅ **正确**,与引擎自己的 `VertexBuffer::Init` 同构 |
| 运行时不编译着色器的判断 | ✅ **正确**,引擎**从不**调 `D3DCompile`(全仓库零匹配) |
| IA 状态保存/恢复的**前提** | ✅ **正确**,GML 确实无 IA 接口 |
| IA 状态保存/恢复的**理由** | ⚠️ **夸大**——理由不成立,但代码本身无害且正确(§1) |
| 「测试通过」的含义 | ❌ **被高估**——`igpu_draw` 从未真正画出东西(§3) |
| 着色器能被绑定吗? | ✅ **已修复** —— 新增 igpu_shader_bind(§3) |
| 能力声明 | ❌ **11 项虚报**,报 true 但无对应 API(§4) |
| 设备丢失 | ❌ **无恢复路径**,use-after-free 风险(§5) |

**一句话:常量映射这类"细节"全部正确(说明作者的功课很扎实),但三处结构性问题上,
文档声称的能力**大于**代码实际具备的能力。**

---

## 1. IA 状态保存/恢复:前提对,理由错

### 前提 ✅

`gpu_get_state` / `gpu_set_state` 确实**不含**任何 IA 状态。
`Function_3D.cpp:36-64`(`g_SaveRenderStates[]`)与 `:75-87`(`g_SaveSamplerStates[]`)
覆盖 blend / depth / stencil / cull / alphatest / sampler —— **零个 IA 条目**。
所以 `IaStateGuard` 手工保存/恢复是**唯一**可行做法,不是重复造轮子。

(小误:扩展注释把 `scissor` 也列进去了,但 `scissor` 并不在该表里。无关紧要。)

### 理由 ⚠️ 夸大

扩展在 `spec.gmidl:244-253` 与 `igpu_draw.h:19-20` 写:

> "A bind/draw pair would therefore leak IGPU's state into GameMaker's own drawing,
> and no GML-level call could undo it."

**这个因果链不成立。** 引擎在每次自己绘制前都会**无条件**重设 IA 三件套:

```
VertexBuilderM.cpp:793  IASetVertexBuffers(0, 1, &_pVB, &stride, &offset);
VertexBuilderM.cpp:812  IASetPrimitiveTopology(prim);
VertexBuilderM.cpp:814  IASetInputLayout(inputLayout);
VertexBuilderM.cpp:816  VSSetShader(pVShader, ...);
VertexBuilderM.cpp:822  PSSetShader(pPShader, ...);
```

全 DX11 引擎里 IA 设置点**只有这一处**(我独立 grep 验证过),且**没有任何缓存/脏标记**。
`StateManagerM.h:27-28` 明确写着 IA 状态"not included yet"。

**所以:IGPU 即使不恢复,引擎下一次绘制也会自动修复。**
`IaStateGuard` 不是"防止污染 GM 绘制"的必要手段 —— 引擎的无条件重绑才是。

**但这不等于该删。** 它仍有两个价值:(a) 让设备状态对任何直接读 IA 的代码保持一致;
(b) 若将来 GM 引入 IA 脏标记,这个保护就变成必要的。
**建议:保留实现,修正文档措辞** —— 把"防止污染 GM"改成"保持设备状态自洽,
并在 GM 缓存 IA 时提供保护"。**理由夸大会导致接手者高估它的重要性,进而在真正需要它
的场景(如未来 GM 变化)误判。**

### 实现质量 ✅

逐字段审计(全部通过 `IAGet*` 读回,不是猜):
- 顶点缓冲 **16 个 slot 全部**,含每 slot 的 stride 与 offset
- 索引缓冲:指针 + **格式** + 偏移
- 输入布局、图元拓扑
- 引用计数:先重绑后释放,顺序正确;`held_` 保证幂等

**无遗漏字段。** 唯一可议:引擎只用 slot 0,恢复全部 16 个是纯开销(4 次廉价调用,可接受)。

---

## 2. 常量映射:全部正确,且已被运行时反证

这一节是**好消息**,值得单独记下来。

| 扩展硬编码 | 引擎定义 | 位置 |
|---|---|---|
| `pr_*` = 1–6 | `ePrimType_POINTLIST=1 … TRIFAN=6` | `Graphics.h:789-807` |
| `vertex_usage_*` = 1–9 | `yyVUPOSITION=1 … yyVUBINORMAL=9` | `Vertex_Class.h:33-41` |
| `vertex_type_*` = 1–6 | `yyVTFLOAT1=1 … yyVTUBYTE4=6` | `Vertex_Class.h:10-15` |
| `"POSITION"` `"TEXCOORD"` … | `g_VertexUsageStrings[]` | `ShaderM.cpp:1336-1354` |
| `Colour` → `R8G8B8A8_UNORM` | `VERTEX_COLOR_FORMAT` | `GraphicsTypesM.h:14` |

**最强证据是现有测试本身**(`Create_0.gml:168-186`):
`igpu_vertex_format` 会把布局拿去和**编译出的字节码输入签名**校验。
- L177 断言"匹配的布局被接受"——若 `vertex_usage_position` 差一位,它会变成
  `COLOR` 而**必定被拒**。测试通过 ⇒ 常量正确。
- L186 断言"不匹配的布局被拒"——证明该校验**真的有鉴别力**,不是恒过。

**这条比读任何源码都硬**:它是在真实运行时、用真实编译器、验证真实语义名。
两个审计子代理都把"GML 常量值是否与 `yyVU*` 一致"列为**无法确定**项 —— **现已确定:一致。**

### 但有一个覆盖缺口

引擎还定义 `yyVUTESSFACTOR=10 … yyVUSAMPLE=14`(`Vertex_Class.h:42-46`),
**扩展只支持到 9**。`usage_from_int` 对 10–14 返回 false。
这不是错误(未实现就明确拒绝,符合"优雅降级"),但**文档没说明为什么**,
接手者可能以为是遗漏。**建议在 spec 里点明这个有意为之的边界。**

---

## 3. 最严重的问题:`igpu_draw` 从未真正画出东西

### 证据链

**(a) 引擎从不运行时编译着色器。** 全仓库 grep `D3DCompile` / `D3DReflect` → **零匹配**。
引擎只用预编译字节码(`D3D11ShaderDataHeader::pShader`)。

**(b) 引擎绑定着色器的地方只有一处**,且只认引擎自己的对象:

```cpp
// VertexBuilderM.cpp:658-674
if (g_ActiveUserShader != NULL) {
    D3D11Shader* pShader = g_Shaders.arr[shaderIndexM];
    pVShader = pShader->m_pVShader;      // ← 只可能来自引擎的 D3D11Shader
    pPShader = pShader->m_pPShader;
    inputLayout = pShader->GetInputLayout(_SizeOfVertex);
}
```

**(c) `spec.gmidl` 里没有 `igpu_shader_bind`。** 31 个函数里没有任何一个能把
IGPU 编译出的 `ID3D11VertexShader*` 绑到管线上。

**(d) 测试从未调用 `shader_set`。** `Create_0.gml` 全文搜索 → **零匹配**。

### 后果

`spec.gmidl:263` 写着:

> "The caller is responsible for having set a shader and any render target."

**但调用方根本无法设置 IGPU 的着色器** —— 没有这个 API。
`igpu_draw` 实际执行的是:设 IA 状态 → 调 `ID3D11DeviceContext::Draw()`。
此时管线里挂着的是 **GM 当时碰巧绑着的着色器**(若有),而该着色器的输入签名
几乎必然与 IGPU 的布局**不匹配** → 顶点数据被解释成错误语义,或干脆不画。

**"igpu_draw 返回 true"只证明"调用发出去了",不证明"画出了任何东西"。**
这解释了为什么测试里的负向断言(顶点数溢出等)全部有效,而正向断言
("draw succeeds")却空洞 —— **正向路径从未被真正验证过。**

### 建议 —— ✅ 已执行

**已新增 `igpu_shader_bind(shader, stage)` 与 `igpu_get_bound_shader(stage)`**,
补上了这条断掉的链路(共 31 → 33 个函数)。

实现要点:
- `ShaderEntry` 记录编译时的 stage,绑定时**校验 stage 匹配** ——
  把 pixel shader 传给 `VSSetShader` 这类类型错误在**绑定点**就报错,
  而不是等设备给出难以理解的失败
- `shader = 0` 为**显式解绑**(非错误),不会释放着色器
- **释放着色器会清掉绑定记录** —— 否则"我的着色器还绑着吗"会对
  已释放的句柄答"是"
- `reset()` 同步清空 `bound_shaders`,避免 shutdown/re-init 后残留陈旧句柄

**有意不自动恢复**(与 IA 状态不同):着色器是调用方主动设定的状态,
不是绘制的副作用;每次绘制后重绑 GM 的着色器会**对抗调用方自己的切换**,
且没有任何 GML 接口能读回"之前绑的是谁"。所以绑定是粘性的。
**代价**:GM 自己的绘制仍会覆盖 IGPU 的绑定,故应在需要的
`igpu_draw` **紧前**绑定 —— 这条限制已写进 spec 注释。

**测试**:新增 13 项绑定断言(总数 136 → 152),覆盖成功绑定、
按 stage 独立、stage 不匹配被拒且不留半成品、未知句柄/stage、
显式解绑、释放时清绑定记录。

**金丝雀验证**:把 stage 校验改成 `if (false)` → 测试
`CHECK FAILED : stage mismatch is rejected`、退出码 1;恢复后通过。
**这条断言是能失败的**,不是装饰。

> ⚠️ **仍未解决**:上述修复让"画得出东西"成为**可能**,
> 但**没有**证明像素真的被光栅化 —— 那仍需要 §9 第 5 项(渲染目标 API)
> 配合 `surface_getpixel` 读回。§3 的正向结论不变:
> 当时那批测试**从未**真正验证过绘制结果。


---

## 4. 能力声明虚报:共 11 项

用脚本对全部 31 个能力逐一核对"是否报 true"与"是否有对应 API",
结果 **11 项报 true 却没有任何 API 支撑**:

| 能力 | 需要但不存在 |
|---|---|
| `Texture3D` | `igpu_texture_create` 系列 |
| `TextureArray` | 同上 |
| `TextureCubemap` | 同上 |
| `MultipleRenderTargets` | `igpu_render_target_*` |
| `Instancing` | `igpu_draw_instanced` |
| `IndirectDraw` | `igpu_draw_indirect` |
| `Queries` | `igpu_query_*` |
| `Timestamps` | `igpu_timestamp_*` |
| `OcclusionQuery` | `igpu_occlusion_*` |
| `Fence` | `igpu_fence_*` |
| `Wireframe` | `igpu_set_fill_mode` |

(`igpu_capabilities.cpp:104-118` 全部 `return native`,即 Windows 上恒 true。)

对照:以下 13 项**有真实 API 支撑**,报 true 是诚实的 ——
`ShaderCompileRuntime`、`ShaderStageVertex/Pixel/Compute`、`InputLayout`、
`VertexBuffer`、`IndexBuffer`、`UniformBuffer`、`BufferResize`、`BufferReadback`、
`Draw`、`DrawIndexed`、`DrawStateRestore`。

**这直接违反项目自己的核心约束**(`spec.gmidl:38-41`):

> "Every capability that is not universally available must be queryable via
> igpu_get_capabilities() / igpu_supports(), so callers can degrade instead of
> crashing."

`igpu_supports(Instancing)` 返回 true,调用方据此去调 `igpu_draw_instanced()`
—— **该函数不存在**。这比"返回 false 说不支持"更糟:**它主动误导调用方。**

### 建议 —— 已执行

能力位已**全部改为 false**,并加注释标明"待哪个 API"。
未实现的报 false,实现某项时连同其 API 一起翻 true。

**并已加一条检查到 `verify_handover.ps1` 第 3b 组**:每个报 true 的能力,
必须能在 `spec.gmidl` 里找到对应函数。

**这条检查本身就是一次教训**(值得记下):我第一版写错了,而且是**双重错误**:

1. `$capsCpp` 是**路径**不是文本,直接喂给 `[regex]::Matches` 得 0 个匹配
   → `$nativeExprs` 为空 → 每个能力都命中 `continue` → **检查恒过**
2. 循环里我用了 `$root` 作为词根变量,而 `$root` 是脚本的**仓库根路径**
   → 覆盖后,后面所有 `Join-Path $root ...` 的检查(第 6/7/8/9 组)**静默失效**

两个 bug 叠加的结果是:**脚本照样打印"全部一致 — 交接文档可信"**,
在 11 个能力虚报、且 4 组检查已被破坏的情况下。

这正是本文件 §3 批评的那个问题的镜像 —— **一个不会失败的检查器比没有更糟,
因为它制造虚假的信心。** 已加自检("能解析出 N 个返回分支")防止 (1) 复发;
变量改名 `$apiRoot` 并在注释里写明原因防止 (2) 复发。

> **顺带记录一个验证陷阱**：我给第 2 组（幽灵 API 检查）做金丝雀时，
> 前两次"通过"**都是假象** —— 因为该组的提取规则要求 API 名写成
> **反引号 + 括号**的形式（`` `name()` ``），而我插入的是裸名字，
> 根本没进入被检查的集合。用正确格式重做后才真正触发失败。
>
> **教训**：金丝雀"没红"有两种可能 —— 检查器坏了，**或者你的突变没生效**。
> 做金丝雀时必须**确认突变真的落到了被检查的范围内**
> （本次靠 `Select-String` 确认插入位置 + 检查组计数从 38 变 39 才判定生效）。


---

## 5. 设备丢失:use-after-free 风险

`igpu_device.cpp:47-50` 有意不释放 device/context("borrowed, never release")。
但引擎的 `HandleDeviceLost()`(`Graphics_DisplayM.cpp:1225-1258`)会
**释放并重建**两者,`InvalidateD3DResources():1291-1292` 明确置空:

```cpp
GR_D3D_Device = NULL;
GR_D3D_Context = NULL;
```

IGPU **没有任何重新获取指针的路径** —— `igpu_init` 只由 GML 调一次,
且 IGPU 不检测 `DXGI_ERROR_DEVICE_REMOVED`。
设备丢失后(Alt+Tab、驱动更新、GPU 重置、切换独显),IGPU 仍持旧指针。

**严重度:高,但条件触发。** COM 引用计数可能让对象存活(取决于 IGPU 是否是最后持有者),
所以**是不是一定崩,我无法从源码断定** —— 但**没有任何机制保证它不崩**。

### 建议

- 检测 `DXGI_ERROR_DEVICE_REMOVED` / `DXGI_ERROR_DEVICE_RESET`,
  失败时清空 `initialised` 并设错误
- 暴露 `igpu_reinit(device, context, swapchain)`,或让 `igpu_init` 可重复调用
- 至少在文档里写明:**设备丢失后必须重新 init**

---

## 6. 顺带确认:渲染目标链路完全可行

用引擎源码逐环验证了 §9 第 5 项,结论比符号层更强:

```cpp
// Graphics_Surface.cpp:687,712 —— 引擎自己就是这么干的
void* pTexture = GR_Texture_Get_Surface(pSurf->tex);
Graphics::SetRenderTarget(_stage, pTexture, nullptr);
```

这正是我上一轮从 `YoYo.lib` 符号推出的链路,**引擎源码逐字确认**。
且 `surface_set_target_ext(stage, id, depth_id)` 已是 **GML 内置函数**
(`Function_Graphics.cpp:8476`),`MAX_MRTS = 4`(`Graphics.h:8`)。

**重要推论 §9 第 7 项(MRT)可能不需要新 API** —— GML 已有
`surface_set_target_ext` 可以设 stage 1–3。需要新增的其实是
**"画到 IGPU 自己的目标并读回像素"**,而不是 MRT 本身。
这一点值得在重排优先级时纳入考虑。

---

## 建议的下一步(按优先级)

1. **补 `igpu_shader_bind`**(或明确文档化其缺失)—— 否则 draw 链路是断的,§3
2. **修能力表虚报** —— 6 个能力报 false,并把"能力↔API 一致性"加进自检脚本,§4
3. **设备丢失处理** —— 至少检测 + 置错,不要求立刻做完整重建,§5
4. **像素级验证**(§9 第 5 项)—— 现在有了引擎源码,路径完全清晰,§6
5. **修正 IA 保存/恢复的文档措辞**(实现不动),§1
6. **spec 里点明 usage 10–14 的有意不支持**,§2

**关于顺序的说明**:第 1 项我之前完全没意识到 —— 我此前的报告都说
"draw 链路已跑通",那是**基于测试通过**的判断。用引擎源码一看,
"跑通"的含义比文档暗示的弱得多。**这正是这次审计最大的收获:
它不是发现了新 bug,而是发现了"我们以为已经验证过的东西其实没有"。**

---

## 附:审计方法与可信度

- **符号层**:`YoYo.lib` 实际链接 + 运行,取到真实地址(6 个符号)
- **源码层**:`OpenGM` 读到函数定义体(非反推)
- **交叉验证**:两者完全吻合
- **独立复核**:两个审计子代理 + 我本人各自 grep 验证关键结论
  (引擎不用索引缓冲、不编译着色器、IA 单一设置点)—— **三处都独立复现**

**未能确定的事**(诚实列出):
1. `GR_D3D_Device` 在设备丢失时的实际引用计数 → 需运行时插桩
2. `desc[32]`(`ShaderM.cpp:1388`)在 >32 输入时是否溢出 → 引擎自身不太可能触发
3. DX12 路径细节 → IGPU 只针对 DX11,未深挖
