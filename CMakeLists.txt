cmake_minimum_required (VERSION 3.6)
cmake_policy(SET CMP0076 NEW)

project(MultiScaleErosion)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)


#set(PNG_BUILD_ZLIB FALSE)
#set(ZLIB_ROOT "C:/Users/GeomodCore/Documents/GitHub/MultiScaleErosion/build/_deps/zlib-src")

include(glfw)
include(glm)
include(glew)
#include(zlib)
#include(libpng)

add_subdirectory(src)
