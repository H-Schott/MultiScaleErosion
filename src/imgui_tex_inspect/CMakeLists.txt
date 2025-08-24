add_library(imgui_tex_inspect STATIC)
#target_sources(imgui_tex_inspect
#        PUBLIC
#        imgui.h
#        imgui_internal.h
#        backends/imgui_impl_opengl3_loader.h
#        backends/imgui_impl_opengl3.h
#        backends/imgui_impl_glfw.h
#        misc/cpp/imgui_stdlib.h
#
#        PRIVATE
#        imgui.cpp
#        imgui_demo.cpp
#        imgui_draw.cpp
#        imgui_widgets.cpp
#        imgui_tables.cpp
#        backends/imgui_impl_opengl3.cpp
#        backends/imgui_impl_glfw.cpp
#        misc/cpp/imgui_stdlib.cpp
#)


target_sources(
imgui_tex_inspect
        PUBLIC
        imgui_tex_inspect.h
        imgui_tex_inspect.cpp
        backends/tex_inspect_opengl.cpp
        backends/tex_inspect_opengl.h
)

target_include_directories(imgui_tex_inspect PUBLIC
        .
        backends
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/backends
        loaders
)

target_compile_definitions(imgui_tex_inspect PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLEW)

target_link_libraries(imgui_tex_inspect PUBLIC
        glfw
        libglew_static
        imgui
        glm
)
