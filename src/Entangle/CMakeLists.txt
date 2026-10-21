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

target_sources(${PROJECT_NAME}
PRIVATE
    EntangleEditor.cpp
    EntangleProcessor.cpp)

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_VERSION>")

target_link_libraries(${PROJECT_NAME}
    PRIVATE   
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        )
