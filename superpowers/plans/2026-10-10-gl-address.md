# OpenGL 钳制与重复采样实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 OpenGL 探针里的 `igpu_draw_sampled` 接受一个最近点采样器，用它在这一次绘制里选择钳制到边缘或重复，并在返回前把纹理的包裹参数恢复回去。

**Architecture:** 公开函数已经存在。`igpu_sampler_state_create` 在门面里校验级数和各向异性，比较、边框和各向异性大于 1 会走到别的虚函数。最近点、三轴同为钳制或同为重复、没有级数偏移时，门面调用 `Backend::sampler_state_create_filters_range`。这一刀只实现这个虚函数和 `state_release`。采样器句柄是 CPU 上的一个布尔，不创建 GL 采样器对象。`gl_draw_sampled` 在绑上源纹理之后用 `glTexParameteri` 改 `GL_TEXTURE_WRAP_S` 和 `GL_TEXTURE_WRAP_T`，绘制后写回原来的值，再恢复 `GL_TEXTURE_BINDING_2D`。句柄 0 不改包裹。`igpu_draw`、`igpu_draw_indexed` 和 `igpu_draw_to_render_targets` 仍然拒绝任何非 0 采样器。

**Tech Stack:** C++17，Windows `GL/gl.h` 加已经加载的 GL 2.0 入口。WGL 探针，CMake 预设 `win-x64-release-vs18`，MSVC。编译进 MSVC 的 cpp 注释只用 ASCII。

**Spec:** 这一刀的契约就是本计划。站立约束在 `SESSION_HANDOVER.md` 开头和「必须守住的结构」。`superpowers/specs/2026-10-06-slim-api-design.md` 不改。`spec.gmidl` 不改。

## Global Constraints

- 公共头、`spec.gmidl` 和 GML 不出现 d3d、dxgi、hlsl、`_5_0`、ID3D。方言字符串 `hlsl` / `glsl` / `glsl_es` / `msl` / `spirv` 仍然允许。
- 调用链保持 `spec.gmidl` → 生成的 `code_gen` → `IGPU_native.cpp` → `igpu_gpu.cpp` 里的 `igpu::sampler_state_create_full` → `Backend::sampler_state_create_filters_range`。绘制链是 `igpu::draw_sampled` → `Backend::draw_sampled`。不改生成文件。不跑 extgen。签名不变。
- 门面在虚函数调用之前执行 `require_device`。虚函数不写默认参数。
- GameMaker 颜色是 BGR。读回丢掉 alpha。红是 `255`，蓝是 `16711680`，清屏是 `0`。
- MSVC 按代码页 936 读 cpp。编译进 MSVC 的 cpp 注释只用 ASCII。中文注释里的字节 `0x5C` 会吃掉下一行。
- 键数保持 35。键清单代码块里不要写 `surface_*`。不要新写「N 个键」这种句子。检查项保持 **878**。版本保持 `0.5.0`。`HANDOVER.md` 里的 `HEAD` 必须是仓库里真实存在的提交。
- 只有 `SamplerState` 改成 `native || opengl_backend()`。`BlendState`、`DepthState`、`RasterState` 保持 `return native`。
- OpenGL 只在探针里。Windows 的 GameMaker DLL 构建时不定义 `IGPU_HAS_OPENGL`。不要加 ANGLE。不要改三个指针的 `igpu_init`。`wglCreateContext`、`wglDeleteContext`、`wglMakeCurrent` 只留在 `tests/gl_probe`。
- DLL 的源文件 GLOB 保持 `native/*.cpp` 和 `native/d3d11/*.cpp`。不要把 `native/gl/` 加进 `src/CMakeLists.txt` 的 DLL 目标。
- 不创建 GL 采样器对象，不调用 `glActiveTexture`，不调用 `glGenerateMipmap`，不改 `GL_TEXTURE_MIN_FILTER` 或 `GL_TEXTURE_MAG_FILTER`。过滤保持创建纹理时的 `GL_NEAREST`。
- 线性过滤、镜像、边框色、比较、各向异性大于 1、级数偏移、非 0 的最细级数，这一刀都拒绝。混合、深度、光栅仍然拒绝。图元 6 仍拒绝。不做 ES 2，不让 `glsl_es` 编译成功。
- `first_vertex + vertex_count` 在特别大的正数上可能回绕。这一刀不改这道算术。
- 借用的上下文：探针仍先 `igpu_shutdown()`，再 `wglDeleteContext`。`reset()` 仍先清掉活动后端。
- `igpu_draw_sampled` 不清屏，不改帧缓冲、视口和程序，不调用 `UseProgram`。返回前恢复顶点数组、数组缓冲、二维纹理绑定，以及这一次改过的包裹参数。不改 `GL_ACTIVE_TEXTURE`。

## Review Focus

下面这些输入，单独看到「某个采样点变红或变蓝」时仍会漏掉。每一条都在任务 1 的探针里有检查。

