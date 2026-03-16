# EnTT - Entity Component System Library
# https://github.com/skypjack/entt
#
# EnTT is a header-only library, we use FetchContent to download it.

include(FetchContent)

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.2  # Latest stable version
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(entt)

# EnTT provides a target called EnTT::EnTT
# Create an alias for consistency with our naming
if(NOT TARGET entt)
    add_library(entt INTERFACE)
    target_link_libraries(entt INTERFACE EnTT::EnTT)
endif()
