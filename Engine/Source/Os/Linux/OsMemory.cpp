#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <utility>
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

// HoT: the next block is tried right after the last one before the mappings are walked again, as on Windows.
static std::atomic<uintptr_t> s_nearHint;

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
    s_nearHint.store(address + bytes, std::memory_order_relaxed);

    uint8_t* block = static_cast<uint8_t*>(ptr) + header;

    std::lock_guard<std::mutex> lock(s_blockLock);
    s_blocks.insert(reinterpret_cast<uintptr_t>(block));

    return block;
}

// Model data and its near blocks share one 2GB region as on Windows, since macOS does not honor mmap hints next to the heap.
static const uintptr_t s_modelRegionSize = 0x7FFF0000;
static const size_t s_blockAlign = 16;
static const size_t s_blockHeader = 16;
static const size_t s_minSplit = 64;
static const size_t s_discardMin = 0x10000;

struct ModelRegion {
    uintptr_t m_base;
    uintptr_t m_end;
    std::map<uintptr_t, size_t> m_freeByAddr;
    std::set<std::pair<size_t, uintptr_t>> m_freeBySize;
};

static ModelRegion* s_modelRegion;
static bool s_modelRegionTried;

static void InsertFree(ModelRegion* region, uintptr_t address, size_t size) {
    region->m_freeByAddr[address] = size;
    region->m_freeBySize.emplace(size, address);
}

static void EraseFree(ModelRegion* region, uintptr_t address, size_t size) {
    region->m_freeByAddr.erase(address);
    region->m_freeBySize.erase(std::make_pair(size, address));
}

// Pages are only backed once touched.
static ModelRegion* GetModelRegion() {
    if (!s_modelRegionTried) {
        s_modelRegionTried = true;

        void* base = mmap(nullptr, s_modelRegionSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);

        if (base != MAP_FAILED) {
            s_modelRegion = new ModelRegion();
            s_modelRegion->m_base = reinterpret_cast<uintptr_t>(base);
            s_modelRegion->m_end = s_modelRegion->m_base + s_modelRegionSize;
            InsertFree(s_modelRegion, s_modelRegion->m_base, s_modelRegionSize);
        }
    }

    return s_modelRegion;
}

static bool InModelRegion(uintptr_t address) {
    return s_modelRegion && address >= s_modelRegion->m_base && address < s_modelRegion->m_end;
}

static size_t BlockNeed(size_t size) {
    return ((size + s_blockAlign - 1) & ~(s_blockAlign - 1)) + s_blockHeader;
}

static void* RegionAlloc(ModelRegion* region, size_t need, bool zero) {
    auto fit = region->m_freeBySize.lower_bound(std::make_pair(need, static_cast<uintptr_t>(0)));

    if (fit == region->m_freeBySize.end()) {
        return nullptr;
    }

    uintptr_t address = fit->second;
    size_t size = fit->first;
    EraseFree(region, address, size);

    if (size - need >= s_minSplit) {
        InsertFree(region, address + need, size - need);
        size = need;
    }

    *reinterpret_cast<size_t*>(address) = size;
    void* ptr = reinterpret_cast<void*>(address + s_blockHeader);

    if (zero) {
        memset(ptr, 0, size - s_blockHeader);
    }

    return ptr;
}

// Mapping fresh pages over freed space returns it to the system and keeps the reservation.
static void RegionFree(ModelRegion* region, uintptr_t address) {
    size_t size = *reinterpret_cast<size_t*>(address);
    uintptr_t start = address;
    uintptr_t end = address + size;

    auto next = region->m_freeByAddr.find(end);

    if (next != region->m_freeByAddr.end()) {
        end += next->second;
        EraseFree(region, next->first, next->second);
    }

    auto prev = region->m_freeByAddr.lower_bound(start);

    if (prev != region->m_freeByAddr.begin()) {
        --prev;

        if (prev->first + prev->second == start) {
            start = prev->first;
            EraseFree(region, prev->first, prev->second);
        }
    }

    InsertFree(region, start, end - start);

    uintptr_t page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    uintptr_t pageStart = (start + page - 1) & ~(page - 1);
    uintptr_t pageEnd = end & ~(page - 1);

    if (pageEnd > pageStart && pageEnd - pageStart >= s_discardMin) {
        mmap(reinterpret_cast<void*>(pageStart), pageEnd - pageStart, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE | MAP_FIXED, -1, 0);
    }
}

void* OsAllocModel(size_t size) {
    std::lock_guard<std::mutex> lock(s_blockLock);
    ModelRegion* region = GetModelRegion();
    return region ? RegionAlloc(region, BlockNeed(size), false) : nullptr;
}

bool OsIsNear(const void* ptr) {
    std::lock_guard<std::mutex> lock(s_blockLock);
    uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
    return InModelRegion(address) || s_blocks.count(address) != 0;
}

void* OsAllocNear(const void* anchor, size_t size) {
    {
        std::lock_guard<std::mutex> lock(s_blockLock);

        if (InModelRegion(reinterpret_cast<uintptr_t>(anchor))) {
            if (void* ptr = RegionAlloc(s_modelRegion, BlockNeed(size), true)) {
                return ptr;
            }
        }
    }

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

    uintptr_t hint = s_nearHint.load(std::memory_order_relaxed);

    if (hint > center && hint < maximum && maximum - hint >= bytes) {
        if (void* ptr = MapAt(hint, bytes, header)) {
            return ptr;
        }
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
            uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

            if (InModelRegion(address)) {
                RegionFree(s_modelRegion, address - s_blockHeader);
                return;
            }

            s_blocks.erase(address);
        }

        uintptr_t header = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
        uint8_t* base = static_cast<uint8_t*>(ptr) - header;
        munmap(base, *reinterpret_cast<uintptr_t*>(base));
    }
}

#endif
