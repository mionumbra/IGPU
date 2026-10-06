#pragma once

#include <Windows.h>
#include <GL/gl.h>

#include <cstddef>
#include <cstdint>

// Desktop GL 2.0 entry points are not exported from opengl32. Load them with
// wglGetProcAddress after a context is current. Typedefs live here so the
// probe does not depend on glext.h.

using IgpuGlCreateShader = GLuint(APIENTRY*)(GLenum);
using IgpuGlShaderSource = void(APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
using IgpuGlCompileShader = void(APIENTRY*)(GLuint);
using IgpuGlDeleteShader = void(APIENTRY*)(GLuint);
using IgpuGlCreateProgram = GLuint(APIENTRY*)();
using IgpuGlAttachShader = void(APIENTRY*)(GLuint, GLuint);
using IgpuGlBindAttribLocation = void(APIENTRY*)(GLuint, GLuint, const char*);
using IgpuGlLinkProgram = void(APIENTRY*)(GLuint);
using IgpuGlUseProgram = void(APIENTRY*)(GLuint);
using IgpuGlDeleteProgram = void(APIENTRY*)(GLuint);
using IgpuGlGetShaderiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using IgpuGlGetProgramiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using IgpuGlGetShaderInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using IgpuGlGetProgramInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using IgpuGlGenBuffers = void(APIENTRY*)(GLsizei, GLuint*);
using IgpuGlBindBuffer = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlBufferData = void(APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum);
using IgpuGlDeleteBuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using IgpuGlGenFramebuffers = void(APIENTRY*)(GLsizei, GLuint*);
using IgpuGlBindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlFramebufferTexture2D = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using IgpuGlDeleteFramebuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using IgpuGlCheckFramebufferStatus = GLenum(APIENTRY*)(GLenum);
using IgpuGlEnableVertexAttribArray = void(APIENTRY*)(GLuint);
using IgpuGlDisableVertexAttribArray = void(APIENTRY*)(GLuint);
using IgpuGlVertexAttribPointer = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using IgpuGlGenVertexArrays = void(APIENTRY*)(GLsizei, GLuint*);
using IgpuGlBindVertexArray = void(APIENTRY*)(GLuint);

struct IgpuGlFns
{
    IgpuGlCreateShader CreateShader = nullptr;
    IgpuGlShaderSource ShaderSource = nullptr;
    IgpuGlCompileShader CompileShader = nullptr;
    IgpuGlDeleteShader DeleteShader = nullptr;
    IgpuGlCreateProgram CreateProgram = nullptr;
    IgpuGlAttachShader AttachShader = nullptr;
    IgpuGlBindAttribLocation BindAttribLocation = nullptr;
    IgpuGlLinkProgram LinkProgram = nullptr;
    IgpuGlUseProgram UseProgram = nullptr;
    IgpuGlDeleteProgram DeleteProgram = nullptr;
    IgpuGlGetShaderiv GetShaderiv = nullptr;
    IgpuGlGetProgramiv GetProgramiv = nullptr;
    IgpuGlGetShaderInfoLog GetShaderInfoLog = nullptr;
    IgpuGlGetProgramInfoLog GetProgramInfoLog = nullptr;
    IgpuGlGenBuffers GenBuffers = nullptr;
    IgpuGlBindBuffer BindBuffer = nullptr;
    IgpuGlBufferData BufferData = nullptr;
    IgpuGlDeleteBuffers DeleteBuffers = nullptr;
    IgpuGlGenFramebuffers GenFramebuffers = nullptr;
    IgpuGlBindFramebuffer BindFramebuffer = nullptr;
    IgpuGlFramebufferTexture2D FramebufferTexture2D = nullptr;
    IgpuGlDeleteFramebuffers DeleteFramebuffers = nullptr;
    IgpuGlCheckFramebufferStatus CheckFramebufferStatus = nullptr;
    IgpuGlEnableVertexAttribArray EnableVertexAttribArray = nullptr;
    IgpuGlDisableVertexAttribArray DisableVertexAttribArray = nullptr;
    IgpuGlVertexAttribPointer VertexAttribPointer = nullptr;
    IgpuGlGenVertexArrays GenVertexArrays = nullptr;
    IgpuGlBindVertexArray BindVertexArray = nullptr;
};

IgpuGlFns& igpu_gl_fns();
