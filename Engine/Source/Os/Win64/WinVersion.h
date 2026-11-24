#ifndef ENGINE_SOURCE_OS_WIN64_WINVERSION_H
#define ENGINE_SOURCE_OS_WIN64_WINVERSION_H

#include <cstring>
#include <windows.h>

inline bool OsGetWindowsVersion(OSVERSIONINFOEXW* info) {
    typedef LONG(WINAPI* RtlGetVersionFn)(OSVERSIONINFOEXW*);
    static RtlGetVersionFn rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));

    memset(info, 0, sizeof(*info));
    info->dwOSVersionInfoSize = sizeof(*info);

    return rtlGetVersion && rtlGetVersion(info) == 0;
}

#endif
