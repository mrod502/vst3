cmake_minimum_required(VERSION 3.25)
project(vstplug VERSION 0.0.1)

# Without a build type no -O flag reaches any TU and clang compiles the whole plugin at -O0 (F-008).
# Multi-config generators choose the configuration at build time, so only single-config ones get this.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 23)

# Release defines NDEBUG, which compiles out the assert() tripwires in src/ — D-004's
# short-delivery guard in HopScheduler and the L|H and F>=2L-1 invariants in Grain. They are
# deliberate, so they stay live at -O3.
#
# The flag only takes effect in the translation unit that contains the assert: an option set on a
# consumer does not rebuild a static library's own objects. So it is carried by an interface
# target, linked PRIVATE into each JUCE-light library that owns a tripwire and into the JUCE-light
# console tests whose own TUs instantiate the header-inline ones.
#
# Never linked into a target that includes a JUCE header: undefined NDEBUG flips JUCE_DEBUG on
# (juce_TargetPlatform.h), changing JUCE class layout in that TU while the JUCE modules were built
# with NDEBUG — an ODR break, not a style preference.
option(ENTANGLE_ASSERTS "Keep src/ assert() tripwires live in Release" ON)

add_library(EntangleAsserts INTERFACE)
if(ENTANGLE_ASSERTS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(EntangleAsserts INTERFACE -UNDEBUG)
endif()

add_subdirectory(juce)

include(CTest)
if(BUILD_TESTING)
    include(cmake/EntangleTestFramework.cmake)
endif()

add_subdirectory(src)
# After src: the harness links the plugin's shared-code target.
add_subdirectory(tools)
