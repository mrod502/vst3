add_library(
    Fft
    Fft.cpp
    )

target_include_directories(Fft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Fft PRIVATE EntangleAsserts)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
