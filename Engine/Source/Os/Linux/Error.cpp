#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/Os/Error.h"
#include "Engine/Source/Os/Linux/OsPosix.h"

// The Windows version formats GetLastError(), errno is the POSIX equivalent. The string is malloc'd and released by
// OsFreeLastErrorStr, as FormatMessage's buffer is released with LocalFree.
char* OsGetLastErrorStr() {
    int error = errno;
    char buffer[256];
    const char* message = OsPosixErrorString(error, buffer, sizeof(buffer));

    return strdup(message ? message : "Unknown error");
}

void OsFreeLastErrorStr(char* str) {
    free(str);
}

#endif
