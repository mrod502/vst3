# ShellTests is the first Catch2 program in this repo that drives the shell: it takes a real processor from
# createPluginFilter(), sets APVTS choices, applies a setBusesLayout and renders through processBlock.
#
# It compiles Catch2's own amalgamated TU rather than linking EntangleCatch2, for one reason: that library ships
# an unconditional main (catch_amalgamated.cpp:4777), and this binary has to own its startup — the GUI
# initialiser that claims this thread as the message thread (F-018), the Logger filter that keeps the shell's
# layout report out of the run's output, the teardown order Logger::~Logger asserts on, and the --dump-wavs
# argument, which has to leave argv before Catch2 turns it into a usage error. CATCH_AMALGAMATED_CUSTOM_MAIN is
# the framework's own switch for exactly this, and one binary still contains exactly one Catch2.
add_executable(ShellTests
    ShellTests.cpp
    ${entangle_catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp)

target_compile_definitions(ShellTests PRIVATE CATCH_AMALGAMATED_CUSTOM_MAIN=1)
target_include_directories(ShellTests PRIVATE ${entangle_catch2_SOURCE_DIR}/extras)
target_link_libraries(ShellTests PRIVATE EntangleHarnessShell)

# Armed here and nowhere else in this tree: OfflineRender never calls the function, so no operator new
# replacement exists in it, and criterion 8's unarmed half is that binary's successful link rather than a script
# asserting an absence.
entangle_enable_alloc_watch(ShellTests)

catch_discover_tests(ShellTests)

# CrossProductTests is M5 item 02's half of the same shape: the twelve gate-accepted layouts against the whole
# callback plan set, at L = 1024 and at the shipped grain length. It is a second binary rather than more cases in
# ShellTests because the two halves of the cross product land in two different run sets (item 02 decision A: the
# long arms carry the "cross product long: " prefix and the quick set excludes it), and a case name — not a file,
# and not a target name — is the only filter this repo has.
#
# Same Catch2 wiring as above for the same reasons: the binary owns its startup (the message thread per F-018 and
# the Logger teardown order), so it compiles the amalgamated source with CATCH_AMALGAMATED_CUSTOM_MAIN rather than
# linking EntangleCatch2's unconditional main. One Catch2 per program.
#
# No entangle_enable_alloc_watch here: item 02 makes no allocation claim — the hop-path verdict is item 06's, and
# the tripwire's controls are ShellTests'.
add_executable(CrossProductTests
    CrossProductTests.cpp
    ${entangle_catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp)

target_compile_definitions(CrossProductTests PRIVATE CATCH_AMALGAMATED_CUSTOM_MAIN=1)
target_include_directories(CrossProductTests PRIVATE ${entangle_catch2_SOURCE_DIR}/extras)
target_link_libraries(CrossProductTests PRIVATE EntangleHarnessShell)

catch_discover_tests(CrossProductTests)
