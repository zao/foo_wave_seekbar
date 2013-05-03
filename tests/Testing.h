#if defined(_DEBUG)
#define SEEKBAR_TESTING 1
#else
#define SEEKBAR_TESTING 0
#endif
#if SEEKBAR_TESTING
#include "PchSeekbar.h"
#include "../../SDK/foobar2000.h"

namespace tests
{
	// {E26C3024-D4CB-4B2D-9A26-B2A0148FADCA}
	static const GUID tests_group = 
	{ 0xe26c3024, 0xd4cb, 0x4b2d, { 0x9a, 0x26, 0xb2, 0xa0, 0x14, 0x8f, 0xad, 0xca } };

	static contextmenu_group_popup_factory g_tests_group(tests_group, contextmenu_groups::root, "Seekbar tests", 50.0);
}
#endif