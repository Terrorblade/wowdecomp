#include "HoT/Platform/Memory.h"
#include <windows.h>

namespace HoT::Platform {

void* AllocatePages(size_t size) {
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void FreePages(void* pages, size_t size) {
    VirtualFree(pages, 0, MEM_RELEASE);
}

void* PrivateHeapCreate() {
    return HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
}

void PrivateHeapDestroy(void* heap) {
    if (heap) {
        HeapDestroy(heap);
    }
}

void* PrivateHeapAlloc(void* heap, size_t size) {
    return heap ? HeapAlloc(heap, 0, size) : nullptr;
}

void* PrivateHeapRealloc(void* heap, void* block, size_t size) {
    return HeapReAlloc(heap, 0, block, size);
}

void PrivateHeapFree(void* heap, void* block) {
    HeapFree(heap, 0, block);
}

}
