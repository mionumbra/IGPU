#include "igpu_state.h"

#include <cstdint>
#include <string>

#include "d3d11/igpu_d3d11_state.h"
#include "igpu_device.h"
#include "igpu_error.h"

namespace igpu
{
namespace d3d11_impl
{
    namespace
    {
        // GameMaker passes these integers straight through. The values below
        // are the engine enums, which match the GML constants of the same name.

        bool blend_factor(std::int32_t raw, bool alpha_slot, D3D11_BLEND& out)
        {
            // bm_zero .. bm_src_alpha_sat. Colour factors are legal on the
            // alpha slot; D3D11 rejects them there, so they become the alpha
            // factor GameMaker's own converter uses.
            switch (raw)
            {
            case 1: out = D3D11_BLEND_ZERO; return true;
            case 2: out = D3D11_BLEND_ONE; return true;
            case 3: out = alpha_slot ? D3D11_BLEND_SRC_ALPHA : D3D11_BLEND_SRC_COLOR; return true;
            case 4: out = alpha_slot ? D3D11_BLEND_INV_SRC_ALPHA : D3D11_BLEND_INV_SRC_COLOR; return true;
            case 5: out = D3D11_BLEND_SRC_ALPHA; return true;
            case 6: out = D3D11_BLEND_INV_SRC_ALPHA; return true;
            case 7: out = D3D11_BLEND_DEST_ALPHA; return true;
            case 8: out = D3D11_BLEND_INV_DEST_ALPHA; return true;
            case 9: out = alpha_slot ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_DEST_COLOR; return true;
            case 10: out = alpha_slot ? D3D11_BLEND_INV_DEST_ALPHA : D3D11_BLEND_INV_DEST_COLOR; return true;
            case 11: out = D3D11_BLEND_SRC_ALPHA_SAT; return true;
            default: return false;
            }
        }

        // bm_eq_*: 1 add, 2 max, 3 subtract (dest - src), 4 min, 5 reverse (src - dest).
        // Same numbering as gpu_set_blendequation.
        bool blend_equation(std::int32_t raw, D3D11_BLEND_OP& out)
        {
            switch (raw)
            {
            case 1: out = D3D11_BLEND_OP_ADD; return true;
            case 2: out = D3D11_BLEND_OP_MAX; return true;
            case 3: out = D3D11_BLEND_OP_REV_SUBTRACT; return true;
            case 4: out = D3D11_BLEND_OP_MIN; return true;
            case 5: out = D3D11_BLEND_OP_SUBTRACT; return true;
            default: return false;
            }
        }

        bool comparison(std::int32_t raw, D3D11_COMPARISON_FUNC& out)
        {
            switch (raw)
            {
            case 1: out = D3D11_COMPARISON_NEVER; return true;
            case 2: out = D3D11_COMPARISON_LESS; return true;
            case 3: out = D3D11_COMPARISON_EQUAL; return true;
            case 4: out = D3D11_COMPARISON_LESS_EQUAL; return true;
            case 5: out = D3D11_COMPARISON_GREATER; return true;
            case 6: out = D3D11_COMPARISON_NOT_EQUAL; return true;
            case 7: out = D3D11_COMPARISON_GREATER_EQUAL; return true;
            case 8: out = D3D11_COMPARISON_ALWAYS; return true;
            default: return false;
            }
        }

        bool stencil_op(std::int32_t raw, D3D11_STENCIL_OP& out)
        {
            switch (raw)
            {
            case 1: out = D3D11_STENCIL_OP_KEEP; return true;
            case 2: out = D3D11_STENCIL_OP_ZERO; return true;
            case 3: out = D3D11_STENCIL_OP_REPLACE; return true;
            case 4: out = D3D11_STENCIL_OP_INCR_SAT; return true;
            case 5: out = D3D11_STENCIL_OP_DECR_SAT; return true;
            case 6: out = D3D11_STENCIL_OP_INVERT; return true;
            case 7: out = D3D11_STENCIL_OP_INCR; return true;
            case 8: out = D3D11_STENCIL_OP_DECR; return true;
            default: return false;
            }
        }

