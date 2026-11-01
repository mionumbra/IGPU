#include "igpu_gl_texture.h"
#include "igpu_gl_draw.h"
#include "igpu_gl_targets.h"
#include "igpu_gl_compute.h"
#include "igpu_gl_color_state.h"
#include "igpu_gl_loader.h"
#include "igpu_error.h"
#include <algorithm>
#include <string>
#include <unordered_map>

namespace igpu
{
    namespace
    {
        constexpr GLenum draw_fbo=0x8CA9,read_fbo=0x8CA8,color0=0x8CE0,complete=0x8CD5;
        constexpr GLenum base_level=0x813C,max_level=0x813D;
        std::unordered_map<std::uint64_t,GlTexture> textures;
        std::uint64_t next_texture=1;
        bool context=false;
        GLint max_size=0,max_3d=0,max_array=0,max_cube=0;
        struct TextureFormat { GLint internal; GLenum channels; GLenum type; };

        bool texture_format(std::int32_t format, TextureFormat& out)
        {
            switch (format)
            {
            case 6:  out = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}; return true;
            case 9:  out = {0x822D /* R16F */, GL_RED, GL_FLOAT}; return true;
            case 10: out = {0x822E /* R32F */, GL_RED, GL_FLOAT}; return true;
            case 11: out = {GL_RGBA4, GL_RGBA, GL_UNSIGNED_BYTE}; return true;
            case 12: out = {0x8229 /* R8 */, GL_RED, GL_UNSIGNED_BYTE}; return true;
            case 13: out = {0x822B /* RG8 */, 0x8227 /* RG */, GL_UNSIGNED_BYTE}; return true;
            case 14: out = {0x881A /* RGBA16F */, GL_RGBA, GL_FLOAT}; return true;
            case 15: out = {0x8814 /* RGBA32F */, GL_RGBA, GL_FLOAT}; return true;
            default: return false;
            }
        }
        std::int32_t gl_mip_size(std::int32_t full, std::int32_t mip)
        {
            std::int32_t size = full;
            for (std::int32_t level = 0; level < mip; ++level)
            {
                size = size > 1 ? size / 2 : 1;
            }
            return size;
        }


