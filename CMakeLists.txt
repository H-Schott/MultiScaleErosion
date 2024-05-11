cmake_minimum_required (VERSION 3.1)

project(MultiScaleErosion)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(glfw)
include(glm)
include(glew)

add_subdirectory(src)