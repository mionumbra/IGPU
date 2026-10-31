#include "buffer_tests.h"
#include "IGPU_native.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using Handle=std::int64_t;
    using Bytes=std::vector<unsigned char>;
    using ReadGpu=void(APIENTRY*)(GLenum,std::ptrdiff_t,std::ptrdiff_t,void*);
    using GetParameter=void(APIENTRY*)(GLenum,GLenum,GLint*);
    constexpr GLenum array_buffer=0x8892, array_binding=0x8894;
    constexpr GLenum binding_fields[]={0x8894,0x85B5,0x8895,0x8F36,0x8F37};
    bool state_ok=true;
    struct Bindings
    {
        std::array<GLint,5> values{};
        Bindings() { for (int i=0;i<5;++i) glGetIntegerv(binding_fields[i],&values[i]); }
        bool operator==(const Bindings& b) const
        {
            for (int i=0;i<5;++i) if (values[i]!=b.values[i])
                std::fprintf(stderr,"buffer binding 0x%x changed %d -> %d\n",binding_fields[i],values[i],b.values[i]);
            return values==b.values;
        }
    };
    template<class F> auto api(F action)
    {
        const Bindings before; const auto result=action();
        state_ok=(before==Bindings{}) && state_ok; return result;
    }
    struct Caller
    {
        Bindings saved;
        GLuint vao=0, buffers[4]{};
        Caller()
        {
            auto& f=igpu_gl_fns(); f.GenVertexArrays(1,&vao); f.BindVertexArray(vao); f.GenBuffers(4,buffers);
            const GLenum targets[]={0x8892,0x8893,0x8F36,0x8F37}; const Bytes sentinel(64,0xA7);
            for (int i=0;i<4;++i) { f.BindBuffer(targets[i],buffers[i]); f.BufferData(targets[i],sentinel.size(),sentinel.data(),0x88E4); }
        }
        ~Caller()
        {
            auto& f=igpu_gl_fns(); f.BindVertexArray(saved.values[1]); f.BindBuffer(0x8892,saved.values[0]);
            f.BindBuffer(0x8F36,saved.values[3]); f.BindBuffer(0x8F37,saved.values[4]);
            f.DeleteBuffers(4,buffers); f.DeleteVertexArrays(1,&vao);
        }
    };
    IgpuGlGenBuffers real_gen=nullptr;
    GLuint generated_name=0;
    void APIENTRY observe_gen(GLsizei count,GLuint* names)
    {
        real_gen(count,names);
        if (count>0) generated_name=names[count-1];
    }
    struct ObserveCreation
    {
        IgpuGlGenBuffers previous=igpu_gl_fns().GenBuffers;
        ObserveCreation() { real_gen=previous; generated_name=0; igpu_gl_fns().GenBuffers=observe_gen; }
        ~ObserveCreation() { igpu_gl_fns().GenBuffers=previous; real_gen=nullptr; }
    };
    bool check(bool result,const char* label)
    {
        if (!result) std::fprintf(stderr,"buffer lifecycle: %s: %s\n",label,igpu_get_last_error().c_str());
        return result;
    }
    struct Buffer { Handle handle=0; GLuint name=0; };
    struct Probe
    {
        std::vector<Buffer> owned;
        ReadGpu read_gpu=reinterpret_cast<ReadGpu>(wglGetProcAddress("glGetBufferSubData"));
        GetParameter get_parameter=reinterpret_cast<GetParameter>(wglGetProcAddress("glGetBufferParameteriv"));
        ~Probe() { for (auto b:owned) if (b.handle) api([&]{return igpu_buffer_release(b.handle);}); }
        Buffer create(std::int64_t size,int usage,int bind,int stride)
        {
            ObserveCreation observer;
            Buffer b; b.handle=api([&]{return igpu_buffer_create(size,usage,bind,stride);}); b.name=generated_name;
            if (b.handle) owned.push_back(b);
            return b;
        }
        bool write(Buffer b,std::int64_t offset,Bytes& bytes)
        {
            return api([&]{return igpu_buffer_write(b.handle,offset,gm::wire::GMBuffer(bytes.data(),bytes.size()));});
        }
        Handle size(Buffer b) { return api([&]{return igpu_buffer_size(b.handle);}); }
        bool resize(Buffer b,Handle size) { return api([&]{return igpu_buffer_resize(b.handle,size);}); }
        bool release(Buffer b)
        {
            const bool result=api([&]{return igpu_buffer_release(b.handle);});
            if (result) for (auto& entry:owned) if (entry.handle==b.handle) entry.handle=0;
            return result;
        }
        bool rejected_read(Buffer b,Handle offset,std::size_t count)
        {
            Bytes bytes(count ? count : 1,0xB6); const Bytes before=bytes;
            const bool read=api([&]{return igpu_buffer_read(b.handle,offset,gm::wire::GMBuffer(bytes.data(),count));});
            return check(!read && bytes==before && !igpu_get_last_error().empty(),"invalid read preserves destination");
        }
        bool gpu_equals(Buffer b,const Bytes& expected,const char* label)
        {
            if (!read_gpu || !get_parameter || !b.name) return check(false,"GPU readback entry points/name");
            GLint previous=0,size=0; glGetIntegerv(array_binding,&previous);
            igpu_gl_fns().BindBuffer(array_buffer,b.name); get_parameter(array_buffer,0x8764,&size);
            Bytes actual(expected.size(),0xCD);
            if (size==static_cast<GLint>(expected.size())) read_gpu(array_buffer,0,actual.size(),actual.data());
            igpu_gl_fns().BindBuffer(array_buffer,previous);
            if (size!=static_cast<GLint>(expected.size()) || actual!=expected)
            {
                std::size_t index=0; while (index<actual.size() && actual[index]==expected[index]) ++index;
                std::fprintf(stderr,"buffer %lld GL %u: %s size=%d expected=%zu first mismatch=%zu actual=%u expected=%u\n",
                    static_cast<long long>(b.handle),b.name,label,size,expected.size(),index,
                    index<actual.size()?actual[index]:0,index<expected.size()?expected[index]:0);
                return false;
            }
            return check(glGetError()==GL_NO_ERROR,label);
        }
        bool public_equals(Buffer b,std::int64_t offset,const Bytes& expected)
        {
            Bytes actual(expected.size(),0xCD);
            const bool read=api([&]{return igpu_buffer_read(b.handle,offset,gm::wire::GMBuffer(actual.data(),actual.size()));});
            return check(read && actual==expected,"public GPU byte read");
        }
    };
    bool dynamic_core(Probe& p,int bind,int stride,int initial,int grown,int shrunk)
    {
        const auto b=p.create(initial,1,bind,stride);
        if (!check(b.handle!=0,"dynamic creation") || !check(p.size(b)==initial,"initial size")) return false;
        Bytes expected(initial,0);
        if (!p.gpu_equals(b,expected,"zero allocation")) return false;
        for (int i=0;i<initial;++i) expected[i]=static_cast<unsigned char>(i*13+7);
        if (!check(p.write(b,0,expected),"initial public write")) return false;
        Bytes patch={0xF1,0x02,0xE3,0x04,0xD5};
        if (!check(p.write(b,3,patch),"unaligned partial byte update")) return false;
        std::copy(patch.begin(),patch.end(),expected.begin()+3);
        if (!p.gpu_equals(b,expected,"partial update retains surrounding bytes")) return false;
        if (!check(p.resize(b,grown) && p.size(b)==grown,"dynamic grow")) return false;
        expected.resize(grown,0);
        if (!p.gpu_equals(b,expected,"same GL object preserves overlap and zeroes growth")) return false;
        if (!check(p.resize(b,shrunk) && p.size(b)==shrunk,"dynamic shrink")) return false;
        expected.resize(shrunk);
        return p.gpu_equals(b,expected,"shrink keeps prefix and GL object identity");
    }
    struct Allocation { int usage,bind,stride,size; };
    bool allocation_matrix(Probe& p)
    {
        const Allocation valid[]={
            {0,1,0,24},{0,1,8,24},{0,1,12,36},{0,1,16,48},
            {1,1,0,24},{1,1,8,24},{1,1,12,36},{1,1,16,48},{0,2,0,12},{1,2,0,12},
            {0,4,0,32},{1,4,0,32},{0,8,4,32},{1,8,4,32},{0,8,12,36},{1,8,12,36},
            {0,8,2048,4096},{1,8,2048,4096},{0,16,0,16},{2,0,0,32}
        };
        for (const auto c:valid)
        {
            const auto b=p.create(c.size,c.usage,c.bind,c.stride); Bytes expected(c.size,0);
            if (!check(b.handle && p.size(b)==c.size,"valid allocation/size") || !p.gpu_equals(b,expected,"all roles start zeroed")) return false;
            if (c.bind==0) { if (!p.public_equals(b,0,expected)) return false; continue; }
            for (std::size_t i=0;i<expected.size();++i) expected[i]=static_cast<unsigned char>(i*17+c.bind);
            const Bytes source=expected;
            if (!check(p.write(b,0,expected) && expected==source,"write leaves CPU source intact")) return false;
            Bytes patch={0xA1,0xB2,0xC3};
            if (c.bind==16)
            {
                Bytes partial(8,0x6D);
                if (!check(!p.write(b,0,partial) && !p.write(b,4,partial),"Indirect rejects short and offset updates")) return false;
            }
            else
            {
                if (!check(p.write(b,3,patch),"partial role update")) return false;
                std::copy(patch.begin(),patch.end(),expected.begin()+3);
            }
            if (!p.gpu_equals(b,expected,"role GPU write contents")) return false;
            if (c.bind==8) { if (!p.public_equals(b,0,expected) || !p.public_equals(b,3,patch)) return false; }
            else if (!p.rejected_read(b,0,4)) return false;
            if (c.usage==1)
            {
                if (!check(!p.resize(b,c.size+1) && p.size(b)==c.size,"Dynamic role alignment rejected")) return false;
                if (!check(p.resize(b,c.size*2),"Dynamic role growth")) return false;
                expected.resize(c.size*2,0);
                if (!p.gpu_equals(b,expected,"role growth overlap/zero/object identity")) return false;
                if (!check(p.resize(b,c.size) && p.resize(b,c.size),"role shrink and same-size resize")) return false;
                expected.resize(c.size);
            }
            else if (!check(!p.resize(b,c.size*2),"Static resize rejected")) return false;
            if (!p.gpu_equals(b,expected,"resize preserved role bytes")) return false;
        }
        const Allocation invalid[]={
            {-1,1,8,32},{3,1,8,32},{0,0,0,32},{1,0,0,32},{2,1,8,32},{2,2,0,32},{2,4,0,32},{2,8,4,32},{2,16,0,32},
            {0,3,0,32},{0,5,0,32},{0,12,0,32},{0,32,0,32},{0,-1,0,32},{0,1,4,32},{0,1,20,40},{0,1,8,12},
            {0,2,0,3},{0,2,2,32},{0,4,0,15},{0,4,16,32},{0,8,0,32},{0,8,2,32},{0,8,6,36},{0,8,2052,4104},
            {0,8,4,14},{1,16,0,16},{0,16,4,16},{0,16,0,12},{0,16,0,18}
        };
        for (const auto c:invalid) if (!check(!p.create(c.size,c.usage,c.bind,c.stride).handle && !igpu_get_last_error().empty(),"invalid allocation rejected")) return false;
        for (Handle size:{Handle{0},Handle{-1},std::numeric_limits<Handle>::max(),Handle{2147483648LL}})
            if (!check(!p.create(size,1,1,8).handle,"empty/negative/oversized allocation")) return false;
        std::printf("buffer lifecycle: 20 valid role allocations and 34 invalid allocations passed\n");
        return true;
    }
    bool invalid_operations(Probe& p,Buffer storage,Buffer staging,Bytes expected)
    {
        const auto dynamic=p.create(32,1,8,4); Bytes seed(32,0x76), patch={9,8,7,6}, empty;
        if (!check(dynamic.handle && p.write(dynamic,0,seed),"invalid-operation fixture")) return false;
        for (Handle offset:{Handle{-1},Handle{30},Handle{32},std::numeric_limits<Handle>::max()})
        {
            const Bytes source=patch;
            if (!check(!p.write(storage,offset,patch) && patch==source,"invalid write preserves CPU source") ||
                !p.rejected_read(storage,offset,4)) return false;
        }
        if (!check(!p.write(storage,0,empty),"empty write rejected") || !p.rejected_read(storage,0,0)) return false;
        for (Handle size:{Handle{0},Handle{-1},Handle{30},std::numeric_limits<Handle>::max(),Handle{2147483648LL}})
            if (!check(!p.resize(dynamic,size) && p.size(dynamic)==32,"invalid resize/size unchanged")) return false;
        if (!check(!p.resize(staging,64),"Staging resize rejected") || !p.gpu_equals(dynamic,seed,"invalid resize retains GPU bytes")) return false;
        const auto released=p.create(16,0,8,4);
        if (!check(released.handle && p.release(released),"released fixture")) return false;
        for (const auto b:{Buffer{},released,Buffer{std::numeric_limits<Handle>::max(),0}})
        {
            if (!check(p.size(b)==0 && !igpu_get_last_error().empty(),"unknown/released size error") ||
                !check(!p.write(b,0,patch) && !p.resize(b,32) && !p.release(b),"unknown/released operations") ||
                !p.rejected_read(b,0,4)) return false;
        }
        return p.public_equals(storage,0,expected) && p.gpu_equals(storage,expected,"invalid calls preserve storage GPU bytes");
    }
    struct Mapping
    {
        using Map=void*(APIENTRY*)(GLenum,GLenum);
        using Unmap=GLboolean(APIENTRY*)(GLenum);
        GLuint name;
        Unmap unmap=reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        bool active=false;
        explicit Mapping(GLuint buffer):name(buffer)
        {
            const auto map=reinterpret_cast<Map>(wglGetProcAddress("glMapBuffer"));
            if (!map || !unmap) return;
            GLint previous=0; glGetIntegerv(array_binding,&previous); igpu_gl_fns().BindBuffer(array_buffer,name);
            active=map(array_buffer,0x88BA)!=nullptr; igpu_gl_fns().BindBuffer(array_buffer,previous);
        }
        bool close()
        {
            if (!active) return false;
            GLint previous=0; glGetIntegerv(array_binding,&previous); igpu_gl_fns().BindBuffer(array_buffer,name);
            const bool ok=unmap(array_buffer)==GL_TRUE; active=false; igpu_gl_fns().BindBuffer(array_buffer,previous); return ok;
        }
        ~Mapping() { if (active) close(); }
    };
    bool unavailable_operations(Probe& p)
    {
        const auto b=p.create(32,1,8,4); Bytes expected(32,0x4C),patch={1,2,3,4};
        if (!check(b.handle && p.write(b,0,expected),"guard fixture write")) return false;
        {
            Mapping mapping(b.name);
            if (!check(mapping.active,"native GL_READ_WRITE mapping")) return false;
            const Bytes source=patch;
            if (!check(!p.write(b,4,patch) && patch==source && !p.resize(b,64) && p.size(b)==32,"mapped write/resize rejected") ||
                !p.rejected_read(b,0,4) || !check(mapping.close(),"unmap mapped guard")) return false;
        }
        if (!p.public_equals(b,0,expected) || !p.gpu_equals(b,expected,"mapped operations preserved bytes")) return false;
        {
            struct CopyGuard
            {
                IgpuGlCopyBufferSubData saved=igpu_gl_fns().CopyBufferSubData;
                CopyGuard() { igpu_gl_fns().CopyBufferSubData=nullptr; }
                ~CopyGuard() { igpu_gl_fns().CopyBufferSubData=saved; }
            } unavailable;
            if (!check(!igpu_supports(51) && igpu_supports(52),"capabilities follow optional copy support")) return false;
            if (!check(!p.resize(b,64) && p.size(b)==32,"resize rejects absent CopyBufferSubData") ||
                !p.gpu_equals(b,expected,"unavailable copy preserves object/size/contents")) return false;
        }
        return p.public_equals(b,0,expected);
    }
    template<class T,std::size_t N> Bytes byte_copy(const T (&values)[N])
    {
        const auto* first=reinterpret_cast<const unsigned char*>(values);
        return Bytes(first,first+sizeof(values));
    }
    std::array<std::byte,7> wire_i32(std::int32_t value)
    {
        std::array<std::byte,7> bytes{}; bytes[0]=std::byte{1}; bytes[2]=std::byte{6};
        std::memcpy(bytes.data()+3,&value,4); return bytes;
    }
    std::array<std::byte,11> wire_u64(std::uint64_t value)
    {
        std::array<std::byte,11> bytes{}; bytes[0]=std::byte{1}; bytes[2]=std::byte{12};
        std::memcpy(bytes.data()+3,&value,8); return bytes;
    }
    template<std::size_t N> gm::wire::GMArrayView view(const std::array<std::byte,N>& bytes)
    {
        return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array,bytes.data()));
    }
    struct Drawing
    {
        Handle shader=0,layout=0,target=0,source=0;
        GLuint program=0;
        ~Drawing()
        {
            if (program) igpu_gl_fns().DeleteProgram(program);
            if (layout) igpu_input_layout_release(layout);
            if (target) igpu_texture_release(target);
            if (source) igpu_texture_release(source);
            if (shader) igpu_shader_release(shader);
        }
        bool initialize()
        {
            const char* sources[]={"#version 120\nattribute vec2 in_pos; void main(){gl_Position=vec4(in_pos,0,1);}",
                                   "#version 120\nuniform sampler2D igpu_tex; uniform int sample_mode; void main(){gl_FragColor=sample_mode==1?texture2D(igpu_tex,vec2(0.5)):vec4(1,0,0,1);}"};
            shader=igpu_shader_compile(sources[0],"main",0,"glsl");
            const auto usage=wire_i32(1),type=wire_i32(2),step=wire_i32(0);
            layout=igpu_input_layout_create(shader,view(usage),view(type),view(step),1,8,0);
            target=igpu_texture_create(0,8,8,1,6,true,false,1);
            source=igpu_texture_create(0,2,2,1,6,true,false,1);
            auto& f=igpu_gl_fns(); program=f.CreateProgram();
            for (int i=0;i<2;++i)
            {
                const auto s=f.CreateShader(i ? 0x8B30 : 0x8B31);
                f.ShaderSource(s,1,&sources[i],nullptr); f.CompileShader(s);
                GLint ok=0; f.GetShaderiv(s,0x8B81,&ok);
                if (!ok) { char log[512]{}; f.GetShaderInfoLog(s,512,nullptr,log); std::fprintf(stderr,"buffer draw shader: %s\n",log); f.DeleteShader(s); return false; }
                f.AttachShader(program,s); f.DeleteShader(s);
            }
            f.BindAttribLocation(program,0,"in_pos"); f.LinkProgram(program);
            GLint linked=0; f.GetProgramiv(program,0x8B82,&linked);
            if (!(shader && layout && target && source && linked)) return false;
            std::int32_t saved=0,viewport[4]{};
            if (!igpu::gl_color_target_begin(source,saved,viewport)) return false;
            glPushAttrib(GL_COLOR_BUFFER_BIT|GL_ENABLE_BIT); glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glClearColor(1,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            glPopAttrib(); igpu::gl_color_target_end(saved,viewport); return glGetError()==GL_NO_ERROR;
        }
        bool pixels(Buffer vertex,Buffer index,int count,int left,int right,int path=0)
        {
            std::int32_t saved_fbo=0,viewport[4]{}; GLint old_program=0,pack_buffer=0,pack[4]{};
            auto& f=igpu_gl_fns();
            if (!igpu::gl_color_target_begin(target,saved_fbo,viewport)) return false;
            glGetIntegerv(0x8B8D,&old_program); glGetIntegerv(0x88ED,&pack_buffer);
            const GLenum pack_fields[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS};
            for (int i=0;i<4;++i) { glGetIntegerv(pack_fields[i],&pack[i]); glPixelStorei(pack_fields[i],i ? 0 : 1); }
            f.BindBuffer(0x88EB,0);
            glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_SCISSOR_BIT);
            glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            f.UseProgram(program); f.Uniform1i(f.GetUniformLocation(program,"sample_mode"),path==2 ? 1 : 0);
            const auto targets=wire_u64(target); const auto zero=wire_i32(0);
            const bool drew=api([&]{
                if (path==1) return igpu_draw_to_render_targets(vertex.handle,layout,4,0,count,view(targets),view(zero),view(zero),0,0,0,0);
                if (path==2) return igpu_draw_sampled(vertex.handle,layout,4,0,count,source,0,0,0,0);
                return index.handle ? igpu_draw_indexed(vertex.handle,layout,index.handle,4,0,count,0,0,0,0) :
                    igpu_draw(vertex.handle,0,layout,4,0,count,1,0,0,0,0); });
            unsigned char a[4]{},b[4]{}; glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,a); glReadPixels(6,6,1,1,GL_RGBA,GL_UNSIGNED_BYTE,b);
            glPopAttrib(); igpu::gl_color_target_end(saved_fbo,viewport); f.UseProgram(old_program); f.BindBuffer(0x88EB,pack_buffer);
            for (int i=0;i<4;++i) glPixelStorei(pack_fields[i],pack[i]);
            if (!drew || a[0]!=left || b[0]!=right)
                std::fprintf(stderr,"buffer draw: path=%d indexed=%d count=%d pixels=%u/%u expected=%d/%d\n",path,index.handle!=0,count,a[0],b[0],left,right);
            return check(drew && a[0]==left && b[0]==right && glGetError()==GL_NO_ERROR,"rendered lifecycle pixels");
        }
    };
    bool drawing_regression(Probe& p)
    {
        Drawing draw;
        if (!check(draw.initialize(),"render setup")) return false;
        const auto vertex=p.create(48,1,1,8),index=p.create(12,1,2,0);
        const float positions[]={-1,-1, 0,-1, -1,1, 0,-1, 0,1, -1,1};
        const std::uint16_t indices[]={0,1,2,3,4,5};
        auto vertices=byte_copy(positions),elements=byte_copy(indices);
        if (!check(vertex.handle && index.handle && p.write(vertex,0,vertices) && p.write(index,0,elements),"render data")) return false;
        if (!draw.pixels(vertex,{},6,255,0) || !draw.pixels(vertex,index,6,255,0)) return false;
        const float one[]={1.f}; auto patch=byte_copy(one);
        for (Handle offset:{Handle{8},Handle{24},Handle{32}}) if (!check(p.write(vertex,offset,patch),"partial position update")) return false;
        if (!draw.pixels(vertex,{},6,255,255) || !draw.pixels(vertex,index,6,255,255)) return false;
        if (!check(p.resize(vertex,80),"render vertex growth") || !draw.pixels(vertex,index,6,255,255) ||
            !check(p.resize(vertex,48),"render vertex shrink") || !draw.pixels(vertex,{},6,255,255)) return false;
        if (!draw.pixels(vertex,{},6,255,255,1) || !draw.pixels(vertex,{},6,255,255,2)) return false;
        const std::uint16_t degenerate[]={0,0,0}; auto partial_index=byte_copy(degenerate);
        if (!check(p.write(index,0,partial_index),"partial index update") || !draw.pixels(vertex,index,6,0,255)) return false;
        if (!check(p.resize(index,20),"render index growth") || !draw.pixels(vertex,index,6,0,255) ||
            !check(p.resize(index,6),"render index shrink") || !draw.pixels(vertex,index,3,0,0)) return false;
        std::printf("buffer lifecycle: eleven rendered lifecycle cases across all four draw paths passed\n");
        return true;
    }
}

