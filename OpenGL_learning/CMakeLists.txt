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

# ECS 模块 (EnTT) - 位于 scene/ecs/ 下
file(GLOB ECS_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/scene/ecs/*.h"
    "${SOURCE_DIR}/scene/ecs/*.cpp"
)

# 资产模块
file(GLOB ASSET_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/asset/*.h"
    "${SOURCE_DIR}/asset/*.cpp"
)

# 编辑器模块
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/editor/*.h"
    "${SOURCE_DIR}/editor/*.cpp"
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

# 物理模块
file(GLOB PHYSICS_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/physics/*.h"
    "${SOURCE_DIR}/physics/*.cpp"
)

# 旧版源文件（保持兼容）
file(GLOB LEGACY_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_DIR}/*.h"
    "${SOURCE_DIR}/*.cpp"
)

# 排除入口点文件（单独添加）
list(FILTER LEGACY_SOURCES EXCLUDE REGEX ".*main\\.cpp$")
list(FILTER LEGACY_SOURCES EXCLUDE REGEX ".*editor_main\\.cpp$")

# 将 EditorApp 从 EDITOR_SOURCES 分离（它是编辑器可执行文件专用的）
set(EDITOR_APP_SOURCES
    "${SOURCE_DIR}/editor/EditorApp.h"
    "${SOURCE_DIR}/editor/EditorApp.cpp"
)
list(FILTER EDITOR_SOURCES EXCLUDE REGEX ".*EditorApp\\.(h|cpp)$")

# ============================================================================
# 第三方库
# ============================================================================
add_subdirectory(${THIRD_PARTY_DIR})

# ============================================================================
# 引擎静态库 (运行时和编辑器共享)
# ============================================================================
add_library(GLRendererEngine STATIC
    ${CORE_SOURCES}
    ${GRAPHICS_SOURCES}
    ${RENDERER_SOURCES}
    ${SCENE_SOURCES}
    ${ECS_SOURCES}
    ${ASSET_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    ${PHYSICS_SOURCES}
    ${LEGACY_SOURCES}
    ${EDITOR_SOURCES}
)

target_include_directories(GLRendererEngine
    PUBLIC
        "${SOURCE_DIR}"
        "${THIRD_PARTY_DIR}"
)

target_link_libraries(GLRendererEngine
    PUBLIC
        glad
        glfw
        glm
        stb_image
        imgui
        assimp
        EnTT::EnTT
        nlohmann_json::nlohmann_json
)

target_compile_definitions(GLRendererEngine PUBLIC
    $<$<CONFIG:Debug>:_DEBUG>
    GLM_ENABLE_EXPERIMENTAL
)

if(MSVC)
    target_compile_options(GLRendererEngine PRIVATE /W3)
else()
    target_compile_options(GLRendererEngine PRIVATE -Wall -Wextra)
endif()

# ============================================================================
# 运行时可执行文件 (SandboxApp)
# ============================================================================
add_executable(GLRenderer
    "${SOURCE_DIR}/main.cpp"
    ${APP_SOURCES}
)

target_link_libraries(GLRenderer PRIVATE GLRendererEngine)

# ============================================================================
# 编辑器可执行文件 (EditorApp)
# ============================================================================
add_executable(GLRendererEditor
    "${SOURCE_DIR}/editor_main.cpp"
    ${EDITOR_APP_SOURCES}
)

target_link_libraries(GLRendererEditor PRIVATE GLRendererEngine)

# ============================================================================
# 兼容旧的 Test 目标名称
# ============================================================================
add_executable(Test ALIAS GLRenderer)

# ============================================================================
# Visual Studio 启动项目设置
# ============================================================================
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GLRendererEditor)

# ============================================================================
# 输出配置
# ============================================================================
set_target_properties(GLRenderer GLRendererEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJ_ROOT_DIR}/bin"
)

# ============================================================================
# 输出信息
# ============================================================================
message(STATUS "GLRenderer configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  Targets: GLRenderer (Runtime), GLRendererEditor (Editor)")
