#include "igpu_d3d11.h"

#include <d3d11shader.h>
#include <d3dcompiler.h>

#include <cstring>
#include <string>

#include "../igpu_error.h"

namespace igpu
{
namespace d3d11_impl
{
    namespace
    {
        std::int32_t map_type(D3D_SHADER_VARIABLE_TYPE type)
        {
            switch (type)
            {
            case D3D_SVT_FLOAT: return static_cast<std::int32_t>(UniformType::Float);
            case D3D_SVT_INT:   return static_cast<std::int32_t>(UniformType::Int);
            case D3D_SVT_UINT:  return static_cast<std::int32_t>(UniformType::Uint);
            case D3D_SVT_BOOL:  return static_cast<std::int32_t>(UniformType::Bool);
            default:            return static_cast<std::int32_t>(UniformType::Unknown);
            }
        }

        std::int32_t map_shape(D3D_SHADER_VARIABLE_CLASS cls)
        {
            switch (cls)
            {
            case D3D_SVC_SCALAR:         return static_cast<std::int32_t>(UniformShape::Scalar);
            case D3D_SVC_VECTOR:         return static_cast<std::int32_t>(UniformShape::Vector);
            case D3D_SVC_MATRIX_COLUMNS: return static_cast<std::int32_t>(UniformShape::MatrixColumns);
            case D3D_SVC_MATRIX_ROWS:    return static_cast<std::int32_t>(UniformShape::MatrixRows);
            case D3D_SVC_STRUCT:         return static_cast<std::int32_t>(UniformShape::Struct);
            default:                     return static_cast<std::int32_t>(UniformShape::Struct);
            }
        }
    }

    bool reflect_uniforms(const void* bytecode, std::size_t size, UniformLayout& out)
    {
        out = {};
        if (bytecode == nullptr || size == 0)
        {
            set_last_error("igpu_shader_compile: shader bytecode is empty");
            return false;
        }

        ID3D11ShaderReflection* reflection = nullptr;
        const HRESULT hr = D3DReflect(
            bytecode,
            size,
            IID_ID3D11ShaderReflection,
            reinterpret_cast<void**>(&reflection));
        if (FAILED(hr) || reflection == nullptr)
        {
            set_last_error("igpu_shader_compile: the shader's uniform layout could not be read");
            return false;
        }

        D3D11_SHADER_DESC shader_desc{};
        if (FAILED(reflection->GetDesc(&shader_desc)))
        {
            reflection->Release();
            set_last_error("igpu_shader_compile: the shader's uniform layout could not be read");
            return false;
        }

        out.blocks.reserve(shader_desc.ConstantBuffers);
        for (UINT block_index = 0; block_index < shader_desc.ConstantBuffers; ++block_index)
        {
            ID3D11ShaderReflectionConstantBuffer* block_ref =
                reflection->GetConstantBufferByIndex(block_index);
            if (block_ref == nullptr)
            {
                continue;
            }

            D3D11_SHADER_BUFFER_DESC block_desc{};
            if (FAILED(block_ref->GetDesc(&block_desc)) || block_desc.Name == nullptr)
            {
                continue;
            }
            if (block_desc.Type != D3D_CT_CBUFFER)
            {
                continue;
            }

            UniformBlock block;
            block.name = block_desc.Name;
            block.size = block_desc.Size;
            block.slot = -1;

            for (UINT resource = 0; resource < shader_desc.BoundResources; ++resource)
            {
                D3D11_SHADER_INPUT_BIND_DESC bind{};
                if (FAILED(reflection->GetResourceBindingDesc(resource, &bind)))
                {
                    continue;
                }
                if (bind.Type == D3D_SIT_CBUFFER && bind.Name != nullptr &&
                    std::strcmp(bind.Name, block_desc.Name) == 0)
                {
                    block.slot = static_cast<std::int32_t>(bind.BindPoint);
                    break;
                }
            }

            block.members.reserve(block_desc.Variables);
            for (UINT variable = 0; variable < block_desc.Variables; ++variable)
            {
                ID3D11ShaderReflectionVariable* variable_ref = block_ref->GetVariableByIndex(variable);
                if (variable_ref == nullptr)
                {
                    continue;
                }
                D3D11_SHADER_VARIABLE_DESC variable_desc{};
                if (FAILED(variable_ref->GetDesc(&variable_desc)) || variable_desc.Name == nullptr)
                {
                    continue;
                }
                ID3D11ShaderReflectionType* type_ref = variable_ref->GetType();
                D3D11_SHADER_TYPE_DESC type_desc{};
                if (type_ref == nullptr || FAILED(type_ref->GetDesc(&type_desc)))
                {
                    continue;
                }

                UniformMember member;
                member.name = variable_desc.Name;
                member.offset = variable_desc.StartOffset;
                member.size = variable_desc.Size;
                member.shape = map_shape(type_desc.Class);
                member.rows = static_cast<std::int32_t>(type_desc.Rows);
                member.columns = static_cast<std::int32_t>(type_desc.Columns);
                member.elements = static_cast<std::int32_t>(type_desc.Elements);
                member.type = member.shape == static_cast<std::int32_t>(UniformShape::Struct)
                    ? static_cast<std::int32_t>(UniformType::Struct)
                    : map_type(type_desc.Type);
                block.members.push_back(std::move(member));
            }

            out.blocks.push_back(std::move(block));
        }

        reflection->Release();
        return true;
    }
}
}
