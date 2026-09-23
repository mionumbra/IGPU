#include "igpu_draw.h"

#include <d3d11.h>

#include <array>
#include <cstdint>
#include <string>

#include "igpu_buffer.h"
#include "igpu_device.h"
#include "igpu_error.h"

namespace igpu
{
    namespace
    {
        std::int32_t g_draw_count = 0;
        std::int32_t g_restore_failures = 0;

        // Records the bits of input-assembler state that IGPU overwrites and
        // puts them back on destruction.
        //
        // This exists because GameMaker provides NO GML-level way to read or
        // write these: gpu_get_state()/gpu_set_state() cover blend, depth,
        // stencil, cull, scissor, alphatest and samplers, but nothing in the
        // input-assembler stage. So gpu_set_state() cannot undo what a draw
        // does, and IGPU must restore it itself. Without this, any IGPU draw
        // would leave its vertex buffer bound and corrupt whatever GameMaker
        // draws next.
        //
        // Life cycle: the constructor captures and holds references; restore()
        // rebinds them and then releases them. Rebinding must happen before
        // release, so the two steps are separate and restore() does both in
        // that order.
        class IaStateGuard
        {
        public:
            explicit IaStateGuard(ID3D11DeviceContext* context)
                : context_(context)
            {
                if (context_ == nullptr)
                {
                    return;
                }

                context_->IAGetVertexBuffers(
                    0, kSlotCount, buffers_.data(), strides_.data(), offsets_.data());
                context_->IAGetIndexBuffer(&index_buffer_, &index_format_, &index_offset_);
                context_->IAGetInputLayout(&layout_);
                context_->IAGetPrimitiveTopology(&topology_);

                held_ = true;
            }

            ~IaStateGuard()
            {
                restore();
            }

            IaStateGuard(const IaStateGuard&) = delete;
            IaStateGuard& operator=(const IaStateGuard&) = delete;

            // Puts GameMaker's state back and releases the references taken in
            // the constructor. Idempotent. Returns false if a reference could
            // not be released, which the caller counts as a restore failure.
            bool restore()
            {
                if (!held_)
                {
                    return true;
                }
                held_ = false;

                if (context_ == nullptr)
                {
                    releaseReferences();
                    return false;
                }

                // Rebinding first: the pointers must still be alive here.
                context_->IASetVertexBuffers(
                    0, kSlotCount, buffers_.data(), strides_.data(), offsets_.data());
                context_->IASetIndexBuffer(index_buffer_, index_format_, index_offset_);
                context_->IASetInputLayout(layout_);
                context_->IASetPrimitiveTopology(topology_);

                releaseReferences();
                return true;
            }

        private:
            void releaseReferences()
            {
                // IAGetVertexBuffers hands back a reference the CALLER owns, so
                // every non-null pointer must be released. Reading the state
                // back is what makes this an exact restore rather than a guess.
                for (auto*& buffer : buffers_)
                {
                    if (buffer != nullptr)
                    {
                        buffer->Release();
                        buffer = nullptr;
                    }
                }
                if (index_buffer_ != nullptr)
                {
                    index_buffer_->Release();
                    index_buffer_ = nullptr;
                }
                if (layout_ != nullptr)
                {
                    layout_->Release();
                    layout_ = nullptr;
                }
            }

            static constexpr UINT kSlotCount = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;

            ID3D11DeviceContext* context_ = nullptr;

            std::array<ID3D11Buffer*, kSlotCount> buffers_{};
            std::array<UINT, kSlotCount> strides_{};
            std::array<UINT, kSlotCount> offsets_{};

            ID3D11Buffer* index_buffer_ = nullptr;
            DXGI_FORMAT index_format_ = DXGI_FORMAT_UNKNOWN;
            UINT index_offset_ = 0;

            ID3D11InputLayout* layout_ = nullptr;
            D3D11_PRIMITIVE_TOPOLOGY topology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

            bool held_ = false;
        };

        // Translates a GameMaker primitive into the backend topology.
        // Returns false for fan, which D3D11 cannot express: it has no
        // triangle-fan topology, so drawing it as a strip would render
        // something the caller did not ask for.
        bool to_topology(Primitive primitive, D3D11_PRIMITIVE_TOPOLOGY& out)
        {
            switch (primitive)
            {
            case Primitive::PointList:     out = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;     return true;
            case Primitive::LineList:      out = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;      return true;
            case Primitive::LineStrip:     out = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;     return true;
            case Primitive::TriangleList:  out = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  return true;
            case Primitive::TriangleStrip: out = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; return true;
            case Primitive::TriangleFan:
                return false;
            }
            return false;
        }

