#include "graphics_stage_tests.h"
#include "IGPU_native.h"
#include "native/gl/igpu_gl_loader.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_uniform.h"
#include "native/gl/igpu_gl_compute.h"
#include "native/igpu_draw.h"
#include "native/gl/igpu_gl_graphics.h"
#include "native/igpu_capabilities.h"
#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <limits>
namespace
{
    using Handle=std::uint64_t;
    bool check(bool ok,const char*label){if(!ok)std::fprintf(stderr,"graphics stages: %s: %s\n",label,igpu_get_last_error().c_str());return ok;}
    struct Array
    {
        std::vector<std::byte>bytes;explicit Array(const std::vector<Handle>&v):bytes(2+9*v.size()){const auto n=static_cast<std::uint16_t>(v.size());std::memcpy(bytes.data(),&n,2);for(std::size_t i=0;i<v.size();++i){bytes[2+9*i]=std::byte{12};std::memcpy(bytes.data()+3+9*i,&v[i],8);}}
        gm::wire::GMArrayView view()const{return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array,bytes.data()));}
    };
    struct State
    {
        static constexpr GLenum fields[]={0x8B8D,0x8CA6,0x8CAA,0x85B5,0x8894,0x8E72};std::array<GLint,6>v{};
        State(){for(int i=0;i<6;++i)glGetIntegerv(fields[i],&v[i]);}
        bool operator==(const State&b)const{return v==b.v;}
    };
    const char*vs="#version 430\nlayout(location=0)in vec2 in_pos;void main(){gl_Position=vec4(in_pos,0,1);}";
    const char*ps="#version 430\nout vec4 c;void main(){c=vec4(0,1,0,1);}";
    const char*gs="#version 430\nlayout(points)in;layout(triangle_strip,max_vertices=4)out;void main(){gl_Position=vec4(-1,-1,0,1);EmitVertex();gl_Position=vec4(1,-1,0,1);EmitVertex();gl_Position=vec4(-1,1,0,1);EmitVertex();gl_Position=vec4(1,1,0,1);EmitVertex();EndPrimitive();}";
    const char*tc="#version 430\nlayout(vertices=3)out;void main(){gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;if(gl_InvocationID==0){gl_TessLevelOuter[0]=1;gl_TessLevelOuter[1]=1;gl_TessLevelOuter[2]=1;gl_TessLevelInner[0]=1;}}";
    const char*te="#version 430\nlayout(triangles,equal_spacing,cw)in;void main(){gl_Position=gl_TessCoord.x*gl_in[0].gl_Position+gl_TessCoord.y*gl_in[1].gl_Position+gl_TessCoord.z*gl_in[2].gl_Position;}";
    struct Fixture
    {
        std::array<std::int64_t,6>old{};std::vector<Handle>shaders;Handle vertex=0,layout=0,target=0,vshader=0,pshader=0;GLint original_program=0,read=0,patch=3;std::int32_t draw=0,viewport[4]{};bool begun=false;
        Fixture(){glGetIntegerv(0x8E72,&patch);glGetIntegerv(0x8B8D,&original_program);glGetIntegerv(0x8CAA,&read);for(int i=0;i<6;++i)old[i]=igpu_get_bound_shader(i);glPushAttrib(GL_ALL_ATTRIB_BITS);}
        ~Fixture(){auto&f=igpu_gl_fns();igpu_shader_bind(0,3);igpu_shader_bind(0,4);igpu_shader_bind(0,5);for(int i:{0,1,3,4,5})igpu_shader_bind(old[i],i);for(auto h:shaders)igpu_shader_release(h);if(begun)igpu::gl_color_target_end(draw,viewport);f.BindFramebuffer(0x8CA8,read);if(layout)igpu_input_layout_release(layout);if(vertex)igpu_buffer_release(vertex);if(target)igpu_texture_release(target);glPopAttrib();if(f.PatchParameteri)f.PatchParameteri(0x8E72,patch);using IsProgram=GLboolean(APIENTRY*)(GLuint);const auto exists=reinterpret_cast<IsProgram>(wglGetProcAddress("glIsProgram"));if(!original_program||exists(original_program))f.UseProgram(original_program);}
        Handle shader(const char*source,int stage){const auto h=igpu_shader_compile(source,"main",stage,"glsl");if(h)shaders.push_back(h);else std::fprintf(stderr,"stage%d compile: %s\n",stage,igpu_get_last_error().c_str());return h;}
        bool initialize()
        {
            vshader=shader(vs,0);pshader=shader(ps,1);Array u({1}),ty({2}),st({0});layout=igpu_input_layout_create(vshader,u.view(),ty.view(),st.view(),1,8,0);vertex=igpu_buffer_create(24,1,1,8);std::array<float,6>p{-1,-1,3,-1,-1,3};target=igpu_texture_create(0,4,4,1,6,true,false,1);
            bool ok=vshader&&pshader&&layout&&vertex&&target&&igpu_buffer_write(vertex,0,gm::wire::GMBuffer(p.data(),sizeof(p)))&&igpu_shader_bind(vshader,0)&&igpu_shader_bind(pshader,1);
            begun=igpu::gl_color_target_begin(target,draw,viewport);for(auto cap:{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_BLEND,GL_ALPHA_TEST,0x809D,0x8C89})glDisable(cap);GLint clips=0;glGetIntegerv(0x0D32,&clips);for(int i=0;i<clips;++i)glDisable(GL_CLIP_PLANE0+i);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);return check(ok&&begun&&!glGetError(),"fixture compile/bind");
        }
        bool pixels(bool patch=false,int first=0,int count=3)
        {const State before;const auto q=igpu_query_create(0);bool ok=q&&igpu_query_begin(q);ok=(patch?igpu_draw_patch(vertex,layout,3,first,count,0,0,0,0):igpu_draw(vertex,0,layout,count==1?1:4,first,count,1,0,0,0,0))&&ok;if(!ok)std::fprintf(stderr,"stage draw: %s\n",igpu_get_last_error().c_str());if(q){ok=igpu_query_end(q)&&ok;ok=igpu_query_result(q)==16&&ok;igpu_query_release(q);}return check(ok&&before==State{}&&igpu_texture_read(target,1,1,0,0)==65280&&!glGetError(),"real sixteen samples, green pixel and caller state");}
        template<class F>bool rejected(F action,const char*label){const State before;const bool accepted=action(),error=!igpu_get_last_error().empty();const auto native_error=glGetError();return check(!accepted&&error&&native_error==GL_NO_ERROR&&before==State{},label);}
    };
    bool geometry_test(){Fixture t;if(!t.initialize())return false;const auto shader=t.shader(gs,3);if(!check(shader&&igpu_shader_bind(shader,3),"geometry compile/bind"))return false;if(!t.pixels(false,0,1))return false;if(!check(igpu_shader_bind(0,3)&&t.pixels(),"ordinary pipeline after geometry unbind"))return false;std::printf("graphics stages: public geometry point expansion renders sixteen samples and ordinary drawing resumes\n");return true;}
    bool tessellation_test(){Fixture t;if(!t.initialize())return false;const auto hull=t.shader(tc,4),domain=t.shader(te,5);if(!check(hull&&domain,"tess compilation"))return false;if(!check(igpu_shader_bind(hull,4),"hull-first staging")||!t.pixels()||!t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,3,0,0,0,0);},"incomplete tess pair rejects patch"))return false;if(!check(igpu_shader_bind(domain,5),"domain completes pair")||!t.pixels(true))return false;if(!check(igpu_shader_bind(0,4),"unbind hull")||!t.pixels()||!t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,3,0,0,0,0);},"missing hull rejects patch"))return false;if(!check(igpu_shader_bind(hull,4),"hull completes domain-first pair")||!t.pixels(true,0,-1))return false;std::printf("graphics stages: tess pair staging in either order, native patches and incomplete-pair ordinary pipeline passed\n");return true;}
    bool uniform_stage_test()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();GLint bindings=0;glGetIntegerv(0x8A2F,&bindings);if(bindings<32){std::printf("graphics stage UBO: native bindings below32, new stage namespace unavailable\n");return true;}
        const std::string source="#version 430\nlayout(points)in;layout(triangle_strip,max_vertices=4)out;layout(std140,binding=24)uniform GData{uvec4 v;};void main(){float x=float(v.x)-7.0;gl_Position=vec4(-1+x,-1,0,1);EmitVertex();gl_Position=vec4(1+x,-1,0,1);EmitVertex();gl_Position=vec4(-1+x,1,0,1);EmitVertex();gl_Position=vec4(1+x,1,0,1);EmitVertex();EndPrimitive();}";
        const auto shader=t.shader(source.c_str(),3);const auto blocks=igpu_shader_reflect(shader);const auto reflection_error=igpu_get_last_error();if(blocks.size()!=1)std::fprintf(stderr,"geometry reflection: %s blocks%zu\n",reflection_error.c_str(),blocks.size());const auto buffer=igpu_buffer_create(16,1,4,0);Array values({7,10,13,16});
        bool ok=check(shader&&buffer&&blocks.size()==1&&blocks[0].name=="GData"&&blocks[0].slot==0,"geometry uniform native reflection");
        ok=check(ok&&igpu_uniform_write(buffer,shader,"GData","v",values.view())&&igpu_uniform_bind(buffer,3,0)&&igpu_shader_bind(shader,3),"geometry uniform write/bind")&&ok;
        if(ok)ok=t.pixels(false,0,1)&&ok;
        igpu::GlUniformBuffer native;if(ok)ok=igpu::gl_uniform_buffer(buffer,native,"test");
        using Map=void*(APIENTRY*)(GLenum,GLenum);using Unmap=GLboolean(APIENTRY*)(GLenum);const auto map=reinterpret_cast<Map>(wglGetProcAddress("glMapBuffer"));const auto unmap=reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        if(ok){GLint old=0;glGetIntegerv(0x8894,&old);f.BindBuffer(0x8892,native.name);void*p=map(0x8892,0x88B8);const bool guarded=p&&t.rejected([&]{return igpu_draw(t.vertex,0,t.layout,1,0,1,1,0,0,0,0);},"late mapped geometry UBO");if(p)unmap(0x8892);f.BindBuffer(0x8892,old);ok=guarded&&ok;}
        igpu_uniform_bind(0,3,0);if(buffer)igpu_buffer_release(buffer);
        if(ok)std::printf("graphics stage UBO: geometry native binding24 reflection/write/pixels and late mapping guard passed\n");return ok;
    }
    bool tessellation_uniform_test()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();GLint bindings=0;glGetIntegerv(0x8A2F,&bindings);if(bindings<48){std::printf("tess UBO: native slots below48, reserved namespace unavailable\n");return true;}
        std::string hull_source=tc;hull_source.insert(hull_source.find("void main()"),"layout(std140,binding=32)uniform HData{uvec4 v;} hdata;");hull_source.replace(hull_source.find("gl_TessLevelOuter[0]=1"),std::strlen("gl_TessLevelOuter[0]=1"),"gl_TessLevelOuter[0]=float(hdata.v.x)/7.0");
        std::string domain_source=te;domain_source.insert(domain_source.find("void main()"),"layout(std140,binding=40)uniform DData{uvec4 v;} ddata;");domain_source.insert(domain_source.rfind('}'),"gl_Position.x+=float(ddata.v.x)-7.0;");
        const auto hull=t.shader(hull_source.c_str(),4),domain=t.shader(domain_source.c_str(),5);std::array<Handle,2> buffers{};bool ok=hull&&domain;
        Array values({7,10,13,16});
        for(int i=0;i<2&&ok;++i){const auto shader=i?domain:hull;const auto blocks=igpu_shader_reflect(shader);const auto message=igpu_get_last_error();if(blocks.size()!=1)std::fprintf(stderr,"tess reflection%d: %s\n",i,message.c_str());buffers[i]=igpu_buffer_create(16,1,4,0);ok=check(blocks.size()==1&&blocks[0].slot==0&&buffers[i]&&igpu_uniform_write(buffers[i],shader,i?"DData":"HData","v",values.view())&&igpu_uniform_bind(buffers[i],i+4,0),"tess uniform reflection/write/bind")&&ok;}
        if(ok)ok=check(igpu_shader_bind(hull,4),"hull uniform bind")&&check(igpu_shader_bind(domain,5),"domain uniform bind")&&t.pixels(true);
        using Map=void*(APIENTRY*)(GLenum,GLenum);using Unmap=GLboolean(APIENTRY*)(GLenum);const auto map=reinterpret_cast<Map>(wglGetProcAddress("glMapBuffer"));const auto unmap=reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        for(int i=0;i<2&&ok;++i){igpu::GlUniformBuffer native;ok=igpu::gl_uniform_buffer(buffers[i],native,"test")&&ok;GLint old=0;glGetIntegerv(0x8894,&old);f.BindBuffer(0x8892,native.name);void*p=map(0x8892,0x88B8);ok=p&&t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,3,0,0,0,0);},"late mapped tessellation UBO")&&ok;if(p)unmap(0x8892);f.BindBuffer(0x8892,old);}
        for(int i=0;i<2;++i){igpu_uniform_bind(0,i+4,0);if(buffers[i])igpu_buffer_release(buffers[i]);}
        if(ok)std::printf("graphics stage UBO: hull32/domain40 reflection/write/native patch pixels and late mappings passed\n");return check(ok,"tess uniform complete path");
    }
    bool feedback_test()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();const auto capture=t.shader("#version 430\nlayout(location=0)in vec2 in_pos;out float captured;void main(){captured=in_pos.x;gl_Position=vec4(in_pos,0,1);}",0),geometry=t.shader(gs,3);
        if(!check(capture&&geometry&&igpu_shader_bind(capture,0),"capture stage fixture"))return false;
        using Varyings=void(APIENTRY*)(GLuint,GLsizei,const char*const*,GLenum);using Begin=void(APIENTRY*)(GLenum);using Control=void(APIENTRY*)();
        const auto varyings=reinterpret_cast<Varyings>(wglGetProcAddress("glTransformFeedbackVaryings"));const auto begin=reinterpret_cast<Begin>(wglGetProcAddress("glBeginTransformFeedback"));const auto end=reinterpret_cast<Control>(wglGetProcAddress("glEndTransformFeedback"));const auto pause=reinterpret_cast<Control>(wglGetProcAddress("glPauseTransformFeedback"));
        GLint program=0,generic=0,indexed=0;glGetIntegerv(0x8B8D,&program);glGetIntegerv(0x8C8F,&generic);f.GetIntegeri(0x8C8F,0,&indexed);const char*name="captured";varyings(program,1,&name,0x8C8C);f.LinkProgram(program);GLint linked=0;f.GetProgramiv(program,0x8B82,&linked);f.UseProgram(program);
        GLuint output=0;f.GenBuffers(1,&output);f.BindBuffer(0x8C8E,output);f.BufferData(0x8C8E,64,nullptr,0x88E4);f.BindBufferBase(0x8C8E,0,output);begin(GL_TRIANGLES);
        bool ok=linked&&glGetError()==GL_NO_ERROR&&t.rejected([&]{return igpu_shader_bind(geometry,3);},"active transform feedback rejects program replacement")&&igpu_get_bound_shader(3)==0;
        ok=t.rejected([&]{return igpu_shader_release(capture);},"active transform feedback retains bound shader lifetime")&&ok;
        GLboolean active=GL_FALSE,paused=GL_FALSE;glGetBooleanv(0x8E24,&active);glGetBooleanv(0x8E23,&paused);ok=active&&!paused&&ok;
        pause();const bool accepted=igpu_shader_bind(geometry,3);end();f.BindBufferBase(0x8C8E,0,indexed);f.BindBuffer(0x8C8E,generic);f.DeleteBuffers(1,&output);
        if(!check(ok&&accepted&&!glGetError(),"feedback switch rejection, paused replacement and clean capture end"))return false;
        if(!t.pixels(false,0,1))return false;
        std::printf("graphics feedback: active capture rejects bind/release without mutation; paused replacement and real geometry pixels pass\n");return true;
    }
    IgpuGlCreateProgram original_create=nullptr;
    GLuint APIENTRY no_program(){return 0;}
    bool transaction_test()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();std::string output=gs;const auto body=output.find("void main(){");output.insert(body,"layout(location=0)out vec3 mismatch;");output.insert(output.find("void main(){")+12,"mismatch=vec3(0,1,0);");const auto good=t.shader(output.c_str(),3);if(!check(good&&igpu_shader_bind(good,3),"good transaction fixture"))return false;
        const auto bad=t.shader("#version 430\nlayout(points)in;layout(triangle_strip,max_vertices=4)out;layout(location=0)out vec3 mismatch;void main(){mismatch=vec3(1);gl_Position=vec4(0);EmitVertex();EndPrimitive();}",3);
        const auto bad_ps=t.shader("#version 430\nlayout(location=0)in vec4 mismatch;out vec4 c;void main(){c=mismatch;}",1);
        // The current geometry has no matching output; the incompatible pixel link must retain the good program.
        const auto preserved=igpu_get_bound_shader(1);const State before;
        bool ok=bad&&bad_ps&&t.rejected([&]{return igpu_shader_bind(bad_ps,1);},"link mismatch rollback")&&igpu_get_bound_shader(1)==preserved&&before==State{};
        GLint deleted=0;f.GetProgramiv(before.v[0],0x8B80,&deleted);ok=!deleted&&ok;
        if(ok)ok=t.pixels(false,0,1)&&ok;
        original_create=f.CreateProgram;f.CreateProgram=no_program;const bool allocation=t.rejected([&]{return igpu_shader_bind(0,3);},"graphics program allocation failure rollback");f.CreateProgram=original_create;
        ok=allocation&&igpu_get_bound_shader(3)==static_cast<std::int64_t>(good)&&ok;
        if(!check(ok,"transaction preserves program lifetime, bindings and pixels"))return false;
        std::printf("graphics transaction: native interface/link and allocation failures preserve last valid bindings/program/pixels\n");return true;
    }
    bool executable_stage_test()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();
        const auto hull=t.shader(tc,4),domain=t.shader(te,5);
        if(!check(hull&&domain&&igpu_shader_bind(hull,4)&&igpu_shader_bind(domain,5),"executable stage pair"))return false;
        GLint owned=0,count=0;glGetIntegerv(0x8B8D,&owned);f.GetProgramiv(owned,0x8B85,&count);
        std::vector<GLuint>attached(count);GLsizei returned=0;f.GetAttachedShaders(owned,count,&returned,attached.data());
        using Detach=void(APIENTRY*)(GLuint,GLuint);const auto detach=reinterpret_cast<Detach>(wglGetProcAddress("glDetachShader"));
        std::vector<GLuint>tess;
        for(auto shader:attached){GLint type=0;f.GetShaderiv(shader,0x8B4F,&type);if(type==0x8E88||type==0x8E87)tess.push_back(shader);}
        if(!check(detach&&tess.size()==2,"native tess attachments"))return false;
        for(auto shader:tess)detach(owned,shader);
        const bool retained=t.pixels(true);
        for(auto shader:tess)f.AttachShader(owned,shader);
        const auto external=f.CreateProgram();const char*sources[]={vs,ps};
        for(int i=0;i<2;++i){const auto shader=f.CreateShader(i?0x8B30:0x8B31);f.ShaderSource(shader,1,&sources[i],nullptr);f.CompileShader(shader);f.AttachShader(external,shader);f.DeleteShader(shader);}
        f.LinkProgram(external);for(auto shader:tess)f.AttachShader(external,shader);f.UseProgram(external);
        const bool pending=t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,3,0,0,0,0);},"post-link tess attachments must not masquerade as executable stages");
        f.LinkProgram(external);for(auto shader:tess)detach(external,shader);
        const bool foreign=t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,3,0,0,0,0);},"external tess executable has explicit unsupported ownership policy");
        f.UseProgram(owned);f.DeleteProgram(external);
        if(!check(retained&&pending&&foreign&&t.pixels(true),"linked executable metadata survives detach and rejects external ownership"))return false;
        std::printf("graphics executable stages: owned linked tess survives detach; external pending attachments and detached executable are rejected safely\n");return true;
    }
    bool primitive_stage_test()
    {
        bool all=true;
        for(bool tessellation:{false,true})
        {
            Fixture t;if(!t.initialize())return false;
            const auto geometry=tessellation?0:t.shader(gs,3),hull=tessellation?t.shader(tc,4):0,domain=tessellation?t.shader(te,5):0;
            if(!check(tessellation?(hull&&domain&&igpu_shader_bind(hull,4)&&igpu_shader_bind(domain,5)):(geometry&&igpu_shader_bind(geometry,3)),"primitive stage fixture"))return false;
            struct Buffers{Handle index=0,args=0;~Buffers(){if(index)igpu_buffer_release(index);if(args)igpu_buffer_release(args);}}buffers;
            buffers.index=igpu_buffer_create(6,1,2,0);buffers.args=igpu_buffer_create(64,0,16,0);
            std::array<std::uint16_t,3>indices{0,1,2};std::array<std::uint32_t,16>commands{3,1,0,0,0,0,0,0,3,1,0,0,0};
            if(!check(buffers.index&&buffers.args&&igpu_buffer_write(buffers.index,0,gm::wire::GMBuffer(indices.data(),sizeof(indices)))&&igpu_buffer_write(buffers.args,0,gm::wire::GMBuffer(commands.data(),sizeof(commands))),"primitive range resources"))return false;
            const char*expected=tessellation?"patch":"geometry";
            auto rejected=[&](auto action,const char*label){const bool ok=t.rejected(action,label);return check(ok&&igpu_get_last_error().find(expected)!=std::string::npos,label);};
            bool ok=rejected([&]{return igpu_draw(t.vertex,0,t.layout,4,0,3,1,0,0,0,0);},"ordinary incompatible stage primitive");
            ok=rejected([&]{return igpu_draw_indexed(t.vertex,t.layout,buffers.index,4,0,3,0,0,0,0);},"indexed incompatible stage primitive")&&ok;
            ok=rejected([&]{return igpu_draw(t.vertex,0,t.layout,4,0,3,2,0,0,0,0);},"instanced incompatible stage primitive")&&ok;
            ok=rejected([&]{return igpu_draw_indirect(t.vertex,0,t.layout,4,buffers.args,0,0,0,0,0);},"indirect incompatible stage primitive")&&ok;
            ok=rejected([&]{return igpu_draw_indexed_indirect(t.vertex,0,t.layout,buffers.index,4,buffers.args,32,0,0,0,0);},"indexed indirect incompatible stage primitive")&&ok;
            Array targets({t.target}),layers({0}),mips({0});
            ok=rejected([&]{return igpu_draw_to_render_targets(t.vertex,t.layout,4,0,3,targets.view(),layers.view(),mips.view(),0,0,0,0);},"target incompatible stage primitive")&&ok;
            const auto sampled=t.shader("#version 430\nuniform sampler2D igpu_tex;out vec4 c;void main(){c=texture(igpu_tex,vec2(.5));}",1);
            const auto source=igpu_texture_create(0,2,2,1,6,false,false,1);
            ok=sampled&&source&&igpu_shader_bind(sampled,1)&&ok;
            ok=rejected([&]{return igpu_draw_sampled(t.vertex,t.layout,4,0,3,source,0,0,0,0);},"sampled incompatible stage primitive")&&ok;
            if(source)igpu_texture_release(source);ok=igpu_shader_bind(t.pshader,1)&&ok;
            const bool valid=tessellation?t.pixels(true):t.pixels(false,0,1);all=check(ok&&valid,"all submission families reject incompatible stages, valid drawing resumes")&&all;
        }
        if(all)std::printf("graphics primitive stages: direct/indexed/instanced/indirect/targets/sampled reject tess ordinary modes and geometry input mismatches\n");return all;
    }

    bool patch_guard_test()
    {
        Fixture t;if(!t.initialize())return false;auto&f=igpu_gl_fns();const auto hull=t.shader(tc,4),domain=t.shader(te,5);if(!check(hull&&domain&&igpu_shader_bind(hull,4)&&igpu_shader_bind(domain,5),"patch guard pair"))return false;
        f.PatchParameteri(0x8E72,7);if(!t.pixels(true))return false;
        const auto huge=(std::numeric_limits<std::int64_t>::max)();
        for(int points:{0,33})if(!t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,points,0,3,0,0,0,0);},"invalid control point bound"))return false;
        if(!t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,2,0,0,0,0);},"fractional patch count")||!t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,huge,3,0,0,0,0);},"huge patch first")||!t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,1,huge,0,0,0,0);},"huge patch count")||!t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,3,-1,0,0,0);},"late patch state failure"))return false;
        const auto raw=f.CreateProgram();const char*sources[]={vs,ps};for(int i=0;i<2;++i){const auto sh=f.CreateShader(i?0x8B30:0x8B31);f.ShaderSource(sh,1,&sources[i],nullptr);f.CompileShader(sh);f.AttachShader(raw,sh);f.DeleteShader(sh);}f.LinkProgram(raw);GLint real=0;glGetIntegerv(0x8B8D,&real);f.UseProgram(raw);
        const bool foreign=t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,3,0,0,0,0);},"foreign ordinary program rejects patch before draw");f.UseProgram(real);f.DeleteProgram(raw);if(!foreign)return false;
        const auto patch=f.PatchParameteri;f.PatchParameteri=nullptr;const bool hidden=!igpu::supports(igpu::Capability::ShaderStageTessellation)&&igpu::supports(igpu::Capability::ShaderStageGeometry);const bool guarded=t.rejected([&]{return igpu_draw_patch(t.vertex,t.layout,3,0,3,0,0,0,0);},"missing PatchParameteri rejects before mutation");f.PatchParameteri=patch;
        if(!check(hidden&&guarded&&igpu::supports(igpu::Capability::ShaderStageTessellation),"honest optional tessellation gate"))return false;
        for(int stage:{6,7})if(!t.rejected([&]{return igpu_shader_compile(vs,"main",stage,"glsl")!=0;},"reserved shader stage"))return false;
        if(!t.rejected([&]{return igpu_shader_bind(hull,3);},"shader handle stage mismatch"))return false;
        std::printf("graphics patches: callerPATCH_VERTICES7, invalid counts/ranges/states, optional capability and reserved/mismatched stage guards passed\n");return true;
    }

    struct StorageRange {
        GLint name = 0;
        std::int64_t offset = 0, size = 0;
        void capture(GLuint slot)
        {
            auto& f = igpu_gl_fns();
            f.GetIntegeri(0x90D3, slot, &name);
            f.GetInteger64i(0x90D4, slot, &offset);
            f.GetInteger64i(0x90D5, slot, &size);
        }
        void restore(GLuint slot) const
        {
            auto& f = igpu_gl_fns();
            if (name && size > 0)
                f.BindBufferRange(0x90D2, slot, name, offset, size);
            else
                f.BindBufferBase(0x90D2, slot, name);
        }
        bool operator==(const StorageRange& b) const { return name == b.name && offset == b.offset && size == b.size; }
    };
    struct StorageCaller {
        GLint generic = 0, alignment = 0;
        GLuint foreign = 0;
        std::vector<GLuint> slots;
        std::vector<StorageRange> original, seeded;
        std::vector<unsigned char> sentinel;
        explicit StorageCaller(const std::vector<GLuint>& indices) : slots(indices)
        {
            auto& f = igpu_gl_fns();
            glGetIntegerv(0x90D3, &generic);
            glGetIntegerv(0x90DF, &alignment);
            for (auto slot : slots) {
                StorageRange range;
                range.capture(slot);
                original.push_back(range);
            }
            f.GenBuffers(1, &foreign);
            f.BindBuffer(0x90D2, foreign);
            sentinel.assign(3 * alignment + 128, 91);
            f.BufferData(0x90D2, sentinel.size(), sentinel.data(), 0x88E4);
            for (std::size_t i = 0; i < slots.size(); ++i) {
                f.BindBufferRange(0x90D2, slots[i], foreign, (i % 3) * alignment, 16 + (i % 2) * 16);
                StorageRange range;
                range.capture(slots[i]);
                seeded.push_back(range);
            }
            f.BindBuffer(0x90D2, foreign);
        }
        ~StorageCaller()
        {
            auto& f = igpu_gl_fns();
            for (std::size_t i = 0; i < slots.size(); ++i) {
                igpu_storage_bind(0, (slots[i] - 8) / 8, (slots[i] - 8) % 8);
                original[i].restore(slots[i]);
            }
            f.BindBuffer(0x90D2, generic);
            if (foreign)
                f.DeleteBuffers(1, &foreign);
        }
        bool unchanged() const
        {
            GLint actual = 0;
            glGetIntegerv(0x90D3, &actual);
            bool ok = actual == static_cast<GLint>(foreign);
            for (std::size_t i = 0; i < slots.size(); ++i) {
                StorageRange range;
                range.capture(slots[i]);
                ok = range == seeded[i] && ok;
            }
            return ok;
        }
        bool bytes() const
        {
            auto& f = igpu_gl_fns();
            GLint previous = 0;
            glGetIntegerv(0x90D3, &previous);
            f.BindBuffer(0x90D2, foreign);
            std::vector<unsigned char> actual(sentinel.size());
            f.GetBufferSubData(0x90D2, 0, actual.size(), actual.data());
            f.BindBuffer(0x90D2, previous);
            return actual == sentinel;
        }
    };
    struct StorageBuffers {
        std::vector<Handle> handles;
        ~StorageBuffers()
        {
            for (auto h : handles)
                igpu_buffer_release(h);
        }
        Handle create(std::uint32_t value = 7)
        {
            auto h = igpu_buffer_create(16, 1, 8, 4);
            if (h)
                handles.push_back(h);
            std::array<std::uint32_t, 4> values {value, value, value, value};
            return h && igpu_buffer_write(h, 0, gm::wire::GMBuffer(values.data(), sizeof(values))) ? h : 0;
        }
    };
    template<class Submit>
    bool storage_rejected(Fixture& t, Submit submit, const char* label)
    {
        const auto count = igpu::draw_count(), failures = igpu::draw_restore_failures();
        GLint generic = 0;
        glGetIntegerv(0x90D3, &generic);
        GLint maximum = 0;
        glGetIntegerv(0x90DD, &maximum);
        const auto observed = static_cast<std::size_t>(maximum < 56 ? maximum : 56);
        std::vector<StorageRange> indexed_before(observed), indexed_after(observed);
        for (GLuint i = 0; i < observed; ++i)
            indexed_before[i].capture(i);
        const bool rejected = t.rejected(submit, label);
        GLint after = 0;
        glGetIntegerv(0x90D3, &after);
        for (GLuint i = 0; i < observed; ++i)
            indexed_after[i].capture(i);
        return check(
            rejected && count == igpu::draw_count() && failures == igpu::draw_restore_failures() && generic == after &&
                indexed_before == indexed_after,
            label
        );
    }
    bool storage_stage_test()
    {
        if (!igpu::gl_read_storage_supported())
            return true;
        Fixture t;
        if (!t.initialize())
            return false;
        auto& f = igpu_gl_fns();
        GLint maximum = 0;
        glGetIntegerv(0x90DD, &maximum);
        const GLenum fields[] = {0x90D7, 0x90D8, 0x90D9};
        GLint limits[3] {};
        for (int i = 0; i < 3; ++i)
            glGetIntegerv(fields[i], &limits[i]);
        std::vector<GLuint> slots;
        for (int stage = 3; stage <= 5; ++stage)
            for (int slot : {0, 7})
                if (8 + stage * 8 + slot < maximum)
                    slots.push_back(8 + stage * 8 + slot);
        StorageCaller caller(slots);
        StorageBuffers buffers;
        const auto data = buffers.create(), replacement = buffers.create();
        if (!check(data && replacement, "graphics storage fixtures"))
            return false;
        bool ok = storage_rejected(t, [&] { return igpu_storage_bind(data, -1, 0); }, "negative storage stage rejects");
        ok = storage_rejected(
                 t, [&] { return igpu_storage_bind(data, 3, -1); }, "negative graphics storage slot rejects"
             )
            && ok;
        ok = storage_rejected(
                 t, [&] { return igpu_storage_bind(data, 5, 8); }, "high graphics storage slot rejects"
             )
            && ok;
        const auto patch = f.PatchParameteri;
        f.PatchParameteri = nullptr;
        for (int stage : {4, 5})
            ok = storage_rejected(
                     t,
                     [&] { return igpu_storage_bind(data, stage, 0); },
                     "missing patch support rejects tessellation storage claim"
                 )
                && ok;
        ok = check(
                 igpu::gl_read_storage_supported() && caller.unchanged(),
                 "optional stage failure retains existing storage capability and caller ranges"
             )
            && ok;
        f.PatchParameteri = patch;
        int rendered = 0, unsupported = 0;
        for (int stage = 3; stage <= 5; ++stage)
            for (int slot : {0, 7}) {
                const GLuint physical = 8 + stage * 8 + slot;
                const bool available = igpu::gl_graphics_stage_supported(stage) && limits[stage - 3] >= 8 &&
                    physical < static_cast<GLuint>(maximum);
                if (!available) {
                    ++unsupported;
                    std::printf("graphics storage case: stage%d slot%d binding%u blocks%d global%d unsupported\n",
                                stage, slot, physical, limits[stage - 3], maximum);
                    ok = storage_rejected(t, [&] { return igpu_storage_bind(data, stage, slot); },
                                          "unsupported higher storage slot rejects quietly") && ok;
                    continue;
                }
                if (!check(igpu_storage_bind(data, stage, slot), "geometry/hull/domain storage input bind")) {
                    ok = false;
                    continue;
                }
                igpu::GlStorageBuffer native;
                ok = igpu::gl_storage_buffer(data, native, "test") && ok;
                StorageRange actual;
                actual.capture(physical);
                // GL4.6 core 6.7.1: base bindings report zero range size; the full buffer is used.
                ok = check(
                         actual.name == static_cast<GLint>(native.name) && actual.offset == 0 && actual.size == 0,
                         "new stage physical storage binding"
                     )
                    && ok;
                std::string source = stage == 3 ? gs : stage == 4 ? tc : te;
                const std::string declaration = "layout(std430,binding=" + std::to_string(physical)
                    + ")readonly buffer StorageData{uint storage_values[];};";
                source.insert(source.find("void main()"), declaration);
                if (stage == 3) {
                    std::size_t pos = 0;
                    while ((pos = source.find("EmitVertex();", pos)) != std::string::npos) {
                        const std::string shift = "gl_Position.x+=float(storage_values[0])-7.0;";
                        source.insert(pos, shift);
                        pos += shift.size() + 12;
                    }
                } else if (stage == 4) {
                    source.replace(
                        source.find("gl_TessLevelOuter[0]=1"),
                        std::strlen("gl_TessLevelOuter[0]=1"),
                        "gl_TessLevelOuter[0]=float(storage_values[0])/7.0"
                    );
                    source.insert(
                        source.find("if(gl_InvocationID==0)"),
                        "gl_out[gl_InvocationID].gl_Position.x+=float(storage_values[0])-7.0;"
                    );
                } else
                    source.insert(source.rfind('}'), "gl_Position.x+=float(storage_values[0])-7.0;");
                const auto custom = t.shader(source.c_str(), stage);
                bool pipeline = custom && igpu_shader_bind(custom, stage);
                if (stage != 3) {
                    const auto other = t.shader(stage == 4 ? te : tc, stage == 4 ? 5 : 4);
                    pipeline = other && igpu_shader_bind(other, stage == 4 ? 5 : 4) && pipeline;
                }
                const bool pixels = pipeline && t.pixels(stage != 3, 0, stage == 3 ? 1 : 3);
                rendered += pixels ? 1 : 0;
                std::printf("graphics storage case: stage%d slot%d binding%u blocks%d global%d %s\n",
                            stage, slot, physical, limits[stage - 3], maximum, pixels ? "rendered" : "failed");
                ok = check(pixels, "higher storage shader pipeline and pixels") && ok;
                using Map = void*(APIENTRY*)(GLenum, GLenum);
                using Unmap = GLboolean(APIENTRY*)(GLenum);
                const auto map = reinterpret_cast<Map>(wglGetProcAddress("glMapBuffer"));
                const auto unmap = reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
                GLint array = 0;
                glGetIntegerv(0x8894, &array);
                f.BindBuffer(0x8892, native.name);
                auto pointer = map(0x8892, 0x88B8);
                if (pointer)
                    ok = storage_rejected(
                             t,
                             [&] {
                                 return stage == 3 ? igpu_draw(t.vertex, 0, t.layout, 1, 0, 1, 1, 0, 0, 0, 0)
                                                   : igpu_draw_patch(t.vertex, t.layout, 3, 0, 3, 0, 0, 0, 0);
                             },
                             "late mapped higher-stage storage input"
                         )
                        && ok;
                else
                    ok = check(false, "map higher storage") && ok;
                if (pointer)
                    unmap(0x8892);
                f.BindBuffer(0x8892, array);
                ok = check(igpu_storage_bind(replacement, stage, slot), "replace higher storage claim") && ok;
                igpu::GlStorageBuffer replaced;
                StorageRange replaced_range;
                replaced_range.capture(physical);
                ok = check(
                         igpu::gl_storage_buffer(replacement, replaced, "test")
                             && replaced_range.name == static_cast<GLint>(replaced.name) && replaced_range.offset == 0
                             && replaced_range.size == 0,
                         "replacement installs the new native storage buffer"
                     )
                    && ok;
                for (int s : {3, 4, 5})
                    igpu_shader_bind(0, s);
                ok = check(
                         igpu_storage_bind(0, stage, slot) && caller.unchanged() && caller.bytes() && !glGetError(),
                         "all borrowed higher SSBO name/start/size/generic/byte91 restored"
                     )
                    && ok;
            }
        // Release the last owner while several higher-stage slots are claimed.
        const auto released = buffers.create();
        if (released) {
            for (int stage = 3; stage <= 5; ++stage)
                if (igpu::gl_graphics_stage_supported(stage) && limits[stage - 3] >= 8 && 8 + stage * 8 < maximum)
                    ok = igpu_storage_bind(released, stage, 0) && ok;
            ok = igpu_buffer_release(released) && caller.unchanged() && caller.bytes() && ok;
            buffers.handles.pop_back();
        }
        if (ok)
            std::printf(
                "graphics storage: rendered=%d/6 unsupported=%d, real pixels16samples, physical bindings, late mapping, "
                "replacement/unbind/release and borrowed ranges/byte91 passed\n", rendered, unsupported
            );
        return ok;
    }
    bool storage_late_map_test()
    {
        if (!igpu::gl_read_storage_supported())
            return true;
        Fixture t;
        if (!t.initialize())
            return false;
        StorageBuffers buffers;
        const auto storage = buffers.create();
        if (!storage)
            return false;
        StorageCaller caller({8, 16, 24});
        if (!check(igpu_storage_bind(storage, 0, 0), "vertex storage late-map fixture"))
            return false;
        auto& f = igpu_gl_fns();
        igpu::GlStorageBuffer native;
        if (!igpu::gl_storage_buffer(storage, native, "test"))
            return false;
        const auto index = igpu_buffer_create(6, 1, 2, 0), args = igpu_buffer_create(16, 0, 16, 0),
                   source = igpu_texture_create(0, 1, 1, 1, 6, true, false, 1);
        std::array<std::uint16_t, 3> indices {0, 1, 2};
        std::array<std::uint32_t, 4> commands {3, 1, 0, 0};
        bool ok = index && args && source
            && igpu_buffer_write(index, 0, gm::wire::GMBuffer(indices.data(), sizeof(indices)))
            && igpu_buffer_write(args, 0, gm::wire::GMBuffer(commands.data(), sizeof(commands)));
        using Map = void*(APIENTRY*)(GLenum, GLenum);
        using Unmap = GLboolean(APIENTRY*)(GLenum);
        const auto map = reinterpret_cast<Map>(wglGetProcAddress("glMapBuffer"));
        const auto unmap = reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        GLint array = 0;
        glGetIntegerv(0x8894, &array);
        f.BindBuffer(0x8892, native.name);
        auto pointer = map(0x8892, 0x88B8);
        if (pointer) {
            Array target({t.target}), zero({0});
            ok = storage_rejected(
                     t,
                     [&] { return igpu_draw(t.vertex, 0, t.layout, 4, 0, 3, 1, 0, 0, 0, 0); },
                     "late mapped vertex SSBO ordinary reject"
                 )
                && ok;
            ok = storage_rejected(
                     t,
                     [&] { return igpu_draw(t.vertex, 0, t.layout, 4, 0, 3, 2, 0, 0, 0, 0); },
                     "late mapped vertex SSBO instanced reject"
                 )
                && ok;
            ok = storage_rejected(
                     t,
                     [&] { return igpu_draw_indexed(t.vertex, t.layout, index, 4, 0, 3, 0, 0, 0, 0); },
                     "late mapped vertex SSBO indexed reject"
                 )
                && ok;
            ok = storage_rejected(
                     t,
                     [&] { return igpu_draw_indirect(t.vertex, 0, t.layout, 4, args, 0, 0, 0, 0, 0); },
                     "late mapped vertex SSBO indirect reject"
                 )
                && ok;
            ok = storage_rejected(
                     t,
                     [&] {
                         return igpu_draw_to_render_targets(
                             t.vertex, t.layout, 4, 0, 3, target.view(), zero.view(), zero.view(), 0, 0, 0, 0
                         );
                     },
                     "late mapped vertex SSBO target reject"
                 )
                && ok;
            const auto sampled = t.shader(
                "#version 430\nuniform sampler2D igpu_tex;out vec4 c;void main(){c=texture(igpu_tex,vec2(.5));}", 1
            );
            ok = sampled && igpu_shader_bind(sampled, 1) && ok;
            ok = storage_rejected(
                     t,
                     [&] { return igpu_draw_sampled(t.vertex, t.layout, 4, 0, 3, source, 0, 0, 0, 0); },
                     "late mapped vertex SSBO sampled reject"
                 )
                && ok;
            unmap(0x8892);
        } else
            ok = check(false, "map vertex storage") && ok;
        f.BindBuffer(0x8892, array);
        igpu_shader_bind(t.pshader, 1);
        const auto hull = t.shader(tc, 4), domain = t.shader(te, 5);
        ok = hull && domain && igpu_shader_bind(hull, 4) && igpu_shader_bind(domain, 5) && ok;
        f.BindBuffer(0x8892, native.name);
        pointer = map(0x8892, 0x88B8);
        if (pointer) {
            ok = storage_rejected(
                     t,
                     [&] { return igpu_draw_patch(t.vertex, t.layout, 3, 0, 3, 0, 0, 0, 0); },
                     "late mapped vertex SSBO patch reject"
                 )
                && ok;
            unmap(0x8892);
        } else
            ok = false;
        f.BindBuffer(0x8892, array);
        igpu_shader_bind(0, 4);
        igpu_shader_bind(0, 5);
        ok = igpu_storage_bind(0, 0, 0) && igpu_storage_bind(storage, 1, 0) && ok;
        f.BindBuffer(0x8892, native.name);
        pointer = map(0x8892, 0x88B8);
        if (pointer) {
            ok = storage_rejected(
                     t,
                     [&] { return igpu_draw(t.vertex, 0, t.layout, 4, 0, 3, 1, 0, 0, 0, 0); },
                     "late mapped Pixel SSBO ordinary reject"
                 )
                && ok;
            unmap(0x8892);
        } else
            ok = false;
        f.BindBuffer(0x8892, array);
        ok = igpu_storage_bind(0, 1, 0) && igpu_storage_bind(storage, 2, 0) && ok;
        f.BindBuffer(0x8892, native.name);
        pointer = map(0x8892, 0x88B8);
        if (pointer) {
            ok = t.pixels() && ok;
            unmap(0x8892);
        } else
            ok = false;
        f.BindBuffer(0x8892, array);
        ok = igpu_storage_bind(0, 2, 0) && caller.unchanged() && caller.bytes() && ok;
        if (index)
            igpu_buffer_release(index);
        if (args)
            igpu_buffer_release(args);
        if (source)
            igpu_texture_release(source);
        if (ok)
            std::printf(
                "graphics storage validation: late mapped Vertex input rejects "
                "ordinary/indexed/instance/indirect/target/sample/patch without counters; mapped Compute-only input "
                "does not block graphics\n"
            );
        return ok;
    }
}
bool run_gl_graphics_stage_tests(){bool ok=geometry_test();ok=tessellation_test()&&ok;ok=uniform_stage_test()&&ok;ok=tessellation_uniform_test()&&ok;ok=feedback_test()&&ok;ok=transaction_test()&&ok;ok=patch_guard_test()&&ok;ok=executable_stage_test()&&ok;ok=primitive_stage_test()&&ok;ok=storage_stage_test()&&ok;ok=storage_late_map_test()&&ok;return ok;}

