add_library(imgui STATIC)
target_sources(imgui
    PUBLIC
    imgui.h
    imgui_internal.h
    backends/imgui_impl_opengl3_loader.h
    backends/imgui_impl_opengl3.h
    backends/imgui_impl_glfw.h
    misc/cpp/imgui_stdlib.h

    PRIVATE
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_widgets.cpp
    imgui_tables.cpp
    backends/imgui_impl_opengl3.cpp
    backends/imgui_impl_glfw.cpp
    misc/cpp/imgui_stdlib.cpp
)

target_include_directories(imgui PUBLIC
    imgui.h
    imgui_internal.h
    backends/imgui_impl_opengl3_loader.h
    backends/imgui_impl_opengl3.h
    backends/imgui_impl_glfw.h
    misc/cpp/imgui_stdlib.h
)

target_link_libraries(imgui PUBLIC
    glfw
    libglew_static
    glm
)
