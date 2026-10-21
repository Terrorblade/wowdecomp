#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/Os/Error.h"

// The Windows version formats GetLastError(); errno is the Linux equivalent. The string is malloc'd and released by
// OsFreeLastErrorStr, as FormatMessage's buffer is released with LocalFree.
char* OsGetLastErrorStr() {
    int error = errno;
    char buffer[256];
    const char* message = strerror_r(error, buffer, sizeof(buffer));

    return strdup(message ? message : "Unknown error");
}

void OsFreeLastErrorStr(char* str) {
    free(str);
}

#endif
