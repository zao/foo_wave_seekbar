//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "frontend_sdk/VisualFrontend.h"

namespace wave
{
	struct frontend_config_impl : visual_frontend_config
	{
		explicit frontend_config_impl(persistent_settings& settings)
			: settings(settings)
		{
		}

		virtual bool get_configuration_string(GUID key, std::string& out) const
		{
			auto& gs = settings.generic_strings;
			auto I = gs.find(key);
			if (gs.end() != I)
			{
				out = I->second;
				return true;
			}
			return false;
		}
		
		virtual void set_configuration_string(GUID key, std::string const& value)
		{
			settings.generic_strings[key] = value;
		}

	private:
		persistent_settings& settings;
	};
}
