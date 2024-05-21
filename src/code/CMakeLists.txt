add_executable(main)
target_sources(main
    PRIVATE
    include/array2.h
    include/box2.h
    include/camera.h
    include/evector.h
    include/gpu_shader.h
    include/ivector.h
    include/mathematics.h
    include/noise.h
    include/random.h
    include/ray.h
    include/scalarfield2.h
    include/shader-api.h
    include/stb_image_write.h
    include/stb_image.h
    include/terrainwidget.h
    include/texture.h
    include/window.h
    PRIVATE
    src/array2.cpp
    src/box2.cpp
    src/camera.cpp
    src/evector.cpp
    src/evector2.cpp
    src/gpu_shader_deposition.cpp
    src/gpu_shader_erosion.cpp
    src/gpu_shader_thermal.cpp
    src/ivector.cpp
    src/main.cpp
    src/mathematics.cpp
    src/random.cpp
    src/ray.cpp
    src/scalarfield2.cpp
    src/shader-api.cpp
    src/terrainwidget.cpp
    src/texture.cpp
    src/window.cpp
)

target_include_directories(main PRIVATE include)

target_link_libraries(main
    PRIVATE
    glfw
    glm
    libglew_static
    imgui
)

# Set RESOURCE_DIR to point to source tree
target_compile_definitions(main PUBLIC -DRESOURCE_DIR="${PROJECT_SOURCE_DIR}/data")

