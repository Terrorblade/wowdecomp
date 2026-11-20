
#include "Engine/Source/Os/OsCacheLimits.h"

const OsCacheLimits& OsGetCacheLimits() {
    static const OsCacheLimits s_limits = { 32 * 1024 * 1024, 0, "32", "16777216", true };
    return s_limits;
}

