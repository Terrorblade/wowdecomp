
#include <cstdint>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/Unicode/Unicode.h"
#include "Engine/Source/BlizzardCore/Source/System/Version/Version.h"

typedef DWORD (WINAPI* GETFILEVERSIONINFOSIZEAPROC)(LPCSTR filename, LPDWORD handle);
typedef BOOL (WINAPI* GETFILEVERSIONINFOAPROC)(LPCSTR filename, DWORD handle, DWORD size, LPVOID data);
typedef BOOL (WINAPI* VERQUERYVALUEAPROC)(LPCVOID block, LPCSTR subBlock, LPVOID* buffer, PUINT length);

// OFFSET: 0x00429380
DWORD OsGetFileVersionInfoSizeA(LPCSTR filename, LPDWORD handle) {
    HMODULE module = LoadLibraryA("version.dll");

    if (!module) {
        return 0;
    }

    DWORD result = 0;
    GETFILEVERSIONINFOSIZEAPROC proc = reinterpret_cast<GETFILEVERSIONINFOSIZEAPROC>(GetProcAddress(module, "GetFileVersionInfoSizeA"));

    if (proc) {
        result = proc(filename, handle);
    } else {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    }

    FreeLibrary(module);
    return result;
}

// OFFSET: 0x004293E0
BOOL OsGetFileVersionInfoA(LPCSTR filename, DWORD handle, DWORD size, LPVOID data) {
    HMODULE module = LoadLibraryA("version.dll");

    if (!module) {
        return FALSE;
    }

    BOOL result = FALSE;
    GETFILEVERSIONINFOAPROC proc = reinterpret_cast<GETFILEVERSIONINFOAPROC>(GetProcAddress(module, "GetFileVersionInfoA"));

    if (proc) {
        result = proc(filename, handle, size, data);
    } else {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    }

    FreeLibrary(module);
    return result;
}

// OFFSET: 0x00429450
BOOL OsVerQueryValueA(LPCVOID block, LPCSTR subBlock, LPVOID* buffer, PUINT length) {
    HMODULE module = LoadLibraryA("version.dll");

    if (!module) {
        return FALSE;
    }

    BOOL result = FALSE;
    VERQUERYVALUEAPROC proc = reinterpret_cast<VERQUERYVALUEAPROC>(GetProcAddress(module, "VerQueryValueA"));

    if (proc) {
        result = proc(block, subBlock, buffer, length);
    } else {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    }

    FreeLibrary(module);
    return result;
}

// OFFSET: 0x004338F0
static bool GetVersionInfo(const char* path, Blizzard::Version::FileVersion& version, int32_t type) {
    version.m_major = 0;
    version.m_minor = 0;
    version.m_build = 0;
    version.m_revision = 0;

    if (Blizzard::File::Exists(path) != 1) {
        return false;
    }

    Blizzard::StringConversion_CodepageToUTF8::ToNative nativePath(path);

    DWORD handle;
    DWORD size = OsGetFileVersionInfoSizeA(nativePath.m_data, &handle);
    uint8_t* data = nullptr;

    if (size) {
        data = new uint8_t[size];
    }

    if (!OsGetFileVersionInfoA(nativePath.m_data, 0, size, data)) {
        if (data) {
            delete[] data;
        }

        return false;
    }

    VS_FIXEDFILEINFO* info;
    UINT length;

    if (!OsVerQueryValueA(data, "\\", reinterpret_cast<LPVOID*>(&info), &length) || length < sizeof(VS_FIXEDFILEINFO)) {
        if (data) {
            delete[] data;
        }

        return false;
    }

    if (type == 0) {
        version.m_major = HIWORD(info->dwProductVersionMS);
        version.m_minor = LOWORD(info->dwProductVersionMS);
        version.m_build = HIWORD(info->dwProductVersionLS);
        version.m_revision = LOWORD(info->dwProductVersionLS);
    } else if (type == 1) {
        version.m_major = HIWORD(info->dwFileVersionMS);
        version.m_minor = LOWORD(info->dwFileVersionMS);
        version.m_build = HIWORD(info->dwFileVersionLS);
        version.m_revision = LOWORD(info->dwFileVersionLS);
    }

    if (data) {
        delete[] data;
    }

    return true;
}

// OFFSET: 0x00433A90
bool Blizzard::Version::GetFileVersion(const char* path, FileVersion& version) {
    return GetVersionInfo(path, version, 1);
}

