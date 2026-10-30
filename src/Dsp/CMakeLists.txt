add_library(
    Dsp
    DcBlocker.cpp
    LevelDetector.cpp
    )

target_include_directories(Dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Dsp PRIVATE EntangleAsserts)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
