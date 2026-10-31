#include "uniform_tests.h"
#include "IGPU_native.h"
#include "native/igpu_capabilities.h"
#include "native/gl/igpu_gl_uniform.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include <vector>
#include <string>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    template<std::size_t N> auto wire(const std::array<std::int32_t,N>& values)
    {
        std::array<std::byte,2+5*N> b{};const auto count=static_cast<std::uint16_t>(N);std::memcpy(b.data(),&count,2);
        for(std::size_t i=0;i<N;++i){b[2+5*i]=std::byte{6};std::memcpy(b.data()+3+5*i,&values[i],4);}return b;
    }
    template<std::size_t N> gm::wire::GMArrayView view(const std::array<std::byte,N>& b)
    {return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array,b.data()));}
}

bool uniform_core_test()
{
    const auto shader=igpu_shader_compile("#version 430\nlayout(local_size_x=1)in;"
        "layout(std140,binding=16)uniform Data{uvec4 values;};"
        "layout(std430,binding=0)buffer Output{uint result[];};"
        "void main(){uint i=gl_GlobalInvocationID.x;result[i]=values[i];}","main",2,"glsl");
    if(!shader){std::fprintf(stderr,"uniform core compile: %s\n",igpu_get_last_error().c_str());return false;}
    const auto blocks=igpu_shader_reflect(shader);
    bool ok=igpu_get_last_error().empty()&&blocks.size()==1;
    if(ok)
    {
        const auto& b=blocks[0];ok=b.name=="Data"&&b.size==16&&b.slot==0&&b.members.size()==1;
        if(ok){const auto&m=b.members[0];ok=m.name=="values"&&m.offset==0&&m.size==16&&m.type==2&&m.rows==1&&m.columns==4&&m.elements==0;}
    }
    if(!ok){std::fprintf(stderr,"uniform core reflection: %s\n",igpu_get_last_error().c_str());igpu_shader_release(shader);return false;}
    const auto uniform=igpu_buffer_create(16,1,4,0),output=igpu_buffer_create(16,0,8,4);
    const auto numbers=wire(std::array<std::int32_t,4>{7,10,13,16});
    const auto kinds=wire(std::array<std::int32_t,1>{1}),targets=wire(std::array<std::int32_t,1>{static_cast<std::int32_t>(output)}),zero=wire(std::array<std::int32_t,1>{0});
    std::array<std::uint32_t,4> actual{};
    ok=uniform&&output&&igpu_uniform_write(uniform,shader,"Data","values",view(numbers))&&igpu_uniform_bind(uniform,2,0)&&
        igpu_shader_bind(shader,2)&&igpu_dispatch(4,1,1,view(kinds),view(targets),view(zero),view(zero))&&
        igpu_buffer_read(output,0,gm::wire::GMBuffer(actual.data(),sizeof(actual)))&&actual==std::array<std::uint32_t,4>{7,10,13,16};
    if(!ok)std::fprintf(stderr,"uniform core GPU data: %s (%u,%u,%u,%u)\n",igpu_get_last_error().c_str(),actual[0],actual[1],actual[2],actual[3]);
    igpu_uniform_bind(0,2,0);igpu_shader_bind(0,2);if(uniform)igpu_buffer_release(uniform);if(output)igpu_buffer_release(output);igpu_shader_release(shader);
    return ok;
}

