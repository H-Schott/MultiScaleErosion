message(STATUS "Fetching glew")

include(FetchContent)

FetchContent_Declare(
    glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG glew-cmake-2.2.0
)

FetchContent_MakeAvailable(glew)

set(ONLY_LIBS ON CACHE INTERNAL "glew lib only")