#include "igpu_gl_loader.h"

#include "igpu_error.h"
#include "igpu_gl.h"

#include <string>

namespace
{
    IgpuGlFns g_fns;
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
        LOAD(BindVertexArray, "glBindVertexArray")
        LOAD(GetUniformLocation, "glGetUniformLocation")
        LOAD(Uniform1i, "glUniform1i")
#undef LOAD
        return true;
    }
}
