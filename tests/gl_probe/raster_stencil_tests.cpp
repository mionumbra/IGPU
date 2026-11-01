#include "raster_stencil_tests.h"
#include "IGPU_native.h"
#include "native/igpu_capabilities.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include <array>
#include <vector>
#include <cstdio>
#include <cstring>

namespace
{
    using Handle = std::uint64_t;
    using StencilFunc = void(APIENTRY*)(GLenum, GLenum, GLint, GLuint);
    using StencilOp = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
    using StencilMask = void(APIENTRY*)(GLenum, GLuint);
    bool check(bool ok, const char* label)
    { if (!ok) std::fprintf(stderr, "raster stencil: %s: %s\n", label, igpu_get_last_error().c_str()); return ok; }
    struct Array
    {
        std::vector<std::byte> bytes;
        explicit Array(std::initializer_list<Handle> values) : bytes(2 + 9 * values.size())
        {
            const auto count = static_cast<std::uint16_t>(values.size()); std::memcpy(bytes.data(), &count, 2);
            std::size_t i = 0;
            for (auto value : values) { bytes[2 + 9 * i] = std::byte{12}; std::memcpy(bytes.data() + 3 + 9 * i++, &value, 8); }
        }
        gm::wire::GMArrayView view() const
        { return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array, bytes.data())); }
    };
    struct NativeState
    {
        static constexpr GLenum fields[] = {0x8B8D,0x85B5,0x8894,0x8CA6,0x8CAA,0x0B44,0x0B45,0x0B46,0x0C11,
            0x0B71,0x0B72,0x0B74,0x0B90,0x0B92,0x0B93,0x0B94,0x0B95,0x0B96,0x0B97,0x0B98,
            0x8800,0x8801,0x8802,0x8803,0x8CA3,0x8CA4,0x8CA5,0x864F};
        std::array<GLint,28> values{}; std::array<GLint,2> polygon{};
        NativeState() { for(int i=0;i<28;++i) glGetIntegerv(fields[i],&values[i]); glGetIntegerv(0x0B40,polygon.data()); }
        bool operator==(const NativeState& other)const {return values==other.values&&polygon==other.polygon;}
    };
    struct Measurement { bool accepted=false, restored=false; std::int64_t samples=-1,pixel=-1; GLenum error=0; };
    struct Fixture
    {
        GLint saved_vao=0,saved_array=0,saved_draw=0,saved_read=0,saved_renderbuffer=0,original_program=0;
        GLint viewport[4]{}; std::array<Handle,6> old_stages{};
        std::vector<Handle> shaders,states;
        Handle vertex=0,index=0,args=0,layout=0,target=0,source=0,vs=0,ps=0;
        GLuint depth_stencil=0;
        StencilFunc stencil_func=reinterpret_cast<StencilFunc>(wglGetProcAddress("glStencilFuncSeparate"));
        StencilOp stencil_op=reinterpret_cast<StencilOp>(wglGetProcAddress("glStencilOpSeparate"));
        StencilMask stencil_mask=reinterpret_cast<StencilMask>(wglGetProcAddress("glStencilMaskSeparate"));
        Fixture()
        {
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            glGetIntegerv(0x85B5,&saved_vao);glGetIntegerv(0x8894,&saved_array);glGetIntegerv(0x8CA6,&saved_draw);
            glGetIntegerv(0x8CAA,&saved_read);glGetIntegerv(0x8CA7,&saved_renderbuffer);glGetIntegerv(0x8B8D,&original_program);
            glGetIntegerv(GL_VIEWPORT,viewport);for(int i=0;i<6;++i)old_stages[i]=igpu_get_bound_shader(i);
        }
        ~Fixture()
        {
            auto&f=igpu_gl_fns();
            for(int stage:{3,4,5,0,1})igpu_shader_bind(old_stages[stage],stage);
            for(auto shader:shaders)igpu_shader_release(shader);for(auto state:states)igpu_state_release(state);
            if(depth_stencil)f.DeleteRenderbuffers(1,&depth_stencil);
            if(layout)igpu_input_layout_release(layout);for(auto buffer:{vertex,index,args})if(buffer)igpu_buffer_release(buffer);
            for(auto texture:{target,source})if(texture)igpu_texture_release(texture);
            f.BindFramebuffer(0x8CA9,saved_draw);f.BindFramebuffer(0x8CA8,saved_read);f.BindRenderbuffer(0x8D41,saved_renderbuffer);
            f.BindVertexArray(saved_vao);f.BindBuffer(0x8892,saved_array);glPopAttrib();
            using IsProgram=GLboolean(APIENTRY*)(GLuint);const auto exists=reinterpret_cast<IsProgram>(wglGetProcAddress("glIsProgram"));
            if(!original_program||exists(original_program))f.UseProgram(original_program);
        }
        Handle shader(const char*code,int stage)
        {const auto h=static_cast<Handle>(igpu_shader_compile(code,"main",stage,"glsl"));if(h)shaders.push_back(h);return h;}
        bool positions(bool inner=false)
        {
            std::array<float,6> values=inner?std::array<float,6>{-.875f,-.875f,.875f,-.875f,0,.875f}:std::array<float,6>{-1,-1,3,-1,-1,3};
            return igpu_buffer_write(vertex,0,gm::wire::GMBuffer(values.data(),sizeof(values)));
        }
        void z(float value)
        {
            auto&f=igpu_gl_fns();GLint program=0;glGetIntegerv(0x8B8D,&program);
            using Uniform=void(APIENTRY*)(GLint,GLfloat);const auto uniform=reinterpret_cast<Uniform>(wglGetProcAddress("glUniform1f"));
            uniform(f.GetUniformLocation(program,"z"),value);
        }
        bool initialize()
        {
            vs=shader("#version 430\nlayout(location=0)in vec2 p;uniform float z;out vec2 uv;void main(){gl_Position=vec4(p,z,1);uv=p*.5+.5;}",0);
            ps=shader("#version 430\nout vec4 c;void main(){c=vec4(0,1,0,1);}",1);
            vertex=igpu_buffer_create(24,1,1,8);index=igpu_buffer_create(6,1,2,0);args=igpu_buffer_create(64,0,16,0);
            Array usage{1},type{2},step{0};layout=igpu_input_layout_create(vs,usage.view(),type.view(),step.view(),1,8,0);
            target=igpu_texture_create(0,16,16,1,6,true,false,1);source=igpu_texture_create(0,16,16,1,6,true,false,1);
            std::array<std::uint16_t,3>indices{0,1,2};std::array<std::uint32_t,16>commands{3,1,0,0,0,0,0,0,3,1,0,0,0};
            if(!check(vs&&ps&&vertex&&index&&args&&layout&&target&&source&&stencil_func&&stencil_op&&stencil_mask&&
                positions()&&igpu_buffer_write(index,0,gm::wire::GMBuffer(indices.data(),sizeof(indices)))&&
                igpu_buffer_write(args,0,gm::wire::GMBuffer(commands.data(),sizeof(commands)))&&igpu_shader_bind(vs,0)&&igpu_shader_bind(ps,1),"fixture allocation/pipeline"))return false;
            auto&f=igpu_gl_fns();std::int32_t ignored=0,box[4]{};
            if(!igpu::gl_color_target_begin(target,ignored,box))return false;
            f.GenRenderbuffers(1,&depth_stencil);f.BindRenderbuffer(0x8D41,depth_stencil);
            f.RenderbufferStorage(0x8D41,0x88F0,16,16);f.FramebufferRenderbuffer(0x8CA9,0x821A,0x8D41,depth_stencil);
            for(auto cap:{GL_CULL_FACE,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_SCISSOR_TEST,GL_BLEND,GL_ALPHA_TEST,0x8C89,0x809D,0x864F})glDisable(cap);
            GLint clips=0;glGetIntegerv(0x0D32,&clips);for(int i=0;i<clips;++i)glDisable(GL_CLIP_PLANE0+i);
            glFrontFace(GL_CCW);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glLineWidth(1.f);z(0.f);
            return check(f.CheckFramebufferStatus(0x8CA9)==0x8CD5&&!glGetError(),"borrowed native packed depth/stencil attachment");
        }
        Handle raster(int fill=0,bool clip=true,int cull=0,bool scissor=false)
        {const auto h=igpu_raster_state_create(cull,fill,scissor,clip);if(h)states.push_back(h);return h;}
        Handle depth(bool test,bool enabled,int func=8,int fail=1,int zfail=1,int pass=1,int ref=0,int read=255,int write=255,int depth_func=8)
        {const auto h=igpu_depth_state_create(test,false,depth_func,enabled,func,fail,zfail,pass,ref,read,write);if(h)states.push_back(h);return h;}
        void clear(int stencil=0)
        {
            glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT|GL_SCISSOR_BIT);
            glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
            stencil_mask(GL_FRONT,255);stencil_mask(GL_BACK,255);glClearColor(0,0,0,1);glClearDepth(1);glClearStencil(stencil);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);glPopAttrib();
        }
        int stencil()
        {
            auto&f=igpu_gl_fns();GLint draw=0,read=0,pack=0;glGetIntegerv(0x8CA6,&draw);glGetIntegerv(0x8CAA,&read);glGetIntegerv(0x88ED,&pack);
            glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);f.BindBuffer(0x88EB,0);f.BindFramebuffer(0x8CA8,draw);
            glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(0x0D02,0);glPixelStorei(0x0D03,0);glPixelStorei(0x0D04,0);
            unsigned char value=0;glReadPixels(8,8,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&value);
            f.BindFramebuffer(0x8CA8,read);glPopClientAttrib();f.BindBuffer(0x88EB,pack);return value;
        }
        void caller_stencil()
        {
            glEnable(GL_STENCIL_TEST);stencil_func(GL_FRONT,GL_NEVER,17,0x3F);stencil_func(GL_BACK,GL_GREATER,29,0xF0);
            stencil_op(GL_FRONT,GL_KEEP,GL_ZERO,GL_INCR);stencil_op(GL_BACK,GL_INVERT,GL_REPLACE,GL_DECR);
            stencil_mask(GL_FRONT,0x5A);stencil_mask(GL_BACK,0xA5);
        }
        template<class Submit>Measurement measure(Submit submit)
        {
            const NativeState before;const auto query=igpu_query_create(0);const bool began=query&&igpu_query_begin(query);
            Measurement result;result.accepted=began&&submit();const bool ended=began&&igpu_query_end(query);
            if(ended)result.samples=igpu_query_result(query);if(query)igpu_query_release(query);
            result.restored=before==NativeState{};result.error=glGetError();result.pixel=igpu_texture_read(target,8,8,0,0);return result;
        }
        bool draw(Handle depth_state=0,Handle raster_state=0,int mode=0)
        {
            if(mode==1)return igpu_draw_indexed(vertex,layout,index,4,0,3,0,depth_state,raster_state,0);
            if(mode==2)return igpu_draw(vertex,0,layout,4,0,3,2,0,depth_state,raster_state,0);
            if(mode==3)return igpu_draw_indirect(vertex,0,layout,4,args,0,0,depth_state,raster_state,0);
            if(mode==4)return igpu_draw_indexed_indirect(vertex,0,layout,index,4,args,32,0,depth_state,raster_state,0);
            return igpu_draw(vertex,0,layout,4,0,3,1,0,depth_state,raster_state,0);
        }
        bool expect(Handle depth_state,Handle raster_state,int pixel,int samples,const char*label,int mode=0)
        {
            const auto value=measure([&]{return draw(depth_state,raster_state,mode);});
            if(!value.accepted||!value.restored||value.error||value.pixel!=pixel||value.samples!=samples)
                std::fprintf(stderr,"raster stencil observed: %s accepted=%d restored=%d error=%x pixel=%lld/%d samples=%lld/%d\n",label,value.accepted,value.restored,value.error,(long long)value.pixel,pixel,(long long)value.samples,samples);
            return check(value.accepted&&value.restored&&!value.error&&value.pixel==pixel&&value.samples==samples,label);
        }
    };
    bool raster_test()
    {
        Fixture x;if(!x.initialize()||!x.positions(true))return false;
        const auto solid=x.raster(),wire=x.raster(1),clamped=x.raster(0,false);
        if(!check(solid&&wire&&clamped&&igpu::supports(igpu::Capability::Wireframe),"wireframe capability and complete raster descriptors"))return false;
        glPolygonMode(GL_FRONT,GL_LINE);glPolygonMode(GL_BACK,GL_POINT);x.clear();
        const auto filled=x.measure([&]{return x.draw(0,solid);});x.clear();const auto lines=x.measure([&]{return x.draw(0,wire);});
        if(!check(filled.accepted&&filled.restored&&!filled.error&&filled.pixel==65280&&filled.samples>64&&
            lines.accepted&&lines.restored&&!lines.error&&lines.pixel==0&&lines.samples>0&&lines.samples<filled.samples,"real filled/wire pixels and asymmetric polygon restoration"))return false;
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);if(!x.positions())return false;x.z(2.f);glEnable(0x864F);x.clear();
        if(!x.expect(0,solid,0,0,"explicit depth clip overrides caller clamp")||!x.expect(0,clamped,65280,256,"depth clip false clamps far plane and restores caller enable"))return false;
        x.clear();if(!x.expect(0,0,65280,256,"handle0 keeps native depth clamp"))return false;
        glDisable(0x864F);x.clear();if(!x.expect(0,clamped,65280,256,"clamp state restores caller disabled bit"))return false;
        if(!x.positions(true))return false;x.z(0.f);
        for(int mode=1;mode<=4;++mode)
        {
            glPolygonMode(GL_FRONT,GL_LINE);glPolygonMode(GL_BACK,GL_POINT);x.clear();
            const auto value=x.measure([&]{return x.draw(0,wire,mode);});
            if(!check(value.accepted&&value.restored&&!value.error&&value.pixel==0&&value.samples>0&&value.samples<filled.samples*(mode==2?2:1),"advanced wireframe native pixels/state"))return false;
        }
        const auto rejected_before=NativeState{};
        const bool invalid=igpu_draw(x.vertex,0,x.layout,4,0,3,1,-1,0,clamped,0);
        const bool error=!igpu_get_last_error().empty();const auto native_error=glGetError();
        if(!check(!invalid&&error&&!native_error&&rejected_before==NativeState{},"late invalid blend does not apply raster state"))return false;
        std::printf("raster states: real wireframe/fill pixels, asymmetric modes, depthclip/clamp and handle0 native restoration passed\n");return true;
    }
    enum class StrictMode { ColorOnly, Extension };
    StrictMode strict_mode = StrictMode::ColorOnly;
    decltype(&glGetIntegerv) original_query = nullptr;
    decltype(&glIsEnabled) original_enabled = nullptr;
    decltype(&glEnable) original_enable = nullptr;
    decltype(&glDisable) original_disable = nullptr;
    IgpuGlDrawArraysInstanced original_instances = nullptr;
    GLenum active_stencil_face = GL_BACK;
    bool extension_enabled = true, extension_seen_during_draw = true;
    void APIENTRY strict_query(GLenum field, GLint* value)
    {
        if (strict_mode == StrictMode::Extension)
        {
            if (field == 0x8911) { *value = static_cast<GLint>(active_stencil_face); return; }
            if (active_stencil_face == GL_BACK)
            {
                const GLenum names[] = {0x0B92,0x0B97,0x0B93,0x0B98,0x0B94,0x0B95,0x0B96};
                const GLint values[] = {GL_ALWAYS,37,0x6F,0x39,GL_KEEP,GL_INCR,GL_REPLACE};
                for (int i=0;i<7;++i) if(field==names[i]) { *value=values[i];return; }
            }
        }
        original_query(field,value);
        if (strict_mode == StrictMode::ColorOnly && (field==0x0B97 || field==0x8CA3))
        {
            GLint bits=0;glGetIntegerv(GL_STENCIL_BITS,&bits);
            if(!bits)*value=0;
        }
    }
    GLboolean APIENTRY strict_enabled(GLenum capability)
    { return capability==0x8910 ? (extension_enabled?GL_TRUE:GL_FALSE) : original_enabled(capability); }
    void APIENTRY strict_enable(GLenum capability)
    { if(capability==0x8910)extension_enabled=true;else original_enable(capability); }
    void APIENTRY strict_disable(GLenum capability)
    { if(capability==0x8910)extension_enabled=false;else original_disable(capability); }
    void APIENTRY strict_face(GLenum face) { active_stencil_face=face; }
    void APIENTRY strict_instances(GLenum primitive,GLint first,GLsizei count,GLsizei instances)
    {
        extension_seen_during_draw=extension_enabled;
        original_instances(primitive,first,count,instances);
    }
    struct StrictDriver
    {
        decltype(igpu_gl_fns().ActiveStencilFaceEXT) saved_face;
        explicit StrictDriver(StrictMode mode):saved_face(igpu_gl_fns().ActiveStencilFaceEXT)
        {
            auto&f=igpu_gl_fns();strict_mode=mode;original_query=f.GetIntegerv;f.GetIntegerv=strict_query;
            original_enabled=f.IsEnabled;original_enable=f.Enable;original_disable=f.Disable;original_instances=f.DrawArraysInstanced;
            if(mode==StrictMode::Extension)
            {
                active_stencil_face=GL_BACK;extension_enabled=true;extension_seen_during_draw=true;
                f.ActiveStencilFaceEXT=strict_face;f.IsEnabled=strict_enabled;f.Enable=strict_enable;f.Disable=strict_disable;
                f.DrawArraysInstanced=strict_instances;
            }
        }
        ~StrictDriver()
        {
            auto&f=igpu_gl_fns();f.GetIntegerv=original_query;f.ActiveStencilFaceEXT=saved_face;
            f.IsEnabled=original_enabled;f.Enable=original_enable;f.Disable=original_disable;f.DrawArraysInstanced=original_instances;
        }
    };
    bool strict_stencil_driver(Fixture&x,Handle both,Handle disabled)
    {
        struct Owned {Handle value;~Owned(){if(value)igpu_texture_release(value);}};
        Owned layered{static_cast<Handle>(igpu_texture_create(2,16,16,2,6,true,false,1))};
        if(!check(layered.value!=0,"strict layered target"))return false;
        Array single{x.source},multiple{x.target,x.source},volume{layered.value},zero{0},one{1},zeros{0,0};
        bool ok=true;
        for(auto state:{disabled,both})for(int family=0;family<3;++family)
        {
            x.clear();x.caller_stencil(); // Prime references immediately before each FBO switch.
            const StrictDriver driver(StrictMode::ColorOnly);
            const auto result=x.measure([&]{
                if(family==0)return igpu_draw_to_render_targets(x.vertex,x.layout,4,0,3,single.view(),zero.view(),zero.view(),0,state,0,0);
                if(family==1)return igpu_draw_to_render_targets(x.vertex,x.layout,4,0,3,multiple.view(),zeros.view(),zeros.view(),0,state,0,0);
                return igpu_draw_to_render_targets(x.vertex,x.layout,4,0,3,volume.view(),one.view(),zero.view(),0,state,0,0);
            });
            const Handle texture=family==0?x.source:family==1?x.target:layered.value;
            ok=check(result.accepted&&result.restored&&!result.error&&result.samples==256&&
                igpu_texture_read(texture,8,8,family==2?1:0,0)==65280,
                "strict zero-stencil-bit query preserves freshly primed front17/back29 across single/MRT/layered")&&ok;
        }
        for(bool was_enabled:{false,true})for(GLenum face:{GL_FRONT,GL_BACK})
        {
            x.clear();x.caller_stencil();
            const StrictDriver driver(StrictMode::Extension);
            extension_enabled=was_enabled;active_stencil_face=face;
            const auto result=x.measure([&]{return x.draw(both,0,2);});
            const auto stencil_value=x.stencil();
            if(!result.accepted||!result.restored||result.error||result.samples!=512||stencil_value!=91||
                active_stencil_face!=face||extension_enabled!=was_enabled||extension_seen_during_draw)
                std::fprintf(stderr,"strict EXT actual: accepted%d restored%d error%u samples%lld stencil%u face%x enabled%d during%d\n",
                    result.accepted,result.restored,result.error,static_cast<long long>(result.samples),stencil_value,
                    active_stencil_face,extension_enabled,extension_seen_during_draw);
            ok=check(result.accepted&&result.restored&&!result.error&&result.samples==512&&stencil_value==91&&
                active_stencil_face==face&&extension_enabled==was_enabled&&!extension_seen_during_draw,
                "strict EXT back selector/query redirection retains independent core faces and extension enable")&&ok;
        }
        if(ok)std::printf("stencil strict drivers: color-only single/MRT/layered refs17/29, EXT back selector/state and real byte91 passed\n");
        return ok;
    }
    bool stencil_test()
    {
        Fixture x;if(!x.initialize())return false;x.caller_stencil();
        for(int op=1;op<=8;++op)
        {
            const int initial=(op==4||op==7)?255:(op==5||op==8)?0:128;
            const int expected=op==1?128:op==2?0:op==3?5:op==4?255:op==5?0:op==6?127:op==7?0:255;
            const auto state=x.depth(false,true,8,1,1,op,5);if(!check(state!=0,"stencil-enabled descriptor"))return false;
            x.clear(initial);if(!x.expect(state,0,65280,256,"stencil pass operation with caller front/back restoration")||!check(x.stencil()==expected,"actual stencil operation byte"))return false;
        }
        for(int compare=1;compare<=8;++compare)
        {
            const bool pass=compare==2||compare==4||compare==6||compare==8;
            const auto state=x.depth(false,true,compare,1,1,1,5);
            x.clear(7);if(!check(state!=0,"stencil compare descriptor")||!x.expect(state,0,pass?65280:0,pass?256:0,"all8 native stencil compares")||!check(x.stencil()==7,"compare keeps native stencil byte"))return false;
        }
        const auto fail=x.depth(false,true,1,3,1,1,42),zfail=x.depth(true,true,8,1,3,1,63,255,255,1);
        x.clear();if(!x.expect(fail,0,0,0,"stencil fail path")||!check(x.stencil()==42,"stencil fail replace"))return false;
        x.clear();if(!x.expect(zfail,0,0,0,"depth fail stencil path")||!check(x.stencil()==63,"stencil depth-fail replace"))return false;
        const auto masked=x.depth(false,true,8,1,1,3,5,255,15),read=x.depth(false,true,3,1,1,1,21,15,0),disabled=x.depth(false,false);
        x.clear(0xA0);if(!x.expect(masked,0,65280,256,"partial stencil write mask")||!check(x.stencil()==0xA5,"masked stencil bits preserved")||!x.expect(read,0,65280,256,"stencil compare read mask"))return false;
        x.clear();if(!x.expect(disabled,0,65280,256,"explicit disabled stencil overrides caller"))return false;
        x.clear();if(!x.expect(0,0,0,0,"handle0 retains rejecting caller stencil"))return false;
        for(int mode=1;mode<=4;++mode){x.clear();if(!x.expect(disabled,0,65280,mode==2?512:256,"advanced draw state restoration",mode))return false;}
        const auto both=x.depth(false,true,8,1,1,3,91);
        std::array<std::uint16_t,3>back_indices{0,2,1};
        if(!igpu_buffer_write(x.index,0,gm::wire::GMBuffer(back_indices.data(),sizeof(back_indices))))return false;
        x.clear();if(!x.expect(both,0,65280,256,"same stencil descriptor applies to back winding",1)||!check(x.stencil()==91,"back-face native replace"))return false;
        Array source_target{x.source},zero{0};
        if(!igpu_draw_to_render_targets(x.vertex,x.layout,4,0,3,source_target.view(),zero.view(),zero.view(),0,disabled,0,0))return false;
        const auto sampled=x.shader("#version 430\nuniform sampler2D igpu_tex;in vec2 uv;out vec4 c;void main(){c=texture(igpu_tex,uv);}",1);
        if(!check(sampled&&igpu_shader_bind(sampled,1),"sampled stencil pipeline"))return false;
        x.clear();const auto sample=x.measure([&]{return igpu_draw_sampled(x.vertex,x.layout,4,0,3,x.source,0,both,0,0);});
        if(!check(sample.accepted&&sample.restored&&!sample.error&&sample.pixel==65280&&sample.samples==256&&x.stencil()==91,"sampled stencil native byte and asymmetric restoration"))return false;
        if(!igpu_shader_bind(x.ps,1))return false;
        Array target{x.target};x.clear();const auto target_draw=x.measure([&]{return igpu_draw_to_render_targets(x.vertex,x.layout,4,0,3,target.view(),zero.view(),zero.view(),0,both,0,0);});
        if(!check(target_draw.accepted&&target_draw.restored&&!target_draw.error&&target_draw.pixel==65280&&target_draw.samples==256&&x.stencil()==91,"single target native stencil attachment retained"))return false;

        const auto mrt_shader=x.shader("#version 430\nlayout(location=0)out vec4 c;layout(location=1)out vec4 d;void main(){c=vec4(0,1,0,1);d=vec4(0,0,1,1);}",1);
        if(!check(mrt_shader&&igpu_shader_bind(mrt_shader,1),"MRT stencil state pipeline"))return false;
        Array multiple{x.target,x.source},layers{0,0},mips{0,0};x.clear();
        const auto colors=x.measure([&]{return igpu_draw_to_render_targets(x.vertex,x.layout,4,0,3,multiple.view(),layers.view(),mips.view(),0,disabled,0,0);});
        if(!check(colors.accepted&&colors.restored&&!colors.error&&colors.pixel==65280&&colors.samples==256&&igpu_texture_read(x.source,8,8,0,0)==16711680,
            "color-only MRT retains asymmetric caller stencil state and actual colors"))return false;
        if(!strict_stencil_driver(x,both,disabled))return false;
        const auto rejected_before=NativeState{};
        const bool invalid=igpu_draw(x.vertex,0,x.layout,4,0,3,1,-1,both,0,0);
        const bool error=!igpu_get_last_error().empty();const auto native_error=glGetError();
        if(!check(!invalid&&error&&!native_error&&rejected_before==NativeState{},"late invalid blend does not apply stencil state"))return false;
        std::printf("stencil states: all8 native ops/saturation/wrap, fail/zfail, masks, disabled/handle0 and advanced front/back restoration passed\n");return true;
    }
}
bool run_gl_raster_stencil_tests(){bool ok=raster_test();ok=stencil_test()&&ok;return ok;}


