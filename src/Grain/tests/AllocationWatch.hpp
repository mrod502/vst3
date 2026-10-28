#pragma once

/** Process-wide allocation counter for the audio-path tests.

    Defined in AllocationWatch.cpp, not in a header: operator new can be replaced only once per
    program and GrainTests links several translation units. */
namespace alloc
{
bool& armed();
long& count();
}

struct AllocationWatch
{
    AllocationWatch()
    {
        alloc::count() = 0;
        alloc::armed() = true;
    }
    ~AllocationWatch() { alloc::armed() = false; }
};
