add_library(
    Container
    Container.cpp
    )

target_include_directories(Container PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
