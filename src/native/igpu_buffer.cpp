#include "igpu_buffer.h"

#include <d3d11.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "igpu_device.h"
#include "igpu_error.h"

namespace igpu
{
    namespace
    {
        // Translates the backend-neutral usage into D3D11's placement model.
        // This is the only place that knows the mapping, so callers never see
        // D3D11_USAGE_* and a future backend only has to replace this function.
        //
        // NOTE: Static maps to DEFAULT, not IMMUTABLE. IGPU's API is
        // create-then-write, and an IMMUTABLE buffer cannot be filled in after
        // creation (D3D11 requires its initial data up front), so IMMUTABLE
        // would make Static buffers impossible to populate. DEFAULT keeps them
        // GPU-only for reads while still accepting one update.
        D3D11_USAGE to_d3d_usage(BufferUsage usage)
        {
            switch (usage)
            {
            case BufferUsage::Static:  return D3D11_USAGE_DEFAULT;
            case BufferUsage::Dynamic: return D3D11_USAGE_DYNAMIC;
            case BufferUsage::Staging: return D3D11_USAGE_STAGING;
            }
            return D3D11_USAGE_DEFAULT;
        }

        bool usage_from_int(std::int32_t raw, BufferUsage& out)
        {
            switch (raw)
            {
            case 0: out = BufferUsage::Static;  return true;
            case 1: out = BufferUsage::Dynamic; return true;
            case 2: out = BufferUsage::Staging; return true;
            default: return false;
            }
        }

        // Maps the neutral bind flags onto D3D11's bind flags. Returns false if
        // the combination is not one D3D11 can express, so the caller gets a
        // clear error instead of a buffer that silently cannot be used.
        bool to_d3d_bind(std::int32_t bind, UINT& out)
        {
            if (bind == 0)
            {
                return false;
            }

            UINT flags = 0;
            if (bind & static_cast<std::int32_t>(BufferBind::Vertex))  flags |= D3D11_BIND_VERTEX_BUFFER;
            if (bind & static_cast<std::int32_t>(BufferBind::Index))   flags |= D3D11_BIND_INDEX_BUFFER;
            if (bind & static_cast<std::int32_t>(BufferBind::Uniform)) flags |= D3D11_BIND_CONSTANT_BUFFER;
            if (bind & static_cast<std::int32_t>(BufferBind::Storage)) flags |= D3D11_BIND_SHADER_RESOURCE;

            // Reject unknown bits rather than silently dropping them: a caller
            // that passes a typo would otherwise get a buffer missing a use it
            // asked for, and only find out at draw time.
            const std::int32_t known =
                static_cast<std::int32_t>(BufferBind::Vertex) |
                static_cast<std::int32_t>(BufferBind::Index) |
                static_cast<std::int32_t>(BufferBind::Uniform) |
                static_cast<std::int32_t>(BufferBind::Storage);

            if ((bind & ~known) != 0)
            {
                return false;
            }

            out = flags;
            return flags != 0;
        }
    }

