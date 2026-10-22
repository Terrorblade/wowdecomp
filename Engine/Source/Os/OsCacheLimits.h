#ifndef ENGINE_SOURCE_OS_OSCACHELIMITS_H
#define ENGINE_SOURCE_OS_OSCACHELIMITS_H

#include <cstdint>

// Texture and sound cache sizes, which the original sized for a 32-bit address space.
struct OsCacheLimits {
    uint32_t m_textureReadAhead;
    // 0 keeps the original limit from physical memory.
    uint32_t m_textureCacheMax;
    // textureCacheSize CVar default, in megabytes.
    const char* m_textureCacheDefault;
    // Sound_MaxCacheSizeInBytes CVar default.
    const char* m_soundCacheDefault;
    // Whether a sound cache setting above 100 MB is clamped to 128 MB like the original.
    bool m_soundCacheClamped;
};

const OsCacheLimits& OsGetCacheLimits();

#endif
