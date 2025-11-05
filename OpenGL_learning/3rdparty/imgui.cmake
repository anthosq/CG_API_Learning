set(imgui_dir "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

file(GLOB IMGUI_SOURCES CONFIGURE_DEPENDS
    "${imgui_dir}/*.h"
    "${imgui_dir}/*.cpp"
    "${imgui_dir}/backends/imgui_impl_glfw.cpp"
    "${imgui_dir}/backends/imgui_impl_opengl3.cpp"
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${imgui_dir}
    ${imgui_dir}/backends
    )
target_link_libraries(imgui PUBLIC glfw)