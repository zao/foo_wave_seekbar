add_library(frontend_sdk INTERFACE
	"FrontendHelpers.h"
	"VisualFrontend.h"
)
set_target_properties(frontend_sdk PROPERTIES
	LINKER_LANGUAGE CXX
)

target_link_libraries(frontend_sdk INTERFACE
	Boost::boost
	Boost::serialization
	unofficial::foobar2000-sdk::foobar2000_component_client
	unofficial::foobar2000-sdk::helpers
	unofficial::foobar2000-sdk::pfc
	unofficial::foobar2000-sdk::SDK
	unofficial::foobar2000-sdk::shared
)