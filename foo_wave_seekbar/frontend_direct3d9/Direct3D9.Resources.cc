//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include "../frontend_sdk/FrontendHelpers.h"
#include "../resource.h"
#include "Direct3D9.Effects.h"

namespace wave {
namespace direct3d9 {
CComPtr<ID3D11Texture2D>
frontend_impl::create_waveform_texture()
{
    HRESULT hr = S_OK;
    D3D11_TEXTURE2D_DESC const tex_desc = {
        .Width = 2048,
        .Height = 1,
        .MipLevels = mip_count,
        .ArraySize = 1,
        .Format = texture_format,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
    };
    CComPtr<ID3D11Texture2D> tex;
    hr = dev->CreateTexture2D(&tex_desc, nullptr, &tex);
    return tex;
}

void
frontend_impl::create_vertex_resources()
{
    HRESULT hr = S_OK;
    D3D11_BUFFER_DESC vb_desc = {
        .ByteWidth = sizeof(float) * 4 * 3,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        .MiscFlags = 0,
    };
    hr = dev->CreateBuffer(&vb_desc, nullptr, &vb);
}

void
frontend_impl::release_vertex_resources()
{
    vb.Release();
}

// {55F2D182-2CFF-4C59-81AD-0EF2784E9D0F}
const GUID guid_fx_string = { 0x55f2d182, 0x2cff, 0x4c59, { 0x81, 0xad, 0xe, 0xf2, 0x78, 0x4e, 0x9d, 0xf } };

void
frontend_impl::create_default_resources()
{
    HRESULT hr = S_OK;

    CComPtr<ID3D11Resource> back_buffer;
    swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    dev->CreateRenderTargetView(back_buffer, nullptr, &rtv);
    ctx->OMSetRenderTargets(1, &rtv.p, nullptr);

    D3D11_VIEWPORT viewport = {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = (FLOAT)client_size.cx,
        .Height = (FLOAT)client_size.cy,
        .MinDepth = 0,
        .MaxDepth = 1,
    };
    ctx->RSSetViewports(1, &viewport);

    ref_ptr<effect_compiler> compiler;
    get_effect_compiler(compiler);

    auto build_effect = [compiler](std::string body) -> ref_ptr<effect_handle> {
        ref_ptr<effect_handle> fx;
        std::deque<diagnostic_collector::entry> errors;
        bool success = compiler->compile_fragment(fx, diagnostic_collector(errors), body, layer_triangle_input_descs);

        if (!success) {
            auto con = console::formatter();
            con << "Seekbar: Direct3D: effect compile failed.\n";
            for (auto& err : errors) {
                con << "Seekbar: Direct3D:   " << err.line.c_str() << "\n";
            }
        }
        return fx;
    };

    // Compile fallback effect
    {
        std::vector<char> fx_body;
        get_resource_contents(fx_body, IDR_DEFAULT_FX_BODY);
        std::string stored_body(fx_body.begin(), fx_body.end());
        auto fx = build_effect(stored_body);
        if (fx)
            effect_stack.push(fx);
    }

    // Compile current stored effect
    {
        std::string stored_body;
        if (conf.get_configuration_string(guid_fx_string, std_string_sink(stored_body))) {
            ref_ptr<effect_handle> fx;
            fx = build_effect(stored_body);
            if (fx)
                effect_stack.push(fx);
        }
    }

    if (effect_stack.empty())
        throw std::exception("Direct3D9: could not create effects.");
}

void
frontend_impl::release_default_resources()
{
    effect_stack = std::stack<ref_ptr<effect_handle>>();
    effect_override.reset();
    rtv.Release();
    ID3D11RenderTargetView* null_rtv{};
    ctx->OMSetRenderTargets(1, &null_rtv, nullptr);
}
}
}