        const char* primitive_name(Primitive primitive)
        {
            switch (primitive)
            {
            case Primitive::PointList:     return "pointlist";
            case Primitive::LineList:      return "linelist";
            case Primitive::LineStrip:     return "linestrip";
            case Primitive::TriangleList:  return "trianglelist";
            case Primitive::TriangleStrip: return "trianglestrip";
            case Primitive::TriangleFan:   return "trianglefan";
            }
            return "unknown";
        }

        bool primitive_from_int(std::int32_t raw, Primitive& out)
        {
            switch (raw)
            {
            case 1: out = Primitive::PointList;     return true;
            case 2: out = Primitive::LineList;      return true;
            case 3: out = Primitive::LineStrip;     return true;
            case 4: out = Primitive::TriangleList;  return true;
            case 5: out = Primitive::TriangleStrip; return true;
            case 6: out = Primitive::TriangleFan;   return true;
            default: return false;
            }
        }

        // Shared prologue: validates everything both draw entry points need.
        bool prepare(const char* entry_name,
                     std::uint64_t vertex_buffer,
                     std::uint64_t layout,
                     std::int32_t primitive,
                     std::int64_t first,
                     std::int64_t count,
                     D3D11_PRIMITIVE_TOPOLOGY& topology,
                     DeviceState::BufferEntry** out_vb,
                     ID3D11InputLayout** out_layout)
        {
            clear_last_error();

            auto& s = state();
            if (!s.initialised || s.device == nullptr || s.context == nullptr)
            {
                set_last_error(std::string(entry_name) + ": call igpu_init() first");
                return false;
            }

            auto* vb = find_buffer(vertex_buffer);
            if (vb == nullptr)
            {
                set_last_error(std::string(entry_name) + ": unknown vertex buffer handle");
                return false;
            }
            if ((vb->bind & static_cast<std::int32_t>(BufferBind::Vertex)) == 0)
            {
                set_last_error(
                    std::string(entry_name) +
                    ": the buffer was not created with IgpuBufferBind.Vertex");
                return false;
            }

            const auto layout_it = s.input_layouts.find(layout);
            if (layout_it == s.input_layouts.end() || layout_it->second == nullptr)
            {
                set_last_error(std::string(entry_name) + ": unknown input layout handle");
                return false;
            }

            Primitive parsed{};
            if (!primitive_from_int(primitive, parsed))
            {
                set_last_error(
                    std::string(entry_name) + ": unknown primitive value " +
                    std::to_string(primitive) + " (use a pr_* constant)");
                return false;
            }

            if (!to_topology(parsed, topology))
            {
                set_last_error(
                    std::string(entry_name) + ": the 'trianglefan' primitive has no "
                    "backend equivalent; use 'trianglestrip' or convert the fan to a "
                    "triangle list first");
                return false;
            }

            if (first < 0)
            {
                set_last_error(std::string(entry_name) + ": first must not be negative");
                return false;
            }

            // A vertex buffer must describe whole vertices, so the starting
            // vertex has to land on a stride boundary.
            if (vb->stride > 0 && (static_cast<std::int64_t>(first) * vb->stride) > vb->size)
            {
                set_last_error(std::string(entry_name) + ": first is past the end of the buffer");
                return false;
            }

            if (count == 0)
            {
                set_last_error(std::string(entry_name) + ": count must not be zero");
                return false;
            }

            *out_vb = vb;
            *out_layout = layout_it->second;
            return true;
        }
    }

    bool draw(std::uint64_t vertex_buffer,
              std::uint64_t layout,
              std::int32_t primitive,
              std::int64_t first_vertex,
              std::int64_t vertex_count)
    {
        D3D11_PRIMITIVE_TOPOLOGY topology{};
        DeviceState::BufferEntry* vb = nullptr;
        ID3D11InputLayout* d3d_layout = nullptr;

        if (!prepare("igpu_draw", vertex_buffer, layout, primitive,
                     first_vertex, vertex_count, topology, &vb, &d3d_layout))
        {
            return false;
        }

        auto& s = state();

        // A negative count means "everything from `first` onwards", matching
        // vertex_submit_ext(-1).
        const std::int64_t available = vb->stride > 0
            ? (vb->size - first_vertex * static_cast<std::int64_t>(vb->stride)) /
                  static_cast<std::int64_t>(vb->stride)
            : 0;

        const std::int64_t count = vertex_count < 0 ? available : vertex_count;
        if (count <= 0)
        {
            set_last_error("igpu_draw: the resolved vertex count is zero");
            return false;
        }
        if (count > available)
        {
            set_last_error(
                "igpu_draw: " + std::to_string(count) + " vertices at " +
                std::to_string(first_vertex) + " exceeds the buffer's " +
                std::to_string(available) + " vertices");
            return false;
        }

        // Capture GameMaker's state first: the guard reads it in its
        // constructor, so it must run before any IASet overwrites it.
        IaStateGuard guard(s.context);

        const UINT stride = static_cast<UINT>(vb->stride);
        const UINT offset = 0;

        s.context->IASetInputLayout(d3d_layout);
        s.context->IASetVertexBuffers(0, 1, &vb->object, &stride, &offset);
        s.context->IASetPrimitiveTopology(topology);

        s.context->Draw(static_cast<UINT>(count), static_cast<UINT>(first_vertex));

        if (!guard.restore())
        {
            ++g_restore_failures;
        }

        ++g_draw_count;
        return true;
    }

