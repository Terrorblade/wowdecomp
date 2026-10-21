#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Os/OsVersionHash.h"
#include "Engine/Source/Storm/H/storm.h"

// On Windows the checksum of a loaded module hashes its PE image with relocations undone, import thunks replaced by
// their original values and writable sections replaced by zeroes, so the result depends only on the file. The Linux
// equivalent hashes the module's PT_LOAD segments in address order: read-only segments as they are mapped (a PIE has
// no relocations in them, so the bytes do not depend on the load address) and writable ones as zeroes. The result is
// deterministic for a given build; it cannot match any Windows value. Checksums of files on disk are unchanged.

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

struct ModuleChecksum {
    IOsVersionHashCallback* m_callback;
    const struct link_map* m_module;
    int32_t m_result;
};

static int ChecksumSegments(struct dl_phdr_info* info, size_t size, void* data) {
    ModuleChecksum* checksum = static_cast<ModuleChecksum*>(data);

    // The first object reported is the main program; a library is identified by its load address.
    if (checksum->m_module && (info->dlpi_addr != checksum->m_module->l_addr || !info->dlpi_name || strcmp(info->dlpi_name, checksum->m_module->l_name))) {
        return 0;
    }

    for (ElfW(Half) i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)& header = info->dlpi_phdr[i];

        if (header.p_type != PT_LOAD || !header.p_memsz) {
            continue;
        }

        if (header.p_flags & PF_W) {
            HashZeroes(*checksum->m_callback, static_cast<uint32_t>(header.p_memsz));
            continue;
        }

        const uint8_t* start = reinterpret_cast<const uint8_t*>(info->dlpi_addr + header.p_vaddr);
        uint64_t remaining = header.p_memsz;

        while (remaining) {
            uint32_t chunk = remaining > 0x10000000 ? 0x10000000 : static_cast<uint32_t>(remaining);
            checksum->m_callback->Process(start, chunk);
            start += chunk;
            remaining -= chunk;
        }
    }

    checksum->m_result = 1;

    return 1;
}

int32_t IOsVersionChecksum(IOsVersionHashCallback& callback, const char* path, int32_t loaded) {
    if (!path && !loaded) {
        return 0;
    }

    if (loaded) {
        ModuleChecksum checksum;
        checksum.m_callback = &callback;
        checksum.m_module = nullptr;
        checksum.m_result = 0;

        void* handle = nullptr;

        // Like GetModuleHandle, this only finds a module that is already loaded.
        if (path) {
            handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);

            if (!handle) {
                return 0;
            }

            struct link_map* map = nullptr;

            if (dlinfo(handle, RTLD_DI_LINKMAP, &map) || !map) {
                dlclose(handle);
                return 0;
            }

            checksum.m_module = map;
        }

        dl_iterate_phdr(ChecksumSegments, &checksum);

        if (handle) {
            dlclose(handle);
        }

        return checksum.m_result;
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
