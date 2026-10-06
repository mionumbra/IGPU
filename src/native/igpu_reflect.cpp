#include "igpu_reflect.h"

#include "core/GMExtWire.h"

#include <cstring>
#include <string>
#include <vector>

#include "igpu_backend.h"
#include "igpu_buffer.h"
#include "igpu_device.h"
#include "igpu_error.h"

namespace igpu
{
    namespace
    {
        Backend* need(const char* entry)
        {
            if (!require_device(entry))
            {
                return nullptr;
            }
            Backend* backend = active_backend();
            if (backend == nullptr)
            {
                set_last_error(std::string(entry) + ": call igpu_init() first");
            }
            return backend;
        }

        const DeviceState::ShaderEntry* find_compiled(std::int64_t shader, const char* entry)
        {
            if (shader <= 0)
            {
                set_last_error(std::string(entry) + ": unknown shader handle");
                return nullptr;
            }
            const auto* found = find_shader(static_cast<std::uint64_t>(shader));
            if (found == nullptr)
            {
                set_last_error(std::string(entry) + ": unknown shader handle");
                return nullptr;
            }
            return found;
        }

        const UniformBlock* find_block(const UniformLayout& layout, std::string_view name, const char* entry)
        {
            for (const auto& block : layout.blocks)
            {
                if (block.name == name)
                {
                    return &block;
                }
            }
            set_last_error(std::string(entry) + ": unknown uniform block '" + std::string(name) + "'");
            return nullptr;
        }

        const UniformMember* find_member(const UniformBlock& block, std::string_view name, const char* entry)
        {
            for (const auto& member : block.members)
            {
                if (member.name == name)
                {
                    return &member;
                }
            }
            set_last_error(std::string(entry) + ": unknown uniform member '" + std::string(name) + "'");
            return nullptr;
        }

        bool read_number(const gm::wire::GMValue& value, double& out)
        {
            if (value.is<double>())
            {
                out = value.as<double>();
                return true;
            }
            if (value.is<std::int32_t>())
            {
                out = value.as<std::int32_t>();
                return true;
            }
            if (value.is<std::uint64_t>())
            {
                out = static_cast<double>(value.as<std::uint64_t>());
                return true;
            }
            return false;
        }

        void store_component(std::byte* dest, double value, std::int32_t type)
        {
            if (type == static_cast<std::int32_t>(UniformType::Int))
            {
                const auto stored = static_cast<std::int32_t>(value);
                std::memcpy(dest, &stored, sizeof(stored));
                return;
            }
            if (type == static_cast<std::int32_t>(UniformType::Uint) ||
                type == static_cast<std::int32_t>(UniformType::Bool))
            {
                const auto stored = type == static_cast<std::int32_t>(UniformType::Bool)
                    ? (value != 0.0 ? 1u : 0u)
                    : static_cast<std::uint32_t>(value);
                std::memcpy(dest, &stored, sizeof(stored));
                return;
            }
            const auto stored = static_cast<float>(value);
            std::memcpy(dest, &stored, sizeof(stored));
        }

