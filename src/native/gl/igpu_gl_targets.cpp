#include "igpu_gl_targets.h"
#include "igpu_gl_draw.h"
#include "igpu_gl_loader.h"
#include "igpu_gl_color_state.h"
#include "igpu_error.h"
#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace igpu
{
    namespace
    {
        constexpr GLenum draw_framebuffer=0x8CA9,color0=0x8CE0;
        bool target_context=false;
        GLint max_targets=0,max_buffers=0;
        bool fail(const char*message){set_last_error(std::string("igpu_draw_to_render_targets: ")+message);return false;}
        bool integer(const gm::wire::GMValue&value,std::int64_t&out)
        {
            if(value.is<std::uint64_t>())
            {const auto n=value.as<std::uint64_t>();if(n>static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))return false;out=static_cast<std::int64_t>(n);return true;}
            if(value.is<std::int32_t>()){out=value.as<std::int32_t>();return true;}
            if(value.is<double>())
            {const auto n=value.as<double>();if(!std::isfinite(n)||std::trunc(n)!=n||n < -9223372036854775808.0||n >= 9223372036854775808.0)return false;out=static_cast<std::int64_t>(n);return true;}
            return false;
        }
        struct Framebuffer
        {
            GLint previous=0,viewport[4]{};
            GLuint owned=0,texture=0;
            Framebuffer(){glGetIntegerv(0x8CA6,&previous);glGetIntegerv(GL_VIEWPORT,viewport);}
            ~Framebuffer()
            {
                auto&f=igpu_gl_fns();
                if(texture)f.FramebufferTexture2D(draw_framebuffer,color0,GL_TEXTURE_2D,texture,0);
                f.BindFramebuffer(draw_framebuffer,static_cast<GLuint>(previous));
                glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
                if(owned)f.DeleteFramebuffers(1,&owned);
            }
        };
    }
    void gl_targets_initialize()
    {
        gl_color_state_initialize();
        target_context=gl_context_at_least(3,0);max_targets=max_buffers=0;
        if(target_context){glGetIntegerv(0x8CDF,&max_targets);glGetIntegerv(0x8824,&max_buffers);}
    }
    bool gl_multiple_targets_supported()
    {return target_context&&max_targets>=4&&max_buffers>=4&&igpu_gl_fns().DrawBuffers&&gl_indexed_blend_supported();}
    bool gl_draw_targets(std::uint64_t vertex,std::uint64_t layout,std::int32_t primitive,
                         std::int64_t first,std::int64_t count,const gm::wire::GMArrayView&targets,
                         const gm::wire::GMArrayView*layers,const gm::wire::GMArrayView*mips,
                         std::int64_t blend,std::int64_t depth,std::int64_t raster,std::int64_t sampler)
    {
        const auto n=targets.size();
        if(n<1||n>4)return fail("pass 1 to 4 render-target textures");
        if((layers&&layers->size()!=n)||(mips&&mips->size()!=n))return fail("the lists must be the same length");
        if(n>1&&!gl_multiple_targets_supported())return fail("this context does not support multiple color targets");
        std::array<GlColorTarget,4> resolved{};
        std::array<std::int64_t,4> handles{},levels{};
        for(std::size_t i=0;i<n;++i)
        {
            std::int64_t layer=0;
            if(!integer(targets[i],handles[i])||(layers&&!integer((*layers)[i],layer))||(mips&&!integer((*mips)[i],levels[i]))||handles[i]<=0||layer!=0||levels[i]<0)
                return fail("targets and mips must be valid integers; only 2D layer zero is supported");
            for(std::size_t j=0;j<i;++j)if(handles[i]==handles[j]&&levels[i]==levels[j])return fail("the same texture subresource cannot appear twice");
            if(!gl_resolve_color_target(static_cast<std::uint64_t>(handles[i]),levels[i],resolved[i]))return false;
            if(i&&(resolved[i].width!=resolved[0].width||resolved[i].height!=resolved[0].height))return fail("selected target dimensions must match");
        }
        if(!gl_target_vertex_range(vertex,first,count))return false;
        auto&f=igpu_gl_fns();Framebuffer saved;
        if(n==1)
        {
            f.BindFramebuffer(draw_framebuffer,resolved[0].framebuffer);
            if(levels[0]){saved.texture=resolved[0].texture;f.FramebufferTexture2D(draw_framebuffer,color0,GL_TEXTURE_2D,saved.texture,static_cast<GLint>(levels[0]));}
        }
        else
        {
            f.GenFramebuffers(1,&saved.owned);if(!saved.owned)return fail("cannot create the temporary framebuffer");
            f.BindFramebuffer(draw_framebuffer,saved.owned);
            std::array<GLenum,4> outputs{};
            for(std::size_t i=0;i<n;++i){outputs[i]=color0+static_cast<GLenum>(i);f.FramebufferTexture2D(draw_framebuffer,outputs[i],GL_TEXTURE_2D,resolved[i].texture,static_cast<GLint>(levels[i]));}
            f.DrawBuffers(static_cast<GLsizei>(n),outputs.data());
        }
        if(f.CheckFramebufferStatus(draw_framebuffer)!=0x8CD5)return fail("the selected framebuffer is incomplete");
        glViewport(0,0,resolved[0].width,resolved[0].height);
        return gl_draw(vertex,layout,primitive,first,count,blend,depth,raster,sampler);
    }
}
