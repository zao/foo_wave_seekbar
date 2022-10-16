#pragma once
#include "waveform_sdk/RefPointer.h"
#include "waveform_sdk/Waveform.h"

namespace wave {
struct waveform_listener
{
    virtual void on_waveform(ref_ptr<waveform>) = 0;
    virtual void on_time(double t) = 0;
    virtual void on_duration(double t) = 0;
    virtual void on_location(playable_location const& loc) = 0;
    virtual void on_play() = 0;
    virtual void on_stop() = 0;
};

struct player : service_base
{
    virtual void register_waveform_listener(waveform_listener*) = 0;
    virtual void deregister_waveform_listener(waveform_listener*) = 0;
    virtual void enumerate_listeners(std::function<void(waveform_listener*)> f) const = 0;

    FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(player)
};

// {BA1D0A2E-14E6-4DE6-AB6E-C64E8ED9FD26}
__declspec(selectany) const GUID player::class_guid = { 0xba1d0a2e,
                                                        0x14e6,
                                                        0x4de6,
                                                        { 0xab, 0x6e, 0xc6, 0x4e, 0x8e, 0xd9, 0xfd, 0x26 } };
}
