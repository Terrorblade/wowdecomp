#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

namespace {

struct SegmentQuery {
    const struct link_map* m_module;
    OsPosixSegment* m_segments;
    int32_t m_max;
    int32_t m_count;
};

int CollectSegments(struct dl_phdr_info* info, size_t size, void* data) {
    SegmentQuery* query = static_cast<SegmentQuery*>(data);

    // The first object reported is the main program, a library is identified by its load address and name.
    if (query->m_module && (info->dlpi_addr != query->m_module->l_addr || !info->dlpi_name || strcmp(info->dlpi_name, query->m_module->l_name))) {
        return 0;
    }

    for (ElfW(Half) i = 0; i < info->dlpi_phnum && query->m_count < query->m_max; i++) {
        const ElfW(Phdr)& header = info->dlpi_phdr[i];

        if (header.p_type != PT_LOAD || !header.p_memsz) {
            continue;
        }

        OsPosixSegment& segment = query->m_segments[query->m_count++];
        segment.m_start = info->dlpi_addr + header.p_vaddr;
        segment.m_size = header.p_memsz;
        segment.m_writable = (header.p_flags & PF_W) != 0;
        segment.m_executable = (header.p_flags & PF_X) != 0;
    }

    return 1;
}

struct ModuleQuery {
    void (*m_callback)(void*, const char*, uintptr_t, uintptr_t);
    void* m_param;
};

int ReportModule(struct dl_phdr_info* info, size_t size, void* data) {
    ModuleQuery* query = static_cast<ModuleQuery*>(data);
    uintptr_t low = UINTPTR_MAX;
    uintptr_t high = 0;

    for (ElfW(Half) i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)& header = info->dlpi_phdr[i];

        if (header.p_type == PT_LOAD) {
            uintptr_t start = info->dlpi_addr + header.p_vaddr;
            uintptr_t end = start + header.p_memsz;
            low = start < low ? start : low;
            high = end > high ? end : high;
        }
    }

    if (high > low) {
        query->m_callback(query->m_param, info->dlpi_name ? info->dlpi_name : "", low, high);
    }

    return 0;
}

}

int32_t OsPosixModuleSegments(void* module, OsPosixSegment* segments, int32_t maxSegments) {
    SegmentQuery query;
    query.m_module = nullptr;
    query.m_segments = segments;
    query.m_max = maxSegments;
    query.m_count = 0;

    if (module) {
        struct link_map* map = nullptr;

        if (dlinfo(module, RTLD_DI_LINKMAP, &map) || !map) {
            return 0;
        }

        query.m_module = map;
    }

    dl_iterate_phdr(CollectSegments, &query);

    return query.m_count;
}

// The dynamic linker records the path of every dlopen handle.
bool OsPosixModulePath(void* module, char* buffer, size_t size) {
    if (!module) {
        return OsPosixExecutablePath(buffer, size);
    }

    struct link_map* map = nullptr;

    if (dlinfo(module, RTLD_DI_LINKMAP, &map) || !map || !map->l_name || strlen(map->l_name) >= size) {
        return false;
    }

    strcpy(buffer, map->l_name);
    return true;
}

// The main program is reported with an empty path.
void OsPosixEnumerateModules(void (*callback)(void* param, const char* path, uintptr_t low, uintptr_t high), void* param) {
    ModuleQuery query;
    query.m_callback = callback;
    query.m_param = param;
    dl_iterate_phdr(ReportModule, &query);
}
