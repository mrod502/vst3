# The skin: the ColourScheme, the font factory, the cap-aligned text helper and the five kit
# components (`02` §3 step 1 — no sixth).
#
# PUBLIC on every JUCE define, for the two reasons FftJuce's comments give (F-009) and which apply
# unchanged here: juce_add_module attaches the module's .cpp as INTERFACE sources, so a consumer of
# UiJuce recompiles those TUs itself and must be told the same shape; and the plugin already builds
# juce_core with curl and the web browser off, so a consumer that was not told ends up with two
# copies of juce_core that disagree and undefined libcurl symbols at the plugin link.

# The two faces the ramp binds, taken unmodified from the foundry's drop, which is tracked: a third weight is one
# more line here and ~26 kB (D-037's byte table). FFL §02 and §05 make subsetting, format conversion, renaming and
# metadata edits a Derivative Work, so each face ships whole, as it arrived, licence text beside it in the drop.
juce_add_binary_data(
    EntangleFonts
    NAMESPACE ui
    HEADER_NAME EntangleFonts.h
    SOURCES
        "${CMAKE_SOURCE_DIR}/public/ClashDisplay_Complete/Fonts/OTF/ClashDisplay-Light.otf"
        "${CMAKE_SOURCE_DIR}/public/ClashDisplay_Complete/Fonts/OTF/ClashDisplay-Bold.otf"
    )

add_library(
    UiJuce
    Theme.cpp
    FontFactory.cpp
    Panel.cpp
    Bar.cpp
    ChoiceBar.cpp
    PieDial.cpp
    Text.cpp
    ResponsePanel.cpp
    EnvelopePanel.cpp
    )

target_include_directories(UiJuce PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    UiJuce
    PUBLIC
        Ui
        EntangleFonts
        juce::juce_gui_basics
    )

target_compile_definitions(UiJuce PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