        // Places values into `dest` using the member's reflected shape.
        // Column-major matrices store one column per 16-byte register.
        // Array elements each start on a 16-byte boundary.
        bool pack_member(const UniformMember& member, const std::vector<double>& values,
                         std::vector<std::byte>& dest)
        {
            if (member.shape == static_cast<std::int32_t>(UniformShape::Struct) ||
                member.type == static_cast<std::int32_t>(UniformType::Struct) ||
                member.type == static_cast<std::int32_t>(UniformType::Unknown))
            {
                set_last_error(
                    "igpu_uniform_write: member '" + member.name +
                    "' is not a scalar, vector or matrix");
                return false;
            }

            const bool as_array = member.elements > 0;
            const std::int32_t elements = as_array ? member.elements : 1;
            std::int32_t per_element = 1;
            if (member.shape == static_cast<std::int32_t>(UniformShape::Vector))
            {
                per_element = member.columns;
            }
            else if (member.shape == static_cast<std::int32_t>(UniformShape::MatrixColumns) ||
                     member.shape == static_cast<std::int32_t>(UniformShape::MatrixRows))
            {
                per_element = member.rows * member.columns;
            }
            if (per_element <= 0 || elements <= 0)
            {
                set_last_error("igpu_uniform_write: member '" + member.name + "' has no components");
                return false;
            }

            const std::size_t expected = static_cast<std::size_t>(elements) * static_cast<std::size_t>(per_element);
            if (values.size() != expected)
            {
                set_last_error(
                    "igpu_uniform_write: member '" + member.name + "' expects " +
                    std::to_string(expected) + " values");
                return false;
            }

            dest.assign(member.size, std::byte{0});
            std::size_t cursor = 0;
            for (std::int32_t element = 0; element < elements; ++element)
            {
                if (member.shape == static_cast<std::int32_t>(UniformShape::MatrixColumns) ||
                    member.shape == static_cast<std::int32_t>(UniformShape::MatrixRows))
                {
                    const bool by_column = member.shape == static_cast<std::int32_t>(UniformShape::MatrixColumns);
                    const std::int32_t majors = by_column ? member.columns : member.rows;
                    const std::int32_t minors = by_column ? member.rows : member.columns;
                    const std::size_t element_base = static_cast<std::size_t>(element) *
                        static_cast<std::size_t>(majors) * 16u;
                    for (std::int32_t major = 0; major < majors; ++major)
                    {
                        for (std::int32_t minor = 0; minor < minors; ++minor)
                        {
                            const std::size_t at = element_base +
                                static_cast<std::size_t>(major) * 16u +
                                static_cast<std::size_t>(minor) * 4u;
                            if (at + 4 > dest.size())
                            {
                                set_last_error(
                                    "igpu_uniform_write: member '" + member.name +
                                    "' does not fit its reflected size");
                                return false;
                            }
                            store_component(dest.data() + at, values[cursor], member.type);
                            ++cursor;
                        }
                    }
                    continue;
                }

                const std::size_t base = as_array ? static_cast<std::size_t>(element) * 16u : 0u;
                for (std::int32_t component = 0; component < per_element; ++component)
                {
                    const std::size_t at = base + static_cast<std::size_t>(component) * 4u;
                    if (at + 4 > dest.size())
                    {
                        set_last_error(
                            "igpu_uniform_write: member '" + member.name +
                            "' does not fit its reflected size");
                        return false;
                    }
                    store_component(dest.data() + at, values[cursor], member.type);
                    ++cursor;
                }
            }
            return true;
        }
    }

