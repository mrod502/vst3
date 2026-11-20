add_executable(ContainerTests CircularBufferTests.cpp TraceRingTests.cpp)
target_link_libraries(ContainerTests PRIVATE Container EntangleCatch2)
catch_discover_tests(ContainerTests)

# Its own binary for the standing reason: the no-allocation cases replace global operator new, which a
# program may do exactly once, and ContainerTests does not (INDEX §4). Its cases carry the words
# "trace ring", which is what `ctest -R "trace ring"` selects — a target name filters to nothing and exits 0.
add_executable(TraceRingAllocTests TraceRingAllocTests.cpp)
target_link_libraries(TraceRingAllocTests PRIVATE Container EntangleCatch2)
catch_discover_tests(TraceRingAllocTests)
