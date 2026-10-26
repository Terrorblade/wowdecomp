#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "Engine/Source/Os/OsAutorelease.h"

void* OsAutoreleasePush() {
    return nullptr;
}

void OsAutoreleasePop(void* pool) {
}

#endif
