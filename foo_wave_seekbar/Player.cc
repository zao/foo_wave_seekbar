#include "Player.h"
#include <SDK/foobar2000-lite.h>
#include <SDK/initquit.h>
#include <SDK/playback_control.h>
#include <SDK/play_callback.h>
#include <SDK/playlist.h>

#include "Cache.h"
#include "Helpers.h"

#include <set>
#include <functional>
#include <mutex>

namespace wave {
struct player_impl : player
{
    player_impl();
    ~player_impl();

    virtual void register_waveform_listener(waveform_listener* p) override
    {
        std::unique_lock<std::mutex> lk(_m);
        _listeners.insert(p);
    }

    virtual void deregister_waveform_listener(waveform_listener* p) override
    {
        std::unique_lock<std::mutex> lk(_m);
        _listeners.erase(p);
    }

    virtual void enumerate_listeners(
      std::function<void(waveform_listener*)> f) const override
    {
        std::unique_lock<std::mutex> lk(_m);
        for (auto I = _listeners.begin(); I != _listeners.end(); ++I) {
            f(*I);
        }
    }

    mutable std::mutex _m;
    std::set<waveform_listener*> _listeners;
};

struct callbacks
  : play_callback_impl_base
  , playlist_callback_impl_base
{
    callbacks()
      : play_callback_impl_base(play_callback::flag_on_playback_all)
      , playlist_callback_impl_base(
          playlist_callback::flag_on_playback_order_changed)
    {}

    service_ptr_t<waveform_query> current_playing_request;
    service_ptr_t<waveform_query> current_selected_request;

    static void invoke_on_waveform(waveform_listener* listener,
                                   ref_ptr<waveform> wf)
    {
        listener->on_waveform(wf);
    }

    void on_waveform_result(playable_location_impl loc, ref_ptr<waveform> wf)
    {
        in_main_thread([=] {
            static_api_ptr_t<player> p;
            static_api_ptr_t<playback_control_v2> pc;
            service_ptr_t<metadb_handle> meta;
            if (pc->get_now_playing(meta) && meta->get_location() == loc) {
                p->enumerate_listeners(
                  std::bind(&invoke_on_waveform, std::placeholders::_1, wf));
            }
        });
    }

    void on_query_result(playable_location_impl loc,
                         service_ptr_t<waveform_query> q)
    {
        on_waveform_result(loc, q->get_waveform());
    }

    virtual void on_playback_new_track(metadb_handle_ptr meta) override
    {
        auto duration = meta->get_length();
        auto const& loc = meta->get_location();
        ref_ptr<waveform> wf;

        static_api_ptr_t<player> p;
        static_api_ptr_t<cache> c;
        if (!c->get_waveform_sync(loc, wf) && !c->is_location_forbidden(loc)) {
            // if not, schedule a scan
            if (current_playing_request.is_valid() &&
                loc != current_playing_request->get_location()) {
                current_playing_request->abort();
                current_playing_request.release();
            }
            if (current_playing_request.is_empty()) {
                auto cb = std::bind(&callbacks::on_query_result,
                                    this,
                                    playable_location_impl(loc),
                                    std::placeholders::_1);
                auto req =
                  c->create_callback_query(loc,
                                           waveform_query::needed_urgency,
                                           waveform_query::unforced_query,
                                           cb);
                current_playing_request = req;
                c->get_waveform(req);
            }
        } else {
            on_waveform_result(loc, wf);
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
        p->enumerate_listeners([&](waveform_listener* l) { l->on_time(t); });
    }

    virtual void on_playback_seek(double t) override { update_time(t); }

    virtual void on_playback_stop(playback_control::t_stop_reason) override
    {
        static_api_ptr_t<player> p;
        p->enumerate_listeners([&](waveform_listener* l) { l->on_stop(); });
    }

    virtual void on_playback_time(double t) override { update_time(t); }

    // uninteresting callbacks
    virtual void on_playback_starting(playback_control::t_track_command,
                                      bool) override
    {}
    virtual void on_playback_pause(bool) override {}
    virtual void on_playback_dynamic_info(const file_info&) override {}
    virtual void on_playback_dynamic_info_track(const file_info&) override {}
    virtual void on_playback_edited(metadb_handle_ptr) override {}
    virtual void on_volume_change(float) override {}
};

player_impl::player_impl() {}

player_impl::~player_impl() {}

static callbacks* g_callbacks = nullptr;

struct callbacks_iq : initquit
{
    virtual void on_init() override
    {
        wave::g_callbacks = new wave::callbacks();
    }

    virtual void on_quit() override { delete wave::g_callbacks; }
};
}

static service_factory_single_t<wave::player_impl> g_player;
static initquit_factory_t<wave::callbacks_iq> g_sadf;
