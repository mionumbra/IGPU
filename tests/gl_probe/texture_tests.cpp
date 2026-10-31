#include "texture_tests.h"
#include "native/IGPUInternal_native.h"
#include "IGPU_native.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_shadow.h"
#include "native/gl/igpu_gl_loader.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using Handle = std::int64_t;
    using GetAttachment = void(APIENTRY*)(GLenum, GLenum, GLenum, GLint*);
    using Uniform2f = void(APIENTRY*)(GLint, GLfloat, GLfloat);
    struct Target
    {
        std::int32_t previous = 0, viewport[4] = {};
        bool valid;
        explicit Target(Handle texture) : valid(igpu::gl_color_target_begin(texture, previous, viewport)) {}
        ~Target() { if (valid) igpu::gl_color_target_end(previous, viewport); }
    };
    std::array<std::byte, 12> pair_wire(std::int32_t a, std::int32_t b)
    {
        std::array<std::byte, 12> bytes{};
        bytes[0] = std::byte{2}; bytes[2] = bytes[7] = std::byte{6};
        std::memcpy(bytes.data() + 3, &a, 4); std::memcpy(bytes.data() + 8, &b, 4);
        return bytes;
    }
    template <std::size_t N> gm::wire::GMArrayView wire_view(const std::array<std::byte, N>& bytes)
    {
        return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array, bytes.data()));
    }
    bool check(bool result, const char* label)
    {
        if (!result) std::fprintf(stderr, "texture tests: %s: %s\n", label, igpu_get_last_error().c_str());
        return result;
    }
    GLuint compile_program(const char* vs, const char* ps)
    {
        auto& f = igpu_gl_fns(); const GLuint program = f.CreateProgram();
        const char* sources[] = {vs,ps};
        for (int i = 0; i < 2; ++i)
        {
            const GLuint shader = f.CreateShader(i ? 0x8B30 : 0x8B31);
            f.ShaderSource(shader,1,&sources[i],nullptr); f.CompileShader(shader);
            GLint ok = 0; f.GetShaderiv(shader,0x8B81,&ok);
            if (!ok)
            {
                char log[1024]{}; f.GetShaderInfoLog(shader,sizeof(log),nullptr,log);
                std::fprintf(stderr,"texture shader compile: %s\n",log);
                f.DeleteShader(shader); f.DeleteProgram(program); return 0;
            }
            f.AttachShader(program,shader); f.DeleteShader(shader);
        }
        f.BindAttribLocation(program,0,"in_pos"); f.BindAttribLocation(program,1,"in_uv");
        f.LinkProgram(program); GLint linked = 0; f.GetProgramiv(program,0x8B82,&linked);
        if (!linked)
        {
            char log[1024]{}; f.GetProgramInfoLog(program,sizeof(log),nullptr,log);
            std::fprintf(stderr,"texture program link: %s\n",log); f.DeleteProgram(program); return 0;
        }
        return program;
    }
    constexpr GLenum state_fields[] = {
        GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_SWAP_BYTES,
        GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SWAP_BYTES,
        0x88ED,0x88EF,0x8B8D,GL_TEXTURE_BINDING_2D,0x8894,0x85B5,0x8CA6,0x84E0,0x8CAA
    };
    struct GLState
    {
        std::array<GLint,19> integers{};
        std::array<GLint,4> viewport{};
        GLState()
        {
            for (std::size_t i=0; i<integers.size(); ++i) glGetIntegerv(state_fields[i],&integers[i]);
            glGetIntegerv(GL_VIEWPORT,viewport.data());
        }
        bool operator==(const GLState& b) const
        {
            for (std::size_t i=0;i<integers.size();++i)
                if (integers[i]!=b.integers[i])
                    std::fprintf(stderr,"texture state 0x%x changed %d -> %d\n",state_fields[i],integers[i],b.integers[i]);
            return integers==b.integers && viewport==b.viewport;
        }
    };
    // CPU fixture/readback operations temporarily use tightly packed client memory.
    // Actual IGPU draws run with the caller's PBO and pixel-store sentinels untouched.
    struct PixelIO
    {
        GLState saved;
        PixelIO()
        {
            igpu_gl_fns().BindBuffer(0x88EB,0); igpu_gl_fns().BindBuffer(0x88EC,0);
            for (int i=0; i<10; ++i) glPixelStorei(state_fields[i],i==0 || i==5 ? 1 : 0);
        }
        ~PixelIO()
        {
            for (int i=0; i<10; ++i) glPixelStorei(state_fields[i],saved.integers[i]);
            igpu_gl_fns().BindBuffer(0x88EB,saved.integers[10]); igpu_gl_fns().BindBuffer(0x88EC,saved.integers[11]);
            glBindTexture(GL_TEXTURE_2D,saved.integers[13]);
        }
    };
    struct Parameters
    {
        std::array<GLint,10> integers{};
        std::array<GLfloat,8> floats{};
        bool operator==(const Parameters& b) const { return integers==b.integers && floats==b.floats; }
    };
    Parameters parameters(GLuint texture)
    {
        GLint binding=0; glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding); glBindTexture(GL_TEXTURE_2D,texture);
        Parameters p;
        const GLenum integer_fields[]={GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,0x8072,GL_TEXTURE_MIN_FILTER,
            GL_TEXTURE_MAG_FILTER,0x813C,0x813D,0x884C,0x884D,0x8E42};
        const GLenum float_fields[]={0x84FE,0x8501,0x813A,0x813B};
        for (int i=0;i<10;++i) glGetTexParameteriv(GL_TEXTURE_2D,integer_fields[i],&p.integers[i]);
        for (int i=0;i<4;++i) glGetTexParameterfv(GL_TEXTURE_2D,float_fields[i],&p.floats[i]);
        glGetTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,p.floats.data()+4);
        glBindTexture(GL_TEXTURE_2D,binding); return p;
    }
    struct Probe
    {
        std::vector<Handle> textures, states;
        Handle vertex = 0, fragment = 0, buffer = 0, layout = 0;
        std::int64_t old_vertex = igpu_get_bound_shader(0), old_fragment = igpu_get_bound_shader(1);
        GLuint program = 0, ordinary_program = 0, color_program = 0, pbos[2]{};
        ~Probe()
        {
            igpu_gl_fns().DeleteBuffers(2,pbos);
            igpu_shader_bind(0, 1); igpu_shader_bind(old_vertex, 0); igpu_shader_bind(old_fragment, 1);
            for (auto driver_program : {program,ordinary_program,color_program})
                if (driver_program) igpu_gl_fns().DeleteProgram(driver_program);
            if (layout) igpu_input_layout_release(layout);
            if (buffer) igpu_buffer_release(buffer);
            if (fragment) igpu_shader_release(fragment);
            if (vertex) igpu_shader_release(vertex);
            for (auto s : states) if (s) igpu_state_release(s);
            for (auto t : textures) if (t) igpu_texture_release(t);
        }
        Handle texture(int format, int mips = 1, int size = 2)
        {
            const auto t = igpu_texture_create(0, size, size, 1, format, true, false, mips);
            if (t) textures.push_back(t);
            return t;
        }
        Handle sampler(int compare, int linear=0, int address=0, float finest=0.f, float coarsest=-1.f, int aniso=1)
        {
            const auto s = igpu_sampler_state_create(linear,linear,aniso>1 ? 1 : 0,address,address,address,aniso,255,compare,0.f,finest,coarsest);
            if (s) states.push_back(s);
            return s;
        }
        bool uv(float u)
        {
            float vertices[] = {-1,-1,u,.5f, 1,-1,u,.5f, -1,1,u,.5f,
                                 1,-1,u,.5f, 1,1,u,.5f, -1,1,u,.5f};
            return igpu_buffer_write(buffer, 0, gm::wire::GMBuffer(vertices, sizeof(vertices)));
        }
        GLuint name(Handle texture)
        {
            Target target(texture); GLint result = 0;
            const auto get = reinterpret_cast<GetAttachment>(wglGetProcAddress("glGetFramebufferAttachmentParameteriv"));
            if (!target.valid || !get) return 0;
            get(0x8D40, 0x8CE0, 0x8CD1, &result);
            return static_cast<GLuint>(result);
        }
        bool initialize()
        {
            const char* vs = "#version 130\nin vec2 in_pos; in vec2 in_uv; out vec2 uv;\n"
                             "void main(){ gl_Position=vec4(in_pos,0,1); uv=in_uv; }\n";
            const char* ps = "#version 130\nuniform sampler2DShadow igpu_tex; uniform vec2 values; in vec2 uv;\n"
                             "void main(){ gl_FragColor=vec4(texture(igpu_tex,vec3(uv,values.x)),0,0,1); }\n";
            vertex = igpu_shader_compile(vs, "main", 0, "glsl");
            fragment = igpu_shader_compile(ps, "main", 1, "glsl");
            buffer = igpu_buffer_create(96, 0, 1, 16);
            const auto usage = pair_wire(1,4), type = pair_wire(2,2), step = pair_wire(0,0);
            if (!vertex || !fragment || !buffer) return false;
            layout = igpu_input_layout_create(vertex, wire_view(usage), wire_view(type), wire_view(step), 2,16,0);
            program = compile_program(vs,ps);
            ordinary_program = compile_program(vs,"#version 130\nuniform sampler2D igpu_tex; in vec2 uv;\n"
                "void main(){ gl_FragColor=texture(igpu_tex,uv); }\n");
            color_program = compile_program(vs,"#version 130\nuniform vec2 values;\n"
                "void main(){ gl_FragColor=vec4(gl_FragCoord.x<1.0?values.x:values.y,0.5,0.75,1.0); }\n");
            return layout && program && ordinary_program && color_program && uv(.25f);
        }
        void sentinels()
        {
            auto& f=igpu_gl_fns(); f.GenBuffers(2,pbos);
            for (int i=0;i<2;++i) { f.BindBuffer(i ? 0x88EC : 0x88EB,pbos[i]); f.BufferData(i ? 0x88EC : 0x88EB,4096,nullptr,0x88E0); }
            const GLint values[]={8,7,1,2,1,8,9,2,1,1};
            for (int i=0;i<10;++i) glPixelStorei(state_fields[i],values[i]);
        }
        std::array<float,64> image(Handle source, int mip=0)
        {
            PixelIO io; std::array<float,64> pixels{};
            glBindTexture(GL_TEXTURE_2D,name(source)); glGetTexImage(GL_TEXTURE_2D,mip,GL_RGBA,GL_FLOAT,pixels.data());
            return pixels;
        }
        bool render(Handle destination, float left, float right)
        {
            igpu_gl_fns().UseProgram(color_program);
            const auto uniform = reinterpret_cast<Uniform2f>(wglGetProcAddress("glUniform2f"));
            if (!uniform) return false;
            uniform(igpu_gl_fns().GetUniformLocation(color_program,"values"),left,right);
            Target target(destination);
            if (!target.valid) return false;
            const GLState before;
            return igpu_draw(buffer,0,layout,4,0,6,1,0,0,0,0) && before==GLState{} && glGetError()==GL_NO_ERROR;
        }
        bool ordinary(Handle source, Handle destination, Handle state, float u, int expected)
        {
            if (!uv(u)) return false;
            igpu_gl_fns().UseProgram(ordinary_program); Target target(destination);
            if (!target.valid) return false;
            const auto object=name(source); const auto params=parameters(object); const GLState before;
            if (!igpu_draw_sampled(buffer,layout,4,0,6,source,0,0,0,state) || !(before==GLState{}) || !(params==parameters(object))) return false;
            unsigned char pixel[4]{}; { PixelIO io; glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); }
            if (std::abs(int(pixel[0])-expected)>1 || pixel[3]!=255 || glGetError()!=GL_NO_ERROR)
            {
                std::fprintf(stderr,"ordinary source %lld u %.2f: red %u expected %d\n",static_cast<long long>(source),u,pixel[0],expected);
                return false;
            }
            return true;
        }
        bool rejected(Handle source, Handle destination, Handle state, bool shadow)
        {
            if (!uv(.25f)) return false;
            igpu_gl_fns().UseProgram(shadow ? program : ordinary_program); Target target(destination);
            if (!target.valid) return false;
            const auto get_uniform=reinterpret_cast<void(APIENTRY*)(GLuint,GLint,GLint*)>(wglGetProcAddress("glGetUniformiv"));
            if (!get_uniform) return false;
            const GLuint selected=shadow ? program : ordinary_program;
            const GLint sampler_uniform=igpu_gl_fns().GetUniformLocation(selected,"igpu_tex");
            igpu_gl_fns().Uniform1i(sampler_uniform,7);
            std::array<unsigned char,4> before{}, after{};
            { PixelIO io; glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,before.data()); }
            const auto object=name(source); const auto params=parameters(object); const GLState saved;
            const bool drew = igpu_draw_sampled(buffer,layout,4,0,6,source,0,0,0,state);
            const bool preserved=saved==GLState{} && params==parameters(object);
            { PixelIO io; glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,after.data()); }
            GLint sampler_after=0; get_uniform(selected,sampler_uniform,&sampler_after);
            return !drew && preserved && before==after && sampler_after==7 && glGetError()==GL_NO_ERROR;
        }
        bool sample(Handle source, Handle destination, Handle state, float u, int expected, float reference = .5f)
        {
            if (!uv(u)) return false;
            igpu_gl_fns().UseProgram(program);
            const auto uniform = reinterpret_cast<Uniform2f>(wglGetProcAddress("glUniform2f"));
            if (!uniform) return false;
            uniform(igpu_gl_fns().GetUniformLocation(program, "values"), reference, 0.f);
            Target target(destination);
            if (!target.valid) return false;
            const auto object=name(source); const auto params=parameters(object); const GLState before;
            if (!igpu_draw_sampled(buffer,layout,4,0,6,source,0,0,0,state) || !(before==GLState{}) || !(params==parameters(object))) return false;
            unsigned char pixel[4]{};
            { PixelIO io; glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); }
            const bool matches = std::abs(int(pixel[0]) - expected) <= 1 && pixel[1] == 0 && pixel[2] == 0 &&
                                 pixel[3] == 255 && glGetError() == GL_NO_ERROR;
            if (!matches) std::fprintf(stderr, "shadow source %lld sampler %lld u %.2f ref %.2f: (%u,%u,%u,%u) expected red %d\n",
                static_cast<long long>(source), static_cast<long long>(state), u, reference,
                pixel[0],pixel[1],pixel[2],pixel[3],expected);
            return matches;
        }
    };
    struct CopyPipeline
    {
        static constexpr GLenum caps[]={GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,
            GL_STENCIL_TEST,0x8C89,0x809D,0x3000,0x3003};
        std::array<GLboolean,8> enabled{};
        std::vector<GLboolean> blend;
        std::array<GLboolean,4> color{};
        std::array<GLint,2> polygon{};
        std::array<GLint,4> scissor{};
        std::array<GLdouble,2> range{};
        GLboolean mask=GL_FALSE;
        GLint function=0;
        CopyPipeline()
        {
            for (int i=0;i<8;++i) enabled[i]=glIsEnabled(caps[i]);
            GLint count=0; glGetIntegerv(0x8824 /* MAX_DRAW_BUFFERS */,&count); blend.resize(count);
            const auto is_enabled=reinterpret_cast<GLboolean(APIENTRY*)(GLenum,GLuint)>(wglGetProcAddress("glIsEnabledi"));
            for(GLint i=0;i<count;++i) blend[i]=is_enabled(GL_BLEND,i);
            glGetBooleanv(GL_COLOR_WRITEMASK,color.data()); glGetIntegerv(GL_POLYGON_MODE,polygon.data());
            glGetIntegerv(GL_SCISSOR_BOX,scissor.data()); glGetDoublev(GL_DEPTH_RANGE,range.data());
            glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); glGetIntegerv(GL_DEPTH_FUNC,&function);
        }
        bool operator==(const CopyPipeline& b) const
        {
            for(std::size_t i=0;i<blend.size();++i)
                if(blend[i]!=b.blend[i]) std::fprintf(stderr,"private copy BLEND[%zu] changed %u -> %u\n",i,blend[i],b.blend[i]);
            return enabled==b.enabled && blend==b.blend && color==b.color && polygon==b.polygon && scissor==b.scissor &&
                   range==b.range && mask==b.mask && function==b.function;
        }
        void restore() const
        {
            for (int i=0;i<8;++i) { if(enabled[i]) glEnable(caps[i]); else glDisable(caps[i]); }
            const auto enable=reinterpret_cast<void(APIENTRY*)(GLenum,GLuint)>(wglGetProcAddress("glEnablei"));
            const auto disable=reinterpret_cast<void(APIENTRY*)(GLenum,GLuint)>(wglGetProcAddress("glDisablei"));
            for(GLuint i=0;i<blend.size();++i) { if(blend[i]) enable(GL_BLEND,i); else disable(GL_BLEND,i); }
            glColorMask(color[0],color[1],color[2],color[3]);
            if(polygon[0]==polygon[1]) glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);
            else { glPolygonMode(GL_FRONT,polygon[0]); glPolygonMode(GL_BACK,polygon[1]); }
            glScissor(scissor[0],scissor[1],scissor[2],scissor[3]);
            const auto set_range=reinterpret_cast<void(APIENTRY*)(GLdouble,GLdouble)>(wglGetProcAddress("glDepthRangedNV"));
            set_range(range[0],range[1]); glDepthMask(mask); glDepthFunc(function);
        }
    };
    IgpuGlCreateProgram original_create=nullptr;
    int copy_program_creations=0;
    GLuint APIENTRY refuse_copy_program() { return 0; }
    GLuint APIENTRY count_copy_programs() { ++copy_program_creations; return original_create(); }
    bool private_copy_regression(Probe& p, Handle source)
    {
        struct RestorePipeline { CopyPipeline saved; ~RestorePipeline(){saved.restore();} } restore_pipeline;
        const auto object=p.name(source);
        GLint original_texture=0, original_read=0, original_base=0, original_max=0, original_swizzle=0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&original_texture); glGetIntegerv(0x8CAA,&original_read);
        glBindTexture(GL_TEXTURE_2D,object);
        glGetTexParameteriv(GL_TEXTURE_2D,0x813C,&original_base); glGetTexParameteriv(GL_TEXTURE_2D,0x813D,&original_max);
        glGetTexParameteriv(GL_TEXTURE_2D,0x8E42 /* TEXTURE_SWIZZLE_R */,&original_swizzle);
        struct RestoreRange
        {
            GLuint object; GLint base,max,binding,swizzle;
            ~RestoreRange(){ glBindTexture(GL_TEXTURE_2D,object); glTexParameteri(GL_TEXTURE_2D,0x813C,base);
                glTexParameteri(GL_TEXTURE_2D,0x813D,max); glTexParameteri(GL_TEXTURE_2D,0x8E42,swizzle);
                glBindTexture(GL_TEXTURE_2D,binding); }
        } restore_range{object,original_base,original_max,original_texture,original_swizzle};
        glTexParameteri(GL_TEXTURE_2D,0x813C,1); glTexParameteri(GL_TEXTURE_2D,0x813D,1);
        glTexParameteri(GL_TEXTURE_2D,0x8E42,GL_ONE);
        glBindTexture(GL_TEXTURE_2D,original_texture);
        auto& f=igpu_gl_fns(); GLuint foreign_read=0;
        f.GenFramebuffers(1,&foreign_read); f.BindFramebuffer(0x8CA8,foreign_read); glReadBuffer(GL_NONE);
        struct RestoreRead
        {
            GLint previous; GLuint object;
            ~RestoreRead(){igpu_gl_fns().BindFramebuffer(0x8CA8,previous);igpu_gl_fns().DeleteFramebuffers(1,&object);}
        } restore_read{original_read,foreign_read};
        for(const auto cap : CopyPipeline::caps) glEnable(cap);
        const auto enable_index=reinterpret_cast<void(APIENTRY*)(GLenum,GLuint)>(wglGetProcAddress("glEnablei"));
        const auto disable_index=reinterpret_cast<void(APIENTRY*)(GLenum,GLuint)>(wglGetProcAddress("glDisablei"));
        enable_index(GL_BLEND,0); disable_index(GL_BLEND,1);
        glDepthMask(GL_FALSE); glDepthFunc(GL_GREATER); glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); glScissor(0,0,0,0);
        const auto set_range=reinterpret_cast<void(APIENTRY*)(GLdouble,GLdouble)>(wglGetProcAddress("glDepthRangedNV"));
        set_range(-7.0,11.0);
        const GLState before; const CopyPipeline pipeline; const auto params=parameters(object);
        struct ShadowOwner { igpu::GlShadow value; ~ShadowOwner(){igpu::gl_shadow_release(value);} } shadow;
        struct RestoreCreate
        {
            IgpuGlCreateProgram saved;
            ~RestoreCreate(){igpu_gl_fns().CreateProgram=saved;}
        } restore_create{f.CreateProgram};
        original_create=f.CreateProgram; copy_program_creations=0; f.CreateProgram=count_copy_programs;
        const bool copied=igpu::gl_shadow_sync(object,2,2,2,shadow.value) &&
                          igpu::gl_shadow_sync(object,2,2,2,shadow.value);
        bool valid=copied && copy_program_creations==1 && before==GLState{} && pipeline==CopyPipeline{} && params==parameters(object);
        igpu::GlShadow failed{};
        f.CreateProgram=refuse_copy_program;
        const bool rejected=!igpu::gl_shadow_sync(object,2,2,2,failed);
        f.CreateProgram=count_copy_programs;
        valid=valid && rejected && !igpu_get_last_error().empty() && !failed.texture && !failed.framebuffer &&
              !failed.program && !failed.vertex_array && before==GLState{} && pipeline==CopyPipeline{} && params==parameters(object);
        {
            PixelIO io; glBindTexture(GL_TEXTURE_2D,shadow.value.texture);
            float base[4]{},coarse[1]{};
            glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,base);
            glGetTexImage(GL_TEXTURE_2D,1,GL_DEPTH_COMPONENT,GL_FLOAT,coarse);
            valid=valid && base[0]==.25f && base[1]==.75f && base[2]==.25f && base[3]==.75f && coarse[0]==.5f;
        }
        return check(valid && glGetError()==GL_NO_ERROR,"private depth copy pixels/cache/source range/pipeline restoration");
    }
    bool mip_range_regression(Probe& p)
    {
        const auto chain = p.texture(6, 0, 4);
        const auto texture = p.name(chain);
        if (!check(texture != 0, "4x4 mip-chain creation")) return false;
        GLint binding = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        glBindTexture(GL_TEXTURE_2D, texture);
        std::array<unsigned char, 64> base{}, zero{};
        const unsigned char color[] = {64,128,192,255};
        for (std::size_t i = 0; i < base.size(); ++i) base[i] = color[i % 4];
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, base.data());
        const GLenum upload_base_error = glGetError();
        glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, zero.data());
        const GLenum upload_one_error = glGetError();
        glTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, zero.data());
        const GLenum upload_two_error = glGetError();
        // Caller range conflicts with the allocated 0..2 chain. Generation must ignore it temporarily.
        glTexParameteri(GL_TEXTURE_2D, 0x813C, 1); glTexParameteri(GL_TEXTURE_2D, 0x813D, 1);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(binding));
        const GLenum setup_error = glGetError();
        const bool generated = igpu_texture_generate_mips(chain);
        const GLenum generation_error = glGetError();
        GLint after_binding = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &after_binding);
        glBindTexture(GL_TEXTURE_2D, texture);
        GLint base_level = 0, max_level = 0;
        glGetTexParameteriv(GL_TEXTURE_2D, 0x813C, &base_level);
        glGetTexParameteriv(GL_TEXTURE_2D, 0x813D, &max_level);
        // Validate restoration above, then expose the whole chain only for driver readback.
        glTexParameteri(GL_TEXTURE_2D, 0x813C, 0); glTexParameteri(GL_TEXTURE_2D, 0x813D, 2);
        bool pixels_match = true, levels_valid = true;
        int mismatch_mip = -1, mismatch_index = -1, mismatch_actual = -1, mismatch_expected = -1;
        for (int mip = 0; mip < 3; ++mip)
        {
            std::array<unsigned char, 64> pixels{};
            GLint width = 0, height = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_WIDTH, &width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, mip, GL_TEXTURE_HEIGHT, &height);
            glGetTexImage(GL_TEXTURE_2D, mip, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            const GLenum level_error = glGetError();
            const int size = 4 >> mip;
            levels_valid = levels_valid && width == size && height == size && level_error == GL_NO_ERROR;
            std::fprintf(stderr, "mip read: level=%d size=%dx%d error=0x%x\n", mip, width, height, level_error);
            for (int i = 0; i < size*size*4; ++i)
                if (pixels[i] != color[i % 4])
                {
                    pixels_match = false;
                    if (mismatch_mip < 0)
                    {
                        mismatch_mip = mip; mismatch_index = i;
                        mismatch_actual = pixels[i]; mismatch_expected = color[i % 4];
                    }
                }
        }
        glTexParameteri(GL_TEXTURE_2D, 0x813C, base_level); glTexParameteri(GL_TEXTURE_2D, 0x813D, max_level);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(binding));
        const GLenum read_error = glGetError();
        if (!(generated && pixels_match && levels_valid && read_error==GL_NO_ERROR))
            std::fprintf(stderr, "mip range: generated=%d binding=%d/%d BASE/MAX=%d/%d errors upload[0,1,2]=0x%x/0x%x/0x%x setup=0x%x generate=0x%x read=0x%x first mismatch mip=%d index=%d actual=%d expected=%d\n",
                     generated ? 1 : 0, binding, after_binding, base_level, max_level,
                     upload_base_error, upload_one_error, upload_two_error, setup_error, generation_error, read_error,
                     mismatch_mip, mismatch_index, mismatch_actual, mismatch_expected);
        return check(generated && after_binding == binding && base_level == 1 && max_level == 1 && pixels_match && levels_valid &&
                     upload_base_error == GL_NO_ERROR && upload_one_error == GL_NO_ERROR && upload_two_error == GL_NO_ERROR &&
                     setup_error == GL_NO_ERROR && generation_error == GL_NO_ERROR && read_error == GL_NO_ERROR,
                     "mipgen must regenerate allocated 0..2 and restore caller BASE=MAX=1");
    }
}

