#include "compute_tests.h"
#include "IGPU_native.h"
#include "native/igpu_capabilities.h"
#include "native/igpu_texture.h"
#include "native/gl/igpu_gl_compute.h"
#include "native/gl/igpu_gl_draw.h"
#include "native/gl/igpu_gl_loader.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

namespace
{
    using Handle = std::int64_t;
    constexpr GLenum ssbo = 0x90D2, ssbo_binding = 0x90D3, array_buffer = 0x8892;
    constexpr GLenum image_fields[] = {0x8F3A, 0x8F3B, 0x8F3C, 0x8F3D, 0x8F3E, 0x906E};
    bool check(bool ok, const char* label)
    {
        if (!ok) std::fprintf(stderr, "compute tests: %s: %s\n", label, igpu_get_last_error().c_str());
        return ok;
    }
    struct Wire
    {
        std::vector<std::byte> bytes;
        Wire(std::initializer_list<std::int32_t> values) : bytes(2 + 5 * values.size())
        {
            const auto count = static_cast<std::uint16_t>(values.size());
            std::memcpy(bytes.data(), &count, 2);
            std::size_t offset = 2;
            for (auto value : values)
            { bytes[offset] = std::byte{6}; std::memcpy(bytes.data() + offset + 1, &value, 4); offset += 5; }
        }
        gm::wire::GMArrayView view() const
        { return gm::wire::GMArrayView(gm::wire::GMValue(gm::wire::GMKind::Array, bytes.data())); }
    };
    bool dispatch(Handle target, int kind = 1, int x = 4, int y = 1, int z = 1, int mip = 0)
    {
        return igpu_dispatch(x, y, z, Wire{kind}.view(), Wire{static_cast<int>(target)}.view(),
                             Wire{0}.view(), Wire{mip}.view());
    }
    struct Indexed
    {
        GLint name = 0;
        std::int64_t start = 0, size = 0;
        void capture(GLuint slot)
        {
            auto& f = igpu_gl_fns();
            f.GetIntegeri(ssbo_binding, slot, &name);
            f.GetInteger64i(0x90D4, slot, &start); f.GetInteger64i(0x90D5, slot, &size);
        }
        void restore(GLuint slot) const
        {
            auto& f = igpu_gl_fns();
            if (name && size) f.BindBufferRange(ssbo, slot, name, static_cast<std::ptrdiff_t>(start), static_cast<std::ptrdiff_t>(size));
            else f.BindBufferBase(ssbo, slot, name);
        }
        bool operator==(const Indexed& b) const { return name == b.name && start == b.start && size == b.size; }
    };
    struct Snapshot
    {
        std::array<Indexed, 32> buffers;
        std::array<std::array<GLint, 6>, 8> images{};
        // Program, generic SSBO, ARRAY, VAO, current 2D texture, active texture, draw/read FBO.
        static constexpr GLenum fields[] = {0x8B8D, ssbo_binding, 0x8894, 0x85B5, GL_TEXTURE_BINDING_2D, 0x84E0, 0x8CA6, 0x8CAA};
        std::array<GLint, 8> globals{};
        Snapshot()
        {
            for (GLuint i = 0; i < buffers.size(); ++i) buffers[i].capture(i);
            for (GLuint i = 0; i < images.size(); ++i)
                for (int j = 0; j < 6; ++j) igpu_gl_fns().GetIntegeri(image_fields[j], i, &images[i][j]);
            for (int i = 0; i < 8; ++i) glGetIntegerv(fields[i], &globals[i]);
        }
        bool operator==(const Snapshot& b) const { return buffers == b.buffers && images == b.images && globals == b.globals; }
        void restore() const
        {
            auto& f = igpu_gl_fns();
            for (GLuint i = 0; i < buffers.size(); ++i) buffers[i].restore(i);
            for (GLuint i = 0; i < images.size(); ++i)
            {
                const auto& v = images[i];
                f.BindImageTexture(i, v[0], v[1], static_cast<GLboolean>(v[2]), v[3], v[4], v[5]);
            }
            f.UseProgram(globals[0]); f.BindBuffer(ssbo, globals[1]); f.BindBuffer(array_buffer, globals[2]);
            f.BindVertexArray(globals[3]);
            reinterpret_cast<void(APIENTRY*)(GLenum)>(wglGetProcAddress("glActiveTexture"))(globals[5]); glBindTexture(GL_TEXTURE_2D, globals[4]);
            f.BindFramebuffer(0x8CA9, globals[6]); f.BindFramebuffer(0x8CA8, globals[7]);
        }
    };
    struct Fixture
    {
        Snapshot original;
        std::vector<Handle> shaders, buffers, textures;
        std::vector<GLuint> programs;
        GLuint foreign = 0, foreign_image = 0;
        Handle previous_compute = igpu_get_bound_shader(2);
        ~Fixture()
        {
            for (int stage = 0; stage < 3; ++stage)
                for (int slot = 0; slot < 8; ++slot) igpu_storage_bind(0, stage, slot);
            igpu_shader_bind(previous_compute, 2);
            for (auto shader : shaders) if (shader) igpu_shader_release(shader);
            for (auto buffer : buffers) if (buffer) igpu_buffer_release(buffer);
            for (auto texture : textures) igpu_texture_release(texture);
            original.restore();
            auto& f = igpu_gl_fns();
            for (auto program : programs) f.DeleteProgram(program);
            if (foreign) f.DeleteBuffers(1, &foreign);
            if (foreign_image) glDeleteTextures(1, &foreign_image);
        }
        Handle shader(const std::string& text)
        {
            auto h = igpu_shader_compile(text, "main", 2, "glsl");
            if (h) shaders.push_back(h);
            return h;
        }
        Handle buffer(int bytes = 16, int role = 8, int usage = 0, int stride = 4)
        {
            auto h = igpu_buffer_create(bytes, usage, role, stride);
            if (h) buffers.push_back(h);
            return h;
        }
        Handle texture(int format = 6, int mips = 1, bool storage = true)
        {
            auto h = igpu_texture_create(0, 4, 4, 1, format, true, storage, mips);
            if (h) textures.push_back(h);
            return h;
        }
        bool release_buffer(Handle h)
        {
            for (auto& b : buffers) if (b == h) b = 0;
            return igpu_buffer_release(h);
        }
        bool release_shader(Handle h)
        {
            for (auto& s : shaders) if (s == h) s = 0;
            return igpu_shader_release(h);
        }
        bool read(Handle h, std::array<std::uint32_t, 4> want)
        {
            std::array<std::uint32_t, 4> actual{};
            const bool ok = igpu_buffer_read(h, 0, gm::wire::GMBuffer(actual.data(), sizeof(actual))) && actual == want;
            if (!ok) std::fprintf(stderr, "compute words: %u %u %u %u, expected %u %u %u %u\n",
                actual[0], actual[1], actual[2], actual[3], want[0], want[1], want[2], want[3]);
            return check(ok, "GPU storage bytes");
        }
        template<class F> bool rejected(F action, const char* label)
        {
            const Snapshot before;
            const bool accepted = action(), error = !igpu_get_last_error().empty();
            const auto gl_error = glGetError();
            const bool state = before == Snapshot{};
            if (accepted || !error || !state || gl_error) std::fprintf(stderr,
                "compute rejection %s: accepted=%d error=%d state=%d GL=0x%x\n", label, accepted, error, state, gl_error);
            return check(!accepted && error && state && !gl_error, label);
        }
        bool sentinels()
        {
            auto& f = igpu_gl_fns();
            GLint alignment = 0; glGetIntegerv(0x90DF, &alignment);
            f.GenBuffers(1, &foreign); f.BindBuffer(ssbo, foreign);
            std::vector<unsigned char> bytes(static_cast<std::size_t>(alignment) * 4 + 256, 0xA6);
            f.BufferData(ssbo, bytes.size(), bytes.data(), 0x88E4);
            for (GLuint i = 0; i < 32; ++i) f.BindBufferRange(ssbo, i, foreign, alignment * (1 + i % 3), 64);
            glGenTextures(1, &foreign_image); glBindTexture(GL_TEXTURE_2D, foreign_image);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            for (GLuint i = 0; i < 8; ++i) f.BindImageTexture(i, foreign_image, 1, GL_FALSE, 0, i % 2 ? 0x88BA : 0x88B8, GL_RGBA8);
            f.BindBuffer(array_buffer, foreign);
            return check(glGetError() == GL_NO_ERROR, "caller indexed ranges and image bindings");
        }
    };
    const char* core_source = "#version 430\nlayout(local_size_x=1)in;layout(std430,binding=0)buffer B{uint data[];};"
                              "void main(){uint i=gl_GlobalInvocationID.x;data[i]=i*3u+7u;}";
    bool buffer_and_input_tests()
    {
        Fixture t;
        const auto core = t.shader(core_source), a = t.buffer(), b = t.buffer(), c = t.buffer();
        if (!check(core && a && b && c && t.sentinels(), "buffer fixtures")) return false;
        const Snapshot before;
        if (!check(igpu_shader_bind(core, 2) && before == Snapshot{} && dispatch(a) && before == Snapshot{},
            "compute bind and dispatch preserve caller state") || !t.read(a, {7, 10, 13, 16})) return false;
        const auto volume=t.buffer(48);
        const auto three_d=t.shader("#version 430\nlayout(local_size_x=1)in;layout(std430,binding=0)buffer B{uint d[];};"
            "void main(){uvec3 p=gl_GlobalInvocationID;uint i=p.x+2u*p.y+6u*p.z;d[i]=100u+i;}");
        std::array<std::uint32_t,12> volume_words{};
        if(!check(volume&&three_d&&igpu_shader_bind(three_d,2)&&dispatch(volume,1,2,3,2)&&
            igpu_buffer_read(volume,0,gm::wire::GMBuffer(volume_words.data(),sizeof(volume_words)))&&
            volume_words==std::array<std::uint32_t,12>{100,101,102,103,104,105,106,107,108,109,110,111},"asymmetric three-dimensional group counts"))return false;
        const std::array<std::uint32_t, 4> seed{3, 4, 5, 6};
        if (!check(igpu_buffer_write(c, 0, gm::wire::GMBuffer(const_cast<std::uint32_t*>(seed.data()), sizeof(seed))), "input seed")) return false;
        const auto add = t.shader("#version 430\nlayout(local_size_x=2)in;"
            "layout(std430,binding=24)readonly buffer A{uint a[];};layout(std430,binding=31)readonly buffer C{uint c[];};"
            "layout(std430,binding=0)writeonly buffer B{uint b[];};void main(){uint i=gl_GlobalInvocationID.x;b[i]=a[i]+2u*c[i];}");
        if (!check(add && igpu_shader_bind(add, 2) && igpu_storage_bind(a, 2, 0) && igpu_storage_bind(c, 2, 7),
            "readonly compute slots 0 and 7")) return false;
        const Snapshot claimed;
        if (!check(dispatch(b, 1, 2) && claimed == Snapshot{}, "local size and readonly state") || !t.read(b, {13, 18, 23, 28})) return false;
        if (!check(igpu_storage_bind(b, 2, 0) && dispatch(a, 1, 2), "GPU write to subsequent GPU read") || !t.read(a, {19, 26, 33, 40})) return false;
        if (!t.rejected([&] { return dispatch(b, 1, 2); }, "readonly/write alias")) return false;
        igpu::GlStorageBuffer native;
        if (!igpu::gl_storage_buffer(b, native, "test")) return false;
        using Map = void*(APIENTRY*)(GLenum, std::ptrdiff_t, std::ptrdiff_t, GLbitfield);
        using Unmap = GLboolean(APIENTRY*)(GLenum);
        const auto map = reinterpret_cast<Map>(wglGetProcAddress("glMapBufferRange"));
        const auto unmap = reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        auto& f = igpu_gl_fns();
        f.BindBuffer(array_buffer, native.name);
        if (!check(map && unmap && map(array_buffer, 0, 16, 1), "map readonly input after claiming it")) return false;
        const bool rejected = t.rejected([&] { return dispatch(a, 1, 2); }, "mapped readonly compute input");
        unmap(array_buffer); f.BindBuffer(array_buffer, t.foreign);
        if (!rejected) return false;
        if (!check(igpu_storage_bind(0, 2, 0) && igpu_storage_bind(0, 2, 7) && before == Snapshot{},
            "unbind restores original indexed ranges after replacing a claim")) return false;
        if (!check(igpu_shader_bind(core, 2) && igpu_storage_bind(a, 0, 0) && igpu_storage_bind(a, 1, 0) && dispatch(a),
            "graphics stage storage claims allow compute writes") || !t.read(a, {7, 10, 13, 16})) return false;
        if (!check(t.release_buffer(a), "release buffer claimed by two stages")) return false;
        if (!check(before == Snapshot{}, "buffer release restores every persistent claim")) return false;
        std::printf("compute buffers: GPU words, local size, chained reads, stage/slot namespaces and persistent range restoration passed\n");
        return true;
    }
    bool image_pixels(Handle handle, int mip, std::array<float, 4> want)
    {
        igpu::GlStorageTexture native;
        if (!igpu::gl_storage_texture(handle, mip, native, "test")) return false;
        auto& f = igpu_gl_fns();
        constexpr GLenum fields[] = {GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS, GL_PACK_SWAP_BYTES};
        GLint held_texture = 0, held_pack = 0, values[5]{};
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &held_texture); glGetIntegerv(0x88ED, &held_pack);
        for (int i = 0; i < 5; ++i) { glGetIntegerv(fields[i], &values[i]); glPixelStorei(fields[i], i ? 0 : 1); }
        f.BindBuffer(0x88EB, 0); glBindTexture(GL_TEXTURE_2D, native.name);
        std::array<float, 64> actual{};
        glGetTexImage(GL_TEXTURE_2D, mip, GL_RGBA, GL_FLOAT, actual.data());
        const auto error = glGetError();
        glBindTexture(GL_TEXTURE_2D, held_texture); f.BindBuffer(0x88EB, held_pack);
        for (int i = 0; i < 5; ++i) glPixelStorei(fields[i], values[i]);
        const int size = mip == 0 ? 4 : mip == 1 ? 2 : 1;
        bool ok = !error;
        for (int p = 0; p < size * size; ++p)
            for (int c = 0; c < 4; ++c) ok = ok && std::fabs(actual[p * 4 + c] - want[c]) < .002f;
        if (!ok) std::fprintf(stderr, "compute image mip%d: %f %f %f %f, expected %f %f %f %f GL=0x%x\n",
            mip, actual[0], actual[1], actual[2], actual[3], want[0], want[1], want[2], want[3], error);
        return check(ok, "all image texels and channels");
    }
    IgpuGlDispatchCompute forward_dispatch = nullptr;
    GLint observed_access = 0;
    GLuint observed_program = 0;
    void APIENTRY observe_dispatch(GLuint x, GLuint y, GLuint z)
    {
        igpu_gl_fns().GetIntegeri(image_fields[4], 0, &observed_access);
        GLint program = 0; glGetIntegerv(0x8B8D, &program); observed_program = static_cast<GLuint>(program);
        forward_dispatch(x, y, z);
    }
    struct DispatchObserver
    {
        DispatchObserver() { forward_dispatch = igpu_gl_fns().DispatchCompute; igpu_gl_fns().DispatchCompute = observe_dispatch; }
        ~DispatchObserver() { igpu_gl_fns().DispatchCompute = forward_dispatch; }
    };
    bool image_and_multi_tests()
    {
        Fixture t;
        if (!t.sentinels()) return false;
        struct Format { int id; const char* qualifier; std::array<float, 4> want; };
        const Format formats[] = {
            {6,"rgba8",{64.f/255,128.f/255,191.f/255,1}}, {9,"r16f",{.25f,0,0,1}},
            {10,"r32f",{.25f,0,0,1}}, {12,"r8",{64.f/255,0,0,1}}, {13,"rg8",{64.f/255,128.f/255,0,1}},
            {14,"rgba16f",{.25f,.5f,.75f,1}}, {15,"rgba32f",{.25f,.5f,.75f,1}}
        };
        for (const auto& format : formats)
        {
            const auto texture = t.texture(format.id, 0);
            const auto shader = t.shader(std::string("#version 430\nlayout(local_size_x=1,local_size_y=1)in;layout(") +
                format.qualifier + ",binding=0)writeonly uniform image2D target;void main(){imageStore(target,ivec2(gl_GlobalInvocationID.xy),vec4(.25,.5,.75,1));}");
            const Snapshot before;
            if (!check(texture && shader && igpu_shader_bind(shader, 2) && dispatch(texture, 0, 4, 4) && before == Snapshot{}, "storage image base write") ||
                !image_pixels(texture, 0, format.want) || !check(dispatch(texture, 0, 2, 2, 1, 1) && before == Snapshot{}, "storage image mip write") ||
                !image_pixels(texture, 1, format.want) || !image_pixels(texture, 2, {0,0,0,1})) return false;
        }
        if (!t.rejected([&] { return igpu_texture_create(0,4,4,1,11,true,true,1)!=0; }, "RGBA4 storage format")) return false;
        const auto image0 = t.texture(), image2 = t.texture(), b1 = t.buffer(), b3 = t.buffer();
        const auto mixed = t.shader("#version 430\nlayout(local_size_x=1,local_size_y=1)in;"
            "layout(rgba8,binding=0)writeonly uniform image2D a;layout(std430,binding=1)buffer B{uint b[];};"
            "layout(rgba8,binding=2)writeonly uniform image2D c;layout(std430,binding=3)buffer D{uint d[];};"
            "void main(){ivec2 p=ivec2(gl_GlobalInvocationID.xy);imageStore(a,p,vec4(1,0,0,1));imageStore(c,p,vec4(0,1,0,1));"
            "if(p.y==0){b[p.x]=101u+uint(p.x);d[p.x]=201u+uint(p.x);}}");
        const Snapshot before;
        {
            const auto both=t.shader("#version 430\nlayout(local_size_x=1,local_size_y=1)in;"
                "layout(rgba8,binding=0)writeonly uniform image2D a;layout(std430,binding=1)buffer B{uint b[];};"
                "void main(){ivec2 p=ivec2(gl_GlobalInvocationID.xy);imageStore(a,p,vec4(1,0,0,1));if(p.y==0)b[p.x]=101u+uint(p.x);}");
            DispatchObserver observed;
            if (!check(both && igpu_shader_bind(both,2) && igpu::dispatch_both(4,4,1,image0,b1), "both-target wrapper") ||
                !check(observed_access == 0x88BA /* READ_WRITE */, "storage images permit read/modify/write")) return false;
        }
        if (!check(image0 && image2 && b1 && b3 && mixed && igpu_shader_bind(mixed,2) &&
            igpu_dispatch(4,4,1,Wire{0,1,0,1}.view(),Wire{int(image0),int(b1),int(image2),int(b3)}.view(),Wire{0,0,0,0}.view(),Wire{0,0,0,0}.view()) &&
            before == Snapshot{}, "mixed image/SSBO target slots") || !image_pixels(image0,0,{1,0,0,1}) || !image_pixels(image2,0,{0,1,0,1}) ||
            !t.read(b1,{101,102,103,104}) || !t.read(b3,{201,202,203,204})) return false;
        const auto update=t.shader("#version 430\nlayout(local_size_x=1,local_size_y=1)in;"
            "layout(rgba8,binding=0)uniform image2D a;void main(){ivec2 p=ivec2(gl_GlobalInvocationID.xy);"
            "vec4 v=imageLoad(a,p);imageStore(a,p,vec4(v.r*.5,.25,.75,v.a));}");
        if(!check(update && igpu_shader_bind(update,2) && dispatch(image0,0,4,4),"GPU image read/modify/write") ||
            !image_pixels(image0,0,{128.f/255,64.f/255,191.f/255,1}))return false;
        std::array<Handle,8> outputs{};
        std::string source="#version 430\nlayout(local_size_x=1)in;";
        for(int i=0;i<8;++i){outputs[i]=t.buffer();source+="layout(std430,binding="+std::to_string(i)+")buffer B"+std::to_string(i)+"{uint d"+std::to_string(i)+"[];};";}
        source+="void main(){uint i=gl_GlobalInvocationID.x;";
        for(int i=0;i<8;++i)source+="d"+std::to_string(i)+"[i]="+std::to_string(11+i)+"u+i;";
        source+="}";
        const auto eight=t.shader(source);
        if(!check(eight&&igpu_shader_bind(eight,2)&&igpu_dispatch(4,1,1,Wire{1,1,1,1,1,1,1,1}.view(),
            Wire{int(outputs[0]),int(outputs[1]),int(outputs[2]),int(outputs[3]),int(outputs[4]),int(outputs[5]),int(outputs[6]),int(outputs[7])}.view(),
            Wire{0,0,0,0,0,0,0,0}.view(),Wire{0,0,0,0,0,0,0,0}.view())&&before==Snapshot{},"eight writable SSBO slots"))return false;
        for(int i=0;i<8;++i)if(!t.read(outputs[i],{std::uint32_t(11+i),std::uint32_t(12+i),std::uint32_t(13+i),std::uint32_t(14+i)}))return false;
        std::array<Handle,8> image_outputs{};
        source="#version 430\nlayout(local_size_x=1,local_size_y=1)in;";
        for(int i=0;i<8;++i){image_outputs[i]=t.texture();source+="layout(rgba8,binding="+std::to_string(i)+")writeonly uniform image2D t"+std::to_string(i)+";";}
        source+="void main(){ivec2 p=ivec2(gl_GlobalInvocationID.xy);";
        for(int i=0;i<8;++i)source+="imageStore(t"+std::to_string(i)+",p,vec4("+std::to_string((i+1)*16)+".0/255.0,0,0,1));";
        source+="}";
        const auto eight_images=t.shader(source);
        if(!check(eight_images&&igpu_shader_bind(eight_images,2)&&igpu_dispatch(4,4,1,Wire{0,0,0,0,0,0,0,0}.view(),
            Wire{int(image_outputs[0]),int(image_outputs[1]),int(image_outputs[2]),int(image_outputs[3]),int(image_outputs[4]),int(image_outputs[5]),int(image_outputs[6]),int(image_outputs[7])}.view(),
            Wire{0,0,0,0,0,0,0,0}.view(),Wire{0,0,0,0,0,0,0,0}.view())&&before==Snapshot{},"eight writable image slots"))return false;
        for(int i=0;i<8;++i)if(!image_pixels(image_outputs[i],0,{float((i+1)*16)/255,0,0,1}))return false;
        std::printf("compute images: seven formats, base/mip isolation, mixed slot positions read/modify/write and eight writable buffers/images passed\n");
        return true;
    }
    bool program_and_guard_tests()
    {
        Fixture t;
        const auto core=t.shader(core_source),other=t.shader(core_source),buffer=t.buffer(),ordinary=t.buffer(16,1,1,8),image=t.texture(),plain=t.texture(6,1,false);
        if(!check(core&&other&&buffer&&ordinary&&image&&plain&&t.sentinels(),"validation fixtures"))return false;
        const Snapshot before;
        if(!t.rejected([&]{return dispatch(buffer);},"no compute shader"))return false;
        if(!check(igpu_shader_bind(core,2)&&before==Snapshot{},"compute bind preserves graphics program"))return false;
        if(!t.rejected([&]{return igpu_shader_bind(core,0);},"compute shader bound as vertex")||
            !t.rejected([&]{return igpu_shader_bind(igpu_get_bound_shader(0),2);},"vertex shader bound as compute"))return false;
        if(!check(t.release_shader(other)&&igpu_get_bound_shader(2)==core&&before==Snapshot{}&&dispatch(buffer),"release unbound compute shader preserves live compute/graphics"))return false;
        if(!t.rejected([&]{return dispatch(buffer,1,0);},"zero groups")||!t.rejected([&]{return dispatch(buffer,1,65536);},"too many groups")||
            !t.rejected([&]{return dispatch(buffer,1,1,-1);},"negative groups")||!t.rejected([&]{return dispatch(ordinary);},"wrong buffer role")||
            !t.rejected([&]{return dispatch(plain,0);},"non-storage image")||!t.rejected([&]{return dispatch(image,0,1,1,1,1);},"out of range image mip")||
            !t.rejected([&]{return igpu_dispatch(1,1,1,Wire{1,1}.view(),Wire{int(buffer),int(buffer)}.view(),Wire{0,0}.view(),Wire{0,0}.view());},"duplicate buffer")||
            !t.rejected([&]{return igpu_dispatch(1,1,1,Wire{0,0}.view(),Wire{int(image),int(image)}.view(),Wire{0,0}.view(),Wire{0,0}.view());},"duplicate image")||
            !t.rejected([&]{return igpu_dispatch(1,1,1,Wire{1}.view(),Wire{int(buffer)}.view(),Wire{1}.view(),Wire{0}.view());},"nonzero layer")||
            !t.rejected([&]{return igpu_dispatch(1,1,1,Wire{1,0}.view(),Wire{int(buffer),int(image)}.view(),Wire{0,0}.view(),Wire{0,1}.view());},"mixed nonzero mips")||
            !t.rejected([&]{return igpu_dispatch(1,1,1,Wire{}.view(),Wire{}.view(),Wire{}.view(),Wire{}.view());},"empty writes")||
            !t.rejected([&]{return igpu_dispatch(1,1,1,Wire{1}.view(),Wire{}.view(),Wire{0}.view(),Wire{0}.view());},"array length mismatch")||
            !t.rejected([&]{return igpu_dispatch(1,1,1,Wire{1,1,1,1,1,1,1,1,1}.view(),Wire{int(buffer),int(buffer),int(buffer),int(buffer),int(buffer),int(buffer),int(buffer),int(buffer),int(buffer)}.view(),Wire{0,0,0,0,0,0,0,0,0}.view(),Wire{0,0,0,0,0,0,0,0,0}.view());},"nine write targets")||
            !t.rejected([&]{return igpu_dispatch(1,1,1,Wire{0,1}.view(),Wire{int(image),-1}.view(),Wire{0,0}.view(),Wire{0,0}.view());},"invalid final target preserves earlier image binding")||
            !t.rejected([&]{return dispatch(buffer,1,1,1,65536);},"too many z groups")||
            !t.rejected([&]{return dispatch(buffer,2);},"unknown write kind")||!t.rejected([&]{return dispatch(0);},"null write")||
            !t.rejected([&]{return igpu::dispatch_both(1,1,1,image,0);},"missing both buffer")||
            !t.rejected([&]{return igpu::dispatch_both(1,1,1,0,buffer);},"missing both image")||
            !t.rejected([&]{return igpu_storage_bind(buffer,6,0);},"unsupported storage stage")||
            !t.rejected([&]{return igpu_storage_bind(buffer,2,8);},"storage slot limit")||
            !t.rejected([&]{return igpu_storage_bind(ordinary,2,0);},"non-storage input"))return false;
        if(!check(t.release_buffer(ordinary),"release guard buffer")||!t.rejected([&]{return dispatch(ordinary);},"released target"))return false;
        igpu::GlStorageBuffer native;
        if(!igpu::gl_storage_buffer(buffer,native,"test"))return false;
        using Map=void*(APIENTRY*)(GLenum,std::ptrdiff_t,std::ptrdiff_t,GLbitfield);
        using Unmap=GLboolean(APIENTRY*)(GLenum);
        auto map=reinterpret_cast<Map>(wglGetProcAddress("glMapBufferRange"));auto unmap=reinterpret_cast<Unmap>(wglGetProcAddress("glUnmapBuffer"));
        auto& f=igpu_gl_fns();f.BindBuffer(array_buffer,native.name);
        if(!check(map(array_buffer,0,16,1)!=nullptr,"map output"))return false;
        const bool mapped=t.rejected([&]{return dispatch(buffer);},"mapped write target");
        unmap(array_buffer);f.BindBuffer(array_buffer,t.foreign);if(!mapped)return false;
        const auto incomplete=t.shader("#version 430\nvoid main(){}");
        if(!check(incomplete!=0,"compile compute without local size for link failure")||
            !t.rejected([&]{return igpu_shader_bind(incomplete,2);},"transactional compute link failure")||
            !check(igpu_get_bound_shader(2)==core&&dispatch(buffer),"failed link preserves previous executable")||!t.read(buffer,{7,10,13,16}))return false;
        // A current program pending deletion cannot survive a temporary switch.
        const auto pending=f.CreateProgram();
        const auto cs=f.CreateShader(0x91B9);const char* text="#version 430\nlayout(local_size_x=1)in;void main(){}";
        f.ShaderSource(cs,1,&text,nullptr);f.CompileShader(cs);f.AttachShader(pending,cs);f.LinkProgram(pending);f.DeleteShader(cs);
        f.UseProgram(pending);f.DeleteProgram(pending);
        const bool kept=t.rejected([&]{return dispatch(buffer);},"current program pending deletion");
        f.UseProgram(before.globals[0]);if(!kept)return false;
        if(!check(t.release_shader(core)&&igpu_get_bound_shader(2)==0&&before==Snapshot{},"release bound compute preserves graphics")||
            !t.rejected([&]{return dispatch(buffer);},"released bound compute"))return false;
        std::printf("compute validation: groups, roles, handles, arrays, aliases, mapped resources and transactional program lifetime passed\n");
        return true;
    }
    GLuint graphics(Fixture& t, const char* vertex, const char* pixel)
    {
        auto& f=igpu_gl_fns();const auto program=f.CreateProgram();t.programs.push_back(program);
        const char* sources[]={vertex,pixel};
        for(int i=0;i<2;++i)
        {
            const auto shader=f.CreateShader(i?0x8B30:0x8B31);f.ShaderSource(shader,1,&sources[i],nullptr);f.CompileShader(shader);
            GLint ok=0;f.GetShaderiv(shader,0x8B81,&ok);if(ok)f.AttachShader(program,shader);f.DeleteShader(shader);
            if(!check(ok!=0,"graphics consumer compile"))return 0;
        }
        f.BindAttribLocation(program,0,"in_pos");f.BindAttribLocation(program,1,"in_uv");f.LinkProgram(program);
        GLint linked=0;f.GetProgramiv(program,0x8B82,&linked);return check(linked!=0,"graphics consumer link")?program:0;
    }
    bool graphics_consumers()
    {
        Fixture t;
        const auto a=t.buffer(),b=t.buffer(),vertex=t.buffer(32,1,1,8),target=t.texture(),source=t.texture();
        const auto core=t.shader(core_source);
        std::array<float,8> positions={-1,-1,1,-1,-1,1,1,1};
        if(!check(a&&b&&vertex&&target&&source&&core&&igpu_buffer_write(vertex,0,gm::wire::GMBuffer(positions.data(),sizeof(positions)))&&
            igpu_shader_bind(core,2)&&dispatch(a)&&dispatch(b),"GPU graphics input fixtures"))return false;
        const auto layout=igpu_input_layout_create(igpu_get_bound_shader(0),Wire{1}.view(),Wire{2}.view(),Wire{0}.view(),1,8,0);
        if(!check(layout!=0,"graphics consumer layout"))return false;
        const auto program=graphics(t,
            "#version 430\nlayout(location=0)in vec2 in_pos;layout(std430,binding=8)readonly buffer V{uint v[];};"
            "void main(){gl_Position=vec4(in_pos+vec2(float(v[0])-7.0,0),0,1);}",
            "#version 430\nlayout(std430,binding=16)readonly buffer P{uint p[];};out vec4 colour;"
            "void main(){colour=vec4(vec3(p[0],p[1],p[2])/255.0,1);}");
        auto& f=igpu_gl_fns();
        GLint read_fbo=0;glGetIntegerv(0x8CAA,&read_fbo);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        std::int32_t previous=0,viewport[4]{};
        const bool begun=igpu::gl_color_target_begin(target,previous,viewport);
        for(auto cap:{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_BLEND,GL_ALPHA_TEST,0x809D,0x8C89})glDisable(cap);
        GLint clips=0;glGetIntegerv(0x0D32,&clips);for(int i=0;i<clips;++i)glDisable(GL_CLIP_PLANE0+i);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        f.UseProgram(program);
        bool ok=begun&&program&&igpu_storage_bind(a,0,0)&&igpu_storage_bind(b,1,0);
        const Snapshot claimed;
        const auto query=igpu_query_create(0);
        ok=ok&&query&&igpu_query_begin(query)&&igpu_draw(vertex,0,layout,5,0,4,1,0,0,0,0);
        if(query){ok=igpu_query_end(query)&&ok;ok=igpu_query_result(query)==16&&ok;igpu_query_release(query);}
        ok=ok&&claimed==Snapshot{}&&igpu_texture_read(target,1,1,0,0)==0x0D0A07;
        igpu_storage_bind(0,0,0);igpu_storage_bind(0,1,0);
        const auto fill=t.shader("#version 430\nlayout(local_size_x=1,local_size_y=1)in;layout(rgba8,binding=0)writeonly uniform image2D a;"
            "void main(){imageStore(a,ivec2(gl_GlobalInvocationID.xy),vec4(.5,.25,0,1));}");
        const auto sampled=graphics(t,"#version 430\nlayout(location=0)in vec2 in_pos;void main(){gl_Position=vec4(in_pos,0,1);}",
            "#version 430\nuniform sampler2D igpu_tex;out vec4 colour;void main(){colour=texture(igpu_tex,vec2(.5));}");
        // Dispatch while a graphics program is current, then immediately sample its output.
        f.UseProgram(sampled);const Snapshot before;
        ok=ok&&fill&&sampled&&igpu_shader_bind(fill,2)&&dispatch(source,0,4,4)&&before==Snapshot{}&&
            igpu_draw_sampled(vertex,layout,5,0,4,source,0,0,0,0)&&before==Snapshot{}&&igpu_texture_read(target,1,1,0,0)==0x004080;
        const auto error=glGetError();
        if(begun)igpu::gl_color_target_end(previous,viewport);f.BindFramebuffer(0x8CA8,read_fbo);glPopAttrib();
        igpu_input_layout_release(layout);
        if(!check(ok&&!error,"GPU compute to vertex/pixel storage fetch and sampled pixels"))return false;
        std::printf("compute consumers: vertex/pixel storage inputs render 16 samples, compute image sampling and graphics program restoration passed\n");
        return true;
    }
    bool transform_feedback_tests()
    {
        Fixture t;auto& f=igpu_gl_fns();
        using Varyings=void(APIENTRY*)(GLuint,GLsizei,const char*const*,GLenum);
        using Gen=void(APIENTRY*)(GLsizei,GLuint*);using Delete=void(APIENTRY*)(GLsizei,const GLuint*);
        using Bind=void(APIENTRY*)(GLenum,GLuint);using Begin=void(APIENTRY*)(GLenum);using Action=void(APIENTRY*)();
        auto varyings=reinterpret_cast<Varyings>(wglGetProcAddress("glTransformFeedbackVaryings"));
        auto gen=reinterpret_cast<Gen>(wglGetProcAddress("glGenTransformFeedbacks"));
        auto remove=reinterpret_cast<Delete>(wglGetProcAddress("glDeleteTransformFeedbacks"));
        auto bind=reinterpret_cast<Bind>(wglGetProcAddress("glBindTransformFeedback"));
        auto begin=reinterpret_cast<Begin>(wglGetProcAddress("glBeginTransformFeedback"));
        auto end=reinterpret_cast<Action>(wglGetProcAddress("glEndTransformFeedback"));
        auto pause=reinterpret_cast<Action>(wglGetProcAddress("glPauseTransformFeedback"));
        auto resume=reinterpret_cast<Action>(wglGetProcAddress("glResumeTransformFeedback"));
        if(!check(varyings&&gen&&remove&&bind&&begin&&end&&pause&&resume,"transform feedback inspection"))return false;
        const auto shader=t.shader(core_source),output=t.buffer();
        const auto program=f.CreateProgram();t.programs.push_back(program);
        const auto vertex=f.CreateShader(0x8B31);
        const char* text="#version 430\nout float captured;void main(){captured=float(gl_VertexID)+.5;gl_Position=vec4(0,0,0,1);}";
        f.ShaderSource(vertex,1,&text,nullptr);f.CompileShader(vertex);f.AttachShader(program,vertex);f.DeleteShader(vertex);
        const char* name="captured";varyings(program,1,&name,0x8C8C);f.LinkProgram(program);
        GLint linked=0;f.GetProgramiv(program,0x8B82,&linked);
        if(!check(shader&&output&&linked&&igpu_shader_bind(shader,2),"transform feedback fixtures"))return false;
        GLint previous_tf=0,previous_buffer=0;glGetIntegerv(0x8E25,&previous_tf);glGetIntegerv(0x8C8F,&previous_buffer);
        const auto discard=glIsEnabled(0x8C89);
        GLuint tf=0,capture=0,vao=0;gen(1,&tf);bind(0x8E22,tf);
        f.GenBuffers(1,&capture);f.BindBuffer(0x8C8E,capture);f.BufferData(0x8C8E,64,nullptr,0x88E4);f.BindBufferBase(0x8C8E,0,capture);
        f.GenVertexArrays(1,&vao);f.BindVertexArray(vao);f.UseProgram(program);glEnable(0x8C89);
        begin(GL_POINTS);glDrawArrays(GL_POINTS,0,1);
        bool ok=t.rejected([&]{return dispatch(output);},"active unpaused transform feedback")&&t.read(output,{0,0,0,0});
        GLint active=0,paused=0;glGetIntegerv(0x8E24,&active);glGetIntegerv(0x8E23,&paused);ok=ok&&active&&!paused;
        pause();const Snapshot before;
        const bool dispatched=dispatch(output);glGetIntegerv(0x8E24,&active);glGetIntegerv(0x8E23,&paused);
        ok=ok&&dispatched&&active&&paused&&before==Snapshot{}&&t.read(output,{7,10,13,16});
        resume();glDrawArrays(GL_POINTS,1,1);end();
        std::array<float,2> captured{};f.GetBufferSubData(0x8C8E,0,sizeof(captured),captured.data());
        const bool after=dispatch(output);ok=ok&&after&&before==Snapshot{}&&captured==std::array<float,2>{.5f,1.5f};
        const auto error=glGetError();
        bind(0x8E22,previous_tf);f.BindBuffer(0x8C8E,previous_buffer);remove(1,&tf);f.DeleteBuffers(1,&capture);
        f.BindVertexArray(t.original.globals[3]);f.DeleteVertexArrays(1,&vao);if(!discard)glDisable(0x8C89);
        if(!check(ok&&!error,"transform feedback guard, paused dispatch, resume output and post-capture dispatch"))return false;
        std::printf("compute capture: active feedback rejected without mutation; paused/ended dispatch, resumed captured values and caller program preserved\n");
        return true;
    }
    bool capability_tests()
    {
        Fixture t;const auto shader=t.shader(core_source),buffer=t.buffer();
        if(!check(shader&&buffer&&igpu_shader_bind(shader,2),"capability fixtures"))return false;
        using igpu::Capability;
        if(!check(igpu::supports(Capability::ShaderStageCompute)&&igpu::supports(Capability::StructuredBuffer)&&igpu::supports(Capability::UnorderedAccess),"complete compute capabilities"))return false;
        auto& f=igpu_gl_fns();
        auto test=[&](auto& function,const char* label)
        {
            const auto saved=function;function=nullptr;
            const bool hidden=!igpu::supports(Capability::ShaderStageCompute)&&!igpu::supports(Capability::StructuredBuffer)&&!igpu::supports(Capability::UnorderedAccess);
            // Snapshot uses optional query functions, so only dispatch/capability results are inspected here.
            const bool rejected=!dispatch(buffer)&&!igpu_get_last_error().empty();
            function=saved;
            return check(hidden&&rejected&&glGetError()==GL_NO_ERROR,label);
        };
        if(!test(f.DispatchCompute,"missing dispatch")||!test(f.MemoryBarrier,"missing barrier")||!test(f.BindImageTexture,"missing image bind")||
            !test(f.BindBufferBase,"missing indexed bind")||!test(f.BindBufferRange,"missing range restore")||
            !test(f.GetIntegeri,"missing indexed query")||!test(f.GetInteger64i,"missing range query"))return false;
        std::printf("compute capabilities: seven optional entrypoint failures hide incomplete features and reject before mutation\n");
        return true;
    }
}

