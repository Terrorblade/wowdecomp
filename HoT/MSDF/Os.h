#ifndef HOT_MSDF_OS_H
#define HOT_MSDF_OS_H

#include <cstddef>
#include <cstdint>
#include <filesystem>

// File handles, mappings and the reserved slot arena of the MSDF glyph cache.
namespace HoT::MSDF::Os {

using Handle = intptr_t;

static const Handle INVALID_FILE = -1;

enum class Access {
    ReadSequential,
    ReadRandom,
    Append,
    Create,
    CreateSequential,
};

Handle OpenFile(const std::filesystem::path& path, Access access);
void CloseFile(Handle file);
bool FileSize(Handle file, uint64_t& size);
bool WriteAll(Handle file, const void* data, size_t size);

// A read only view of the first size bytes of the file, nullptr on failure.
void* MapReadOnly(Handle file, size_t size);
void Unmap(void* view, size_t size);

size_t AllocationGranularity();

// Whether an address range can be reserved and block files mapped into it in place.
bool ArenaSupported();

// Reserves slotCount slots of slotSize bytes back to back, nullptr on failure.
void* ReserveArena(size_t slotSize, size_t slotCount);
void ReleaseArena(void* base, size_t size);

// Drops whatever is mapped in the slot and reserves it again.
void ResetSlot(void* slot, size_t slotSize);

// Maps size bytes of the file read only at the start of a reserved slot, mapping receives what must stay open with it.
void* MapIntoSlot(Handle file, void* slot, size_t size, Handle& mapping);

// Unmaps a view from MapIntoSlot and puts the reservation back.
void UnmapFromSlot(void* view, size_t size);
void CloseMapping(Handle mapping);

}

#endif