- 重复绘制若没有把包裹写回去，句柄 0、`u = 1.0625` 的下一笔会读到纹素 0 的红 `255`。写回去之后必须是边缘纹素的蓝 `16711680`。任务 1 的 `wrap restored`。
- 任何非 0 句柄都被当成重复时，钳制采样器在 `u = -0.0625` 会读到蓝。钳制必须读到纹素 0 的红 `255`，重复在同一个坐标读到蓝 `16711680`。任务 1 的 `wrap edge` 和 `wrap negative`。
- 未知句柄若先改了包裹再失败，源纹理会停在重复上。句柄 99 必须失败且不写像素，随后句柄 0、`u = 1.0625` 仍是蓝。任务 1 在释放采样器之前做这组检查。
- `igpu_draw` 和 `igpu_draw_indexed` 拿到活着的重复采样器时必须失败，错误含有 `draws with no extra state`，目标保持 `0`。采样器不是全局纹理状态。任务 1 的非采样拒绝。
- 放大过滤若被收成线性，`u = 0.484375` 会掺进蓝。创建必须失败，错误含有 `nearest`。句柄 0 在这个坐标上两边仍是红 `255`。任务 1 的 `wrap nearest` 和线性创建拒绝。

---

## 文件职责

- 修改 `src/native/gl/igpu_gl_draw.h` — 声明采样器的创建和释放。
- 修改 `src/native/gl/igpu_gl_draw.cpp` — 保存采样器；`gl_draw_sampled` 在一次绘制里改包裹并恢复。
- 修改 `src/native/gl/igpu_gl_backend.cpp` — `sampler_state_create_filters_range` 和 `state_release` 转发。其余采样器虚函数保持 `not_yet`。
- 修改 `src/native/igpu_capabilities.cpp` — 只改 `SamplerState` 那一行。
- 修改 `tests/gl_probe/main.cpp` — 像素证明和拒绝。
- 修改 `SESSION_HANDOVER.md`，以及 `HANDOVER.md` 里仍写着「下一刀还没定」和「采样器状态对象都先不要做」的短句。
- 不改 `spec.gmidl`、`code_gen/`、`project/`、`src/CMakeLists.txt`、`src/native/igpu_gpu.cpp`、`src/native/igpu_state.cpp`、`src/native/igpu_texture.cpp`。

`igpu_draw` 和 `igpu_draw_indexed` 的像素行为不变。已有的 `sample nearest` 使用句柄 0，坐标 `0.484375`，两边必须仍是 `255`。

## 采样坐标

8×8 纹理，左半边纹素 0–3 是红 `255`，右半边纹素 4–7 是蓝 `16711680`。这些坐标都是 1/16 的倍数，float32 能精确表示。`igpu_texture_read` 的 `y = 0` 对应 GL 行 `height - 1`。源纹理整列同色，所以 `v` 不影响这两个采样点。两边的目标纹素 `(1, 4)` 和 `(6, 4)` 必须读到同一个颜色，因为每个顶点的纹理坐标都相同。

| 调用 | `u` | 结果 |
|---|---|---|
| 重复，三轴都是 `Repeat` | `1.0625` | 卷到 `0.0625`，纹素 0，红 `255` |
| 钳制，三轴都是 `Clamp` | `1.0625` | 边缘纹素 7，蓝 `16711680` |
| 句柄 0，创建时的钳制还在 | `1.0625` | 蓝 `16711680` |
| 重复 | `-0.0625` | 卷到 `0.9375`，纹素 7，蓝 `16711680` |
| 钳制 | `-0.0625` | 纹素 0，红 `255` |
| 句柄 0 | `0.484375` | 纹素 3，红 `255`。线性过滤会掺进蓝 |

不要改这些坐标去迁就一次读回。`1.0625` 若读回蓝，先查是不是仍停在 `GL_CLAMP_TO_EDGE`。`-0.0625` 在钳制采样器上若读回蓝，先查是不是把所有非 0 句柄都设成了 `GL_REPEAT`。

公开创建参数：放大、缩小、级间过滤都是 `0`（`tf_point`）。各向异性是 `1`。边框色是 `0`。比较是 `0`。级数偏移是 `0`，最细是 `0`，最粗是 `-1`（门面把它收成一个很大的正数，表示不设上限）。地址 `0` 是 `IgpuAddressMode.Clamp`，`1` 是 `Repeat`。三轴必须相同。

## 分支

从 `main` 开新分支。不要在 `main` 上提交。

```powershell
git switch main
git pull
git switch -c gl-address
```

本计划已经在 `superpowers/plans/2026-10-10-gl-address.md`。若它还没入库，把它作为这个分支的第一次提交。然后确认树还是绿的：

