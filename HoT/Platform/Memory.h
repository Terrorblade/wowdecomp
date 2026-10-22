#ifndef HOT_PLATFORM_MEMORY_H
#define HOT_PLATFORM_MEMORY_H

#include <cstddef>

namespace HoT::Platform {

// Committed read write pages straight from the OS, nullptr on failure.
void* AllocatePages(size_t size);
void FreePages(void* pages, size_t size);

// A private unserialized heap where the OS has one, otherwise nullptr and the calls below use the C heap.
void* PrivateHeapCreate();
void PrivateHeapDestroy(void* heap);
void* PrivateHeapAlloc(void* heap, size_t size);
void* PrivateHeapRealloc(void* heap, void* block, size_t size);
void PrivateHeapFree(void* heap, void* block);

}

#endif