bool run_gl_raster_stencil_core_tests(std::uint64_t vertex,std::uint64_t layout,std::uint64_t target)
{
    auto&f=igpu_gl_fns();std::int32_t previous=0,viewport[4]{};GLint previous_rb=0;
    glGetIntegerv(0x8CA7,&previous_rb);
    if(!check(igpu::gl_color_target_begin(target,previous,viewport),"core state target"))return false;
    GLuint storage=0;f.GenRenderbuffers(1,&storage);f.BindRenderbuffer(0x8D41,storage);
    f.RenderbufferStorage(0x8D41,0x88F0,4,4);f.FramebufferRenderbuffer(0x8CA9,0x821A,0x8D41,storage);
    bool ok=storage&&f.CheckFramebufferStatus(0x8CA9)==0x8CD5;
    const auto solid=igpu_raster_state_create(0,0,false,true),wire=igpu_raster_state_create(0,1,false,true),clamp=igpu_raster_state_create(0,0,false,false);
    const auto stencil=igpu_depth_state_create(false,false,8,true,8,1,1,3,91,255,255);
    ok=solid&&wire&&clamp&&stencil&&ok;
    f.StencilFuncSeparate(GL_FRONT,GL_NEVER,17,0x3F);f.StencilFuncSeparate(GL_BACK,GL_GREATER,29,0xF0);
    f.StencilOpSeparate(GL_FRONT,GL_KEEP,GL_ZERO,GL_INCR);f.StencilOpSeparate(GL_BACK,GL_INVERT,GL_REPLACE,GL_DECR);
    f.StencilMaskSeparate(GL_FRONT,0x5A);f.StencilMaskSeparate(GL_BACK,0xA5);glEnable(GL_STENCIL_TEST);
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);glEnable(0x864F);
    for(auto raster:{solid,wire,clamp})
    {
        const NativeState before;
        const bool drawn=igpu_draw(vertex,0,layout,4,0,3,1,0,stencil,raster,0);
        const auto error=glGetError();
        ok=check(drawn&&!error&&before==NativeState{},"actual4.1core polygon/clamp/stencil state restored without error")&&ok;
    }
    GLint read=0,pack=0;glGetIntegerv(0x8CAA,&read);glGetIntegerv(0x88ED,&pack);
    const GLenum fields[]={GL_PACK_ALIGNMENT,0x0D02,0x0D03,0x0D04};GLint packing[4]{};
    for(int i=0;i<4;++i){glGetIntegerv(fields[i],&packing[i]);glPixelStorei(fields[i],i?0:1);}
    GLint draw=0;glGetIntegerv(0x8CA6,&draw);f.BindFramebuffer(0x8CA8,draw);f.BindBuffer(0x88EB,0);
    unsigned char value=0;glReadPixels(1,1,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&value);
    f.BindFramebuffer(0x8CA8,read);f.BindBuffer(0x88EB,pack);for(int i=0;i<4;++i)glPixelStorei(fields[i],packing[i]);
    ok=check(value==91,"actual4.1core stencil byte")&&ok;
    for(auto state:{solid,wire,clamp,stencil})if(state)igpu_state_release(state);
    f.FramebufferRenderbuffer(0x8CA9,0x821A,0x8D41,0);f.DeleteRenderbuffers(1,&storage);f.BindRenderbuffer(0x8D41,previous_rb);
    igpu::gl_color_target_end(previous,viewport);
    const auto error=glGetError();
    if(!check(ok&&!error,"core raster stencil cleanup"))return false;
    std::printf("raster stencil core: real4.1core polygon/depthclamp/asymmetric stencil restore and stencil byte91 passed\n");return true;
}
