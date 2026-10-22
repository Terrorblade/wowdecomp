#include "HoT/MSDF/Os.h"
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// macOS has no MAP_NORESERVE, a PROT_NONE anonymous mapping commits nothing there either.
#if !defined(MAP_NORESERVE)
#define MAP_NORESERVE 0
#endif

namespace HoT::MSDF::Os {

Handle OpenFile(const std::filesystem::path& path, Access access) {
    switch (access) {
        case Access::ReadSequential:
        case Access::ReadRandom:
            return open(path.c_str(), O_RDONLY | O_CLOEXEC);
        case Access::Append:
            return open(path.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
        case Access::Create:
        case Access::CreateSequential:
            return open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    }

    return INVALID_FILE;
}

void CloseFile(Handle file) {
    close(static_cast<int>(file));
}

bool FileSize(Handle file, uint64_t& size) {
    struct stat info;

    if (fstat(static_cast<int>(file), &info) != 0) {
        return false;
    }

    size = static_cast<uint64_t>(info.st_size);
    return true;
}

bool WriteAll(Handle file, const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);

    while (size > 0) {
        ssize_t n = write(static_cast<int>(file), p, size);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        p += n;
        size -= static_cast<size_t>(n);
    }

    return true;
}

void* MapReadOnly(Handle file, size_t size) {
    void* view = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, static_cast<int>(file), 0);
    return view == MAP_FAILED ? nullptr : view;
}

void Unmap(void* view, size_t size) {
    munmap(view, size);
}

size_t AllocationGranularity() {
    static const size_t s_pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    return s_pageSize;
}

bool ArenaSupported() {
    return true;
}

// One PROT_NONE reservation stands in for the Windows placeholders, block files are mapped over a slot with MAP_FIXED.
void* ReserveArena(size_t slotSize, size_t slotCount) {
    void* base = mmap(nullptr, slotSize * slotCount, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    return base == MAP_FAILED ? nullptr : base;
}

void ReleaseArena(void* base, size_t size) {
    munmap(base, size);
}

void ResetSlot(void* slot, size_t slotSize) {
    mmap(slot, slotSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE, -1, 0);
}

// mmap() maps the descriptor directly, there is no section object to keep open.
void* MapIntoSlot(Handle file, void* slot, size_t size, Handle& mapping) {
    void* view = mmap(slot, size, PROT_READ, MAP_SHARED | MAP_FIXED, static_cast<int>(file), 0);
    return view == MAP_FAILED ? nullptr : view;
}

void UnmapFromSlot(void* view, size_t size) {
    ResetSlot(view, size);
}

void CloseMapping(Handle mapping) {
}

}
