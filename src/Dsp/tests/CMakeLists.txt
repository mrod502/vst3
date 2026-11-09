add_executable(DspTests
    DcBlockerTests.cpp
    LevelDetectorTests.cpp
    LevelNormalizerTests.cpp
    SoftClipperTests.cpp)
target_link_libraries(DspTests PRIVATE Dsp EntangleCatch2 EntangleAsserts)
catch_discover_tests(DspTests)
