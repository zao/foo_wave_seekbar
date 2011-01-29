#include "PchSeekbar.h"
#include "Direct3D.h"
#include "Helpers.h"
#include "frontend_sdk/FrontendHelpers.h"

namespace wave
{
	namespace direct3d9
	{
		void frontend_impl::update_effect_colors()
		{
		#define UPDATE_COLOR(Name)                 \
			{                                      \
				color c = callback.get_color(config::color_##Name);  \
				D3DXCOLOR d(c.r, c.g, c.b, c.a);                     \
				effect_params.set(parameters::Name##_color, D3DXVECTOR4(d)); \
			}

			UPDATE_COLOR(background)
			UPDATE_COLOR(foreground)
			UPDATE_COLOR(highlight)
			UPDATE_COLOR(selection)
		#undef UPDATE_COLOR
		}

		void frontend_impl::update_effect_cursor()
		{
			effect_params.set(parameters::cursor_position, (float)(callback.get_playback_position() / callback.get_track_length()));
			effect_params.set(parameters::cursor_visible, callback.is_cursor_visible());
			effect_params.set(parameters::seek_position, (float)(callback.get_seek_position() / callback.get_track_length()));
			effect_params.set(parameters::seeking, callback.is_seeking());
			effect_params.set(parameters::viewport_size, D3DXVECTOR4((float)pp.BackBufferWidth, (float)pp.BackBufferHeight, 0, 0));
		}

		void frontend_impl::update_data()
		{
			service_ptr_t<waveform> w;
			if (callback.get_waveform(w))
			{
				if (callback.get_downmix_display() && w->get_channel_map() != audio_chunk::channel_config_mono)
				{
					w = downmix_waveform(w);
				}
				channel_numbers = expand_flags(w->get_channel_map());

				channel_order.clear();
				pfc::list_t<channel_info> infos;
				callback.get_channel_infos(infos);
				infos.enumerate([this, &w](channel_info const& info)
				{
					if (!info.enabled)
						return;

					auto I = std::find(channel_numbers.begin(), channel_numbers.end(), info.channel);
					decltype(I) first = channel_numbers.begin();
					if (I != channel_numbers.end())
					{
						HRESULT hr = S_OK;
						if (!channel_textures.count(info.channel))
							channel_textures[info.channel] = create_waveform_texture();

						channel_order += info;

						int idx = std::distance(first, I);

						CComPtr<IDirect3DTexture9> tex = channel_textures[info.channel];
					
						pfc::list_hybrid_t<float, 2048> avg_min, avg_max, avg_rms;
						w->get_field("minimum", idx, avg_min);
						w->get_field("maximum", idx, avg_max);
						w->get_field("rms", idx, avg_rms);

						for (UINT mip = 0; mip < mip_count; ++mip)
						{
							UINT width = 2048 >> mip;
							D3DLOCKED_RECT lock = {};
							hr = tex->LockRect(mip, &lock, 0, 0);
							if (floating_point_texture)
							{
								D3DXFLOAT16* dst = (D3DXFLOAT16*)lock.pBits;
								for (size_t i = 0; i < width; ++i)
								{
									dst[(i << 2) + 0] = avg_min[i];
									dst[(i << 2) + 1] = avg_max[i];
									dst[(i << 2) + 2] = avg_rms[i];
									dst[(i << 2) + 3] = 0.0f;
								}
							}
							else
							{
								uint32_t* dst = (uint32_t*)lock.pBits;
								if (texture_format == D3DFMT_A2R10G10B10)
								{
									uint32_t i_sgn = 3;
									auto project = [](float f) -> uint32_t
									{
										return (uint32_t)clamp(512.0f * (f + 1.0f), 0.0f, 1023.0f);
									};
									for (size_t i = 0; i < width; ++i)
									{
										uint32_t i_min = project(avg_min[i]);
										uint32_t i_max = project(avg_max[i]);
										uint32_t i_rms = project(avg_rms[i]);
										uint32_t val = ((i_sgn & 0x003) << 30)
													 + ((i_min & 0x3FF) << 20)
													 + ((i_max & 0x3FF) << 10)
													 + ((i_rms & 0x3FF) <<  0);
										dst[i] = val;
									}
								}
								else
								{
									uint32_t i_sgn = 0xFF;
									auto project = [](float f) -> uint32_t
									{
										return (uint32_t)clamp(128.0f * (f + 1.0f), 0.0f, 255.0f);
									};
									for (size_t i = 0; i < width; ++i)
									{
										uint32_t i_min = project(avg_min[i]);
										uint32_t i_max = project(avg_max[i]);
										uint32_t i_rms = project(avg_rms[i]);
										uint32_t val = ((i_sgn & 0xFF) << 24)
													 + ((i_min & 0xFF) << 16)
													 + ((i_max & 0xFF) <<  8)
													 + ((i_rms & 0xFF) <<  0);
										dst[i] = val;
									}
								}
							}
							hr = tex->UnlockRect(mip);
							reduce_by_two(avg_min, width);
							reduce_by_two(avg_max, width);
							reduce_by_two(avg_rms, width);
						}
					}
				});
			}
		}

		void frontend_impl::update_replaygain()
		{
			effect_params.set(parameters::replaygain, D3DXVECTOR4(
					callback.get_replaygain(visual_frontend_callback::replaygain_album_gain),
					callback.get_replaygain(visual_frontend_callback::replaygain_track_gain),
					callback.get_replaygain(visual_frontend_callback::replaygain_album_peak),
					callback.get_replaygain(visual_frontend_callback::replaygain_track_peak)
					));
		}

		void frontend_impl::update_orientation()
		{
			effect_params.set(parameters::orientation, 
				config::orientation_horizontal == callback.get_orientation());
		}

		void frontend_impl::update_flipped()
		{
			effect_params.set(parameters::flipped,
				callback.get_flip_display());
		}

		void frontend_impl::update_shade_played()
		{
			effect_params.set(parameters::shade_played,
				callback.get_shade_played());
		}

		void frontend_impl::update_size()
		{
			release_default_resources();
			auto size = callback.get_size();
			pp.BackBufferWidth = size.cx;
			pp.BackBufferHeight = size.cy;
			HRESULT hr = S_OK;
		
			{
				hr = dev->Reset(&pp);
				if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET)
				{
					device_lost = true;
					return;
				}
			}
			create_default_resources();
			update_effect_colors();
			update_effect_cursor();
			update_replaygain();
			update_orientation();
			update_shade_played();
			if (device_lost)
			{
				device_lost = false;
				on_state_changed((state)~0);
			}
		}
	}
}