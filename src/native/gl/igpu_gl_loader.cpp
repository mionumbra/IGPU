#include "igpu_gl_loader.h"

#include "igpu_error.h"
#include "igpu_gl.h"

#include <string>

namespace
{
    IgpuGlFns g_fns;

    PROC optional_proc(const char* name)
    {
        const PROC entry = wglGetProcAddress(name);
        return entry != nullptr && entry != reinterpret_cast<PROC>(1) &&
            entry != reinterpret_cast<PROC>(2) && entry != reinterpret_cast<PROC>(3) &&
            entry != reinterpret_cast<PROC>(-1) ? entry : nullptr;
    }
}

IgpuGlFns& igpu_gl_fns()
{
    return g_fns;
}

namespace igpu
{
    bool gl_load()
    {
        auto& fns = igpu_gl_fns();
#define LOAD(field, symbol)                                                                 \
        fns.field = reinterpret_cast<decltype(fns.field)>(wglGetProcAddress(symbol));       \
        if (fns.field == nullptr)                                                           \
        {                                                                                   \
            set_last_error(std::string("igpu_bind_current: this OpenGL context is missing ") + symbol); \
            return false;                                                                   \
        }
        LOAD(CreateShader, "glCreateShader")
        LOAD(ShaderSource, "glShaderSource")
        LOAD(CompileShader, "glCompileShader")
        LOAD(DeleteShader, "glDeleteShader")
        LOAD(CreateProgram, "glCreateProgram")
        LOAD(AttachShader, "glAttachShader")
        LOAD(BindAttribLocation, "glBindAttribLocation")
        LOAD(LinkProgram, "glLinkProgram")
        LOAD(UseProgram, "glUseProgram")
        LOAD(DeleteProgram, "glDeleteProgram")
        LOAD(GetShaderiv, "glGetShaderiv")
        LOAD(GetProgramiv, "glGetProgramiv")
        LOAD(GetShaderInfoLog, "glGetShaderInfoLog")
        LOAD(GetProgramInfoLog, "glGetProgramInfoLog")
        LOAD(GenBuffers, "glGenBuffers")
        LOAD(BindBuffer, "glBindBuffer")
        LOAD(BufferData, "glBufferData")
        LOAD(BufferSubData, "glBufferSubData")
        LOAD(GetBufferSubData, "glGetBufferSubData")
        LOAD(GetBufferParameteriv, "glGetBufferParameteriv")
        LOAD(DeleteBuffers, "glDeleteBuffers")
        LOAD(GenFramebuffers, "glGenFramebuffers")
        LOAD(BindFramebuffer, "glBindFramebuffer")
        LOAD(FramebufferTexture2D, "glFramebufferTexture2D")
        LOAD(DeleteFramebuffers, "glDeleteFramebuffers")
        LOAD(CheckFramebufferStatus, "glCheckFramebufferStatus")
        LOAD(EnableVertexAttribArray, "glEnableVertexAttribArray")
        LOAD(DisableVertexAttribArray, "glDisableVertexAttribArray")
        LOAD(VertexAttribPointer, "glVertexAttribPointer")
        LOAD(GenVertexArrays, "glGenVertexArrays")
        LOAD(DeleteVertexArrays, "glDeleteVertexArrays")
        LOAD(BindVertexArray, "glBindVertexArray")
        LOAD(GetUniformLocation, "glGetUniformLocation")
        LOAD(Uniform1i, "glUniform1i")
        LOAD(GetActiveUniform, "glGetActiveUniform")
        LOAD(GenerateMipmap, "glGenerateMipmap")
        LOAD(BlendFuncSeparate, "glBlendFuncSeparate")
        LOAD(BlendEquationSeparate, "glBlendEquationSeparate")
        LOAD(GenRenderbuffers, "glGenRenderbuffers")
        LOAD(DeleteRenderbuffers, "glDeleteRenderbuffers")
        LOAD(BindRenderbuffer, "glBindRenderbuffer")
        LOAD(RenderbufferStorage, "glRenderbufferStorage")
        LOAD(FramebufferRenderbuffer, "glFramebufferRenderbuffer")
#undef LOAD
        // Optional entrypoints gate only their individual capability families.
#define LOAD_OPTIONAL(field, symbol) \
        fns.field = reinterpret_cast<decltype(fns.field)>(optional_proc(symbol));
        LOAD_OPTIONAL(CopyBufferSubData, "glCopyBufferSubData")
        LOAD_OPTIONAL(GenQueries, "glGenQueries")
        LOAD_OPTIONAL(DeleteQueries, "glDeleteQueries")
        LOAD_OPTIONAL(BeginQuery, "glBeginQuery")
        LOAD_OPTIONAL(EndQuery, "glEndQuery")
        LOAD_OPTIONAL(GetQueryiv, "glGetQueryiv")
        LOAD_OPTIONAL(GetQueryObjectiv, "glGetQueryObjectiv")
        LOAD_OPTIONAL(GetQueryObjectuiv, "glGetQueryObjectuiv")
        LOAD_OPTIONAL(GetQueryObjectui64v, "glGetQueryObjectui64v")
        LOAD_OPTIONAL(QueryCounter, "glQueryCounter")
        LOAD_OPTIONAL(FenceSync, "glFenceSync")
        LOAD_OPTIONAL(ClientWaitSync, "glClientWaitSync")
        LOAD_OPTIONAL(DeleteSync, "glDeleteSync")
#undef LOAD_OPTIONAL
        return true;
    }
}
