# The theme's JUCE-light half: five tokens, the seven-role ramp, the 4 × 3 grid.
#
# INTERFACE rather than a compiled library: all three headers are data and arithmetic, so there is no
# out-of-line body to move out of them (item 01 §3 step 1). The cost is that nothing but the test
# binary compiles these headers, which is why UiTests is not optional coverage of them.
#
# INTERFACE keyword, not PUBLIC: CMake rejects target_include_directories(Ui PUBLIC …) on an
# INTERFACE target with "may only set INTERFACE properties on INTERFACE targets", measured 2026-11-13
# with a two-line project. item 01 §3 step 1's PUBLIC spelling is the Fft pattern, which is a
# compiled library, and does not transfer.
#
# No EntangleAsserts link: nothing in these three headers carries an assert() tripwire, so the
# -UNDEBUG switch would be linkage nobody uses (AGENTS.md §5, no unused dependencies).
add_library(Ui INTERFACE)

target_include_directories(Ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
