#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Os/Path.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Storm/H/storm.h"

static CommandLineUTF8 s_commandLineUTF8;

// A null module is the executable, any other module is a dlopen handle. Paths are already UTF-8. Returns the
// length written, without the terminator.
uint32_t GetModuleFileNameUTF8(void* module, char* path, uint32_t size) {
    if (!size) {
        return 0;
    }

    char name[4096];

    if (!OsPosixModulePath(module, name, sizeof(name))) {
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

void OsGetGameDirectory(char* path, uint32_t size) {
    if (!size) {
        return;
    }

    char directory[4096];

    if (!OsPosixGameDirectory(directory, sizeof(directory))) {
        OsGetExePath(path, size);
        return;
    }

    SStrCopy(path, directory, size);
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
