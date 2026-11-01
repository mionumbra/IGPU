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
using IgpuGlGetAttribLocation = GLint(APIENTRY*)(GLuint, const char*);
using IgpuGlGetActiveAttrib = void(APIENTRY*)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, char*);
using IgpuGlBindAttribLocation = void(APIENTRY*)(GLuint, GLuint, const char*);
using IgpuGlLinkProgram = void(APIENTRY*)(GLuint);
using IgpuGlUseProgram = void(APIENTRY*)(GLuint);
using IgpuGlDeleteProgram = void(APIENTRY*)(GLuint);
using IgpuGlGetAttachedShaders = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLuint*);
using IgpuGlGetShaderiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using IgpuGlGetProgramiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using IgpuGlGetShaderInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using IgpuGlGetProgramInfoLog = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using IgpuGlGenBuffers = void(APIENTRY*)(GLsizei, GLuint*);
using IgpuGlBindBuffer = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlBufferData = void(APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum);
using IgpuGlBufferSubData = void(APIENTRY*)(GLenum, std::ptrdiff_t, std::ptrdiff_t, const void*);
using IgpuGlGetBufferSubData = void(APIENTRY*)(GLenum, std::ptrdiff_t, std::ptrdiff_t, void*);
using IgpuGlGetBufferParameteriv = void(APIENTRY*)(GLenum, GLenum, GLint*);
using IgpuGlCopyBufferSubData = void(APIENTRY*)(GLenum, GLenum, std::ptrdiff_t, std::ptrdiff_t, std::ptrdiff_t);
using IgpuGlDeleteBuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using IgpuGlGenFramebuffers = void(APIENTRY*)(GLsizei, GLuint*);
using IgpuGlBindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlFramebufferTexture2D = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using IgpuGlDeleteFramebuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using IgpuGlGetBooleani = void(APIENTRY*)(GLenum, GLuint, GLboolean*);
using IgpuGlIsEnabledi = GLboolean(APIENTRY*)(GLenum, GLuint);
using IgpuGlEnablei = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlDisablei = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlColorMaski = void(APIENTRY*)(GLuint, GLboolean, GLboolean, GLboolean, GLboolean);
using IgpuGlBlendFuncSeparatei = void(APIENTRY*)(GLuint, GLenum, GLenum, GLenum, GLenum);
using IgpuGlBlendEquationSeparatei = void(APIENTRY*)(GLuint, GLenum, GLenum);
using IgpuGlPatchParameteri = void(APIENTRY*)(GLenum, GLint);
using IgpuGlActiveTexture = void(APIENTRY*)(GLenum);
using IgpuGlTexImage3D = void(APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using IgpuGlFramebufferTextureLayer = void(APIENTRY*)(GLenum, GLenum, GLuint, GLint, GLint);
using IgpuGlDrawBuffers = void(APIENTRY*)(GLsizei, const GLenum*);
using IgpuGlCheckFramebufferStatus = GLenum(APIENTRY*)(GLenum);
using IgpuGlEnableVertexAttribArray = void(APIENTRY*)(GLuint);
using IgpuGlDisableVertexAttribArray = void(APIENTRY*)(GLuint);
using IgpuGlGetVertexAttribiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using IgpuGlGetVertexAttribPointerv = void(APIENTRY*)(GLuint, GLenum, void**);
using IgpuGlVertexAttribIPointer = void(APIENTRY*)(GLuint, GLint, GLenum, GLsizei, const void*);
using IgpuGlVertexAttribPointer = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using IgpuGlGenVertexArrays = void(APIENTRY*)(GLsizei, GLuint*);
using IgpuGlDeleteVertexArrays = void(APIENTRY*)(GLsizei, const GLuint*);
using IgpuGlBindVertexArray = void(APIENTRY*)(GLuint);
using IgpuGlGetUniformLocation = GLint(APIENTRY*)(GLuint, const char*);
using IgpuGlGetActiveUniform = void(APIENTRY*)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, char*);
using IgpuGlGenerateMipmap = void(APIENTRY*)(GLenum);
using IgpuGlUniform1i = void(APIENTRY*)(GLint, GLint);
using IgpuGlStencilFuncSeparate = void(APIENTRY*)(GLenum, GLenum, GLint, GLuint);
using IgpuGlStencilOpSeparate = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
using IgpuGlStencilMaskSeparate = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlBlendFuncSeparate = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
using IgpuGlBlendEquationSeparate = void(APIENTRY*)(GLenum, GLenum);
using IgpuGlGenRenderbuffers = void(APIENTRY*)(GLsizei, GLuint*);
using IgpuGlDeleteRenderbuffers = void(APIENTRY*)(GLsizei, const GLuint*);
using IgpuGlBindRenderbuffer = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlRenderbufferStorage = void(APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);
using IgpuGlFramebufferRenderbuffer = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint);

