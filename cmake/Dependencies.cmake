include(FetchContent)

# SAFETYHOOK
FetchContent_Declare(
    safetyhook
    GIT_REPOSITORY https://github.com/cursey/safetyhook.git
    GIT_TAG        v0.6.9
    CMAKE_ARGS
        -DSAFETYHOOK_FETCH_ZYDIS=ON
    EXCLUDE_FROM_ALL
)
# SPDLOG
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.3
    EXCLUDE_FROM_ALL
)
# YAML-CPP
FetchContent_Declare(
    yamlcpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    EXCLUDE_FROM_ALL
)

# This will now respect EXCLUDE_FROM_ALL thanks to the FETCHCONTENT_TRY_EXCLUDE_FROM_ALL_* vars
FetchContent_MakeAvailable(
    safetyhook
    spdlog
    yamlcpp
)
