#include "igpu_gl_uniform.h"
#include "igpu_gl_loader.h"
#include "igpu_gl_graphics.h"
#include "igpu_error.h"
#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace igpu
{
    namespace
    {
        constexpr GLenum ubo = 0x8A11, binding = 0x8A28;
        bool context = false;
        GLint bindings = 0, block_bytes = 0, blocks[6]{};
        bool fail(const char* entry, const std::string& message)
        { set_last_error(std::string(entry) + ": " + message); return false; }
        struct Generic
        {
            GLint previous = 0;
            Generic() { glGetIntegerv(binding, &previous); }
            ~Generic() { igpu_gl_fns().BindBuffer(ubo, static_cast<GLuint>(previous)); }
        };
        struct Range
        {
            GLint name = 0;
            std::int64_t offset = 0, size = 0;
            void capture(GLuint slot)
            {
                auto& f = igpu_gl_fns(); f.GetIntegeri(binding, slot, &name);
                f.GetInteger64i(0x8A29, slot, &offset); f.GetInteger64i(0x8A2A, slot, &size);
            }
            void restore(GLuint slot) const
            {
                auto& f = igpu_gl_fns();
                if (name && size > 0) f.BindBufferRange(ubo, slot, name, static_cast<std::ptrdiff_t>(offset), static_cast<std::ptrdiff_t>(size));
                else f.BindBufferBase(ubo, slot, name);
            }
        };
        struct Claim { std::uint64_t buffer = 0; Range previous; };
        std::unordered_map<int, Claim> claims;
        void shape(GLenum type, GLint row_major, UniformMember& m)
        {
            m.type = static_cast<int>(UniformType::Unknown);
            m.shape = static_cast<int>(UniformShape::Scalar); m.rows = m.columns = 1;
            if (type == GL_FLOAT) m.type = static_cast<int>(UniformType::Float);
            else if (type == GL_INT) m.type = static_cast<int>(UniformType::Int);
            else if (type == GL_UNSIGNED_INT) m.type = static_cast<int>(UniformType::Uint);
            else if (type == 0x8B56) m.type = static_cast<int>(UniformType::Bool);
            else
            {
                struct Vector { GLenum first; int kind; };
                for (const auto& v : {Vector{0x8B50,0}, Vector{0x8B53,1}, Vector{0x8DC6,2}, Vector{0x8B57,3}, Vector{0x8FFC,5}})
                    if (type >= v.first && type <= v.first + 2)
                    { m.type = v.kind; m.shape = static_cast<int>(UniformShape::Vector); m.columns = static_cast<int>(type-v.first)+2; return; }
                const bool wide_matrix = type >= 0x8F46 && type <= 0x8F4E;
                if (wide_matrix) type = type <= 0x8F48 ? 0x8B5A+(type-0x8F46) : 0x8B65+(type-0x8F49);
                struct Matrix { GLenum type; int rows, columns; };
                for (const auto& mat : {Matrix{0x8B5A,2,2}, Matrix{0x8B5B,3,3}, Matrix{0x8B5C,4,4},
                    Matrix{0x8B65,3,2}, Matrix{0x8B66,4,2}, Matrix{0x8B67,2,3}, Matrix{0x8B68,4,3}, Matrix{0x8B69,2,4}, Matrix{0x8B6A,3,4}})
                    if (type == mat.type)
                    {
                        m.type = static_cast<int>(wide_matrix ? UniformType::Unknown : UniformType::Float); m.rows = mat.rows; m.columns = mat.columns;
                        m.shape = static_cast<int>(row_major ? UniformShape::MatrixRows : UniformShape::MatrixColumns); return;
                    }
            }
        }
    }
    void gl_uniform_initialize()
    {
        context = gl_context_at_least(4,3) ||
            (gl_context_extension("GL_ARB_compute_shader") && gl_context_extension("GL_ARB_uniform_buffer_object") &&
             gl_context_extension("GL_ARB_separate_shader_objects"));
        bindings = block_bytes = 0; for (auto& count : blocks) count = 0;
        if (!context) return;
        glGetIntegerv(0x8A2F, &bindings); glGetIntegerv(0x8A30, &block_bytes);
        const GLenum fields[] = {0x8A2B,0x8A2D,0x91BB};
        for (int i=0;i<3;++i) glGetIntegerv(fields[i], &blocks[i]);
        if(gl_context_at_least(3,2))glGetIntegerv(0x8A2C,&blocks[3]);
        if(gl_context_at_least(4,0)||gl_context_extension("GL_ARB_tessellation_shader")){glGetIntegerv(0x8E89,&blocks[4]);glGetIntegerv(0x8E8A,&blocks[5]);}
    }
    bool gl_uniform_supported()
    {
        const auto& f = igpu_gl_fns();
        return context && bindings >= 24 && block_bytes > 0 && blocks[0] >= 8 && blocks[1] >= 8 && blocks[2] >= 8 &&
            f.ProgramParameteri && f.GetActiveUniformBlockiv && f.GetActiveUniformBlockName && f.GetActiveUniformsiv &&
            f.BindBufferBase && f.BindBufferRange && f.GetIntegeri && f.GetInteger64i;
    }
    bool gl_uniform_reflect(std::uint32_t shader, std::int32_t stage, UniformLayout& out)
    {
        const char* entry = "igpu_shader_reflect";
        if (!gl_uniform_supported()) return fail(entry,"this OpenGL context lacks uniform reflection support");
        auto& f = igpu_gl_fns();
        const auto program = f.CreateProgram();
        if (!program) return fail(entry,"could not create a reflection program");
        struct Program { GLuint name; ~Program() { igpu_gl_fns().DeleteProgram(name); } } owned{program};
        f.ProgramParameteri(program,0x8258 /* PROGRAM_SEPARABLE */,GL_TRUE);
        f.AttachShader(program,shader); f.LinkProgram(program);
        GLint linked=0;f.GetProgramiv(program,0x8B82,&linked);
        if (!linked)
        {
            // Ordinary vertex shaders may omit the explicit gl_PerVertex interface
            // required for separable linking. Query a single-stage ordinary program
            // without rewriting their source or changing the caller's executable.
            f.ProgramParameteri(program,0x8258,GL_FALSE);f.LinkProgram(program);
            f.GetProgramiv(program,0x8B82,&linked);
        }
        if(!linked && stage>=3 && stage<=5)
        {
            // Ordinary geometry/tessellation shaders can need companion stages
            // even though separable interfaces are optional in a normal pipeline.
            // Link native companions; the caller's shader text is never modified.
            const auto companion=[&](GLenum type,const char*source)
            {
                const auto name=f.CreateShader(type);if(!name)return false;
                f.ShaderSource(name,1,&source,nullptr);f.CompileShader(name);GLint ok=0;f.GetShaderiv(name,0x8B81,&ok);
                if(ok)f.AttachShader(program,name);f.DeleteShader(name);return ok!=0;
            };
            bool complete=companion(0x8B31,"#version 430\nvoid main(){gl_Position=vec4(0,0,0,1);}");
            if(stage==4)complete=companion(0x8E87,"#version 430\nlayout(triangles,equal_spacing,cw)in;void main(){gl_Position=gl_in[0].gl_Position;}")&&complete;
            if(complete){f.LinkProgram(program);f.GetProgramiv(program,0x8B82,&linked);}
        }
        if (!linked)
        {
            GLint length=0;f.GetProgramiv(program,0x8B84,&length);std::vector<char> text(static_cast<std::size_t>((std::max)(length,1)));
            f.GetProgramInfoLog(program,static_cast<GLsizei>(text.size()),nullptr,text.data());
            return fail(entry,"could not link shader for reflection: "+std::string(text.data()));
        }
        GLint count=0,name_bytes=0;
        f.GetProgramiv(program,0x8A36,&count);f.GetProgramiv(program,0x8B87,&name_bytes);
        UniformLayout layout;
        for (GLint index=0;index<count;++index)
        {
            GLint length=0,size=0,slot=0,members=0;
            f.GetActiveUniformBlockiv(program,index,0x8A41,&length);
            std::vector<char> name(static_cast<std::size_t>((std::max)(length,1)));
            f.GetActiveUniformBlockName(program,index,static_cast<GLsizei>(name.size()),nullptr,name.data());
            f.GetActiveUniformBlockiv(program,index,0x8A40,&size);f.GetActiveUniformBlockiv(program,index,0x8A3F,&slot);
            f.GetActiveUniformBlockiv(program,index,0x8A42,&members);
            if (size <= 0 || size > block_bytes || slot < stage*8 || slot >= stage*8+8 || slot>=bindings || stage<0 || stage>=6 || blocks[stage]<1)
                return fail(entry,"uniform block '"+std::string(name.data())+"' must use GLSL binding stage*8+slot with slot 0..7 and fit the context block limit");
            UniformBlock block;block.name=name.data();block.size=static_cast<std::uint32_t>(size);block.slot=slot-stage*8;
            std::vector<GLint> indices(static_cast<std::size_t>(members));
            if (members) f.GetActiveUniformBlockiv(program,index,0x8A43,indices.data());
            for (auto raw : indices)
            {
                const auto uniform=static_cast<GLuint>(raw);
                std::vector<char> member_name(static_cast<std::size_t>((std::max)(name_bytes,1)));
                GLint elements=0,offset=0,array_stride=0,matrix_stride=0,row_major=0;GLenum type=0;
                f.GetActiveUniform(program,uniform,static_cast<GLsizei>(member_name.size()),nullptr,&elements,&type,member_name.data());
                f.GetActiveUniformsiv(program,1,&uniform,0x8A3B,&offset);f.GetActiveUniformsiv(program,1,&uniform,0x8A3C,&array_stride);
                f.GetActiveUniformsiv(program,1,&uniform,0x8A3D,&matrix_stride);f.GetActiveUniformsiv(program,1,&uniform,0x8A3E,&row_major);
                UniformMember member;member.name=member_name.data();
                const std::string prefix=block.name+".";
                if (member.name.compare(0,prefix.size(),prefix)==0) member.name.erase(0,prefix.size());
                const bool array=member.name.size()>=3 && member.name.compare(member.name.size()-3,3,"[0]")==0;
                if (array) member.name.resize(member.name.size()-3);
                member.offset=static_cast<std::uint32_t>(offset);member.elements=array?elements:0;
                member.array_stride=static_cast<std::uint32_t>(array_stride);member.matrix_stride=static_cast<std::uint32_t>(matrix_stride);
                shape(type,row_major,member);
                const bool matrix=matrix_stride>0;
                const auto majors=matrix?(row_major?member.rows:member.columns):1;
                const auto minors=matrix?(row_major?member.columns:member.rows):member.columns;
                const bool wide = type == GL_DOUBLE || (type >= 0x8FFC && type <= 0x8FFE) || (type >= 0x8F46 && type <= 0x8F4E);
                const auto scalar_bytes = wide ? 8u : 4u;
                const auto span=std::uint64_t(array?(std::max)(elements-1,0):0)*member.array_stride+
                    std::uint64_t(majors-1)*member.matrix_stride+std::uint64_t(minors)*scalar_bytes;
                if (offset<0 || span>block.size || std::uint64_t(member.offset)+span>block.size)
                    return fail(entry,"uniform member '"+member.name+"' lies outside its block");
                member.size=static_cast<std::uint32_t>(span);block.members.push_back(std::move(member));
            }
            layout.blocks.push_back(std::move(block));
        }
        out=std::move(layout);return true;
    }
    bool gl_uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot)
    {
        const char* entry="igpu_uniform_bind";
        if (!gl_uniform_supported()) return fail(entry,"this OpenGL context lacks uniform buffer support");
        if(stage<0||stage>=6||slot<0||slot>=8||!gl_graphics_stage_supported(stage)||blocks[stage]<8||stage*8+slot>=bindings)
            return fail(entry,"the requested stage/slot is unsupported by the context's uniform limits");
        const auto key=stage*8+slot;auto existing=claims.find(key);
        if (!buffer)
        {
            if (existing!=claims.end()) { Generic generic;existing->second.previous.restore(key);claims.erase(existing); }
            return true;
        }
        GlUniformBuffer resolved;
        if (!gl_uniform_buffer(buffer,resolved,entry)) return false;
        if (resolved.bytes>block_bytes) return fail(entry,"the uniform buffer exceeds the context block limit");
        Generic generic;
        if (existing==claims.end()) { Claim claim;claim.buffer=buffer;claim.previous.capture(key);claims.emplace(key,claim); }
        else existing->second.buffer=buffer;
        igpu_gl_fns().BindBufferBase(ubo,key,resolved.name);return true;
    }
    bool gl_uniform_validate_inputs(std::int32_t first, std::int32_t last, const char* entry)
    {
        for (const auto& claim:claims)
        {
            const auto stage=claim.first/8;if(stage<first || stage>last) continue;
            GlUniformBuffer buffer;if (!gl_uniform_buffer(claim.second.buffer,buffer,entry)) return false;
        }
        return true;
    }
    void gl_uniform_release_buffer(std::uint64_t buffer)
    {
        if(claims.empty())return;
        Generic generic;
        for(auto it=claims.begin();it!=claims.end();)
        {
            if(it->second.buffer!=buffer){++it;continue;}
            it->second.previous.restore(it->first);it=claims.erase(it);
        }
    }
    void gl_uniform_resources_release()
    {
        if(claims.empty())return;
        Generic generic;for(const auto& claim:claims)claim.second.previous.restore(claim.first);claims.clear();
    }
}
