#ifndef ENGINE_SOURCE_STORM_SOURCE_MACHINESTATE_H
#define ENGINE_SOURCE_STORM_SOURCE_MACHINESTATE_H

#include <cstdint>
#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(WOWDECOMP_PLATFORM_POSIX)
#include <signal.h>
#include <sys/ucontext.h>
#endif

#define LOG_OPTION_DEFAULT 0x1
#define LOG_OPTION_TITLE 0x2
#define LOG_OPTION_EXEFILE 0x4
#define LOG_OPTION_DATETIME 0x8
#define LOG_OPTION_COMPUTERNAME 0x10
#define LOG_OPTION_USERNAME 0x20
#define LOG_OPTION_REGISTERS 0x10000
#define LOG_OPTION_DBGHELPSTACK 0x20000
#define LOG_OPTION_FRAMEPARAMS 0x40000
#define LOG_OPTION_MANUALSTACK 0x80000
#define LOG_OPTION_CODE 0x100000
#define LOG_OPTION_STACK 0x200000
#define LOG_OPTION_MODULES 0x400000
#define LOG_OPTION_VERBOSE 0x80000000

#if defined(WOWDECOMP_PLATFORM_POSIX)
// Linux stand-ins for SYSTEMTIME, CONTEXT and EXCEPTION_POINTERS: a broken-down local time, the ucontext_t a signal
// handler receives, and the signal's siginfo_t/ucontext_t pair.
struct MACHINESTATETIME {
    /* 0000 */ uint16_t wYear;
    /* 0002 */ uint16_t wMonth;
    /* 0004 */ uint16_t wDayOfWeek;
    /* 0006 */ uint16_t wDay;
    /* 0008 */ uint16_t wHour;
    /* 000A */ uint16_t wMinute;
    /* 000C */ uint16_t wSecond;
    /* 000E */ uint16_t wMilliseconds;
};

struct MACHINESTATEEXCEPTION {
    /* 0000 */ siginfo_t* info;
    /* 0008 */ ucontext_t* context;
};

void GetMachineStateLocalTime(MACHINESTATETIME* time);
void LoadMachineStateSymbols();
void LogComputerInfoHeader(uint32_t flags, void (*callback)(void*, const char*, ...), void* param, const char* title, const MACHINESTATETIME* time);
void LogMachineState(uint32_t flags, void (*callback)(void*, const char*, ...), void* param, uint32_t framestoskip, ucontext_t* context);
int32_t LogMiniDump(void* logfile, MACHINESTATEEXCEPTION* exceptionpointers, uint32_t count, const char** strings);
int32_t LogMiniDumpIsAvailable();
void UnloadMachineStateSymbols();
#else
void LoadMachineStateSymbols();
void LogComputerInfoHeader(uint32_t flags, void (*callback)(void*, const char*, ...), void* param, const char* title, const SYSTEMTIME* time);
void LogMachineState(uint32_t flags, void (*callback)(void*, const char*, ...), void* param, uint32_t framestoskip, CONTEXT* context);
int32_t LogMiniDump(void* logfile, EXCEPTION_POINTERS* exceptionpointers, uint32_t count, const char** strings);
int32_t LogMiniDumpIsAvailable();
void UnloadMachineStateSymbols();
#endif

#endif
