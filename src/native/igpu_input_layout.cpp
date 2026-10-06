#include "igpu_input_layout.h"

#include <d3d11.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "igpu_device.h"
#include "igpu_error.h"

namespace igpu
{
namespace d3d11_impl
{
    namespace
    {
        // Maps a GameMaker vertex usage to the HLSL semantic name that shader
        // authors normally write. These strings are the conventional spelling
        // (POSITION, TEXCOORD...) - they are shader identifiers, not a backend
        // API, so they do not leak backend terminology into the GML surface.
        const char* semantic_name(VertexUsage usage)
        {
            switch (usage)
            {
            case VertexUsage::Position:     return "POSITION";
            case VertexUsage::Colour:       return "COLOR";
            case VertexUsage::Normal:       return "NORMAL";
            case VertexUsage::Texcoord:     return "TEXCOORD";
            case VertexUsage::BlendWeight:  return "BLENDWEIGHT";
            case VertexUsage::BlendIndices: return "BLENDINDICES";
            case VertexUsage::PSize:        return "PSIZE";
            case VertexUsage::Tangent:      return "TANGENT";
            case VertexUsage::Binormal:     return "BINORMAL";
            }
            return nullptr;
        }

        struct FormatInfo
        {
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            std::uint32_t size = 0;
        };

        FormatInfo format_for(VertexType type)
        {
            switch (type)
            {
            case VertexType::Float1: return { DXGI_FORMAT_R32_FLOAT, 4 };
            case VertexType::Float2: return { DXGI_FORMAT_R32G32_FLOAT, 8 };
            case VertexType::Float3: return { DXGI_FORMAT_R32G32B32_FLOAT, 12 };
            case VertexType::Float4: return { DXGI_FORMAT_R32G32B32A32_FLOAT, 16 };
            // GameMaker's colour/ubyte4 types are 4 unsigned bytes. Colour is
            // normalised (0..255 -> 0..1) because it is used as a colour;
            // ubyte4 stays raw integer data.
            case VertexType::Colour: return { DXGI_FORMAT_R8G8B8A8_UNORM, 4 };
            case VertexType::UByte4: return { DXGI_FORMAT_R8G8B8A8_UINT, 4 };
            }
            return {};
        }

        bool usage_from_int(std::int32_t raw, VertexUsage& out)
        {
            switch (raw)
            {
            case 1: out = VertexUsage::Position;     return true;
            case 2: out = VertexUsage::Colour;       return true;
            case 3: out = VertexUsage::Normal;       return true;
            case 4: out = VertexUsage::Texcoord;     return true;
            case 5: out = VertexUsage::BlendWeight;  return true;
            case 6: out = VertexUsage::BlendIndices; return true;
            case 7: out = VertexUsage::PSize;        return true;
            case 8: out = VertexUsage::Tangent;      return true;
            case 9: out = VertexUsage::Binormal;     return true;
            default: return false;
            }
        }

        bool type_from_int(std::int32_t raw, VertexType& out)
        {
            switch (raw)
            {
            case 1: out = VertexType::Float1; return true;
            case 2: out = VertexType::Float2; return true;
            case 3: out = VertexType::Float3; return true;
            case 4: out = VertexType::Float4; return true;
            case 5: out = VertexType::Colour; return true;
            case 6: out = VertexType::UByte4; return true;
            default: return false;
            }
        }

        // Reads an int32 from a wire array element. Returns false if the element
        // is missing or not an integer kind, so a bad array produces a clean
        // error instead of a garbage format.
        bool read_int(const gm::wire::GMArrayView& array, std::size_t index, std::int32_t& out)
        {
            const auto value = array[index];
            if (value.is<std::int32_t>())
            {
                out = value.as<std::int32_t>();
                return true;
            }
            if (value.is<double>())
            {
                out = static_cast<std::int32_t>(value.as<double>());
                return true;
            }
            if (value.is<std::uint64_t>())
            {
                out = static_cast<std::int32_t>(value.as<std::uint64_t>());
                return true;
            }
            return false;
        }
    }