```powershell
pwsh -File tools\verify_handover.ps1
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：脚本退出码 0。探针退出码 0，并打印交接里已有的二十七行，然后是 `PASS`。可执行文件在 `src\Release\igpu_gl_probe.exe`。不要跑 `gm-cli`。不要编译 DLL，除非任务 3 改了 `igpu_capabilities.cpp`；改了之后仍不要跑 `gm-cli`，因为 Windows 的 DLL 上 `native` 仍为 true，`SamplerState` 的结果不变。

---

### 任务 1：包裹探针先失败

**Files:**
- Modify: `tests/gl_probe/main.cpp`
- Test: `tests/gl_probe/main.cpp`

**Interfaces:**
- Consumes: 已有的 `sample_source`、`sample_layout`、`sample_frag`、`sample_saved_fbo`、`sample_held_viewport`、`encode` 不用。`igpu_sampler_state_create(std::int32_t magnification, std::int32_t minification, std::int32_t mip, std::int32_t address_u, std::int32_t address_v, std::int32_t address_w, std::int32_t anisotropy, std::int32_t border, std::int32_t compare, float level_offset, float finest, float coarsest) : std::int64_t`。`igpu_state_release(std::uint64_t state) : bool`。`igpu_supports(std::int32_t capability) : bool`。`igpu::Capability::SamplerState` 在 `igpu_capabilities.h`。`igpu_draw(std::uint64_t vertex_buffer, std::uint64_t instance_buffer, std::uint64_t layout, std::int32_t primitive, std::int64_t first_vertex, std::int64_t vertex_count, std::int64_t instance_count, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state) : bool`。`igpu_draw_indexed(std::uint64_t vertex_buffer, std::uint64_t layout, std::uint64_t index_buffer, std::int32_t primitive, std::int64_t first_index, std::int64_t index_count, std::int64_t blend_state, std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state) : bool`。
- Produces: 探针在实现落地后多这六行，插在已有的 `sample nearest` 之后、`PASS` 之前。任务 1 结束时探针退出码是 1。

```text
wrap repeat  : 255 / 255
wrap clamp   : 16711680 / 16711680
wrap restored: 16711680 / 16711680
wrap negative: 16711680 / 16711680
wrap edge    : 255 / 255
wrap nearest : 255 / 255
```

- [ ] **Step 1: 改掉「任何非 0 采样器都是 extra state」并写入包裹探针**

在文件顶部的 include 里加上：

```cpp
#include "igpu_capabilities.h"
```

把 `tests/gl_probe/main.cpp` 里这一段：

```cpp
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout),
                          4, 0, 6, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 1) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("a nonzero sampler handle was not rejected");
    }
```

换成：

```cpp
    if (igpu_draw_sampled(static_cast<std::uint64_t>(sample_buffer), static_cast<std::uint64_t>(sample_layout),
                          4, 0, 6, static_cast<std::uint64_t>(sample_source), 0, 0, 0, 99) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("an unknown sampler handle was not rejected");
    }