        bool fail(const char*entry,const char*message){set_last_error(std::string(entry)+": "+message);return false;}
        GLenum native_target(int kind){return kind==1?0x806F:kind==2?0x8C1A:kind==3?0x8513:GL_TEXTURE_2D;}
        GLenum native_binding(int kind){return kind==1?0x806A:kind==2?0x8C1D:kind==3?0x8514:GL_TEXTURE_BINDING_2D;}
        int level_layers(const GlTexture&t,int mip){return t.kind==1?gl_mip_size(t.layers,mip):t.layers;}
        bool resolve(const GlTexture&t,std::int64_t layer,std::int64_t mip,GlColorTarget&out)
        {
            if(mip<0||mip>=t.mips||layer<0||layer>=level_layers(t,static_cast<int>(mip)))return false;
            out={t.texture,t.framebuffer,gl_mip_size(t.width,static_cast<int>(mip)),gl_mip_size(t.height,static_cast<int>(mip)),
                 t.kind==3?static_cast<GLenum>(0x8515+layer):GL_TEXTURE_2D,static_cast<int>(layer),static_cast<int>(mip),t.kind==1||t.kind==2};return true;
        }
        struct AllocationState
        {
            GLenum target,binding;GLint texture=0,draw=0,unpack=0;GLfloat clear[4]{};GLboolean mask[4]{},scissor=0;GlColorState indexed;
            AllocationState(GLenum t,GLenum b):target(t),binding(b)
            {glGetIntegerv(binding,&texture);glGetIntegerv(0x8CA6,&draw);glGetIntegerv(0x88EF,&unpack);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);glGetBooleanv(GL_COLOR_WRITEMASK,mask);scissor=glIsEnabled(GL_SCISSOR_TEST);indexed.capture(false);igpu_gl_fns().BindBuffer(0x88EC,0);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);}
            ~AllocationState()
            {auto&f=igpu_gl_fns();f.BindFramebuffer(draw_fbo,draw);glBindTexture(target,texture);f.BindBuffer(0x88EC,unpack);glClearColor(clear[0],clear[1],clear[2],clear[3]);if(!indexed.restore())glColorMask(mask[0],mask[1],mask[2],mask[3]);if(scissor)glEnable(GL_SCISSOR_TEST);}
        };
        struct ReadState
        {
            GLint read=0,pack=0;static constexpr GLenum fields[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_SWAP_BYTES,GL_PACK_LSB_FIRST};GLint values[6]{};GLuint owned=0;
            ReadState(){glGetIntegerv(0x8CAA,&read);glGetIntegerv(0x88ED,&pack);for(int i=0;i<6;++i){glGetIntegerv(fields[i],&values[i]);glPixelStorei(fields[i],i?0:1);}igpu_gl_fns().BindBuffer(0x88EB,0);}
            ~ReadState(){auto&f=igpu_gl_fns();f.BindFramebuffer(read_fbo,read);if(owned)f.DeleteFramebuffers(1,&owned);f.BindBuffer(0x88EB,pack);for(int i=0;i<6;++i)glPixelStorei(fields[i],values[i]);}
        };
    }
    void gl_textures_initialize()
    {
        context=gl_context_at_least(3,0);max_size=max_3d=max_array=max_cube=0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_size);if(context){glGetIntegerv(0x8073,&max_3d);glGetIntegerv(0x88FF,&max_array);glGetIntegerv(0x851C,&max_cube);}
    }
    bool gl_texture_kind_supported(std::int32_t kind)
    {
        if(kind==0)return max_size>0;
        if(!context)return false;
        if(kind==3)return max_cube>0;
        const auto&f=igpu_gl_fns();return (kind==1||kind==2)&&f.TexImage3D&&f.FramebufferTextureLayer&&(kind==1?max_3d>0:max_array>0);
    }
    GlTexture* gl_find_texture(std::uint64_t handle){const auto i=textures.find(handle);return i==textures.end()?nullptr:&i->second;}
    void gl_attach_color(std::uint32_t target,std::uint32_t attachment,const GlColorTarget&t)
    {auto&f=igpu_gl_fns();if(t.layered)f.FramebufferTextureLayer(target,attachment,t.texture,t.mip,t.layer);else f.FramebufferTexture2D(target,attachment,t.attachment_target,t.texture,t.mip);}
    bool gl_resolve_color_subresource(std::uint64_t handle,std::int64_t layer,std::int64_t mip,GlColorTarget&out)
    {const auto*t=gl_find_texture(handle);if(!t||!t->render_target||!resolve(*t,layer,mip,out))return fail("igpu_draw_to_render_targets","unknown, non-target texture or invalid layer/mip");return true;}
    bool gl_resolve_color_target(std::uint64_t handle,std::int64_t mip,GlColorTarget&out){return gl_resolve_color_subresource(handle,0,mip,out);}
    bool gl_texture_format_supported(std::int32_t format){TextureFormat native{};return texture_format(format,native);}
    std::int64_t gl_texture_create_kind(std::int32_t kind,std::int32_t width,std::int32_t height,std::int32_t depth,std::int32_t format,bool target,bool storage)
    {const auto h=gl_texture_create_mips(kind,width,height,depth,format,storage,1);if(h)textures.at(h).render_target=target;return h;}
    std::int64_t gl_texture_create_mips(std::int32_t kind,std::int32_t width,std::int32_t height,std::int32_t depth,std::int32_t format,bool storage,std::int32_t mip_count)
    {
        const char*entry="igpu_texture_create_mips";TextureFormat native{};
        if(!gl_texture_kind_supported(kind)||!texture_format(format,native)||width<=0||height<=0||depth<=0||width>16384||height>16384||
           (kind==0&&depth!=1)||(kind==3&&(depth!=6||width!=height))||(storage&&(!gl_compute_supported()||format==11)))
        {fail(entry,"unsupported texture kind/format/context or invalid dimensions");return 0;}
        const int limit=kind==1?max_3d:kind==3?max_cube:max_size;
        if(width>limit||height>limit||(kind==1&&depth>max_3d)||(kind==2&&depth>max_array)){fail(entry,"dimensions exceed the native context limits");return 0;}
        int chain=1,edge=(std::max)(width,height);if(kind==1)edge=(std::max)(edge,depth);while(edge>1){edge/=2;++chain;}
        if(mip_count!=0&&(mip_count<1||mip_count>chain)){fail(entry,"mip count must be zero for the full chain, or fit that chain");return 0;}
        GlTexture t;t.width=width;t.height=height;t.layers=depth;t.kind=kind;t.format=format;t.storage=storage;t.mips=mip_count?mip_count:chain;t.target=native_target(kind);t.binding=native_binding(kind);
        AllocationState saved(t.target,t.binding);auto&f=igpu_gl_fns();glGenTextures(1,&t.texture);if(!t.texture){fail(entry,"cannot create the texture");return 0;}glBindTexture(t.target,t.texture);
        for(int mip=0;mip<t.mips;++mip)
        {
            const int w=gl_mip_size(width,mip),h=gl_mip_size(height,mip);
            if(kind==1||kind==2)f.TexImage3D(t.target,mip,native.internal,w,h,level_layers(t,mip),0,native.channels,native.type,nullptr);
            else for(int face=0;face<(kind==3?6:1);++face)glTexImage2D(kind==3?0x8515+face:GL_TEXTURE_2D,mip,native.internal,w,h,0,native.channels,native.type,nullptr);
        }
        glTexParameteri(t.target,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);glTexParameteri(t.target,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(t.target,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(t.target,GL_TEXTURE_WRAP_T,0x812F);if(kind)glTexParameteri(t.target,0x8072,0x812F);glTexParameteri(t.target,base_level,0);glTexParameteri(t.target,max_level,t.mips-1);
        f.GenFramebuffers(1,&t.framebuffer);bool ok=t.framebuffer!=0;
        if(ok){f.BindFramebuffer(draw_fbo,t.framebuffer);for(int mip=0;ok&&mip<t.mips;++mip)for(int layer=0;ok&&layer<level_layers(t,mip);++layer){GlColorTarget view;resolve(t,layer,mip,view);gl_attach_color(draw_fbo,color0,view);ok=f.CheckFramebufferStatus(draw_fbo)==complete;if(ok){glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);}}}
        if(!ok){if(t.framebuffer)f.DeleteFramebuffers(1,&t.framebuffer);glDeleteTextures(1,&t.texture);fail(entry,"the selected texture framebuffer is incomplete");return 0;}
        GlColorTarget base;resolve(t,0,0,base);gl_attach_color(draw_fbo,color0,base);
        const auto handle=next_texture++;textures.emplace(handle,std::move(t));return static_cast<std::int64_t>(handle);
    }
    bool gl_texture_generate_mips(std::uint64_t handle)
    {
        auto*t=gl_find_texture(handle);if(!t||t->mips<=1)return fail("igpu_texture_generate_mips","a texture with an owned mip chain is required");
        GLint previous=0,base=0,max=0;glGetIntegerv(t->binding,&previous);glBindTexture(t->target,t->texture);glGetTexParameteriv(t->target,base_level,&base);glGetTexParameteriv(t->target,max_level,&max);glTexParameteri(t->target,base_level,0);glTexParameteri(t->target,max_level,t->mips-1);igpu_gl_fns().GenerateMipmap(t->target);glTexParameteri(t->target,base_level,base);glTexParameteri(t->target,max_level,max);glBindTexture(t->target,previous);return true;
    }
    bool gl_texture_release(std::uint64_t handle)
    {
        auto*t=gl_find_texture(handle);if(!t)return fail("igpu_texture_release","unknown texture handle");auto&f=igpu_gl_fns();f.DeleteFramebuffers(1,&t->framebuffer);if(t->depth)f.DeleteRenderbuffers(1,&t->depth);gl_shadow_release(t->shadow);glDeleteTextures(1,&t->texture);textures.erase(handle);return true;
    }
    void gl_textures_release(){while(!textures.empty())gl_texture_release(textures.begin()->first);next_texture=1;}
    std::int64_t gl_texture_read_level(std::uint64_t handle,std::int32_t x,std::int32_t y,std::int32_t layer,std::int32_t mip)
    {
        const auto*t=gl_find_texture(handle);GlColorTarget view;
        if(!t||t->format!=6||!resolve(*t,layer,mip,view)||x<0||y<0||x>=view.width||y>=view.height){fail("igpu_texture_read","the pixel is outside this RGBA8 subresource");return 0;}
        ReadState saved;auto&f=igpu_gl_fns();f.GenFramebuffers(1,&saved.owned);if(!saved.owned){fail("igpu_texture_read","cannot create the read framebuffer");return 0;}f.BindFramebuffer(read_fbo,saved.owned);gl_attach_color(read_fbo,color0,view);glReadBuffer(color0);
        if(f.CheckFramebufferStatus(read_fbo)!=complete){fail("igpu_texture_read","the read framebuffer is incomplete");return 0;}
        unsigned char pixel[4]{};glReadPixels(x,view.height-1-y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);return pixel[0]|(pixel[1]<<8)|(pixel[2]<<16);
    }
    bool gl_storage_texture(std::uint64_t handle,std::int32_t mip,GlStorageTexture&out,const char*entry)
    {
        const auto*t=gl_find_texture(handle);if(!t||!t->storage||mip<0||mip>=t->mips)return fail(entry,"unknown storage texture or mip level");TextureFormat format{};if(!texture_format(t->format,format)||t->format==11)return false;out={t->texture,static_cast<std::uint32_t>(format.internal),mip,t->kind!=0};return true;
    }
    bool gl_color_target_begin(std::uint64_t texture, std::int32_t& previous_framebuffer,
                               std::int32_t previous_viewport[4])
    {
        const auto it = textures.find(texture);
        if (it == textures.end())
        {
            set_last_error("gl_color_target_begin: unknown texture");
            return false;
        }
        glGetIntegerv(0x8CA6, &previous_framebuffer);
        glGetIntegerv(GL_VIEWPORT, previous_viewport);
        igpu_gl_fns().BindFramebuffer(0x8D40, it->second.framebuffer);
        glViewport(0, 0, it->second.width, it->second.height);
        return true;
    }

    void gl_color_target_end(std::int32_t previous_framebuffer, const std::int32_t previous_viewport[4])
    {
        igpu_gl_fns().BindFramebuffer(0x8D40, static_cast<GLuint>(previous_framebuffer));
        glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    }

    bool gl_attach_depth(std::uint64_t texture)
    {
        const auto it = textures.find(texture);
        if (it == textures.end())
        {
            set_last_error("gl_attach_depth: unknown texture");
            return false;
        }
        auto& object = it->second;
        const auto& fns = igpu_gl_fns();
        GLint previous = 0;
        glGetIntegerv(0x8CA6, &previous);
        if (object.depth == 0)
        {
            fns.GenRenderbuffers(1, &object.depth);
            fns.BindRenderbuffer(0x8D41, object.depth);
            fns.RenderbufferStorage(0x8D41, 0x81A6, object.width, object.height);
            fns.BindRenderbuffer(0x8D41, 0);
            fns.BindFramebuffer(0x8D40, object.framebuffer);
            fns.FramebufferRenderbuffer(0x8D40, 0x8D00, 0x8D41, object.depth);
            const GLenum status = fns.CheckFramebufferStatus(0x8D40);
            if (status != 0x8CD5)
            {
                fns.BindFramebuffer(0x8D40, static_cast<GLuint>(previous));
                fns.DeleteRenderbuffers(1, &object.depth);
                object.depth = 0;
                set_last_error("gl_attach_depth: the framebuffer is incomplete");
                return false;
            }
        }
        else
        {
            fns.BindFramebuffer(0x8D40, object.framebuffer);
        }
        // glClear of the depth buffer honors the write mask. Force it for this clear only.
        GLboolean previous_mask = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &previous_mask);
        glDepthMask(GL_TRUE);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glDepthMask(previous_mask);
        fns.BindFramebuffer(0x8D40, static_cast<GLuint>(previous));
        return true;
    }

}