    std::int64_t input_layout_create_impl(
        std::int64_t shader,
        const gm::wire::GMArrayView& usage,
        const gm::wire::GMArrayView& type,
        const std::int32_t* steps,
        std::int32_t element_count,
        std::int32_t vertex_stride,
        std::int32_t instance_stride,
        const char* api)
    {
        clear_last_error();

        if (!require_device(api))
        {
            return 0;
        }

        auto& s = state();

        // The layout is validated against a vertex shader's input signature, so
        // a missing shader is fatal rather than something to work around.
        auto* shader_entry = find_shader(static_cast<std::uint64_t>(shader));
        if (shader_entry == nullptr)
        {
            set_last_error(std::string(api) + ": unknown shader handle");
            return 0;
        }
        if (shader_entry->bytecode == nullptr)
        {
            set_last_error(std::string(api) + ": shader has no bytecode to read a signature from");
            return 0;
        }

        if (element_count <= 0)
        {
            set_last_error(std::string(api) + ": element_count must be positive");
            return 0;
        }
        if (static_cast<std::size_t>(element_count) > usage.size() ||
            static_cast<std::size_t>(element_count) > type.size())
        {
            set_last_error(std::string(api) + ": element_count exceeds the usage/type arrays");
            return 0;
        }

        std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
        elements.reserve(static_cast<std::size_t>(element_count));

        // Semantic indices are assigned in first-seen order per usage, matching
        // how GameMaker numbers its own formats: two TEXCOORD elements become
        // TEXCOORD0 and TEXCOORD1.
        std::unordered_map<std::int32_t, std::uint32_t> semantic_indices;
        std::vector<std::string> semantic_storage;
        semantic_storage.reserve(static_cast<std::size_t>(element_count));

        std::uint32_t vertex_offset = 0;
        std::uint32_t instance_offset = 0;

        for (std::int32_t i = 0; i < element_count; ++i)
        {
            std::int32_t raw_usage = 0;
            std::int32_t raw_type = 0;
            if (!read_int(usage, static_cast<std::size_t>(i), raw_usage) ||
                !read_int(type, static_cast<std::size_t>(i), raw_type))
            {
                set_last_error(
                    std::string(api) + ": usage/type entries must be numbers "
                    "(use the vertex_usage_* / vertex_type_* constants)");
                return 0;
            }
            std::int32_t step = 0;
            if (steps != nullptr)
            {
                step = steps[i];
                if (step != 0 && step != 1)
                {
                    set_last_error(
                        std::string(api) + ": step must be IgpuVertexStep.Vertex or IgpuVertexStep.Instance");
                    return 0;
                }
            }

            VertexUsage parsed_usage{};
            VertexType parsed_type{};
            if (!usage_from_int(raw_usage, parsed_usage))
            {
                set_last_error(
                    std::string(api) + ": unknown vertex usage value " +
                    std::to_string(raw_usage));
                return 0;
            }
            if (!type_from_int(raw_type, parsed_type))
            {
                set_last_error(
                    std::string(api) + ": unknown vertex type value " +
                    std::to_string(raw_type));
                return 0;
            }

            const FormatInfo info = format_for(parsed_type);
            const std::uint32_t index = semantic_indices[raw_usage]++;

            // The desc stores a const char*, so the strings must outlive the
            // call - semantic_storage owns them and is reserved up front so
            // reallocation cannot invalidate what we hand to CreateInputLayout.
            semantic_storage.emplace_back(semantic_name(parsed_usage));

            const bool per_instance = step == 1;
            std::uint32_t& cursor = per_instance ? instance_offset : vertex_offset;

            D3D11_INPUT_ELEMENT_DESC desc{};
            desc.SemanticName = semantic_storage.back().c_str();
            desc.SemanticIndex = index;
            desc.Format = info.format;
            desc.InputSlot = per_instance ? 1u : 0u;
            desc.AlignedByteOffset = cursor;
            desc.InputSlotClass = per_instance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
            desc.InstanceDataStepRate = per_instance ? 1u : 0u;

            cursor += info.size;
            elements.push_back(desc);
        }

        // `stride` is validated here but not stored: a D3D11 input layout
        // describes only the elements, while the vertex stride is supplied by
        // the caller when the buffer is bound at draw time. Rejecting an
        // undersized stride now turns a silent memory overlap into a clear
        // error at the call that caused it.
        //
        // A stride of 0 (or any negative value) means "tightly packed", which
        // is what the GML helper's -1 default sends, so both must be accepted.
        if (vertex_stride > 0 && static_cast<std::uint32_t>(vertex_stride) < vertex_offset)
        {
            set_last_error(
                std::string(api) + ": stride " + std::to_string(vertex_stride) +
                " is smaller than the element size " + std::to_string(vertex_offset));
            return 0;
        }
        if (instance_offset > 0 && instance_stride > 0 &&
            static_cast<std::uint32_t>(instance_stride) < instance_offset)
        {
            set_last_error(
                std::string(api) + ": instance stride " + std::to_string(instance_stride) +
                " is smaller than the element size " + std::to_string(instance_offset));
            return 0;
        }

        ID3D11InputLayout* layout = nullptr;
        const HRESULT hr = s.device->CreateInputLayout(
            elements.data(),
            static_cast<UINT>(elements.size()),
            shader_entry->bytecode->GetBufferPointer(),
            shader_entry->bytecode->GetBufferSize(),
            &layout);

        if (FAILED(hr) || layout == nullptr)
        {
            char hex[16] = {};
            std::snprintf(hex, sizeof(hex), "%08lX", static_cast<unsigned long>(hr));
            set_last_error(
                std::string(api) +
                ": the vertex shader's input signature does not match the requested elements (hr=0x" +
                hex + ")");
            if (layout != nullptr)
            {
                layout->Release();
            }
            return 0;
        }

        const std::uint64_t id = s.next_input_layout_id++;
        s.input_layouts.emplace(id, layout);
        return static_cast<std::int64_t>(id);
    }

