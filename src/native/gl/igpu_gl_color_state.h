#pragma once
#include "igpu_gl_loader.h"
#include <array>
#include <vector>
namespace igpu
{
    void gl_color_state_initialize();
    bool gl_indexed_blend_supported();
    // Global GL blend/mask setters affect every native draw-buffer slot.
    class GlColorState
    {
    public:
        bool capture(bool blending);
        bool restore()const;
    private:
        struct Slot
        {
            GLboolean enabled=GL_FALSE;
            std::array<GLboolean,4> mask{};
            std::array<GLint,6> functions{};
        };
        bool blending_=false;
        std::vector<Slot> slots_;
    };
}
