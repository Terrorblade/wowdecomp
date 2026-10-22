#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "Engine/Source/Storm/H/storm.h"

// The Windows version looks up TryEnterCriticalSection in kernel32.dll (missing on Win9x). Compat.h always provides
// it here, so there is nothing to load.
void SServerInitialize() {
}

void SServerDestroy() {
}

#endif