```

句柄 99 从来不会被创建。句柄 1 会是后面创建的第一个采样器，不能再拿来表示「没有这个对象」。

在 `rejected stride 0 sample changed pixels` 那段检查之后、`void* context = gl_probe_context();` 之前，插入下面整段。注释只用 ASCII。不要改 `sample nearest` 那段已有绘制。

```cpp
    const auto wrap_repeat = igpu_sampler_state_create(0, 0, 0, 1, 1, 1, 1, 0, 0, 0.f, 0.f, -1.f);
    const auto wrap_clamp = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (wrap_repeat == 0 || wrap_clamp == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_buffer = igpu_buffer_create(96, 0, 1, 16);
    const auto wrap_repeat_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_clamp_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_restored_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_negative_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_edge_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_nearest_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    const auto wrap_black_dest = igpu_texture_create(0, 8, 8, 1, 6, true, false, 1);
    if (wrap_buffer == 0 || wrap_repeat_dest == 0 || wrap_clamp_dest == 0 || wrap_restored_dest == 0 ||
        wrap_negative_dest == 0 || wrap_edge_dest == 0 || wrap_nearest_dest == 0 || wrap_black_dest == 0)
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto write_uv = [&](float u, float v) -> bool {
        SampleVertex verts[6] = {
            {-1.f, -1.f, u, v}, {1.f, -1.f, u, v}, {-1.f, 1.f, u, v},
            {1.f, -1.f, u, v},  {1.f, 1.f, u, v},  {-1.f, 1.f, u, v},
        };
        return igpu_buffer_write(static_cast<std::uint64_t>(wrap_buffer), 0,
                                 gm::wire::GMBuffer(verts, sizeof(verts)));
    };
    const auto sample_uv = [&](std::int64_t dest, std::int64_t sampler) -> bool {
        std::int32_t saved = 0;
        std::int32_t viewport[4] = {};
        if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(dest), saved, viewport))
        {
            return false;
        }
        const bool drew = igpu_draw_sampled(
            static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
            static_cast<std::uint64_t>(sample_source), 0, 0, 0, sampler);
        igpu::gl_color_target_end(saved, viewport);
        return drew;
    };
    if (!igpu_shader_bind(sample_vert, 0) || !igpu_shader_bind(sample_frag, 1))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (!write_uv(1.0625f, 0.5f) || !sample_uv(wrap_repeat_dest, wrap_repeat))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_repeat_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_repeat_dest), 1, 4, 0, 0);
    const auto wrap_repeat_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_repeat_dest), 6, 4, 0, 0);
    std::printf("wrap repeat  : %lld / %lld\n", static_cast<long long>(wrap_repeat_left),
                static_cast<long long>(wrap_repeat_right));
    if (wrap_repeat_left != 255 || wrap_repeat_right != 255)
    {
        return fail("repeat sample at u=1.0625 was not pure red");
    }
    if (!sample_uv(wrap_clamp_dest, wrap_clamp))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_clamp_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_clamp_dest), 1, 4, 0, 0);
    const auto wrap_clamp_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_clamp_dest), 6, 4, 0, 0);
    std::printf("wrap clamp   : %lld / %lld\n", static_cast<long long>(wrap_clamp_left),
                static_cast<long long>(wrap_clamp_right));
    if (wrap_clamp_left != 16711680 || wrap_clamp_right != 16711680)
    {
        return fail("clamp sample at u=1.0625 was not pure blue");
    }
    if (!sample_uv(wrap_restored_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_restored_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_restored_dest), 1, 4, 0, 0);
    const auto wrap_restored_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_restored_dest), 6, 4, 0, 0);
    std::printf("wrap restored: %lld / %lld\n", static_cast<long long>(wrap_restored_left),
                static_cast<long long>(wrap_restored_right));
    if (wrap_restored_left != 16711680 || wrap_restored_right != 16711680)
    {
        return fail("sampler 0 did not stay clamped after a repeat draw");
    }
    if (!write_uv(-0.0625f, 0.5f) || !sample_uv(wrap_negative_dest, wrap_repeat))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_negative_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_negative_dest), 1, 4, 0, 0);
    const auto wrap_negative_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_negative_dest), 6, 4, 0, 0);
    std::printf("wrap negative: %lld / %lld\n", static_cast<long long>(wrap_negative_left),
                static_cast<long long>(wrap_negative_right));
    if (wrap_negative_left != 16711680 || wrap_negative_right != 16711680)
    {
        return fail("repeat sample at u=-0.0625 was not pure blue");
    }
    if (!sample_uv(wrap_edge_dest, wrap_clamp))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_edge_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_edge_dest), 1, 4, 0, 0);
    const auto wrap_edge_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_edge_dest), 6, 4, 0, 0);
    std::printf("wrap edge    : %lld / %lld\n", static_cast<long long>(wrap_edge_left),
                static_cast<long long>(wrap_edge_right));
    if (wrap_edge_left != 255 || wrap_edge_right != 255)
    {
        return fail("clamp sample at u=-0.0625 was not pure red");
    }
    if (!write_uv(0.484375f, 0.5f) || !sample_uv(wrap_nearest_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    const auto wrap_nearest_left = igpu_texture_read(static_cast<std::uint64_t>(wrap_nearest_dest), 1, 4, 0, 0);
    const auto wrap_nearest_right = igpu_texture_read(static_cast<std::uint64_t>(wrap_nearest_dest), 6, 4, 0, 0);
    std::printf("wrap nearest : %lld / %lld\n", static_cast<long long>(wrap_nearest_left),
                static_cast<long long>(wrap_nearest_right));
    if (wrap_nearest_left != 255 || wrap_nearest_right != 255)
    {
        return fail("sampler 0 at u=0.484375 was not pure red after wrap draws");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("wrap draws changed the source texture");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(wrap_black_dest), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("wrap reject target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                          static_cast<std::uint64_t>(sample_source), 0, 0, 0, 99) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("unknown sampler 99 was accepted");
    }
    if (igpu_draw(static_cast<std::uint64_t>(wrap_buffer), 0, static_cast<std::uint64_t>(sample_layout), 4, 0, 6, 1,
                  0, 0, 0, wrap_repeat) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("igpu_draw accepted a sampler");
    }
    if (igpu_draw_indexed(static_cast<std::uint64_t>(indexed_vertices_buffer), static_cast<std::uint64_t>(layout),
                          static_cast<std::uint64_t>(indexed_index_buffer), 4, 0, 6, 0, 0, 0, wrap_repeat) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("igpu_draw_indexed accepted a sampler");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                          static_cast<std::uint64_t>(sample_source), 1, 0, 0, 0) ||
        std::string(igpu_get_last_error()).find("draws with no extra state") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("blend state was accepted on a sampled draw");
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 1, 4, 0, 0) != 0 ||
        igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 6, 4, 0, 0) != 0)
    {
        return fail("rejected wrap draw changed pixels");
    }
    if (!write_uv(1.0625f, 0.5f) || !sample_uv(wrap_restored_dest, 0))
    {
        return fail(igpu_get_last_error().c_str());
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_restored_dest), 1, 4, 0, 0) != 16711680)
    {
        return fail("unknown sampler 99 left the texture repeating");
    }

    if (!igpu::gl_color_target_begin(static_cast<std::uint64_t>(sample_source), sample_saved_fbo,
                                     sample_held_viewport))
    {
        return fail("wrap feedback target begin failed");
    }
    if (igpu_draw_sampled(static_cast<std::uint64_t>(wrap_buffer), static_cast<std::uint64_t>(sample_layout), 4, 0, 6,
                          static_cast<std::uint64_t>(sample_source), 0, 0, 0, wrap_repeat) ||
        std::string(igpu_get_last_error()).find("the texture is the current color target") == std::string::npos)
    {
        igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
        return fail("repeat sampler was allowed to sample the current color target");
    }
    igpu::gl_color_target_end(sample_saved_fbo, sample_held_viewport);
    if (igpu_texture_read(static_cast<std::uint64_t>(sample_source), 1, 4, 0, 0) != 255 ||
        igpu_texture_read(static_cast<std::uint64_t>(sample_source), 6, 4, 0, 0) != 16711680)
    {
        return fail("rejected repeat feedback changed the source texture");
    }

    const auto wrap_linear = igpu_sampler_state_create(1, 0, 0, 0, 0, 0, 1, 0, 0, 0.f, 0.f, -1.f);
    if (wrap_linear != 0 || std::string(igpu_get_last_error()).find("nearest") == std::string::npos)
    {
        return fail("linear magnification was accepted");
    }
    const auto wrap_mirror = igpu_sampler_state_create(0, 0, 0, 2, 2, 2, 1, 0, 0, 0.f, 0.f, -1.f);
    if (wrap_mirror != 0 || std::string(igpu_get_last_error()).find("clamp or repeat") == std::string::npos)
    {
        return fail("mirror addressing was accepted");
    }
    const auto wrap_mixed = igpu_sampler_state_create(0, 0, 0, 1, 0, 1, 1, 0, 0, 0.f, 0.f, -1.f);
    if (wrap_mixed != 0 || std::string(igpu_get_last_error()).find("clamp or repeat") == std::string::npos)
    {
        return fail("mixed address axes were accepted");
    }
    const auto wrap_compare = igpu_sampler_state_create(0, 0, 0, 0, 0, 0, 1, 0, 1, 0.f, 0.f, -1.f);
    if (wrap_compare != 0 || std::string(igpu_get_last_error()).find("does not implement") == std::string::npos)
    {
        return fail("comparison sampler was accepted");
    }
    const auto wrap_aniso = igpu_sampler_state_create(1, 1, 1, 1, 1, 1, 2, 0, 0, 0.f, 0.f, -1.f);
    if (wrap_aniso != 0 || std::string(igpu_get_last_error()).find("does not implement") == std::string::npos)
    {
        return fail("anisotropic sampler was accepted");
    }
    const auto wrap_lod = igpu_sampler_state_create(0, 0, 0, 1, 1, 1, 1, 0, 0, 1.f, 0.f, -1.f);
    if (wrap_lod != 0 || std::string(igpu_get_last_error()).find("no mip") == std::string::npos)
    {
        return fail("level offset was accepted");
    }
    if (!igpu_state_release(static_cast<std::uint64_t>(wrap_repeat)) ||
        igpu_state_release(static_cast<std::uint64_t>(wrap_repeat)) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        return fail("sampler release did not retire the handle");
    }
    if (sample_uv(wrap_black_dest, wrap_repeat) ||
        std::string(igpu_get_last_error()).find("unknown sampler handle") == std::string::npos)
    {
        return fail("released sampler was accepted");
    }
    if (igpu_texture_read(static_cast<std::uint64_t>(wrap_black_dest), 1, 4, 0, 0) != 0)
    {
        return fail("released sampler changed pixels");
    }
    if (!sample_uv(wrap_clamp_dest, wrap_clamp) ||
        igpu_texture_read(static_cast<std::uint64_t>(wrap_clamp_dest), 1, 4, 0, 0) != 16711680)
    {
        return fail("releasing the repeat sampler broke the clamp sampler");
    }
    if (!igpu_supports(static_cast<std::int32_t>(igpu::Capability::SamplerState)))
    {
        return fail("SamplerState is false on the OpenGL backend");
    }
