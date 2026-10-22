#ifndef ENGINE_SOURCE_OS_SYSTEM_H
#define ENGINE_SOURCE_OS_SYSTEM_H

#include <cstdint>

enum OsType {
    OsType_Unknown = 0,
    OsType_Win95 = 1,
    OsType_Win95OSR2 = 2,
    OsType_Win98 = 3,
    OsType_Win98SE = 4,
    OsType_WinME = 5,
    OsType_WinNT4 = 6,
    OsType_Win2000 = 7,
    OsType_WinXP = 8,
    OsType_Win2003 = 9,
    OsType_Win9X_Other = 10,
    OsType_WinNT_Other = 11,
    OsType_MacOS9 = 12,
    OsType_MacOSX = 13,
    OsType_Linux = 14,
    OsType_WinVista = 15,
    OsType_Last = 16,
};

struct CpuIdInfo {
    /* 0000 */ char m_vendor[12];
    /* 000C */ uint32_t m_maxStandard;
    /* 0010 */ uint32_t m_standard1b;
    /* 0014 */ uint32_t m_standard1d;
    /* 0018 */ uint32_t m_standard4a;
    /* 001C */ uint32_t m_maxExtended;
    /* 0020 */ uint32_t m_extended1c;
    /* 0024 */ uint32_t m_extended1d;
    /* 0028 */ uint32_t m_extended8c;
    /* 002C */ char m_brand[48];
};

void* CreateNamedEventA(const char* name, int32_t manualReset);
int32_t IOsGetPowerProfFrequency();
int32_t IOsGetProcessorFeatures(CpuIdInfo* info);
int32_t IOsParseProcessorFrequency(const char* brand);
void IOsSystemCpuLog(const CpuIdInfo* info);
void OsFindTextSectionBounds();
uint32_t OsGetBackgroundSleepMs();
int32_t OsGetComputerName(char* buffer, uint32_t* size);
uint32_t OsGetNumberOfProcessors();
uint32_t OsGetOsId();
uint64_t OsGetPhysicalMemory();
uint32_t OsGetProcessorId();
int32_t OsGetUserName(char* buffer, uint32_t* size);
OsType OsGetVersion();
void OsGetVersionString(char* buffer, int32_t size);
int32_t OsIsRemoteSession();
int32_t OsLaunchURL(const char* url);
void OsSetBackgroundSleepMs(uint32_t milliseconds);
void OsSetSleepInBackground(int32_t sleep);
int32_t OsSleepInBackground();
void OsSystemEnableCpuLog();
void OsValidateFunctionPointer(const void* ptr);

#if defined(WOWDECOMP_PLATFORM_POSIX)
// Linux has no GetCommandLine; main hands argv to the Os layer.
const char* OsGetCommandLine();
void OsSetCommandLine(int argc, char** argv);
#endif

#endif
