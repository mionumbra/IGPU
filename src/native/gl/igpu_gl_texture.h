#pragma once
#include "igpu_gl_shadow.h"
#include <cstdint>
namespace igpu
{
    struct GlTexture
    {
        GLuint texture=0,framebuffer=0,depth=0;
        std::int32_t width=0,height=0,mips=1,format=6;
        bool render_target=true;
        GlShadow shadow{};
        bool storage=false;
        std::int32_t kind=0,layers=1;
        GLenum target=GL_TEXTURE_2D,binding=GL_TEXTURE_BINDING_2D;
    };
    void gl_textures_initialize();
    void gl_textures_release();
    bool gl_texture_kind_supported(std::int32_t kind);
    GlTexture* gl_find_texture(std::uint64_t handle);
    struct GlColorTarget;
    bool gl_resolve_color_subresource(std::uint64_t handle,std::int64_t layer,std::int64_t mip,GlColorTarget&out);
    void gl_attach_color(std::uint32_t framebuffer_target,std::uint32_t attachment,const GlColorTarget&target);
}
