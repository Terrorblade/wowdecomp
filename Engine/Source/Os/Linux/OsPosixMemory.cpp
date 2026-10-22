#include <cstdio>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

void* OsPosixMapAt(uintptr_t address, size_t bytes) {
    void* ptr = mmap(reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

    if (ptr == MAP_FAILED) {
        return nullptr;
    }

    if (reinterpret_cast<uintptr_t>(ptr) != address) {
        // Kernels before 4.17 treat the flag as a hint.
        munmap(ptr, bytes);
        return nullptr;
    }

    return ptr;
}

void OsPosixEnumerateMappings(bool (*callback)(void* param, uintptr_t start, uintptr_t end), void* param) {
    FILE* maps = fopen("/proc/self/maps", "r");

    if (!maps) {
        return;
    }

    char line[512];

    while (fgets(line, sizeof(line), maps)) {
        unsigned long long start;
        unsigned long long end;

        if (sscanf(line, "%llx-%llx", &start, &end) != 2) {
            continue;
        }

        if (!callback(param, static_cast<uintptr_t>(start), static_cast<uintptr_t>(end))) {
            break;
        }
    }

    fclose(maps);
}

// process_vm_readv on our own pid fails with EFAULT instead of faulting.
bool OsPosixReadMemory(const void* address, void* out, size_t size) {
    iovec local = { out, size };
    iovec remote = { const_cast<void*>(address), size };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}
