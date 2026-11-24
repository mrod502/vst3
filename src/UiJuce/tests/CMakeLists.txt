# UiJuceTests is the skin's half of the Grain/GrainEngineJuceTests pair: the same subject measured on
# the side that needs JUCE, in its own binary so the light suite stays light (spec A5).
#
# It owns its main() and therefore compiles Catch2's amalgamated source with the framework's own
# custom-main switch, rather than linking EntangleCatch2 — that library ships an unconditional main,
# and this binary needs a ScopedJuceInitialiser_GUI before any Image, Typeface or GlyphCache exists.
# Offscreen rendering only: there is no display here, so no ComponentPeer (INDEX §4). One Catch2 per
# program still holds.
add_executable(UiJuceTests
    UiJuceTests.cpp
    ${entangle_catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp)

target_compile_definitions(UiJuceTests PRIVATE CATCH_AMALGAMATED_CUSTOM_MAIN=1)
# The criterion-7 leg reads Theme.hpp's own text to prove the two inherited slot setters are shadowed
# private — a requires-expression cannot ask about access, so the mechanism is what the leg can see.
target_compile_definitions(UiJuceTests PRIVATE ENTANGLE_SRC_DIR="${CMAKE_SOURCE_DIR}/src")
target_include_directories(UiJuceTests PRIVATE ${entangle_catch2_SOURCE_DIR}/extras
    # The sfnt reader behind the face legs: header-only, no JUCE, and the only route to nameID 6 (F-030).
    ${CMAKE_SOURCE_DIR}/tools/harness)
target_link_libraries(UiJuceTests PRIVATE UiJuce)

catch_discover_tests(UiJuceTests)
