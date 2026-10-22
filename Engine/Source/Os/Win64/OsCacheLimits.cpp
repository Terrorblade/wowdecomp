#if defined(WOWDECOMP_ARCH_64)

#include "Engine/Source/Os/OsCacheLimits.h"

const OsCacheLimits& OsGetCacheLimits() {
    static const OsCacheLimits s_limits = { 0x400000, 0, "32", "16777216", true };
    return s_limits;
}

#endif