```

`sample_vert` 和 `SampleVertex` 已经在 `main` 前面定义。`indexed_vertices_buffer`、`indexed_index_buffer` 和 `layout` 也还在作用域里。

- [ ] **Step 2: 跑探针，确认它失败**

```powershell
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：退出码 1。标准错误是 `an unknown sampler handle was not rejected`。这一步还没有创建采样器的实现，非 0 句柄仍在绘制入口被拒绝，错误里是 `draws with no extra state`。不要把断言改回旧句子来让它变绿。不要跑 `gm-cli`。

- [ ] **Step 3: 提交失败的探针**

```powershell
git add tests/gl_probe/main.cpp superpowers/plans/2026-10-10-gl-address.md
git commit -m "test: probe OpenGL clamp and repeat sampling before the backend accepts it"
```

计划文件若已在之前的提交里，就不要再把它加进去。

---

### 任务 2：实现最近点钳制和重复

**Files:**
- Modify: `src/native/gl/igpu_gl_draw.h`
- Modify: `src/native/gl/igpu_gl_draw.cpp`
- Modify: `src/native/gl/igpu_gl_backend.cpp`
- Modify: `src/native/igpu_capabilities.cpp`
- Test: `tests/gl_probe/main.cpp`（任务 1 已经写好，这一任务不改它）

