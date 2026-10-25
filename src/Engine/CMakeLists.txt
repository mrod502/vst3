add_library(
    Engine
    HopScheduler.cpp
    )

target_include_directories(Engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Engine PUBLIC Container)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
