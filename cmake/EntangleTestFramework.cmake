# Test framework for the JUCE-light libraries: console programs only, never a JUCE link.
#

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
