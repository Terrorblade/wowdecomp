#include "HoT/Platform/Memory.h"
#include <cstdlib>
#include <sys/mman.h>

namespace HoT::Platform {

void* AllocatePages(size_t size) {
    void* pages = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return pages == MAP_FAILED ? nullptr : pages;
}

void FreePages(void* pages, size_t size) {
    munmap(pages, size);
}

// There is no private heap here, large blocks go to the C heap.
void* PrivateHeapCreate() {
    return nullptr;
}

void PrivateHeapDestroy(void* heap) {
}

void* PrivateHeapAlloc(void* heap, size_t size) {
    return malloc(size);
}

void* PrivateHeapRealloc(void* heap, void* block, size_t size) {
    return realloc(block, size);
}

void PrivateHeapFree(void* heap, void* block) {
    free(block);
}

}
