add_library(
    FftJuce
    JuceFft.cpp
    )

target_include_directories(FftJuce PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    FftJuce
    PUBLIC
        Fft
        juce::juce_dsp
    )

# PUBLIC, not PRIVATE: juce_add_module attaches the module's .cpp as INTERFACE sources, so every
# consumer of FftJuce recompiles juce_dsp.cpp itself. A PRIVATE define would compile FFTWImpl with
# one shape here and another shape downstream — the same class with two layouts in one link.
# Non-Apple only: on macOS vDSP is already compiled in at priority 5 and wins regardless.
if(NOT APPLE)
    target_compile_definitions(FftJuce PUBLIC JUCE_DSP_USE_SHARED_FFTW=1)
endif()

# Same propagation rule, and this one fails loudly instead of silently: the plugin already builds
# juce_core with curl and the web browser off, so a consumer of FftJuce must be told the same or
# the two copies of juce_core disagree and libcurl symbols go undefined at the link.
target_compile_definitions(FftJuce PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

# The attached module closure is compiled into libFftJuce.a itself — ar t on the built archive
# shows juce_core, juce_audio_formats and the FLAC objects sitting in it alongside JuceFft.cpp —
# which is why the PIC rule in the root CMakeLists.txt matters here more than anywhere: this is the
# archive whose non-PIC objects first broke the plugin's shared link, on FLAC symbols no one reading
# the diff would think to look at (F-009).
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
