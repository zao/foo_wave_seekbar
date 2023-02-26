//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D11.h"
#include "Direct3D11.h"
#include "../frontend_sdk/FrontendHelpers.h"
#include <sstream>

namespace wave {
inline void
reduce_by_two(pfc::list_base_t<float>& data, UINT n)
{
    for (UINT i = 0; i < n; i += 2) {
        float avg = (data[i] + data[i + 1]) / 2.0f;
        data.replace_item(i >> 1, avg);
    }
}

template<typename T>
T
clamp(T v, T a, T b)
{
    return (std::max)(a, (std::min)(b, v));
}

namespace direct3d11 {
void
frontend_impl::update_effect_colors()
{
#define UPDATE_COLOR(Name)                                                                                             \
    {                                                                                                                  \
        color c = callback.get_color(config::color_##Name);                                                            \
        DirectX::SimpleMath::Color d(c.r, c.g, c.b, c.a);                                                              \
        effect_params.set(parameters::Name##_color, d.ToVector4());                                                    \
    }

    UPDATE_COLOR(background);
    UPDATE_COLOR(foreground);
    UPDATE_COLOR(highlight);
    UPDATE_COLOR(selection);
#undef UPDATE_COLOR
}

void
frontend_impl::update_effect_cursor()
{
    effect_params.set(parameters::cursor_position,
                      (float)(callback.get_playback_position() / callback.get_track_length()));
    effect_params.set(parameters::cursor_visible, callback.is_cursor_visible());
    effect_params.set(parameters::seek_position, (float)(callback.get_seek_position() / callback.get_track_length()));
    effect_params.set(parameters::seeking, callback.is_seeking());
    auto& bd = swap_chain_desc.BufferDesc;
    effect_params.set(parameters::viewport_size,
                      DirectX::SimpleMath::Vector4{ (float)bd.Width, (float)bd.Height, 0, 0 });
}

template<typename I, typename T>
T const&
min_element_or_default(I first, I last, T const& def)
{
    if (first == last)
        return def;
    return *std::min_element(first, last);
}

template<typename I, typename T>
T const&
max_element_or_default(I first, I last, T const& def)
{
    if (first == last)
        return def;
    return *std::max_element(first, last);
}

void
frontend_impl::update_data()
{
    using DirectX::SimpleMath::Vector4;

    ref_ptr<waveform> w;
    if (!callback.get_waveform(w))
        w = make_placeholder_waveform();

    {
        switch (callback.get_downmix_display()) {
            case config::downmix_mono:
                if (w->get_channel_count() > 1)
                    w = downmix_waveform(w, 1);
                break;
            case config::downmix_stereo:
                if (w->get_channel_count() > 2)
                    w = downmix_waveform(w, 2);
                break;
        }
        channel_numbers = expand_flags(w->get_channel_map());

        Vector4 const init_magnitude(FLT_MAX, -FLT_MAX, 0.0f, 1.0f);
        track_magnitude = init_magnitude;
        channel_order.clear();
        pfc::list_t<channel_info> infos;
        callback.get_channel_infos(list_array_sink<channel_info>(infos));
        for (channel_info const& info : infos) {
            if (!info.enabled)
                return;

            auto I = std::find(channel_numbers.begin(), channel_numbers.end(), info.channel);
            decltype(I) first = channel_numbers.begin();
            if (I != channel_numbers.end()) {
                HRESULT hr = S_OK;
                if (!channel_textures.count(info.channel))
                    channel_textures[info.channel] = create_waveform_texture();

                channel_order.push_front(info);

                auto idx = (unsigned)std::distance(first, I);

                auto& [tex, srv] = channel_textures[info.channel];
                auto& magnitude = channel_magnitudes[info.channel];
                magnitude = init_magnitude;

                pfc::list_t<float> avg_min, avg_max, avg_rms;
                w->get_field("minimum", idx, list_array_sink<float>(avg_min));
                w->get_field("maximum", idx, list_array_sink<float>(avg_max));
                w->get_field("rms", idx, list_array_sink<float>(avg_rms));

                {
                    auto min_from = avg_min.get_ptr(), min_to = min_from + avg_min.get_size();
                    auto max_from = avg_max.get_ptr(), max_to = max_from + avg_max.get_size();
                    auto rms_from = avg_rms.get_ptr(), rms_to = rms_from + avg_rms.get_size();

                    auto low = min_element_or_default(min_from, min_to, init_magnitude.x);
                    auto high = max_element_or_default(max_from, max_to, init_magnitude.y);
                    auto rms = max_element_or_default(rms_from, rms_to, init_magnitude.z);
                    magnitude.x = (std::min)(magnitude.x, low);
                    magnitude.y = (std::max)(magnitude.y, high);
                    magnitude.z = (std::max)(magnitude.z, rms);
                    track_magnitude.x = (std::min)(track_magnitude.x, magnitude.x);
                    track_magnitude.y = (std::max)(track_magnitude.y, magnitude.y);
                    track_magnitude.z = (std::max)(track_magnitude.z, magnitude.z);
                    if (magnitude == Vector4(0.0f, 0.0f, 0.0f, 1.0f))
                        magnitude = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                }
                for (UINT mip = 0; mip < mip_count; ++mip) {
                    UINT width = 2048 >> mip;
                    UINT sub_res = D3D11CalcSubresource(mip, 0, mip_count);
                    std::vector<DirectX::SimpleMath::Vector4> mip_buf(width);
                    UINT mip_stride = (UINT)std::span(mip_buf).size_bytes();

                    auto* dst = mip_buf.data();
                    for (size_t x_idx = 0; x_idx < width; ++x_idx) {
                        *dst++ = Vector4{ avg_min[x_idx], avg_max[x_idx], avg_rms[x_idx], 0.0f };
                    }

                    D3D11_MAPPED_SUBRESOURCE mip_msr{};
                    ctx->UpdateSubresource(tex, sub_res, nullptr, mip_buf.data(), mip_stride, mip_stride);

                    reduce_by_two(avg_min, width);
                    reduce_by_two(avg_max, width);
                    reduce_by_two(avg_rms, width);
                }
            }
        }
        if (track_magnitude == Vector4(0.0f, 0.0f, 0.0f, 1.0f))
            track_magnitude = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

void
frontend_impl::update_replaygain()
{
    effect_params.set(
      parameters::replaygain,
      DirectX::SimpleMath::Vector4(callback.get_replaygain(visual_frontend_callback::replaygain_album_gain),
                                   callback.get_replaygain(visual_frontend_callback::replaygain_track_gain),
                                   callback.get_replaygain(visual_frontend_callback::replaygain_album_peak),
                                   callback.get_replaygain(visual_frontend_callback::replaygain_track_peak)));
}

void
frontend_impl::update_orientation()
{
    effect_params.set(parameters::orientation, config::orientation_horizontal == callback.get_orientation());
}

void
frontend_impl::update_flipped()
{
    effect_params.set(parameters::flipped, callback.get_flip_display());
}

void
frontend_impl::update_shade_played()
{
    effect_params.set(parameters::shade_played, callback.get_shade_played());
}

void
frontend_impl::update_size()
{
    auto size = callback.get_size();
    swap_chain_desc.BufferDesc.Width = size.cx;
    swap_chain_desc.BufferDesc.Height = size.cy;
    HRESULT hr = S_OK;

    {
        back_buffer_rt.Release();
        hr = swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        CComPtr<ID3D11Resource> back_buffer;
        swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        hr = dev->CreateRenderTargetView(back_buffer, nullptr, &back_buffer_rt);
    }

    update_effect_cursor();
    update_orientation();
}
}
}
