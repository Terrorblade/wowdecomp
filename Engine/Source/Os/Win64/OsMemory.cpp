
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <windows.h>
#include "Engine/Source/Os/OsMemory.h"

static const uintptr_t s_nearRange = 0x60000000;

// HoT: small near blocks come out of shared 64MB arenas. One reservation per block left thousands of 64KB
// regions in the window, every walk crawled through them and the window eventually ran out.
static const uintptr_t s_arenaSize = 0x4000000;
static const size_t s_arenaMaxBlock = s_arenaSize / 4;
static const size_t s_blockAlign = 16;
static const size_t s_blockHeader = 16;
static const size_t s_minSplit = 64;
static const size_t s_decommitMin = 0x10000;

struct NearArena {
    uintptr_t m_base;
    uintptr_t m_end;
    std::map<uintptr_t, size_t> m_freeByAddr;
    std::multimap<size_t, uintptr_t> m_freeBySize;
};

// HoT: model data and everything its 32-bit offsets point at share one region under 2GB, so every offset fits
// by construction and nothing has to be searched for. The arenas only catch what overflows it.
static const uintptr_t s_modelRegionSize = 0x7FFF0000;

static SRWLOCK s_arenaLock = SRWLOCK_INIT;
static std::map<uintptr_t, NearArena*> s_arenas;
static NearArena* s_modelRegion;
static bool s_modelRegionTried;

// HoT: walks resume past the last block handed out, so an allocation no longer scans every live block before it.
static std::atomic<uintptr_t> s_nearHint;

// HoT: only the pages the caller asked for are committed instead of the whole 64KB granule.
static void* AllocAt(uintptr_t candidate, uintptr_t bytes, size_t commit) {
    void* ptr = VirtualAlloc(reinterpret_cast<void*>(candidate), bytes, MEM_RESERVE, PAGE_READWRITE);

    if (!ptr) {
        return nullptr;
    }

    if (commit && !VirtualAlloc(ptr, commit, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(ptr, 0, MEM_RELEASE);
        return nullptr;
    }

    s_nearHint.store(candidate + bytes, std::memory_order_relaxed);
    return ptr;
}

static void* AllocUpward(uintptr_t address, uintptr_t maximum, uintptr_t bytes, size_t commit, uintptr_t granularity) {
    MEMORY_BASIC_INFORMATION region;

    while (address < maximum && VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);
        uintptr_t end = base + region.RegionSize;

        if (region.State == MEM_FREE) {
            uintptr_t candidate = ((base > address ? base : address) + granularity - 1) & ~(granularity - 1);

            if (candidate + bytes <= end && candidate + bytes <= maximum) {
                void* ptr = AllocAt(candidate, bytes, commit);

                if (ptr) {
                    return ptr;
                }
            }
        }

        address = end;
    }

    return nullptr;
}

static void NearWindow(uintptr_t center, uintptr_t& minimum, uintptr_t& maximum, uintptr_t& granularity) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    granularity = info.dwAllocationGranularity;
    minimum = reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
    maximum = reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress);

    if (center > minimum + s_nearRange) {
        minimum = center - s_nearRange;
    }

    if (center + s_nearRange < maximum) {
        maximum = center + s_nearRange;
    }
}

static void* ReserveNear(uintptr_t center, size_t size, size_t commit) {
    uintptr_t minimum;
    uintptr_t maximum;
    uintptr_t granularity;
    NearWindow(center, minimum, maximum, granularity);

    uintptr_t bytes = (size + granularity - 1) & ~(granularity - 1);
    uintptr_t hint = s_nearHint.load(std::memory_order_relaxed);

    if (hint > center && hint < maximum) {
        if (void* ptr = AllocUpward(hint, maximum, bytes, commit, granularity)) {
            return ptr;
        }
    }

    if (void* ptr = AllocUpward(center, maximum, bytes, commit, granularity)) {
        return ptr;
    }

    MEMORY_BASIC_INFORMATION region;
    uintptr_t address = center;

    while (address > minimum && VirtualQuery(reinterpret_cast<void*>(address - 1), &region, sizeof(region))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);
        uintptr_t end = base + region.RegionSize;

        if (end > address) {
            end = address;
        }

        if (region.State == MEM_FREE && end - base >= bytes) {
            uintptr_t candidate = (end - bytes) & ~(granularity - 1);

            if (candidate >= base && candidate >= minimum) {
                void* ptr = AllocAt(candidate, bytes, commit);

                if (ptr) {
                    return ptr;
                }
            }
        }

        address = base;
    }

    return nullptr;
}

static void InsertFree(NearArena* arena, uintptr_t address, size_t size) {
    arena->m_freeByAddr[address] = size;
    arena->m_freeBySize.emplace(size, address);
}

static void EraseFree(NearArena* arena, uintptr_t address, size_t size) {
    arena->m_freeByAddr.erase(address);

    auto range = arena->m_freeBySize.equal_range(size);

    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == address) {
            arena->m_freeBySize.erase(it);
            break;
        }
    }
}

