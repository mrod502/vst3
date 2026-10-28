add_executable(
    GrainTests
    AllocationWatch.cpp
    GrainEngineTests.cpp
    GrainHistoryTests.cpp
    OlaAccumulatorTests.cpp
    RingAndWindowTests.cpp
    )

target_link_libraries(GrainTests PRIVATE Grain Fft EntangleCatch2)
catch_discover_tests(GrainTests)

# The same integration checks over fft::JuceFft, so §6.1's "on both transforms" is two binaries
# running one set of assertions rather than one set of assertions with a caveat. Separate target
# because it is the only Grain test that pulls JUCE: the JUCE-light build must stay light.
add_executable(
    GrainEngineJuceTests
    AllocationWatch.cpp
    GrainEngineJuceTests.cpp
    )

target_link_libraries(GrainEngineJuceTests PRIVATE Grain Fft FftJuce EntangleCatch2)
catch_discover_tests(GrainEngineJuceTests)
