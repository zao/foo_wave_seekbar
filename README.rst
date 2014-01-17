  cmake ..\foobar2000_sdk ^
    "-DFB2K_COMPONENTS=foo_wave_seekbar;foo_vis_osc_dx110;foo_vis_osc_gl" ^
    -G "Visual Studio 12" -T v120_xp ^
    -DASIO_ROOT=D:\opt\i686-pc-v120_xp\asio-1.5.3 ^
    -DBOOST_ROOT=D:\opt\i686-pc-v120_xp\boost-1.54 ^
    -DGLFW_BUILD_EXAMPLES=OFF ^
    -DGLFW_BUILD_TESTS=OFF ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DGLFW_INSTALL=OFF