namespace
{
    using Handle=std::int64_t;
    constexpr GLenum ubo=0x8A11;
    bool check(bool ok,const char*label)
    {if(!ok)std::fprintf(stderr,"uniform tests: %s: %s\n",label,igpu_get_last_error().c_str());return ok;}
    struct Range
    {
        GLint name=0;std::int64_t offset=0,size=0;
        void capture(GLuint slot){auto&f=igpu_gl_fns();f.GetIntegeri(0x8A28,slot,&name);f.GetInteger64i(0x8A29,slot,&offset);f.GetInteger64i(0x8A2A,slot,&size);}
        void restore(GLuint slot)const{auto&f=igpu_gl_fns();if(name&&size)f.BindBufferRange(ubo,slot,name,static_cast<std::ptrdiff_t>(offset),static_cast<std::ptrdiff_t>(size));else f.BindBufferBase(ubo,slot,name);}
        bool operator==(const Range&b)const{return name==b.name&&offset==b.offset&&size==b.size;}
    };
    struct State
    {
        std::array<Range,24> ranges;
        std::array<GLint,4> globals{};
        State(){for(GLuint i=0;i<24;++i)ranges[i].capture(i);const GLenum fields[]={0x8A28,0x8894,0x8B8D,0x85B5};for(int i=0;i<4;++i)glGetIntegerv(fields[i],&globals[i]);}
        bool operator==(const State&b)const{return ranges==b.ranges&&globals==b.globals;}
        void restore()const{auto&f=igpu_gl_fns();for(GLuint i=0;i<24;++i)ranges[i].restore(i);f.BindBuffer(ubo,globals[0]);f.BindBuffer(0x8892,globals[1]);f.UseProgram(globals[2]);f.BindVertexArray(globals[3]);}
    };
    struct Fixture
    {
        State original;
        std::vector<Handle> shaders,buffers;
        GLuint foreign=0;
        ~Fixture()
        {
            for(int stage=0;stage<3;++stage)for(int slot=0;slot<8;++slot)igpu_uniform_bind(0,stage,slot);
            igpu_shader_bind(0,2);for(auto s:shaders)if(s)igpu_shader_release(s);for(auto b:buffers)if(b)igpu_buffer_release(b);
            original.restore();if(foreign)igpu_gl_fns().DeleteBuffers(1,&foreign);
        }
        Handle shader(const std::string&source){auto h=igpu_shader_compile(source,"main",2,"glsl");if(h)shaders.push_back(h);return h;}
        Handle buffer(int size,int role=4,int usage=1,int stride=0){auto h=igpu_buffer_create(size,usage,role,stride);if(h)buffers.push_back(h);return h;}
        bool release(Handle h){for(auto&b:buffers)if(b==h)b=0;return igpu_buffer_release(h);}
        template<std::size_t N>bool write(Handle b,Handle s,const char*member,const std::array<std::int32_t,N>&values,const char*block="Data")
        {const auto w=wire(values);return check(igpu_uniform_write(b,s,block,member,view(w)),member);}
        bool dispatch(Handle output,int n=1)
        {const auto k=wire(std::array<std::int32_t,1>{1}),t=wire(std::array<std::int32_t,1>{int(output)}),z=wire(std::array<std::int32_t,1>{0});return igpu_dispatch(n,1,1,view(k),view(t),view(z),view(z));}
        template<std::size_t N>bool read(Handle h,const std::array<std::uint32_t,N>&want)
        {
            std::array<std::uint32_t,N>actual{};bool ok=igpu_buffer_read(h,0,gm::wire::GMBuffer(actual.data(),sizeof(actual)))&&actual==want;
            if(!ok){std::fprintf(stderr,"uniform GPU words:");for(auto v:actual)std::fprintf(stderr," %u",v);std::fprintf(stderr,"\n");}return check(ok,"literal GPU words");
        }
        template<class F>bool rejected(F action,const char*label)
        {const State before;const bool result=action(),error=!igpu_get_last_error().empty();const auto glerror=glGetError();return check(!result&&error&&before==State{}&&!glerror,label);}
        bool sentinels()
        {
            auto&f=igpu_gl_fns();GLint alignment=0;glGetIntegerv(0x8A34,&alignment);f.GenBuffers(1,&foreign);f.BindBuffer(ubo,foreign);
            std::vector<unsigned char>bytes(static_cast<std::size_t>(alignment)*4+256,0xA6);f.BufferData(ubo,bytes.size(),bytes.data(),0x88E4);
            for(GLuint i=0;i<24;++i)f.BindBufferRange(ubo,i,foreign,alignment*(1+i%3),64);f.BindBuffer(0x8892,foreign);
            return check(glGetError()==GL_NO_ERROR,"external UBO indexed ranges");
        }
    };
    const char* layout_source="#version 430\nlayout(local_size_x=1)in;layout(std140,binding=16)uniform Data{"
        "vec3 v;float tail;float a[2];mat2x3 cm;layout(row_major)mat2x3 rm;mat2 matrices[2];uvec2 uv;ivec2 iv;bvec2 bv;float one[1];};"
        "layout(std430,binding=0)buffer Output{uint r[];};void main(){"
        "r[0]=uint(v.x);r[1]=uint(v.y);r[2]=uint(v.z);r[3]=uint(tail);r[4]=uint(a[0]);r[5]=uint(a[1]);"
        "r[6]=uint(cm[0][2]);r[7]=uint(cm[1][1]);r[8]=uint(rm[1][0]);r[9]=uint(rm[0][2]);"
        "r[10]=uint(matrices[0][1][0]);r[11]=uint(matrices[1][0][1]);r[12]=uint(matrices[1][1][1]);"
        "r[13]=uv.x;r[14]=uv.y;r[15]=uint(iv.x+20);r[16]=uint(iv.y);r[17]=uint(bv.x);r[18]=uint(bv.y);r[19]=uint(one[0]);}";
    bool layout_tests()
    {
        Fixture t;const auto shader=t.shader(layout_source),buffer=t.buffer(240),output=t.buffer(80,8,0,4);
        if(!check(shader&&buffer&&output&&t.sentinels(),"layout fixtures"))return false;
        const State before;const auto blocks=igpu_shader_reflect(shader);
        if(!check(blocks.size()==1&&blocks[0].name=="Data"&&blocks[0].size==240&&blocks[0].slot==0&&blocks[0].members.size()==10&&before==State{},"std140 reflection and program preservation"))return false;
        struct Member{const char*name;int offset,size,type,rows,columns,elements;};
        const Member expected[]={{"v",0,12,0,1,3,0},{"tail",12,4,0,1,1,0},{"a",16,20,0,1,1,2},
            {"cm",48,28,0,3,2,0},{"rm",80,40,0,3,2,0},{"matrices",128,56,0,2,2,2},
            {"uv",192,8,2,1,2,0},{"iv",200,8,1,1,2,0},{"bv",208,8,3,1,2,0},{"one",224,4,0,1,1,1}};
        for(const auto&e:expected)
        {
            const auto it=std::find_if(blocks[0].members.begin(),blocks[0].members.end(),[&](const auto&m){return m.name==e.name;});
            if(!check(it!=blocks[0].members.end()&&it->offset==e.offset&&it->size==e.size&&it->type==e.type&&it->rows==e.rows&&it->columns==e.columns&&it->elements==e.elements,e.name))return false;
        }
        std::array<float,4>prefix{99,99,99,9};
        if(!check(igpu_buffer_write(buffer,0,gm::wire::GMBuffer(prefix.data(),sizeof(prefix))),"ordinary partial uniform upload")||
            !t.write(buffer,shader,"v",std::array<std::int32_t,3>{3,4,5})||!t.write(buffer,shader,"a",std::array<std::int32_t,2>{11,13})||
            !t.write(buffer,shader,"cm",std::array<std::int32_t,6>{1,2,3,4,5,6})||!t.write(buffer,shader,"rm",std::array<std::int32_t,6>{7,8,9,10,11,12})||
            !t.write(buffer,shader,"matrices",std::array<std::int32_t,8>{1,2,3,4,5,6,7,8})||!t.write(buffer,shader,"uv",std::array<std::int32_t,2>{101,103})||
            !t.write(buffer,shader,"iv",std::array<std::int32_t,2>{-7,9})||!t.write(buffer,shader,"bv",std::array<std::int32_t,2>{0,2})||
            !t.write(buffer,shader,"one",std::array<std::int32_t,1>{29}))return false;
        const auto want=std::array<std::uint32_t,20>{3,4,5,9,11,13,3,5,8,11,3,6,8,101,103,13,9,0,1,29};
        if(!check(igpu_shader_bind(shader,2)&&igpu_uniform_bind(buffer,2,0),"layout bind"))return false;
        const State claimed;
        if(!check(t.dispatch(output)&&claimed==State{},"matrix/array dispatch state")||!t.read(output,want))return false;
        const auto cached=igpu_shader_reflect(shader);
        if(!check(cached.size()==1&&cached[0].members.size()==10&&claimed==State{},"cached reflection"))return false;
        if(!check(igpu_buffer_resize(buffer,256)&&t.dispatch(output),"dynamic uniform growth")||!t.read(output,want))return false;
        igpu::GlUniformBuffer native;
        if(!igpu::gl_uniform_buffer(buffer,native,"test"))return false;
        auto&f=igpu_gl_fns();GLint saved=0;glGetIntegerv(0x8894,&saved);f.BindBuffer(0x8892,native.name);
        std::array<unsigned char,16>zero{};zero.fill(0xFF);f.GetBufferSubData(0x8892,240,16,zero.data());f.BindBuffer(0x8892,saved);
        if(!check(zero==std::array<unsigned char,16>{},"new uniform bytes zeroed")||!check(igpu_buffer_resize(buffer,16),"uniform shrink"))return false;
        if(!t.rejected([&]{const auto w=wire(std::array<std::int32_t,3>{1,2,3});return igpu_uniform_write(buffer,shader,"Data","v",view(w));},"buffer smaller than reflected block"))return false;
        if(!check(igpu_buffer_resize(buffer,240)&&t.dispatch(output),"uniform regrowth")||
            !t.read(output,std::array<std::uint32_t,20>{3,4,5,9,0,0,0,0,0,0,0,0,0,0,0,20,0,0,0,0}))return false;
        if(!check(igpu_uniform_bind(0,2,0)&&before==State{},"restore original range after resized claim"))return false;
        std::printf("uniform layouts: native metadata, scalar/vector/int/uint/bool, row/column matrices, arrays and vec3 neighbor bytes passed\n");
        std::printf("uniform lifecycle: zero/partial upload, GPU-preserving grow/shrink/regrow and cached reflection passed\n");return true;
    }
    bool small_packed_blocks()
    {
        Fixture t;
        for(int count=1;count<=3;++count)
        {
            std::string source="#version 430\nlayout(local_size_x=1)in;layout(packed,binding=16)uniform Data{";
            for(int i=0;i<count;++i)source+="float v"+std::to_string(i)+";";
            source+="};layout(std430,binding=0)buffer Output{uint r[];};void main(){";
            for(int i=0;i<count;++i)source+="r["+std::to_string(i)+"]=uint(v"+std::to_string(i)+");";
            source+="}";
            const auto shader=t.shader(source);const auto blocks=igpu_shader_reflect(shader);
            if(!check(blocks.size()==1,"small packed reflection"))return false;
            const auto buffer=t.buffer(blocks[0].size),output=t.buffer(count*4,8,0,4);
            if(!check(buffer&&output,"allocate exact reflected packed block size"))return false;
            for(int i=0;i<count;++i)
                if(!t.write(buffer,shader,("v"+std::to_string(i)).c_str(),std::array<std::int32_t,1>{7+3*i}))return false;
            if(!check(igpu_uniform_bind(buffer,2,0)&&igpu_shader_bind(shader,2)&&t.dispatch(output),"small packed dispatch"))return false;
            std::array<std::uint32_t,3> actual{};
            if(!check(igpu_buffer_read(output,0,gm::wire::GMBuffer(actual.data(),count*4)),"small packed read"))return false;
            for(int i=0;i<count;++i)if(!check(actual[i]==7+3*i,"small packed literal GPU word"))return false;
            if(!check(igpu_buffer_resize(buffer,blocks[0].size+4)&&igpu_buffer_resize(buffer,blocks[0].size),"small uniform resize"))return false;
            std::printf("uniform small packed: %d active scalars, reflected %lld bytes, exact allocation and dispatch passed\n",count,static_cast<long long>(blocks[0].size));
        }
        return true;
    }
    bool packed_and_struct_tests()
    {
        Fixture t;
        const auto shader=t.shader("#version 430\nlayout(local_size_x=1)in;layout(packed,binding=16)uniform Data{float a[3];mat2 m;vec3 v;float tail;};"
            "layout(std430,binding=0)buffer Output{uint r[];};void main(){r[0]=uint(a[0]+a[1]+a[2]);r[1]=uint(m[0][1]+m[1][0]);r[2]=uint(v.x+v.y+v.z+tail);}");
        const auto blocks=igpu_shader_reflect(shader);
        if(!check(blocks.size()==1,"packed block reflection"))return false;
        const auto buffer=t.buffer(blocks[0].size),output=t.buffer(12,8,0,4);
        if(!check(buffer&&output,"packed buffers")||!t.write(buffer,shader,"a",std::array<std::int32_t,3>{5,7,9})||
            !t.write(buffer,shader,"m",std::array<std::int32_t,4>{1,2,3,4})||!t.write(buffer,shader,"tail",std::array<std::int32_t,1>{17})||
            !t.write(buffer,shader,"v",std::array<std::int32_t,3>{2,3,4})||!check(igpu_uniform_bind(buffer,2,0)&&igpu_shader_bind(shader,2)&&t.dispatch(output),"packed dispatch")||
            !t.read(output,std::array<std::uint32_t,3>{21,5,26}))return false;
        const auto nested=t.shader("#version 430\nlayout(local_size_x=1)in;struct S{vec2 v;float tail;};layout(std140,binding=16)uniform Data{S s[2];};"
            "layout(std430,binding=0)buffer Output{uint r[];};void main(){r[0]=uint(s[0].v.x+s[0].v.y+s[0].tail);r[1]=uint(s[1].v.x+s[1].v.y+s[1].tail);}");
        const auto n=t.buffer(32);
        if(!check(nested&&n,"nested fixture")||!t.write(n,nested,"s[0].v",std::array<std::int32_t,2>{2,3})||!t.write(n,nested,"s[0].tail",std::array<std::int32_t,1>{5})||
            !t.write(n,nested,"s[1].v",std::array<std::int32_t,2>{7,11})||!t.write(n,nested,"s[1].tail",std::array<std::int32_t,1>{13})||
            !check(igpu_uniform_bind(n,2,0)&&igpu_shader_bind(nested,2)&&t.dispatch(output),"nested dispatch")||!t.read(output,std::array<std::uint32_t,3>{10,31,26}))return false;
        std::printf("uniform packing: implementation-chosen strides and struct-array leaf writes preserve actual GPU values\n");return true;
    }
    bool unknown_types()
    {
        Fixture t;const auto shader=t.shader("#version 430\nlayout(local_size_x=1)in;layout(std140,binding=16)uniform Data{double d;dvec2 v;dmat2 m;};"
            "layout(std430,binding=0)buffer O{uint r;};void main(){r=uint(d+v.x+v.y+m[0][0]+m[1][1]);}");
        const auto blocks=igpu_shader_reflect(shader);if(!check(blocks.size()==1&&blocks[0].members.size()==3&&blocks[0].size==64,"double block metadata"))return false;
        for(const auto&m:blocks[0].members)
        {
            const auto size=m.name=="d"?8:m.name=="v"?16:32;
            const auto columns=m.name=="d"?1:2;
            if(!check(m.type==5&&m.size==size&&m.columns==columns,"unsupported double keeps accurate native dimensions and byte span"))return false;
        }
        const auto buffer=t.buffer(64);const auto values=wire(std::array<std::int32_t,1>{1});
        if(!t.rejected([&]{return igpu_uniform_write(buffer,shader,"Data","d",view(values));},"unsupported double write"))return false;
        return true;
    }
    bool eight_slots()
    {
        Fixture t;std::string source="#version 430\nlayout(local_size_x=1)in;";
        std::array<Handle,8>buffers{};
        for(int i=0;i<8;++i)source+="layout(std140,binding="+std::to_string(16+i)+")uniform C"+std::to_string(i)+"{uint v"+std::to_string(i)+";};";
        source+="layout(std430,binding=0)buffer O{uint r;};void main(){r=v0+v1+v2+v3+v4+v5+v6+v7;}";
        const auto shader=t.shader(source),output=t.buffer(4,8,0,4);
        if(!check(shader&&output&&t.sentinels(),"eight UBO fixtures"))return false;
        const State before;const auto blocks=igpu_shader_reflect(shader);
        if(!check(blocks.size()==8,"eight UBO reflection"))return false;
        for(int i=0;i<8;++i)
        {
            buffers[i]=t.buffer(16);const auto block="C"+std::to_string(i),member="v"+std::to_string(i);
            const auto it=std::find_if(blocks.begin(),blocks.end(),[&](const auto&b){return b.name==block;});
            if(!check(it!=blocks.end()&&it->slot==i&&buffers[i],"eight logical slots")||
                !t.write(buffers[i],shader,member.c_str(),std::array<std::int32_t,1>{i+1},block.c_str())||!check(igpu_uniform_bind(buffers[i],2,i),"eight physical binds"))return false;
        }
        const State claimed;
        if(!check(igpu_shader_bind(shader,2)&&t.dispatch(output)&&claimed==State{},"eight UBO dispatch")||!t.read(output,std::array<std::uint32_t,1>{36}))return false;
        for(int i=0;i<8;++i)if(!igpu_uniform_bind(0,2,i))return false;
        if(!check(before==State{},"eight UBO original ranges restored"))return false;
        std::printf("uniform slots: eight compute block bindings and literal GPU sum36 passed\n");return true;
    }
    bool graphics_inputs()
    {
        Fixture t;auto&f=igpu_gl_fns();
        const char*vs="#version 430\nlayout(location=0)in vec2 in_pos;layout(std140,binding=0)uniform VData{vec4 shift;};"
            "void main(){gl_Position=vec4(in_pos+vec2(shift.x-7.0,0),0,1);}";
        const char*ps="#version 430\nlayout(std140,binding=8)uniform PData{vec4 colour;};uniform sampler2D igpu_tex;out vec4 out_colour;void main(){out_colour=colour/255.0+texelFetch(igpu_tex,ivec2(0),0)/1024.0;}";
        const auto vertex_shader=igpu_shader_compile(vs,"main",0,"glsl"),pixel_shader=igpu_shader_compile(ps,"main",1,"glsl");
        const auto vertex_uniform=t.buffer(16,4,0),pixel_uniform=t.buffer(16);
        const State reflect_before;
        const auto vertex_layout=igpu_shader_reflect(vertex_shader);const auto vertex_error=igpu_get_last_error();
        const auto pixel_layout=igpu_shader_reflect(pixel_shader);const auto pixel_error=igpu_get_last_error();
        bool metadata=vertex_shader&&pixel_shader&&vertex_uniform&&pixel_uniform&&vertex_layout.size()==1&&pixel_layout.size()==1&&
            vertex_layout[0].name=="VData"&&vertex_layout[0].slot==0&&pixel_layout[0].name=="PData"&&pixel_layout[0].slot==0&&reflect_before==State{};
        metadata=metadata&&t.write(vertex_uniform,vertex_shader,"shift",std::array<std::int32_t,4>{7,10,13,16},"VData")&&
            t.write(pixel_uniform,pixel_shader,"colour",std::array<std::int32_t,4>{128,64,32,255},"PData");
        if(!metadata)std::fprintf(stderr,"uniform stage reflection: vertex=%s pixel=%s\n",vertex_error.c_str(),pixel_error.c_str());
        // Existing graphics release relinks the bound graphics pair; finish that before the draw fixture.
        if(vertex_shader)igpu_shader_release(vertex_shader);if(pixel_shader)igpu_shader_release(pixel_shader);
        glGetIntegerv(0x8B8D,&t.original.globals[2]);
        if(!check(metadata,"public vertex/pixel reflection and member writes"))return false;
        const auto vertex=t.buffer(32,1,1,8);std::array<float,8>positions{-1,-1,1,-1,-1,1,1,1};
        const auto layout=igpu_input_layout_create(igpu_get_bound_shader(0),view(wire(std::array<std::int32_t,1>{1})),
            view(wire(std::array<std::int32_t,1>{2})),view(wire(std::array<std::int32_t,1>{0})),1,8,0);
        const auto target=igpu_texture_create(0,4,4,1,6,true,false,1);
        const auto sample=igpu_texture_create(0,1,1,1,6,false,false,1);
        const auto program=f.CreateProgram();const char*sources[]={vs,ps};
        bool compiled=program!=0;
        for(int i=0;i<2;++i){const auto sh=f.CreateShader(i?0x8B30:0x8B31);f.ShaderSource(sh,1,&sources[i],nullptr);f.CompileShader(sh);GLint ok=0;f.GetShaderiv(sh,0x8B81,&ok);compiled=compiled&&ok;if(ok)f.AttachShader(program,sh);f.DeleteShader(sh);}
        f.LinkProgram(program);GLint linked=0;f.GetProgramiv(program,0x8B82,&linked);
        GLint read=0;glGetIntegerv(0x8CAA,&read);glPushAttrib(GL_ALL_ATTRIB_BITS);
        std::int32_t previous=0,viewport[4]{};const bool begun=igpu::gl_color_target_begin(target,previous,viewport);
        for(auto cap:{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_BLEND,GL_ALPHA_TEST,0x809D,0x8C89})glDisable(cap);
        GLint clips=0;glGetIntegerv(0x0D32,&clips);for(int i=0;i<clips;++i)glDisable(GL_CLIP_PLANE0+i);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        f.UseProgram(program);
        bool ok=vertex&&layout&&target&&compiled&&linked&&begun&&igpu_buffer_write(vertex,0,gm::wire::GMBuffer(positions.data(),sizeof(positions)))&&
            igpu_uniform_bind(vertex_uniform,0,0)&&igpu_uniform_bind(pixel_uniform,1,0);
        const State before;
        const auto query=igpu_query_create(0);
        ok=ok&&query&&igpu_query_begin(query)&&igpu_draw(vertex,0,layout,5,0,4,1,0,0,0,0);
        if(query){ok=igpu_query_end(query)&&ok;ok=igpu_query_result(query)==16&&ok;igpu_query_release(query);}
        ok=ok&&before==State{}&&igpu_texture_read(target,1,1,0,0)==0x204080;
        using Map=void*(APIENTRY*)(GLenum,std::ptrdiff_t,std::ptrdiff_t,GLbitfield);using Unmap=GLboolean(APIENTRY*)(GLenum);
        auto map=reinterpret_cast<Map>(wglGetProcAddress("glMapBufferRange"));auto unmap=reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        GLint array=0;glGetIntegerv(0x8894,&array);
        for(auto uniform:{vertex_uniform,pixel_uniform})
        {
            igpu::GlUniformBuffer native;ok=igpu::gl_uniform_buffer(uniform,native,"test")&&ok;
            f.BindBuffer(0x8892,native.name);void*mapped=map(0x8892,0,16,1);
            if(mapped)
            {
                ok=t.rejected([&]{return igpu_draw(vertex,0,layout,5,0,4,1,0,0,0,0);},"late mapped graphics UBO before draw")&&ok;
                ok=t.rejected([&]{return igpu_draw_sampled(vertex,layout,5,0,4,sample,0,0,0,0);},"late mapped graphics UBO before sampled draw")&&ok;
                ok=igpu_get_last_error().find("mapped")!=std::string::npos&&ok;
                unmap(0x8892);
            }
            else ok=false;
        }
        f.BindBuffer(0x8892,array);
        ok=igpu_draw_sampled(vertex,layout,5,0,4,sample,0,0,0,0)&&ok;
        ok=igpu_texture_read(target,1,1,0,0)==0x204080&&ok;
        const auto error=glGetError();
        if(begun)igpu::gl_color_target_end(previous,viewport);f.BindFramebuffer(0x8CA8,read);glPopAttrib();
        igpu_uniform_bind(0,0,0);igpu_uniform_bind(0,1,0);igpu_input_layout_release(layout);igpu_texture_release(target);igpu_texture_release(sample);
        t.original.restore();f.DeleteProgram(program);
        if(!check(ok&&!error,"vertex/pixel UBO real pixels and sixteen samples"))return false;
        std::printf("uniform graphics: public vertex/pixel layouts, Static/Dynamic member writes, 16 samples and pixel128/64/32, mapped-input guard passed\n");return true;
    }
    bool capability_tests()
    {
        Fixture t;using igpu::Capability;auto&f=igpu_gl_fns();
        if(!check(igpu::supports(Capability::UniformBuffer)&&igpu::supports(Capability::UniformReflection),"uniform capability completeness"))return false;
        const auto shader=t.shader("#version 430\nlayout(local_size_x=1)in;layout(std140,binding=16)uniform Data{uint v;};layout(std430,binding=0)buffer O{uint r;};void main(){r=v;}");
        const auto buffer=t.buffer(16);
        const auto legacy=igpu_shader_compile("#version 120\nattribute vec2 in_pos;void main(){gl_Position=vec4(in_pos,0,1);}","main",0,"glsl");
        const auto legacy_blocks=igpu_shader_reflect(legacy);const bool legacy_ok=legacy&&legacy_blocks.empty()&&igpu_get_last_error().empty();
        if(legacy)igpu_shader_release(legacy);glGetIntegerv(0x8B8D,&t.original.globals[2]);
        if(!check(legacy_ok,"legacy GLSL120 reflects empty blocks without changing compilation semantics"))return false;
        auto test=[&](auto&function,const char*label)
        {
            const auto saved=function;function=nullptr;
            const bool hidden=!igpu::supports(Capability::UniformBuffer)&&!igpu::supports(Capability::UniformReflection);
            const bool rejected=!igpu_uniform_bind(buffer,2,0)&&!igpu_get_last_error().empty();
            const auto blocks=igpu_shader_reflect(shader);const bool reflection=blocks.empty()&&!igpu_get_last_error().empty();
            function=saved;return check(hidden&&rejected&&reflection&&glGetError()==GL_NO_ERROR,label);
        };
        if(!test(f.ProgramParameteri,"missing separable program query")||!test(f.GetActiveUniformBlockiv,"missing block query")||
            !test(f.GetActiveUniformBlockName,"missing block name")||!test(f.GetActiveUniformsiv,"missing member offsets")||
            !test(f.BindBufferBase,"missing UBO base bind")||!test(f.BindBufferRange,"missing UBO range restore")||
            !test(f.GetIntegeri,"missing UBO name query")||!test(f.GetInteger64i,"missing UBO range query"))return false;
        std::printf("uniform capabilities: legacy shader compatibility and eight missing-entrypoint gates passed\n");return true;
    }
    bool guards_and_claims()
    {
        Fixture t;const auto shader=t.shader("#version 430\nlayout(local_size_x=1)in;layout(std140,binding=16)uniform Data{uvec4 v;};"
            "layout(std430,binding=0)buffer Output{uint r[];};void main(){r[0]=v.x;}");
        const auto a=t.buffer(16),b=t.buffer(32),wrong=t.buffer(16,8,0,4),output=t.buffer(4,8,0,4);
        if(!check(shader&&a&&b&&wrong&&output&&t.sentinels(),"guard fixtures"))return false;
        const State before;const auto w=wire(std::array<std::int32_t,4>{1,2,3,4});const auto shorter=wire(std::array<std::int32_t,1>{1});
        if(!t.rejected([&]{return igpu_uniform_write(wrong,shader,"Data","v",view(w));},"wrong uniform role")||
            !t.rejected([&]{return igpu_uniform_write(a,shader,"Data","missing",view(w));},"unknown member")||
            !t.rejected([&]{return igpu_uniform_write(a,shader,"Other","v",view(w));},"unknown block")||
            !t.rejected([&]{return igpu_uniform_write(a,shader,"Data","v",view(shorter));},"wrong value count")||
            !t.rejected([&]{return igpu_uniform_write(a,-1,"Data","v",view(w));},"unknown shader")||
            !t.rejected([&]{return igpu_uniform_bind(a,3,0);},"unsupported stage")||!t.rejected([&]{return igpu_uniform_bind(a,2,8);},"slot upper limit")||
            !t.rejected([&]{return igpu_uniform_bind(wrong,2,0);},"binding wrong role"))return false;
        const auto bad=t.shader("#version 430\nlayout(local_size_x=1)in;layout(std140,binding=0)uniform Bad{uint v;};layout(std430,binding=0)buffer O{uint r;};void main(){r=v;}");
        if(!t.rejected([&]{return !igpu_shader_reflect(bad).empty();},"block outside stage namespace"))return false;
        if(!check(igpu_uniform_bind(a,2,0)&&igpu_uniform_bind(b,2,0)&&igpu_uniform_bind(0,2,0)&&before==State{},"replacement/unbind restores original range"))return false;
        if(!check(igpu_uniform_write(a,shader,"Data","v",view(w))&&igpu_shader_bind(shader,2)&&igpu_uniform_bind(a,2,0),"mapped fixture bind"))return false;
        igpu::GlUniformBuffer native;if(!igpu::gl_uniform_buffer(a,native,"test"))return false;
        using Map=void*(APIENTRY*)(GLenum,std::ptrdiff_t,std::ptrdiff_t,GLbitfield);using Unmap=GLboolean(APIENTRY*)(GLenum);
        auto map=reinterpret_cast<Map>(wglGetProcAddress("glMapBufferRange"));auto unmap=reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        auto&f=igpu_gl_fns();f.BindBuffer(0x8892,native.name);if(!check(map(0x8892,0,16,1)!=nullptr,"map claimed uniform"))return false;
        const bool rejected=t.rejected([&]{return t.dispatch(output);},"late mapped uniform input")&&
            t.rejected([&]{return igpu_uniform_write(a,shader,"Data","v",view(w));},"mapped member write")&&
            t.rejected([&]{return igpu_uniform_bind(a,1,0);},"mapped uniform bind");
        unmap(0x8892);f.BindBuffer(0x8892,t.foreign);if(!rejected)return false;
        if(!check(t.dispatch(output),"dispatch after unmapping")||!t.read(output,std::array<std::uint32_t,1>{1}))return false;
        if(!check(igpu_uniform_bind(a,0,0)&&igpu_uniform_bind(a,1,7)&&t.release(a)&&before==State{},"release restores claims in every stage"))return false;
        if(!t.rejected([&]{return igpu_uniform_bind(a,2,0);},"released uniform handle"))return false;
        std::printf("uniform validation: roles/layouts/values/slots/stages/mappings, replacement and release range restoration passed\n");return true;
    }
}

