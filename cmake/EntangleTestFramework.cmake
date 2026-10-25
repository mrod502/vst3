# Test framework for the JUCE-light libraries: console programs only, never a JUCE link.
#
# FetchContent_MakeAvailable(Catch2) is unusable here: cool-turkey/CMakeLists.txt already
# add_subdirectory(Catch2)s it, so the Catch2 / Catch2WithMain targets exist in this build
# tree and a second add_subdirectory fails on the duplicate name. We populate the pinned
# tree and compile its amalgamated sources under our own target, which also survives
# cool-turkey's removal unchanged. See D-005 in docs/decisions.md.

include(FetchContent)

FetchContent_Declare(
    entangle_catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
)
FetchContent_Populate(entangle_catch2)

add_library(EntangleCatch2 STATIC ${entangle_catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp)
target_include_directories(EntangleCatch2 PUBLIC ${entangle_catch2_SOURCE_DIR}/extras)

list(APPEND CMAKE_MODULE_PATH ${entangle_catch2_SOURCE_DIR}/extras)
include(Catch)