**Interfaces:**
- Consumes: 任务 1 的探针。`Backend::sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification, std::int32_t mip, std::int32_t address_u, std::int32_t address_v, std::int32_t address_w, float level_offset, float finest, float coarsest) : std::int64_t`。`Backend::state_release(std::uint64_t handle) : bool`。`gl_draw_sampled` 的现有签名不变。
- Produces: `igpu::gl_sampler_state_create_filters_range(...) : std::int64_t`，参数与上面的虚函数相同。`igpu::gl_state_release(std::uint64_t handle) : bool`。成功的句柄从 1 起。`repeat == true` 表示三轴都是 `Repeat`。`gl_resources_release()` 同时丢掉这张表。

- [ ] **Step 1: 确认任务 1 的探针仍然失败**

```powershell
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：退出码 1，标准错误含有 `an unknown sampler handle was not rejected`。若可执行文件已过期，先按任务 1 的命令重编探针。

- [ ] **Step 2: 加上采样器表和绘制时的包裹恢复**

在 `src/native/gl/igpu_gl_draw.h` 的 `gl_texture_read_level` 声明之后加上：

```cpp
    std::int64_t gl_sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                       std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                       std::int32_t address_w, float level_offset, float finest,
                                                       float coarsest);
    bool gl_state_release(std::uint64_t handle);
```

在 `src/native/gl/igpu_gl_draw.cpp` 匿名命名空间里，`g_layouts` 旁边加上：

```cpp
        struct SamplerObject
        {
            bool repeat = false;
        };

        std::unordered_map<std::uint64_t, SamplerObject> g_samplers;
        std::uint64_t g_next_sampler = 1;
```

`gl_resources_release()` 里，在 `g_layouts.clear()` 之后加上：

```cpp
        g_samplers.clear();
        g_next_sampler = 1;
```

把匿名命名空间里的 `gl_draw_accepted` 换成这两个函数。四个调用点里，`gl_draw`、`gl_draw_indexed` 和 `gl_draw_to_render_targets_layer` 继续调用 `gl_draw_accepted`。只有 `gl_draw_sampled` 改成调用 `gl_pipeline_accepted`。

```cpp
        bool gl_pipeline_accepted(const char* entry, std::int32_t primitive, std::int64_t blend_state,
                                  std::int64_t depth_state, std::int64_t raster_state, GLenum& mode)
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
            if (blend_state != 0 || depth_state != 0 || raster_state != 0)
            {
                set_last_error(std::string(entry) + ": the opengl backend draws with no extra state");
                return false;
            }
            return true;
        }

        bool gl_draw_accepted(const char* entry, std::int32_t primitive, std::int64_t blend_state,
                              std::int64_t depth_state, std::int64_t raster_state, std::int64_t sampler_state,
                              GLenum& mode)
        {
            if (!gl_pipeline_accepted(entry, primitive, blend_state, depth_state, raster_state, mode))
            {
                return false;
            }
            if (sampler_state != 0)
            {
                set_last_error(std::string(entry) + ": the opengl backend draws with no extra state");
                return false;
            }
            return true;
        }
```

在 `gl_draw_sampled` 里，把 `gl_draw_accepted("igpu_draw_sampled", ...)` 换成 `gl_pipeline_accepted("igpu_draw_sampled", primitive, blend_state, depth_state, raster_state, mode)`。不要把 `sampler_state` 传进 `gl_pipeline_accepted`。

在 `igpu_tex` 的 uniform 检查成功之后、`if (g_vertex_array == 0)` 之前，插入：

```cpp
        bool repeat = false;
        if (sampler_state != 0)
        {
            const auto sampler_it = g_samplers.find(static_cast<std::uint64_t>(sampler_state));
            if (sampler_it == g_samplers.end())
            {
                set_last_error("igpu_draw_sampled: unknown sampler handle");
                return false;
            }
            repeat = sampler_it->second.repeat;
        }
```

这段必须放在帧缓冲反馈检查和 uniform 检查之后。未知句柄返回时还没有调用 `glBindTexture`，也没有改包裹。

把现有的绑定、绘制和恢复换成：

```cpp
        GLint previous_array = 0;
        GLint previous_texture = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_array);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
        glBindTexture(GL_TEXTURE_2D, source->second.texture);
        GLint previous_wrap_s = GL_CLAMP_TO_EDGE;
        GLint previous_wrap_t = GL_CLAMP_TO_EDGE;
        if (sampler_state != 0)
        {
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &previous_wrap_s);
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &previous_wrap_t);
            const GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        }
        fns.Uniform1i(sampler, 0);
        fns.BindVertexArray(g_vertex_array);
        fns.BindBuffer(GL_ARRAY_BUFFER, vertex->second.id);
        bind_vertices(vertex->second, 0);
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
        unbind_vertices();
        fns.BindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_array));
        fns.BindVertexArray(0);
        if (sampler_state != 0)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, previous_wrap_s);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, previous_wrap_t);
        }
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
        return true;
