#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Os/OsVersionHash.h"
#include "Engine/Source/Storm/H/storm.h"

// On Windows the checksum of a loaded module hashes its PE image with relocations undone, import thunks replaced by
// their original values and writable sections replaced by zeroes, so the result depends only on the file. The POSIX
// equivalent hashes the module's loaded segments in address order: read-only segments as they are mapped (a PIE has
// no relocations in them, so the bytes do not depend on the load address) and writable ones as zeroes. The result is
// deterministic for a given build but cannot match any Windows value. Checksums of files on disk are unchanged.

OsVersionHash::~OsVersionHash() {
    if (this->m_entries) {
        SMemFree(this->m_entries, __FILE__, __LINE__, 0);
    }
}

bool OsVersionHash::AddEntry(const OsVersionHashEntry& entry) {
    if (!this->m_entries) {
        return false;
    }

    if (this->m_count >= this->m_capacity) {
        this->m_capacity *= 2;
        this->m_entries = static_cast<OsVersionHashEntry*>(SMemReAlloc(this->m_entries, this->m_capacity * sizeof(OsVersionHashEntry), __FILE__, __LINE__, 0));

        if (!this->m_entries) {
            return false;
        }
    }

    this->m_entries[this->m_count] = entry;
    this->m_count++;

    return true;
}

static void HashZeroes(IOsVersionHashCallback& callback, uint32_t size) {
    uint8_t zeroes[4096];
    memset(zeroes, 0, sizeof(zeroes));

    while (size) {
        uint32_t chunk = size > sizeof(zeroes) ? sizeof(zeroes) : size;
        callback.Process(zeroes, chunk);
        size -= chunk;
    }
}

OsVersionHash::OsVersionHash() : m_entries(nullptr) {
    this->m_entries = static_cast<OsVersionHashEntry*>(SMemAlloc(256 * sizeof(OsVersionHashEntry), __FILE__, __LINE__, 0));
    this->m_count = 0;
    this->m_capacity = 256;
}

static int32_t ChecksumSegments(IOsVersionHashCallback& callback, void* module) {
    OsPosixSegment segments[64];
    int32_t count = OsPosixModuleSegments(module, segments, 64);

    for (int32_t i = 0; i < count; i++) {
        if (segments[i].m_writable) {
            HashZeroes(callback, static_cast<uint32_t>(segments[i].m_size));
            continue;
        }

        const uint8_t* start = reinterpret_cast<const uint8_t*>(segments[i].m_start);
        uint64_t remaining = segments[i].m_size;

        while (remaining) {
            uint32_t chunk = remaining > 0x10000000 ? 0x10000000 : static_cast<uint32_t>(remaining);
            callback.Process(start, chunk);
            start += chunk;
            remaining -= chunk;
        }
    }

    return count > 0;
}

int32_t IOsVersionChecksum(IOsVersionHashCallback& callback, const char* path, int32_t loaded) {
    if (!path && !loaded) {
        return 0;
    }

    if (loaded) {
        void* handle = nullptr;

        // Like GetModuleHandle, this only finds a module that is already loaded.
        if (path) {
            handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);

            if (!handle) {
                return 0;
            }
        }

        int32_t result = ChecksumSegments(callback, handle);

        if (handle) {
            dlclose(handle);
        }

        return result;
    }

    int32_t result = 0;
    void* buffer = nullptr;

    HOSFILE file = OsCreateFile(path, OS_GENERIC_READ, 1, OS_OPEN_EXISTING, OS_FILE_ATTRIBUTE_NORMAL, OS_FILE_TYPE_ANY);

    if (file != OS_INVALID_FILE) {
        uint64_t size = OsGetFileSize(file);

        if (size == static_cast<uint32_t>(size)) {
            buffer = SMemAlloc(static_cast<uint32_t>(size), __FILE__, __LINE__, 0);

            uint32_t bytesRead;
            if (buffer && OsReadFile(file, buffer, static_cast<uint32_t>(size), &bytesRead) && bytesRead == size) {
                callback.Process(buffer, static_cast<uint32_t>(size));
                result = 1;
            }
        }
    }

    if (buffer) {
        SMemFree(buffer, __FILE__, __LINE__, 0);
    }

    if (file != OS_INVALID_FILE) {
        OsCloseFile(file);
    }

    return result;
}

// Linux executables carry no VERSIONINFO resource, so there is never version data (as for a Windows file without
// one): m_data stays empty and the versions zero.
OsFileVersionInfo::OsFileVersionInfo(const char* filename) : m_fileVersion(), m_productVersion() {
    m_data.Clear();
}

#endif
