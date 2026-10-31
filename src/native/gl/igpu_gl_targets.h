#pragma once
#include "core/GMExtWire.h"
#include <cstdint>
namespace igpu
{
    struct GlColorTarget
    {
        std::uint32_t texture=0,framebuffer=0;
        std::int32_t width=0,height=0;
        std::uint32_t attachment_target=0x0DE1;
        std::int32_t layer=0,mip=0;
        bool layered=false;
    };
    void gl_targets_initialize();
    bool gl_multiple_targets_supported();
    bool gl_resolve_color_target(std::uint64_t handle,std::int64_t mip,GlColorTarget&out);
    bool gl_target_vertex_range(std::uint64_t vertex,std::int64_t first,std::int64_t&count);
    bool gl_draw_targets(std::uint64_t vertex,std::uint64_t layout,std::int32_t primitive,
                         std::int64_t first,std::int64_t count,const gm::wire::GMArrayView&targets,
                         const gm::wire::GMArrayView*layers,const gm::wire::GMArrayView*mips,
                         std::int64_t blend=0,std::int64_t depth=0,std::int64_t raster=0,std::int64_t sampler=0);
}
