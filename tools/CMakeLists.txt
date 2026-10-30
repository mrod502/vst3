juce_add_console_app(OfflineRender PRODUCT_NAME "Entangle Offline Render")

target_sources(OfflineRender PRIVATE OfflineRender.cpp)

target_compile_definitions(OfflineRender PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# vstplug is the plugin's shared code (libEntangle_SharedCode.a): the harness renders through
# the same processor the VST3 bundle loads, instead of recompiling the shell.
#
# Grain and FftJuce are linked directly, not inherited: vstplug takes them PRIVATE, so §5.1's
# reference chain — the same engine and transform with no shell in it — needs its own route to
# both. src/Entangle is on the path for the processor's header, which names the parameter ids and
# the clamp rule the harness asserts against.
target_link_libraries(OfflineRender PRIVATE
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