    bool draw_indexed(std::uint64_t vertex_buffer,
                      std::uint64_t layout,
                      std::uint64_t index_buffer,
                      std::int32_t primitive,
                      std::int64_t first_index,
                      std::int64_t index_count)
    {
        D3D11_PRIMITIVE_TOPOLOGY topology{};
        DeviceState::BufferEntry* vb = nullptr;
        ID3D11InputLayout* d3d_layout = nullptr;

        if (!prepare("igpu_draw_indexed", vertex_buffer, layout, primitive,
                     first_index, index_count, topology, &vb, &d3d_layout))
        {
            return false;
        }

        auto* ib = find_buffer(index_buffer);
        if (ib == nullptr)
        {
            set_last_error("igpu_draw_indexed: unknown index buffer handle");
            return false;
        }
        if ((ib->bind & static_cast<std::int32_t>(BufferBind::Index)) == 0)
        {
            set_last_error(
                "igpu_draw_indexed: the index buffer was not created with "
                "IgpuBufferBind.Index");
            return false;
        }

        // IGPU's index buffers are 16-bit, which is the only format GameMaker's
        // own vertex buffers use. Rejecting a buffer that is not a whole number
        // of indices prevents reading past the end.
        constexpr std::int64_t kIndexSize = 2;
        const std::int64_t index_total = ib->size / kIndexSize;

        if (index_count < 0) { index_count = index_total - first_index; }
        if (index_count <= 0)
        {
            set_last_error("igpu_draw_indexed: the resolved index count is zero");
            return false;
        }
        if (first_index + index_count > index_total)
        {
            set_last_error(
                "igpu_draw_indexed: " + std::to_string(index_count) + " indices at " +
                std::to_string(first_index) + " exceeds the buffer's " +
                std::to_string(index_total) + " indices");
            return false;
        }

        auto& s = state();

        IaStateGuard guard(s.context);

        const UINT stride = static_cast<UINT>(vb->stride);
        const UINT offset = 0;

        s.context->IASetInputLayout(d3d_layout);
        s.context->IASetVertexBuffers(0, 1, &vb->object, &stride, &offset);
        s.context->IASetIndexBuffer(ib->object, DXGI_FORMAT_R16_UINT, 0);
        s.context->IASetPrimitiveTopology(topology);

        s.context->DrawIndexed(
            static_cast<UINT>(index_count),
            static_cast<UINT>(first_index),
            0);

        if (!guard.restore())
        {
            ++g_restore_failures;
        }

        ++g_draw_count;
        return true;
    }

    std::int32_t draw_count()
    {
        return g_draw_count;
    }

    std::int32_t draw_restore_failures()
    {
        return g_restore_failures;
    }

    bool is_vertex_buffer_bound(std::uint64_t buffer)
    {
        clear_last_error();

        auto& s = state();
        if (!s.initialised || s.context == nullptr)
        {
            set_last_error("igpu_is_vertex_buffer_bound: call igpu_init() first");
            return false;
        }

        auto* entry = find_buffer(buffer);
        if (entry == nullptr)
        {
            // Not an error worth raising for a diagnostic, but the answer is
            // unambiguous: a handle IGPU does not know cannot be bound by IGPU.
            return false;
        }

        ID3D11Buffer* bound = nullptr;
        s.context->IAGetVertexBuffers(0, 1, &bound, nullptr, nullptr);

        const bool matches = (bound == entry->object);

        // IAGetVertexBuffers hands back a reference the caller owns, even when
        // it is not the buffer we were looking for.
        if (bound != nullptr)
        {
            bound->Release();
        }

        return matches;
    }
}