namespace
{
    IgpuGlCreateShader forward_shader=nullptr;GLuint last_shader=0;
    GLuint APIENTRY observe_shader(GLenum type){last_shader=forward_shader(type);return last_shader;}
}
bool run_gl_graphics_stage_shutdown_tests()
{
    auto&f=igpu_gl_fns();f.UseProgram(0);const auto context=wglGetCurrentContext();GLint patch=0,original_generic=0;glGetIntegerv(0x8E72,&patch);glGetIntegerv(0x8A28,&original_generic);
    std::array<Handle,5>handles{};std::array<GLuint,5>native{};const char*sources[]={vs,ps,gs,tc,te};const int stages[]={0,1,3,4,5};
    bool ok=true;
    for(int i=0;i<5;++i){forward_shader=f.CreateShader;f.CreateShader=observe_shader;handles[i]=igpu_shader_compile(sources[i],"main",stages[i],"glsl");native[i]=last_shader;f.CreateShader=forward_shader;ok=handles[i]&&ok;}
    ok=ok&&igpu_shader_bind(handles[0],0)&&igpu_shader_bind(handles[1],1)&&igpu_shader_bind(handles[3],4)&&igpu_shader_bind(handles[4],5);
    for(int i=2;i<5;++i){igpu_shader_reflect(handles[i]);ok=igpu_get_last_error().empty()&&ok;}
    const auto uniform=igpu_buffer_create(16,1,4,0);igpu::GlUniformBuffer buffer;ok=uniform&&igpu::gl_uniform_buffer(uniform,buffer,"test")&&ok;
    GLuint foreign=0;f.GenBuffers(1,&foreign);f.BindBuffer(0x8A11,foreign);GLint alignment=0;glGetIntegerv(0x8A34,&alignment);std::vector<unsigned char>sentinel(alignment+128,0xA6);f.BufferData(0x8A11,sentinel.size(),sentinel.data(),0x88E4);
    GLint bindings=0;glGetIntegerv(0x8A2F,&bindings);std::vector<GLuint>slots;
    for(int stage=3;stage<6;++stage)if(stage*8<bindings){const GLuint slot=stage*8;slots.push_back(slot);f.BindBufferRange(0x8A11,slot,foreign,alignment,64);ok=igpu_uniform_bind(uniform,stage,0)&&ok;}
    GLint original_ssbo=0,storage_alignment=0,storage_maximum=0;GLuint storage_foreign=0;Handle storage=0;igpu::GlStorageBuffer storage_native;
    std::vector<GLuint>storage_slots;std::vector<StorageRange>storage_previous;std::vector<unsigned char>storage_sentinel;
    if(igpu::gl_read_storage_supported())
    {
        glGetIntegerv(0x90D3,&original_ssbo);glGetIntegerv(0x90DF,&storage_alignment);glGetIntegerv(0x90DD,&storage_maximum);
        storage=igpu_buffer_create(16,1,8,4);ok=storage&&igpu::gl_storage_buffer(storage,storage_native,"shutdown storage")&&ok;
        f.GenBuffers(1,&storage_foreign);f.BindBuffer(0x90D2,storage_foreign);storage_sentinel.assign(storage_alignment+128,91);f.BufferData(0x90D2,storage_sentinel.size(),storage_sentinel.data(),0x88E4);
        const GLenum stage_fields[]={0x90D7,0x90D8,0x90D9};for(int stage=3;stage<=5;++stage)
        {
            GLint count=0;glGetIntegerv(stage_fields[stage-3],&count);const GLuint slot=8+stage*8;
            if(count<8||slot>=static_cast<GLuint>(storage_maximum))continue;
            StorageRange previous;previous.capture(slot);storage_previous.push_back(previous);storage_slots.push_back(slot);
            f.BindBufferRange(0x90D2,slot,storage_foreign,storage_alignment,64);ok=igpu_storage_bind(storage,stage,0)&&ok;
        }
        f.BindBuffer(0x90D2,storage_foreign);
    }
    if(!check(ok,"graphics shutdown fixtures"))return false;
    igpu_shutdown();ok=!igpu_is_available()&&wglGetCurrentContext()==context;
    using IsShader=GLboolean(APIENTRY*)(GLuint);using IsBuffer=GLboolean(APIENTRY*)(GLuint);const auto is_shader=reinterpret_cast<IsShader>(wglGetProcAddress("glIsShader"));const auto is_buffer=reinterpret_cast<IsBuffer>(wglGetProcAddress("glIsBuffer"));
    for(auto shader:native)ok=!is_shader(shader)&&ok;ok=!is_buffer(buffer.name)&&ok;GLint generic=0;glGetIntegerv(0x8A28,&generic);ok=generic==static_cast<GLint>(foreign)&&ok;
    for(auto slot:slots){GLint name=0;std::int64_t offset=0,size=0;f.GetIntegeri(0x8A28,slot,&name);f.GetInteger64i(0x8A29,slot,&offset);f.GetInteger64i(0x8A2A,slot,&size);ok=name==static_cast<GLint>(foreign)&&offset==alignment&&size==64&&ok;f.BindBufferBase(0x8A11,slot,0);}
    if(storage_foreign)
    {
        GLint ssbo=0;glGetIntegerv(0x90D3,&ssbo);ok=ssbo==static_cast<GLint>(storage_foreign)&&!is_buffer(storage_native.name)&&ok;
        for(std::size_t i=0;i<storage_slots.size();++i){StorageRange restored;restored.capture(storage_slots[i]);ok=restored.name==static_cast<GLint>(storage_foreign)&&restored.offset==storage_alignment&&restored.size==64&&ok;storage_previous[i].restore(storage_slots[i]);}
        f.BindBuffer(0x90D2,storage_foreign);std::vector<unsigned char>actual(storage_sentinel.size());f.GetBufferSubData(0x90D2,0,actual.size(),actual.data());ok=actual==storage_sentinel&&ok;
        f.BindBuffer(0x90D2,original_ssbo);f.DeleteBuffers(1,&storage_foreign);
    }
    GLint actual_patch=0;glGetIntegerv(0x8E72,&actual_patch);
    f.BindBuffer(0x8A11,foreign);std::vector<unsigned char>actual(sentinel.size());f.GetBufferSubData(0x8A11,0,actual.size(),actual.data());ok=actual==sentinel&&actual_patch==patch&&ok;f.BindBuffer(0x8A11,original_generic);f.DeleteBuffers(1,&foreign);
    if(!check(ok&&!glGetError(),"graphics shutdown deletes native shaders/buffer and restores new stage ranges"))return false;
    if(!check(igpu_bind_current()&&igpu::supports(igpu::Capability::ShaderStageGeometry)&&igpu::supports(igpu::Capability::ShaderStageTessellation)&&!igpu_shader_bind(handles[2],3),"graphics rebind capabilities and stale handles"))return false;
    std::printf("graphics shutdown: five graphics shaders/native UBO deleted, geometry/hull/domain UBO+SSBO ranges/caller bytes restored and rebind passed\n");return true;
}