bool run_gl_uniform_tests()
{
    return uniform_core_test()&&unknown_types()&&layout_tests()&&packed_and_struct_tests()&&small_packed_blocks()&&eight_slots()&&guards_and_claims()&&graphics_inputs()&&capability_tests();
}

bool run_gl_uniform_shutdown_tests()
{
    auto&f=igpu_gl_fns();f.UseProgram(0);const State original;
    const auto context=wglGetCurrentContext();
    GLuint foreign=0;f.GenBuffers(1,&foreign);f.BindBuffer(ubo,foreign);
    GLint alignment=0;glGetIntegerv(0x8A34,&alignment);
    std::vector<unsigned char>bytes(static_cast<std::size_t>(alignment)*2+64,0xB3);
    f.BufferData(ubo,bytes.size(),bytes.data(),0x88E4);
    for(GLuint slot:{0u,8u,16u,23u})f.BindBufferRange(ubo,slot,foreign,alignment,64);
    const auto shader=igpu_shader_compile("#version 430\nlayout(local_size_x=1)in;layout(std140,binding=16)uniform Data{uint v;};"
        "layout(std430,binding=0)buffer O{uint r;};void main(){r=v;}","main",2,"glsl");
    const auto buffer=igpu_buffer_create(16,1,4,0);
    igpu::GlUniformBuffer native;
    const auto value=wire(std::array<std::int32_t,1>{7});
    bool ok=shader&&buffer&&igpu_shader_reflect(shader).size()==1&&igpu_uniform_write(buffer,shader,"Data","v",view(value))&&
        igpu::gl_uniform_buffer(buffer,native,"test");
    const State before;
    ok=ok&&igpu_uniform_bind(buffer,0,0)&&igpu_uniform_bind(buffer,1,0)&&igpu_uniform_bind(buffer,2,0)&&igpu_uniform_bind(buffer,2,7);
    if(!check(ok,"uniform shutdown fixtures"))return false;
    igpu_shutdown();
    using Is=GLboolean(APIENTRY*)(GLuint);const auto is_buffer=reinterpret_cast<Is>(wglGetProcAddress("glIsBuffer"));
    ok=before==State{}&&is_buffer&&!is_buffer(native.name)&&!igpu_is_available()&&wglGetCurrentContext()==context;
    std::vector<unsigned char>actual(bytes.size());f.GetBufferSubData(ubo,0,actual.size(),actual.data());ok=ok&&actual==bytes&&glGetError()==GL_NO_ERROR;
    original.restore();f.DeleteBuffers(1,&foreign);
    if(!check(ok,"shutdown restores all UBO ranges and generic binding before deleting owned buffer"))return false;
    if(!check(igpu_bind_current()&&!igpu_uniform_bind(buffer,2,0)&&igpu_shader_reflect(shader).empty()&&!igpu_get_last_error().empty()&&
        igpu::supports(igpu::Capability::UniformBuffer)&&igpu::supports(igpu::Capability::UniformReflection),"fresh bind has no stale uniform claims or layouts"))return false;
    std::printf("uniform shutdown: three-stage ranges/caller bytes restored, owned buffer deleted, cached layouts cleared and borrowed context preserved\n");return true;
}
