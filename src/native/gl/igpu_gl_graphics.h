#pragma once
#include <cstdint>
namespace igpu
{
    void gl_graphics_initialize();
    bool gl_graphics_stage_supported(std::int32_t stage);
    bool gl_geometry_supported();
    bool gl_tessellation_supported();
    bool gl_graphics_switch_allowed(const char*entry);
    void gl_graphics_program_linked(std::uint32_t program,bool geometry,bool tessellation);
    bool gl_graphics_draw_allowed(std::uint32_t mode,const char*entry);
    bool gl_patch_begin(std::int32_t points,std::int32_t&previous);
    void gl_patch_end(std::int32_t previous);
}