        // cull_noculling = 0, cull_clockwise = 1, cull_counterclockwise = 2.
        // Front faces are clockwise, matching GameMaker's rasteriser.
        bool cull_mode(std::int32_t raw, D3D11_CULL_MODE& out)
        {
            switch (raw)
            {
            case 0: out = D3D11_CULL_NONE; return true;
            case 1: out = D3D11_CULL_FRONT; return true;
            case 2: out = D3D11_CULL_BACK; return true;
            default: return false;
            }
        }

        std::uint64_t store_state(DeviceState::StateKind kind, ID3D11DeviceChild* object, std::uint32_t stencil_ref)
        {
            auto& s = state();
            const std::uint64_t id = s.next_state_id++;
            s.states.emplace(id, DeviceState::StateEntry{ kind, object, stencil_ref });
            return id;
        }

        bool take_state(
            const char* entry,
            const char* slot,
            std::int64_t raw,
            DeviceState::StateKind expected,
            ID3D11DeviceChild*& out,
            std::uint32_t* stencil_ref)
        {
            if (raw == 0)
            {
                return true;
            }
            if (raw < 0)
            {
                set_last_error(std::string(entry) + ": " + slot + " handle is not a state object");
                return false;
            }

            auto* found = find_state(static_cast<std::uint64_t>(raw));
            if (found == nullptr)
            {
                set_last_error(std::string(entry) + ": unknown " + slot + " handle");
                return false;
            }
            if (found->kind != expected)
            {
                set_last_error(std::string(entry) + ": " + slot + " handle is a different kind of state");
                return false;
            }

            out = found->object;
            if (stencil_ref != nullptr)
            {
                *stencil_ref = found->stencil_ref;
            }
            return true;
        }
    }

    std::int64_t blend_state_create(
        bool enabled,
        std::int32_t src,
        std::int32_t dest,
        std::int32_t equation,
        std::int32_t src_alpha,
        std::int32_t dest_alpha,
        std::int32_t equation_alpha,
        bool write_red,
        bool write_green,
        bool write_blue,
        bool write_alpha)
    {
        clear_last_error();
        if (!require_device("igpu_blend_state_create"))
        {
            return 0;
        }

        D3D11_BLEND src_b{};
        D3D11_BLEND dest_b{};
        D3D11_BLEND src_a{};
        D3D11_BLEND dest_a{};
        D3D11_BLEND_OP eq{};
        D3D11_BLEND_OP eq_a{};
        if (!blend_factor(src, false, src_b) || !blend_factor(dest, false, dest_b) ||
            !blend_factor(src_alpha, true, src_a) || !blend_factor(dest_alpha, true, dest_a))
        {
            set_last_error("igpu_blend_state_create: blend factor must be a bm_* constant (bm_zero through bm_src_alpha_sat)");
            return 0;
        }
        if (!blend_equation(equation, eq) || !blend_equation(equation_alpha, eq_a))
        {
            set_last_error("igpu_blend_state_create: blend equation must be a bm_eq_* constant");
            return 0;
        }

        D3D11_BLEND_DESC desc{};
        desc.AlphaToCoverageEnable = FALSE;
        desc.IndependentBlendEnable = FALSE;
        auto& rt = desc.RenderTarget[0];
        rt.BlendEnable = enabled ? TRUE : FALSE;
        rt.SrcBlend = src_b;
        rt.DestBlend = dest_b;
        rt.BlendOp = eq;
        rt.SrcBlendAlpha = src_a;
        rt.DestBlendAlpha = dest_a;
        rt.BlendOpAlpha = eq_a;
        UINT8 mask = 0;
        if (write_red) mask |= D3D11_COLOR_WRITE_ENABLE_RED;
        if (write_green) mask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
        if (write_blue) mask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
        if (write_alpha) mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
        rt.RenderTargetWriteMask = mask;

        ID3D11BlendState* object = nullptr;
        const HRESULT hr = state().device->CreateBlendState(&desc, &object);
        if (FAILED(hr) || object == nullptr)
        {
            set_last_error("igpu_blend_state_create: the device rejected this blend state");
            return 0;
        }

        return static_cast<std::int64_t>(store_state(DeviceState::StateKind::Blend, object, 0));
    }

