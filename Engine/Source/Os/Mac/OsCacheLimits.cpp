#include "Engine/Source/Os/OsCacheLimits.h"

// The macOS client reads 64 MB of textures at once, lets the texture cache grow to 2 GB (1 GB by default) and keeps
// 256 MB of sounds with no upper clamp.
const OsCacheLimits& OsGetCacheLimits() {
    static const OsCacheLimits s_limits = { 64 * 1024 * 1024, 2047u * 1024 * 1024, "1024", "268435456", false };
    return s_limits;
}
