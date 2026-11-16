# The skin: the ColourScheme, the font factory, the cap-aligned text helper and the five kit
# components (`02` §3 step 1 — no sixth).
#
# PUBLIC on every JUCE define, for the two reasons FftJuce's comments give (F-009) and which apply
# unchanged here: juce_add_module attaches the module's .cpp as INTERFACE sources, so a consumer of
# UiJuce recompiles those TUs itself and must be told the same shape; and the plugin already builds
# juce_core with curl and the web browser off, so a consumer that was not told ends up with two
# copies of juce_core that disagree and undefined libcurl symbols at the plugin link.
# The design's face, compiled in (D-025). Helvetica Neue ships with macOS and nowhere else and JUCE 9
# resolves no substitute for a family it cannot match (F-022), so a family-name request draws the sheet in
# somebody else's type on every host but one. The two faces the ramp binds to live under public/ and are
# embedded, which is the only route that draws Neue everywhere; see FontFactory.cpp for what that buys and
# costs. Only these two: the ramp declares two weights and a third embedded face is 600 KB of nobody reading it.
juce_add_binary_data(
    EntangleFonts
    NAMESPACE ui
    HEADER_NAME EntangleFonts.h
    SOURCES
        "${CMAKE_SOURCE_DIR}/public/helvetica-neue-5/HelveticaNeueBold.otf"
        "${CMAKE_SOURCE_DIR}/public/helvetica-neue-5/HelveticaNeueThin.otf"
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