    std::int64_t buffer_create(std::int64_t size, std::int32_t usage, std::int32_t bind,
                               std::int32_t stride)
    {
        clear_last_error();

        auto& s = state();
        if (!s.initialised || s.device == nullptr)
        {
            set_last_error("igpu_buffer_create: call igpu_init() first");
            return 0;
        }

        if (size <= 0)
        {
            set_last_error("igpu_buffer_create: size must be positive");
            return 0;
        }

        BufferUsage parsed_usage{};
        if (!usage_from_int(usage, parsed_usage))
        {
            set_last_error(
                "igpu_buffer_create: unknown usage value " + std::to_string(usage) +
                " (use IgpuBufferUsage)");
            return 0;
        }

        // A staging buffer is purely a CPU-side readback target: D3D11 requires
        // it to have NO bind flags at all. Asking for both is a caller mistake,
        // not something to silently half-honour. This check comes first because
        // a staging buffer legitimately passes bind = 0 (IgpuBufferBind.None),
        // which the bind validation below would otherwise reject.
        if (parsed_usage == BufferUsage::Staging)
        {
            if (bind != static_cast<std::int32_t>(BufferBind::None))
            {
                set_last_error(
                    "igpu_buffer_create: a staging buffer cannot also be bound for "
                    "use by the pipeline (create a separate buffer for that)");
                return 0;
            }
        }
        else if (bind == static_cast<std::int32_t>(BufferBind::None))
        {
            set_last_error(
                "igpu_buffer_create: a non-staging buffer must declare at least "
                "one IgpuBufferBind use");
            return 0;
        }

        UINT d3d_bind = 0;
        if (bind != static_cast<std::int32_t>(BufferBind::None) &&
            !to_d3d_bind(bind, d3d_bind))
        {
            set_last_error(
                "igpu_buffer_create: invalid bind flags " + std::to_string(bind) +
                " (use a combination of IgpuBufferBind)");
            return 0;
        }

        // A vertex buffer must declare its stride, because a draw call derives
        // the vertex count from size/stride. Without it a draw cannot know how
        // many vertices exist and would have to trust the caller's count.
        const bool is_vertex =
            (bind & static_cast<std::int32_t>(BufferBind::Vertex)) != 0;

        if (is_vertex && stride <= 0)
        {
            set_last_error(
                "igpu_buffer_create: a vertex buffer needs a positive stride "
                "(bytes per vertex) so draws can derive the vertex count");
            return 0;
        }
        if (!is_vertex && stride != 0)
        {
            set_last_error(
                "igpu_buffer_create: stride is only meaningful for a vertex "
                "buffer; pass 0 for this one");
            return 0;
        }
        if (is_vertex && (size % stride) != 0)
        {
            set_last_error(
                "igpu_buffer_create: size " + std::to_string(size) +
                " is not a whole number of " + std::to_string(stride) +
                "-byte vertices");
            return 0;
        }

        // Constant buffers must be a multiple of 16 bytes; D3D11 rejects
        // anything else. Reported here so the caller learns the real rule.
        if ((d3d_bind & D3D11_BIND_CONSTANT_BUFFER) != 0 && (size % 16) != 0)
        {
            set_last_error(
                "igpu_buffer_create: a uniform buffer's size must be a multiple "
                "of 16 bytes (got " + std::to_string(size) + ")");
            return 0;
        }

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(size);
        desc.Usage = to_d3d_usage(parsed_usage);
        desc.BindFlags = d3d_bind;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        if (parsed_usage == BufferUsage::Dynamic)
        {
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        }
        else if (parsed_usage == BufferUsage::Staging)
        {
            // Staging is read-only from the CPU's point of view and is never
            // bound to the pipeline; both are D3D11 requirements.
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.BindFlags = 0;
        }

        // The buffer starts zero-filled; fill it with igpu_buffer_write().
        ID3D11Buffer* object = nullptr;
        const HRESULT hr = s.device->CreateBuffer(&desc, nullptr, &object);
        if (FAILED(hr) || object == nullptr)
        {
            char hex[16] = {};
            std::snprintf(hex, sizeof(hex), "%08lX", static_cast<unsigned long>(hr));
            set_last_error(
                std::string("igpu_buffer_create: backend rejected the buffer "
                            "description (hr=0x") + hex + ")");
            if (object != nullptr)
            {
                object->Release();
            }
            return 0;
        }

        const std::uint64_t id = s.next_buffer_id++;
        DeviceState::BufferEntry entry{};
        entry.object = object;
        entry.size = size;
        entry.usage = usage;
        entry.bind = bind;
        entry.stride = stride;
        s.buffers.emplace(id, entry);

        return static_cast<std::int64_t>(id);
    }

