//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include "Direct3D9.Effects.h"
#include <dxgi.h>
#include "../frontend_sdk/FrontendHelpers.h"
#include "../resource.h"

#include <sstream>
#include <comdef.h>
#include <span>

namespace wave {
template<typename T>
inline T
lerp(T a, T b, float n)
{
    return (1.0f - n) * a + n * b;
}

std::string
narrow_string(std::wstring const& s)
{
    auto cch = (int)(s.size() + 1);
    auto cb = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), cch, nullptr, 0, nullptr, nullptr);
    auto buf = std::make_unique<char[]>(cb);
    buf[0] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), cch, buf.get(), cb, nullptr, nullptr);
    return buf.get();
}

namespace direct3d9 {
frontend_impl::frontend_impl(HWND wnd,
                             wave::size client_size,
                             visual_frontend_callback& callback,
                             visual_frontend_config& conf)
  : mip_count(4)
  , callback(callback)
  , conf(conf)
  , client_size(client_size)
{
    HRESULT hr = S_OK;

    DXGI_SWAP_CHAIN_DESC scd = {
        .BufferDesc = {
            .Width = (UINT)client_size.cx,
            .Height = (UINT)client_size.cy,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        },
        .SampleDesc = {1, 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .OutputWindow = wnd,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
        .Flags = 0,
    };

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
    };
    UINT flags = D3D11_CREATE_DEVICE_DEBUG;
    hr = D3D11CreateDeviceAndSwapChain(nullptr,
                                       D3D_DRIVER_TYPE_HARDWARE,
                                       nullptr,
                                       flags,
                                       std::data(feature_levels),
                                       (UINT)std::size(feature_levels),
                                       D3D11_SDK_VERSION,
                                       &scd,
                                       &swap_chain,
                                       &dev,
                                       &feature_level,
                                       &ctx);

    if (!SUCCEEDED(hr)) {
        std::ostringstream error_stream;
        error_stream << "Direct3D9: device create failed: " << narrow_string(_com_error{ hr }.ErrorMessage()) << ".";
        throw std::exception(error_stream.str().c_str());
    }

    texture_format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    create_vertex_resources();
    create_default_resources();
}

void
frontend_impl::clear()
{
    color c = callback.get_color(config::color_background);
    float clear_color[4] = { c.r, c.g, c.b, c.a };
    ctx->ClearRenderTargetView(rtv, clear_color);
}

void
frontend_impl::draw()
{
    draw_to_target(client_size.cx, client_size.cy, rtv);
}

bool
frontend_impl::draw_to_target(int target_width, int target_height, ID3D11RenderTargetView* render_target)
{
    ctx->OMSetRenderTargets(1, &render_target, nullptr);

    UINT num_viewports = 1;
    D3D11_VIEWPORT window_viewport;
    ctx->RSGetViewports(&num_viewports, &window_viewport);

    auto draw_quad = [&, this](int idx, int ch, int n) {
        auto channel_viewport = window_viewport;
        dsm::Vector2 sides((float)n - idx - 1, (float)n - idx);
        dsm::Vector4 viewport = DirectX::XMFLOAT4((float)target_width, (float)target_height, 0.0f, 0.0f);
        sides /= (float)n;

        std::vector<float> buf;

        if (callback.get_orientation() == config::orientation_horizontal) {
            channel_viewport.TopLeftY = floor(sides.x * (float)target_height);
            float h = (sides.y - sides.x) * (float)target_height;
            channel_viewport.Height = floor(h);
            viewport.y = floor(h);
            float arr[] = {
                // position2f, texcoord2f
                -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 3.0f, -1.0f, 3.0f, 3.0f, -1.0f, 3.0f, -1.0f,
            };
            buf.insert(buf.end(), std::begin(arr), std::end(arr));
        } else {
            channel_viewport.TopLeftX = floor(sides.x * (float)target_width);
            float w = (sides.y - sides.x) * target_width;
            channel_viewport.Width = floor(w);
            viewport.x = floor(w);
            float arr[] = {
                -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 3.0f, 1.0f, 3.0f, 3.0f, -1.0f, -3.0f, -1.0f,
            };
            buf.insert(buf.end(), std::begin(arr), std::end(arr));
        }

        effect_params.set(parameters::viewport_size, viewport);

        HRESULT hr = S_OK;
        effect_params.set(parameters::waveform_data, channel_textures[ch]);
        effect_params.set(parameters::channel_magnitude, channel_magnitudes[ch]);
        effect_params.set(parameters::track_magnitude, track_magnitude);
        effect_params.set(parameters::track_time, (float)callback.get_playback_position());
        effect_params.set(parameters::track_duration, (float)callback.get_track_length());
        effect_params.set(parameters::real_time, (float)real_time.get_elapsed());

        auto effect = select_effect();
        auto fx = effect->get_effect();
        effect_params.apply_to(fx);

        // ctx->PSSetShaderResources(0, 1, &channel_textures[ch].p);
        ctx->RSSetViewports(1, &channel_viewport);

        D3DX11_TECHNIQUE_DESC tech_desc;
        ID3DX11EffectTechnique* tech = fx->GetTechniqueByIndex(0);
        tech->GetDesc(&tech_desc);

        {
            D3D11_MAPPED_SUBRESOURCE msr{};
            ctx->Map(vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
            auto buf_span = std::span(buf);
            memcpy(msr.pData, buf_span.data(), buf_span.size_bytes());
            ctx->Unmap(vb, 0);
        }
        for (UINT pass_idx = 0; pass_idx < tech_desc.Passes; ++pass_idx) {
            auto* pass = tech->GetPassByIndex(pass_idx);
            pass->Apply(0, ctx);

            auto il = effect->get_input_layout_for_pass(pass_idx);
            ctx->IASetInputLayout(il);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            UINT stride = sizeof(float) * 4;
            UINT offset = 0;
            ctx->IASetVertexBuffers(0, 1, &vb.p, &stride, &offset);
            ctx->Draw(3, 0);
        }
    };

    size_t num = channel_order.size();
    auto I = channel_order.begin();
    for (size_t idx = 0; idx < num; ++idx, ++I) {
        draw_quad((int)idx, (int)I->channel, (int)num);
    }
    ctx->RSSetViewports(1, &window_viewport);
    return true;
}

void
frontend_impl::present()
{
    swap_chain->Present(0, 0);
}

ref_ptr<effect_handle>
frontend_impl::select_effect()
{
    if (effect_override)
        return effect_override;
    return effect_stack.top();
}

void
frontend_impl::on_state_changed(state s)
{
    if (s & state_size)
        update_size();
    if (s & state_color)
        update_effect_colors();
    if (s & state_position)
        update_effect_cursor();
    if (s & state_replaygain)
        update_replaygain();
    if (s & (state_data | state_channel_order | state_downmix_display))
        update_data();
    if (s & state_orientation)
        update_orientation();
    if (s & state_flip_display)
        update_flipped();
    if (s & state_shade_played)
        update_shade_played();

    DXGI_SWAP_CHAIN_DESC scd{};
    swap_chain->GetDesc(&scd);
    CWindow wnd = scd.OutputWindow;
    wnd.Invalidate(FALSE);
}

void
frontend_impl::show_configuration(HWND parent)
{
    if (config)
        config->BringWindowToTop();
    else {
        ref_ptr<frontend_impl> p(this);
        config.reset(new config_dialog(p));
        config->Create(parent);
    }
}

void
frontend_impl::close_configuration()
{
    if (config) {
        config->DestroyWindow();
        config.reset();
    }
}

void
frontend_impl::make_screenshot(screenshot_settings const* settings)
{
    HRESULT hr = S_OK;

    CComPtr<ID3D11Texture2D> tex;
    CComPtr<ID3D11RenderTargetView> rtv;

    D3D11_TEXTURE2D_DESC tex_desc = {
        .Width = (UINT)settings->width,
        .Height = (UINT)settings->height,
        .MipLevels = 1,
        .ArraySize = 0,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET,
        .CPUAccessFlags = D3D11_CPU_ACCESS_READ,
        .MiscFlags = 0,
    };
    hr = dev->CreateTexture2D(&tex_desc, nullptr, &tex);
    hr = dev->CreateRenderTargetView(tex, nullptr, &rtv);

    CComPtr<ID3D11RenderTargetView> old_rtv;
    CComPtr<ID3D11DepthStencilView> old_dsv;
    ctx->OMGetRenderTargets(1, &old_rtv, &old_dsv);
    if (draw_to_target(settings->width, settings->height, rtv)) {
        D3D11_MAPPED_SUBRESOURCE msr{};
        hr = ctx->Map(tex, 0, D3D11_MAP_READ, 0, &msr);
        settings->write_screenshot(settings->context, (BYTE const*)msr.pData, msr.RowPitch);
    }
    ctx->OMSetRenderTargets(1, &old_rtv.p, old_dsv);
}

void
frontend_impl::get_effect_compiler(ref_ptr<effect_compiler>& out)
{
    out.reset(new effect_compiler_impl(dev));
}

void
frontend_impl::set_effect(ref_ptr<effect_handle> in, bool permanent)
{
    if (permanent) {
        if (effect_stack.size() > 1) // TODO: dynamic count for the future?
            effect_stack.pop();
        effect_stack.push(in);
    } else
        effect_override = in;
}

namespace parameters {
std::string const background_color = "BACKGROUNDCOLOR", foreground_color = "TEXTCOLOR",
                  highlight_color = "HIGHLIGHTCOLOR", selection_color = "SELECTIONCOLOR",

                  cursor_position = "CURSORPOSITION", cursor_visible = "CURSORVISIBLE",

                  seek_position = "SEEKPOSITION", seeking = "SEEKING",

                  viewport_size = "VIEWPORTSIZE", replaygain = "REPLAYGAIN",

                  orientation = "ORIENTATION", flipped = "FLIPPED", shade_played = "SHADEPLAYED",

                  waveform_data = "WAVEFORMDATA",

                  channel_magnitude = "CHANNELMAGNITUDE", track_magnitude = "TRACKMAGNITUDE",

                  track_time = "TRACKTIME", track_duration = "TRACKDURATION", real_time = "REALTIME";
}

struct attribute_setter
{
    CComPtr<ID3DX11Effect> const& fx;
    std::string const& key;
    ID3DX11EffectVariable* var{};

    explicit attribute_setter(CComPtr<ID3DX11Effect> const& fx, std::string const& key)
      : fx(fx)
      , key(key)
    {
        D3DX11_EFFECT_DESC fx_desc{};
        fx->GetDesc(&fx_desc);
        for (size_t var_idx = 0; var_idx < fx_desc.GlobalVariables; ++var_idx) {
            auto* var_cand = fx->GetVariableByIndex((uint32_t)var_idx);
            D3DX11_EFFECT_VARIABLE_DESC var_desc{};
            var_cand->GetDesc(&var_desc);
            if (CSTR_EQUAL == CompareStringA(MAKELCID(LOCALE_INVARIANT, SORT_DEFAULT),
                                             LINGUISTIC_IGNORECASE,
                                             key.c_str(),
                                             (int)key.size(),
                                             var_desc.Semantic,
                                             -1)) {
                var = var_cand;
                return;
            }
        }
    }

    ID3DX11EffectVariable* get() const { return var; }
    typedef effect_parameters::attribute attribute;

    void operator()(attribute const& attr) const
    {
        switch (attr.kind) {
            case attribute::FLOAT:
                apply(attr.f);
                break;
            case attribute::BOOL:
                apply(attr.b);
                break;
            case attribute::VECTOR4:
                apply(attr.v);
                break;
            case attribute::MATRIX:
                apply(attr.m);
                break;
            case attribute::TEXTURE:
                apply(attr.t);
                break;
        }
    }

    // <float, bool, Vector4, Matrix, Texture2D>
    void apply(float const& f) const
    {
        if (auto h = get())
            h->AsScalar()->SetFloat(f);
    }

    void apply(bool const& b) const
    {
        if (auto h = get())
            h->AsScalar()->SetBool(b);
    }

    void apply(std::array<float, 4> const& a) const
    {
        if (auto h = get())
            h->AsVector()->SetFloatVector(a.data());
    }

    void apply(std::array<float, 16> const& a) const
    {
        if (auto h = get())
            h->AsMatrix()->SetMatrix(a.data());
    }

    void apply(ID3D11ShaderResourceView* tex) const
    {
        if (auto h = get())
            h->AsShaderResource()->SetResource(tex);
    }
};

void
effect_parameters::apply_to(CComPtr<ID3DX11Effect> fx)
{
    for (auto& [name, val] : attributes) {
        attribute_setter vtor(fx, name);
        vtor(val);
    }
}
}
}
