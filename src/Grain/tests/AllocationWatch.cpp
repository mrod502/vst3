#include "AllocationWatch.hpp"

#include <cstdlib>
#include <new>

namespace alloc
{
bool& armed()
{
    static bool value = false;
    return value;
}

long& count()
{
    static long value = 0;
    return value;
}
}

namespace
{
void* allocate(std::size_t size)
{
    if (alloc::armed())
        ++alloc::count();
    void* memory = std::malloc(size != 0 ? size : 1);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}
}

void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void* operator new(std::size_t size, std::align_val_t alignment)
{
    static_cast<void>(alignment);
    return allocate(size);
}
void* operator new[](std::size_t size, std::align_val_t alignment)
{
    static_cast<void>(alignment);
    return allocate(size);
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
