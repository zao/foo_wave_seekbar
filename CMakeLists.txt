project(foo_wave_seekbar)
cmake_minimum_required(VERSION 3.23)
set(CMAKE_CXX_STANDARD 20)

# The package boost is compatible with built-in CMake targets:
# 
#     find_package(Boost REQUIRED [COMPONENTS <libs>...])
#     target_link_libraries(main PRIVATE Boost::boost Boost::<lib1> Boost::<lib2> ...)
#
#     # this is heuristically generated, and may not be correct
#     find_package(effects11 CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE Microsoft::Effects11)
#
# directxtk provides CMake targets:
# 
#     # this is heuristically generated, and may not be correct
#     find_package(directxtk CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE Microsoft::DirectXTK)
#
# The package fmt provides CMake targets:
# 
#     find_package(fmt CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE fmt::fmt)
# 
#     # Or use the header-only version
#     find_package(fmt CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE fmt::fmt-header-only)
# 
# imgui provides CMake targets:
# 
#     # this is heuristically generated, and may not be correct
#     find_package(imgui CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE imgui::imgui)
#
# jsoncpp provides CMake targets:
# 
#     # this is heuristically generated, and may not be correct
#     find_package(jsoncpp CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE jsoncpp_lib jsoncpp_object JsonCpp::JsonCpp)
# 
# The package liblzma provides CMake targets:
# 
#     find_package(LibLZMA REQUIRED)
#     target_link_libraries(main PRIVATE LibLZMA::LibLZMA)
# 
#     or
# 
#     find_package(liblzma CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE liblzma::liblzma)
# 
# The package nlohmann-json provides CMake targets:
# 
#     find_package(nlohmann_json CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE nlohmann_json::nlohmann_json)
# 
# The package nlohmann-json can be configured to not provide implicit conversions via a custom triplet file:
# 
#     set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)
# 
# For more information, see the docs here:
#     
#     https://json.nlohmann.me/api/macros/json_use_implicit_conversions/
# 
# sqlite3 provides CMake targets:
# 
#     # this is heuristically generated, and may not be correct
#     find_package(unofficial-sqlite3 CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE unofficial::sqlite3::sqlite3)
# 
# wtl is header-only and can be used from CMake via:
# 
#     find_path(WTL_INCLUDE_DIRS "atlapp.h")
#     target_include_directories(main PRIVATE ${WTL_INCLUDE_DIRS})
# 
# The package zlib is compatible with built-in CMake targets:
# 
#     find_package(ZLIB REQUIRED)
#     target_link_libraries(main PRIVATE ZLIB::ZLIB)
#
# The package zstd provides CMake targets:
# 
#     find_package(zstd CONFIG REQUIRED)
#     target_link_libraries(main PRIVATE $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>)

# 7-zip
find_package(7zip CONFIG REQUIRED)

# Boost
find_package(Boost REQUIRED COMPONENTS serialization)

# Columns UI
find_package(unofficial-columns-ui-sdk CONFIG REQUIRED)

# DirectXTK
find_package(directxtk CONFIG REQUIRED)

# FX11
find_package(effects11 CONFIG REQUIRED)

# fmt
find_package(fmt CONFIG REQUIRED)

# foobar2000
find_package(unofficial-foobar2000-sdk CONFIG REQUIRED)

# Dear ImGui
find_package(imgui CONFIG REQUIRED)

# jsoncpp
find_package(jsoncpp CONFIG REQUIRED)

# nlohmann-json
find_package(nlohmann_json CONFIG REQUIRED)

# sqlite3
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# wtl
find_path(WTL_INCLUDE_DIRS "atlapp.h")

# zlib 
find_package(ZLIB REQUIRED)

# zstd
find_package(zstd CONFIG REQUIRED)

add_compile_options(-MP)

add_compile_definitions(UNICODE _UNICODE)

add_subdirectory(foo_wave_seekbar)