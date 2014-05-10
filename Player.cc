#include "PchSeekbar.h"
#include "Player.h"
#include "Cache.h"
#include "Helpers.h"

#include <set>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/mutex.hpp>

namespace wave
{
struct player_impl : player
{
	virtual void register_waveform_listener(waveform_listener* p) override
	{
		boost::unique_lock<boost::mutex> lk(_m);
		_listeners.insert(p);
	}

	virtual void deregister_waveform_listener(waveform_listener* p) override
	{
		boost::unique_lock<boost::mutex> lk(_m);
		_listeners.erase(p);
	}

	virtual void enumerate_listeners(boost::function<void (waveform_listener*)> f) const override
	{
		boost::unique_lock<boost::mutex> lk(_m);
		for (auto I = _listeners.begin(); I != _listeners.end(); ++I) {
			f(*I);
		}
	}

	mutable boost::mutex _m;
	std::set<waveform_listener*> _listeners;
};

struct callbacks : play_callback_impl_base, playlist_callback_impl_base
{
	callbacks()
		: play_callback_impl_base(play_callback::flag_on_playback_all)
		, playlist_callback_impl_base(playlist_callback::flag_on_playback_order_changed)
	{}

	void on_waveform_result(playable_location_impl loc, boost::shared_ptr<get_response> resp)
	{
		boost::function<void(waveform_listener*)> on_waveform = [resp](waveform_listener* l) {
			l->on_waveform(resp->waveform);
		};
		in_main_thread([=]{
			static_api_ptr_t<player> p;
			static_api_ptr_t<playback_control_v2> pc;
			service_ptr_t<metadb_handle> meta;
			if (pc->get_now_playing(meta) && meta->get_location() == loc) {
				p->enumerate_listeners(on_waveform);
			}
		});
	}

	virtual void on_playback_new_track(metadb_handle_ptr meta) override
	{
		auto duration = meta->get_length();
		auto const& loc = meta->get_location();
		ref_ptr<waveform> wf;
		
		static_api_ptr_t<player> p;
		static_api_ptr_t<cache> c;
		if (! c->get_waveform_sync(loc, wf) && ! c->is_location_forbidden(loc)) {
			// if not, schedule a scan
			auto req = boost::make_shared<get_request>();
			req->completion_handler = boost::bind(&callbacks::on_waveform_result, this, playable_location_impl(loc), _1);
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
			l->on_location(loc);
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
		update_time(t);
	}

	virtual void on_playback_stop(playback_control::t_stop_reason) override
	{
		static_api_ptr_t<player> p;
		p->enumerate_listeners([&](waveform_listener* l) {
			l->on_stop();
		});
	}

	virtual void on_playback_time(double t) override
	{
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