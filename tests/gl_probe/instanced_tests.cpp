#include "instanced_tests.h"
#include "IGPU_native.h"
#include "native/igpu_capabilities.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using Handle = std::int64_t;
    constexpr GLenum array_buffer=0x8892, element_buffer=0x8893, indirect_buffer=0x8F3F;
    constexpr GLenum fields[]={0x8B8D,0x85B5,0x8894,0x8895,0x8F43,0x8F36,0x8F37};
    bool check(bool ok,const char* label)
    { if(!ok) std::fprintf(stderr,"instanced tests: %s: %s\n",label,igpu_get_last_error().c_str()); return ok; }
    struct Bindings
    {
        std::array<GLint,7> values{};
        Bindings(){for(int i=0;i<7;++i) glGetIntegerv(fields[i],&values[i]);}
        bool operator==(const Bindings& b)const{return values==b.values;}
        void restore()const
        {
            auto& f=igpu_gl_fns(); f.UseProgram(values[0]); f.BindVertexArray(values[1]);
            f.BindBuffer(array_buffer,values[2]); f.BindBuffer(element_buffer,values[3]);
            f.BindBuffer(indirect_buffer,values[4]); f.BindBuffer(0x8F36,values[5]); f.BindBuffer(0x8F37,values[6]);
        }
    };
    template<std::size_t N> auto wire(const std::array<std::int32_t,N>& values)
    {
        std::array<std::byte,2+5*N> b{}; b[0]=std::byte{N};
        for(std::size_t i=0;i<N;++i){b[2+5*i]=std::byte{6};std::memcpy(b.data()+3+5*i,&values[i],4);} return b;
    }
    template<std::size_t N> gm::wire::GMArrayView view(const std::array<std::byte,N>& b)
    {return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array,b.data()));}
    IgpuGlGenBuffers forward_gen=nullptr;
    GLuint generated_buffer=0;
    void APIENTRY observe_buffer(GLsizei count,GLuint* names)
    {forward_gen(count,names);if(count)generated_buffer=names[count-1];}
    struct Fixture
    {
        Bindings saved;
        Handle vertex=0, instances=0, index=0, args=0, zeros=0, target=0, basic=0, uv=0, colour=0;
        GLuint program=0, vao=0, foreign=0;
        std::vector<Handle> buffers,layouts,states;
        std::array<std::uint32_t,16> commands{};
        std::array<unsigned char,64> sentinel{};
        ~Fixture()
        {
            auto& f=igpu_gl_fns(); saved.restore();
            for(auto id:layouts) igpu_input_layout_release(id);
            for(auto id:buffers) igpu_buffer_release(id);
            for(auto id:states) igpu_state_release(id);
            if(target) igpu_texture_release(target);
            if(program) f.DeleteProgram(program);
            if(vao) f.DeleteVertexArrays(1,&vao); if(foreign) f.DeleteBuffers(1,&foreign);
        }
        Handle buffer(int size,int bind,int stride)
        {const auto h=igpu_buffer_create(size,bind==16?0:1,bind,stride);if(h)buffers.push_back(h);return h;}
        template<class T> bool write(Handle id,T& data)
        {return check(igpu_buffer_write(id,0,gm::wire::GMBuffer(data.data(),sizeof(data))),"upload");}
        Handle layout(int instance_stride=0,bool is_colour=false,bool interleaved=false)
        {
            const auto shader=igpu_get_bound_shader(0);
            Handle h=0;
            if(!instance_stride && !interleaved)
            {
                const auto u=wire(std::array<std::int32_t,1>{1}),t=wire(std::array<std::int32_t,1>{2}),s=wire(std::array<std::int32_t,1>{0});
                h=igpu_input_layout_create(shader,view(u),view(t),view(s),1,8,0);
            }
            else
            {
                const auto u=wire(std::array<std::int32_t,2>{1,is_colour?2:4});
                const auto t=wire(std::array<std::int32_t,2>{2,is_colour?5:2});
                const auto s=wire(std::array<std::int32_t,2>{0,interleaved?0:1});
                h=igpu_input_layout_create(shader,view(u),view(t),view(s),2,interleaved?16:8,instance_stride);
            }
            if(h)layouts.push_back(h);return h;
        }
        bool commands_set(std::array<std::uint32_t,4> arrays,std::array<std::uint32_t,5> indexed)
        {commands.fill(0);std::memcpy(commands.data(),arrays.data(),16);std::memcpy(commands.data()+8,indexed.data(),20);return write(args,commands);}
        bool initialize()
        {
            using C=igpu::Capability;
            if(!check(igpu::supports(C::Instancing)&&igpu::supports(C::IndirectDraw),"advertised draw capabilities"))return false;
            auto& f=igpu_gl_fns();
            const char* sources[]={
                "#version 330\nlayout(location=0)in vec2 in_pos;layout(location=1)in vec4 in_data;uniform int mode;out vec4 c;"
                "void main(){vec2 off=mode==1?in_data.xy:(mode==3?vec2(0):vec2(float(gl_InstanceID)-0.5,0));"
                "gl_Position=vec4(in_pos+off,0,1);c=mode==2?in_data:vec4(1,0,0,1);}",
                "#version 330\nin vec4 c;out vec4 colour;void main(){colour=c;}"};
            program=f.CreateProgram();
            for(int stage=0;stage<2;++stage)
            {
                const auto shader=f.CreateShader(stage?0x8B30:0x8B31);f.ShaderSource(shader,1,&sources[stage],nullptr);f.CompileShader(shader);
                GLint ok=0;f.GetShaderiv(shader,0x8B81,&ok);if(ok)f.AttachShader(program,shader);f.DeleteShader(shader);
                if(!check(ok!=0,"test shader"))return false;
            }
            f.LinkProgram(program);GLint linked=0;f.GetProgramiv(program,0x8B82,&linked);if(!check(linked!=0,"test program"))return false;
            vertex=buffer(40,1,8);instances=buffer(16,1,8);index=buffer(8,2,0);args=buffer(64,16,0);zeros=buffer(64,16,0);
            basic=layout();uv=layout(8);colour=layout(8,true);target=igpu_texture_create(0,16,8,1,6,true,false,1);
            std::array<float,10> positions={99,99,-.25f,-.5f,.25f,-.5f,-.25f,.5f,.25f,.5f};
            std::array<float,4> offsets={-.5f,0,.5f,0};std::array<std::uint16_t,4> indices={0,1,2,3};
            if(!check(vertex&&instances&&index&&args&&zeros&&basic&&uv&&colour&&target,"fixture allocation")||
                !write(vertex,positions)||!write(instances,offsets)||!write(index,indices)||!commands_set({4,2,1,0},{4,2,0,1,0}))return false;
            f.GenVertexArrays(1,&vao);f.GenBuffers(1,&foreign);f.BindVertexArray(vao);f.BindBuffer(array_buffer,foreign);
            sentinel.fill(0xA6);f.BufferData(array_buffer,sentinel.size(),sentinel.data(),0x88E4);
            for(auto target:{element_buffer,indirect_buffer,GLenum{0x8F36},GLenum{0x8F37}})f.BindBuffer(target,foreign);
            f.UseProgram(program);return check(glGetError()==GL_NO_ERROR,"fixture state");
        }
        bool direct(Handle ib,Handle input,std::int64_t n=2,std::int64_t first=1,std::int64_t count=4,
                    Handle blend=0,Handle depth=0,Handle raster=0)
        {return igpu_draw(vertex,ib,input,5,first,count,n,blend,depth,raster,0);}
        bool indirect(Handle ib,Handle input,bool indexed=false,Handle arguments=0,std::int64_t offset=-1)
        {
            if(!arguments)arguments=args;if(offset<0)offset=indexed?32:0;
            return indexed?igpu_draw_indexed_indirect(vertex,ib,input,index,5,arguments,offset,0,0,0,0):
                igpu_draw_indirect(vertex,ib,input,5,arguments,offset,0,0,0,0);
        }
        template<class F> bool image(F draw,int mode,std::array<std::uint32_t,3> colors,int samples,const char* label)
        {
            auto& f=igpu_gl_fns();std::int32_t previous=0,viewport[4]{};GLint read=0,pack=0,pack_values[5]{};
            const GLenum pixel_fields[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_SWAP_BYTES};
            glGetIntegerv(0x8CAA,&read);glGetIntegerv(0x88ED,&pack);for(int i=0;i<5;++i)glGetIntegerv(pixel_fields[i],&pack_values[i]);
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            if(!igpu::gl_color_target_begin(target,previous,viewport)){glPopAttrib();return false;}
            for(auto cap:{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_BLEND,GL_ALPHA_TEST,0x809D,0x8C89})glDisable(cap);
            GLint clips=0;glGetIntegerv(0x0D32,&clips);for(int i=0;i<clips;++i)glDisable(GL_CLIP_PLANE0+i);
            glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            f.UseProgram(program);f.Uniform1i(f.GetUniformLocation(program,"mode"),mode);
            const Bindings before;const auto query=igpu_query_create(0);
            bool ok=query&&igpu_query_begin(query)&&draw()&&igpu_get_last_error().empty();
            const bool state=before==Bindings{};if(query)ok=igpu_query_end(query)&&ok;
            const auto count=query?igpu_query_result(query):-1;ok=ok&&count==samples&&igpu_get_last_error().empty();if(query)igpu_query_release(query);
            f.BindBuffer(0x88EB,0);for(int i=0;i<5;++i)glPixelStorei(pixel_fields[i],i?0:1);
            for(int i=0;i<3;++i){std::array<unsigned char,4> pixel{};glReadPixels(4+4*i,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
                const auto actual=std::uint32_t(pixel[0])|(std::uint32_t(pixel[1])<<8)|(std::uint32_t(pixel[2])<<16);ok=ok&&actual==colors[i];}
            const auto error=glGetError();
            for(int i=0;i<5;++i)glPixelStorei(pixel_fields[i],pack_values[i]);f.BindBuffer(0x88EB,pack);
            igpu::gl_color_target_end(previous,viewport);f.BindFramebuffer(0x8CA8,read);glPopAttrib();
            if(!(ok&&state&&error==GL_NO_ERROR))std::fprintf(stderr,"advanced image %s: samples=%lld expected=%d state=%d GL=0x%x\n",label,static_cast<long long>(count),samples,state,error);
            return check(ok&&state&&error==GL_NO_ERROR,label);
        }
        template<class F> bool rejected(F action,const char* label)
        {const Bindings before;const bool result=action();const bool error=!igpu_get_last_error().empty();return check(!result&&error&&before==Bindings{}&&glGetError()==GL_NO_ERROR,label);}
        bool foreign_unchanged()
        {
            auto& f=igpu_gl_fns();const Bindings before;f.BindBuffer(array_buffer,foreign);std::array<unsigned char,64> actual{};
            f.GetBufferSubData(array_buffer,0,actual.size(),actual.data());before.restore();return check(actual==sentinel,"foreign buffer bytes unchanged");
        }
    };
}