bool run_gl_compute_tests()
{
    return buffer_and_input_tests() && image_and_multi_tests() && program_and_guard_tests() && graphics_consumers() && transform_feedback_tests() && capability_tests();
}

bool run_gl_compute_shutdown_tests()
{
    auto& f=igpu_gl_fns();
    // The preceding teardown may leave its deleted graphics program current.
    f.UseProgram(0);
    const Snapshot original;
    const auto context=wglGetCurrentContext();
    GLuint foreign=0;f.GenBuffers(1,&foreign);f.BindBuffer(ssbo,foreign);
    GLint alignment=0;glGetIntegerv(0x90DF,&alignment);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(alignment)*2+64,0xB7);
    f.BufferData(ssbo,bytes.size(),bytes.data(),0x88E4);
    for(GLuint slot:{8u,16u,24u,31u})f.BindBufferRange(ssbo,slot,foreign,alignment,64);
    const auto shader=igpu_shader_compile(core_source,"main",2,"glsl");
    const auto buffer=igpu_buffer_create(16,0,8,4);
    const auto texture=igpu_texture_create(0,4,4,1,6,true,true,1);
    igpu::GlStorageBuffer native_buffer;igpu::GlStorageTexture native_texture;
    bool ok=shader&&buffer&&texture&&igpu_shader_bind(shader,2)&&
        igpu::gl_storage_buffer(buffer,native_buffer,"test")&&igpu::gl_storage_texture(texture,0,native_texture,"test");
    GLuint program=0,native_shader=0;
    {
        DispatchObserver observed;
        ok=ok&&dispatch(buffer);program=observed_program;
    }
    using Attached=void(APIENTRY*)(GLuint,GLsizei,GLsizei*,GLuint*);
    using Is=GLboolean(APIENTRY*)(GLuint);
    auto attached=reinterpret_cast<Attached>(wglGetProcAddress("glGetAttachedShaders"));
    auto is_program=reinterpret_cast<Is>(wglGetProcAddress("glIsProgram"));
    auto is_shader=reinterpret_cast<Is>(wglGetProcAddress("glIsShader"));
    auto is_buffer=reinterpret_cast<Is>(wglGetProcAddress("glIsBuffer"));
    GLsizei count=0;if(attached&&program)attached(program,1,&count,&native_shader);
    const Snapshot before;
    ok=ok&&count==1&&is_program&&is_shader&&is_buffer&&igpu_storage_bind(buffer,0,0)&&igpu_storage_bind(buffer,1,0)&&
        igpu_storage_bind(buffer,2,0)&&igpu_storage_bind(buffer,2,7);
    if(!check(ok,"shutdown compute fixtures"))return false;
    igpu_shutdown();
    const Snapshot after;
    ok=before==after&&!igpu_is_available()&&wglGetCurrentContext()==context&&!is_program(program)&&!is_shader(native_shader)&&
        !is_buffer(native_buffer.name)&&!glIsTexture(native_texture.name);
    f.BindBuffer(ssbo,foreign);std::vector<unsigned char> actual(bytes.size());f.GetBufferSubData(ssbo,0,actual.size(),actual.data());
    ok=ok&&actual==bytes&&glGetError()==GL_NO_ERROR;
    original.restore();f.DeleteBuffers(1,&foreign);
    if(!check(ok,"shutdown restores claims and deletes owned compute program/shader/buffer/image"))return false;
    if(!check(igpu_bind_current()&&!igpu_buffer_release(buffer)&&!igpu_texture_release(texture)&&!igpu_shader_release(shader)&&
        igpu_get_bound_shader(2)==0&&igpu::supports(igpu::Capability::ShaderStageCompute),"compute rebind has no stale resources or claims"))return false;
    std::printf("compute shutdown: indexed ranges and caller bytes restored; program/shader/buffer/image deleted; borrowed context and rebind preserved\n");
    return true;
}
