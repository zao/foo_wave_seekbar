//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include <DirectXMath.h>
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

namespace direct3d9 {
void
frontend_impl::update_effect_colors()
{
#define UPDATE_COLOR(Name)                                                                                             \
    {                                                                                                                  \
        color c = callback.get_color(config::color_##Name);                                                            \
        DirectX::XMFLOAT4 d(c.r, c.g, c.b, c.a);                                                                       \
        effect_params.set(parameters::Name##_color, d);                                                                \
    }

    UPDATE_COLOR(background)
    UPDATE_COLOR(foreground)
    UPDATE_COLOR(highlight)
    UPDATE_COLOR(selection)
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
    effect_params.set(parameters::viewport_size, DirectX::XMFLOAT4((float)client_size.cx, (float)client_size.cy, 0, 0));
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

        dsm::Vector4 const init_magnitude(FLT_MAX, -FLT_MAX, 0.0f, 1.0f);
        track_magnitude = init_magnitude;
        channel_order.clear();
        pfc::list_t<channel_info> infos;
        callback.get_channel_infos(list_array_sink<channel_info>(infos));
        for (auto& info : infos) {
            if (!info.enabled)
                continue;

            auto I = std::find(channel_numbers.begin(), channel_numbers.end(), info.channel);
            decltype(I) first = channel_numbers.begin();
            if (I != channel_numbers.end()) {
                HRESULT hr = S_OK;
                if (!channel_textures.count(info.channel)) {
                    auto tex = create_waveform_texture();
                    CComPtr<ID3D11ShaderResourceView> srv;
                    hr = dev->CreateShaderResourceView(tex, nullptr, &srv);
                    channel_textures[info.channel] = srv;
                }

                channel_order.push_front(info);

                auto idx = (unsigned)std::distance(first, I);

                CComPtr<ID3D11ShaderResourceView> tex = channel_textures[info.channel];
                CComPtr<ID3D11Resource> tex_res;
                tex->GetResource(&tex_res);

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
                    if (magnitude == dsm::Vector4{ 0.0f, 0.0f, 0.0f, 1.0f })
                        magnitude = dsm::Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
                }
                std::vector<dsm::Vector4> upload_scratch;
                upload_scratch.resize(2048);
                for (UINT mip = 0; mip < mip_count; ++mip) {
                    UINT width = 2048 >> mip;
                    UINT pitch = width * sizeof(dsm::Vector4);
                    for (size_t i = 0; i < width; ++i) {
                        upload_scratch[i] = dsm::Vector4(
                          avg_min[i], avg_max[i], avg_rms[i], 0.0f); // Alpha of zero means no bias in shader.
                    }
                    ctx->UpdateSubresource(tex_res,
                                           D3D11CalcSubresource(mip, 0, mip_count),
                                           nullptr,
                                           std::data(upload_scratch),
                                           pitch,
                                           pitch);
                    reduce_by_two(avg_min, width);
                    reduce_by_two(avg_max, width);
                    reduce_by_two(avg_rms, width);
                }
            }
        }
        if (track_magnitude == dsm::Vector4{ 0.0f, 0.0f, 0.0f, 1.0f })
            track_magnitude = dsm::Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
    }
}

void
frontend_impl::update_replaygain()
{
    effect_params.set(parameters::replaygain,
                      dsm::Vector4{ callback.get_replaygain(visual_frontend_callback::replaygain_album_gain),
                                    callback.get_replaygain(visual_frontend_callback::replaygain_track_gain),
                                    callback.get_replaygain(visual_frontend_callback::replaygain_album_peak),
                                    callback.get_replaygain(visual_frontend_callback::replaygain_track_peak) });
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
    release_default_resources();
    auto size = callback.get_size();
    client_size = size;
    HRESULT hr = S_OK;

    swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

    create_default_resources();

    update_effect_colors();
    update_effect_cursor();
    update_replaygain();
    update_orientation();
    update_shade_played();
}
}
}