bool run_gl_instanced_tests()
{
    Fixture p;if(!p.initialize())return false;
    constexpr std::uint32_t red=255;
    if(!p.image([&]{return p.direct(0,p.basic);},0,{red,0,red},32,"builtin InstanceID")||
        !p.image([&]{return p.direct(p.instances,p.uv);},1,{red,0,red},32,"step-1 UV")||
        !p.image([&]{return p.direct(p.instances,p.uv,1);},1,{red,0,0},16,"one instance")||
        !check(igpu_buffer_resize(p.instances,24),"grow instance buffer")||
        !p.image([&]{return p.direct(p.instances,p.uv,3,1,-1);},1,{red,red,red},48,"zero-grown third instance")||
        !check(igpu_buffer_resize(p.instances,16),"shrink instance buffer"))return false;
    for(int stride:{8,12,16})
    {
        const auto layout=p.layout(stride),buffer=p.buffer(stride*2,1,stride);std::vector<unsigned char> data(stride*2,0);
        const float left=-.5f,right=.5f;std::memcpy(data.data(),&left,4);std::memcpy(data.data()+stride,&right,4);
        if(!check(layout&&buffer&&igpu_buffer_write(buffer,0,gm::wire::GMBuffer(data.data(),data.size())),"padded instances")||
            !p.image([&]{return p.direct(buffer,layout);},1,{red,0,red},32,"padded instance stride"))return false;
    }
    const auto colors=p.buffer(16,1,8);std::array<unsigned char,16> bytes={128,0,0,255,0,0,0,0,0,64,0,255,0,0,0,0};
    if(!p.write(colors,bytes)||!p.image([&]{return p.direct(colors,p.colour);},2,{128,0,16384},32,"normalized colour instances")||
        !p.image([&]{return p.indirect(0,p.basic);},0,{red,0,red},32,"array indirect")||
        !p.image([&]{return p.indirect(p.instances,p.uv,true);},1,{red,0,red},32,"indexed indirect baseVertex")||
        !p.commands_set({4,1,1,1},{4,1,0,1,1})||
        !p.image([&]{return p.indirect(p.instances,p.uv);},1,{0,0,red},16,"array baseInstance")||
        !p.image([&]{return p.indirect(p.instances,p.uv,true);},1,{0,0,red},16,"indexed baseInstance"))return false;
    std::array<std::uint16_t,4> shifted={2,3,4,5};
    if(!p.write(p.index,shifted)||!p.commands_set({0,0,0,0},{4,2,0,0xFFFFFFFFu,0})||
        !p.image([&]{return p.indirect(p.instances,p.uv,true);},1,{red,0,red},32,"negative baseVertex")||
        !p.image([&]{return p.indirect(0,p.basic,false,p.zeros);},0,{0,0,0},0,"zero array command")||
        !p.image([&]{return p.indirect(0,p.basic,true,p.zeros);},0,{0,0,0},0,"zero indexed command"))return false;
    const auto ordinary=p.layout(0,false,true),collapsed=p.buffer(64,1,16);
    std::array<float,16> collapsed_data={-.25f,-.5f,.25f,0,.25f,-.5f,-.25f,0,-.25f,.5f,.25f,0,.25f,.5f,-.25f,0};
    if(!p.write(collapsed,collapsed_data)||!p.image([&]{return igpu_draw(collapsed,0,ordinary,5,0,4,1,0,0,0,0);},1,{0,0,0},0,"ordinary attributes after divisor reset"))return false;
    const auto blend=igpu_blend_state_create(false,2,2,1,2,2,1,true,true,true,true);
    const auto depth=igpu_depth_state_create(false,false,8,false,8,1,1,1,0,255,255);
    const auto raster=igpu_raster_state_create(0,0,false,true);p.states={blend,depth,raster};
    if(!check(blend&&depth&&raster,"pipeline states")||!p.image([&]{
        glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_ZERO);glEnable(GL_CULL_FACE);glCullFace(GL_FRONT);
        const bool drew=p.direct(0,p.basic,2,1,4,blend,depth,raster);
        GLint cull=0;glGetIntegerv(GL_CULL_FACE_MODE,&cull);const bool restored=glIsEnabled(GL_BLEND)&&glIsEnabled(GL_CULL_FACE)&&cull==GL_FRONT;
        glDisable(GL_BLEND);glDisable(GL_CULL_FACE);return drew&&restored;
    },0,{red,0,red},32,"pipeline override and restore"))return false;
    for(std::int64_t value:{std::int64_t{-1},std::int64_t{2},std::int64_t{52},(std::numeric_limits<std::int64_t>::max)()})
        if(!p.rejected([&]{return igpu_draw_indirect(p.vertex,0,p.basic,5,p.args,value,0,0,0,0);},"invalid argument offset"))return false;
    for(std::int64_t value:{std::int64_t{0},std::int64_t{-1},(std::numeric_limits<std::int64_t>::max)()})
        if(!p.rejected([&]{return p.direct(0,p.basic,value);},"invalid instance count"))return false;
    const auto huge=(std::numeric_limits<std::int64_t>::max)();
    if(!p.rejected([&]{return p.direct(0,p.basic,2,huge,4);},"huge first vertex")||
        !p.rejected([&]{return p.direct(0,p.basic,2,1,huge);},"huge vertex count")||
        !p.rejected([&]{return p.direct(p.instances,p.uv,3);},"instance capacity")||
        !p.rejected([&]{return p.direct(0,p.uv);},"missing instance stream")||
        !p.rejected([&]{return p.direct(p.args,p.uv);},"wrong instance role")||
        !p.rejected([&]{return igpu_draw_indirect(p.vertex,0,p.basic,5,p.vertex,0,0,0,0,0);},"wrong argument role")||
        !p.rejected([&]{return igpu_draw_indexed_indirect(p.vertex,0,p.basic,0,5,p.args,32,0,0,0,0);},"missing index")||
        !p.rejected([&]{return igpu_draw_indexed_indirect(p.vertex,0,p.basic,p.vertex,5,p.args,32,0,0,0,0);},"wrong index role")||
        !p.rejected([&]{return igpu_draw(p.vertex,0,p.uv,5,1,4,1,0,0,0,0);},"ordinary draw with instance layout"))return false;
    const auto released=igpu_buffer_create(16,1,1,8);
    if(!check(released&&igpu_buffer_release(released),"release fixture")||
        !p.rejected([&]{return igpu_draw(released,0,p.basic,5,0,2,2,0,0,0,0);},"released vertex")||
        !p.rejected([&]{return p.direct(released,p.uv);},"released instances")||
        !p.rejected([&]{return igpu_draw_indirect(p.vertex,0,p.basic,5,released,0,0,0,0,0);},"released arguments")||
        !p.rejected([&]{return igpu_draw_indexed_indirect(p.vertex,0,p.basic,released,5,p.args,32,0,0,0,0);},"released index"))return false;
    const auto dead_layout=p.layout();if(!check(dead_layout&&igpu_input_layout_release(dead_layout),"release layout fixture"))return false;p.layouts.pop_back();
    if(!p.rejected([&]{return p.direct(0,dead_layout);},"released layout"))return false;
    auto& f=igpu_gl_fns();
    f.UseProgram(0);const bool missing_program=p.rejected([&]{return p.direct(0,p.basic);},"missing program");f.UseProgram(p.program);
    if(!missing_program)return false;
    using MapBuffer=void*(APIENTRY*)(GLenum,GLenum);using UnmapBuffer=GLboolean(APIENTRY*)(GLenum);
    const auto map=reinterpret_cast<MapBuffer>(wglGetProcAddress("glMapBuffer"));const auto unmap=reinterpret_cast<UnmapBuffer>(wglGetProcAddress("glUnmapBuffer"));
    if(!check(map&&unmap,"mapping inspection functions"))return false;
    for(int bind:{1,16})
    {
        forward_gen=f.GenBuffers;f.GenBuffers=observe_buffer;
        const auto mapped=p.buffer(bind==16?64:16,bind,bind==16?0:8);
        f.GenBuffers=forward_gen;const auto native=generated_buffer;
        GLint previous=0;glGetIntegerv(0x8894,&previous);f.BindBuffer(array_buffer,native);const auto address=map(array_buffer,0x88BA);
        const bool guarded=address&&p.rejected([&]{return bind==16?igpu_draw_indirect(p.vertex,0,p.basic,5,mapped,0,0,0,0,0):p.direct(mapped,p.uv);},"externally mapped input rejected");
        if(address)unmap(array_buffer);f.BindBuffer(array_buffer,previous);
        if(!check(guarded,"mapped buffer state and error"))return false;
    }
    using C=igpu::Capability;
    const auto divisor=f.VertexAttribDivisor;f.VertexAttribDivisor=nullptr;
    const bool gated=!igpu::supports(C::Instancing)&&!igpu::supports(C::IndirectDraw)&&p.rejected([&]{return p.direct(0,p.basic);},"missing divisor");f.VertexAttribDivisor=divisor;
    const auto arrays=f.DrawArraysIndirect;f.DrawArraysIndirect=nullptr;
    const bool indirect_gate=igpu::supports(C::Instancing)&&!igpu::supports(C::IndirectDraw)&&p.rejected([&]{return p.indirect(0,p.basic);},"missing indirect entry");f.DrawArraysIndirect=arrays;
    if(!check(gated&&indirect_gate,"independent capability gates")||!p.foreign_unchanged())return false;
    std::printf("advanced draws: InstanceID, UV/colour8/12/16, Dynamic grow/shrink, indirect offsets/baseVertex/baseInstance/no-op, divisor/state/buffer preservation passed\n");
    std::printf("advanced validation: numeric bounds, roles, missing streams/entrypoints and independent capability gates passed\n");
    return true;
}
