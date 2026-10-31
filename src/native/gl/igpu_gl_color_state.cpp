#include "igpu_gl_color_state.h"
namespace igpu
{
    namespace
    {
        bool indexed_context=false,blend_context=false;
        GLint max_slots=0;
        constexpr GLenum fields[]={0x80C9,0x80C8,0x80CB,0x80CA,0x8009,0x883D};
        bool masks_supported()
        {const auto&f=igpu_gl_fns();return indexed_context&&max_slots>0&&f.GetBooleani&&f.ColorMaski;}
    }
    void gl_color_state_initialize()
    {
        indexed_context=gl_context_at_least(3,0);
        blend_context=indexed_context&&(gl_context_at_least(4,0)||gl_context_extension("GL_ARB_draw_buffers_blend"));
        max_slots=0;if(indexed_context)glGetIntegerv(0x8824,&max_slots);
    }
    bool gl_indexed_blend_supported()
    {
        const auto&f=igpu_gl_fns();return masks_supported()&&blend_context&&f.GetIntegeri&&f.IsEnabledi&&f.Enablei&&f.Disablei&&f.BlendFuncSeparatei&&f.BlendEquationSeparatei;
    }
    bool GlColorState::capture(bool blending)
    {
        slots_.clear();blending_=blending;
        if(!masks_supported()||(blending&&!gl_indexed_blend_supported()))return false;
        auto&f=igpu_gl_fns();slots_.resize(max_slots);
        for(GLuint i=0;i<slots_.size();++i)
        {
            auto&v=slots_[i];f.GetBooleani(GL_COLOR_WRITEMASK,i,v.mask.data());
            if(blending_){v.enabled=f.IsEnabledi(GL_BLEND,i);for(int k=0;k<6;++k)f.GetIntegeri(fields[k],i,&v.functions[k]);}
        }
        return true;
    }
    bool GlColorState::restore()const
    {
        if(slots_.empty())return false;
        auto&f=igpu_gl_fns();
        for(GLuint i=0;i<slots_.size();++i)
        {
            const auto&v=slots_[i];f.ColorMaski(i,v.mask[0],v.mask[1],v.mask[2],v.mask[3]);
            if(blending_){(v.enabled?f.Enablei:f.Disablei)(GL_BLEND,i);f.BlendFuncSeparatei(i,v.functions[0],v.functions[1],v.functions[2],v.functions[3]);f.BlendEquationSeparatei(i,v.functions[4],v.functions[5]);}
        }
        return true;
    }
}