```

`if (g_vertex_array == 0) { fns.GenVertexArrays(1, &g_vertex_array); }` 留在这段前面。句柄 0 不读、也不写包裹参数。不要调用 `glActiveTexture`。不要改 min/mag filter。

在 `gl_draw_sampled` 之后、命名空间结束之前，加上：

```cpp
    std::int64_t gl_sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                       std::int32_t mip, std::int32_t address_u, std::int32_t address_v,
                                                       std::int32_t address_w, float level_offset, float finest,
                                                       float coarsest)
    {
        // One mip. finest must be the base level. A non-negative coarsest
        // cannot select another level on this backend.
        if (magnification != 0 || minification != 0 || mip != 0)
        {
            set_last_error("igpu_sampler_state_create: the opengl backend only samples the nearest texel");
            return 0;
        }
        const bool clamp = address_u == 0 && address_v == 0 && address_w == 0;
        const bool repeat = address_u == 1 && address_v == 1 && address_w == 1;
        if (!clamp && !repeat)
        {
            set_last_error("igpu_sampler_state_create: the opengl backend only accepts clamp or repeat on every axis");
            return 0;
        }
        if (level_offset != 0.f || finest != 0.f || !(coarsest >= 0.f))
        {
            set_last_error("igpu_sampler_state_create: the opengl backend has no mip chain");
            return 0;
        }
        const std::uint64_t handle = g_next_sampler++;
        g_samplers.emplace(handle, SamplerObject{repeat});
        return static_cast<std::int64_t>(handle);
    }

    bool gl_state_release(std::uint64_t handle)
    {
        if (g_samplers.erase(handle) == 0)
        {
            set_last_error("igpu_state_release: unknown sampler handle");
            return false;
        }
        return true;
    }
```

`SamplerObject` 在匿名命名空间里。上面两个函数在 `namespace igpu` 里，和 `gl_draw_sampled` 一样，可以直接使用它。

在 `src/native/gl/igpu_gl_backend.cpp` 里，只改这两个虚函数。其它 `sampler_state_create*` 保持 `not_yet("igpu_sampler_state_create")`。

```cpp
            std::int64_t sampler_state_create_filters_range(std::int32_t magnification, std::int32_t minification,
                                                            std::int32_t mip, std::int32_t address_u,
                                                            std::int32_t address_v, std::int32_t address_w,
                                                            float level_offset, float finest, float coarsest) override
            {
                return gl_sampler_state_create_filters_range(
                    magnification, minification, mip, address_u, address_v, address_w, level_offset, finest, coarsest);
            }
```

```cpp
            bool state_release(std::uint64_t handle) override { return gl_state_release(handle); }
```

`igpu_gl_backend.cpp` 已经包含 `igpu_gl_draw.h`。不要给 `state_release` 再写默认参数。

在 `src/native/igpu_capabilities.cpp` 里只改这一行：

```cpp
        case Capability::SamplerState:     return native || opengl_backend();
```

`BlendState`、`DepthState`、`RasterState` 旁边的 `return native` 不要动。

- [ ] **Step 3: 跑探针，确认通过**

```powershell
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：退出码 0。标准输出在原有二十七行之后有任务 1 的六行，然后是 `PASS`。`wrap repeat` 是 `255 / 255`。`wrap clamp` 和 `wrap restored` 是 `16711680 / 16711680`。`wrap negative` 是 `16711680 / 16711680`。`wrap edge` 和 `wrap nearest` 是 `255 / 255`。

若 `wrap repeat` 仍是蓝，包裹没有改成 `GL_REPEAT`。若 `wrap edge` 是蓝，钳制采样器被写成了重复。若 `wrap restored` 是红，绘制返回前没有写回原来的包裹。不要改探针里的坐标。

再跑：

```powershell
pwsh -File tools\verify_handover.ps1
```

预期：退出码 0。`SamplerState` 不再是裸的 `return native`，脚本会少查一项恒 true，这是预期的。不要跑 `gm-cli`。

- [ ] **Step 4: 提交实现**

```powershell
git add src/native/gl/igpu_gl_draw.h src/native/gl/igpu_gl_draw.cpp src/native/gl/igpu_gl_backend.cpp src/native/igpu_capabilities.cpp
git commit -m "feat: sample OpenGL textures with clamp or repeat"
```

---

### 任务 3：交接写成这一刀已经进探针

**Files:**
- Modify: `SESSION_HANDOVER.md`
- Modify: `HANDOVER.md`
- Test: `tools/verify_handover.ps1`

**Interfaces:**
- Consumes: 任务 2 的探针输出。
- Produces: 两份交接的开头不再把下一刀写成「还没定，采样器不要开」。再下一刀仍不指定。

- [ ] **Step 1: 改 SESSION_HANDOVER.md 的开头**