static void* ArenaAlloc(NearArena* arena, size_t need, bool zero) {
    auto fit = arena->m_freeBySize.lower_bound(need);

    if (fit == arena->m_freeBySize.end()) {
        return nullptr;
    }

    uintptr_t address = fit->second;
    size_t size = fit->first;
    EraseFree(arena, address, size);

    if (size - need >= s_minSplit) {
        InsertFree(arena, address + need, size - need);
        size = need;
    }

    if (!VirtualAlloc(reinterpret_cast<void*>(address), size, MEM_COMMIT, PAGE_READWRITE)) {
        InsertFree(arena, address, size);
        return nullptr;
    }

    *reinterpret_cast<size_t*>(address) = size;
    void* ptr = reinterpret_cast<void*>(address + s_blockHeader);

    if (zero) {
        memset(ptr, 0, size - s_blockHeader);
    }

    return ptr;
}

static void ArenaFree(NearArena* arena, uintptr_t address) {
    size_t size = *reinterpret_cast<size_t*>(address);
    uintptr_t start = address;
    uintptr_t end = address + size;

    auto next = arena->m_freeByAddr.find(end);

    if (next != arena->m_freeByAddr.end()) {
        end += next->second;
        EraseFree(arena, next->first, next->second);
    }

    auto prev = arena->m_freeByAddr.lower_bound(start);

    if (prev != arena->m_freeByAddr.begin()) {
        --prev;

        if (prev->first + prev->second == start) {
            start = prev->first;
            EraseFree(arena, prev->first, prev->second);
        }
    }

    InsertFree(arena, start, end - start);

    uintptr_t pageStart = (start + 0xFFF) & ~static_cast<uintptr_t>(0xFFF);
    uintptr_t pageEnd = end & ~static_cast<uintptr_t>(0xFFF);

    if (pageEnd > pageStart && pageEnd - pageStart >= s_decommitMin) {
        VirtualFree(reinterpret_cast<void*>(pageStart), pageEnd - pageStart, MEM_DECOMMIT);
    }
}

static NearArena* ModelRegion() {
    if (!s_modelRegionTried) {
        s_modelRegionTried = true;

        if (void* base = VirtualAlloc(nullptr, s_modelRegionSize, MEM_RESERVE, PAGE_READWRITE)) {
            s_modelRegion = new NearArena();
            s_modelRegion->m_base = reinterpret_cast<uintptr_t>(base);
            s_modelRegion->m_end = s_modelRegion->m_base + s_modelRegionSize;
            InsertFree(s_modelRegion, s_modelRegion->m_base, s_modelRegionSize);
        }
    }

    return s_modelRegion;
}

static bool InRegion(const NearArena* arena, uintptr_t address) {
    return arena && address >= arena->m_base && address < arena->m_end;
}

static size_t BlockNeed(size_t size) {
    return ((size + s_blockAlign - 1) & ~(s_blockAlign - 1)) + s_blockHeader;
}

static NearArena* FindArena(uintptr_t address) {
    if (InRegion(s_modelRegion, address)) {
        return s_modelRegion;
    }

    auto it = s_arenas.upper_bound(address);

    if (it == s_arenas.begin()) {
        return nullptr;
    }

    --it;
    return address < it->second->m_end ? it->second : nullptr;
}

void* OsAllocModel(size_t size) {
    AcquireSRWLockExclusive(&s_arenaLock);

    NearArena* region = ModelRegion();
    void* ptr = region ? ArenaAlloc(region, BlockNeed(size), false) : nullptr;

    ReleaseSRWLockExclusive(&s_arenaLock);

    return ptr;
}

bool OsIsNear(const void* ptr) {
    AcquireSRWLockShared(&s_arenaLock);
    bool owned = FindArena(reinterpret_cast<uintptr_t>(ptr)) != nullptr;
    ReleaseSRWLockShared(&s_arenaLock);

    return owned;
}

void* OsAllocNear(const void* anchor, size_t size) {
    uintptr_t center = reinterpret_cast<uintptr_t>(anchor);
    size_t need = BlockNeed(size);

    AcquireSRWLockExclusive(&s_arenaLock);

    void* ptr = InRegion(s_modelRegion, center) ? ArenaAlloc(s_modelRegion, need, true) : nullptr;

    if (ptr || need > s_arenaMaxBlock) {
        ReleaseSRWLockExclusive(&s_arenaLock);
        return ptr ? ptr : ReserveNear(center, size, size);
    }

    uintptr_t low = center > s_nearRange ? center - s_nearRange : 0;
    uintptr_t high = center + s_nearRange;

    for (auto it = s_arenas.lower_bound(low); it != s_arenas.end() && it->first < high && !ptr; ++it) {
        if (it->second->m_end <= high) {
            ptr = ArenaAlloc(it->second, need, true);
        }
    }

    if (!ptr) {
        if (void* base = ReserveNear(center, s_arenaSize, 0)) {
            NearArena* arena = new NearArena();
            arena->m_base = reinterpret_cast<uintptr_t>(base);
            arena->m_end = arena->m_base + s_arenaSize;
            InsertFree(arena, arena->m_base, s_arenaSize);
            s_arenas[arena->m_base] = arena;
            ptr = ArenaAlloc(arena, need, true);
        }
    }

    ReleaseSRWLockExclusive(&s_arenaLock);

    return ptr;
}

void OsFreeNear(void* ptr) {
    if (!ptr) {
        return;
    }

    uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

    AcquireSRWLockExclusive(&s_arenaLock);

    NearArena* arena = FindArena(address);

    if (arena) {
        ArenaFree(arena, address - s_blockHeader);
    }

    ReleaseSRWLockExclusive(&s_arenaLock);

    if (!arena) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
}

