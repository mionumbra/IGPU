#include "layered_texture_tests.h"
#include "IGPU_native.h"
#include "native/gl/igpu_gl_loader.h"
#include "native/gl/igpu_gl_texture.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/igpu_capabilities.h"
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
namespace
{
    using Handle=std::uint64_t;
    bool check(bool ok,const char*label){if(!ok)std::fprintf(stderr,"layered tests: %s: %s\n",label,igpu_get_last_error().c_str());return ok;}
    struct Array
    {
        std::vector<std::byte>bytes;
        explicit Array(const std::vector<Handle>&v):bytes(2+9*v.size()){const auto n=static_cast<std::uint16_t>(v.size());std::memcpy(bytes.data(),&n,2);for(std::size_t i=0;i<v.size();++i){bytes[2+9*i]=std::byte{12};std::memcpy(bytes.data()+3+9*i,&v[i],8);}}
        gm::wire::GMArrayView view()const{return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array,bytes.data()));}
    };
    struct State
    {
        static constexpr GLenum fields[]={0x8CA6,0x8CAA,0x8B8D,0x85B5,0x8894,0x88ED,0x88EF,GL_TEXTURE_BINDING_2D,0x806A,0x8C1D,0x8514,GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,0x84E0};
        std::array<GLint,16>v{};std::array<GLint,4>viewport{};
        State(){for(int i=0;i<16;++i)glGetIntegerv(fields[i],&v[i]);glGetIntegerv(GL_VIEWPORT,viewport.data());}
        bool operator==(const State&b)const{return v==b.v&&viewport==b.viewport;}
        void restore()const{auto&f=igpu_gl_fns();f.BindFramebuffer(0x8CA9,v[0]);f.BindFramebuffer(0x8CA8,v[1]);f.UseProgram(v[2]);f.BindVertexArray(v[3]);f.BindBuffer(0x8892,v[4]);f.BindBuffer(0x88EB,v[5]);f.BindBuffer(0x88EC,v[6]);f.ActiveTexture(v[15]);glBindTexture(GL_TEXTURE_2D,v[7]);glBindTexture(0x806F,v[8]);glBindTexture(0x8C1A,v[9]);glBindTexture(0x8513,v[10]);for(int i=11;i<15;++i)glPixelStorei(fields[i],v[i]);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);}
    };
    GLuint compile(const char*ps)
    {
        const char*vs="#version 430\nlayout(location=0)in vec2 p;void main(){gl_Position=vec4(p,0,1);}";const char*sources[]={vs,ps};auto&f=igpu_gl_fns();const auto p=f.CreateProgram();
        for(int i=0;i<2;++i){const auto s=f.CreateShader(i?0x8B30:0x8B31);f.ShaderSource(s,1,&sources[i],nullptr);f.CompileShader(s);GLint ok=0;f.GetShaderiv(s,0x8B81,&ok);if(ok)f.AttachShader(p,s);f.DeleteShader(s);if(!ok){f.DeleteProgram(p);return 0;}}
        f.LinkProgram(p);GLint ok=0;f.GetProgramiv(p,0x8B82,&ok);if(!ok){f.DeleteProgram(p);return 0;}return p;
    }
    struct Fixture
    {
        State original;std::vector<Handle>textures,shaders;Handle vertex=0,layout=0;GLuint program=0;
        Fixture(){glPushAttrib(GL_ALL_ATTRIB_BITS);}
        ~Fixture(){auto&f=igpu_gl_fns();igpu_shader_bind(0,2);for(auto s:shaders)igpu_shader_release(s);original.restore();if(program)f.DeleteProgram(program);if(layout)igpu_input_layout_release(layout);if(vertex)igpu_buffer_release(vertex);for(auto t:textures)igpu_texture_release(t);glPopAttrib();original.restore();}
        bool initialize()
        {
            Array u({1}),ty({2}),st({0});layout=igpu_input_layout_create(igpu_get_bound_shader(0),u.view(),ty.view(),st.view(),1,8,0);vertex=igpu_buffer_create(32,1,1,8);std::array<float,8>positions{-1,-1,1,-1,-1,1,1,1};
            program=compile("#version 430\nlayout(location=0)out vec4 a;layout(location=1)out vec4 b;layout(location=2)out vec4 c;layout(location=3)out vec4 d;void main(){a=vec4(1,0,0,1);b=vec4(0,1,0,1);c=vec4(0,0,1,1);d=vec4(1,1,1,1);}");auto&f=igpu_gl_fns();f.UseProgram(program);
            for(auto cap:{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_BLEND,GL_ALPHA_TEST,0x809D,0x8C89})glDisable(cap);GLint clips=0;glGetIntegerv(0x0D32,&clips);for(int i=0;i<clips;++i)glDisable(GL_CLIP_PLANE0+i);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            return check(program&&layout&&vertex&&igpu_buffer_write(vertex,0,gm::wire::GMBuffer(positions.data(),sizeof(positions)))&&!glGetError(),"quad fixture");
        }
        Handle texture(int kind,int size=4,int depth=4,int mips=0,bool storage=false,int format=6)
        {const auto h=igpu_texture_create(kind,size,size,kind==0?1:kind==3?6:depth,format,true,storage,mips);if(h)textures.push_back(h);return h;}
        bool draw(const std::vector<Handle>&targets,const std::vector<Handle>&layers,int mip=0)
        {Array t(targets),l(layers),m(std::vector<Handle>(targets.size(),mip));return igpu_draw_to_render_targets(vertex,layout,5,0,-1,t.view(),l.view(),m.view(),0,0,0,0);}
        template<class F>bool rejected(F action,const char*label){const State before;const bool accepted=action();const bool error=!igpu_get_last_error().empty();return check(!accepted&&error&&!glGetError()&&before==State{},label);}
    };
    bool layer_tests()
    {
        Fixture t;if(!t.initialize())return false;const std::array<std::int64_t,4>colors{255,65280,16711680,16777215};
        std::array<Handle,4>sources{};
        for(int kind:{1,2,3})
        {
            const State allocation;const auto h=t.texture(kind);sources[kind]=h;if(!check(h&&allocation==State{},"native type allocation preserves bindings"))return false;
            const int depth=kind==3?6:4;
            for(int layer=0;layer<depth;++layer){const State before;if(!check(igpu_texture_read(h,0,0,layer,0)==0&&igpu_get_last_error().empty()&&before==State{},"zeroed layer and read state"))return false;}
            const State before;const auto q=igpu_query_create(0);bool ok=q&&igpu_query_begin(q)&&t.draw({h,h,h,h},{0,1,2,3});if(!ok)std::fprintf(stderr,"layered MRT: %s\n",igpu_get_last_error().c_str());if(q){ok=igpu_query_end(q)&&ok;ok=igpu_query_result(q)==16&&ok;igpu_query_release(q);}
            if(!check(ok&&before==State{},"same texture distinct layers MRT and sixteen samples"))return false;
            for(int layer=0;layer<4;++layer)if(!check(igpu_texture_read(h,0,0,layer,0)==colors[layer],"layer output color"))return false;
            for(int face=4;face<depth;++face)if(!check(t.draw({h},{static_cast<Handle>(face)})&&igpu_texture_read(h,0,0,face,0)==255,"last cube faces"))return false;
            if(!check(t.draw({h,h},{0,1},1)&&igpu_texture_read(h,0,0,0,1)==255&&igpu_texture_read(h,0,0,1,1)==65280&&igpu_texture_read(h,0,0,2,0)==16711680,"selected layered mip and base isolation"))return false;
            if(!t.rejected([&]{return t.draw({h},{static_cast<Handle>(kind==1?2:depth)},1);},"mip layer boundary")||!t.rejected([&]{return t.draw({h,h},{1,1});},"duplicate exact subresource"))return false;
        }
        const auto plain=t.texture(0);const State mixed;
        if(!check(t.draw({sources[1],sources[2],sources[3],plain},{2,1,5,0})&&mixed==State{},"cross-kind MRT")||
            !check(igpu_texture_read(sources[1],0,0,2,0)==255&&igpu_texture_read(sources[2],0,0,1,0)==65280&&igpu_texture_read(sources[3],0,0,5,0)==16711680&&igpu_texture_read(plain,0,0,0,0)==16777215,"cross-kind literal pixels"))return false;
        std::printf("layered targets: volume slices, fixed array layers, all six cube faces, selected mips and cross-kind MRT passed\n");return true;
    }
    bool sampling_tests()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();const auto target=t.texture(0);bool ok=true;
        for(int kind:{1,2,3})
        {
            const auto source=t.texture(kind);if(!check(source&&t.draw({source},{0}),"sampling source layer draw"))return false;
            const char*type=kind==1?"sampler3D":kind==2?"sampler2DArray":"samplerCube";
            const std::string ps=std::string("#version 430\nuniform ")+type+" igpu_tex;out vec4 c;void main(){c=textureLod(igpu_tex,"+(kind==1?"vec3(.5,.5,.125)":kind==2?"vec3(.5,.5,0)":"vec3(1,0,0)")+",0);}";
            const auto program=compile(ps.c_str());f.UseProgram(program);std::int32_t saved=0,viewport[4]{};const bool begun=igpu::gl_color_target_begin(target,saved,viewport);const State before;
            const bool drawn=igpu_draw_sampled(t.vertex,t.layout,5,0,4,source,0,0,0,0);const auto message=igpu_get_last_error();const bool state=before==State{};const auto pixel=igpu_texture_read(target,0,0,0,0);
            if(begun)igpu::gl_color_target_end(saved,viewport);f.UseProgram(t.program);f.DeleteProgram(program);
            if(!drawn)std::fprintf(stderr,"typed sample kind%d: %s\n",kind,message.c_str());ok=check(drawn&&state&&pixel==255,"typed native-target sampling")&&ok;
        }
        return ok;
    }
    bool compute_tests()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();bool ok=true;
        for(int kind:{1,2,3})
        {
            const auto target=t.texture(kind,4,4,0,true);const char*type=kind==1?"image3D":kind==2?"image2DArray":"imageCube";
            const std::string code=std::string("#version 430\nlayout(local_size_x=1,local_size_y=1,local_size_z=1)in;layout(rgba8,binding=0)uniform ")+type+" dst;void main(){ivec3 p=ivec3(gl_GlobalInvocationID);imageStore(dst,p,vec4(float(p.z+1)/8.0,0,0,1));}";
            const auto shader=igpu_shader_compile(code.c_str(),"main",2,"glsl");if(shader)t.shaders.push_back(shader);const bool bound=shader&&igpu_shader_bind(shader,2);Array k({0}),targets({target}),zero({0});const State before;
            const bool dispatched=bound&&igpu_dispatch(4,4,kind==3?6:4,k.view(),targets.view(),zero.view(),zero.view());const auto message=igpu_get_last_error();const bool state=before==State{};bool pixels=true;
            for(int layer=0;layer<(kind==3?6:4);++layer){const auto value=igpu_texture_read(target,0,0,layer,0);const int expected=(255*(layer+1)+4)/8;pixels=pixels&&std::abs(static_cast<int>(value)-expected)<=1;}
            if(!dispatched)std::fprintf(stderr,"layered compute kind%d: %s\n",kind,message.c_str());ok=check(dispatched&&state&&pixels,"layered compute image writes every slice/face")&&ok;
            constexpr GLenum image_fields[]={0x8F3A,0x8F3B,0x8F3C,0x8F3D,0x8F3E,0x906E};std::array<GLint,6>original_image{},before_image{},after_image{};
            for(int i=0;i<6;++i)f.GetIntegeri(image_fields[i],0,&original_image[i]);
            const auto*texture=igpu::gl_find_texture(target);f.BindImageTexture(0,texture->texture,0,GL_FALSE,1,0x88B8,GL_RGBA8);
            for(int i=0;i<6;++i)f.GetIntegeri(image_fields[i],0,&before_image[i]);Array one({1});const State before_mip;
            bool mip_ok=igpu_dispatch(2,2,kind==1?2:kind==3?6:4,k.view(),targets.view(),zero.view(),one.view());
            for(int i=0;i<6;++i)f.GetIntegeri(image_fields[i],0,&after_image[i]);mip_ok=mip_ok&&before_mip==State{}&&before_image==after_image;
            for(int layer=0;layer<(kind==1?2:kind==3?6:4);++layer){const auto value=igpu_texture_read(target,0,0,layer,1);const int expected=(255*(layer+1)+4)/8;mip_ok=std::abs(static_cast<int>(value)-expected)<=1&&mip_ok;}
            f.BindImageTexture(0,original_image[0],original_image[1],original_image[2],original_image[3],original_image[4],original_image[5]);
            ok=check(mip_ok,"nonbase layered compute writes and exact six-field image restoration")&&ok;f.UseProgram(t.program);
        }
        if(ok)std::printf("layered compute: whole3D/array/cube image writes at base/nonbase mips and six-field caller image state passed\n");return ok;
    }
    bool w_address_tests()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();const auto source=t.texture(1),target=t.texture(0);
        if(!check(t.draw({source,source,source,source},{0,1,2,3}),"W addressing layers"))return false;
        bool ok=true;
        for(bool negative:{false,true})
        {
            const std::string ps=std::string("#version 430\nuniform sampler3D igpu_tex;out vec4 c;void main(){c=textureLod(igpu_tex,vec3(.5,.5,")+(negative?"-.125":"1.125")+"),0);}";
            const auto program=compile(ps.c_str());f.UseProgram(program);std::int32_t old=0,viewport[4]{};if(!igpu::gl_color_target_begin(target,old,viewport))return false;
            for(int w:{0,1,2})
            {
                const auto sampler=igpu_sampler_state_create(0,0,0,0,0,w,1,0,0,0.f,0.f,-1.f);
                const State before;const bool drawn=sampler&&igpu_draw_sampled(t.vertex,t.layout,5,0,4,source,0,0,0,sampler);const bool state=before==State{};
                const auto pixel=igpu_texture_read(target,0,0,0,0);const auto want=negative?(w==1?16777215:255):(w==1?255:16777215);
                const auto*texture=igpu::gl_find_texture(source);GLint saved=0,wrap=0;glGetIntegerv(texture->binding,&saved);glBindTexture(texture->target,texture->texture);glGetTexParameteriv(texture->target,0x8072,&wrap);glBindTexture(texture->target,saved);
                if(!drawn||pixel!=want)std::fprintf(stderr,"W address negative%d mode%d pixel%lld want%d\n",negative,w,static_cast<long long>(pixel),want);
                if(sampler)igpu_state_release(sampler);ok=check(drawn&&state&&pixel==want&&wrap==0x812F,"3D W clamp/repeat/mirror and parameter restore")&&ok;
            }
            igpu::gl_color_target_end(old,viewport);f.UseProgram(t.program);f.DeleteProgram(program);
        }
        if(ok)std::printf("layered sampling: positive/negative W clamp/repeat/mirror produce actual slice pixels and preserve parameters\n");return ok;
    }
    bool mip_generation_tests()
    {
        Fixture t;if(!t.initialize())return false;
        for(int kind:{1,2,3})
        {
            const auto h=t.texture(kind);const int layers=kind==3?6:4;for(int layer=0;layer<layers;++layer)if(!check(t.draw({h},{static_cast<Handle>(layer)}),"mip generation base slice"))return false;
            auto*texture=igpu::gl_find_texture(h);GLint saved=0;glGetIntegerv(texture->binding,&saved);glBindTexture(texture->target,texture->texture);glTexParameteri(texture->target,0x813C,1);glTexParameteri(texture->target,0x813D,1);glBindTexture(texture->target,saved);
            const State before;if(!check(igpu_texture_generate_mips(h)&&before==State{},"generate owned mip chain preserves native target binding"))return false;
            glBindTexture(texture->target,texture->texture);GLint base=0,max=0;glGetTexParameteriv(texture->target,0x813C,&base);glGetTexParameteriv(texture->target,0x813D,&max);glBindTexture(texture->target,saved);
            if(!check(base==1&&max==1,"native base/max restored after generation"))return false;
            for(int mip=1;mip<3;++mip)for(int layer=0;layer<(kind==1?4>>mip:layers);++layer)
                if(!check(igpu_texture_read(h,0,0,layer,mip)==255,"generated slice mip actual pixels"))return false;
        }
        // 3D mip count also follows depth when it is the largest dimension.
        const auto deep=t.texture(1,2,8);const auto*meta=igpu::gl_find_texture(deep);
        if(!check(meta&&meta->mips==4&&t.draw({deep},{0},3)&&igpu_texture_read(deep,0,0,0,3)==255,"volume full chain includes depth"))return false;
        std::printf("layered mips: generated volume/array/cube pixels, native base/max restoration and depth-driven full chain passed\n");return true;
    }
    bool full_sampling_tests()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();const auto target=t.texture(0);const std::array<std::int64_t,6>colors{255,65280,16711680,16777215,255,65280};
        const char*directions[]={"vec3(1,0,0)","vec3(-1,0,0)","vec3(0,1,0)","vec3(0,-1,0)","vec3(0,0,1)","vec3(0,0,-1)"};
        for(int kind:{1,2,3})
        {
            const auto h=t.texture(kind);if(!check(t.draw({h,h,h,h},{0,1,2,3})&&(kind!=3||t.draw({h,h},{4,5})),"all sample layers fixture"))return false;
            for(int layer=0;layer<(kind==3?6:4);++layer)
            {
                const std::string coordinate=kind==1?"vec3(.5,.5,"+std::to_string((layer+.5)/4)+")":kind==2?"vec3(.5,.5,"+std::to_string(layer)+")":directions[layer];
                const std::string ps=std::string("#version 430\nuniform ")+(kind==1?"sampler3D":kind==2?"sampler2DArray":"samplerCube")+" igpu_tex;out vec4 c;void main(){c=textureLod(igpu_tex,"+coordinate+",0);}";
                const auto program=compile(ps.c_str());f.UseProgram(program);std::int32_t old=0,vp[4]{};if(!igpu::gl_color_target_begin(target,old,vp))return false;
                f.ActiveTexture(0x84C3);const State before;const auto q=igpu_query_create(0);bool ok=q&&igpu_query_begin(q)&&igpu_draw_sampled(t.vertex,t.layout,5,0,4,h,0,0,0,0);
                if(q){ok=igpu_query_end(q)&&ok;ok=igpu_query_result(q)==16&&ok;igpu_query_release(q);}const bool state=before==State{};const auto pixel=igpu_texture_read(target,0,0,0,0);f.ActiveTexture(0x84C0);igpu::gl_color_target_end(old,vp);f.UseProgram(t.program);f.DeleteProgram(program);
                if(!check(ok&&state&&pixel==colors[layer],"typed slices/six face order, sixteen samples and nonzero active texture unit"))return false;
            }
            const auto wrong=compile("#version 430\nuniform sampler2D igpu_tex;out vec4 c;void main(){c=texture(igpu_tex,vec2(.5));}");f.UseProgram(wrong);
            if(!t.rejected([&]{return igpu_draw_sampled(t.vertex,t.layout,5,0,4,h,0,0,0,0);},"sampler kind mismatch"))return false;f.UseProgram(t.program);f.DeleteProgram(wrong);
        }
        std::printf("layered samples: every volume/array slice and six native cube directions, sampler mismatch, sixteen samples and active unit3 preserved\n");return true;
    }
    bool validation_tests()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();
        struct Invalid{int kind,w,h,d,mips;};
        const Invalid cases[]={{-1,4,4,4,0},{4,4,4,4,0},{0,4,4,2,1},{1,0,4,4,1},{1,4,4,0,1},{2,4,-1,4,1},{3,4,8,6,1},{3,4,4,5,1},{1,4,4,4,4},{2,4,4,4,4},{3,4,4,6,4},{1,4,4,(std::numeric_limits<int>::max)(),1},{2,4,4,(std::numeric_limits<int>::max)(),1}};
        for(const auto&v:cases)if(!t.rejected([&]{return igpu_texture_create(v.kind,v.w,v.h,v.d,6,true,false,v.mips)!=0;},"invalid kind/shape/depth/mip/native limit"))return false;
        for(int kind:{1,2,3})
        {
            const auto h=t.texture(kind);if(!check(h!=0,"guard texture"))return false;
            if(!t.rejected([&]{igpu_texture_read(h,0,0,0,(std::numeric_limits<int>::max)());return igpu_get_last_error().empty();},"huge mip read rejected before dimension loop")||
               !t.rejected([&]{igpu_texture_read(h,0,0,kind==3?6:4,0);return igpu_get_last_error().empty();},"layer/face read boundary")||
               !t.rejected([&]{return t.draw({h},{0},3);},"mip draw boundary"))return false;
        }
        const auto dead=t.texture(2);const auto native=igpu::gl_find_texture(dead)->texture;if(!check(igpu_texture_release(dead)&&!glIsTexture(native),"native layered texture deletion"))return false;t.textures.pop_back();
        if(!t.rejected([&]{return t.draw({dead},{0});},"released texture target"))return false;
        const auto plain=t.texture(0),cube=t.texture(3);
        const auto gates=[&](auto&entry,const char*label)
        {
            const auto saved=entry;entry=nullptr;const bool hidden=!igpu::supports(igpu::Capability::Texture3D)&&!igpu::supports(igpu::Capability::TextureArray)&&igpu::supports(igpu::Capability::TextureCubemap);
            bool ok=t.rejected([&]{return igpu_texture_create(1,4,4,4,6,true,false,1)!=0;},label)&&t.rejected([&]{return igpu_texture_create(2,4,4,4,6,true,false,1)!=0;},label);
            ok=t.draw({plain},{0})&&t.draw({cube},{0})&&ok;entry=saved;return check(hidden&&ok&&igpu::supports(igpu::Capability::Texture3D)&&igpu::supports(igpu::Capability::TextureArray),label);
        };
        if(!gates(f.TexImage3D,"missing TexImage3D keeps2D/cube")||!gates(f.FramebufferTextureLayer,"missing layered attachment keeps2D/cube"))return false;
        std::printf("layered validation: shape/native limits/mip/layer/handle guards, native deletion and optional gates retain2D/cube\n");return true;
    }
    bool read_state_tests()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();const auto h=t.texture(2);if(!t.draw({h},{2}))return false;
        const State original;GLuint pack=0;f.GenBuffers(1,&pack);f.BindBuffer(0x88EB,pack);std::array<unsigned char,256>sentinel{};sentinel.fill(0xA6);f.BufferData(0x88EB,sentinel.size(),sentinel.data(),0x88E4);
        glPixelStorei(GL_PACK_ALIGNMENT,8);glPixelStorei(GL_PACK_ROW_LENGTH,11);glPixelStorei(GL_PACK_SKIP_ROWS,2);glPixelStorei(GL_PACK_SKIP_PIXELS,3);const State before;
        bool ok=igpu_texture_read(h,1,1,2,0)==255&&before==State{};std::array<unsigned char,256>actual{};f.GetBufferSubData(0x88EB,0,actual.size(),actual.data());ok=actual==sentinel&&ok;original.restore();f.DeleteBuffers(1,&pack);
        if(!check(ok&&glGetError()==GL_NO_ERROR,"layered read preserves caller PACK settings/PBO bytes"))return false;
        std::printf("layered read: private READ FBO and caller PACK/PBO bytes preserved\n");return true;
    }
    bool capability_test(){return check(igpu::supports(igpu::Capability::Texture3D)&&igpu::supports(igpu::Capability::TextureArray)&&igpu::supports(igpu::Capability::TextureCubemap),"all implemented texture kinds advertised");}
}
bool run_gl_layered_texture_tests(){bool ok=layer_tests();ok=sampling_tests()&&ok;ok=compute_tests()&&ok;ok=capability_test()&&ok;ok=w_address_tests()&&ok;ok=mip_generation_tests()&&ok;ok=full_sampling_tests()&&ok;ok=validation_tests()&&ok;ok=read_state_tests()&&ok;return ok;}

