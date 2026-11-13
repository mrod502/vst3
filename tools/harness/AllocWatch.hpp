#pragma once

#include <cstddef>

/** The shell-side allocation tripwire: one mechanism, opted into per target, so "a program may replace global
    `operator new` exactly once" is enforced by the linker instead of by a comment.

    The state is inline, so every translation unit of an armed binary sees the same counters and no TU but the
    shim touches `operator new`. `entangle_enable_alloc_watch(<target>)` in tools/harness/CMakeLists.txt appends
    the shim to one target; a binary that never calls it has no replacement at all, and one that calls it twice
    fails at link time with a duplicated symbol — a check the linker already performs, so this file does not
    re-implement it.

    Deliberately *not* merged with the six light copies (`src/Dsp/tests/AllocationWatch.hpp`,
    `src/Grain/tests/AllocationWatch.{hpp,cpp}` and the five Engine binaries' inline versions, plus
    `tools/PrepareFootprint`'s byte counter): those sit in JUCE-light test directories, and pointing them at a
    target that links `vstplug` would hand a JUCE dependency to code whose whole purpose is not having one
    (`01` §0 C). Names match the `Dsp` variant because no binary ever contains both.

    What this can see is allocation. Nothing here observes a lock or a blocking call — `06` states the verdict and
    its limits. */
namespace allocwatch
{
inline bool armed = false;
inline long allocations = 0;

struct Watch
{
    Watch()
    {
        allocations = 0;
        armed = true;
    }
    ~Watch() { armed = false; }
};
}