using IgpuGlProgramParameteri = void(APIENTRY*)(GLuint, GLenum, GLint);
using IgpuGlGetActiveUniformBlockiv = void(APIENTRY*)(GLuint, GLuint, GLenum, GLint*);
using IgpuGlGetActiveUniformBlockName = void(APIENTRY*)(GLuint, GLuint, GLsizei, GLsizei*, char*);
using IgpuGlGetActiveUniformsiv = void(APIENTRY*)(GLuint, GLsizei, const GLuint*, GLenum, GLint*);

using IgpuGlDispatchCompute = void(APIENTRY*)(GLuint, GLuint, GLuint);
using IgpuGlMemoryBarrier = void(APIENTRY*)(GLbitfield);
using IgpuGlBindImageTexture = void(APIENTRY*)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);
using IgpuGlBindBufferBase = void(APIENTRY*)(GLenum, GLuint, GLuint);
using IgpuGlBindBufferRange = void(APIENTRY*)(GLenum, GLuint, GLuint, std::ptrdiff_t, std::ptrdiff_t);
using IgpuGlGetIntegeri = void(APIENTRY*)(GLenum, GLuint, GLint*);
using IgpuGlGetInteger64i = void(APIENTRY*)(GLenum, GLuint, std::int64_t*);

using IgpuGlDrawArraysInstanced = void(APIENTRY*)(GLenum, GLint, GLsizei, GLsizei);
using IgpuGlVertexAttribDivisor = void(APIENTRY*)(GLuint, GLuint);
using IgpuGlDrawArraysIndirect = void(APIENTRY*)(GLenum, const void*);
using IgpuGlDrawElementsIndirect = void(APIENTRY*)(GLenum, GLenum, const void*);

using IgpuGlGenQueries = void(APIENTRY*)(GLsizei, GLuint*);
using IgpuGlDeleteQueries = void(APIENTRY*)(GLsizei, const GLuint*);
using IgpuGlBeginQuery = void(APIENTRY*)(GLenum, GLuint);
using IgpuGlEndQuery = void(APIENTRY*)(GLenum);
using IgpuGlGetQueryiv = void(APIENTRY*)(GLenum, GLenum, GLint*);
using IgpuGlGetQueryObjectiv = void(APIENTRY*)(GLuint, GLenum, GLint*);
using IgpuGlGetQueryObjectuiv = void(APIENTRY*)(GLuint, GLenum, GLuint*);
using IgpuGlGetQueryObjectui64v = void(APIENTRY*)(GLuint, GLenum, std::uint64_t*);
using IgpuGlQueryCounter = void(APIENTRY*)(GLuint, GLenum);
using IgpuGlSync = void*;
using IgpuGlFenceSync = IgpuGlSync(APIENTRY*)(GLenum, GLbitfield);
using IgpuGlClientWaitSync = GLenum(APIENTRY*)(IgpuGlSync, GLbitfield, std::uint64_t);
using IgpuGlDeleteSync = void(APIENTRY*)(IgpuGlSync);

