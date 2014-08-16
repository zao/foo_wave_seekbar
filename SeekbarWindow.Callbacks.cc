//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "SeekbarWindow.h"
#include <boost/make_shared.hpp>
#include <boost/thread/mutex.hpp>
#include "../helpers/file_info_const_impl.h"

namespace wave
{
	void enqueue(playable_location const& location)
	{
		auto request = boost::make_shared<get_request>();

		request->location.copy(location);
		request->user_requested = false;
		request->completion_handler = [](boost::shared_ptr<get_response>) {};

		static_api_ptr_t<cache> c;
		c->get_waveform(request);
	}

	void seekbar_window::on_waveform(ref_ptr<waveform> wf)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		fe->callback->set_waveform(wf);
		if (fe->frontend)
			fe->frontend->on_state_changed(visual_frontend::state_data);
	}

	void seekbar_window::on_time(double t)
	{
		if (t == 0.0) set_playback_time(t);
	}

	void seekbar_window::on_duration(double t)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		fe->callback->set_cursor_visible(true);
		fe->callback->set_playback_position(0.0);
		fe->callback->set_track_length(t);
		if (fe->frontend)
			fe->frontend->on_state_changed(visual_frontend::state(visual_frontend::state_position | visual_frontend::state_track));
	}

	void seekbar_window::on_location(playable_location const& loc)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		{
			static_api_ptr_t<metadb> db;
			service_ptr_t<metadb_handle> meta;
			db->handle_create(meta, loc);
			fe->displayed_song = meta;
			file_info_const_impl info;
			meta->get_info(info);
			replaygain_info rg = info.get_replaygain();
#define SET_REPLAYGAIN(Name) fe->callback->set_replaygain(visual_frontend_callback::replaygain_##Name, rg.m_##Name);
			SET_REPLAYGAIN(album_gain)
			SET_REPLAYGAIN(track_gain)
			SET_REPLAYGAIN(album_peak)
			SET_REPLAYGAIN(track_peak)
#undef SET_REPLAYGAIN
		}

		set_cursor_position(0.0f);
		fe->callback->set_playable_location(loc);
		fe->callback->set_cursor_visible(true);
		if (fe->frontend)
			fe->frontend->on_state_changed(visual_frontend::state(visual_frontend::state_replaygain | visual_frontend::state_position | visual_frontend::state_track));
	}
	
	void seekbar_window::on_play()
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		fe->callback->set_cursor_visible(true);
		fe->callback->set_playback_position(0.0);
		if (fe->frontend) {
			repaint_timer_id = SetTimer(REPAINT_TIMER_ID, (DWORD)(present_interval / present_scale));
			fe->frontend->on_state_changed(visual_frontend::state(visual_frontend::state_position));
		}
	}

	void seekbar_window::on_stop()
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		fe->callback->set_cursor_visible(false);
		fe->callback->set_playback_position(0.0);
		if (fe->frontend) {
			KillTimer(repaint_timer_id);
			repaint_timer_id = 0;
			fe->frontend->on_state_changed(visual_frontend::state(visual_frontend::state_position));
		}
	}

	static const GUID order_default = { 0xbfc61179, 0x49ad, 0x4e95, { 0x8d, 0x60, 0xa2, 0x27, 0x06, 0x48, 0x55, 0x05 } };
	static const GUID order_repeat_playlist = { 0x681cc6ea, 0x60ae, 0x4bf9, { 0x91, 0x3b, 0xbb, 0x5f, 0x4e, 0x86, 0x4f, 0x2a } };
	static const GUID order_repeat_track = { 0x4bf4b280, 0x0bb4, 0x4dd0, { 0x8e, 0x84, 0x37, 0xc3, 0x20, 0x9c, 0x3d, 0xa2 } };
	static const GUID order_random = { 0x611af974, 0x4316, 0x43ac, { 0xab, 0xec, 0xc2, 0xea, 0xa3, 0x5c, 0x3f, 0x9b } };
	static const GUID order_shuffle_tracks = { 0xc5cf4a57, 0x8c01, 0x480c, { 0xb3, 0x34, 0x36, 0x19, 0x64, 0x5a, 0xda, 0x8b } };
	static const GUID order_shuffle_albums = { 0x499e0b08, 0xc887, 0x48c1, { 0x9c, 0xca, 0x27, 0x37, 0x7c, 0x8b, 0xfd, 0x30 } };
	static const GUID order_shuffle_folders = { 0x83c37600, 0xd725, 0x4727, { 0xb5, 0x3c, 0xbd, 0xef, 0xfe, 0x5f, 0x8d, 0xc7 } };

	void seekbar_window::test_playback_order(t_size playback_order_index)
	{
		if (!core_api::are_services_available())
			return;
		static_api_ptr_t<playlist_manager> pm;

		GUID current_order = pm->playback_order_get_guid(playback_order_index);
		if (current_order == order_default || current_order == order_repeat_playlist || current_order == order_shuffle_albums || current_order == order_shuffle_folders)
		{
			t_size playlist, index;
			if (pm->get_playing_item_location(&playlist, &index))
			{
				t_size count = pm->playlist_get_item_count(playlist);
				metadb_handle_ptr next = pm->playlist_get_item_handle(playlist, (index + 1) % count);
				try
				{
					enqueue(next->get_location());
					possible_next_enqueued = true;
				}
				catch (exception_service_not_found&)
				{}
				catch (exception_service_duplicated&)
				{}
			}
		}
		else
			possible_next_enqueued = true;
	}
}