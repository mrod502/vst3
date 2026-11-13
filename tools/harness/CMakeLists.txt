# The shared plugin-seam harness (M5 item 01): everything the shell-level legs need, reachable from more than
# one binary. Split by the one property that matters — the Core half includes no JUCE header, so the generator
# and fuzz legs of `05` can link it and stay console programs (AGENTS §9).

add_library(EntangleHarnessCore HarnessStream.cpp)
target_include_directories(EntangleHarnessCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Dsp for fractionAboveKnee's dsp::SoftClipper::knee. It is a JUCE-light library, so criterion 11's "no JUCE
# target on the link line" survives this line — that is what decides it, not inspection.
#
# EntangleAsserts stays PRIVATE: -UNDEBUG must never reach an includer that also includes a JUCE header, because
# an undefined NDEBUG flips JUCE_DEBUG and changes the layout of JUCE's classes in that TU while the modules were
# built without it (root CMakeLists.txt).
target_link_libraries(EntangleHarnessCore PUBLIC Dsp PRIVATE EntangleAsserts)

add_library(EntangleHarnessShell HarnessShell.cpp)
target_include_directories(EntangleHarnessShell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/Entangle)

# Same three as OfflineRender and PrepareFootprint, PUBLIC so every consumer compiles the JUCE modules with the
# same configuration the harness's own TU does.
target_compile_definitions(EntangleHarnessShell PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# vstplug is the plugin's shared code (libEntangle_SharedCode.a) and the harness adds no shell .cpp of its own:
# the shell a leg renders and the plugin the VST3 bundle loads have to be the same object code, or §5.1's
# shell-identity legs compare a copy against itself. Grain and FftJuce are linked directly rather than inherited
# — vstplug takes them PRIVATE, and renderReference builds the reference chain through both.
target_link_libraries(EntangleHarnessShell PUBLIC
    EntangleHarnessCore
    vstplug
    Engine
    Grain
    FftJuce
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# The allocation tripwire: the header is the interface, and the shim that replaces global operator new is added
# to a target only on request. Calling this twice for one target is a silent no-op — CMake keeps one entry per
# source path, so the build graph does not even change — which is why exactly-once needs no linker check here.
# What stays a duplicate-symbol error is a *second, different* operator new replacement reaching the same binary,
# which is why the light shims under src/ must never link this.
add_library(EntangleAllocWatch INTERFACE)
target_include_directories(EntangleAllocWatch INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

function(entangle_enable_alloc_watch target)
    # CMAKE_CURRENT_FUNCTION_LIST_DIR, not CMAKE_CURRENT_SOURCE_DIR: inside a function the latter is the calling
    # directory, which would look for the shim beside whoever asked for it.
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AllocWatchShim.cpp)
    target_link_libraries(${target} PRIVATE EntangleAllocWatch)
endfunction()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
