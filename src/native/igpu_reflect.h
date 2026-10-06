#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gm
{
    namespace wire
    {
        struct GMArrayView;
    }
}

namespace igpu
{
    // Scalar kind of one uniform member. Mirrors IgpuUniformType in spec.gmidl.
    enum class UniformType : std::int32_t
    {
        Float = 0,
        Int = 1,
        Uint = 2,
        Bool = 3,
        Struct = 4,
        Unknown = 5
    };

    // How the member is laid out inside its block. Not exposed to GML: callers
    // see rows, columns and elements, and the packer needs the orientation.
    enum class UniformShape : std::int32_t
    {
        Scalar = 0,
        Vector = 1,
        MatrixColumns = 2,
        MatrixRows = 3,
        Struct = 4
    };

    struct UniformMember
    {
        std::string name;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
        std::int32_t type = static_cast<std::int32_t>(UniformType::Unknown);
        std::int32_t shape = static_cast<std::int32_t>(UniformShape::Struct);
        std::int32_t rows = 0;
        std::int32_t columns = 0;
        std::int32_t elements = 0;
    };

    struct UniformBlock
    {
        std::string name;
        std::uint32_t size = 0;
        std::int32_t slot = -1;
        std::vector<UniformMember> members;
    };

    // Backend-neutral uniform layout of one compiled shader.
    struct UniformLayout
    {
        std::vector<UniformBlock> blocks;
    };

    std::int32_t shader_block_count(std::int64_t shader);
    std::string shader_block_name(std::int64_t shader, std::int32_t index);
    std::int32_t shader_block_size(std::int64_t shader, std::string_view block);
    std::int32_t shader_block_slot(std::int64_t shader, std::string_view block);
    std::int32_t shader_member_count(std::int64_t shader, std::string_view block);
    std::string shader_member_name(std::int64_t shader, std::string_view block, std::int32_t index);
    std::int32_t shader_member_offset(std::int64_t shader, std::string_view block, std::string_view member);
    std::int32_t shader_member_size(std::int64_t shader, std::string_view block, std::string_view member);
    std::int32_t shader_member_type(std::int64_t shader, std::string_view block, std::string_view member);
    std::int32_t shader_member_rows(std::int64_t shader, std::string_view block, std::string_view member);
    std::int32_t shader_member_columns(std::int64_t shader, std::string_view block, std::string_view member);
    std::int32_t shader_member_elements(std::int64_t shader, std::string_view block, std::string_view member);

    bool uniform_write(std::uint64_t buffer, std::int64_t shader, std::string_view block,
                       std::string_view member, const gm::wire::GMArrayView& values);
    bool uniform_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot);
}
