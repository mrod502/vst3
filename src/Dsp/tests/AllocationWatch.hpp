#pragma once

#include <cstddef>

/** The allocation tripwire for this test binary: process() must not allocate, and the only honest
    way to prove that here is to count the allocations the process path makes.

    Replacing global operator new is allowed once per program, so the definitions live in exactly one
    translation unit of DspTests and every TU shares the state through these inline variables. */
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
