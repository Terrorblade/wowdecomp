#include "HoT/Runtime/LuaPool.h"
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#else
#include <cstdlib>
#include <sys/mman.h>
#endif

namespace HoT::LuaRuntime {

LuaPool::LuaPool() {
    uint32_t cls = 0;

    for (uint32_t size = 16; size <= 256; size += 16) {
        this->m_classes[cls++] = { nullptr, size };
    }

    for (uint32_t size = 288; size <= 512; size += 32) {
        this->m_classes[cls++] = { nullptr, size };
    }

    for (uint32_t size = 576; size <= 1024; size += 64) {
        this->m_classes[cls++] = { nullptr, size };
    }

    for (uint32_t size = 1152; size <= 2048; size += 128) {
        this->m_classes[cls++] = { nullptr, size };
    }

    uint32_t current = 0;

    for (uint32_t bucket = 0; bucket <= (SMALL_LIMIT >> 4); bucket++) {
        while (this->m_classes[current].m_size < bucket * 16) {
            current++;
        }

        this->m_classOf[bucket] = static_cast<uint8_t>(current);
    }

#if defined(_WIN32)
    this->m_heap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
#endif
}

LuaPool::~LuaPool() {
    for (void* slab : this->m_slabs) {
#if defined(_WIN32)
        VirtualFree(slab, 0, MEM_RELEASE);
#else
        munmap(slab, SLAB_SIZE);
#endif
    }

#if defined(_WIN32)
    if (this->m_heap) {
        HeapDestroy(this->m_heap);
    }
#endif
}

void* LuaPool::Reallocate(void* ptr, size_t osize, size_t nsize) {
    if (nsize == 0) {
        if (ptr && osize) {
            this->Free(ptr, osize);
            this->m_freeCalls++;
            this->m_live -= osize;
        }

        return nullptr;
    }

    this->m_allocCalls++;
    this->m_bytesAllocated += nsize;

    if (!ptr || osize == 0) {
        void* block = this->Allocate(nsize);

        if (block) {
            this->m_live += nsize;
        }

        return block;
    }

    bool oldSmall = osize <= SMALL_LIMIT;
    bool newSmall = nsize <= SMALL_LIMIT;

    if (oldSmall && newSmall && this->ClassOf(osize) == this->ClassOf(nsize)) {
        this->m_live += nsize - osize;
        return ptr;
    }

    if (!oldSmall && !newSmall) {
#if defined(_WIN32)
        void* block = HeapReAlloc(this->m_heap, 0, ptr, nsize);
#else
        void* block = realloc(ptr, nsize);
#endif

        if (block) {
            this->m_live += nsize - osize;
        }

        return block;
    }

    void* block = this->Allocate(nsize);

    if (!block) {
        return nullptr;
    }

    memcpy(block, ptr, osize < nsize ? osize : nsize);
    this->Free(ptr, osize);
    this->m_live += nsize - osize;

    return block;
}

void* LuaPool::Allocate(size_t size) {
    if (size > SMALL_LIMIT) {
#if defined(_WIN32)
        return this->m_heap ? HeapAlloc(this->m_heap, 0, size) : nullptr;
#else
        // Large blocks go to the C heap on Linux, m_heap stays unused.
        return malloc(size);
#endif
    }

    return this->AllocateSmall(this->ClassOf(size));
}

void LuaPool::Free(void* ptr, size_t size) {
    if (size > SMALL_LIMIT) {
#if defined(_WIN32)
        HeapFree(this->m_heap, 0, ptr);
#else
        free(ptr);
#endif
        return;
    }

    SizeClass& cls = this->m_classes[this->ClassOf(size)];
    FreeBlock* block = static_cast<FreeBlock*>(ptr);
    block->m_next = cls.m_free;
    cls.m_free = block;
}

void* LuaPool::AllocateSmall(uint32_t cls) {
    SizeClass& c = this->m_classes[cls];

    if (!c.m_free && !this->Refill(cls)) {
        return nullptr;
    }

    FreeBlock* block = c.m_free;
    c.m_free = block->m_next;

    return block;
}

bool LuaPool::Refill(uint32_t cls) {
#if defined(_WIN32)
    void* slab = VirtualAlloc(nullptr, SLAB_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!slab) {
        return false;
    }
#else
    void* slab = mmap(nullptr, SLAB_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (slab == MAP_FAILED) {
        return false;
    }
#endif

    this->m_slabs.push_back(slab);

    SizeClass& c = this->m_classes[cls];
    size_t count = SLAB_SIZE / c.m_size;
    char* base = static_cast<char*>(slab);
    FreeBlock* head = c.m_free;

    for (size_t i = count; i-- > 0;) {
        FreeBlock* block = reinterpret_cast<FreeBlock*>(base + i * c.m_size);
        block->m_next = head;
        head = block;
    }

    c.m_free = head;

    return true;
}

}