bool run_gl_buffer_tests()
{
    state_ok=true;
    if (!check(igpu_supports(51)==(igpu_gl_fns().CopyBufferSubData!=nullptr) && igpu_supports(52),
               "buffer resize/readback capability discovery")) return false;
    Caller caller; Probe p;
    if (!dynamic_core(p,1,8,48,80,32) || !dynamic_core(p,2,0,12,20,6)) return false;
    const auto staging=p.create(32,2,0,0), storage=p.create(32,0,8,4);
    if (!check(staging.handle && storage.handle,"staging/storage creation")) return false;
    Bytes zero(32,0), data(32,0x39), patch={1,2,3,4,5,6,7};
    if (!p.public_equals(staging,0,zero) || !p.public_equals(staging,5,Bytes(7,0)) || !p.public_equals(storage,0,zero)) return false;
    if (!check(!p.write(staging,0,data),"staging rejects CPU writes") || !p.gpu_equals(staging,zero,"staging bytes unchanged")) return false;
    if (!check(p.write(storage,0,data) && p.write(storage,9,patch),"storage public writes")) return false;
    std::copy(patch.begin(),patch.end(),data.begin()+9);
    if (!p.public_equals(storage,0,data) || !p.gpu_equals(storage,data,"storage GPU contents") ||
        !p.public_equals(storage,9,patch)) return false;
    if (!allocation_matrix(p) || !invalid_operations(p,storage,staging,data) || !drawing_regression(p) || !unavailable_operations(p)) return false;
    for (auto b:p.owned) if (b.handle && !check(p.release(b),"release preserves caller bindings")) return false;
    for (auto name:caller.buffers) if (!p.gpu_equals({0,name},Bytes(64,0xA7),"foreign buffer bytes")) return false;
    if (!check(state_ok && glGetError()==GL_NO_ERROR,"caller state / GL errors")) return false;
    std::printf("buffer lifecycle: dynamic vertex/index zero/write/grow/shrink and staging/storage GPU bytes passed\n");
    return true;
}