    std::int64_t input_layout_create(
        std::int64_t shader,
        const gm::wire::GMArrayView& usage,
        const gm::wire::GMArrayView& type,
        std::int32_t element_count,
        std::int32_t stride)
    {
        return input_layout_create_impl(
            shader, usage, type, nullptr, element_count, stride, 0, "igpu_input_layout_create");
    }

    std::int64_t input_layout_create_step(
        std::int64_t shader,
        const gm::wire::GMArrayView& usage,
        const gm::wire::GMArrayView& type,
        const gm::wire::GMArrayView& step,
        std::int32_t element_count,
        std::int32_t vertex_stride,
        std::int32_t instance_stride)
    {
        if (element_count <= 0 || static_cast<std::size_t>(element_count) > step.size())
        {
            clear_last_error();
            set_last_error("igpu_input_layout_create: element_count exceeds the step array");
            return 0;
        }
        std::vector<std::int32_t> steps(static_cast<std::size_t>(element_count));
        for (std::int32_t i = 0; i < element_count; ++i)
        {
            if (!read_int(step, static_cast<std::size_t>(i), steps[static_cast<std::size_t>(i)]))
            {
                clear_last_error();
                set_last_error("igpu_input_layout_create: step entries must be numbers");
                return 0;
            }
        }
        return input_layout_create_impl(
            shader, usage, type, steps.data(), element_count, vertex_stride, instance_stride,
            "igpu_input_layout_create");
    }

    bool input_layout_release(std::uint64_t layout)
    {
        auto& s = state();
        const auto it = s.input_layouts.find(layout);
        if (it == s.input_layouts.end())
        {
            set_last_error("igpu_input_layout_release: unknown layout handle");
            return false;
        }

        if (it->second != nullptr)
        {
            it->second->Release();
        }
        s.input_layouts.erase(it);
        return true;
    }
}
}
