# UiTests is the evidence that the light half is light (spec A5, item 01 criterion 8): it links Ui,
# which is header-only, and EntangleCatch2, and nothing else — so there is no JUCE in its link line to
# disprove. Keep it that way: a JUCE include in src/Ui or in this binary ends the criterion.
add_executable(UiTests
    PaletteTests.cpp
    TypeRampTests.cpp
    TextFitTests.cpp
    GridTests.cpp
    BarGeometryTests.cpp
    PieGeometryTests.cpp
    KitGeometryTests.cpp
    DisplayTraceTests.cpp
    SourcePolicyTests.cpp
    )

target_link_libraries(UiTests PRIVATE Ui EntangleCatch2 EntangleAsserts)

# The policy case reads source text off disk, so it needs the tree's location. A compile definition
# rather than a relative path: the case runs from whatever working directory ctest happens to use.
target_compile_definitions(UiTests PRIVATE
    ENTANGLE_SRC_DIR="${CMAKE_SOURCE_DIR}/src")

catch_discover_tests(UiTests)