    bool buffer_write(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer data)
    {
        clear_last_error();

        auto* entry = find_buffer(buffer);
        if (entry == nullptr)
        {
            set_last_error("igpu_buffer_write: unknown buffer handle");
            return false;
        }

        if (offset < 0)
        {
            set_last_error("igpu_buffer_write: offset must not be negative");
            return false;
        }

        const auto source_size = static_cast<std::int64_t>(data.length());
        if (source_size <= 0 || data.data() == nullptr)
        {
            set_last_error("igpu_buffer_write: source buffer is empty");
            return false;
        }

        // Bounds check against the destination. Without this a caller could
        // scribble past the GPU allocation.
        if (offset + source_size > entry->size)
        {
            set_last_error(
                "igpu_buffer_write: " + std::to_string(source_size) +
                " bytes at offset " + std::to_string(offset) +
                " exceeds the buffer size " + std::to_string(entry->size));
            return false;
        }

        auto& s = state();

        if (entry->usage == static_cast<std::int32_t>(BufferUsage::Dynamic))
        {
            // Dynamic buffers are written through Map/Discard. Discard is safe
            // here because we overwrite a whole range and do not need the old
            // contents; it also avoids stalling on a buffer still in use.
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const HRESULT hr = s.context->Map(
                entry->object, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            if (FAILED(hr) || mapped.pData == nullptr)
            {
                set_last_error("igpu_buffer_write: could not map the dynamic buffer");
                return false;
            }

            std::memcpy(
                static_cast<std::byte*>(mapped.pData) + offset,
                data.data(),
                static_cast<std::size_t>(source_size));
            s.context->Unmap(entry->object, 0);
            return true;
        }

        if (entry->usage == static_cast<std::int32_t>(BufferUsage::Staging))
        {
            set_last_error(
                "igpu_buffer_write: a staging buffer is a readback target and "
                "cannot be written from the CPU");
            return false;
        }

        // Static: a DEFAULT-usage buffer is updated through UpdateSubresource,
        // which does not require a map. This is the intended one-shot upload
        // path.
        D3D11_BOX box{};
        box.left = static_cast<UINT>(offset);
        box.right = static_cast<UINT>(offset + source_size);
        box.top = 0;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;

        s.context->UpdateSubresource(
            entry->object, 0, &box, data.data(), 0, 0);
        return true;
    }

    bool buffer_resize(std::uint64_t buffer, std::int64_t size)
    {
        clear_last_error();

        auto* entry = find_buffer(buffer);
        if (entry == nullptr)
        {
            set_last_error("igpu_buffer_resize: unknown buffer handle");
            return false;
        }
        if (size <= 0)
        {
            set_last_error("igpu_buffer_resize: size must be positive");
            return false;
        }
        if (entry->usage != static_cast<std::int32_t>(BufferUsage::Dynamic))
        {
            set_last_error(
                "igpu_buffer_resize: only a Dynamic buffer can be resized "
                "(this one is not Dynamic)");
            return false;
        }

        // D3D11 cannot resize a buffer in place, so this creates a replacement
        // and preserves as much of the old contents as both sizes allow. Any
        // previously handed-out layout still refers to the shader, not the
        // buffer, so nothing else needs rebuilding here.
        auto& s = state();

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(size);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        // Rebuild the original bind flags rather than assuming vertex data.
        UINT original_bind = 0;
        if (!to_d3d_bind(entry->bind, original_bind))
        {
            set_last_error("igpu_buffer_resize: the buffer's bind flags are no longer valid");
            return false;
        }
        desc.BindFlags = original_bind;

        ID3D11Buffer* replacement = nullptr;
        const HRESULT hr = s.device->CreateBuffer(&desc, nullptr, &replacement);
        if (FAILED(hr) || replacement == nullptr)
        {
            set_last_error("igpu_buffer_resize: backend rejected the new size");
            if (replacement != nullptr)
            {
                replacement->Release();
            }
            return false;
        }

        // Copy the overlapping prefix across so a grow keeps existing data.
        const std::int64_t copy_size = entry->size < size ? entry->size : size;

        D3D11_MAPPED_SUBRESOURCE source_map{};
        if (SUCCEEDED(s.context->Map(entry->object, 0, D3D11_MAP_READ, 0, &source_map)) &&
            source_map.pData != nullptr)
        {
            D3D11_MAPPED_SUBRESOURCE dest_map{};
            if (SUCCEEDED(s.context->Map(replacement, 0, D3D11_MAP_WRITE_DISCARD, 0, &dest_map)) &&
                dest_map.pData != nullptr)
            {
                std::memcpy(dest_map.pData, source_map.pData,
                            static_cast<std::size_t>(copy_size));
                s.context->Unmap(replacement, 0);
            }
            s.context->Unmap(entry->object, 0);
        }

        entry->object->Release();
        entry->object = replacement;
        entry->size = size;
        return true;
    }

    bool buffer_read(std::uint64_t buffer, std::int64_t offset, gm::wire::GMBuffer dest)
    {
        clear_last_error();

        auto* entry = find_buffer(buffer);
        if (entry == nullptr)
        {
            set_last_error("igpu_buffer_read: unknown buffer handle");
            return false;
        }

        if (offset < 0)
        {
            set_last_error("igpu_buffer_read: offset must not be negative");
            return false;
        }

        const auto dest_size = static_cast<std::int64_t>(dest.length());
        if (dest_size <= 0 || dest.data() == nullptr)
        {
            set_last_error("igpu_buffer_read: destination buffer is empty");
            return false;
        }

        if (offset + dest_size > entry->size)
        {
            set_last_error(
                "igpu_buffer_read: " + std::to_string(dest_size) +
                " bytes at offset " + std::to_string(offset) +
                " exceeds the buffer size " + std::to_string(entry->size));
            return false;
        }

        // Only a staging buffer can be mapped for reading. Any other usage
        // would require a copy into a staging buffer first, which is a caller
        // visible decision rather than something to do silently here.
        if (entry->usage != static_cast<std::int32_t>(BufferUsage::Staging))
        {
            set_last_error(
                "igpu_buffer_read: only a Staging buffer can be read back; this "
                "buffer uses a different usage");
            return false;
        }

        auto& s = state();
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT hr = s.context->Map(entry->object, 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr) || mapped.pData == nullptr)
        {
            set_last_error("igpu_buffer_read: could not map the staging buffer for reading");
            return false;
        }

        std::memcpy(
            dest.data(),
            static_cast<const std::byte*>(mapped.pData) + offset,
            static_cast<std::size_t>(dest_size));
        s.context->Unmap(entry->object, 0);
        return true;
    }

    std::int64_t buffer_size(std::uint64_t buffer)
    {
        const auto* entry = find_buffer(buffer);
        return entry == nullptr ? 0 : entry->size;
    }

    bool buffer_release(std::uint64_t buffer)
    {
        auto& s = state();
        const auto it = s.buffers.find(buffer);
        if (it == s.buffers.end())
        {
            set_last_error("igpu_buffer_release: unknown buffer handle");
            return false;
        }

        if (it->second.object != nullptr)
        {
            it->second.object->Release();
        }
        s.buffers.erase(it);
        return true;
    }
}