    std::int64_t depth_state_create(
        bool depth_test,
        bool depth_write,
        std::int32_t depth_func,
        bool stencil_enable,
        std::int32_t stencil_func,
        std::int32_t stencil_fail,
        std::int32_t stencil_depth_fail,
        std::int32_t stencil_pass,
        std::int32_t stencil_ref,
        std::int32_t stencil_read_mask,
        std::int32_t stencil_write_mask)
    {
        clear_last_error();
        if (!require_device("igpu_depth_state_create"))
        {
            return 0;
        }

        D3D11_COMPARISON_FUNC depth_cmp{};
        D3D11_COMPARISON_FUNC stencil_cmp{};
        D3D11_STENCIL_OP fail_op{};
        D3D11_STENCIL_OP zfail_op{};
        D3D11_STENCIL_OP pass_op{};
        if (!comparison(depth_func, depth_cmp))
        {
            set_last_error("igpu_depth_state_create: depth function must be a cmpfunc_* constant");
            return 0;
        }
        if (!comparison(stencil_func, stencil_cmp) ||
            !stencil_op(stencil_fail, fail_op) ||
            !stencil_op(stencil_depth_fail, zfail_op) ||
            !stencil_op(stencil_pass, pass_op))
        {
            set_last_error("igpu_depth_state_create: stencil function must be cmpfunc_* and stencil ops must be stencilop_*");
            return 0;
        }
        if (stencil_ref < 0 || stencil_ref > 255 ||
            stencil_read_mask < 0 || stencil_read_mask > 255 ||
            stencil_write_mask < 0 || stencil_write_mask > 255)
        {
            set_last_error("igpu_depth_state_create: stencil ref and masks must be in 0..255");
            return 0;
        }

        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = depth_test ? TRUE : FALSE;
        desc.DepthWriteMask = depth_write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = depth_cmp;
        desc.StencilEnable = stencil_enable ? TRUE : FALSE;
        desc.StencilReadMask = static_cast<UINT8>(stencil_read_mask);
        desc.StencilWriteMask = static_cast<UINT8>(stencil_write_mask);
        desc.FrontFace.StencilFailOp = fail_op;
        desc.FrontFace.StencilDepthFailOp = zfail_op;
        desc.FrontFace.StencilPassOp = pass_op;
        desc.FrontFace.StencilFunc = stencil_cmp;
        desc.BackFace = desc.FrontFace;

        ID3D11DepthStencilState* object = nullptr;
        const HRESULT hr = state().device->CreateDepthStencilState(&desc, &object);
        if (FAILED(hr) || object == nullptr)
        {
            set_last_error("igpu_depth_state_create: the device rejected this depth state");
            return 0;
        }

        return static_cast<std::int64_t>(
            store_state(DeviceState::StateKind::Depth, object, static_cast<std::uint32_t>(stencil_ref)));
    }

    std::int64_t raster_state_create(
        std::int32_t cull,
        std::int32_t fill,
        bool scissor,
        bool depth_clip)
    {
        clear_last_error();
        if (!require_device("igpu_raster_state_create"))
        {
            return 0;
        }

        D3D11_CULL_MODE cull_d3d{};
        if (!cull_mode(cull, cull_d3d))
        {
            set_last_error("igpu_raster_state_create: cull must be cull_noculling, cull_clockwise or cull_counterclockwise");
            return 0;
        }

        D3D11_FILL_MODE fill_d3d{};
        if (fill == 0)
        {
            fill_d3d = D3D11_FILL_SOLID;
        }
        else if (fill == 1)
        {
            fill_d3d = D3D11_FILL_WIREFRAME;
        }
        else
        {
            set_last_error("igpu_raster_state_create: fill must be IgpuFill.Solid or IgpuFill.Wireframe");
            return 0;
        }

        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = fill_d3d;
        desc.CullMode = cull_d3d;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthBias = 0;
        desc.DepthBiasClamp = 0.0f;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthClipEnable = depth_clip ? TRUE : FALSE;
        desc.ScissorEnable = scissor ? TRUE : FALSE;
        desc.MultisampleEnable = FALSE;
        desc.AntialiasedLineEnable = FALSE;

        ID3D11RasterizerState* object = nullptr;
        const HRESULT hr = state().device->CreateRasterizerState(&desc, &object);
        if (FAILED(hr) || object == nullptr)
        {
            set_last_error("igpu_raster_state_create: the device rejected this raster state");
            return 0;
        }

        return static_cast<std::int64_t>(store_state(DeviceState::StateKind::Raster, object, 0));
    }

