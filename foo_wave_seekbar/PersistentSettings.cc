//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PersistentSettings.h"

#pragma comment(lib, "rpcrt4.lib")

#include <regex>
#include <set>
#include <boost/property_tree/info_parser.hpp>

namespace {
static GUID
as_guid(std::string s)
{
    GUID g = {};
    unsigned char* guid_string = reinterpret_cast<unsigned char*>(&s[0]);
    UuidFromStringA(guid_string, &g);
    return g;
}

static std::string
as_string(GUID const& g)
{
    RPC_CSTR guid_string = nullptr;
    UuidToStringA(&g, &guid_string);
    std::string ret = (char const*)guid_string;
    RpcStringFreeA(&guid_string);
    return ret;
}

template<typename T>
static std::string
as_string(T const& t)
{
    return std::to_string(t);
}
}

namespace wave {
persistent_settings::persistent_settings()
  : active_frontend_kind(config::frontend_direct3d9)
  , has_border(true)
  , shade_played(true)
  , display_mode(config::display_normal)
  , flip_display(false)
  , downmix_display(config::downmix_none)
  , generic_strings(&less_guid)
{
    std::fill(colors.begin(), colors.end(), color());
    std::fill(override_colors.begin(), override_colors.end(), false);
#define APPEND_CHANNEL(Ch, Val) channel_order.push_back(std::make_pair((Ch), (Val)))
    APPEND_CHANNEL(audio_chunk::channel_back_left, true);
    APPEND_CHANNEL(audio_chunk::channel_front_left, true);
    APPEND_CHANNEL(audio_chunk::channel_front_center, true);
    APPEND_CHANNEL(audio_chunk::channel_front_right, true);
    APPEND_CHANNEL(audio_chunk::channel_back_right, true);
    APPEND_CHANNEL(audio_chunk::channel_lfe, true);
#undef APPEND_CHANNEL
    insert_remaining_channels();
}

void
persistent_settings::insert_remaining_channels()
{
    std::set<int> all_channels;
#define INSERT_CHANNEL(Ch) all_channels.insert(Ch)
    INSERT_CHANNEL(audio_chunk::channel_back_left);
    INSERT_CHANNEL(audio_chunk::channel_front_left);
    INSERT_CHANNEL(audio_chunk::channel_front_center);
    INSERT_CHANNEL(audio_chunk::channel_front_right);
    INSERT_CHANNEL(audio_chunk::channel_back_right);
    INSERT_CHANNEL(audio_chunk::channel_lfe);
    INSERT_CHANNEL(audio_chunk::channel_front_center_left);
    INSERT_CHANNEL(audio_chunk::channel_front_center_right);
    INSERT_CHANNEL(audio_chunk::channel_back_center);
    INSERT_CHANNEL(audio_chunk::channel_side_left);
    INSERT_CHANNEL(audio_chunk::channel_side_right);
    INSERT_CHANNEL(audio_chunk::channel_top_center);
    INSERT_CHANNEL(audio_chunk::channel_top_front_left);
    INSERT_CHANNEL(audio_chunk::channel_top_front_center);
    INSERT_CHANNEL(audio_chunk::channel_top_front_right);
    INSERT_CHANNEL(audio_chunk::channel_top_back_left);
    INSERT_CHANNEL(audio_chunk::channel_top_back_center);
    INSERT_CHANNEL(audio_chunk::channel_top_back_right);
#undef INSERT_CHANNEL
    for (auto I = all_channels.begin(); I != all_channels.end(); ++I) {
        int ch = *I;
        auto pred = [ch](decltype(channel_order[0]) a) { return a.first == ch; };
        if (std::find_if(channel_order.begin(), channel_order.end(), pred) == channel_order.end()) {
            channel_order.push_back(std::make_pair(ch, false));
        }
    }
}

void
persistent_settings::from_ptree(boost::property_tree::ptree const& src)
{
    active_frontend_kind = static_cast<config::frontend>(src.get<int>("active_frontend_kind"));
    has_border = src.get<bool>("has_border");

    {
        auto colors_tree = src.get_child("colors");
        auto I = colors_tree.begin();
        for (size_t i = 0; i < colors.size(); ++i, ++I) {
            color c;
            c.r = I->second.get<float>("r");
            c.g = I->second.get<float>("g");
            c.b = I->second.get<float>("b");
            c.a = I->second.get<float>("a");
            colors[i] = c;
            override_colors[i] = I->second.get<bool>("override");
        }
    }

    shade_played = src.get<bool>("shade_played");
    display_mode = static_cast<config::display_mode>(src.get<int>("display_mode"));
    try {
        downmix_display = static_cast<config::downmix>(src.get<int>("downmix_display"));
    } catch (boost::property_tree::ptree_bad_data e) {
        // fallback to old to-mono boolean flag
        bool to_mono = src.get<bool>("downmix_display");
        downmix_display = (to_mono ? config::downmix_mono : config::downmix_stereo);
    }

    {
        auto channel_order_tree = src.get_child("channel_order");
        channel_order.clear();
        for (auto I = channel_order_tree.begin(); I != channel_order_tree.end(); ++I) {
            std::pair<int, bool> p;
            p.first = I->second.get<int>("channel");
            p.second = I->second.get<bool>("enabled");
            channel_order.push_back(p);
        }
    }

    {
        auto generic_strings_tree = src.get_child("generic_strings");
        for (auto I = generic_strings_tree.begin(); I != generic_strings_tree.end(); ++I) {
            GUID key = as_guid(I->first);
            std::string value = I->second.get_value<std::string>();
            generic_strings[key] = value;
        }
    }

    flip_display = src.get<bool>("flip_display");
}

void
persistent_settings::to_ptree(boost::property_tree::ptree& out) const
{
    using boost::property_tree::ptree;
    out.put("active_frontend_kind", static_cast<int>(active_frontend_kind));
    out.put("has_border", has_border);

    ptree& colors_pt = out.add("colors", "");
    for (size_t i = 0; i < colors.size(); ++i) {
        color const& c = colors[i];
        bool overridden = override_colors[i];

        ptree& _ = colors_pt.add("color", "");
        _.add("r", c.r);
        _.add("g", c.g);
        _.add("b", c.b);
        _.add("a", c.a);
        _.add("override", overridden);
    }

    out.put("shade_played", shade_played);
    out.put("display_mode", display_mode);
    out.put("flip_display", flip_display);
    out.put("downmix_display", downmix_display);

    ptree& channel_order_pt = out.add("channel_order", "");
    for (auto I = channel_order.begin(); I != channel_order.end(); ++I) {
        auto p = *I;
        ptree& _ = channel_order_pt.add("mapping", "");
        _.add("channel", p.first);
        _.add("enabled", p.second);
    }

    ptree& generic_strings_pt = out.add("generic_strings", "");
    for (auto I = generic_strings.begin(); I != generic_strings.end(); ++I) {
        auto p = *I;
        generic_strings_pt.add(as_string(p.first), p.second);
    }
}
}
