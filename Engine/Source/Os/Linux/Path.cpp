#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include "Engine/Source/Os/Path.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Storm/H/storm.h"

static CommandLineUTF8 s_commandLineUTF8;

// A null module is the executable (/proc/self/exe); any other module is a dlopen handle, whose path the dynamic
// linker records. Paths are already UTF-8. Returns the length written, without the terminator.
uint32_t GetModuleFileNameUTF8(void* module, char* path, uint32_t size) {
    if (!size) {
        return 0;
    }

    char buffer[4096];
    const char* name = nullptr;

    if (!module) {
        ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

        if (length > 0) {
            buffer[length] = '\0';
            name = buffer;
        }
    } else {
        struct link_map* map = nullptr;

        if (!dlinfo(module, RTLD_DI_LINKMAP, &map) && map && map->l_name) {
            name = map->l_name;
        }
    }

    if (!name) {
        path[0] = '\0';
        return 0;
    }

    uint32_t written = static_cast<uint32_t>(strlen(name));

    if (size - 1 >= written) {
        memcpy(path, name, written);
        path[written] = '\0';
    } else {
        memcpy(path, name, size - 1);
        path[size - 1] = '\0';
    }

    return written;
}

CommandLineUTF8::CommandLineUTF8() {
    const char* commandLine = OsGetCommandLine();

    if (!commandLine) {
        commandLine = "";
    }

    uint32_t size = static_cast<uint32_t>(strlen(commandLine) + 1);
    m_commandLine = static_cast<char*>(SMemAlloc(size, __FILE__, __LINE__, 0));
    memcpy(m_commandLine, commandLine, size);
}

CommandLineUTF8::~CommandLineUTF8() {
    delete[] m_commandLine;
}

#endif