    std::int64_t sampler_state_create_address(std::int32_t filter, std::int32_t address, std::int32_t anisotropy)
    {
        clear_last_error();
        const char* const api = "igpu_sampler_state_create_address";
        if (!require_device(api))
        {
            return 0;
        }
        if (anisotropy < 1 || anisotropy > 16)
        {
            set_last_error(std::string(api) + ": anisotropy must be in 1..16");
            return 0;
        }

        D3D11_FILTER filter_d3d{};
        switch (filter)
        {
        case 0: filter_d3d = D3D11_FILTER_MIN_MAG_MIP_POINT; break;
        case 1: filter_d3d = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break;
        case 2: filter_d3d = D3D11_FILTER_ANISOTROPIC; break;
        default:
            set_last_error(std::string(api) + ": filter must be tf_point, tf_linear or tf_anisotropic");
            return 0;
        }

        // IgpuAddressMode.Border is not accepted here: the colour is a
        // separate argument on igpu_sampler_state_create_border.
        if (address == 3)
        {
            set_last_error(std::string(api) + ": border address mode needs a colour; use igpu_sampler_state_create_border");
            return 0;
        }
        D3D11_TEXTURE_ADDRESS_MODE address_d3d{};
        switch (address)
        {
        case 0: address_d3d = D3D11_TEXTURE_ADDRESS_CLAMP; break;
        case 1: address_d3d = D3D11_TEXTURE_ADDRESS_WRAP; break;
        case 2: address_d3d = D3D11_TEXTURE_ADDRESS_MIRROR; break;
        default:
            set_last_error(std::string(api) + ": address must be IgpuAddressMode.Clamp, Repeat or Mirror");
            return 0;
        }

        D3D11_SAMPLER_DESC desc{};
        desc.Filter = filter_d3d;
        desc.AddressU = address_d3d;
        desc.AddressV = address_d3d;
        desc.AddressW = address_d3d;
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = static_cast<UINT>(anisotropy);
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        ID3D11SamplerState* object = nullptr;
        const HRESULT hr = state().device->CreateSamplerState(&desc, &object);
        if (FAILED(hr) || object == nullptr)
        {
            set_last_error(std::string(api) + ": the device rejected this sampler state");
            return 0;
        }

        return static_cast<std::int64_t>(store_state(DeviceState::StateKind::Sampler, object, 0));
    }

    std::int64_t sampler_state_create_border(
        std::int32_t filter, std::int32_t anisotropy, float red, float green, float blue, float alpha)
    {
        clear_last_error();
        const char* const api = "igpu_sampler_state_create_border";
        if (!require_device(api))
        {
            return 0;
        }
        if (anisotropy < 1 || anisotropy > 16)
        {
            set_last_error(std::string(api) + ": anisotropy must be in 1..16");
            return 0;
        }

        D3D11_FILTER filter_d3d{};
        switch (filter)
        {
        case 0: filter_d3d = D3D11_FILTER_MIN_MAG_MIP_POINT; break;
        case 1: filter_d3d = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break;
        case 2: filter_d3d = D3D11_FILTER_ANISOTROPIC; break;
        default:
            set_last_error(std::string(api) + ": filter must be tf_point, tf_linear or tf_anisotropic");
            return 0;
        }

        D3D11_SAMPLER_DESC desc{};
        desc.Filter = filter_d3d;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        desc.BorderColor[0] = red;
        desc.BorderColor[1] = green;
        desc.BorderColor[2] = blue;
        desc.BorderColor[3] = alpha;
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = static_cast<UINT>(anisotropy);
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        ID3D11SamplerState* object = nullptr;
        const HRESULT hr = state().device->CreateSamplerState(&desc, &object);
        if (FAILED(hr) || object == nullptr)
        {
            set_last_error(std::string(api) + ": the device rejected this sampler state");
            return 0;
        }
        return static_cast<std::int64_t>(store_state(DeviceState::StateKind::Sampler, object, 0));
    }

