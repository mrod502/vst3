add_executable(EngineTests HopSchedulerTests.cpp)
target_link_libraries(EngineTests PRIVATE Engine EntangleCatch2)
catch_discover_tests(EngineTests)

# Its own binary rather than a second TU of EngineTests: the no-allocation case replaces global
# operator new, which a program may do exactly once — EngineTests already does in
# HopSchedulerTests.cpp. Grain splits its two binaries for the same reason.
add_executable(MixEngineTests MixEngineTests.cpp)
target_link_libraries(MixEngineTests PRIVATE Engine EntangleCatch2)
catch_discover_tests(MixEngineTests)
