//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "PersistentSettings.h"

#include <fstream>
#include <boost/algorithm/string/find.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/detail/rapidxml.hpp>
namespace rxml = boost::property_tree::detail::rapidxml;

namespace pt = boost::property_tree;
namespace rxml = pt::detail::rapidxml;

#include <boost/phoenix/phoenix.hpp>
namespace phx = boost::phoenix;
namespace phxph = phx::placeholders;

static GUID as_guid(std::string s)
{
	GUID g = {};
	unsigned char* guid_string = reinterpret_cast<unsigned char*>(&s[0]);
	UuidFromStringA(guid_string, &g);
	return g;
}

static std::string as_string(GUID const& g)
{
	RPC_CSTR guid_string = NULL;
	UuidToStringA(&g, &guid_string);
	std::string ret = (char const*)guid_string;
	RpcStringFreeA(&guid_string);
	return ret;
}

template <typename T>
static std::string as_string(T const& t)
{
	return boost::lexical_cast<std::string>(t);
}

namespace wave
{
	template <typename T>
	typename boost::enable_if<boost::is_unsigned<T>>::type
	scan_int(T& out, char const* from, size_t n)
	{
		std::string s(from, from + n);
		out = (T)boost::lexical_cast<uint64_t>(s);
	}

	template <typename T>
	typename boost::disable_if<boost::is_unsigned<T>>::type
	scan_int(T& out, char const* from, size_t n)
	{
		std::string s(from, from + n);
		out = (T)boost::lexical_cast<int64_t>(s);
	}

	template <typename T>
	void extract_int(rxml::xml_node<>* node, T& out)
	{
		if (!node || strlen(node->value()) == 0)
			boost::throw_exception(std::runtime_error("Usable integer element not found."));
		char* first = node->value();
		char* last = first + node->value_size();
		std::string s(first, last);

		scan_int(out, node->value(), node->value_size());
	}

	inline void extract_bool(rxml::xml_node<>* node, bool& out)
	{
		int x = 0;
		extract_int(node, x);
		out = !!x;
	}

	template <typename T>
	void extract_float(rxml::xml_node<>* node, T& out)
	{
		if (!node || strlen(node->value()) == 0)
			boost::throw_exception(std::runtime_error("Usable float element not found."));
		char* first = node->value();
		char* last = first + node->value_size();
		out = (T)strtod(first, &last);
	}

	template <typename T, typename F>
	void extract_array(rxml::xml_node<>* node, T& out, F f)
	{
		if (!node)
			boost::throw_exception(std::runtime_error("Usable array element not found."));
		node = node->first_node("elems");
		if (!node)
			boost::throw_exception(std::runtime_error("Tree of elems for array not found."));
		auto item_node = node->first_node("item");
		size_t i = 0;
		for (;
		     item_node && i < out.size();
		     item_node = item_node->next_sibling("item"), ++i)
		{
			f(item_node, out[i]);
		}
		if (i != out.size())
			boost::throw_exception(std::runtime_error("Item count mismatch for array."));
	}

	template <typename T, typename F>
	void extract_carray(rxml::xml_node<>* node, T* out, size_t N, F f)
	{
		size_t const num_elems = N;
		if (!node)
			boost::throw_exception(std::runtime_error("Usable C-array element not found."));
		auto item_node = node->first_node("item");
		size_t i = 0;
		for (;
		     item_node && i < num_elems;
		     item_node = item_node->next_sibling("item"), ++i)
		{
			f(item_node, out[i]);
		}
		if (i != num_elems)
			boost::throw_exception(std::runtime_error("Item count mismatch for array."));

	}

	template <typename T, typename F>
	void extract_vector(rxml::xml_node<>* node, std::vector<T>& out, F f)
	{
		if (!node)
			boost::throw_exception(std::runtime_error("Usable vector element not found."));
		auto item_node = node->first_node("item");
		for (;
		     item_node;
		     item_node = item_node->next_sibling("item"))
		{
			T t;
			f(item_node, t);
			out.push_back(t);
		}
	}

	template <typename T>
	struct extract_fun
	{
		typedef typename boost::function<void (rxml::xml_node<>*, typename boost::remove_const<T>::type&)> type;
	};

	template <typename M>
	void extract_map(rxml::xml_node<>* node, M& out, typename extract_fun<typename M::key_type>::type key_fun, typename extract_fun<typename M::mapped_type>::type value_fun)
	{
		if (!node)
			boost::throw_exception(std::runtime_error("Usable map element not found."));
		auto item_node = node->first_node("item");
		for (;
		     item_node;
		     item_node = item_node->next_sibling("item"))
		{
			std::pair<typename boost::remove_const<typename M::key_type>::type, typename M::mapped_type> p;
			extract_pair(item_node, p, key_fun, value_fun);
			out[p.first] = p.second;
		}
	}

