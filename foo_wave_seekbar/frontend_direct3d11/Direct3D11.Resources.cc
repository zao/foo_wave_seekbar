//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D11.h"
#include "Direct3D11.h"
#include "../frontend_sdk/FrontendHelpers.h"
#include "../resource.h"
#include "Direct3D11.Effects.h"

#include <sstream>

namespace wave {
namespace direct3d11 {
frontend_impl::TexWithSrv
frontend_impl::create_waveform_texture()
{
    CComPtr<ID3D11Texture2D> tex;
    D3D11_TEXTURE2D_DESC tex_desc{
        .Width = 2048,
        .Height = 1,
        .MipLevels = mip_count,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .SampleDesc{ 1, 0 },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        .MiscFlags = 0,
    };
    dev->CreateTexture2D(&tex_desc, nullptr, &tex);

    CComPtr<ID3D11ShaderResourceView> srv;
    dev->CreateShaderResourceView(tex, nullptr, &srv);
    return { tex, srv };
}

frontend_impl::EffectChoice
frontend_impl::build_effect_choice(ref_ptr<effect_handle> effect)
{
    HRESULT hr;

    D3D11_INPUT_ELEMENT_DESC ieds[]{
        D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{
          "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    EffectChoice choice{ .effect = effect };
    if (!effect) {
        return {};
    }
    {
        auto fx = effect->get_effect();
        D3DX11_EFFECT_DESC fx_desc{};
        fx->GetDesc(&fx_desc);

        auto tech = fx->GetTechniqueByIndex(0);
        D3DX11_TECHNIQUE_DESC tech_desc{};
        tech->GetDesc(&tech_desc);

        for (uint32_t pass_idx = 0; pass_idx < tech_desc.Passes; ++pass_idx) {
            auto pass = tech->GetPassByIndex(pass_idx);
            D3DX11_PASS_SHADER_DESC vs_desc{};
            pass->GetVertexShaderDesc(&vs_desc);

            D3DX11_EFFECT_SHADER_DESC vs_desc2{};
            vs_desc.pShaderVariable->GetShaderDesc(vs_desc.ShaderIndex, &vs_desc2);
            std::span<uint8_t const> effect_data(vs_desc2.pBytecode, vs_desc2.BytecodeLength);

            CComPtr<ID3D11InputLayout> il;
            hr = dev->CreateInputLayout(
              std::data(ieds), (UINT)std::size(ieds), effect_data.data(), effect_data.size(), &il);
            if (!SUCCEEDED(hr)) {
                console::warning("Direct3D11: could not create input layout.");
                return {};
            }
            choice.per_pass_layout.push_back(il);
        }
    }

    return choice;
}

// {55F2D182-2CFF-4C59-81AD-0EF2784E9D0F}
const GUID guid_fx_string = { 0x55f2d182, 0x2cff, 0x4c59, { 0x81, 0xad, 0xe, 0xf2, 0x78, 0x4e, 0x9d, 0xf } };

void
frontend_impl::create_default_resources()
{
    HRESULT hr = S_OK;

    ref_ptr<effect_compiler> compiler;
    get_effect_compiler(compiler);

    auto build_effect = [compiler](std::string body) -> ref_ptr<effect_handle> {
        ref_ptr<effect_handle> fx;
        std::deque<diagnostic_collector::entry> errors;
        bool success = compiler->compile_fragment(fx, diagnostic_collector(errors), body.c_str(), body.size());

        if (!success) {
            FB2K_console_formatter() << "Seekbar: Direct3D: effect compile failed.";
            std::ostringstream text;
            for (auto& e : errors) {
                text << e.line << std::endl;
            }
            FB2K_console_formatter() << "Seekbar: Direct3D: " << text.str().c_str();
        }
        return fx;
    };

    // Compile fallback effect
    {
        std::vector<char> fx_body;
        get_resource_contents(fx_body, IDR_DEFAULT_FX_BODY);
        std::string stored_body(fx_body.begin(), fx_body.end());
        auto fx = build_effect(stored_body);
        if (fx) {
            auto choice = build_effect_choice(fx);
            permanent_effects.push(choice);
        }
    }

    // Compile current stored effect
    {
        std::string stored_body;
        if (conf.get_configuration_string(guid_fx_string, std_string_sink(stored_body))) {
            ref_ptr<effect_handle> fx;
            fx = build_effect(stored_body);
            if (fx) {
                auto choice = build_effect_choice(fx);
                permanent_effects.push(choice);
            }
        }
    }

    if (permanent_effects.empty())
        throw std::exception("Direct3D9: could not create effects.");
}

void
frontend_impl::release_default_resources()
{
    permanent_effects = {};
    preview_effect = {};
}
}
}