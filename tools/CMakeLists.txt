# Before the targets that consume it: OfflineRender and ShellTests both link the harness, and the harness is what
# holds the 35 shared helpers this program used to define privately (M5 item 01).
add_subdirectory(harness)

juce_add_console_app(OfflineRender PRODUCT_NAME "Entangle Offline Render")

target_sources(OfflineRender PRIVATE OfflineRender.cpp)

target_compile_definitions(OfflineRender PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# vstplug is the plugin's shared code (libEntangle_SharedCode.a): the harness renders through
# the same processor the VST3 bundle loads, instead of recompiling the shell.
#
# EntangleHarnessShell brings the 35 shared helpers (drivers, layouts, generators, metrics, expectations, the WAV
# writer) that this program used to define in its own translation unit, and with them the Core half and the
# src/Entangle include path — see tools/harness. vstplug stays listed explicitly: it is the reason the shell in
# this binary is the shell that ships.
#
# Grain and FftJuce are linked directly, not inherited: vstplug takes them PRIVATE, so §5.1's
# reference chain — the same engine and transform with no shell in it — needs its own route to
# both. src/Entangle is on the path for the processor's header, which names the parameter ids and
# the clamp rule the harness asserts against.
target_link_libraries(OfflineRender PRIVATE
    EntangleHarnessShell
    vstplug
    Engine
    Grain
    FftJuce
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

target_include_directories(OfflineRender PRIVATE ${CMAKE_SOURCE_DIR}/src/Entangle)

if(BUILD_TESTING)
    add_test(NAME OfflineRender COMMAND OfflineRender)
endif()

# The M2 §6.7 CPU baseline. Plain executable rather than juce_add_console_app: it needs the
# transform, not the plugin shell. Deliberately not registered with CTest — wall-clock numbers on a
# shared machine are a measurement to record for M7, not a pass/fail signal.
add_executable(CpuBaseline CpuBaseline.cpp)
target_link_libraries(CpuBaseline PRIVATE Grain Engine Fft FftJuce)

# The M4 item-04 prepare-time footprint instrument: it counts live bytes through a replaced global
# operator new, so it cannot join OfflineRender (which must stay free of that hook) or any of the Engine
# test binaries, and it is deliberately not registered with CTest on CpuBaseline's stated grounds — a
# footprint measured on one host through one transform backend is a figure to record, not a pass/fail
# signal (D-022 C). It links vstplug for createPluginFilter(), so the shell figure is the shell that ships.
juce_add_console_app(PrepareFootprint PRODUCT_NAME "Entangle Prepare Footprint")

target_sources(PrepareFootprint PRIVATE PrepareFootprint.cpp)

target_compile_definitions(PrepareFootprint PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(ENTANGLE_ASSERTS)
    target_compile_definitions(PrepareFootprint PRIVATE ENTANGLE_ASSERTS_CONFIGURED=1)
endif()

target_link_libraries(PrepareFootprint PRIVATE
    vstplug
    Engine
    Grain
    FftJuce
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

target_include_directories(PrepareFootprint PRIVATE ${CMAKE_SOURCE_DIR}/src/Entangle)
