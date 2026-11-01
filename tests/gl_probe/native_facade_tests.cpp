#include "native_facade_tests.h"
#include "IGPU_native.h"
#include "native/igpu_draw.h"
#include "native/igpu_state.h"
#include "native/igpu_texture.h"
#include "native/igpu_input_layout.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include "native/gl/igpu_gl_texture.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <string_view>
#include <vector>

namespace
{
    using Handle = std::uint64_t;
    bool check(bool ok, const char* label)
    {
        if (!ok) std::fprintf(stderr, "native facade: %s: %s\n", label, igpu_get_last_error().c_str());
        return ok;
    }
    using VertexPointer = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    VertexPointer original_pointer = nullptr;
    GLfloat observed_anisotropy = -1.f;
    void APIENTRY observe_pointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                  GLsizei stride, const void* pointer)
    {
        if (index == 0) glGetTexParameterfv(GL_TEXTURE_2D, 0x84FE, &observed_anisotropy);
        original_pointer(index, size, type, normalized, stride, pointer);
    }
    struct Array
    {
        std::vector<std::byte> bytes;
        explicit Array(std::initializer_list<Handle> values) : bytes(2 + 9 * values.size())
        {
            gm::byteio::writeLe(bytes.data(), static_cast<std::uint16_t>(values.size()));
            std::size_t offset = 2;
            for (auto value : values)
            {
                bytes[offset] = static_cast<std::byte>(gm::wire::GMKind::UInt64);
                gm::byteio::writeLe(bytes.data() + offset + 1, value);
                offset += 9;
            }
        }
        gm::wire::GMArrayView view() const
        { return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array, bytes.data())); }
    };
    struct NativeState
    {
        std::array<GLint, 8> values{};
        std::array<GLint, 4> viewport{};
        NativeState()
        {
            const GLenum fields[] = {0x8B8D, 0x85B5, 0x8894, 0x8895, 0x8CA6, 0x8CAA, 0x84E0, GL_TEXTURE_BINDING_2D};
            for (int i = 0; i < 8; ++i) glGetIntegerv(fields[i], &values[i]);
            glGetIntegerv(GL_VIEWPORT, viewport.data());
        }
        bool operator==(const NativeState& other) const
        { return values == other.values && viewport == other.viewport; }
    };
    struct Parameters
    {
        std::array<GLint, 5> integers{};
        std::array<GLfloat, 7> floats{};
        explicit Parameters(GLuint texture)
        {
            auto& f = igpu_gl_fns(); GLint active = 0, bound = 0;
            glGetIntegerv(0x84E0, &active); f.ActiveTexture(0x84C0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound); glBindTexture(GL_TEXTURE_2D, texture);
            const GLenum names[] = {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, 0x8072, GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER};
            for (int i = 0; i < 5; ++i) glGetTexParameteriv(GL_TEXTURE_2D, names[i], &integers[i]);
            glGetTexParameterfv(GL_TEXTURE_2D, 0x8501, &floats[0]);
            glGetTexParameterfv(GL_TEXTURE_2D, 0x813A, &floats[1]);
            glGetTexParameterfv(GL_TEXTURE_2D, 0x813B, &floats[2]);
            glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, floats.data() + 3);
            glBindTexture(GL_TEXTURE_2D, bound); f.ActiveTexture(active);
        }
        bool operator==(const Parameters& other) const
        { return integers == other.integers && floats == other.floats; }
    };
    struct Fixture
    {
        NativeState saved;
        std::array<Handle, 6> stages{};
        std::vector<Handle> textures, layouts;
        Handle vertex = 0, layout = 0, target = 0, source = 0, vs = 0, ps = 0;
        Fixture()
        {
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            for (int i = 0; i < 6; ++i) stages[i] = igpu_get_bound_shader(i);
        }
        ~Fixture()
        {
            auto& f = igpu_gl_fns();
            for (int stage : {3, 4, 5, 0, 1}) igpu_shader_bind(stages[stage], stage);
            if (vs) igpu_shader_release(vs); if (ps) igpu_shader_release(ps);
            for (auto value : layouts) igpu_input_layout_release(value);
            for (auto value : textures) igpu_texture_release(value);
            if (vertex) igpu_buffer_release(vertex);
            f.BindFramebuffer(0x8CA9, saved.values[4]); f.BindFramebuffer(0x8CA8, saved.values[5]);
            f.BindVertexArray(saved.values[1]); f.BindBuffer(0x8892, saved.values[2]);
            f.BindBuffer(0x8893, saved.values[3]); f.ActiveTexture(saved.values[6]);
            glPopAttrib();
            using IsProgram = GLboolean(APIENTRY*)(GLuint);
            const auto exists = reinterpret_cast<IsProgram>(wglGetProcAddress("glIsProgram"));
            if (!saved.values[0] || exists(saved.values[0])) f.UseProgram(saved.values[0]);
        }
        Handle texture(int kind, int width, int height, int layers, int mips = 1)
        {
            const auto value = static_cast<Handle>(igpu_texture_create(kind, width, height, layers, 6, true, false, mips));
            if (value) textures.push_back(value);
            return value;
        }
        bool initialize()
        {
            vs = igpu_shader_compile("#version 430\nlayout(location=0)in vec2 p;void main(){gl_Position=vec4(p,0,1);}", "main", 0, "glsl");
            ps = igpu_shader_compile("#version 430\nuniform sampler2D igpu_tex;uniform vec2 uv;uniform float lod;uniform int mode;out vec4 c;void main(){c=mode==1?textureLod(igpu_tex,uv,lod):(mode==2?vec4(1,0,0,1):vec4(0,1,0,1));}", "main", 1, "glsl");
            vertex = igpu_buffer_create(24, 1, 1, 8);
            Array usage{1}, type{2}, step{0};
            layout = igpu_input_layout_create(vs, usage.view(), type.view(), step.view(), 1, 8, 0);
            if (layout) layouts.push_back(layout);
            target = texture(0, 8, 8, 1); source = texture(0, 2, 2, 1, 2);
            std::array<float, 6> positions{-1, -1, 3, -1, -1, 3};
            if (!check(vs && ps && vertex && layout && target && source &&
                igpu_buffer_write(vertex, 0, gm::wire::GMBuffer(positions.data(), sizeof(positions))) &&
                igpu_shader_bind(0, 3) && igpu_shader_bind(0, 4) && igpu_shader_bind(0, 5) &&
                igpu_shader_bind(vs, 0) && igpu_shader_bind(ps, 1), "canonical fixture")) return false;
            for (auto cap : {GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_ALPHA_TEST, 0x8C89, 0x809D}) glDisable(cap);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            auto& f = igpu_gl_fns(); f.ActiveTexture(0x84C0);
            GLint old = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &old);
            glBindTexture(GL_TEXTURE_2D, igpu::gl_find_texture(source)->texture);
            glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
            GLint unpack = 0; glGetIntegerv(0x88EF, &unpack); f.BindBuffer(0x88EC, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glPixelStorei(0x0CF2, 0); glPixelStorei(0x0CF3, 0); glPixelStorei(0x0CF4, 0);
            const unsigned char pixels[] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
            const unsigned char yellow[] = {255,255,0,255};
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, yellow);
            f.BindBuffer(0x88EC, unpack); glPopClientAttrib();
            glBindTexture(GL_TEXTURE_2D, old); f.ActiveTexture(0x84C3);
            return check(glGetError() == GL_NO_ERROR, "native source pixels");
        }
        void uniforms(int mode, float u = .25f, float v = .25f, float lod = 0)
        {
            auto& f = igpu_gl_fns(); GLint program = 0; glGetIntegerv(0x8B8D, &program);
            using Uniform2 = void(APIENTRY*)(GLint, GLfloat, GLfloat);
            using Uniform1 = void(APIENTRY*)(GLint, GLfloat);
            f.Uniform1i(f.GetUniformLocation(program, "mode"), mode);
            reinterpret_cast<Uniform2>(wglGetProcAddress("glUniform2f"))(f.GetUniformLocation(program, "uv"), u, v);
            reinterpret_cast<Uniform1>(wglGetProcAddress("glUniform1f"))(f.GetUniformLocation(program, "lod"), lod);
        }
        bool sample(Handle sampler, int expected, float u, float v, float lod)
        {
            uniforms(1, u, v, lod);
            const NativeState before; const Parameters parameters(igpu::gl_find_texture(source)->texture);
            std::int32_t previous = 0, viewport[4]{};
            if (!igpu::gl_color_target_begin(target, previous, viewport)) return false;
            const bool drawn = igpu_draw_sampled(vertex, layout, 4, 0, 3, source, 0, 0, 0, sampler);
            igpu::gl_color_target_end(previous, viewport);
            const auto pixel = igpu_texture_read(target, 4, 4, 0, 0);
            bool close = true;
            for (int shift : {0, 8, 16}) close = close && std::abs(int((pixel >> shift) & 255) - ((expected >> shift) & 255)) <= 1;
            return check(drawn && close && before == NativeState{} &&
                parameters == Parameters(igpu::gl_find_texture(source)->texture) && !glGetError(), "literal sampler pixel and caller state");
        }
    };
    bool samplers(Fixture& x)
    {
        constexpr float unlimited = (std::numeric_limits<float>::max)();
        struct Case { const char* name; std::function<std::int64_t()> create; int pixel; float u, v, lod; };
        std::vector<Case> cases = {
            {"simple clamp point", []{return igpu::sampler_state_create(0,false,1);},65280,2.25f,.25f,0},
            {"simple repeat point", []{return igpu::sampler_state_create(0,true,1);},255,2.25f,.25f,0},
            {"simple linear", []{return igpu::sampler_state_create(1,false,1);},8421504,.5f,.5f,0},
            {"nonaniso ignores factor", []{return igpu::sampler_state_create(0,false,2);},255,.25f,.25f,0},
            {"address mirror", []{return igpu::sampler_state_create_address(0,2,1);},65280,1.25f,.25f,0},
            {"border", []{return igpu::sampler_state_create_border(0,1,16711680);},16711680,2,.25f,0},
            {"axes", []{return igpu::sampler_state_create_axes(0,1,0,2,1);},255,1.25f,.25f,0},
            {"axes border", []{return igpu::sampler_state_create_axes_border(0,3,3,3,1,16711680);},16711680,2,.25f,0},
            {"axes range point", []{return igpu::sampler_state_create_axes_range(0,0,0,0,1,0,1,1);},65535,.25f,.25f,0},
            {"axes range linear", [=]{return igpu::sampler_state_create_axes_range(1,0,0,0,1,0,0,unlimited);},33023,.25f,.25f,.5f},
            {"axes range aniso", [=]{return igpu::sampler_state_create_axes_range(2,0,0,0,1,0,0,unlimited);},8421504,.5f,.5f,0},
            {"axes border range", []{return igpu::sampler_state_create_axes_border_range(0,3,3,3,1,16711680,0,0,1);},16711680,2,.25f,0},
            {"filters", []{return igpu::sampler_state_create_filters(1,1,0,0,0,0);},8421504,.5f,.5f,0},
            {"filters border", []{return igpu::sampler_state_create_filters_border(0,0,0,3,3,3,16711680);},16711680,2,.25f,0},
            {"filters offset", []{return igpu::sampler_state_create_filters_offset(0,0,0,0,0,0,1);},65535,.25f,.25f,0}
        };
        const bool anisotropy_supported = igpu::gl_context_at_least(4,6) ||
            igpu::gl_context_extension("GL_ARB_texture_filter_anisotropic") ||
            igpu::gl_context_extension("GL_EXT_texture_filter_anisotropic");
        GLfloat max_anisotropy = 1.f;
        if (anisotropy_supported) glGetFloatv(0x84FF, &max_anisotropy);
        if (max_anisotropy >= 2.f)
            cases.push_back({"actual anisotropic factor", []{return igpu::sampler_state_create(2,false,2);},8421504,.5f,.5f,0});
        bool ok = true;
        for (const auto& item : cases)
        {
            const auto state = item.create();
            if (!check(state > 0, item.name)) { ok = false; continue; }
            const bool observe = anisotropy_supported &&
                (std::string_view(item.name) == "nonaniso ignores factor" || std::string_view(item.name) == "actual anisotropic factor");
            auto& f = igpu_gl_fns();
            if (observe) { observed_anisotropy = -1.f; original_pointer = f.VertexAttribPointer; f.VertexAttribPointer = observe_pointer; }
            const bool sampled = x.sample(state, item.pixel, item.u, item.v, item.lod);
            if (observe) f.VertexAttribPointer = original_pointer;
            ok = sampled && ok;
            if (observe) ok = check(observed_anisotropy == (std::string_view(item.name) == "actual anisotropic factor" ? 2.f : 1.f),
                "actual driver anisotropy during draw matches filter kind") && ok;
            ok = check(igpu_state_release(state), "native sampler release") && ok;
            ok = check(!igpu_draw_sampled(x.vertex,x.layout,4,0,3,x.source,0,0,0,state) && !igpu_get_last_error().empty(), "released sampler rejected") && ok;
        }
        const NativeState before;
        for (auto create : std::vector<std::function<std::int64_t()>>{
            []{return igpu::sampler_state_create(3,false,1);}, []{return igpu::sampler_state_create(0,false,0);},
            []{return igpu::sampler_state_create(0,false,17);}, []{return igpu::sampler_state_create_address(0,3,1);},
            []{return igpu::sampler_state_create_axes(0,0,3,0,1);}, []{return igpu::sampler_state_create_filters(2,0,0,0,0,0);}})
            ok = check(create() == 0 && !igpu_get_last_error().empty() && before == NativeState{} && !glGetError(), "invalid native sampler is quiet and rejects") && ok;
        return ok;
    }
    bool resources(Fixture& x)
    {
        bool ok = true; Array usage{1}, type{2};
        for (int stride : {0,8})
        {
            const auto layout = igpu::input_layout_create(x.vs,usage.view(),type.view(),1,stride);
            ok = check(layout > 0, "native per-vertex layout") && ok;
            if (layout) { x.layouts.push_back(layout); x.layout = layout; }
        }
        Array color_usage{1,2}, color_type{2,5}, uv_usage{1,4}, uv_type{2,2};
        for (bool color : {true,false})
        {
            const auto layout = igpu::input_layout_create(x.vs, color ? color_usage.view() : uv_usage.view(),
                color ? color_type.view() : uv_type.view(), 2, 0);
            ok = check(layout > 0, "native packed color/UV layout default stride") && ok;
            if (layout) x.layouts.push_back(layout);
        }
        const auto allocated = igpu::texture_create(8,8,6,true);
        ok = check(allocated > 0, "native simple2D allocation") && ok;
        if (allocated) { x.textures.push_back(allocated); x.target = allocated; }
        const auto array = x.texture(2,8,8,2), mips = x.texture(0,8,8,1,2);
        if (!check(array && mips, "layer/mip fixtures")) return false;
        x.uniforms(2);
        for (auto target : {array,mips})
        {
            Array targets{target}, zero{0};
            ok = check(igpu_draw_to_render_targets(x.vertex,x.layout,4,0,3,targets.view(),zero.view(),zero.view(),0,0,0,0),
                "initialize unselected base subresources red") && ok;
        }
        x.uniforms(0);
        struct Case { Handle target; int layer, mip; std::function<bool()> draw; };
        const std::vector<Case> cases = {
            {x.target,0,0,[&]{return igpu::draw_to_texture(x.vertex,x.layout,4,0,3,x.target);}},
            {array,1,0,[&]{return igpu::draw_to_texture_layer(x.vertex,x.layout,4,0,3,array,1);}},
            {mips,0,1,[&]{return igpu::draw_to_texture_level(x.vertex,x.layout,4,0,3,mips,0,1);}}
        };
        for (const auto& item : cases)
        {
            const NativeState before; const int count = igpu::draw_count(), failures = igpu::draw_restore_failures();
            const bool drawn = item.draw();
            ok = check(drawn && igpu::draw_count() == count+1 && igpu::draw_restore_failures() == failures &&
                before == NativeState{} && !glGetError() && igpu_texture_read(item.target,1,1,item.layer,item.mip)==65280,
                "native target draw pixels, one count and restoration") && ok;
        }
        ok = check(igpu::texture_get_pixel(x.target,1,1)==65280 && igpu_get_last_error().empty(), "native getpixel layer0 mip0") && ok;
        ok = check(igpu::texture_read(array,1,1,1)==65280 && igpu_get_last_error().empty(), "native read selected layer mip0") && ok;
        ok = check(igpu::texture_read(array,1,1,0)==255 && igpu::texture_get_pixel(mips,1,1)==255,
            "native selected target draws preserve other layer and base mip") && ok;
        const auto sampled_only = igpu::texture_create(8,8,6,false);
        ok = check(sampled_only > 0, "native sample-only role allocation") && ok;
        if (sampled_only) x.textures.push_back(sampled_only);
        const NativeState before; const int count = igpu::draw_count();
        for (auto draw : std::vector<std::function<bool()>>{
            [&]{return igpu::draw_to_texture(x.vertex,x.layout,4,0,3,0);},
            [&]{return igpu::draw_to_texture(x.vertex,x.layout,4,0,3,sampled_only);},
            [&]{return igpu::draw_to_texture_layer(x.vertex,x.layout,4,0,3,array,-1);},
            [&]{return igpu::draw_to_texture_layer(x.vertex,x.layout,4,0,3,array,2);},
            [&]{return igpu::draw_to_texture_level(x.vertex,x.layout,4,0,3,mips,0,-1);},
            [&]{return igpu::draw_to_texture_level(x.vertex,x.layout,4,0,3,mips,0,2);}})
            ok = check(!draw() && !igpu_get_last_error().empty() && count==igpu::draw_count() && before==NativeState{} && !glGetError(), "invalid native target rejected without mutation/count") && ok;
        ok = check(igpu::input_layout_create(x.vs,usage.view(),type.view(),1,7)==0 && !igpu_get_last_error().empty() && before==NativeState{},
            "native layout retains stride validation") && ok;
        const auto stale = igpu::texture_create(4,4,6,true);
        ok = check(stale > 0 && igpu::texture_release(stale) && igpu::texture_get_pixel(stale,0,0)==0 &&
            !igpu_get_last_error().empty() && !igpu::draw_to_texture(x.vertex,x.layout,4,0,3,stale) &&
            !igpu_get_last_error().empty() && before==NativeState{} && count==igpu::draw_count() && !glGetError(),
            "released native texture read/draw preserves state and count") && ok;
        ok = check(igpu::texture_create(0,8,6,true)==0 && !igpu_get_last_error().empty(), "invalid simple texture shape") && ok;
        ok = check(igpu::texture_read(array,1,1,-1)==0 && !igpu_get_last_error().empty() && before==NativeState{} && !glGetError(), "invalid native read layer") && ok;
        return ok;
    }
}
bool run_gl_native_facade_tests()
{
    Fixture x;
    if (!x.initialize()) return false;
    const auto canonical = igpu::sampler_state_create_filters_range(0,0,0,0,0,0,0,0,(std::numeric_limits<float>::max)());
    if (!check(canonical > 0 && x.sample(canonical,255,.25f,.25f,0), "canonical sampler fixture control")) return false;
    igpu_state_release(canonical);
    bool ok = samplers(x);
    ok = resources(x) && ok;
    if (ok) std::printf("native facade: sampler defaults/literal pixels/actual anisotropy, simple2D/read/layout, layer/mip isolation, exact diagnostics and caller state passed\n");
    return ok;
}
