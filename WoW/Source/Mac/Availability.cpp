#if defined(WOWDECOMP_PLATFORM_MAC)

#include <cstdint>

// @available calls into compiler-rt, which has no Darwin build in a Windows or Linux LLVM. From macOS 11 the check
// is a single libSystem call.

struct dyld_build_version_t {
    uint32_t platform;
    uint32_t version;
};

extern "C" bool _availability_version_check(uint32_t count, dyld_build_version_t versions[]);

extern "C" int32_t __isPlatformVersionAtLeast(uint32_t platform, uint32_t major, uint32_t minor, uint32_t subminor) {
    dyld_build_version_t version = { platform, (major << 16) | ((minor & 0xFF) << 8) | (subminor & 0xFF) };
    return _availability_version_check(1, &version);
}

extern "C" int32_t __isOSVersionAtLeast(int32_t major, int32_t minor, int32_t subminor) {
    return __isPlatformVersionAtLeast(1, static_cast<uint32_t>(major), static_cast<uint32_t>(minor), static_cast<uint32_t>(subminor));
}

#endif