struct IgpuGlFns
{
    decltype(&glGetIntegerv) GetIntegerv = glGetIntegerv;
    decltype(&glIsEnabled) IsEnabled = glIsEnabled;
    decltype(&glEnable) Enable = glEnable;
    decltype(&glDisable) Disable = glDisable;
    void(APIENTRY* ActiveStencilFaceEXT)(GLenum) = nullptr;
    void(APIENTRY* GetFramebufferAttachmentParameteriv)(GLenum, GLenum, GLenum, GLint*) = nullptr;
    IgpuGlGetBooleani GetBooleani = nullptr;
    IgpuGlIsEnabledi IsEnabledi = nullptr;
    IgpuGlEnablei Enablei = nullptr;
    IgpuGlDisablei Disablei = nullptr;
    IgpuGlColorMaski ColorMaski = nullptr;
    IgpuGlBlendFuncSeparatei BlendFuncSeparatei = nullptr;
    IgpuGlBlendEquationSeparatei BlendEquationSeparatei = nullptr;
    IgpuGlPatchParameteri PatchParameteri = nullptr;
    IgpuGlActiveTexture ActiveTexture = nullptr;
    IgpuGlTexImage3D TexImage3D = nullptr;
    IgpuGlFramebufferTextureLayer FramebufferTextureLayer = nullptr;
    IgpuGlDrawBuffers DrawBuffers = nullptr;
    IgpuGlProgramParameteri ProgramParameteri = nullptr;
    IgpuGlGetActiveUniformBlockiv GetActiveUniformBlockiv = nullptr;
    IgpuGlGetActiveUniformBlockName GetActiveUniformBlockName = nullptr;
    IgpuGlGetActiveUniformsiv GetActiveUniformsiv = nullptr;
    IgpuGlDispatchCompute DispatchCompute = nullptr;
    IgpuGlMemoryBarrier MemoryBarrier = nullptr;
    IgpuGlBindImageTexture BindImageTexture = nullptr;
    IgpuGlBindBufferBase BindBufferBase = nullptr;
    IgpuGlBindBufferRange BindBufferRange = nullptr;
    IgpuGlGetIntegeri GetIntegeri = nullptr;
    IgpuGlGetInteger64i GetInteger64i = nullptr;
    IgpuGlDrawArraysInstanced DrawArraysInstanced = nullptr;
    IgpuGlVertexAttribDivisor VertexAttribDivisor = nullptr;
    IgpuGlDrawArraysIndirect DrawArraysIndirect = nullptr;
    IgpuGlDrawElementsIndirect DrawElementsIndirect = nullptr;
    IgpuGlGenQueries GenQueries = nullptr;
    IgpuGlDeleteQueries DeleteQueries = nullptr;
    IgpuGlBeginQuery BeginQuery = nullptr;
    IgpuGlEndQuery EndQuery = nullptr;
    IgpuGlGetQueryiv GetQueryiv = nullptr;
    IgpuGlGetQueryObjectiv GetQueryObjectiv = nullptr;
    IgpuGlGetQueryObjectuiv GetQueryObjectuiv = nullptr;
    IgpuGlGetQueryObjectui64v GetQueryObjectui64v = nullptr;
    IgpuGlQueryCounter QueryCounter = nullptr;
    IgpuGlFenceSync FenceSync = nullptr;
    IgpuGlClientWaitSync ClientWaitSync = nullptr;
    IgpuGlDeleteSync DeleteSync = nullptr;
    IgpuGlCreateShader CreateShader = nullptr;
    IgpuGlShaderSource ShaderSource = nullptr;
    IgpuGlCompileShader CompileShader = nullptr;
    IgpuGlDeleteShader DeleteShader = nullptr;
    IgpuGlCreateProgram CreateProgram = nullptr;
    IgpuGlAttachShader AttachShader = nullptr;
    IgpuGlGetAttribLocation GetAttribLocation = nullptr;
    IgpuGlGetActiveAttrib GetActiveAttrib = nullptr;
    IgpuGlBindAttribLocation BindAttribLocation = nullptr;
    IgpuGlLinkProgram LinkProgram = nullptr;
    IgpuGlUseProgram UseProgram = nullptr;
    IgpuGlDeleteProgram DeleteProgram = nullptr;
    IgpuGlGetAttachedShaders GetAttachedShaders = nullptr;
    IgpuGlGetShaderiv GetShaderiv = nullptr;
    IgpuGlGetProgramiv GetProgramiv = nullptr;
    IgpuGlGetShaderInfoLog GetShaderInfoLog = nullptr;
    IgpuGlGetProgramInfoLog GetProgramInfoLog = nullptr;
    IgpuGlGenBuffers GenBuffers = nullptr;
    IgpuGlBindBuffer BindBuffer = nullptr;
    IgpuGlBufferData BufferData = nullptr;
    IgpuGlBufferSubData BufferSubData = nullptr;
    IgpuGlGetBufferSubData GetBufferSubData = nullptr;
    IgpuGlGetBufferParameteriv GetBufferParameteriv = nullptr;
    IgpuGlCopyBufferSubData CopyBufferSubData = nullptr;
    IgpuGlDeleteBuffers DeleteBuffers = nullptr;
    IgpuGlGenFramebuffers GenFramebuffers = nullptr;
    IgpuGlBindFramebuffer BindFramebuffer = nullptr;
    IgpuGlFramebufferTexture2D FramebufferTexture2D = nullptr;
    IgpuGlDeleteFramebuffers DeleteFramebuffers = nullptr;
    IgpuGlCheckFramebufferStatus CheckFramebufferStatus = nullptr;
    IgpuGlEnableVertexAttribArray EnableVertexAttribArray = nullptr;
    IgpuGlDisableVertexAttribArray DisableVertexAttribArray = nullptr;
    IgpuGlGetVertexAttribiv GetVertexAttribiv = nullptr;
    IgpuGlGetVertexAttribPointerv GetVertexAttribPointerv = nullptr;
    IgpuGlVertexAttribIPointer VertexAttribIPointer = nullptr;
    IgpuGlVertexAttribPointer VertexAttribPointer = nullptr;
    IgpuGlGenVertexArrays GenVertexArrays = nullptr;
    IgpuGlDeleteVertexArrays DeleteVertexArrays = nullptr;
    IgpuGlBindVertexArray BindVertexArray = nullptr;
    IgpuGlGetUniformLocation GetUniformLocation = nullptr;
    IgpuGlUniform1i Uniform1i = nullptr;
    IgpuGlGetActiveUniform GetActiveUniform = nullptr;
    IgpuGlGenerateMipmap GenerateMipmap = nullptr;
    IgpuGlStencilFuncSeparate StencilFuncSeparate = nullptr;
    IgpuGlStencilOpSeparate StencilOpSeparate = nullptr;
    IgpuGlStencilMaskSeparate StencilMaskSeparate = nullptr;
    IgpuGlBlendFuncSeparate BlendFuncSeparate = nullptr;
    IgpuGlBlendEquationSeparate BlendEquationSeparate = nullptr;
    IgpuGlGenRenderbuffers GenRenderbuffers = nullptr;
    IgpuGlDeleteRenderbuffers DeleteRenderbuffers = nullptr;
    IgpuGlBindRenderbuffer BindRenderbuffer = nullptr;
    IgpuGlRenderbufferStorage RenderbufferStorage = nullptr;
    IgpuGlFramebufferRenderbuffer FramebufferRenderbuffer = nullptr;
};

IgpuGlFns& igpu_gl_fns();

namespace igpu
{
    bool gl_vertex_array_has_attributes(std::uint32_t name);
    bool gl_context_at_least(int major, int minor);
    bool gl_context_extension(const char* name);
}
