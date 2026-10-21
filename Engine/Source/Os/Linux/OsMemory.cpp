#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <set>
#include <sys/mman.h>
#include <unistd.h>
#include "Engine/Source/Os/OsMemory.h"

// VirtualQuery/VirtualAlloc become a walk over the gaps between the mappings in /proc/self/maps and
// mmap(MAP_FIXED_NOREPLACE). munmap needs the size VirtualFree does not take, so each block starts with a page that
// records its mapping size; the returned pointer is the byte after that page.

static const uintptr_t s_nearRange = 0x60000000;
static const uintptr_t s_granularity = 0x10000;

static std::mutex s_blockLock;
static std::set<uintptr_t> s_blocks;

static void* MapAt(uintptr_t address, uintptr_t bytes, uintptr_t header) {
    void* ptr = mmap(reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

    if (ptr == MAP_FAILED) {
        return nullptr;
    }

    if (reinterpret_cast<uintptr_t>(ptr) != address) {
        // Kernels before 4.17 treat the flag as a hint.
        munmap(ptr, bytes);
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

    FILE* maps = fopen("/proc/self/maps", "r");

    if (!maps) {
        return nullptr;
    }

    // Free gaps between consecutive mappings that intersect [minimum, maximum).
    struct Gap {
        uintptr_t base;
        uintptr_t end;
    };

    Gap gaps[512];
    uint32_t count = 0;
    uintptr_t previousEnd = 0;
    char line[512];

    while (count < sizeof(gaps) / sizeof(gaps[0]) && fgets(line, sizeof(line), maps)) {
        unsigned long long start;
        unsigned long long end;

        if (sscanf(line, "%llx-%llx", &start, &end) != 2) {
            continue;
        }

        if (start > previousEnd && start > minimum && previousEnd < maximum) {
            gaps[count].base = previousEnd > minimum ? previousEnd : minimum;
            gaps[count].end = start < maximum ? start : maximum;
            count++;
        }

        previousEnd = end;
    }

    fclose(maps);

    if (count < sizeof(gaps) / sizeof(gaps[0]) && previousEnd < maximum) {
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
