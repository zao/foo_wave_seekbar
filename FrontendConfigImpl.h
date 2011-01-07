#pragma once

#include "VisualFrontend.h"

namespace wave
{
	struct frontend_config_impl : visual_frontend_config
	{
		explicit frontend_config_impl(persistent_settings& settings)
			: settings(settings)
		{
		}

		virtual bool get_configuration_string(GUID key, pfc::string& out) const
		{
			auto& gs = settings.generic_strings;
			auto I = gs.find(key);
			if (gs.end() != I)
			{
				out.set_string(I->second.c_str(), I->second.size());
				return true;
			}
			return false;
		}
		
		virtual void set_configuration_string(GUID key, pfc::string const& value)
		{
			settings.generic_strings[key] = value.get_ptr();
		}

	private:
		persistent_settings& settings;
	};
}