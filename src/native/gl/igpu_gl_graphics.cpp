#include "igpu_gl_graphics.h"
#include "igpu_gl_loader.h"
#include "igpu_gl_compute.h"
#include "igpu_error.h"
#include <string>
#include <vector>
namespace igpu
{
    namespace{bool geometry=false,tessellation=false,feedback=false;GLint max_patch=0;bool fail(const char*entry,const char*message){set_last_error(std::string(entry)+": "+message);return false;}}
    void gl_graphics_initialize()
    {
        feedback=gl_context_at_least(4,0)||gl_context_extension("GL_ARB_transform_feedback2");
        geometry=feedback&&gl_context_at_least(3,2);tessellation=feedback&&(gl_context_at_least(4,0)||gl_context_extension("GL_ARB_tessellation_shader"));
        max_patch=0;if(tessellation)glGetIntegerv(0x8E7D,&max_patch);
    }
    bool gl_geometry_supported(){return geometry;}
    bool gl_tessellation_supported(){return tessellation&&max_patch>0&&igpu_gl_fns().PatchParameteri;}
    bool gl_graphics_stage_supported(std::int32_t stage)
    {return stage==0||stage==1||(stage==2&&gl_compute_supported())||(stage==3&&gl_geometry_supported())||((stage==4||stage==5)&&gl_tessellation_supported());}
    bool gl_graphics_switch_allowed(const char*entry)
    {
        if(feedback){GLboolean active=GL_FALSE,paused=GL_FALSE;glGetBooleanv(0x8E24,&active);glGetBooleanv(0x8E23,&paused);if(active&&!paused)return fail(entry,"pause active transform feedback before changing the graphics program");}return true;
    }
    bool gl_patch_begin(std::int32_t points,std::int32_t&previous)
    {
        if(!gl_tessellation_supported())return fail("igpu_draw_patch","this context does not support tessellation patch drawing");
        if(points<1||points>32||points>max_patch)return fail("igpu_draw_patch","control points must be 1..32 and fit the native context limit");
        GLint program=0,linked=0;glGetIntegerv(0x8B8D,&program);bool control=false,evaluation=false;
        if(program)
        {
            auto&f=igpu_gl_fns();f.GetProgramiv(program,0x8B82,&linked);
            if(linked){GLint count=0;f.GetProgramiv(program,0x8B85,&count);std::vector<GLuint>attached(static_cast<std::size_t>(count));GLsizei returned=0;f.GetAttachedShaders(program,count,&returned,attached.data());
                for(GLsizei i=0;i<returned;++i){GLint type=0;f.GetShaderiv(attached[i],0x8B4F,&type);control=control||type==0x8E88;evaluation=evaluation||type==0x8E87;}}
        }
        if(!linked||!control||!evaluation)return fail("igpu_draw_patch","the current native program must contain linked hull and domain shaders");
        glGetIntegerv(0x8E72,&previous);igpu_gl_fns().PatchParameteri(0x8E72,points);return true;
    }
    void gl_patch_end(std::int32_t previous){igpu_gl_fns().PatchParameteri(0x8E72,previous);}
}
