add_executable(FftTests DftTests.cpp SpectralTests.cpp)
target_link_libraries(FftTests PRIVATE Fft EntangleCatch2)
catch_discover_tests(FftTests)