    std::int32_t shader_block_count(std::int64_t shader)
    {
        clear_last_error();
        if (need("igpu_shader_block_count") == nullptr)
        {
            return 0;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_block_count");
        return entry == nullptr ? 0 : static_cast<std::int32_t>(entry->uniforms.blocks.size());
    }

    std::string shader_block_name(std::int64_t shader, std::int32_t index)
    {
        clear_last_error();
        if (need("igpu_shader_block_name") == nullptr)
        {
            return {};
        }
        const auto* entry = find_compiled(shader, "igpu_shader_block_name");
        if (entry == nullptr)
        {
            return {};
        }
        if (index < 0 || static_cast<std::size_t>(index) >= entry->uniforms.blocks.size())
        {
            set_last_error("igpu_shader_block_name: index is outside the shader");
            return {};
        }
        return entry->uniforms.blocks[static_cast<std::size_t>(index)].name;
    }

    std::int32_t shader_block_size(std::int64_t shader, std::string_view block)
    {
        clear_last_error();
        if (need("igpu_shader_block_size") == nullptr)
        {
            return 0;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_block_size");
        if (entry == nullptr)
        {
            return 0;
        }
        const auto* found = find_block(entry->uniforms, block, "igpu_shader_block_size");
        return found == nullptr ? 0 : static_cast<std::int32_t>(found->size);
    }

    std::int32_t shader_block_slot(std::int64_t shader, std::string_view block)
    {
        clear_last_error();
        if (need("igpu_shader_block_slot") == nullptr)
        {
            return -1;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_block_slot");
        if (entry == nullptr)
        {
            return -1;
        }
        const auto* found = find_block(entry->uniforms, block, "igpu_shader_block_slot");
        if (found == nullptr)
        {
            return -1;
        }
        if (found->slot < 0)
        {
            set_last_error("igpu_shader_block_slot: the block does not request a slot");
            return -1;
        }
        return found->slot;
    }

    std::int32_t shader_member_count(std::int64_t shader, std::string_view block)
    {
        clear_last_error();
        if (need("igpu_shader_member_count") == nullptr)
        {
            return 0;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_member_count");
        if (entry == nullptr)
        {
            return 0;
        }
        const auto* found = find_block(entry->uniforms, block, "igpu_shader_member_count");
        return found == nullptr ? 0 : static_cast<std::int32_t>(found->members.size());
    }

    std::string shader_member_name(std::int64_t shader, std::string_view block, std::int32_t index)
    {
        clear_last_error();
        if (need("igpu_shader_member_name") == nullptr)
        {
            return {};
        }
        const auto* entry = find_compiled(shader, "igpu_shader_member_name");
        if (entry == nullptr)
        {
            return {};
        }
        const auto* found = find_block(entry->uniforms, block, "igpu_shader_member_name");
        if (found == nullptr)
        {
            return {};
        }
        if (index < 0 || static_cast<std::size_t>(index) >= found->members.size())
        {
            set_last_error("igpu_shader_member_name: index is outside the block");
            return {};
        }
        return found->members[static_cast<std::size_t>(index)].name;
    }

    std::int32_t shader_member_offset(std::int64_t shader, std::string_view block, std::string_view member)
    {
        clear_last_error();
        if (need("igpu_shader_member_offset") == nullptr)
        {
            return -1;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_member_offset");
        if (entry == nullptr)
        {
            return -1;
        }
        const auto* found_block = find_block(entry->uniforms, block, "igpu_shader_member_offset");
        if (found_block == nullptr)
        {
            return -1;
        }
        const auto* found = find_member(*found_block, member, "igpu_shader_member_offset");
        return found == nullptr ? -1 : static_cast<std::int32_t>(found->offset);
    }

    std::int32_t shader_member_size(std::int64_t shader, std::string_view block, std::string_view member)
    {
        clear_last_error();
        if (need("igpu_shader_member_size") == nullptr)
        {
            return 0;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_member_size");
        if (entry == nullptr)
        {
            return 0;
        }
        const auto* found_block = find_block(entry->uniforms, block, "igpu_shader_member_size");
        if (found_block == nullptr)
        {
            return 0;
        }
        const auto* found = find_member(*found_block, member, "igpu_shader_member_size");
        return found == nullptr ? 0 : static_cast<std::int32_t>(found->size);
    }

    std::int32_t shader_member_type(std::int64_t shader, std::string_view block, std::string_view member)
    {
        clear_last_error();
        if (need("igpu_shader_member_type") == nullptr)
        {
            return static_cast<std::int32_t>(UniformType::Unknown);
        }
        const auto* entry = find_compiled(shader, "igpu_shader_member_type");
        if (entry == nullptr)
        {
            return static_cast<std::int32_t>(UniformType::Unknown);
        }
        const auto* found_block = find_block(entry->uniforms, block, "igpu_shader_member_type");
        if (found_block == nullptr)
        {
            return static_cast<std::int32_t>(UniformType::Unknown);
        }
        const auto* found = find_member(*found_block, member, "igpu_shader_member_type");
        return found == nullptr
            ? static_cast<std::int32_t>(UniformType::Unknown)
            : found->type;
    }

    std::int32_t shader_member_rows(std::int64_t shader, std::string_view block, std::string_view member)
    {
        clear_last_error();
        if (need("igpu_shader_member_rows") == nullptr)
        {
            return 0;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_member_rows");
        if (entry == nullptr)
        {
            return 0;
        }
        const auto* found_block = find_block(entry->uniforms, block, "igpu_shader_member_rows");
        if (found_block == nullptr)
        {
            return 0;
        }
        const auto* found = find_member(*found_block, member, "igpu_shader_member_rows");
        return found == nullptr ? 0 : found->rows;
    }

    std::int32_t shader_member_columns(std::int64_t shader, std::string_view block, std::string_view member)
    {
        clear_last_error();
        if (need("igpu_shader_member_columns") == nullptr)
        {
            return 0;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_member_columns");
        if (entry == nullptr)
        {
            return 0;
        }
        const auto* found_block = find_block(entry->uniforms, block, "igpu_shader_member_columns");
        if (found_block == nullptr)
        {
            return 0;
        }
        const auto* found = find_member(*found_block, member, "igpu_shader_member_columns");
        return found == nullptr ? 0 : found->columns;
    }

    std::int32_t shader_member_elements(std::int64_t shader, std::string_view block, std::string_view member)
    {
        clear_last_error();
        if (need("igpu_shader_member_elements") == nullptr)
        {
            return 0;
        }
        const auto* entry = find_compiled(shader, "igpu_shader_member_elements");
        if (entry == nullptr)
        {
            return 0;
        }
        const auto* found_block = find_block(entry->uniforms, block, "igpu_shader_member_elements");
        if (found_block == nullptr)
        {
            return 0;
        }
        const auto* found = find_member(*found_block, member, "igpu_shader_member_elements");
        return found == nullptr ? 0 : found->elements;
    }

    bool uniform_write(std::uint64_t buffer, std::int64_t shader, std::string_view block,
                       std::string_view member, const gm::wire::GMArrayView& values)
    {
        clear_last_error();
        if (need("igpu_uniform_write") == nullptr)
        {
            return false;
        }
        const auto* shader_entry = find_compiled(shader, "igpu_uniform_write");
        if (shader_entry == nullptr)
        {
            return false;
        }
        const auto* found_block = find_block(shader_entry->uniforms, block, "igpu_uniform_write");
        if (found_block == nullptr)
        {
            return false;
        }
        const auto* found = find_member(*found_block, member, "igpu_uniform_write");
        if (found == nullptr)
        {
            return false;
        }

        auto* buffer_entry = find_buffer(buffer);
        if (buffer_entry == nullptr)
        {
            set_last_error("igpu_uniform_write: unknown buffer handle");
            return false;
        }
        if ((buffer_entry->bind & static_cast<std::int32_t>(BufferBind::Uniform)) == 0)
        {
            set_last_error("igpu_uniform_write: the buffer was not created with IgpuBufferBind.Uniform");
            return false;
        }
        if (buffer_entry->size < static_cast<std::int64_t>(found_block->size) ||
            static_cast<std::uint32_t>(buffer_entry->size) < found->offset + found->size)
        {
            set_last_error("igpu_uniform_write: the buffer is smaller than the uniform block");
            return false;
        }

        std::vector<double> numbers;
        numbers.reserve(values.size());
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            double number = 0;
            if (!read_number(values[i], number))
            {
                set_last_error("igpu_uniform_write: a value is not a number");
                return false;
            }
            numbers.push_back(number);
        }

        std::vector<std::byte> packed;
        if (!pack_member(*found, numbers, packed))
        {
            return false;
        }
        return buffer_patch(buffer, static_cast<std::int64_t>(found->offset), packed.data(), packed.size(),
                            "igpu_uniform_write");
    }

    bool uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot)
    {
        clear_last_error();
        Backend* backend = need("igpu_uniform_bind");
        return backend != nullptr && backend->uniform_bind(buffer, stage, slot);
    }
}
