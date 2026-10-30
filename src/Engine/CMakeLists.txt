add_library(
    Engine
    MixEngine.cpp
    HopScheduler.cpp
    )

target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Engine PUBLIC Container Dsp PRIVATE EntangleAsserts)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
