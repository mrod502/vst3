cmake_minimum_required(VERSION 3.25)
project(vstplug VERSION 0.0.1)
set(CMAKE_CXX_STANDARD 23)

add_subdirectory(juce)
add_subdirectory(cool-turkey)
include_directories("${CMAKE_SOURCE_DIR}/cool-turkey"
    "${CMAKE_SOURCE_DIR}/cool-turkey/Fft"
    "${CMAKE_SOURCE_DIR}/src/Dsp"
)

include(CTest)
if(BUILD_TESTING)
    include(cmake/EntangleTestFramework.cmake)
endif()

add_subdirectory(src)
# After src: the harness links the plugin's shared-code target.
add_subdirectory(tools)
