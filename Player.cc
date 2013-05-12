#include "PchSeekbar.h"
#include "Player.h"
#include "Cache.h"
#include "Helpers.h"

#include <mutex>

namespace wave
{
struct player_impl : player
{
	player_impl();

	virtual void register_waveform_listener(waveform_listener* p) override
	{
		std::lock_guard<std::mutex> l(_m);
		_listeners.insert(p);
	}

	virtual void deregister_waveform_listener(waveform_listener* p) override
	{
		std::lock_guard<std::mutex> l(_m);
		_listeners.erase(p);
	}

	virtual void enumerate_listeners(std::function<void (waveform_listener*)> f) const override
	{
		std::lock_guard<std::mutex> l(_m);
		for (auto listener : _listeners)
			f(listener);
	}

	mutable std::mutex _m;
	std::set<waveform_listener*> _listeners;
};

struct callbacks : play_callback_impl_base, playlist_callback_impl_base
{
	callbacks()
		: play_callback_impl_base(play_callback::flag_on_playback_all)
		, playlist_callback_impl_base(playlist_callback::flag_on_playback_order_changed)
	{}

	void on_waveform_result(playable_location_impl loc, shared_ptr<get_response> resp)
	{
		in_main_thread([=]{
			static_api_ptr_t<player> p;
			static_api_ptr_t<playback_control_v2> pc;
			service_ptr_t<metadb_handle> meta;
			if (pc->get_now_playing(meta) && meta->get_location() == loc) {
				p->enumerate_listeners([&](waveform_listener* l) {
					l->on_waveform(resp->waveform);
				});
			}
		});
	}

	virtual void on_playback_new_track(metadb_handle_ptr meta) override
	{
		console::info("on_playback_new_track");
		console::info_location(meta);
		auto duration = meta->get_length();
		auto const& loc = meta->get_location();
		ref_ptr<waveform> wf;
		
		static_api_ptr_t<player> p;
		service_ptr_t<cache_v5> c;
		static_api_ptr_t<cache>()->service_query_t(c);
		if (! c->get_waveform_sync(loc, wf) && ! c->is_location_forbidden(loc)) {
			// if not, schedule a scan
			auto req = boost::make_shared<get_request>();
			req->completion_handler = std::bind(&callbacks::on_waveform_result, this, playable_location_impl(loc), std::placeholders::_1);
			req->location = loc;
			req->user_requested = false;
			c->get_waveform(req);
		}
		else {
			auto resp = boost::make_shared<get_response>();
			resp->waveform = wf;
			on_waveform_result(loc, resp);
		}
		p->enumerate_listeners([&](waveform_listener* l) {
			l->on_duration(duration);
			l->on_time(0.0);
			l->on_play();
		});
	}

	void update_time(double t)
	{	
		static_api_ptr_t<player> p;
		p->enumerate_listeners([&](waveform_listener* l) {
			l->on_time(t);
		});
	}

	virtual void on_playback_seek(double t) override
	{
		console::info("on_playback_seek");
		update_time(t);
	}

	virtual void on_playback_stop(playback_control::t_stop_reason) override
	{
		console::info("on_playback_stop");
		static_api_ptr_t<player> p;
		p->enumerate_listeners([&](waveform_listener* l) {
			l->on_stop();
		});
	}

	virtual void on_playback_time(double t) override
	{
		console::info("on_playback_time");
		update_time(t);
	}
	
	// uninteresting callbacks
	virtual void on_playback_starting(playback_control::t_track_command,bool) override {}
	virtual void on_playback_pause(bool) override {}
	virtual void on_playback_dynamic_info(const file_info &) override {}
	virtual void on_playback_dynamic_info_track(const file_info &) override {}
	virtual void on_playback_edited(metadb_handle_ptr) override {}
	virtual void on_volume_change(float) override {}
};

player_impl::player_impl()
{
}

static callbacks* g_callbacks = nullptr;

struct callbacks_iq : initquit
{
	virtual void on_init() override
	{
		wave::g_callbacks = new wave::callbacks();
	}

	virtual void on_quit() override
	{
		delete wave::g_callbacks;
	}
};
}

static service_factory_single_t<wave::player_impl> g_player;
static initquit_factory_t<wave::callbacks_iq> g_sadf;