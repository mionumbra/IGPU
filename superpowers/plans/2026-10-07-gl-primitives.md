# OpenGL 点、线、三角带实现计划

> **给执行的代理：** 必须使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans，按任务逐个实现。步骤用复选框（`- [ ]`）跟踪。

**目标：** 让 OpenGL 探针里的 `igpu_draw`、`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 接受图元 1、2、3、5（点列表、线列表、线带、三角带），并用 8×8 纹素证明画的是这个图元，不是三角形列表。

**架构：** 公开函数和门面已经存在。`igpu::draw` 在 `require_device` 之后调用 `Backend::draw`。三个 OpenGL 绘制函数现在只放行图元 4，并且把 `glDrawArrays` / `glDrawElements` 写成 `GL_TRIANGLES`。在 `igpu_gl_draw.cpp` 里加一个图元到 `GLenum` 的转换，三处绘制共用它。图元 6 仍拒绝。非零的混合、深度、光栅、采样器仍拒绝。不调用 `glPointSize`、`glLineWidth`、`glPolygonMode`，也不打开平滑、点精灵或背面剔除。Windows 的 GameMaker DLL 不编译 `native/gl/`。

**技术栈：** C++17，Windows `GL/gl.h` 里已有的 `GL_POINTS`、`GL_LINES`、`GL_LINE_STRIP`、`GL_TRIANGLES`、`GL_TRIANGLE_STRIP`，WGL 探针，CMake 预设 `win-x64-release-vs18`，MSVC。编译进 MSVC 的 cpp 注释只用 ASCII。

**规格：** 这一刀是会话里选定的下一刀：图元 1、2、3、5。站立约束在 `SESSION_HANDOVER.md` 开头和「必须守住的结构」。`superpowers/specs/2026-10-06-slim-api-design.md` 不改。批准执行时，把本计划另存为 `superpowers/plans/2026-10-07-gl-primitives.md` 并随第一个提交入库。

## 全局约束

- 公共头、`spec.gmidl` 和 GML 不出现 d3d、dxgi、hlsl、`_5_0`、ID3D。方言字符串 `hlsl` / `glsl` / `glsl_es` / `msl` / `spirv` 仍然允许。
- 调用链保持 `spec.gmidl` → 生成的 `code_gen` → `IGPU_native.cpp` → `igpu_gpu.cpp` 里的 `igpu::` → `Backend`。不改生成文件。不跑 extgen。签名不变。
- 门面在虚函数调用之前执行 `require_device`。虚函数不写默认参数。
- GameMaker 颜色是 BGR。这一刀的片元着色器是常量红，读回是 `255`。清屏是 `0`。
- MSVC 按代码页 936 读 cpp。编译进 MSVC 的 cpp 注释只用 ASCII。中文注释里的字节 `0x5C` 会吃掉下一行。
- 能力表不动。`Draw` 已经是 `native || opengl_backend()`。不要把别的键改成 true。键数保持 35。键清单代码块里不要写 `surface_*`。不要新写「N 个键」这种句子。
- 检查项保持 **878**。版本保持 `0.5.0`。`HANDOVER.md` 里的 `HEAD` 必须是仓库里真实存在的提交。
- OpenGL 只在探针里。Windows 的 GameMaker DLL 构建时不定义 `IGPU_HAS_OPENGL`。不要加 ANGLE。不要改三个指针的 `igpu_init`。`wglCreateContext`、`wglDeleteContext`、`wglMakeCurrent` 只留在 `tests/gl_probe`。
- DLL 的源文件 GLOB 保持 `native/*.cpp` 和 `native/d3d11/*.cpp`。不要把 `native/gl/` 加进 `src/CMakeLists.txt` 的 DLL 目标。
- 这一刀不开采样器、混合、深度、多目标、立方体、三维、实例化、间接、计算、统一缓冲、查询。不让 `glsl_es` 编译成功。不做 ES 2。四个状态句柄必须是 0。图元 6 拒绝。图元 0 和 7 拒绝。
- `first_index + index_count` 和 `first_vertex + vertex_count` 在特别大的正数上可能回绕。这一刀不改这道算术。探针只用很小的计数。
- 借用的上下文：探针仍先 `igpu_shutdown()`，再 `wglDeleteContext`。`reset()` 仍先清掉活动后端。
- 默认点大小是 1，线宽是 1，`GL_POINT_SMOOTH`、`GL_LINE_SMOOTH`、`GL_CULL_FACE` 保持关闭。不要为了让点或线更好看而改这些状态。

## 复查重点

下面这些输入，单独看到「某个采样点变红」时仍会漏掉。每一条都在对应任务的探针里有检查。

- 四顶点三角带若被当成 `GL_TRIANGLES`，只画出第一条三角形。样本在第二条三角形里，左采样点必须保持 `0`。任务 1 检查。
- 四顶点线列表若被当成线带，会把两段之间的空隙也涂上。空隙纹素 `(3,4)` 必须保持 `0`，而 `(1,4)` 和 `(6,4)` 是 `255`。任务 3 检查。
- 三顶点线带若被当成 `GL_LINES`，第三个顶点被丢掉，右采样点保持 `0`。同一次绘制必须让 `(1,4)` 和 `(6,4)` 都是 `255`。索引路径同样检查。任务 3 检查。
- 点在纹素中心，点大小保持 1。邻纹素 `(2,4)` 和 `(1,3)` 保持 `0`。`GL_POINT_SIZE` 和 `GL_LINE_WIDTH` 仍是 `1`。任务 3 检查。
- 图元 5 配任何一个非 0 状态句柄，以及图元 0、6、7，都拒绝，两个采样点保持 `0`。`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 使用同一个模式，不能留下 `GL_TRIANGLES`。任务 1 检查三角带和拒绝；任务 3 检查索引线带和画进纹理的线段。

---

## 文件职责

- 修改 `src/native/gl/igpu_gl_draw.cpp` — 增加 `gl_primitive_mode` 和 `gl_draw_accepted`，三处绘制改用返回的 `GLenum`。
- 修改 `tests/gl_probe/main.cpp` — 像素证明和拒绝。
- 修改 `SESSION_HANDOVER.md`，以及 `HANDOVER.md` 里仍写着「下一刀还没定」的短句。
- 不改 `spec.gmidl`、`code_gen/`、`project/`、`src/CMakeLists.txt`、`src/native/igpu_capabilities.cpp`、`src/native/igpu_gpu.cpp`、`src/native/gl/igpu_gl_backend.cpp`、`src/native/gl/igpu_gl_draw.h`。

`igpu_draw` 和 `igpu_draw_indexed` 仍画进调用方已经 current 的帧缓冲，不清屏，不改帧缓冲、视口和程序。`igpu_draw_to_render_targets` 仍自己绑定目标并在返回前恢复 `GL_FRAMEBUFFER_BINDING` 和 `GL_VIEWPORT`。`gl_draw_to_render_targets_layer` 仍用 `bind_vertices(..., first_vertex)` 做偏移，`glDrawArrays` 的起始顶点保持 `0`，只改模式。

## 为什么样本不能沿用现在的半屏四边形

现在的左半屏三角形列表，第一条三角形已经盖住 `(1,4)`。把同样的四个角点当成三角带时，错用 `GL_TRIANGLES` 仍会把左采样点涂红。这一刀的三角带把样本放在第二条三角形里。

8×8 视口里，窗口坐标 `window = (ndc + 1) * 4`。这些 `ndc` 都是 1/8 的倍数，float32 能精确表示。`igpu_texture_read` 的 `y = 0` 对应 GL 行 `height - 1`。

| 纹素 (x, y) | GL 行 | 纹素中心的 ndc |
|---|---|---|
| (1, 4) | 3 | (-0.625, -0.125) |
| (6, 4) | 3 | (0.625, -0.125) |
| (3, 4) | 3 | (-0.125, -0.125) |
| (1, 1) | 6 | 与画线的那一行不是同一行 |

三角带四个顶点，偶数三角形是 `n, n+1, n+2`，奇数三角形是 `n+1, n, n+2`。左边四顶点：

```text
v0 (0, -1), v1 (0, 1), v2 (-0.25, -1), v3 (-1, 0)
```

- 三角形 0：`(0,-1), (0,1), (-0.25,-1)`。不包含 `(-0.625, -0.125)`。
- 三角形 1：`(-0.25,-1), (0,1), (-1,0)`。包含 `(-0.625, -0.125)`，不包含 `(0.625, -0.125)`。

右边是 x 取反：

```text
v4 (0, -1), v5 (0, 1), v6 (0.25, -1), v7 (1, 0)
```

三角形 0 不包含 `(0.625, -0.125)`，三角形 1 包含它。因此 `GL_TRIANGLES` 加 4 个顶点时，两个采样点都保持 `0`。

点放在 `(1,4)` 和 `(6,4)` 的纹素中心。边长 1 的正方形只盖住这个纹素中心，邻纹素中心距离是 1，在正方形外。

线是水平的，穿过 GL 行 3 的纹素中心，也就是 `ndc y = -0.125`。线列表两段分别是纹素 0 的中心到纹素 2 的中心，以及纹素 5 的中心到纹素 7 的中心。`(1,4)` 和 `(6,4)` 在段的内部，`(3,4)` 在空隙里。线带三个顶点是纹素 0、2、7 的中心。一次画出三个顶点时，两段都画；若当成 `GL_LINES`，第三顶点被丢下，右边保持 `0`。`(1,1)` 不在这条线上，必须保持 `0`。

不要改这些坐标去迁就一次读回。若探针读回和上表不一致，先打印 8×8，再查模式、顶点和有没有改点大小或线宽。

## 分支

从 `main` 开新分支。不要在 `main` 上提交。

```powershell
git switch main
git switch -c gl-primitives
```

第一次改代码之前，先确认树还是绿的：

```powershell
pwsh -File tools\verify_handover.ps1
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：脚本退出码 0。探针退出码 0，并打印下面十行，然后是 `PASS`：

```text
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

可执行文件在 `src\Release\igpu_gl_probe.exe`，不在预设目录的 `Release` 根上。不要跑 `gm-cli`。DLL 不属于这一刀。

---

### Task 1: Triangle strip probe fails first

### 任务 1：三角带探针先失败

**文件：**
- 修改：`tests/gl_probe/main.cpp`（插在 `void* context = gl_probe_context();` 之前）
- 测试：同一个文件。这个仓库没有单独的 GL 单元测试程序。

**接口：**
- 使用：`igpu_draw(vertex_buffer, instance_buffer, layout, primitive, first_vertex, vertex_count, instance_count, blend, depth, raster, sampler)`。`instance_buffer` 是 0，`instance_count` 是 1。
- 使用：`igpu_draw_indexed(vertex_buffer, layout, index_buffer, primitive, first_index, index_count, blend, depth, raster, sampler)`。
- 使用：`igpu_draw_to_render_targets(...)`，以及文件里已经有的 `encode_u64`、`as_array`、`layout`。常量红片元着色器此时已经重新绑上。
- 产出：实现前探针退出码 1。实现后多五行，见步骤 4。

- [ ] **步骤 1：插入会失败的探针段**

把下面这段插进 `tests/gl_probe/main.cpp`，紧挨在 `void* context = gl_probe_context();` 之前。

```cpp
    const float strip_vertices[] = {
        0.f, -1.f, 0.f, 1.f, -0.25f, -1.f, -1.f, 0.f,
        0.f, -1.f, 0.f, 1.f, 0.25f, -1.f, 1.f, 0.f,
    };
    const std::uint16_t strip_indices[] = {0, 1, 2, 3, 4, 5, 6, 7};
    const auto strip_buffer = igpu_buffer_create(64, 0, 1, 8);
    const auto strip_index_buffer = igpu_buffer_create(16, 0, 2, 0);
    const auto strip_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto strip_indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto strip_target_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (strip_buffer == 0 || strip_index_buffer == 0 || strip_texture == 0 ||
        strip_indexed_texture == 0 || strip_target_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(strip_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(strip_vertices), 64)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(strip_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(strip_indices), 16)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    auto strip_pixels_are = [&](std::int64_t texture, std::int64_t left, std::int64_t right,
                                const char* label) {
        const auto got_left = igpu_texture_read(static_cast<std::uint64_t>(texture), 1, 4, 0, 0);
        const auto got_right = igpu_texture_read(static_cast<std::uint64_t>(texture), 6, 4, 0, 0);
        if (got_left != left || got_right != right)
        {
            std::fprintf(stderr, "%s left=%lld right=%lld\n", label,
                         static_cast<long long>(got_left), static_cast<long long>(got_right));
            return false;
        }
        return true;
    };
    if (!strip_pixels_are(strip_texture, 0, 0, "fresh strip texture"))
    {
        return fail("fresh strip texture was not clear black");
    }

    std::int32_t strip_saved_fbo = 0;
    std::int32_t strip_held_viewport[4] = {};
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("strip color target begin failed");
    }
    const auto reject_strip = [&](std::int32_t primitive, std::int64_t blend, std::int64_t depth,
                                  std::int64_t raster, std::int64_t sampler, const char* what,
                                  const char* error_part) {
        if (igpu_draw(static_cast<std::uint64_t>(strip_buffer), 0, static_cast<std::uint64_t>(layout),
                      primitive, 0, 4, 1, blend, depth, raster, sampler))
        {
            igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
            std::fprintf(stderr, "%s was accepted\n", what);
            return false;
        }
        if (igpu_get_last_error().find(error_part) == std::string::npos)
        {
            igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
            std::fprintf(stderr, "%s error: %s\n", what, igpu_get_last_error().c_str());
            return false;
        }
        return true;
    };
    if (!reject_strip(0, 0, 0, 0, 0, "primitive 0", "does not draw this primitive") ||
        !reject_strip(7, 0, 0, 0, 0, "primitive 7", "does not draw this primitive") ||
        !reject_strip(6, 0, 0, 0, 0, "triangle fan", "trianglefan") ||
        !reject_strip(5, 1, 0, 0, 0, "strip blend", "draws with no extra state") ||
        !reject_strip(5, 0, 1, 0, 0, "strip depth", "draws with no extra state") ||
        !reject_strip(5, 0, 0, 1, 0, "strip raster", "draws with no extra state") ||
        !reject_strip(5, 0, 0, 0, 1, "strip sampler", "draws with no extra state"))
    {
        return fail("strip rejection check failed");
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (!strip_pixels_are(strip_texture, 0, 0, "rejected strip"))
    {
        return fail("a rejected strip draw changed a pixel");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("strip color target begin failed");
    }
    GLint strip_fbo = 0;
    GLint strip_viewport[4] = {};
    GLint strip_program = 0;
    glGetIntegerv(0x8CA6, &strip_fbo);
    glGetIntegerv(GL_VIEWPORT, strip_viewport);
    glGetIntegerv(0x8B8D, &strip_program);
    if (!igpu_draw(static_cast<std::uint64_t>(strip_buffer), 0, static_cast<std::uint64_t>(layout),
                   5, 0, 4, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    GLint strip_fbo_after = 0;
    GLint strip_viewport_after[4] = {};
    GLint strip_program_after = 0;
    GLboolean strip_cull = glIsEnabled(0x0B44);
    glGetIntegerv(0x8CA6, &strip_fbo_after);
    glGetIntegerv(GL_VIEWPORT, strip_viewport_after);
    glGetIntegerv(0x8B8D, &strip_program_after);
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    if (strip_cull != GL_FALSE || strip_fbo_after != strip_fbo || strip_program_after != strip_program ||
        strip_viewport_after[0] != strip_viewport[0] || strip_viewport_after[1] != strip_viewport[1] ||
        strip_viewport_after[2] != strip_viewport[2] || strip_viewport_after[3] != strip_viewport[3])
    {
        return fail("igpu_draw strip changed the framebuffer, viewport, program, or cull face");
    }
    const auto strip_left = igpu_texture_read(static_cast<std::uint64_t>(strip_texture), 1, 4, 0, 0);
    const auto strip_right = igpu_texture_read(static_cast<std::uint64_t>(strip_texture), 6, 4, 0, 0);
    std::printf("strip pixels  : %lld / %lld\n", static_cast<long long>(strip_left),
                static_cast<long long>(strip_right));
    if (strip_left != 255 || strip_right != 0)
    {
        return fail("triangle strip did not paint only the left sample");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("strip color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(strip_buffer), 0, static_cast<std::uint64_t>(layout),
                   5, 4, -1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto strip_tail_left = igpu_texture_read(static_cast<std::uint64_t>(strip_texture), 1, 4, 0, 0);
    const auto strip_tail_right = igpu_texture_read(static_cast<std::uint64_t>(strip_texture), 6, 4, 0, 0);
    std::printf("strip tail    : %lld / %lld\n", static_cast<long long>(strip_tail_left),
                static_cast<long long>(strip_tail_right));
    if (strip_tail_left != 255 || strip_tail_right != 255)
    {
        return fail("triangle strip tail did not paint only the right sample");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_indexed_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("indexed strip color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(strip_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(strip_index_buffer), 5, 0, 4, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto strip_index_left = igpu_texture_read(static_cast<std::uint64_t>(strip_indexed_texture), 1, 4, 0, 0);
    const auto strip_index_right = igpu_texture_read(static_cast<std::uint64_t>(strip_indexed_texture), 6, 4, 0, 0);
    std::printf("strip indexed : %lld / %lld\n", static_cast<long long>(strip_index_left),
                static_cast<long long>(strip_index_right));
    if (strip_index_left != 255 || strip_index_right != 0)
    {
        return fail("indexed triangle strip did not paint only the left sample");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(strip_indexed_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("indexed strip color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(strip_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(strip_index_buffer), 5, 4, -1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto strip_index_tail_left =
        igpu_texture_read(static_cast<std::uint64_t>(strip_indexed_texture), 1, 4, 0, 0);
    const auto strip_index_tail_right =
        igpu_texture_read(static_cast<std::uint64_t>(strip_indexed_texture), 6, 4, 0, 0);
    std::printf("strip index tail: %lld / %lld\n", static_cast<long long>(strip_index_tail_left),
                static_cast<long long>(strip_index_tail_right));
    if (strip_index_tail_left != 255 || strip_index_tail_right != 255)
    {
        return fail("indexed triangle strip tail did not paint only the right sample");
    }

    const auto strip_target_bytes = encode_u64(static_cast<std::uint64_t>(strip_target_texture));
    const auto strip_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(strip_buffer), static_cast<std::uint64_t>(layout),
                                     5, 0, 4, as_array(strip_target_bytes), as_array(strip_zero_bytes),
                                     as_array(strip_zero_bytes), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    GLint strip_target_viewport[4] = {};
    GLint strip_target_fbo = 0;
    glGetIntegerv(GL_VIEWPORT, strip_target_viewport);
    glGetIntegerv(0x8CA6, &strip_target_fbo);
    if (strip_target_viewport[2] != 1 || strip_target_viewport[3] != 1 || strip_target_fbo != 0)
    {
        return fail("strip draw-to-targets left the framebuffer or viewport bound");
    }
    const auto strip_target_left = igpu_texture_read(static_cast<std::uint64_t>(strip_target_texture), 1, 4, 0, 0);
    const auto strip_target_right = igpu_texture_read(static_cast<std::uint64_t>(strip_target_texture), 6, 4, 0, 0);
    std::printf("strip target  : %lld / %lld\n", static_cast<long long>(strip_target_left),
                static_cast<long long>(strip_target_right));
    if (strip_target_left != 255 || strip_target_right != 0)
    {
        return fail("draw-to-targets triangle strip did not paint only the left sample");
    }
```

- [ ] **步骤 2：跑探针，确认失败**

```powershell
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：退出码 1。失败发生在图元 0 的错误文本上，当前文本仍是 `only draws a triangle list`，其中没有 `does not draw this primitive`。不要改探针去迁就旧文本。

- [ ] **步骤 3：提交失败的探针**

```powershell
git add tests/gl_probe/main.cpp
git commit -m "test: expect OpenGL triangle strips to paint the second triangle"
```

---

### Task 2: Implement triangle strips; primitives 1, 2, and 3 still fail

### 任务 2：三角带实现，图元 1、2、3 仍拒绝

**文件：**
- 修改：`src/native/gl/igpu_gl_draw.cpp`
- 测试：`tests/gl_probe/main.cpp`（任务 1 已经插入，这一任务不改它）

**接口：**
- 使用：任务 1 的探针。
- 产出：`gl_primitive_mode(std::int32_t primitive, GLenum& mode)` 和 `gl_draw_accepted(...)`。图元 4 返回 `GL_TRIANGLES`，图元 5 返回 `GL_TRIANGLE_STRIP`。图元 1、2、3 在这一任务里仍返回 false。三个绘制函数使用返回的模式。

- [ ] **步骤 1：在匿名命名空间里加上转换**

放在 `igpu_gl_draw.cpp` 的匿名命名空间中，`unbind_vertices` 之后。注释只用 ASCII。

```cpp
        bool gl_primitive_mode(std::int32_t primitive, GLenum& mode)
        {
            switch (primitive)
            {
            case 4: mode = GL_TRIANGLES; return true;
            case 5: mode = GL_TRIANGLE_STRIP; return true;
            default: return false;
            }
        }

        bool gl_draw_accepted(const char* entry, std::int32_t primitive, std::int64_t blend_state,
                              std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state,
                              GLenum& mode)
        {
            if (primitive == 6)
            {
                set_last_error(std::string(entry) + ": the 'trianglefan' primitive has no backend equivalent");
                return false;
            }
            if (!gl_primitive_mode(primitive, mode))
            {
                set_last_error(std::string(entry) + ": the opengl backend does not draw this primitive");
                return false;
            }
            if (blend_state != 0 || depth_state != 0 || raster_state != 0 || sampler_state != 0)
            {
                set_last_error(std::string(entry) + ": the opengl backend draws with no extra state");
                return false;
            }
            return true;
        }
```

- [ ] **步骤 2：三处绘制改用它**

`gl_draw_to_render_targets_layer`、`gl_draw`、`gl_draw_indexed` 各自删掉开头的 `primitive == 6` 判断，以及含有 `primitive != 4` 的那条判断。换成：

```cpp
        GLenum mode = GL_TRIANGLES;
        if (!gl_draw_accepted("igpu_draw", primitive, blend_state, depth_state, raster_state, sampler_state, mode))
        {
            return false;
        }
```

`igpu_draw_indexed` 的 entry 字符串是 `"igpu_draw_indexed"`。`igpu_draw_to_render_targets` 的 entry 字符串是 `"igpu_draw_to_render_targets"`。目标个数、layer、mip 的检查留在 `gl_draw_accepted` 之后，不要删。

然后把三处绘制调用改成这个模式：

```cpp
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
```

```cpp
        glDrawElements(mode, static_cast<GLsizei>(index_count), GL_UNSIGNED_SHORT,
                       reinterpret_cast<const void*>(static_cast<std::uintptr_t>(first_index * kIndexSize)));
```

`gl_draw_to_render_targets_layer` 保持起始顶点 0，因为 `first_vertex` 已经进了属性指针：

```cpp
        glDrawArrays(mode, 0, static_cast<GLsizei>(vertex_count));
```

不要调用 `glPointSize`、`glLineWidth`、`glEnable`、`glPolygonMode`、`glCullFace`。

- [ ] **步骤 3：跑探针，确认通过**

```powershell
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：退出码 0。原有十行仍在，后面多这五行，然后是 `PASS`：

```text
strip pixels  : 255 / 0
strip tail    : 255 / 255
strip indexed : 255 / 0
strip index tail: 255 / 255
strip target  : 255 / 0
```

若左边仍是 `0`，说明画成了三角形列表，或者顶点顺序被改成了半屏四边形。不要放宽断言。

- [ ] **步骤 4：提交**

```powershell
git add src/native/gl/igpu_gl_draw.cpp
git commit -m "feat: draw OpenGL triangle strips"
```

---

### Task 3: Point and line probe fails first

### 任务 3：点和线的探针先失败

**文件：**
- 修改：`tests/gl_probe/main.cpp`（插在任务 1 那段的 `strip target` 检查之后、`void* context = gl_probe_context();` 之前）
- 测试：同一个文件。

**接口：**
- 使用：任务 2 的 `gl_primitive_mode`。这一任务结束时，图元 1、2、3 仍被拒绝，所以新探针退出码是 1。
- 产出：任务 4 落地后多八行，见任务 4 步骤 2。

- [ ] **步骤 1：插入点和线的探针**

```cpp
    const float point_vertices[] = {-0.625f, -0.125f, 0.625f, -0.125f};
    const std::uint16_t point_indices[] = {0, 1};
    const auto point_buffer = igpu_buffer_create(16, 0, 1, 8);
    const auto point_index_buffer = igpu_buffer_create(4, 0, 2, 0);
    const auto point_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto point_indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (point_buffer == 0 || point_index_buffer == 0 || point_texture == 0 || point_indexed_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(point_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(point_vertices), 16)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(point_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(point_indices), 4)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(point_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("point color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(point_buffer), 0, static_cast<std::uint64_t>(layout),
                   1, 0, 1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto point_left = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 1, 4, 0, 0);
    const auto point_right = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 6, 4, 0, 0);
    const auto point_neighbor_x = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 2, 4, 0, 0);
    const auto point_neighbor_y = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 1, 3, 0, 0);
    std::printf("point pixels  : %lld / %lld\n", static_cast<long long>(point_left),
                static_cast<long long>(point_right));
    if (point_left != 255 || point_right != 0 || point_neighbor_x != 0 || point_neighbor_y != 0)
    {
        return fail("point list did not light only the left pixel center");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(point_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("point color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(point_buffer), 0, static_cast<std::uint64_t>(layout),
                   1, 1, -1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    GLfloat point_size = 0.f;
    GLfloat line_width = 0.f;
    glGetFloatv(0x0B11, &point_size);
    glGetFloatv(0x0B21, &line_width);
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto point_tail_left = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 1, 4, 0, 0);
    const auto point_tail_right = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 6, 4, 0, 0);
    const auto point_tail_neighbor = igpu_texture_read(static_cast<std::uint64_t>(point_texture), 5, 4, 0, 0);
    std::printf("point tail    : %lld / %lld\n", static_cast<long long>(point_tail_left),
                static_cast<long long>(point_tail_right));
    if (point_tail_left != 255 || point_tail_right != 255 || point_tail_neighbor != 0 ||
        point_size != 1.f || line_width != 1.f)
    {
        return fail("point tail changed the point size, line width, or a neighbor");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(point_indexed_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("indexed point color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(point_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(point_index_buffer), 1, 0, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto point_index_left = igpu_texture_read(static_cast<std::uint64_t>(point_indexed_texture), 1, 4, 0, 0);
    const auto point_index_right = igpu_texture_read(static_cast<std::uint64_t>(point_indexed_texture), 6, 4, 0, 0);
    std::printf("point indexed : %lld / %lld\n", static_cast<long long>(point_index_left),
                static_cast<long long>(point_index_right));
    if (point_index_left != 255 || point_index_right != 0)
    {
        return fail("indexed point did not light only the left pixel");
    }

    const float line_vertices[] = {
        -0.875f, -0.125f, -0.375f, -0.125f, 0.375f, -0.125f, 0.875f, -0.125f,
    };
    const auto line_buffer = igpu_buffer_create(32, 0, 1, 8);
    const auto line_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto line_target_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (line_buffer == 0 || line_texture == 0 || line_target_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(line_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(line_vertices), 32)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(line_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("line color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(line_buffer), 0, static_cast<std::uint64_t>(layout),
                   2, 0, 4, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto line_left = igpu_texture_read(static_cast<std::uint64_t>(line_texture), 1, 4, 0, 0);
    const auto line_gap = igpu_texture_read(static_cast<std::uint64_t>(line_texture), 3, 4, 0, 0);
    const auto line_right = igpu_texture_read(static_cast<std::uint64_t>(line_texture), 6, 4, 0, 0);
    const auto line_off = igpu_texture_read(static_cast<std::uint64_t>(line_texture), 1, 1, 0, 0);
    std::printf("line pixels   : %lld / %lld / %lld\n", static_cast<long long>(line_left),
                static_cast<long long>(line_gap), static_cast<long long>(line_right));
    if (line_left != 255 || line_gap != 0 || line_right != 255 || line_off != 0)
    {
        return fail("line list did not keep the gap and the off-row pixel clear");
    }

    const float line_strip_vertices[] = {-0.875f, -0.125f, -0.375f, -0.125f, 0.875f, -0.125f};
    const std::uint16_t line_strip_indices[] = {0, 1, 2};
    const auto line_strip_buffer = igpu_buffer_create(24, 0, 1, 8);
    const auto line_strip_index_buffer = igpu_buffer_create(6, 0, 2, 0);
    const auto line_strip_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto line_strip_indexed_texture = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (line_strip_buffer == 0 || line_strip_index_buffer == 0 || line_strip_texture == 0 ||
        line_strip_indexed_texture == 0 ||
        !igpu_buffer_write(static_cast<std::uint64_t>(line_strip_buffer), 0,
                           gm::wire::GMBuffer(const_cast<float*>(line_strip_vertices), 24)) ||
        !igpu_buffer_write(static_cast<std::uint64_t>(line_strip_index_buffer), 0,
                           gm::wire::GMBuffer(const_cast<std::uint16_t*>(line_strip_indices), 6)))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(line_strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("line strip color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(line_strip_buffer), 0, static_cast<std::uint64_t>(layout),
                   3, 0, 2, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto line_strip_left = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 1, 4, 0, 0);
    const auto line_strip_right = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 6, 4, 0, 0);
    std::printf("line strip    : %lld / %lld\n", static_cast<long long>(line_strip_left),
                static_cast<long long>(line_strip_right));
    if (line_strip_left != 255 || line_strip_right != 0)
    {
        return fail("line strip first segment painted the right sample");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(line_strip_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("line strip color target begin failed");
    }
    if (!igpu_draw(static_cast<std::uint64_t>(line_strip_buffer), 0, static_cast<std::uint64_t>(layout),
                   3, 1, -1, 1, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto line_strip_tail_left = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 1, 4, 0, 0);
    const auto line_strip_tail_right = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 6, 4, 0, 0);
    const auto line_strip_off = igpu_texture_read(static_cast<std::uint64_t>(line_strip_texture), 1, 1, 0, 0);
    std::printf("line strip tail: %lld / %lld\n", static_cast<long long>(line_strip_tail_left),
                static_cast<long long>(line_strip_tail_right));
    if (line_strip_tail_left != 255 || line_strip_tail_right != 255 || line_strip_off != 0)
    {
        return fail("line strip tail did not light the right sample on the same row");
    }
    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(line_strip_indexed_texture), strip_saved_fbo,
                                     strip_held_viewport))
    {
        return fail("indexed line strip color target begin failed");
    }
    if (!igpu_draw_indexed(static_cast<std::uint64_t>(line_strip_buffer), static_cast<std::uint64_t>(layout),
                           static_cast<std::uint64_t>(line_strip_index_buffer), 3, 0, 3, 0, 0, 0, 0))
    {
        igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
        return fail(igpu_get_last_error().c_str());
    }
    igpu::gl_color_target_end(strip_saved_fbo, strip_held_viewport);
    const auto line_index_left =
        igpu_texture_read(static_cast<std::uint64_t>(line_strip_indexed_texture), 1, 4, 0, 0);
    const auto line_index_right =
        igpu_texture_read(static_cast<std::uint64_t>(line_strip_indexed_texture), 6, 4, 0, 0);
    std::printf("line indexed  : %lld / %lld\n", static_cast<long long>(line_index_left),
                static_cast<long long>(line_index_right));
    if (line_index_left != 255 || line_index_right != 255)
    {
        return fail("indexed line strip did not light both samples in one draw");
    }

    const auto line_target_bytes = encode_u64(static_cast<std::uint64_t>(line_target_texture));
    const auto line_zero_bytes = encode_u64(0);
    if (!igpu_draw_to_render_targets(static_cast<std::uint64_t>(line_buffer), static_cast<std::uint64_t>(layout),
                                     2, 0, 2, as_array(line_target_bytes), as_array(line_zero_bytes),
                                     as_array(line_zero_bytes), 0, 0, 0, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    GLint line_target_viewport[4] = {};
    GLint line_target_fbo = 0;
    glGetIntegerv(GL_VIEWPORT, line_target_viewport);
    glGetIntegerv(0x8CA6, &line_target_fbo);
    if (line_target_viewport[2] != 1 || line_target_viewport[3] != 1 || line_target_fbo != 0)
    {
        return fail("line draw-to-targets left the framebuffer or viewport bound");
    }
    const auto line_target_left = igpu_texture_read(static_cast<std::uint64_t>(line_target_texture), 1, 4, 0, 0);
    const auto line_target_right = igpu_texture_read(static_cast<std::uint64_t>(line_target_texture), 6, 4, 0, 0);
    std::printf("line target   : %lld / %lld\n", static_cast<long long>(line_target_left),
                static_cast<long long>(line_target_right));
    if (line_target_left != 255 || line_target_right != 0)
    {
        return fail("draw-to-targets line did not paint only the left sample");
    }
```

- [ ] **步骤 2：跑探针，确认失败**

```powershell
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：退出码 1。stderr 里的错误含有 `does not draw this primitive`。任务 1 的五行已经打印过。不要在这一步改 `gl_primitive_mode`。

- [ ] **步骤 3：提交失败的探针**

```powershell
git add tests/gl_probe/main.cpp
git commit -m "test: expect OpenGL points and lines to light exact texels"
```

---

### Task 4: Accept points, line lists, and line strips

### 任务 4：补上点、线列表和线带

**文件：**
- 修改：`src/native/gl/igpu_gl_draw.cpp` 里的 `gl_primitive_mode`
- 测试：任务 3 的探针

**接口：**
- 使用：任务 2 的 `gl_draw_accepted`。三个绘制函数已经把 `mode` 传给 `glDrawArrays` / `glDrawElements`。
- 产出：图元 1 → `GL_POINTS`，图元 2 → `GL_LINES`，图元 3 → `GL_LINE_STRIP`。这些枚举在 `<GL/gl.h>` 里，不要自己 `#define`。

- [ ] **步骤 1：扩展 switch**

把 `gl_primitive_mode` 换成：

```cpp
        bool gl_primitive_mode(std::int32_t primitive, GLenum& mode)
        {
            switch (primitive)
            {
            case 1: mode = GL_POINTS; return true;
            case 2: mode = GL_LINES; return true;
            case 3: mode = GL_LINE_STRIP; return true;
            case 4: mode = GL_TRIANGLES; return true;
            case 5: mode = GL_TRIANGLE_STRIP; return true;
            default: return false;
            }
        }
```

不要加 `GL_LINE_LOOP` 或 `GL_TRIANGLE_FAN`。图元 6 仍走 `gl_draw_accepted` 里的扇形错误。

- [ ] **步骤 2：跑探针，确认通过**

```powershell
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：退出码 0。任务 2 的五行仍在，后面再多这八行，然后是 `PASS`：

```text
point pixels  : 255 / 0
point tail    : 255 / 255
point indexed : 255 / 0
line pixels   : 255 / 0 / 255
line strip    : 255 / 0
line strip tail: 255 / 255
line indexed  : 255 / 255
line target   : 255 / 0
```

邻纹素、空隙和线外纹素的失败不会多打印一行，只会走 `fail`。若点的邻居也是 `255`，不要把断言改成「附近有红」。先确认没有调用 `glPointSize`。若线的空隙是 `255`，说明线列表被画成了线带。

- [ ] **步骤 3：提交**

```powershell
git add src/native/gl/igpu_gl_draw.cpp
git commit -m "feat: draw OpenGL points and lines"
```

---

### Task 5: Record the slice in the handover

### 任务 5：交接文档跟上这一刀

**文件：**
- 修改：`SESSION_HANDOVER.md`
- 修改：`HANDOVER.md`
- 修改：`superpowers/plans/2026-10-07-gl-primitives.md`（把本计划抄进去；会话里的 plan 文件不在仓库中）

**接口：**
- 使用：探针现在的二十三行输出（原来的十行，加五行三角带，加八行点和线）。不改检查项、版本、能力键、公开函数个数。

- [ ] **步骤 1：改 SESSION_HANDOVER.md 开头**

标题改成「OpenGL 点、线、三角带已在探针里」。删掉「下一刀还没定」这句，改成：图元 1、2、3、5 已在探针里证明。图元 6 仍拒绝。采样器、混合、深度、多目标、立方体、三维、实例化、间接、计算、统一缓冲、查询仍不要开。ES 2 和成功的 `glsl_es` 编译仍不要做。下一刀还没定。

探针预期输出在原有十行之后加上十三行：任务 1 的五行三角带，加上任务 3 的八行点和线。「绘制只接受……图元 4」那一段改成：绘制接受图元 1、2、3、4、5。图元 6 拒绝且不改像素。图元 0 和 7 拒绝，错误含有 `does not draw this primitive`。非零状态句柄拒绝，错误含有 `draws with no extra state`。点大小和线宽保持 1。`igpu_draw` 与 `igpu_draw_indexed` 仍不改帧缓冲、视口和程序。`igpu_draw_to_render_targets` 仍会恢复这两项。

GL 像素表加一行：三角带 `255 / 0`，随后右边也是 `255`；点是单个纹素 `255`，邻居是 `0`；线列表是 `255 / 0 / 255`；线带随后右边也是 `255`。

不要改「当前检查项 **878** 项」。不要改版本 `0.5.0`。不要新写「N 个键」。`HEAD` 短哈希必须是 `git rev-parse --short HEAD` 能找到的已有提交；若脚本说它落后，那是允许的，不要为了追平 HEAD 再提交一次空文档。

- [ ] **步骤 2：改 HANDOVER.md 里的两处短句**

开头「下一刀还没定」那句，以及后面优先级列表里重复的那句，改成和 `SESSION_HANDOVER.md` 相同的结论：这一刀已经落地，推迟清单仍然关闭，再下一刀还没定。不要改 9 月审计正文。不要改 `当前检查项 **878** 项`。

- [ ] **步骤 3：跑核对和探针**

```powershell
pwsh -File tools\verify_handover.ps1
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：脚本退出码 0。探针退出码 0，二十三行像素输出后是 `PASS`。不要跑 `gm-cli`。不要编译 DLL，除非脚本或编译意外碰到了它。

- [ ] **步骤 4：提交**

```powershell
git add SESSION_HANDOVER.md HANDOVER.md superpowers/plans/2026-10-07-gl-primitives.md
git commit -m "docs: record OpenGL points, lines, and triangle strips"
```

---

## 自检

- 规格覆盖：图元 1、2、3、5 在 `igpu_draw` 上有像素证明。三角带同时覆盖索引绘制和画进纹理。点覆盖索引绘制。线列表覆盖画进纹理。线带覆盖索引绘制。图元 6、0、7 和非零状态有拒绝证明。推迟清单没有打开。
- 没有留给执行者填的空话。顶点坐标、错误子串、printf 文本和提交命令都写在任务里。
- `gl_primitive_mode` 和 `gl_draw_accepted` 的名字在任务 2 和任务 4 里一致。任务 4 只扩展 switch，不改函数签名。
- 复查重点里的五条都有探针：第二条三角形、线列表空隙、三顶点线带、点的邻居和点大小、状态句柄与另外两条绘制入口。
