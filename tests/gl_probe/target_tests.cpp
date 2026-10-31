#include "target_tests.h"
#include "IGPU_native.h"
#include "native/gl/igpu_gl_loader.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_targets.h"
#include "native/igpu_capabilities.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <array>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    using Handle=std::uint64_t;
    bool check(bool ok,const char*label)
    {if(!ok)std::fprintf(stderr,"target tests: %s: %s\n",label,igpu_get_last_error().c_str());return ok;}
    struct Array
    {
        std::vector<std::byte> bytes;
        explicit Array(const std::vector<Handle>&values):bytes(2+9*values.size())
        {
            const auto n=static_cast<std::uint16_t>(values.size());std::memcpy(bytes.data(),&n,2);
            for(std::size_t i=0;i<values.size();++i){bytes[2+9*i]=std::byte{12};std::memcpy(bytes.data()+3+9*i,&values[i],8);}
        }
        gm::wire::GMArrayView view()const{return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array,bytes.data()));}
    };
    struct Bindings
    {
        GLint draw=0,read=0,program=0,vao=0,array=0;std::array<GLint,4> viewport{};
        Bindings(){glGetIntegerv(0x8CA6,&draw);glGetIntegerv(0x8CAA,&read);glGetIntegerv(0x8B8D,&program);glGetIntegerv(0x85B5,&vao);glGetIntegerv(0x8894,&array);glGetIntegerv(GL_VIEWPORT,viewport.data());}
        bool operator==(const Bindings&b)const{return draw==b.draw&&read==b.read&&program==b.program&&vao==b.vao&&array==b.array&&viewport==b.viewport;}
        void restore()const{auto&f=igpu_gl_fns();f.BindFramebuffer(0x8CA9,draw);f.BindFramebuffer(0x8CA8,read);f.UseProgram(program);f.BindVertexArray(vao);f.BindBuffer(0x8892,array);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);}
    };
    struct Colors
    {
        using Boolean=void(APIENTRY*)(GLenum,GLuint,GLboolean*);
        using Integer=void(APIENTRY*)(GLenum,GLuint,GLint*);
        Integer integer=reinterpret_cast<Integer>(wglGetProcAddress("glGetIntegeri_v"));
        using Enabled=GLboolean(APIENTRY*)(GLenum,GLuint);
        using Toggle=void(APIENTRY*)(GLenum,GLuint);
        using Mask=void(APIENTRY*)(GLuint,GLboolean,GLboolean,GLboolean,GLboolean);
        using Func=void(APIENTRY*)(GLuint,GLenum,GLenum,GLenum,GLenum);
        using Equation=void(APIENTRY*)(GLuint,GLenum,GLenum);
        Boolean get=reinterpret_cast<Boolean>(wglGetProcAddress("glGetBooleani_v"));
        Enabled enabled=reinterpret_cast<Enabled>(wglGetProcAddress("glIsEnabledi"));
        Toggle enable=reinterpret_cast<Toggle>(wglGetProcAddress("glEnablei")),disable=reinterpret_cast<Toggle>(wglGetProcAddress("glDisablei"));
        Mask mask=reinterpret_cast<Mask>(wglGetProcAddress("glColorMaski"));
        Func func=reinterpret_cast<Func>(wglGetProcAddress("glBlendFuncSeparatei"));
        Equation equation=reinterpret_cast<Equation>(wglGetProcAddress("glBlendEquationSeparatei"));
        struct Slot
        {
            GLboolean blend=0;std::array<GLboolean,4> mask{};std::array<GLint,6> functions{};
            bool operator==(const Slot&b)const{return blend==b.blend&&mask==b.mask&&functions==b.functions;}
        };
        static constexpr GLenum fields[]={0x80C9,0x80C8,0x80CB,0x80CA,0x8009,0x883D};
        std::vector<Slot> slots;
        Colors(){GLint n=0;glGetIntegerv(0x8824,&n);slots.resize(n);for(GLuint i=0;i<slots.size();++i){auto&v=slots[i];v.blend=enabled(GL_BLEND,i);get(GL_COLOR_WRITEMASK,i,v.mask.data());for(int k=0;k<6;++k)integer(fields[k],i,&v.functions[k]);}}
        bool operator==(const Colors&b)const{return slots==b.slots;}
        void restore()const{for(GLuint i=0;i<slots.size();++i){const auto&v=slots[i];(v.blend?enable:disable)(GL_BLEND,i);mask(i,v.mask[0],v.mask[1],v.mask[2],v.mask[3]);func(i,v.functions[0],v.functions[1],v.functions[2],v.functions[3]);equation(i,v.functions[4],v.functions[5]);}}
        void different()const{for(GLuint i=0;i<slots.size();++i){(i%2?enable:disable)(GL_BLEND,i);mask(i,i%2==0,i%3==0,i%4==0,i%2!=0);func(i,i%2?GL_SRC_ALPHA:GL_ONE,i%2?GL_ONE_MINUS_SRC_ALPHA:GL_ZERO,GL_ONE,GL_ZERO);equation(i,i%2?0x800A:0x8006,i%2?0x800B:0x8006);}}
    };
    struct State
    {
        Bindings bindings;Colors colors;
        std::vector<GLint> routes;
        std::array<GLint,33> attachments{};
        State()
        {
            GLint n=0;glGetIntegerv(0x8824,&n);routes.resize(n+1);for(int i=0;i<n;++i)glGetIntegerv(0x8825+i,&routes[i]);glGetIntegerv(GL_READ_BUFFER,&routes[n]);
            using GetAttachment=void(APIENTRY*)(GLenum,GLenum,GLenum,GLint*);const auto get=reinterpret_cast<GetAttachment>(wglGetProcAddress("glGetFramebufferAttachmentParameteriv"));
            int k=0;for(auto target:{GLenum{0x8CA9},GLenum{0x8CA8}})for(int i=0;i<4;++i)
            {
                const auto bound=target==0x8CA9?bindings.draw:bindings.read;
                if(bound){get(target,0x8CE0+i,0x8CD0,&attachments[k]);if(attachments[k])get(target,0x8CE0+i,0x8CD1,&attachments[k+1]);if(attachments[k]==GL_TEXTURE)get(target,0x8CE0+i,0x8CD2,&attachments[k+2]);}
                k+=4;
            }
            glGetIntegerv(0x88ED,&attachments[32]);
        }
        bool operator==(const State&b)const{return bindings==b.bindings&&colors==b.colors&&routes==b.routes&&attachments==b.attachments;}
    };
    IgpuGlGenBuffers forward_buffer=nullptr;GLuint generated_buffer=0;
    void APIENTRY observe_buffer(GLsizei n,GLuint*out){forward_buffer(n,out);if(n)generated_buffer=out[0];}
    IgpuGlGenFramebuffers forward_framebuffers=nullptr;IgpuGlDeleteFramebuffers forward_delete=nullptr;
    GLuint generated_fbo=0;int generated_count=0,deleted_count=0;
    void APIENTRY observe_fbo(GLsizei n,GLuint*out){forward_framebuffers(n,out);generated_count+=n;if(n)generated_fbo=out[0];}
    void APIENTRY observe_delete(GLsizei n,const GLuint*names){deleted_count+=n;forward_delete(n,names);}
    GLenum APIENTRY incomplete(GLenum){return 0x8CD6;}
    void APIENTRY no_framebuffer(GLsizei n,GLuint*out){for(GLsizei i=0;i<n;++i)out[i]=0;}
    struct Fixture
    {
        Bindings original;Colors original_colors;
        std::vector<Handle> textures;
        Handle vertex=0,layout=0;
        GLuint program=0,native_vertex=0;
        Fixture(){glPushAttrib(GL_ALL_ATTRIB_BITS);}
        ~Fixture(){auto&f=igpu_gl_fns();original.restore();if(program)f.DeleteProgram(program);if(layout)igpu_input_layout_release(layout);if(vertex)igpu_buffer_release(vertex);for(auto t:textures)igpu_texture_release(t);glPopAttrib();original.restore();original_colors.restore();}
        Handle texture(int width=4,int height=4,int format=6,int mips=1,bool target=true)
        {const auto h=igpu_texture_create(0,width,height,1,format,target,false,mips);if(h)textures.push_back(h);return h;}
        bool initialize()
        {
            Array usage({1}),type({2}),step({0});layout=igpu_input_layout_create(igpu_get_bound_shader(0),usage.view(),type.view(),step.view(),1,8,0);
            auto&loader=igpu_gl_fns();forward_buffer=loader.GenBuffers;loader.GenBuffers=observe_buffer;
            vertex=igpu_buffer_create(32,1,1,8);native_vertex=generated_buffer;loader.GenBuffers=forward_buffer;std::array<float,8>positions{-1,-1,1,-1,-1,1,1,1};
            if(!check(layout&&vertex&&igpu_buffer_write(vertex,0,gm::wire::GMBuffer(positions.data(),sizeof(positions))),"quad fixture"))return false;
            const char*sources[]={"#version 330\nlayout(location=0)in vec2 in_pos;void main(){gl_Position=vec4(in_pos,0,1);}",
                "#version 330\nlayout(location=0)out vec4 a;layout(location=1)out vec4 b;layout(location=2)out vec4 c;layout(location=3)out vec4 d;void main(){a=vec4(1,0,0,1);b=vec4(0,1,0,1);c=vec4(0,0,1,1);d=vec4(1,1,1,1);}"};
            auto&f=igpu_gl_fns();program=f.CreateProgram();
            for(int i=0;i<2;++i){const auto sh=f.CreateShader(i?0x8B30:0x8B31);f.ShaderSource(sh,1,&sources[i],nullptr);f.CompileShader(sh);GLint ok=0;f.GetShaderiv(sh,0x8B81,&ok);if(ok)f.AttachShader(program,sh);f.DeleteShader(sh);if(!check(ok!=0,"output shader compile"))return false;}
            f.LinkProgram(program);GLint linked=0;f.GetProgramiv(program,0x8B82,&linked);if(!check(linked!=0,"output program link"))return false;
            f.UseProgram(program);for(auto cap:{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_BLEND,GL_ALPHA_TEST,0x809D,0x8C89})glDisable(cap);
            GLint clips=0;glGetIntegerv(0x0D32,&clips);for(int i=0;i<clips;++i)glDisable(GL_CLIP_PLANE0+i);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            return check(glGetError()==GL_NO_ERROR,"clean fixture state");
        }
        template<class F>bool rejected(F action,const char*label)
        {const State before;const bool accepted=action();const auto error=igpu_get_last_error();const auto gl_error=glGetError();return check(!accepted&&!error.empty()&&!gl_error&&before==State{},label);}
        bool draw(const std::vector<Handle>&targets,const std::vector<Handle>&mips={},const std::vector<Handle>&layers={},std::int64_t first=0,std::int64_t count=4,std::int64_t blend=0)
        {Array t(targets),m(mips.empty()?std::vector<Handle>(targets.size(),0):mips),l(layers.empty()?std::vector<Handle>(targets.size(),0):layers);return igpu_draw_to_render_targets(vertex,layout,5,first,count,t.view(),l.view(),m.view(),blend,0,0,0);}
    };
    bool output_tests()
    {
        Fixture t;if(!t.initialize())return false;
        std::vector<Handle>targets;for(int i=0;i<4;++i)targets.push_back(t.texture());
        const std::array<std::int64_t,4>colors{255,65280,16711680,16777215};
        for(int n=2;n<=4;++n)
        {
            const Bindings before;const auto q=igpu_query_create(0);bool ok=q&&igpu_query_begin(q);
            ok=t.draw(std::vector<Handle>(targets.begin(),targets.begin()+n))&&ok;
            if(!ok)std::fprintf(stderr,"target output draw: %s\n",igpu_get_last_error().c_str());
            if(q){ok=igpu_query_end(q)&&ok;ok=igpu_query_result(q)==16&&ok;igpu_query_release(q);}
            if(!check(ok&&before==Bindings{},"multiple outputs, sixteen geometry samples and caller bindings"))return false;
            for(int i=0;i<n;++i)if(!check(igpu_texture_read(targets[i],1,1,0,0)==colors[i],"literal output color"))return false;
        }
        std::printf("targets: two/three/four outputs, literal RGB/white and sixteen geometry samples passed\n");return true;
    }
    bool read_color(Handle handle,int mip,const std::array<float,4>&expected)
    {
        igpu::GlColorTarget target;if(!igpu::gl_resolve_color_target(handle,mip,target))return false;
        auto&f=igpu_gl_fns();GLint read=0,pack=0;glGetIntegerv(0x8CAA,&read);glGetIntegerv(0x88ED,&pack);
        constexpr GLenum fields[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_SWAP_BYTES};GLint old[5]{};
        for(int i=0;i<5;++i){glGetIntegerv(fields[i],&old[i]);glPixelStorei(fields[i],i?0:1);}f.BindBuffer(0x88EB,0);
        GLuint framebuffer=0;f.GenFramebuffers(1,&framebuffer);f.BindFramebuffer(0x8CA8,framebuffer);f.FramebufferTexture2D(0x8CA8,0x8CE0,GL_TEXTURE_2D,target.texture,mip);glReadBuffer(0x8CE0);
        std::array<float,4>actual{};glReadPixels(1,1,1,1,GL_RGBA,GL_FLOAT,actual.data());
        f.BindFramebuffer(0x8CA8,read);f.DeleteFramebuffers(1,&framebuffer);f.BindBuffer(0x88EB,pack);for(int i=0;i<5;++i)glPixelStorei(fields[i],old[i]);
        bool ok=glGetError()==GL_NO_ERROR;for(int i=0;i<4;++i)ok=ok&&std::fabs(actual[i]-expected[i])<0.002;
        if(!ok)std::fprintf(stderr,"target pixel: %.4f %.4f %.4f %.4f\n",actual[0],actual[1],actual[2],actual[3]);return check(ok,"native float output pixel");
    }
    bool mip_and_format_tests()
    {
        Fixture t;if(!t.initialize())return false;
        const auto a=t.texture(8,8,6,0),b=t.texture(),c=t.texture(16,16,6,0),d=t.texture();
        if(!check(t.draw({a,b,c,d},{1,0,2,0}, {},0,-1),"matching selected mips and inferred count")||
            !check(igpu_texture_read(a,1,1,0,1)==255&&igpu_texture_read(c,1,1,0,2)==16711680,"selected mip pixels")||
            !check(igpu_texture_read(a,1,1,0,0)==0&&igpu_texture_read(c,1,1,0,0)==0,"unselected mip isolation"))return false;
        for(int format:{6,9,10,11,12,13,14,15})
        {
            const auto x=t.texture(4,4,format),y=t.texture();
            if(!check(x&&y&&t.draw({x,y}),"mixed color formats")||!read_color(x,0,{1,0,0,1})||!read_color(y,0,{0,1,0,1}))return false;
        }
        if(!check(t.draw({b},{},{},0,-1),"single-target inferred count"))return false;
        std::printf("targets: selected mip dimensions/isolation, eight mixed formats and inferred vertex counts passed\n");return true;
    }
    bool validation_tests()
    {
        Fixture t;if(!t.initialize())return false;
        const auto a=t.texture(),b=t.texture(),small=t.texture(2,4),sample=t.texture(4,4,6,1,false);
        const auto huge=(std::numeric_limits<std::int64_t>::max)();
        if(!t.rejected([&]{return t.draw({});},"empty targets")||!t.rejected([&]{return t.draw({a,b,a,b,a});},"five targets")||
            !t.rejected([&]{return t.draw({a,a});},"duplicate subresource")||!t.rejected([&]{return t.draw({a,small});},"dimension mismatch")||
            !t.rejected([&]{return t.draw({a,sample});},"non-render-target texture")||!t.rejected([&]{return t.draw({a,0});},"zero texture")||
            !t.rejected([&]{return t.draw({a,Handle{0x7FFFFFFF}});},"unknown texture")||!t.rejected([&]{return t.draw({a,b},{0,1});},"invalid mip")||
            !t.rejected([&]{return t.draw({a,b},{0,0},{0,1});},"unsupported layer")||!t.rejected([&]{return t.draw({a,b},{0,0},{0});},"mismatched arrays")||
            !t.rejected([&]{return t.draw({a,b},{},{},huge,4);},"huge first vertex")||!t.rejected([&]{return t.draw({a,b},{},{},1,huge);},"huge vertex count")||
            !t.rejected([&]{return t.draw({a,b},{},{},-1,4);},"negative first vertex")||!t.rejected([&]{return t.draw({a,b},{},{},0,0);},"zero vertex count")||
            !t.rejected([&]{return t.draw({a,b},{},{},0,4,-1);},"late invalid blend state"))return false;
        auto&f=igpu_gl_fns();Array targets({a,b}),zero({0,0});
        for(double number:{0.5,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),9223372036854775808.0})
        {
            auto malformed=zero;malformed.bytes[2]=std::byte{9};std::memcpy(malformed.bytes.data()+3,&number,8);
            if(!t.rejected([&]{return igpu_draw_to_render_targets(t.vertex,t.layout,5,0,4,targets.view(),zero.view(),malformed.view(),0,0,0,0);},"fractional/nonfinite/out-of-range mip"))return false;
        }
        Array real_targets=targets;
        for(std::size_t i=0;i<2;++i){const double h=static_cast<double>(i?b:a);real_targets.bytes[2+9*i]=std::byte{9};std::memcpy(real_targets.bytes.data()+3+9*i,&h,8);}
        if(!check(igpu_draw_to_render_targets(t.vertex,t.layout,5,0,4,real_targets.view(),zero.view(),zero.view(),0,0,0,0),"integral double target handles"))return false;
        auto huge_target=targets;const auto max_handle=(std::numeric_limits<std::uint64_t>::max)();std::memcpy(huge_target.bytes.data()+3,&max_handle,8);
        if(!t.rejected([&]{return igpu_draw_to_render_targets(t.vertex,t.layout,5,0,4,huge_target.view(),zero.view(),zero.view(),0,0,0,0);},"unsigned handle above signed range"))return false;
        auto wrong_type=zero;wrong_type.bytes[2]=std::byte{10};wrong_type.bytes.erase(wrong_type.bytes.begin()+4,wrong_type.bytes.begin()+11);
        if(!t.rejected([&]{return igpu_draw_to_render_targets(t.vertex,t.layout,5,0,4,targets.view(),wrong_type.view(),zero.view(),0,0,0,0);},"non-numeric layer"))return false;
        const auto dead=t.texture();if(!check(igpu_texture_release(dead),"released fixture"))return false;t.textures.pop_back();
        if(!t.rejected([&]{return t.draw({a,dead});},"released target"))return false;
        using Map=void*(APIENTRY*)(GLenum,GLenum);using Unmap=GLboolean(APIENTRY*)(GLenum);auto map=reinterpret_cast<Map>(wglGetProcAddress("glMapBuffer"));auto unmap=reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        GLint old=0;glGetIntegerv(0x8894,&old);f.BindBuffer(0x8892,t.native_vertex);void*address=map(0x8892,0x88B8);
        const bool guarded=address&&t.rejected([&]{return t.draw({a,b});},"mapped vertex rejected");if(address)unmap(0x8892);f.BindBuffer(0x8892,old);if(!check(guarded,"mapping guard"))return false;
        f.UseProgram(0);const bool missing=t.rejected([&]{return t.draw({a,b});},"missing program");f.UseProgram(t.program);if(!missing)return false;
        std::printf("targets: malformed arrays/numbers, roles/subresources, range overflow, mapped inputs and late state/program failures preserve caller state\n");return true;
    }
    bool indexed_state_tests()
    {
        Fixture t;if(!t.initialize())return false;
        const auto a=t.texture(),b=t.texture(),c=t.texture(),d=t.texture();
        Colors{}.different();const Colors before_allocation;
        const auto extra=t.texture();const bool allocation=check(extra&&before_allocation==Colors{},"texture allocation preserves all indexed write masks");
        before_allocation.restore();const State before;
        const auto blend=igpu_blend_state_create(false,2,1,1,2,1,1,true,true,true,true);
        const bool drawn=t.draw({a,b,c,d},{},{},0,4,blend);const auto error=glGetError();const bool restored=before==State{};
        if(blend)igpu_state_release(blend);
        const bool pixels=read_color(a,0,{1,0,0,1})&&read_color(b,0,{0,1,0,1})&&read_color(c,0,{0,0,1,1})&&read_color(d,0,{1,1,1,1});
        const bool blend_ok=check(drawn&&!error&&restored&&pixels,"explicit blend preserves every indexed slot including native slots above four");
        if(!allocation||!blend_ok)return false;
        // Without an explicit descriptor, indexed caller masks remain active for their output.
        Colors neutral;for(GLuint i=0;i<neutral.slots.size();++i){neutral.disable(GL_BLEND,i);neutral.mask(i,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);}neutral.mask(0,GL_FALSE,GL_TRUE,GL_TRUE,GL_TRUE);
        const auto x=t.texture(),y=t.texture();neutral.mask(0,GL_FALSE,GL_TRUE,GL_TRUE,GL_TRUE);const State caller;
        if(!check(t.draw({x,y})&&caller==State{},"caller indexed masks remain active")||!read_color(x,0,{0,0,0,1})||!read_color(y,0,{0,1,0,1}))return false;
        const auto masked_x=t.texture(),masked_y=t.texture();const auto masked=igpu_blend_state_create(false,2,1,1,2,1,1,false,true,true,true);
        const State masked_before;const bool masked_draw=t.draw({masked_x,masked_y},{},{},0,4,masked);const bool masked_restore=masked_before==State{};if(masked)igpu_state_release(masked);
        if(!check(masked_draw&&masked_restore,"descriptor color write mask and indexed restore")||!read_color(masked_x,0,{0,0,0,1})||!read_color(masked_y,0,{0,1,0,1}))return false;
        const auto keep_x=t.texture(),keep_y=t.texture();const auto keep=igpu_blend_state_create(true,1,2,1,1,2,1,true,true,true,true);
        const State keep_before;const bool keep_draw=t.draw({keep_x,keep_y},{},{},0,4,keep);const bool keep_restore=keep_before==State{};if(keep)igpu_state_release(keep);
        if(!check(keep_draw&&keep_restore,"enabled blend keeps destination independently for both outputs")||!read_color(keep_x,0,{0,0,0,1})||!read_color(keep_y,0,{0,0,0,1}))return false;
        std::printf("targets: native indexed blend enables/functions/equations and masks survive allocation, descriptors and ordinary caller-controlled draws\n");return true;
    }
    bool framebuffer_tests()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();
        const auto a=t.texture(),b=t.texture(),c=t.texture(),d=t.texture();igpu::GlColorTarget ca,cb,cc,cd;
        if(!igpu::gl_resolve_color_target(a,0,ca)||!igpu::gl_resolve_color_target(b,0,cb)||!igpu::gl_resolve_color_target(c,0,cc)||!igpu::gl_resolve_color_target(d,0,cd))return false;
        GLuint caller[2]{};f.GenFramebuffers(2,caller);f.BindFramebuffer(0x8CA9,caller[0]);
        f.FramebufferTexture2D(0x8CA9,0x8CE0,GL_TEXTURE_2D,ca.texture,0);f.FramebufferTexture2D(0x8CA9,0x8CE1,GL_TEXTURE_2D,cb.texture,0);
        const GLenum reverse[]={0x8CE1,0x8CE0};f.DrawBuffers(2,reverse);f.BindFramebuffer(0x8CA8,caller[1]);f.FramebufferTexture2D(0x8CA8,0x8CE1,GL_TEXTURE_2D,cd.texture,0);glReadBuffer(0x8CE1);glViewport(3,5,17,19);
        const State before;forward_framebuffers=f.GenFramebuffers;forward_delete=f.DeleteFramebuffers;f.GenFramebuffers=observe_fbo;f.DeleteFramebuffers=observe_delete;generated_count=deleted_count=0;
        const bool success=t.draw({a,b,c,d});const bool restored=before==State{};
        using IsFramebuffer=GLboolean(APIENTRY*)(GLuint);const auto exists=reinterpret_cast<IsFramebuffer>(wglGetProcAddress("glIsFramebuffer"));
        bool cleanup=generated_count==1&&deleted_count==1&&!exists(generated_fbo);
        const bool failed=t.rejected([&]{return t.draw({a,b},{},{},0,4,-1);},"private framebuffer late failure restoration");cleanup=cleanup&&generated_count==2&&deleted_count==2&&!exists(generated_fbo);
        const auto status=f.CheckFramebufferStatus;f.CheckFramebufferStatus=incomplete;
        const bool incomplete_ok=t.rejected([&]{return t.draw({a,b});},"incomplete temporary framebuffer restores state");f.CheckFramebufferStatus=status;
        cleanup=cleanup&&generated_count==3&&deleted_count==3&&!exists(generated_fbo);
        f.GenFramebuffers=no_framebuffer;
        const bool allocation_failed=t.rejected([&]{return t.draw({a,b});},"framebuffer allocation failure restores state");
        f.GenFramebuffers=forward_framebuffers;f.DeleteFramebuffers=forward_delete;
        const auto extra=t.texture();const bool allocation=before==State{};
        before.bindings.restore();f.BindFramebuffer(0x8CA9,0);f.BindFramebuffer(0x8CA8,0);f.DeleteFramebuffers(2,caller);
        if(!check(success&&restored&&failed&&incomplete_ok&&allocation_failed&&cleanup,"distinct caller framebuffer attachments/routing restored and temporary names deleted")||!check(extra&&allocation,"texture initialization preserves distinct read/draw framebuffers"))return false;
        std::printf("targets: distinct caller READ/DRAW framebuffer attachments/routing, viewport and temporary-name cleanup passed\n");return true;
    }
    bool capability_test()
    {
        if(!check(igpu::supports(igpu::Capability::MultipleRenderTargets),"multiple render targets advertised after complete implementation"))return false;
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();const auto a=t.texture(),b=t.texture();
        const auto test=[&](auto&function,const char*label)
        {
            const auto saved=function;function=nullptr;
            const bool hidden=!igpu::supports(igpu::Capability::MultipleRenderTargets);
            const bool rejected=t.rejected([&]{return t.draw({a,b});},label);
            const bool single=t.draw({a});function=saved;
            return check(hidden&&rejected&&single&&igpu::supports(igpu::Capability::MultipleRenderTargets),label);
        };
        if(!test(f.DrawBuffers,"missing DrawBuffers")||!test(f.GetBooleani,"missing indexed mask query")||!test(f.GetIntegeri,"missing indexed blend query")||
           !test(f.IsEnabledi,"missing indexed enable query")||!test(f.Enablei,"missing indexed enable")||!test(f.Disablei,"missing indexed disable")||
           !test(f.ColorMaski,"missing indexed mask setter")||!test(f.BlendFuncSeparatei,"missing indexed blend functions")||!test(f.BlendEquationSeparatei,"missing indexed blend equations"))return false;
        std::printf("targets: nine optional entrypoint gates hide incomplete MRT and preserve existing single-target draws\n");return true;
    }

    GLuint sampling_program(bool comparison)
    {
        const char*vs="#version 330\nlayout(location=0)in vec2 p;void main(){gl_Position=vec4(p,0,1);}";
        const char*ps=comparison?"#version 330\nuniform sampler2DShadow igpu_tex;out vec4 c;void main(){c=vec4(texture(igpu_tex,vec3(.5,.5,.5)),0,0,1);}":
            "#version 330\nuniform sampler2D igpu_tex;out vec4 c;void main(){c=texture(igpu_tex,vec2(.5));}";
        const char*sources[]={vs,ps};auto&f=igpu_gl_fns();const auto program=f.CreateProgram();
        for(int i=0;i<2;++i){const auto sh=f.CreateShader(i?0x8B30:0x8B31);f.ShaderSource(sh,1,&sources[i],nullptr);f.CompileShader(sh);GLint ok=0;f.GetShaderiv(sh,0x8B81,&ok);if(ok)f.AttachShader(program,sh);f.DeleteShader(sh);if(!ok){f.DeleteProgram(program);return 0;}}
        f.LinkProgram(program);GLint ok=0;f.GetProgramiv(program,0x8B82,&ok);if(!ok){f.DeleteProgram(program);return 0;}return program;
    }
    bool subsequent_draw_tests()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();const auto a=t.texture(),b=t.texture(),target=t.texture(),floating=t.texture(4,4,10);
        if(!check(t.draw({a,b})&&t.draw({floating,b}),"MRT before subsequent draws"))return false;
        std::int32_t previous=0,viewport[4]{};if(!igpu::gl_color_target_begin(target,previous,viewport))return false;
        const bool ordinary=igpu_draw(t.vertex,0,t.layout,5,0,4,1,0,0,0,0);const bool pixel=igpu_texture_read(target,1,1,0,0)==255;
        bool ok=ordinary&&pixel;
        for(bool comparison:{false,true})
        {
            const auto program=sampling_program(comparison);f.UseProgram(program);
            const auto sampler=comparison?igpu_sampler_state_create(0,0,0,0,0,0,1,0,4,0.f,0.f,-1.f):0;
            const State before;const auto query=igpu_query_create(0);bool drawn=program&&query&&igpu_query_begin(query);
            drawn=igpu_draw_sampled(t.vertex,t.layout,5,0,4,comparison?floating:a,0,0,0,sampler)&&drawn;
            if(!drawn)std::fprintf(stderr,"subsequent draw: %s\n",igpu_get_last_error().c_str());
            if(query){drawn=igpu_query_end(query)&&drawn;drawn=igpu_query_result(query)==16&&drawn;igpu_query_release(query);}
            const bool state=before==State{};const bool color=igpu_texture_read(target,1,1,0,0)==255;
            if(sampler)igpu_state_release(sampler);f.UseProgram(t.program);if(program)f.DeleteProgram(program);
            ok=check(drawn&&state&&color,comparison?"comparison sampling after MRT excludes mirror samples and preserves state":"ordinary sampling after MRT")&&ok;
        }
        igpu::gl_color_target_end(previous,viewport);
        if(!check(ok&&glGetError()==GL_NO_ERROR,"ordinary and comparison draws after MRT"))return false;
        std::printf("targets: ordinary draw, ordinary sampling and comparison mirrors after MRT retain real pixels, sixteen samples and caller state\n");return true;
    }

}
bool run_gl_target_tests(){bool ok=output_tests();ok=mip_and_format_tests()&&ok;ok=validation_tests()&&ok;ok=indexed_state_tests()&&ok;ok=framebuffer_tests()&&ok;ok=capability_test()&&ok;ok=subsequent_draw_tests()&&ok;return ok;}