    bool resolve_sampler_address(const char* api, std::int32_t address, D3D11_TEXTURE_ADDRESS_MODE& out)
    {
        switch (address)
        {
        case 0: out = D3D11_TEXTURE_ADDRESS_CLAMP; return true;
        case 1: out = D3D11_TEXTURE_ADDRESS_WRAP; return true;
        case 2: out = D3D11_TEXTURE_ADDRESS_MIRROR; return true;
        case 3: out = D3D11_TEXTURE_ADDRESS_BORDER; return true;
        default:
            set_last_error(std::string(api) + ": address must be IgpuAddressMode.Clamp, Repeat, Mirror or Border");
            return false;
        }
    }

    bool resolve_sampler_filter(const char* api, std::int32_t filter, D3D11_FILTER& out)
    {
        switch (filter)
        {
        case 0: out = D3D11_FILTER_MIN_MAG_MIP_POINT; return true;
        case 1: out = D3D11_FILTER_MIN_MAG_MIP_LINEAR; return true;
        case 2: out = D3D11_FILTER_ANISOTROPIC; return true;
        default:
            set_last_error(std::string(api) + ": filter must be tf_point, tf_linear or tf_anisotropic");
            return false;
        }
    }

    std::int64_t make_axis_sampler(
        const char* api,
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        bool allow_border,
        const float* border,
        float level_offset,
        float finest,
        float coarsest)
    {
        clear_last_error();
        if (!require_device(api))
        {
            return 0;
        }
        if (anisotropy < 1 || anisotropy > 16)
        {
            set_last_error(std::string(api) + ": anisotropy must be in 1..16");
            return 0;
        }
        D3D11_FILTER filter_d3d{};
        if (!resolve_sampler_filter(api, filter, filter_d3d))
        {
            return 0;
        }
        D3D11_TEXTURE_ADDRESS_MODE mode_u{};
        D3D11_TEXTURE_ADDRESS_MODE mode_v{};
        D3D11_TEXTURE_ADDRESS_MODE mode_w{};
        if (!resolve_sampler_address(api, address_u, mode_u) ||
            !resolve_sampler_address(api, address_v, mode_v) ||
            !resolve_sampler_address(api, address_w, mode_w))
        {
            return 0;
        }
        const bool wants_border = address_u == 3 || address_v == 3 || address_w == 3;
        if (wants_border && !allow_border)
        {
            set_last_error(std::string(api) + ": border address mode needs a colour; use igpu_sampler_state_create_axes_border");
            return 0;
        }

        D3D11_SAMPLER_DESC desc{};
        desc.Filter = filter_d3d;
        desc.AddressU = mode_u;
        desc.AddressV = mode_v;
        desc.AddressW = mode_w;
        if (border != nullptr)
        {
            desc.BorderColor[0] = border[0];
            desc.BorderColor[1] = border[1];
            desc.BorderColor[2] = border[2];
            desc.BorderColor[3] = border[3];
        }
        desc.MipLODBias = level_offset;
        desc.MaxAnisotropy = static_cast<UINT>(anisotropy);
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = finest;
        desc.MaxLOD = coarsest;

        ID3D11SamplerState* object = nullptr;
        const HRESULT hr = state().device->CreateSamplerState(&desc, &object);
        if (FAILED(hr) || object == nullptr)
        {
            set_last_error(std::string(api) + ": the device rejected this sampler state");
            return 0;
        }
        return static_cast<std::int64_t>(store_state(DeviceState::StateKind::Sampler, object, 0));
    }

    std::int64_t sampler_state_create_axes(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy)
    {
        return make_axis_sampler(
            "igpu_sampler_state_create_axes", filter, address_u, address_v, address_w, anisotropy, false, nullptr,
            0.0f, 0.0f, D3D11_FLOAT32_MAX);
    }

    std::int64_t sampler_state_create_axes_border(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        float red,
        float green,
        float blue,
        float alpha)
    {
        const float border[4] = { red, green, blue, alpha };
        return make_axis_sampler(
            "igpu_sampler_state_create_axes_border", filter, address_u, address_v, address_w, anisotropy, true, border,
            0.0f, 0.0f, D3D11_FLOAT32_MAX);
    }

