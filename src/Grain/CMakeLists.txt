add_library(
    Grain
    OlaAccumulator.cpp
    Window.cpp
    GrainHistory.cpp
    GrainEngine.cpp
    )

target_include_directories(Grain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Grain PUBLIC Engine Fft PRIVATE EntangleAsserts)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
