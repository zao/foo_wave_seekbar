  cmake ..\foobar2000_sdk ^
    -DFB2K_COMPONENTS=foo_wave_seekbar ^
    -G "Visual Studio 12" -T v120_xp ^
    -DASIO_ROOT=C:\opt\i686-pc-v120_xp\asio-1.5.3 ^
    -DBOOST_ROOT=C:\opt\i686-pc-v120_xp\boost-1.54