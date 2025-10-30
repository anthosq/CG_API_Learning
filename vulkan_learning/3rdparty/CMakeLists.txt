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
