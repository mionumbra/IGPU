#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace igpu
{
    // Observe only: native state and pending GL errors are never changed here.
    class GlDrawInputState
    {
    public:
        GlDrawInputState();
        bool matches_current() const;
    private:
        std::array<std::int32_t, 4> bindings_{};
        std::vector<std::array<std::int32_t, 9>> attributes_;
        std::vector<void*> pointers_;
        std::uint32_t features_ = 0;
    };
    std::int32_t gl_diagnostic_draw_count();
    std::int32_t gl_diagnostic_restore_failures();
    void gl_diagnostic_record(bool accepted, bool restored);

    template<class Submit> bool gl_diagnostic_submit(Submit submit)
    {
        const GlDrawInputState before;
        const bool accepted = submit();
        gl_diagnostic_record(accepted, before.matches_current());
        return accepted;
    }
}
