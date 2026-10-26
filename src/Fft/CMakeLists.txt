add_library(
    Fft
    Fft.cpp
    )

target_include_directories(Fft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
