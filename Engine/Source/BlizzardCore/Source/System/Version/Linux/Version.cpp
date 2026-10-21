#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Linux/LinuxPath.h"
#include "Engine/Source/BlizzardCore/Source/System/Version/Version.h"

// There is no version.dll here. The files asked about are Windows PE images from the game folder (e.g. Launcher.exe),
// so the VS_FIXEDFILEINFO block of their version resource is located directly: it starts with the signature
// 0xFEEF04BD followed by structure version 0x00010000, and is 4-byte aligned inside the resource.
struct FixedFileInfo {
    uint32_t dwSignature;
    uint32_t dwStrucVersion;
    uint32_t dwFileVersionMS;
    uint32_t dwFileVersionLS;
    uint32_t dwProductVersionMS;
    uint32_t dwProductVersionLS;
};

static bool ReadFixedFileInfo(const char* path, FixedFileInfo& info) {
    FILE* stream = fopen(path, "rb");

    if (!stream) {
        return false;
    }

    std::vector<uint8_t> data;
    uint8_t chunk[65536];
    size_t count;

    while ((count = fread(chunk, 1, sizeof(chunk), stream)) > 0 && data.size() < 0x4000000) {
        data.insert(data.end(), chunk, chunk + count);
    }

    fclose(stream);

    if (data.size() < 2 || data[0] != 'M' || data[1] != 'Z') {
        return false;
    }

    // Take the last match: version resources live in .rsrc, at the end of the image.
    bool found = false;

    for (size_t offset = 0; offset + sizeof(FixedFileInfo) <= data.size(); offset += 4) {
        uint32_t signature;
        uint32_t version;
        memcpy(&signature, &data[offset], 4);
        memcpy(&version, &data[offset + 4], 4);

        if (signature == 0xFEEF04BD && version == 0x00010000) {
            memcpy(&info, &data[offset], sizeof(info));
            found = true;
        }
    }

    return found;
}

static bool GetVersionInfo(const char* path, Blizzard::Version::FileVersion& version, int32_t type) {
    version.m_major = 0;
    version.m_minor = 0;
    version.m_build = 0;
    version.m_revision = 0;

    if (Blizzard::File::Exists(path) != 1) {
        return false;
    }

    char nativePath[4096];

    if (!System_File::LinuxResolvePath(path, nativePath, sizeof(nativePath), false)) {
        return false;
    }

    FixedFileInfo info;

    if (!ReadFixedFileInfo(nativePath, info)) {
        return false;
    }

    if (type == 0) {
        version.m_major = info.dwProductVersionMS >> 16;
        version.m_minor = info.dwProductVersionMS & 0xFFFF;
        version.m_build = info.dwProductVersionLS >> 16;
        version.m_revision = info.dwProductVersionLS & 0xFFFF;
    } else if (type == 1) {
        version.m_major = info.dwFileVersionMS >> 16;
        version.m_minor = info.dwFileVersionMS & 0xFFFF;
        version.m_build = info.dwFileVersionLS >> 16;
        version.m_revision = info.dwFileVersionLS & 0xFFFF;
    }

    return true;
}

bool Blizzard::Version::GetFileVersion(const char* path, FileVersion& version) {
    return GetVersionInfo(path, version, 1);
}

#endif