bool run_gl_texture_tests()
{
    PixelIO clean_caller_transfers;
    Probe p;
    const auto source = p.texture(10), destination = p.texture(6);
    if (!check(source && destination, "R32F render-target creation") || !check(p.initialize(), "shadow draw setup")) return false;
    const auto less=p.sampler(2), greater=p.sampler(5), linear=p.sampler(2,1), reverse_linear=p.sampler(5,1);
    const GLuint texture = p.name(source);
    if (!check(less && greater && linear && reverse_linear && texture, "comparison fixtures")) return false;
    GLint binding = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glBindTexture(GL_TEXTURE_2D, texture);
    const float values[] = {.25f,.75f,.25f,.75f};
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, 0x1903, GL_FLOAT, values);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(binding));
    if (!check(glGetError() == GL_NO_ERROR, "R32F fixture upload")) return false;
    if (!check(p.sample(source,destination,less,.25f,0) && p.sample(source,destination,less,.75f,255) &&
               p.sample(source,destination,greater,.25f,255) && p.sample(source,destination,greater,.75f,0) &&
               p.sample(source,destination,linear,.5f,128) && p.sample(source,destination,reverse_linear,.5f,128), "actual comparison pixels")) return false;
    const auto half = p.texture(9);
    const auto ordinary = p.sampler(0);
    if (!check(ordinary != 0, "ordinary sampler creation")) return false;
    std::array<Handle, 8> comparison{};
    for (int i = 0; i < 8; ++i) if (!check((comparison[i] = p.sampler(i+1)) != 0, "comparison constructor")) return false;
    const auto comparison_result = [](int compare, float reference, float texel) -> int {
        switch (compare)
        {
            case 1: return 0;
            case 2: return reference < texel ? 255 : 0;
            case 3: return reference == texel ? 255 : 0;
            case 4: return reference <= texel ? 255 : 0;
            case 5: return reference > texel ? 255 : 0;
            case 6: return reference != texel ? 255 : 0;
            case 7: return reference >= texel ? 255 : 0;
            default: return 255;
        }
    };
    constexpr int formats[]={6,9,10,11,12,13,14,15}, channels[]={4,1,1,4,1,2,4,4};
    std::array<Handle,8> format_textures{};
    for (int i=0;i<8;++i) if (!check((format_textures[i]=p.texture(formats[i],0))!=0,"format/mip creation")) return false;
    const auto coarse=p.sampler(3,0,0,1.f,1.f), ordinary_coarse=p.sampler(0,0,0,1.f,1.f), border=p.sampler(0,0,3);
    const auto released=p.sampler(2); if (!check(released && igpu_state_release(released),"sampler release")) return false;
    p.states.back()=0;
    GLfloat max_aniso=1.f; glGetFloatv(0x84FF,&max_aniso);
    const auto aniso_cmp=max_aniso>=2.f ? p.sampler(2,1,0,0.f,-1.f,2) : 0;
    const auto aniso_normal=max_aniso>=2.f ? p.sampler(0,1,0,0.f,-1.f,2) : 0;
    if (!check(coarse && ordinary_coarse && border,"LOD/border fixtures")) return false;
    if (!mip_range_regression(p)) return false;
    p.sentinels();
    for (int i=0;i<8;++i)
    {
        const auto t=format_textures[i];
        if (!check(p.render(t,.25f,.25f),"format public rendering")) return false;
        const auto object=p.name(t); const auto params=parameters(object); const GLState before;
        if (!check(igpu_texture_generate_mips(t) && before==GLState{} && params==parameters(object),"mip generation/state")) return false;
        for (int mip=0;mip<2;++mip)
        {
            const auto pixels=p.image(t,mip); const int size=2>>mip;
            for (int pixel=0;pixel<size*size;++pixel)
                for (int channel=0;channel<4;++channel)
                {
                    const float expected=channel==3 ? 1.f : channel<channels[i] ? .25f*(channel+1) : 0.f;
                    if (!check(std::abs(pixels[pixel*4+channel]-expected)<(formats[i]==11 ? .034f : .005f),"format/mip pixel")) return false;
                }
            PixelIO io; const auto read=igpu_texture_read(t,0,0,0,mip);
            if (!check(formats[i]==6 ? read==(64|(128<<8)|(191<<16)) : read==0 && !igpu_get_last_error().empty(),
                       "public read supports only RGBA8")) return false;
        }
        if (!check(p.ordinary(t,destination,ordinary,.25f,int(std::lround(p.image(t)[0]*255.f))),"ordinary format sample/state")) return false;
    }
    if (!check(p.render(source,.25f,.75f) && p.sample(source,destination,less,.75f,255) &&
               p.ordinary(source,destination,ordinary,.25f,64) && p.ordinary(source,destination,ordinary,.75f,191),
               "ordinary sampling after comparison/public R32F rendering")) return false;
    if (!check(p.rejected(destination,source,less,true) && p.rejected(source,destination,ordinary,true) &&
               p.rejected(source,destination,less,false) && p.rejected(source,destination,released,true) &&
               p.rejected(source,destination,0x7FFFFFFFFFFFFFFFLL,true),"format/type/released/unknown rejection")) return false;
    if (!check(p.ordinary(source,destination,border,-1.f,255),"ordinary red border")) return false;
    const auto mip_source=format_textures[2];
    if (!check(p.render(mip_source,.25f,.75f) && igpu_texture_generate_mips(mip_source) &&
               p.sample(mip_source,destination,coarse,.25f,255) && p.ordinary(mip_source,destination,ordinary_coarse,.25f,128) &&
               p.sample(mip_source,destination,less,.25f,0) && p.sample(mip_source,destination,less,.75f,255),"coarse LOD/default reset")) return false;
    const auto fractional=igpu_sampler_state_create(0,0,1,0,0,0,1,0,3,0.f,.5f,.5f);
    if (fractional) p.states.push_back(fractional);
    if (!check(fractional && p.sample(mip_source,destination,fractional,.25f,128) &&
               p.sample(mip_source,destination,fractional,.75f,128),"linear mip comparison at LOD 0.5")) return false;
    if (!private_copy_regression(p,mip_source)) return false;
    if (max_aniso>=2.f && !check(aniso_cmp && aniso_normal && p.sample(source,destination,aniso_cmp,.25f,0) &&
        p.sample(source,destination,aniso_cmp,.75f,255) && p.ordinary(source,destination,aniso_normal,.25f,64),"anisotropic compare/ordinary")) return false;
    std::printf("texture tests: eight formats, mip pixels/state, ordinary-after, validation and LOD passed\n");
    int matrix_samples = 0;
    for (const auto float_source : {source, half})
    {
        const auto object = p.name(float_source);
        if (!check(object != 0, "floating source name")) return false;
        for (const auto range : {std::array<float,2>{.25f,.75f}, std::array<float,2>{.5f,.5f},
                                std::array<float,2>{-.25f,1.25f}, std::array<float,2>{-65504.f,65504.f}})
        {
            if (!check(p.render(float_source,range[0],range[1]), "public float render/update")) return false;
            const auto raw=p.image(float_source);
            if (!check(raw[0]==range[0] && raw[4]==range[1], "float render must retain original values")) return false;
            for (float reference : {.5f, -.5f, 1.5f, -.25f, -.125f, 1.125f, 1.25f, -65504.f, -32752.f, 32752.f, 65504.f})
                for (int compare = 1; compare <= 8; ++compare)
                    for (int side = 0; side < 2; ++side)
                    {
                        if (!check(p.sample(float_source,destination,comparison[compare-1],side ? .75f : .25f,
                                            comparison_result(compare,reference,range[side]),reference), "comparison matrix")) return false;
                        ++matrix_samples;
                    }
        }
    }
    for (const float magnitude : {1.e30f, (std::numeric_limits<float>::max)()})
    {
        if (!check(p.render(source,-magnitude,magnitude), "extreme R32F source rendering")) return false;
        const auto raw=p.image(source);
        if (!check(raw[0]==-magnitude && raw[4]==magnitude, "extreme R32F source data")) return false;
        for (const float reference : {-magnitude, -magnitude/2.f, 0.f, magnitude/2.f, magnitude})
            for (int compare=1; compare<=8; ++compare)
                for (int side=0; side<2; ++side)
                {
                    if (!check(p.sample(source,destination,comparison[compare-1],side ? .75f : .25f,
                                        comparison_result(compare,reference,side ? magnitude : -magnitude),reference),
                               "extreme R32F comparison")) return false;
                    ++matrix_samples;
                }
    }
    std::printf("texture tests: %d matrix samples plus point/linear and mip-range regression passed\n", matrix_samples);
    return true;
}