    std::int64_t sampler_state_create_axes_range(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        float level_offset,
        float finest,
        float coarsest)
    {
        return make_axis_sampler(
            "igpu_sampler_state_create_axes_range", filter, address_u, address_v, address_w, anisotropy, false, nullptr,
            level_offset, finest, coarsest);
    }

    std::int64_t sampler_state_create_axes_border_range(
        std::int32_t filter,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        float red,
        float green,
        float blue,
        float alpha,
        float level_offset,
        float finest,
        float coarsest)
    {
        const float border[4] = { red, green, blue, alpha };
        return make_axis_sampler(
            "igpu_sampler_state_create_axes_border_range", filter, address_u, address_v, address_w, anisotropy, true, border,
            level_offset, finest, coarsest);
    }

    bool resolve_filter_type(const char* api, const char* which, std::int32_t filter, D3D11_FILTER_TYPE& out)
    {
        if (filter == 0)
        {
            out = D3D11_FILTER_TYPE_POINT;
            return true;
        }
        if (filter == 1)
        {
            out = D3D11_FILTER_TYPE_LINEAR;
            return true;
        }
        set_last_error(std::string(api) + ": " + which + " must be tf_point or tf_linear");
        return false;
    }

    std::int64_t make_filter_sampler(
        const char* api,
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        bool allow_border,
        const float* border,
        float level_offset,
        float finest,
        float coarsest)
    {
        clear_last_error();
        if (!require_device(api))
        {
            return 0;
        }
        D3D11_FILTER_TYPE mag_type{};
        D3D11_FILTER_TYPE min_type{};
        D3D11_FILTER_TYPE mip_type{};
        if (!resolve_filter_type(api, "magnification", magnification, mag_type) ||
            !resolve_filter_type(api, "minification", minification, min_type) ||
            !resolve_filter_type(api, "mip", mip, mip_type))
        {
            return 0;
        }
        D3D11_TEXTURE_ADDRESS_MODE mode_u{};
        D3D11_TEXTURE_ADDRESS_MODE mode_v{};
        D3D11_TEXTURE_ADDRESS_MODE mode_w{};
        if (!resolve_sampler_address(api, address_u, mode_u) ||
            !resolve_sampler_address(api, address_v, mode_v) ||
            !resolve_sampler_address(api, address_w, mode_w))
        {
            return 0;
        }
        const bool wants_border = address_u == 3 || address_v == 3 || address_w == 3;
        if (wants_border && !allow_border)
        {
            set_last_error(std::string(api) + ": border address mode needs a colour; use igpu_sampler_state_create_filters_border");
            return 0;
        }

        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_ENCODE_BASIC_FILTER(min_type, mag_type, mip_type, D3D11_FILTER_REDUCTION_TYPE_STANDARD);
        desc.AddressU = mode_u;
        desc.AddressV = mode_v;
        desc.AddressW = mode_w;
        if (border != nullptr)
        {
            desc.BorderColor[0] = border[0];
            desc.BorderColor[1] = border[1];
            desc.BorderColor[2] = border[2];
            desc.BorderColor[3] = border[3];
        }
        desc.MipLODBias = level_offset;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = finest;
        desc.MaxLOD = coarsest;

        ID3D11SamplerState* object = nullptr;
        const HRESULT hr = state().device->CreateSamplerState(&desc, &object);
        if (FAILED(hr) || object == nullptr)
        {
            set_last_error(std::string(api) + ": the device rejected this sampler state");
            return 0;
        }
        return static_cast<std::int64_t>(store_state(DeviceState::StateKind::Sampler, object, 0));
    }

    std::int64_t sampler_state_create_filters(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w)
    {
        return make_filter_sampler(
            "igpu_sampler_state_create_filters", magnification, minification, mip,
            address_u, address_v, address_w, false, nullptr, 0.0f, 0.0f, D3D11_FLOAT32_MAX);
    }

    std::int64_t sampler_state_create_filters_border(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        float red,
        float green,
        float blue,
        float alpha)
    {
        const float border[4] = { red, green, blue, alpha };
        return make_filter_sampler(
            "igpu_sampler_state_create_filters_border", magnification, minification, mip,
            address_u, address_v, address_w, true, border, 0.0f, 0.0f, D3D11_FLOAT32_MAX);
    }