标题改成「OpenGL 钳制与重复采样已在探针里」。删掉「再下一刀还没定。采样器状态对象、混合、深度……」那句，改成下面这段：

```text
最近点采样器只在探针里。`igpu_sampler_state_create` 接受三轴相同的钳制或重复，放大、缩小和级间过滤都是最近点，各向异性是 1，比较是 0，级数偏移是 0，最细级数是 0。句柄记在 CPU 上，不创建 GL 采样器对象。`igpu_draw_sampled` 在这一次绘制里把源纹理的 S、T 包裹设成对应值，返回前写回绘制前的值。句柄 0 不改包裹。不调用 `glActiveTexture`，也不改过滤。

`u = 1.0625` 在重复下两边都是红 `255`，在钳制采样器下两边都是蓝 `16711680`。随后句柄 0 仍是蓝，重复没有留下来。`u = -0.0625` 在重复下是蓝，在钳制下是红。句柄 0、`u = 0.484375` 两边仍是红 `255`。

线性放大失败，错误含有 `nearest`。镜像和三轴不一致失败，错误含有 `clamp or repeat`。比较和各向异性大于 1 仍失败，错误含有 `does not implement`。级数偏移失败，错误含有 `no mip`。未知句柄和已经释放的句柄失败，错误含有 `unknown sampler handle`，像素不变。`igpu_draw` 和 `igpu_draw_indexed` 拿到活着的采样器仍失败，错误含有 `draws with no extra state`。被采样的纹理若是当前颜色附件，重复采样器也同样失败。`SamplerState` 在这份 OpenGL 后端上为 true。Windows 的 GameMaker DLL 仍没有 OpenGL 后端，那里 `SamplerState` 仍只跟着 Direct3D。

混合、深度、光栅、边框色、镜像、线性过滤、各向异性、比较、多目标、立方体、三维、实例化、间接、计算、统一缓冲、查询都还不要开。ES 2 和 `glsl_es` 的成功编译也还不要做。再下一刀还没定。
```

把探针预期从「二十七行」改成「三十三行」。在 `sample nearest: 255 / 255` 下面加上：

```text
wrap repeat  : 255 / 255
wrap clamp   : 16711680 / 16711680
wrap restored: 16711680 / 16711680
wrap negative: 16711680 / 16711680
wrap edge    : 255 / 255
wrap nearest : 255 / 255
```

当前数字表里 GL 纹理采样那一行保留原句，再加一行：GL 包裹是重复 `u = 1.0625` 为 `255 / 255`，钳制和句柄 0 为 `16711680 / 16711680`。不要改版本行。不要改 `当前检查项 **878** 项`。不要加「N 个键」。不要编一个新的 HEAD 哈希。脚本只要求文档里的哈希是仓库里的真实对象；这一任务提交之前，继续沿用文件里已经写着的那个哈希。

- [ ] **Step 2: 改 HANDOVER.md 开头仍写着旧下一刀的短句**

只改开头状态、2026-10-10 那段提示，以及优先级列表里重复的那句。改成和 `SESSION_HANDOVER.md` 相同的结论：钳制与重复已经在探针里落地。混合、深度、光栅、边框色、镜像、线性过滤、各向异性、比较、多目标、立方体、三维、实例化、间接、计算、统一缓冲、查询、ES 2，以及成功的 `glsl_es` 编译，都先不要做。不要在这里指定再下一个功能。不要改 9 月审计正文。不要改版本行。不要改 `当前检查项 **878** 项`。

- [ ] **Step 3: 跑交接脚本和探针**

```powershell
pwsh -File tools\verify_handover.ps1
cmake --build --preset win-x64-release-vs18 --target igpu_gl_probe
out\build\win-x64-release\src\Release\igpu_gl_probe.exe
```

预期：脚本退出码 0。探针退出码 0，三十三行像素之后是 `PASS`。不要跑 `gm-cli`。

- [ ] **Step 4: 提交交接**

```powershell
git add SESSION_HANDOVER.md HANDOVER.md
git commit -m "docs: record OpenGL clamp and repeat sampling"
```

---

## 自检记录

- 规格覆盖：钳制、重复、句柄 0 不改包裹、恢复、未知句柄、释放、非采样绘制拒绝、线性、镜像、混合轴、比较、各向异性、级数偏移、反馈目标、源纹理不变、`SamplerState`、能力表只动这一项。都落在任务 1 的探针和任务 2 的实现里。交接在任务 3。
- 没有留给执行者的占位句子。比较和各向异性故意仍走 `not_yet`，探针锁住 `does not implement`。
- 句柄类型全程是 `std::int64_t` 创建、`std::uint64_t` 释放和绘制。`gl_sampler_state_create_filters_range` 的参数表与 `Backend::sampler_state_create_filters_range` 一致。
- Review Focus 的五条都在任务 1 的探针里：`wrap restored`、`wrap edge` 对 `wrap negative`、句柄 99 之后的句柄 0、`igpu_draw` 与 `igpu_draw_indexed`、`wrap nearest` 与线性创建拒绝。
