#include <cstring>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

namespace {

// The image dyld loaded for a dlopen handle, or the main executable for a null module.
int32_t FindImage(void* module) {
    uint32_t count = _dyld_image_count();

    for (uint32_t i = 0; i < count; i++) {
        const mach_header* header = _dyld_get_image_header(i);

        if (!module) {
            if (header && header->filetype == MH_EXECUTE) {
                return static_cast<int32_t>(i);
            }

            continue;
        }

        const char* name = _dyld_get_image_name(i);
        void* handle = name ? dlopen(name, RTLD_LAZY | RTLD_NOLOAD) : nullptr;

        if (handle) {
            dlclose(handle);

            if (handle == module) {
                return static_cast<int32_t>(i);
            }
        }
    }

    return -1;
}

// Calls proc for each mapped segment of an image, __PAGEZERO (no access, no file data) left out.
template <typename Proc>
void ForEachSegment(uint32_t image, Proc proc) {
    const mach_header_64* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(image));

    if (!header || header->magic != MH_MAGIC_64) {
        return;
    }

    intptr_t slide = _dyld_get_image_vmaddr_slide(image);
    const uint8_t* command = reinterpret_cast<const uint8_t*>(header + 1);

    for (uint32_t i = 0; i < header->ncmds; i++) {
        const load_command* load = reinterpret_cast<const load_command*>(command);

        if (load->cmd == LC_SEGMENT_64) {
            const segment_command_64* segment = reinterpret_cast<const segment_command_64*>(load);

            if (segment->vmsize && segment->initprot) {
                proc(static_cast<uintptr_t>(segment->vmaddr + slide), static_cast<uintptr_t>(segment->vmsize), segment->initprot);
            }
        }

        command += load->cmdsize;
    }
}

}

int32_t OsPosixModuleSegments(void* module, OsPosixSegment* segments, int32_t maxSegments) {
    int32_t image = FindImage(module);
    int32_t count = 0;

    if (image < 0) {
        return 0;
    }

    ForEachSegment(static_cast<uint32_t>(image), [&](uintptr_t start, uintptr_t size, int32_t protection) {
        if (count < maxSegments) {
            OsPosixSegment& segment = segments[count++];
            segment.m_start = start;
            segment.m_size = size;
            segment.m_writable = (protection & VM_PROT_WRITE) != 0;
            segment.m_executable = (protection & VM_PROT_EXECUTE) != 0;
        }
    });

    return count;
}

bool OsPosixModulePath(void* module, char* buffer, size_t size) {
    if (!module) {
        return OsPosixExecutablePath(buffer, size);
    }

    int32_t image = FindImage(module);
    const char* name = image >= 0 ? _dyld_get_image_name(static_cast<uint32_t>(image)) : nullptr;

    if (!name || strlen(name) >= size) {
        return false;
    }

    strcpy(buffer, name);
    return true;
}

void OsPosixEnumerateModules(void (*callback)(void* param, const char* path, uintptr_t low, uintptr_t high), void* param) {
    uint32_t count = _dyld_image_count();

    for (uint32_t i = 0; i < count; i++) {
        uintptr_t low = UINTPTR_MAX;
        uintptr_t high = 0;

        ForEachSegment(i, [&](uintptr_t start, uintptr_t size, int32_t protection) {
            low = start < low ? start : low;
            high = start + size > high ? start + size : high;
        });

        const char* name = _dyld_get_image_name(i);

        if (high > low) {
            callback(param, name ? name : "", low, high);
        }
    }
}