    std::int64_t sampler_state_create_filters_offset(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        float level_offset)
    {
        return make_filter_sampler(
            "igpu_sampler_state_create_filters_offset", magnification, minification, mip,
            address_u, address_v, address_w, false, nullptr, level_offset, 0.0f, D3D11_FLOAT32_MAX);
    }

    std::int64_t sampler_state_create_filters_range(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        float level_offset,
        float finest,
        float coarsest)
    {
        return make_filter_sampler(
            "igpu_sampler_state_create_filters_range", magnification, minification, mip,
            address_u, address_v, address_w, false, nullptr, level_offset, finest, coarsest);
    }

    std::int64_t sampler_state_create_filters_border_range(
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        float red,
        float green,
        float blue,
        float alpha,
        float level_offset,
        float finest,
        float coarsest)
    {
        const float border[4] = { red, green, blue, alpha };
        return make_filter_sampler(
            "igpu_sampler_state_create_filters_border_range", magnification, minification, mip,
            address_u, address_v, address_w, true, border, level_offset, finest, coarsest);
    }

    std::int64_t sampler_state_create_compare(
        std::int32_t compare,
        std::int32_t magnification,
        std::int32_t minification,
        std::int32_t mip,
        std::int32_t address_u,
        std::int32_t address_v,
        std::int32_t address_w,
        std::int32_t anisotropy,
        float level_offset,
        float finest,
        float coarsest)
    {
        const char* api = "igpu_sampler_state_create_compare";
        clear_last_error();
        if (!require_device(api))
        {
            return 0;
        }
        D3D11_COMPARISON_FUNC compare_func{};
        if (!comparison(compare, compare_func))
        {
            set_last_error(std::string(api) + ": compare must be a cmpfunc_* constant");
            return 0;
        }
        D3D11_FILTER_TYPE mag_type{};
        D3D11_FILTER_TYPE min_type{};
        D3D11_FILTER_TYPE mip_type{};
        if (!resolve_filter_type(api, "magnification", magnification, mag_type) ||
            !resolve_filter_type(api, "minification", minification, min_type) ||
            !resolve_filter_type(api, "mip", mip, mip_type))
        {
            return 0;
        }
        if (address_u == 3 || address_v == 3 || address_w == 3)
        {
            set_last_error(std::string(api) + ": border address mode is not available on a comparison sampler");
            return 0;
        }
        D3D11_TEXTURE_ADDRESS_MODE mode_u{};
        D3D11_TEXTURE_ADDRESS_MODE mode_v{};
        D3D11_TEXTURE_ADDRESS_MODE mode_w{};
        if (!resolve_sampler_address(api, address_u, mode_u) ||
            !resolve_sampler_address(api, address_v, mode_v) ||
            !resolve_sampler_address(api, address_w, mode_w))
        {
            return 0;
        }

        D3D11_SAMPLER_DESC desc{};
        if (anisotropy > 1)
        {
            desc.Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
            desc.MaxAnisotropy = static_cast<UINT>(anisotropy);
        }
        else
        {
            desc.Filter = D3D11_ENCODE_BASIC_FILTER(
                min_type, mag_type, mip_type, D3D11_FILTER_REDUCTION_TYPE_COMPARISON);
            desc.MaxAnisotropy = 1;
        }
        desc.AddressU = mode_u;
        desc.AddressV = mode_v;
        desc.AddressW = mode_w;
        desc.MipLODBias = level_offset;
        desc.ComparisonFunc = compare_func;
        desc.MinLOD = finest;
        desc.MaxLOD = coarsest;

        ID3D11SamplerState* object = nullptr;
        const HRESULT hr = state().device->CreateSamplerState(&desc, &object);
        if (FAILED(hr) || object == nullptr)
        {
            set_last_error(std::string(api) + ": the device rejected this sampler state");
            return 0;
        }
        return static_cast<std::int64_t>(store_state(DeviceState::StateKind::Sampler, object, 0));
    }

    std::int64_t sampler_state_create(std::int32_t filter, bool repeat, std::int32_t anisotropy)
    {
        const std::int64_t handle = sampler_state_create_address(filter, repeat ? 1 : 0, anisotropy);
        if (!last_error().empty() && last_error().rfind("igpu_sampler_state_create_address:", 0) == 0)
        {
            set_last_error("igpu_sampler_state_create:" + last_error().substr(std::string("igpu_sampler_state_create_address").size()));
        }
        return handle;
    }

