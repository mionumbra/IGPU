# 引擎渲染目标 / 纹理接入点

> 用途:为 IGPU §9 第 5 项「渲染目标绑定」提供**可静态链接**的引擎入口,
> 替代 GMD3D11 那种 VMT hook 方案。
>
> 两个证据来源,互相印证:
> 1. **符号层**:`runtime-2026.0.0.23\yyc\Win32\lib\x64\YoYo.lib`
>    (x64,90.3 MB COFF 归档,482,910 个符号)—— 已实测链接成功(§5)
> 2. **源码层**:`OpenGM`(GMS2 反混淆源码,`runtime/GMS2-Runner-Main/VC_Runner/Files/`)
>    —— 已读到函数**定义体**,参数语义不再是反推(§1.1)
>
> **状态:符号可链接(实测) + 语义已从源码确认。**

---

## 1. 渲染目标(直接解决 §9 第 5 项)

```cpp
struct Graphics {
    static bool SetRenderTarget(int, void*, void*);  // ?SetRenderTarget@Graphics@@SA_NHPEAX0@Z
    static bool SaveRenderTarget(void);              // ?SaveRenderTarget@Graphics@@SA_NXZ
    static bool RestoreRenderTarget(void);           // ?RestoreRenderTarget@Graphics@@SA_NXZ
};

extern ID3D11DeviceContext*    GR_D3D_Context;              // 设备上下文
extern ID3D11RenderTargetView* g_DefaultRenderTargetView;   // 默认 RTV
extern ID3D11Texture2D*        g_DefaultRenderTargetTexture;
extern ID3D11DepthStencilView* g_DefaultDepthStencilView;
```

`SaveRenderTarget` / `RestoreRenderTarget` 的存在说明**引擎自己就在做渲染目标状态
的保存/恢复** —— 这与 IGPU 在 `igpu_draw` 里做 IA 状态保存/恢复是同一个模式,
可以直接复用它,不必自己重造。

### 1.1 参数语义(源码确认,非反推)

源码位置:`Graphics_API/DirectX11/TexturesM.cpp`

```cpp
// L2520 —— _target 是 **MRT 索引**(0 = 主目标),不是计数或标志位
bool Graphics::SetRenderTarget( int _target, void* _pTexture, void* _pDepthBuffer )
{
    Graphics::Flush();                     // ← 注意:它自己会 Flush
    if (_pTexture == NULL) {               // ← 传 NULL 表示"回到根渲染目标"
        GR_D3D_Context->OMSetRenderTargets(1, &g_DefaultRenderTargetView,
                                              g_DefaultDepthStencilView);
        g_pCurrentRenderTarget = NULL;
        return true;
    }
    Texture* pText = (Texture*)_pTexture;  // ← 必须是引擎的 Texture*,不是 D3D 纹理!
    SetupD3DTextureIfInvalid(pText);
    D3D11TextureInfo* pTextInfo = (D3D11TextureInfo*)(pText->m_pPointer);
    if (pTextInfo == NULL)      return false;
    if (pTextInfo->m_pRTView == NULL) return false;   // ← 无 RTV 则失败
    ...
    GR_D3D_Context->OMSetRenderTargets(1, &(pTextInfo->m_pRTView),
                                          pTextInfo->m_pDepthRTView);
    g_pCurrentRenderTarget = (Texture*)_pTexture;
    return true;
}
```

**三条关键结论(这解决了我上一轮标为"未验证"的疑点):**

1. **`_target` 是 MRT 索引**(0 起),不是计数/标志。`:2529` 有 `_target < 0 || >= MAX_MRTS` 检查。
2. **`_pTexture` 收的是引擎 `Texture*`,不是 `ID3D11Texture2D*`。** 内部再从
   `Texture::m_pPointer` 取 `D3D11TextureInfo*`,用它的 `m_pRTView`。
   所以**真正需要的是拿到引擎的 `Texture*`**,RTV 由引擎自己维护 —— 见 §2.1。
3. **`_pTexture == NULL` 是合法且有用的**:表示恢复默认渲染目标(backbuffer)。

`RestoreRenderTarget()`(`:2486`)等价于 `SetRenderTarget` 到 `g_pScreenTexture`
(屏幕纹理),内部同样是 `OMSetRenderTargets`,并 `Flush()`。

### 1.2 RTV 是引擎已经建好的

`Graphics_API/DirectX11/GraphicsTypesM.h:85`:

```cpp
struct D3D11TextureInfo {
    D3D11_TEXTURE2D_DESC      m_TextureDesc;
    ID3D11Texture2D*          m_pTexture;
    ID3D11ShaderResourceView* m_pView;      // SRV
    ID3D11Texture2D*          m_pAATexture; // MSAA 时用于 RTV 的解析纹理
    ID3D11RenderTargetView*   m_pRTView;    // ★ RTV —— 已建好
    ID3D11DepthStencilView*   m_pDepthRTView;
    ...
};
```

**这推翻了我上一轮的一个担忧。** 我说"SRV 只能读,要当渲染目标还得自己
`CreateRenderTargetView`,这一步 GMD3D11 没做也就没验证过"——
实际上**引擎早就把 RTV 建好并挂在 `Texture` 上了**,IGPU 只需要找到那个 `Texture*`。


## 2. Surface 内部(x64 无修饰名,可直接声明)

```cpp
struct RSurface* GR_Surface_Get(int id);   // 句柄 -> RSurface*,其余访问器的入口
__int64 GR_Surface_Get_Texture(int id);    // 另有 GR_Surface_Get_Texture_Depth
int  GR_Surface_Get_Width(int);
int  GR_Surface_Get_Height(int);
bool GR_Surface_Exists(int);
void GR_Surface_Free(int, bool);
enum eTextureFormat GR_Surface_Get_Format(int);
```

这**印证了 GMD3D11 的发现**:GM 的 surface 句柄确实能经由引擎内部拿到纹理,
不必走 VMT hook。

### 2.1 完整调用链(源码逐环确认)

从 GML 的 `surface_id` 到可用的 `Texture*`,四环全部在源码里读到了:

```cpp
// 环 1 —— Graphics/Graphics_Surface.h:137  RSurface 的真实布局
struct RSurface {
    int surf_id;    // 表面 ID
    int tex;        // ★ 对应纹理 ID(是 ID,不是指针)
    int tex_depth;  // 深度缓冲纹理 ID
    int w, h;
};

// 环 2 —— Graphics/Graphics_Surface.cpp:543
intptr_t GR_Surface_Get_Texture(int id) {
    RSurface* pSurf = GR_Surface_Get(id);
    if (pSurf != NULL) return pSurf->tex;   // 返回 tex 这个 ID
    return -1;
}

// 环 3 —— Graphics/Graphics_Texture.cpp:2825
void* GR_Texture_Get_Surface(int id) {
    RTexture* pRTex = GR_Texture_Get(id);
    if ((pRTex == NULL) || (pRTex->usingFallback)) return NULL;
    return pRTex->tex;                      // ★ 这里才是真正的 Texture*
}

// 环 4 —— 交给引擎自己绑,它会用 Texture::m_pPointer -> m_pRTView
Graphics::SetRenderTarget(0, pTexture, NULL);
```

等价于引擎内部 `Function_Graphics.cpp:5665` 等处的既有写法:

```cpp
void* pTexture = GR_Texture_Get_Surface( GR_Surface_Get_Texture( surf ) );
```

**所以完整链路是:**

```
GML surface_id
  → GR_Surface_Get(id)           → RSurface*
  → RSurface::tex                → 纹理 ID
  → GR_Texture_Get(id)           → RTexture*
  → RTexture::tex                → Texture*     ★ 这是 SetRenderTarget 要的东西
  → Graphics::SetRenderTarget(0, texture, NULL) → 引擎用 Texture::m_pPointer->m_pRTView 绑定
```

**注意 `GR_Texture_Get_Surface` 会检查 `usingFallback` 并返回 NULL** ——
这是必须处理的失败路径,不能假设一定成功。

**关键结论:不需要自己创建 RTV,也不需要反查 `ID3D11Texture2D*`。**
把引擎的 `Texture*` 交给 `SetRenderTarget`,剩下的事引擎自己做。
这正是"复用引擎词汇表"原则在引擎层的体现。


## 3. 纹理 / 绘制(可能复用于自建离屏目标)

```cpp
struct Graphics {  // 均为 public static
    static void* CreateTexture(int,int,int,eTextureFlags,eTextureFormat,unsigned char*);
    static void  FreeTexture(void*);
    static bool  CopySurface(void*,int,int,int,void*,int,eTextureFormat);
    static void  SetTexture(int, void*);
    static void  SetViewPort(int,int,int,int);
    static bool  IsTextureFormatSupported(eTextureFormat);
    static int   GetTextureFormatStride(eTextureFormat);
    static bool  ShadersSupported(void);
    static bool  Flip(void);          // 呈现
    static void  PushRenderStates(void);
    static void  PopRenderStates(void);
};
```