bool run_gl_layered_texture_shutdown_tests()
{
    auto&f=igpu_gl_fns();f.UseProgram(0);const State original;const auto context=wglGetCurrentContext();
    std::array<Handle,3>handles{};std::array<GLuint,3>native{},framebuffers{};
    for(int i=0;i<3;++i){handles[i]=igpu_texture_create(i+1,4,4,i==2?6:4,6,true,true,0);auto*t=igpu::gl_find_texture(handles[i]);if(!check(t!=nullptr,"shutdown layered fixture"))return false;native[i]=t->texture;framebuffers[i]=t->framebuffer;}
    GLuint foreign[4]{};glGenTextures(4,foreign);const GLenum targets[]={GL_TEXTURE_2D,0x806F,0x8C1A,0x8513};for(int i=0;i<4;++i)glBindTexture(targets[i],foreign[i]);const State before;
    igpu_shutdown();bool ok=before==State{}&&!igpu_is_available()&&wglGetCurrentContext()==context;
    using IsFramebuffer=GLboolean(APIENTRY*)(GLuint);const auto exists=reinterpret_cast<IsFramebuffer>(wglGetProcAddress("glIsFramebuffer"));
    for(int i=0;i<3;++i)ok=!glIsTexture(native[i])&&!exists(framebuffers[i])&&igpu::gl_find_texture(handles[i])==nullptr&&ok;
    for(auto texture:foreign)ok=glIsTexture(texture)&&ok;
    original.restore();glDeleteTextures(4,foreign);
    if(!check(ok&&!glGetError(),"shutdown layered native cleanup/caller bindings/context"))return false;
    if(!check(igpu_bind_current()&&igpu::supports(igpu::Capability::Texture3D)&&igpu::supports(igpu::Capability::TextureArray)&&igpu::supports(igpu::Capability::TextureCubemap)&&!igpu_texture_release(handles[0]),"layered rebind capabilities and stale handles"))return false;
    std::printf("layered shutdown: volume/array/cube texture and FBO deletion, foreign bindings/context and fresh rebind passed\n");return true;
}
