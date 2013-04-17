add_library(frontend_sdk STATIC
	"FrontendHelpers.h"
	"VisualFrontend.h"
)
set_target_properties(frontend_sdk PROPERTIES
	LINKER_LANGUAGE CXX
)