`Graphics::` 命名空间下共有 **81 个公开符号**,以上只是与渲染目标/纹理相关的子集。
完整列表可用 `dumpbin /linkermember:1` 自行导出。

## 4. 链接要求(实测得出,缺任一都会 LNK2019)

```
YoYo.lib d3d11.lib dxgi.lib d3dcompiler.lib ws2_32.lib winmm.lib
comdlg32.lib gdi32.lib user32.lib ole32.lib oleaut32.lib advapi32.lib
shell32.lib iphlpapi.lib mfplat.lib mfreadwrite.lib mfuuid.lib
propsys.lib comctl32.lib shlwapi.lib version.lib wininet.lib
crypt32.lib gdiplus.lib rpcrt4.lib avrt.lib pathcch.lib
```

后 20 个是 `YoYo.lib` 自身传递依赖(directshow / gdiplus / rpcrt4 / avrt 等)拉进来的。
**建议**:与其手工维护这个列表,不如用 `/DEFAULTLIB` 指令写进一个 `.obj` 集中管理。

## 5. 验证记录(2026-09-23 实测)

用 `cl` + 上述库实际链接,六个目标符号全部解析成功并取到运行地址:

```
SetRenderTarget     = 00007FF78D665F70
SaveRenderTarget    = 00007FF78D665F10
GR_Surface_Get      = 00007FF78D668000
GR_Surface_Get_Tex  = 00007FF78D6680A0
&GR_D3D_Context     = 00007FF78DAD5DA0
&g_DefaultRTV       = 00007FF78DAD5DA8
```

**"能链接"已被证明;"行为符合预期"尚未证明** —— 见 §6。

## 6. 重要警告(动手前必读)

1. **这是引擎内部符号,不是官方稳定 API。** GM 升级后随时可能改名、改签名、
   改语义。IGPU 若要采用,**必须做版本化绑定**(运行时探测 + 失败即降级),
   不能硬编码进核心路径。

2. **YYC 专用。** `YoYo.lib` 只存在于 YYC 运行时。VM 模式没有这个库,
   所以必须 `igpu_supports(...)` 可查、优雅降级 —— 这正是本项目既有的设计约束。

3. **签名语义已从源码确认(§1.1 / §2.1),但仍需行为验证。**
   与上一版不同:我不再是"从修饰名反推"。`_target` 是 MRT 索引、
   `_pTexture` 是引擎 `Texture*`、`NULL` 表示回根目标 —— 这些都在
   `TexturesM.cpp:2520` 的定义体里读到。
   **但"源码在某个 GM 版本里如此"不等于"你机器上跑的这个版本如此"**,
   动手时仍应先用探针确认(传已知 surface,看是否真的绑上)。

4. **不要用 VMT hook。** GMD3D11 篡改 `ID3D11DeviceContext` 虚表来拦截 `Draw`,
   那依赖硬编码的虚表索引,GM 一升级就静默错位。
   这些符号**能静态链接**,无需虚表 hook —— 风险等级完全不同。

5. **`os_get_info()` 仍是更安全的来源**(官方文档化),
   用于拿 device/context/swapchain。本文件提供的是**渲染目标**方面的补充,
   因为 `os_get_info()` 不暴露任何 surface→RTV 的能力。
   能靠官方接口拿到的,继续用官方接口。

6. **源码仓库 `OpenGM` 是反混淆产物,不是官方源码。**
   它能极大提高"理解引擎意图"的效率,但**结论仍应以本机 YoYo.lib 的符号为准**。
   两者目前完全吻合(见 §1.1 与符号表),这份吻合本身就是交叉验证。


## 7. 复现命令

```pwsh
# 导出全部符号(约 48 万行,耗时数十秒)
$dumpbin = "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.29.30133\bin\HostX64\x64\dumpbin.exe"
$lib = "C:\ProgramData\GameMakerStudio2-LTS2026\Cache\runtimes\runtime-2026.0.0.23\yyc\Win32\lib\x64\YoYo.lib"
& $dumpbin /symbols $lib > syms.txt

# 只看公开可链接符号索引
& $dumpbin /linkermember:1 $lib > index.txt

# 查某个目标
Select-String -Path index.txt -Pattern 'SetRenderTarget@Graphics'
```
