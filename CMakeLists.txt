cmake_minimum_required (VERSION 3.1)
cmake_policy(SET CMP0076 NEW)

project(MultiScaleErosion)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(glfw)
include(glm)
include(glew)

add_subdirectory(src)
