set(ADHD_PLUGIN_VERSION v0.0.1)
set(ADHD_PLUGIN_NAME "ADHD Simulator")
configure_file(ADHDSimulator.h.in "${CMAKE_CURRENT_SOURCE_DIR}/ADHDSimulator.h")

juce_add_gui_app(${PROJECT_NAME}
    PRODUCT_NAME "ADHDSimulator")

target_sources(${PROJECT_NAME}
PRIVATE
    MainComponent.cpp
    ADHDSimulator.cpp
    )
target_compile_definitions(${PROJECT_NAME}
PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_VERSION>")

    target_link_libraries(${PROJECT_NAME}
        PRIVATE   
            juce::juce_gui_extra
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