    bool state_release(std::uint64_t handle)
    {
        clear_last_error();
        auto& s = state();
        const auto it = s.states.find(handle);
        if (it == s.states.end())
        {
            set_last_error("igpu_state_release: unknown state handle");
            return false;
        }
        if (it->second.object != nullptr)
        {
            it->second.object->Release();
        }
        s.states.erase(it);
        return true;
    }

    bool resolve_draw_states(
        const char* entry,
        std::int64_t blend,
        std::int64_t depth,
        std::int64_t raster,
        std::int64_t sampler,
        ResolvedDrawState& out)
    {
        out = {};
        ID3D11DeviceChild* blend_obj = nullptr;
        ID3D11DeviceChild* depth_obj = nullptr;
        ID3D11DeviceChild* raster_obj = nullptr;
        ID3D11DeviceChild* sampler_obj = nullptr;
        std::uint32_t stencil_ref = 0;

        if (!take_state(entry, "blend", blend, DeviceState::StateKind::Blend, blend_obj, nullptr) ||
            !take_state(entry, "depth", depth, DeviceState::StateKind::Depth, depth_obj, &stencil_ref) ||
            !take_state(entry, "raster", raster, DeviceState::StateKind::Raster, raster_obj, nullptr) ||
            !take_state(entry, "sampler", sampler, DeviceState::StateKind::Sampler, sampler_obj, nullptr))
        {
            return false;
        }

        out.blend = static_cast<ID3D11BlendState*>(blend_obj);
        out.depth = static_cast<ID3D11DepthStencilState*>(depth_obj);
        out.stencil_ref = stencil_ref;
        out.raster = static_cast<ID3D11RasterizerState*>(raster_obj);
        out.sampler = static_cast<ID3D11SamplerState*>(sampler_obj);
        out.any = blend_obj || depth_obj || raster_obj || sampler_obj;
        return true;
    }

    OutputStateGuard::OutputStateGuard(ID3D11DeviceContext* context, const ResolvedDrawState& states)
        : context_(context)
    {
        if (context_ == nullptr || !states.any)
        {
            restored_ = true;
            return;
        }

        active_ = true;
        const FLOAT one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        if (states.blend != nullptr)
        {
            context_->OMGetBlendState(&blend_, blend_factor_, &sample_mask_);
            context_->OMSetBlendState(states.blend, one, 0xffffffff);
            touch_blend_ = true;
        }
        if (states.depth != nullptr)
        {
            context_->OMGetDepthStencilState(&depth_, &stencil_ref_);
            context_->OMSetDepthStencilState(states.depth, states.stencil_ref);
            touch_depth_ = true;
        }
        if (states.raster != nullptr)
        {
            context_->RSGetState(&raster_);
            context_->RSSetState(states.raster);
            touch_raster_ = true;
        }
        if (states.sampler != nullptr)
        {
            context_->PSGetSamplers(0, 1, &sampler_);
            ID3D11SamplerState* bound = states.sampler;
            context_->PSSetSamplers(0, 1, &bound);
            touch_sampler_ = true;
        }
    }

    OutputStateGuard::~OutputStateGuard()
    {
        restore();
    }

    bool OutputStateGuard::restore()
    {
        if (restored_)
        {
            return true;
        }
        restored_ = true;
        if (!active_ || context_ == nullptr)
        {
            return true;
        }

        if (touch_blend_)
        {
            context_->OMSetBlendState(blend_, blend_factor_, sample_mask_);
        }
        if (touch_depth_)
        {
            context_->OMSetDepthStencilState(depth_, stencil_ref_);
        }
        if (touch_raster_)
        {
            context_->RSSetState(raster_);
        }
        if (touch_sampler_)
        {
            context_->PSSetSamplers(0, 1, &sampler_);
        }

        if (blend_ != nullptr) blend_->Release();
        if (depth_ != nullptr) depth_->Release();
        if (raster_ != nullptr) raster_->Release();
        if (sampler_ != nullptr) sampler_->Release();
        blend_ = nullptr;
        depth_ = nullptr;
        raster_ = nullptr;
        sampler_ = nullptr;
        return true;
    }
}
}
