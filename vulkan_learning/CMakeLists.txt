cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

Project(TEST LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(BUILD_SHARED_LIBS OFF)

include(CMakeDependentOption)



set(PROJ_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(THIRD_PARTY_DIR "${PROJ_ROOT_DIR}/3rdparty")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJ_ROOT_DIR}/bin")

# TODO: 命名格式 Configuration-Platform-Project
# add_compile_options(-Wall -Wextra -Wpedantic -Werror)


# file(GLOB_RECURSE sources CONFIGURE_DEPENDS
#     "${SOURCE_DIR}/ *.h"
#     "${SOURCE_DIR}/ *.cpp"
# )

add_subdirectory(${THIRD_PARTY_DIR})

add_executable(Test main.cpp)


# 添加源文件到可执行文件
# target_sources(Test PUBLIC ${sources})


set(vulkan_include ${THIRD_PARTY_DIR}/VulkanSDK/include)
set(vulkan_lib ${THIRD_PARTY_DIR}/VulkanSDK/lib/Win32/vulkan-1.lib)
set(glslangValidator_executable ${THIRD_PARTY_DIR}/VulkanSDK/bin/Win32/glslangValidator.exe)

target_include_directories(Test 
    PUBLIC 
        "${SOURCE_DIR}"
        "${THIRD_PARTY_DIR}"
        "${vulkan_include}"
)

target_link_libraries(Test 
    PUBLIC 
        glfw
        glm
        "${vulkan_lib}"
)