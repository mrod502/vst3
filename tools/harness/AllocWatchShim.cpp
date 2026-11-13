#include "AllocWatch.hpp"

#include <cstdlib>
#include <new>

/** The one translation unit of an armed binary that replaces global `operator new`. Added to a target only by
    `entangle_enable_alloc_watch()`, which is what keeps the one-replacement-per-program rule structural: two
    calls means two copies of these definitions, and the linker says so. */
namespace
{
void *allocate(std::size_t size)
{
    if (allocwatch::armed)
        ++allocwatch::allocations;
    void *memory = std::malloc(size != 0 ? size : 1);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}
}

void *operator new(std::size_t size) { return allocate(size); }
void *operator new[](std::size_t size) { return allocate(size); }
void *operator new(std::size_t size, std::align_val_t alignment)
{
    static_cast<void>(alignment);
    return allocate(size);
}
void *operator new[](std::size_t size, std::align_val_t alignment)
{
    static_cast<void>(alignment);
    return allocate(size);
}
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void *memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
