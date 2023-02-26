//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D11.h"
#include "Direct3D11.h"
#include "Direct3D11.Effects.h"
#include "../frontend_sdk/FrontendHelpers.h"
#include "../resource.h"

#include <sstream>
#include <comdef.h>

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
    auto cb = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)(s.size() + 1), nullptr, 0, nullptr, nullptr);
    auto buf = std::make_unique<char[]>(cb);
    buf[0] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)(s.size() + 1), buf.get(), cb, nullptr, nullptr);
    return buf.get();
}

namespace direct3d11 {
frontend_impl::frontend_impl(HWND wnd,
                             wave::size client_size,
                             visual_frontend_callback& callback,
                             visual_frontend_config& conf)
  : mip_count(4)
  , callback(callback)
  , conf(conf)
{
    HRESULT hr = S_OK;

    swap_chain_desc = DXGI_SWAP_CHAIN_DESC{
        .BufferDesc{
          .Width = (UINT)client_size.cx,
          .Height = (UINT)client_size.cy,
          .RefreshRate{ 1, 0 },
          .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
          .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
          .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
        },
        .SampleDesc{ 1, 0 },
        .BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .OutputWindow = wnd,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
        .Flags = 0,
    };

    UINT d3d11_flags = 0;
#if _DEBUG
    d3d11_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL feature_levels[]{ D3D_FEATURE_LEVEL_11_0 };
    hr = D3D11CreateDeviceAndSwapChain(nullptr,
                                       D3D_DRIVER_TYPE_HARDWARE,
                                       nullptr,
                                       d3d11_flags,
                                       std::data(feature_levels),
                                       (UINT)std::size(feature_levels),
                                       D3D11_SDK_VERSION,
                                       &swap_chain_desc,
                                       &swap_chain,
                                       &dev,
                                       &feature_level,
                                       &ctx);

    if (!SUCCEEDED(hr)) {
        std::ostringstream error_stream;
        error_stream << "Direct3D11: device create failed: " << narrow_string(_com_error{ hr }.ErrorMessage()) << ".";
        throw std::exception(error_stream.str().c_str());
    }

    CComPtr<ID3D11Resource> back_buffer;
    swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    hr = dev->CreateRenderTargetView(back_buffer, nullptr, &back_buffer_rt);

    texture_format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    create_default_resources();

    std::span<uint8_t const> default_effect_data;
    {
        auto fx = this->effect_stack.top()->get_effect();
        D3DX11_PASS_SHADER_DESC vs_desc{};
        fx->GetTechniqueByIndex(0)->GetPassByIndex(0)->GetVertexShaderDesc(&vs_desc);
        D3DX11_EFFECT_SHADER_DESC vs_desc2{};
        vs_desc.pShaderVariable->GetShaderDesc(vs_desc.ShaderIndex, &vs_desc2);
        default_effect_data = std::span(vs_desc2.pBytecode, vs_desc2.BytecodeLength);
    }
    create_vertex_resources(default_effect_data);
}

void
frontend_impl::clear()
{
    color c = callback.get_color(config::color_background);
    DirectX::SimpleMath::Color bg(c.r, c.g, c.b, c.a);
    ctx->ClearRenderTargetView(back_buffer_rt, bg);
}

void
frontend_impl::draw()
{
    draw_to_target(swap_chain_desc.BufferDesc.Width, swap_chain_desc.BufferDesc.Height, back_buffer_rt);
}

bool
frontend_impl::draw_to_target(int target_width, int target_height, ID3D11RenderTargetView* render_target)
{
    HRESULT hr = S_OK;
    ctx->OMSetRenderTargets(1, &render_target, nullptr);

    D3D11_VIEWPORT window_viewport{};
    {
        UINT num_viewports = 1;
        ctx->RSGetViewports(&num_viewports, &window_viewport);
    }

    auto draw_quad = [&, this](int idx, int ch, int n) {
        D3D11_VIEWPORT channel_viewport{
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = (float)target_width,
            .Height = (float)target_height,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        };
        using DirectX::SimpleMath::Vector2;
        using DirectX::SimpleMath::Vector4;
        Vector2 sides((float)n - idx - 1, (float)n - idx);
        Vector4 viewport = Vector4((float)target_width, (float)target_height, 0.0f, 0.0f);
        sides /= (float)n;

        std::vector<float> buf;

        if (callback.get_orientation() == config::orientation_horizontal) {
            channel_viewport.TopLeftY = std::floor(sides.x * target_height);
            float h = (sides.y - sides.x) * target_height;
            channel_viewport.Height = std::floor(h);
            viewport.y = floor(h);
            float arr[] = {
                // position2f, texcoord2f
                -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 3.0f, -1.0f, 3.0f, 3.0f, -1.0f, 3.0f, -1.0f,
            };
            buf.insert(buf.end(), std::begin(arr), std::end(arr));
        } else {
            channel_viewport.TopLeftX = std::floor(sides.x * target_width);
            float w = (sides.y - sides.x) * target_width;
            channel_viewport.Width = std::floor(w);
            viewport.x = floor(w);
            float arr[] = {
                -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 3.0f, 1.0f, 3.0f, 3.0f, -1.0f, -3.0f, -1.0f,
            };
            buf.insert(buf.end(), std::begin(arr), std::end(arr));
        }
        CComPtr<ID3D11Buffer> vb;
        {
            D3D11_BUFFER_DESC vb_desc{
                .ByteWidth = (UINT)(std::size(buf) * sizeof(buf[0])),
                .Usage = D3D11_USAGE_IMMUTABLE,
                .BindFlags = D3D11_BIND_VERTEX_BUFFER,
                .CPUAccessFlags = 0,
                .MiscFlags = 0,
                .StructureByteStride = 0,
            };
            D3D11_SUBRESOURCE_DATA vb_srd{ .pSysMem = buf.data() };
            hr = dev->CreateBuffer(&vb_desc, &vb_srd, &vb);
        }

        effect_params.set(parameters::viewport_size, viewport);
        effect_params.set(parameters::waveform_data, channel_textures[ch].second);
        effect_params.set(parameters::channel_magnitude, channel_magnitudes[ch]);
        effect_params.set(parameters::track_magnitude, track_magnitude);
        effect_params.set(parameters::track_time, (float)callback.get_playback_position());
        effect_params.set(parameters::track_duration, (float)callback.get_track_length());
        effect_params.set(parameters::real_time, (float)real_time.get_elapsed());

        CComPtr<ID3DX11Effect> fx = select_effect();
        effect_params.apply_to(fx);

        ctx->IASetInputLayout(decl);
        ctx->RSSetViewports(1, &channel_viewport);

        auto* tech = fx->GetTechniqueByIndex(0);
        D3DX11_TECHNIQUE_DESC tech_desc{};
        tech->GetDesc(&tech_desc);
        for (UINT pass_idx = 0; pass_idx < tech_desc.Passes; ++pass_idx) {
            auto* pass = tech->GetPassByIndex(pass_idx);
            pass->Apply(0, ctx);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            UINT vb_stride = sizeof(float) * 4;
            UINT vb_offset = 0;
            ctx->IASetVertexBuffers(0, 1, &vb.p, &vb_stride, &vb_offset);
            ctx->Draw(3, 0);
        }
    };

    size_t num = channel_order.size();
    auto I = channel_order.begin();
    for (size_t idx = 0; idx < num; ++idx, ++I) {
        draw_quad((int)idx, I->channel, (int)num);
    }
    ctx->RSSetViewports(1, &window_viewport);
    return true;
}

void
frontend_impl::present()
{
    HRESULT hr = swap_chain->Present(1, 0);
}

CComPtr<ID3DX11Effect>
frontend_impl::select_effect()
{
    if (effect_override)
        return effect_override->get_effect();
    return effect_stack.top()->get_effect();
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
    CWindow wnd = this->swap_chain_desc.OutputWindow;
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
    CComPtr<ID3D11Texture2D> rt_tex;
    HRESULT hr = S_OK;
    D3D11_TEXTURE2D_DESC rt_desc{
        .Width = (UINT)settings->width,
        .Height = (UINT)settings->height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc{ 1, 0 },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
    };
    hr = dev->CreateTexture2D(&rt_desc, nullptr, &rt_tex);

    CComPtr<ID3D11Texture2D> rt_stage;
    rt_desc.Usage = D3D11_USAGE_STAGING;
    rt_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = dev->CreateTexture2D(&rt_desc, nullptr, &rt_stage);

    CComPtr<ID3D11RenderTargetView> rt_rtv;
    hr = dev->CreateRenderTargetView(rt_tex, nullptr, &rt_rtv);
    if (FAILED(hr))
        return;

    CComPtr<ID3D11RenderTargetView> old_rt{};
    ctx->OMGetRenderTargets(1, &old_rt, nullptr);
    if (draw_to_target(settings->width, settings->height, rt_rtv)) {
        ctx->CopySubresourceRegion(rt_stage, 0, 0, 0, 0, rt_tex, 0, nullptr);
        D3D11_MAPPED_SUBRESOURCE msr{};
        ctx->Map(rt_stage, 0, D3D11_MAP_READ, 0, &msr);
        settings->write_screenshot(settings->context, (BYTE*)msr.pData);
        ctx->Unmap(rt_stage, 0);
    }
    ctx->OMSetRenderTargets(1, &old_rt.p, nullptr);
}

void
frontend_impl::get_effect_compiler(ref_ptr<effect_compiler>& out)
{
    out.reset(new effect_compiler_impl(dev, ctx));
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
    explicit attribute_setter(CComPtr<ID3DX11Effect> const& fx, std::string const& key)
      : fx(fx)
      , key(key)
    {
    }

    ID3DX11EffectVariable* get() const
    {
        D3DX11_EFFECT_DESC fx_desc{};
        fx->GetDesc(&fx_desc);
        uint32_t const var_count = fx_desc.GlobalVariables;
        for (uint32_t var_idx = 0; var_idx < var_count; ++var_idx) {
            auto* var = fx->GetVariableByIndex(var_idx);
            D3DX11_EFFECT_VARIABLE_DESC var_desc{};
            var->GetDesc(&var_desc);
            if (var_desc.Semantic && key == var_desc.Semantic) {
                return var;
            }
        }
        return nullptr;
    }
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

    // <float, bool, D3DXVECTOR4, D3DXMATRIX, IDirect3DTexture9>
    void apply(float const& f) const
    {
        if (auto h = get())
            if (auto var = h->AsScalar())
                var->SetFloat(f);
    }

    void apply(bool const& b) const
    {
        if (auto h = get())
            if (auto var = h->AsScalar())
                var->SetBool(b);
    }

    void apply(std::array<float, 4> const& a) const
    {
        if (auto h = get())
            if (auto var = h->AsVector())
                var->SetFloatVector(a.data());
    }

    void apply(std::array<float, 16> const& a) const
    {
        if (auto h = get())
            if (auto var = h->AsMatrix())
                var->SetMatrix(a.data());
    }

    void apply(ID3D11ShaderResourceView* tex) const
    {
        if (auto h = get())
            if (auto var = h->AsShaderResource())
                var->SetResource(tex);
    }
};

void
effect_parameters::apply_to(CComPtr<ID3DX11Effect> fx)
{
    for (auto& [var_name, attrib] : attributes) {
        attribute_setter vtor(fx, var_name);
        vtor(attrib);
    }
}
}
}
