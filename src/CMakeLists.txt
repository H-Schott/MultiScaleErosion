set (CMAKE_CXX_STANDARD 17)

set(GDAL_BUILD_OPTIONAL_DRIVERS OFF CACHE BOOL "Build optional GDAL drivers" FORCE)
set(OGR_BUILD_OPTIONAL_DRIVERS OFF CACHE BOOL "Build optional OGR drivers" FORCE)

function(add_gdal)
    add_subdirectory(gdal)
endfunction()

#add_subdirectory(gdal)
add_gdal()
add_subdirectory(imgui)
add_subdirectory(imgui_tex_inspect)
add_subdirectory(code)