	template <typename T1, typename T2>
	void extract_pair(rxml::xml_node<>* node, std::pair<T1, T2>& out, typename extract_fun<T1>::type f1, typename extract_fun<T2>::type f2)
	{
		if (!node)
			boost::throw_exception(std::runtime_error("Usable pair element not found."));
		auto first_node = node->first_node("first");
		auto second_node = node->first_node("second");
		f1(first_node, out.first);
		f2(second_node, out.second);
	}

	inline void extract_color(rxml::xml_node<>* node, wave::color& out)
	{
		if (!node)
			boost::throw_exception(std::runtime_error("Usable color element not found."));
		auto r_node = node->first_node("r");
		auto g_node = r_node->next_sibling("g");
		if (!g_node)
			g_node = r_node->next_sibling("r");

		auto b_node = g_node->next_sibling("b");
		if (!b_node)
			b_node = g_node->next_sibling("r");

		auto a_node = node->first_node("a");

		extract_float(r_node, out.r);
		extract_float(g_node, out.g);
		extract_float(b_node, out.b);
		extract_float(a_node, out.a);
	}

	inline void extract_guid(rxml::xml_node<>* node, GUID& out)
	{
		if (!node)
			boost::throw_exception(std::runtime_error("Usable GUID element not found."));
		extract_int(node->first_node("Data1"), out.Data1);
		extract_int(node->first_node("Data2"), out.Data2);
		extract_int(node->first_node("Data3"), out.Data3);
		extract_carray(node->first_node("Data4"), out.Data4, 8, &extract_int<uint8_t>);
	}

	inline void extract_string(rxml::xml_node<>* node, std::string& out)
	{
		if (!node)
			boost::throw_exception(std::runtime_error("Usable string element not found."));
		out.assign(node->value(), node->value() + node->value_size());
	}
}

namespace wave
{
	persistent_settings::persistent_settings()
		: active_frontend_kind(config::frontend_direct3d9), has_border(true), shade_played(true)
		, display_mode(config::display_normal), flip_display(false), downmix_display(config::downmix_none)
		, generic_strings(&less_guid)
	{
		std::fill(colors.begin(), colors.end(), color());
		std::fill(override_colors.begin(), override_colors.end(), false);
		channel_order = map_list_of
			(audio_chunk::channel_back_left, true)
			(audio_chunk::channel_front_left, true)
			(audio_chunk::channel_front_center, true)
			(audio_chunk::channel_front_right, true)
			(audio_chunk::channel_back_right, true)
			(audio_chunk::channel_lfe, true);
		insert_remaining_channels();
	}

	void persistent_settings::insert_remaining_channels()
	{
		std::set<int> all_channels = list_of
			(audio_chunk::channel_back_left)
			(audio_chunk::channel_front_left)
			(audio_chunk::channel_front_center)
			(audio_chunk::channel_front_right)
			(audio_chunk::channel_back_right)
			(audio_chunk::channel_lfe)
			(audio_chunk::channel_front_center_left)
			(audio_chunk::channel_front_center_right)
			(audio_chunk::channel_back_center)
			(audio_chunk::channel_side_left)
			(audio_chunk::channel_side_right)
			(audio_chunk::channel_top_center)
			(audio_chunk::channel_top_front_left)
			(audio_chunk::channel_top_front_center)
			(audio_chunk::channel_top_front_right)
			(audio_chunk::channel_top_back_left)
			(audio_chunk::channel_top_back_center)
			(audio_chunk::channel_top_back_right);
		for each(int ch in all_channels)
		{
			if (std::find_if(channel_order.begin(), channel_order.end(), [ch](decltype(channel_order[0]) const& a) { return a.first == ch; }) == channel_order.end())
				channel_order.push_back(std::make_pair(ch, false));
		}
	}

