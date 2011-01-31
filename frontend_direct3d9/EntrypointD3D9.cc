#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include <boost/extension/extension.hpp>
#include <boost/extension/factory.hpp>
#include <boost/extension/type_map.hpp>

BOOST_EXTENSION_TYPE_MAP_FUNCTION {
	using namespace boost::extensions;
	std::map
	<
		wave::config::frontend,
		factory<wave::visual_frontend,
		HWND,
		wave::size,
		wave::visual_frontend_callback&,
		wave::visual_frontend_config&
		>
	>& factories(types.get());

	factories[wave::config::frontend_direct3d9].set<wave::direct3d9::frontend_impl>();
}
