set(third_party_dir "3rdparty")


if(NOT TARGET glm)
    add_subdirectory(glm)
    set_target_properties(glm PROPERTIES FOLDER ${third_party_dir}/glm)
endif()

if(NOT TARGET glfw)
    option(GLFW_BUILD_EXAMPLES "" OFF)
    option(GLFW_BUILD_TESTS "" OFF)
    option(GLFW_BUILD_DOCS "" OFF)
    option(GLFW_INSTALL "" OFF)
    add_subdirectory(glfw)

    set_target_properties(glfw PROPERTIES FOLDER ${third_party_dir}/glfw)
    set_target_properties(update_mappings PROPERTIES FOLDER ${third_party_dir}/glfw)
endif()

if(NOT TARGET glad)
    include(glad.cmake)
    set_target_properties(glad PROPERTIES FOLDER ${third_party_dir}/glad)
endif()

if(NOT TARGET stb_image)
    add_library(stb_image STATIC ${CMAKE_CURRENT_SOURCE_DIR}/stb_image/stb_image.cpp)
    target_include_directories(stb_image PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/stb_image
    )
    set_target_properties(stb_image PROPERTIES FOLDER ${third_party_dir}/stb_image)
endif()

if(NOT TARGET imgui)
    include(imgui.cmake)
    set_target_properties(imgui PROPERTIES FOLDER ${third_party_dir}/imgui)
endif()

if(NOT TARGET assimp)
    option(ASSIMP_BUILD_TESTS "" OFF)
    option(ASSIMP_BUILD_SAMPLES "" OFF)
    option(ASSIMP_BUILD_DOCS "" OFF)
    option(ASSIMP_INSTALL "" OFF)
    add_subdirectory(assimp)
    set_target_properties(assimp PROPERTIES FOLDER ${third_party_dir}/assimp)
endif()

# EnTT - Entity Component System
if(NOT TARGET entt)
    include(entt.cmake)
endif()

# nlohmann/json - Header-only JSON
if(NOT TARGET nlohmann_json)
    include(nlohmann_json.cmake)
endif()