	void read_s11n_node(rxml::xml_node<>* doc, persistent_settings& settings)
	{
		auto node = doc->first_node("settings");
		if (!node)
			boost::throw_exception(std::runtime_error("Couldn't find <settings> element in saved settings."));

		auto version_attr = node->first_attribute("version");
		std::string version_string(version_attr->value(), version_attr->value() + version_attr->value_size());
		int version = atoi(version_string.c_str());

		if (version >= 0)
			extract_int(node->first_node("active_frontend_kind"), settings.active_frontend_kind);
		if (version >= 2)
			extract_bool(node->first_node("has_border"), settings.has_border);
		if (version >= 3)
			extract_array(node->first_node("colors"), settings.colors, &extract_color);
		if (version >= 4)
			extract_array(node->first_node("override_colors"), settings.override_colors, &extract_bool);
		if (version >= 6)
			extract_bool(node->first_node("shade_played"), settings.shade_played);
		if (version >= 7)
		{
			extract_int(node->first_node("display_mode"), settings.display_mode);
			bool to_mono;
			extract_bool(node->first_node("downmix_display"), to_mono);
			settings.downmix_display = (to_mono ? config::downmix_mono : config::downmix_none);
		}
		if (version >= 9)
		{
			settings.channel_order.clear();
			extract_vector(node->first_node("channel_order"), settings.channel_order, [](rxml::xml_node<>* node, std::pair<int, bool>& pair)
			{
				extract_pair<int, bool>(node, pair, &extract_int<int>, &extract_bool);
			});
			settings.insert_remaining_channels();
		}
		if (version >= 10)
		{
			extract_map(node->first_node("generic_strings"), settings.generic_strings, &extract_guid, &extract_string);
		}
		if (version >= 11)
		{
			extract_bool(node->first_node("flip_display"), settings.flip_display);
		}
	}

	void persistent_settings::from_ptree(boost::property_tree::ptree const& src)
	{
		active_frontend_kind = (config::frontend)src.get<int>("active_frontend_kind");
		has_border = src.get<bool>("has_border");

		{
			auto colors_tree = src.get_child("colors");
			auto I = colors_tree.begin();
			for (size_t i = 0; i < colors.size(); ++i, ++I)
			{
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
		display_mode = (config::display_mode)src.get<int>("display_mode");
		try
		{
			downmix_display = (config::downmix)src.get<int>("downmix_display");
		}
		catch (boost::property_tree::ptree_bad_data e)
		{
			// fallback to old to-mono boolean flag
			bool to_mono = src.get<bool>("downmix_display");
			downmix_display = (to_mono ? config::downmix_mono : config::downmix_stereo);
		}

		{
			auto channel_order_tree = src.get_child("channel_order");
			channel_order.clear();
			for (auto I = channel_order_tree.begin(); I != channel_order_tree.end(); ++I)
			{
				std::pair<int, bool> p;
				p.first = I->second.get<int>("channel");
				p.second = I->second.get<bool>("enabled");
				channel_order.push_back(p);
			}
		}

		{
			auto generic_strings_tree = src.get_child("generic_strings");
			for (auto I = generic_strings_tree.begin(); I != generic_strings_tree.end(); ++I)
			{
				GUID key = as_guid(I->first);
				std::string value = I->second.get_value<std::string>();
				generic_strings[key] = value;
			}
		}

		flip_display = src.get<bool>("flip_display");
	}

	void persistent_settings::to_ptree(boost::property_tree::ptree& out) const
	{
		using boost::property_tree::ptree;
		out.put("active_frontend_kind", (int)active_frontend_kind);
		out.put("has_border", has_border);
			
		ptree& colors_pt = out.add("colors", "");
		for (size_t i = 0; i < colors.size(); ++i)
		{
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
		BOOST_FOREACH(auto& p, channel_order)
		{
			ptree& _ = channel_order_pt.add("mapping", "");
			_.add("channel", p.first);
			_.add("enabled", p.second);
		}
			 
		ptree& generic_strings_pt = out.add("generic_strings", "");
		BOOST_FOREACH(auto& p, generic_strings)
		{
			generic_strings_pt.add(as_string(p.first), p.second);
		}
	}
		
	static std::string slurp_file(std::string filename)
	{
		std::ifstream is(filename.c_str(), std::ios::binary);
		is.seekg(0, std::ios::end);
		auto cb = is.tellg();
		is.seekg(0, std::ios::beg);
		std::string ret((int)cb, '\0');
		is.read(&ret[0], cb);
		return ret;
	}

	static std::string extract_xml_part(std::string const& contents)
	{
		auto first_tag = boost::algorithm::find_first(contents, "<?xml version=");
		auto last_tag = boost::algorithm::find_first(boost::make_iterator_range(first_tag.end(), contents.end()), "</boost_serialization>");
		return std::string(first_tag.begin(), last_tag.end());
	}

	void read_s11n_xml(std::string xml, persistent_settings& settings)
	{
		if (xml.empty())
			return;

		rxml::xml_document<> doc;
		enum { ParseFlags = rxml::parse_declaration_node | rxml::parse_doctype_node };
		doc.parse<ParseFlags>(&xml[0]);
		rxml::xml_node<>* node = doc.first_node("boost_serialization");
		if (node)
		{
			read_s11n_node(node, settings);
		}
	}
}