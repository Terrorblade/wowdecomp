#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <set>
#include <sys/mman.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Os/OsMemory.h"

// VirtualQuery/VirtualAlloc become a walk over the gaps between the process mappings and an mmap at an exact
// address. munmap needs the size VirtualFree does not take, so each block starts with a page that
// records its mapping size; the returned pointer is the byte after that page.

static const uintptr_t s_nearRange = 0x60000000;
static const uintptr_t s_granularity = 0x10000;

static std::mutex s_blockLock;
static std::set<uintptr_t> s_blocks;

// Free gaps between consecutive mappings that intersect [minimum, maximum).
struct Gap {
    uintptr_t base;
    uintptr_t end;
};

struct GapSearch {
    Gap gaps[512];
    uint32_t count;
    uintptr_t previousEnd;
    uintptr_t minimum;
    uintptr_t maximum;
};

static bool CollectGap(void* param, uintptr_t start, uintptr_t end) {
    GapSearch* search = static_cast<GapSearch*>(param);

    if (start > search->previousEnd && start > search->minimum && search->previousEnd < search->maximum) {
        search->gaps[search->count].base = search->previousEnd > search->minimum ? search->previousEnd : search->minimum;
        search->gaps[search->count].end = start < search->maximum ? start : search->maximum;
        search->count++;
    }

    search->previousEnd = end;

    return search->count < sizeof(search->gaps) / sizeof(search->gaps[0]);
}

static void* MapAt(uintptr_t address, uintptr_t bytes, uintptr_t header) {
    void* ptr = OsPosixMapAt(address, bytes);

    if (!ptr) {
        return nullptr;
    }

    *static_cast<uintptr_t*>(ptr) = bytes;

    uint8_t* block = static_cast<uint8_t*>(ptr) + header;

    std::lock_guard<std::mutex> lock(s_blockLock);
    s_blocks.insert(reinterpret_cast<uintptr_t>(block));

    return block;
}

// No shared model region here yet, so model data takes the caller's A16 heap fallback as before HoT added it.
void* OsAllocModel(size_t size) {
    (void)size;
    return nullptr;
}

bool OsIsNear(const void* ptr) {
    std::lock_guard<std::mutex> lock(s_blockLock);
    return s_blocks.count(reinterpret_cast<uintptr_t>(ptr)) != 0;
}

void* OsAllocNear(const void* anchor, size_t size) {
    uintptr_t header = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    uintptr_t bytes = (size + header + s_granularity - 1) & ~(s_granularity - 1);
    uintptr_t center = reinterpret_cast<uintptr_t>(anchor);
    uintptr_t minimum = s_granularity;
    uintptr_t maximum = 0x00007FFFFFFF0000;

    if (center > minimum + s_nearRange) {
        minimum = center - s_nearRange;
    }

    if (center + s_nearRange < maximum) {
        maximum = center + s_nearRange;
    }

    GapSearch search;
    search.count = 0;
    search.previousEnd = 0;
    search.minimum = minimum;
    search.maximum = maximum;
    OsPosixEnumerateMappings(CollectGap, &search);

    Gap* gaps = search.gaps;
    uint32_t count = search.count;
    uintptr_t previousEnd = search.previousEnd;

    if (count < sizeof(search.gaps) / sizeof(search.gaps[0]) && previousEnd < maximum) {
        gaps[count].base = previousEnd > minimum ? previousEnd : minimum;
        gaps[count].end = maximum;
        count++;
    }

    // Upwards from the anchor first, then downwards, as the Windows search does.
    for (uint32_t i = 0; i < count; i++) {
        if (gaps[i].end <= center) {
            continue;
        }

        uintptr_t base = gaps[i].base > center ? gaps[i].base : center;
        uintptr_t candidate = (base + s_granularity - 1) & ~(s_granularity - 1);

        if (candidate + bytes <= gaps[i].end) {
            void* ptr = MapAt(candidate, bytes, header);

            if (ptr) {
                return ptr;
            }
        }
    }

    for (uint32_t i = count; i > 0; i--) {
        const Gap& gap = gaps[i - 1];

        if (gap.base >= center) {
            continue;
        }

        uintptr_t end = gap.end < center ? gap.end : center;

        if (end - gap.base < bytes) {
            continue;
        }

        uintptr_t candidate = (end - bytes) & ~(s_granularity - 1);

        if (candidate >= gap.base) {
            void* ptr = MapAt(candidate, bytes, header);

            if (ptr) {
                return ptr;
            }
        }
    }

    return nullptr;
}

void OsFreeNear(void* ptr) {
    if (ptr) {
        {
            std::lock_guard<std::mutex> lock(s_blockLock);
            s_blocks.erase(reinterpret_cast<uintptr_t>(ptr));
        }

        uintptr_t header = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
        uint8_t* base = static_cast<uint8_t*>(ptr) - header;
        munmap(base, *reinterpret_cast<uintptr_t*>(base));
    }
}

#endif
