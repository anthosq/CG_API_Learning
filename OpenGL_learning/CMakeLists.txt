cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

# 项目名称更新为 GLRenderer
project(GLRenderer LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(BUILD_SHARED_LIBS OFF)

include(CMakeDependentOption)

set(PROJ_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(THIRD_PARTY_DIR "${PROJ_ROOT_DIR}/3rdparty")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJ_ROOT_DIR}/bin")

# ============================================================================
# 按模块组织源文件
# ============================================================================

# 应用模块
file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/app/*.h"
    "${SOURCE_DIR}/app/*.cpp"
)

# 核心模块
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/core/*.h"
    "${SOURCE_DIR}/core/*.cpp"
)

# 图形模块
file(GLOB GRAPHICS_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/graphics/*.h"
    "${SOURCE_DIR}/graphics/*.cpp"
)

# 场景模块
file(GLOB SCENE_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/scene/*.h"
    "${SOURCE_DIR}/scene/*.cpp"
)

# 渲染器模块
file(GLOB RENDERER_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/renderer/*.h"
    "${SOURCE_DIR}/renderer/*.cpp"
)

# UI 模块
file(GLOB UI_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/ui/*.h"
    "${SOURCE_DIR}/ui/*.cpp"
)

# 工具模块
file(GLOB UTILS_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/utils/*.h"
    "${SOURCE_DIR}/utils/*.cpp"
)

# 旧版源文件（保持兼容）
file(GLOB LEGACY_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/*.h"
    "${SOURCE_DIR}/*.cpp"
)

# 排除 main.cpp（单独添加）
list(FILTER LEGACY_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

# ============================================================================
# 第三方库
# ============================================================================
add_subdirectory(${THIRD_PARTY_DIR})

# ============================================================================
# 主可执行文件
# ============================================================================
add_executable(GLRenderer "${SOURCE_DIR}/main.cpp")

# 添加所有源文件
target_sources(GLRenderer PUBLIC
    ${APP_SOURCES}
    ${CORE_SOURCES}
    ${GRAPHICS_SOURCES}
    ${RENDERER_SOURCES}
    ${SCENE_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    ${LEGACY_SOURCES}
)

# 包含目录
target_include_directories(GLRenderer
    PUBLIC
        "${SOURCE_DIR}"
        "${THIRD_PARTY_DIR}"
)

# 链接库
target_link_libraries(GLRenderer
    PUBLIC
        glad
        glfw
        glm
        stb_image
        imgui
        assimp
)

# ============================================================================
# 兼容旧的 Test 目标名称
# ============================================================================
add_executable(Test ALIAS GLRenderer)

# ============================================================================
# 编译选项
# ============================================================================
if(MSVC)
    target_compile_options(GLRenderer PRIVATE /W3)
else()
    target_compile_options(GLRenderer PRIVATE -Wall -Wextra)
endif()

# Debug 模式定义
target_compile_definitions(GLRenderer PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
)

# ============================================================================
# 输出信息
# ============================================================================
message(STATUS "GLRenderer configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
