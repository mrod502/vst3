set(ENTANGLE_PLUGIN_VERSION v0.0.1)
set(ENTANGLE_PLUGIN_NAME "ENTANGLE Simulator")
set(JUCE_COPY_PLUGIN_AFTER_BUILD true)

configure_file(Entangle.h.in "${CMAKE_CURRENT_SOURCE_DIR}/Entangle.h")

juce_add_plugin(
    ${PROJECT_NAME}
    PLUGIN_MANUFACTURER_CODE mrod
    PLUGIN_CODE entangle
    FORMATS VST3 Standalone
    PRODUCT_NAME "Entangle"
)

# JUCE seals the bundle in checkBundleSigning.cmake before it generates moduleinfo.json, so the JSON is
# left unsealed and hosts that validate the seal (Ableton Live: "a sealed resource is missing or invalid")
# reject the bundle. POST_BUILD steps run in definition order, so this re-signs once the bundle is final.
if(APPLE) # TARGET_BUNDLE_DIR is Bundle-only; a Linux VST3 is a shared lib, so unguarded this fails configure there.
add_custom_command(TARGET ${PROJECT_NAME}_VST3 POST_BUILD
    COMMAND /usr/bin/codesign --force --sign - $<TARGET_BUNDLE_DIR:${PROJECT_NAME}_VST3>
    VERBATIM)
endif()

target_sources(${PROJECT_NAME}
PRIVATE
    EntangleEditor.cpp
    EntangleProcessor.cpp
    SheetLayout.cpp)

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_VERSION>")

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Engine
        # Grain for the Mode 1 engine and its TransformFactory, FftJuce for the transform the
        # shell injects at that seam. Fft arrives transitively: the processor names no Fft header.
        Grain
        FftJuce
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_audio_utils
    PUBLIC
        # PUBLIC because the editor's header names the kit types, so anything that includes
        # EntangleEditor.h — OfflineRender §5.22 — needs the same include paths to read it. Ui
        # arrives transitively, the way Fft does above.
        UiJuce
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        )
