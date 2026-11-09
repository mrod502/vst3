add_executable(EngineTests HopSchedulerTests.cpp)
target_link_libraries(EngineTests PRIVATE Engine EntangleCatch2)
catch_discover_tests(EngineTests)

# Its own binary rather than a second TU of EngineTests: the no-allocation case replaces global
# operator new, which a program may do exactly once — EngineTests already does in
# HopSchedulerTests.cpp. Grain splits its two binaries for the same reason.
add_executable(MixEngineTests MixEngineTests.cpp)
target_link_libraries(MixEngineTests PRIVATE Engine EntangleCatch2)
catch_discover_tests(MixEngineTests)

# Its own binary for the same reason as above: the zero-allocation leg replaces global operator new.
add_executable(ChannelMapTests ChannelMapTests.cpp)
target_link_libraries(ChannelMapTests PRIVATE Engine EntangleCatch2)
catch_discover_tests(ChannelMapTests)

# Its own binary for the same reason again. The gate's cases all carry the words "layout gate", which is
# what `ctest -R "layout gate"` selects — a target name filters to nothing and exits 0 (INDEX §4).
add_executable(LayoutGateTests LayoutGateTests.cpp)
target_link_libraries(LayoutGateTests PRIVATE Engine EntangleCatch2)
catch_discover_tests(LayoutGateTests)

# The fifth binary in this directory, for the same operator-new reason, and the second thing it holds: the
# reconfigure key's per-field equality and the scheduler's memory measured as allocated bytes. Its cases all
# carry the words "reconfigure", which is what `ctest -R "reconfigure"` selects.
add_executable(ReconfigureTests ReconfigureKeyTests.cpp)
target_link_libraries(ReconfigureTests PRIVATE Engine EntangleCatch2)
catch_discover_tests(ReconfigureTests)
