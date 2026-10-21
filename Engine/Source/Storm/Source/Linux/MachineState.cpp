#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cxxabi.h>
#include <dirent.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <link.h>
#include <pwd.h>
#include <sys/uio.h>
#include <unistd.h>
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"
#include "Engine/Source/Storm/Source/MachineState.h"

// Crash-log output in the same layout as the Windows version. DbgHelp's role (symbolised stack walk) is taken by
// backtrace() + dladdr(); minidumps have no Linux equivalent here and report as unavailable.

#define LOG_PTR "%016llX"
#define LOG_PTR_EXTRA 8

struct LOADEDMODULE {
    char name[256];
    void* base;
    uint32_t size;
};

struct MEMDUMP {
    void (*logLineProc)(void*, const char*, ...);
    void* logLineProcParam;
};

struct MODULEENUM {
    LOADEDMODULE* modules;
    size_t count;
    size_t max;
};

static char s_monthNames[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

int32_t g_memFullError;

static LONG sgRecursionLevel;

static LONG sgSymbolsLoadCount;

static inline unsigned long long LogPtr(uintptr_t value) {
    return static_cast<unsigned long long>(value);
}

// IsBadReadPtr: process_vm_readv on our own pid fails with EFAULT instead of faulting.
static bool sReadMemory(const void* address, void* out, size_t size) {
    iovec local = { out, size };
    iovec remote = { const_cast<void*>(address), size };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}

void GetMachineStateLocalTime(MACHINESTATETIME* time) {
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    tm local;
    localtime_r(&now.tv_sec, &local);

    time->wYear = static_cast<uint16_t>(local.tm_year + 1900);
    time->wMonth = static_cast<uint16_t>(local.tm_mon + 1);
    time->wDayOfWeek = static_cast<uint16_t>(local.tm_wday);
    time->wDay = static_cast<uint16_t>(local.tm_mday);
    time->wHour = static_cast<uint16_t>(local.tm_hour);
    time->wMinute = static_cast<uint16_t>(local.tm_min);
    time->wSecond = static_cast<uint16_t>(local.tm_sec);
    time->wMilliseconds = static_cast<uint16_t>(now.tv_nsec / 1000000);
}

static void sLogSeparatorLine(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, char character, int32_t longLine) {
    char line[80];
    uint32_t length = longLine ? 78 : 40;
    memset(line, character, length);
    line[length] = 0;
    logLineProc(logLineProcParam, "%s", line);
}

static void sLogHeader(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, const char* headerString) {
    logLineProc(logLineProcParam, "");
    sLogSeparatorLine(logLineProc, logLineProcParam, '-', 0);
    logLineProc(logLineProcParam, "    %s", headerString);
    sLogSeparatorLine(logLineProc, logLineProcParam, '-', 0);
    logLineProc(logLineProcParam, "");
}

static void sLogMemoryHexDump(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, uint8_t* address, uint32_t numBytes, int32_t alignedLines) {
    char buffer[80 + LOG_PTR_EXTRA];
    uint32_t numLines = numBytes >> 4;
    uint32_t offset = reinterpret_cast<uintptr_t>(address) & 0xF;

    if (alignedLines) {
        memset(buffer, ' ', 78 + LOG_PTR_EXTRA);
        buffer[0] = '*';
        char* marker = &buffer[10 + LOG_PTR_EXTRA + 3 * offset + (offset >> 2)];
        buffer[78 + LOG_PTR_EXTRA] = 0;
        buffer[2] = '=';
        memcpy(&buffer[4], "addr", 4);
        marker[0] = '*';
        marker[1] = '*';
        buffer[62 + LOG_PTR_EXTRA + offset] = '*';
        logLineProc(logLineProcParam, "%s", buffer);

        if (offset) {
            address -= offset;
            ++numLines;
        }
    }

    for (; numLines; --numLines) {
        char* text = buffer + sprintf(buffer, LOG_PTR ": ", LogPtr(reinterpret_cast<uintptr_t>(address)));
        uint8_t bytes[16];

        if (!sReadMemory(address, bytes, sizeof(bytes))) {
            strcpy(text, "<can't read from this address>");
            logLineProc(logLineProcParam, "%s", buffer);
            return;
        }

        text += sprintf(text, "%02X %02X %02X %02X  ", bytes[0], bytes[1], bytes[2], bytes[3]);
        text += sprintf(text, "%02X %02X %02X %02X  ", bytes[4], bytes[5], bytes[6], bytes[7]);
        text += sprintf(text, "%02X %02X %02X %02X  ", bytes[8], bytes[9], bytes[10], bytes[11]);
        text += sprintf(text, "%02X %02X %02X %02X  ", bytes[12], bytes[13], bytes[14], bytes[15]);

        for (uint32_t i = 0; i < 16; ++i) {
            text[i] = isprint(bytes[i]) ? static_cast<char>(bytes[i]) : '.';
        }

        text[16] = 0;
        logLineProc(logLineProcParam, "%s", buffer);
        address += 16;
    }
}

static const char* sGetPathLeaf(const char* path) {
    const char* leaf = nullptr;
    const char* slash = strrchr(path, '/');

    if (slash) {
        leaf = slash;
    }

    const char* backslash = strrchr(path, '\\');

    if (backslash && backslash > leaf) {
        leaf = backslash;
    }

    return leaf ? leaf + 1 : path;
}

static void sLogExeFile(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam) {
    char exeFullPath[MAX_PATH * 4];
    StormLinux::GetExePath(exeFullPath, sizeof(exeFullPath));
    logLineProc(logLineProcParam, "%-10s%s", "Exe:", exeFullPath);
}

static void sLogDateTimeString(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, const MACHINESTATETIME* time) {
    uint16_t hour = time->wHour;
    char ampm = hour < 12 ? 'A' : 'P';

    if (hour > 12) {
        hour -= 12;
    } else if (!hour) {
        hour = 12;
    }

    logLineProc(logLineProcParam, "%-10s%3s %2d, %4d %2d:%02d:%02d.%03d %cM", "Time:", s_monthNames[time->wMonth - 1], time->wDay, time->wYear, hour, time->wMinute, time->wSecond, time->wMilliseconds, ampm);
}

static void sLogUserName(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam) {
    char userName[MAX_PATH];
    userName[0] = 0;

    passwd entry;
    passwd* result = nullptr;
    char buffer[1024];

    if (getpwuid_r(geteuid(), &entry, buffer, sizeof(buffer), &result) == 0 && result && result->pw_name) {
        SStrCopy(userName, result->pw_name, sizeof(userName));
    } else if (getenv("USER")) {
        SStrCopy(userName, getenv("USER"), sizeof(userName));
    }

    if (!userName[0]) {
        strcpy(userName, "<unknown>");
    }

    logLineProc(logLineProcParam, "%-10s%s", "User:", userName);
}

static void sLogComputerName(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam) {
    char computerName[256];

    if (gethostname(computerName, sizeof(computerName)) != 0) {
        strcpy(computerName, "<unknown>");
    }

    computerName[sizeof(computerName) - 1] = 0;
    logLineProc(logLineProcParam, "%-10s%s", "Computer:", computerName);
}

static void sLogMemory(uint32_t logOptions, void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, uint8_t* instructionPointer, uint8_t* stackPointer) {
    sLogHeader(logLineProc, logLineProcParam, "Memory Dump");

    if (logOptions & LOG_OPTION_CODE) {
        logLineProc(logLineProcParam, "Code: %d bytes starting at (EIP = " LOG_PTR ")", 16, LogPtr(reinterpret_cast<uintptr_t>(instructionPointer)));
        logLineProc(logLineProcParam, "");
        sLogMemoryHexDump(logLineProc, logLineProcParam, instructionPointer, 16, 0);
        logLineProc(logLineProcParam, "");
        logLineProc(logLineProcParam, "");
    }

    if (logOptions & LOG_OPTION_STACK) {
        logLineProc(logLineProcParam, "Stack: %d bytes starting at (ESP = " LOG_PTR ")", 1024, LogPtr(reinterpret_cast<uintptr_t>(stackPointer)));
        logLineProc(logLineProcParam, "");
        sLogMemoryHexDump(logLineProc, logLineProcParam, stackPointer, 1024, 1);
        logLineProc(logLineProcParam, "");
        logLineProc(logLineProcParam, "");
    }
}

static int sModuleCompareProc(const void* elem1, const void* elem2) {
    const LOADEDMODULE* module1 = static_cast<const LOADEDMODULE*>(elem1);
    const LOADEDMODULE* module2 = static_cast<const LOADEDMODULE*>(elem2);

    if (module1->base > module2->base) {
        return 1;
    }

    return module1->base < module2->base ? -1 : 0;
}

// Windows reports section:offset inside the PE image; ELF code is reported as offset from the module's load base
// with section 0.
static void sGetLogicalAddress(void* addr, char* moduleName, uint32_t moduleNameSize, uint32_t* section, uint32_t* offset) {
    SStrCopy(moduleName, "<unknown>", moduleNameSize);
    *offset = 0;
    *section = 0;

    Dl_info info;

    if (!dladdr(addr, &info) || !info.dli_fbase) {
        return;
    }

    if (info.dli_fname && *info.dli_fname) {
        SStrCopy(moduleName, info.dli_fname, moduleNameSize);
    } else {
        StormLinux::GetExePath(moduleName, moduleNameSize);
    }

    *offset = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(info.dli_fbase));
}

static void STORMAPI CmdMemOutput(void* hOutput, const MEMREPORTENTRY* entry) {
    MEMDUMP* dump = static_cast<MEMDUMP*>(hOutput);
    dump->logLineProc(dump->logLineProcParam, "%5d %5d %s(%d)", entry->unk_0010, entry->unk_0014, entry->unk_0024, entry->unk_0020 >> 1);
}

static void sShowOutOfMemory(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam) {
    if (!g_memFullError) {
        return;
    }

    MEMDUMP dump;
    dump.logLineProc = logLineProc;
    dump.logLineProcParam = logLineProcParam;
    SMemGenerateReport(SMEM_REPORT_BY_CALLER, CmdMemOutput, &dump);
    sLogSeparatorLine(logLineProc, logLineProcParam, '-', 1);
}

void SMemGenerateReport(SMEMREPORTTYPE type, void (STORMAPI* callback)(void*, const MEMREPORTENTRY*), void* context) {
    StormCallService(STORM_SERVICE_GENERATEREPORT, type, callback, context);
}

static uint32_t sCountThreads() {
    uint32_t count = 0;
    DIR* dir = opendir("/proc/self/task");

    if (dir) {
        while (dirent* entry = readdir(dir)) {
            if (entry->d_name[0] != '.') {
                ++count;
            }
        }

        closedir(dir);
    }

    return count ? count : 1;
}

// Other threads' registers are not reachable without ptrace, so the walks cover the current thread only (the
// Windows code falls back to the same when it cannot open other threads).
static void sLogX86ManualStackTrace(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, uintptr_t registerEip, uintptr_t registerEbp, uint32_t stackFramesToSkip) {
    sLogHeader(logLineProc, logLineProcParam, "Stack Trace (Manual)");
    logLineProc(logLineProcParam, "%s", "Address  Frame    Logical addr  Module");
    logLineProc(logLineProcParam, "");

    uint32_t threadId = SGetCurrentThreadId();
    logLineProc(logLineProcParam, "Showing %d/%d threads...", 1, sCountThreads());
    logLineProc(logLineProcParam, "");
    logLineProc(logLineProcParam, "--- Thread ID: %d [Current Thread] ---", threadId);

    uintptr_t eip = registerEip;
    uintptr_t ebp = registerEbp;

    for (uint32_t frame = 0; frame < 100; ++frame) {
        if (frame >= stackFramesToSkip) {
            char modulePath[MAX_PATH * 4];
            uint32_t section;
            uint32_t offset;
            sGetLogicalAddress(reinterpret_cast<void*>(eip), modulePath, sizeof(modulePath), &section, &offset);
            logLineProc(logLineProcParam, LOG_PTR " " LOG_PTR " %04X:%08X %s", LogPtr(eip), LogPtr(ebp), section, offset, modulePath);
        }

        uintptr_t record[2];

        if (!sReadMemory(reinterpret_cast<void*>(ebp), record, sizeof(record))) {
            break;
        }

        eip = record[1];
        uintptr_t previous = ebp;
        ebp = record[0];

        if (ebp & 3) {
            break;
        }

        if (ebp <= previous) {
            break;
        }

        if (!sReadMemory(reinterpret_cast<void*>(ebp), record, sizeof(record))) {
            break;
        }
    }
}

static void sLogStackFrame(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, uintptr_t address) {
    char moduleName[32];
    char symbolName[256];
    uint32_t symbolDisplacement = 0;

    Dl_info info;
    memset(&info, 0, sizeof(info));

    if (dladdr(reinterpret_cast<void*>(address), &info) && info.dli_fname) {
        SStrCopy(moduleName, sGetPathLeaf(info.dli_fname), sizeof(moduleName));
    } else {
        strcpy(moduleName, "<unknown module>");
    }

    if (info.dli_sname) {
        int status = 0;
        char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
        SStrCopy(symbolName, status == 0 && demangled ? demangled : info.dli_sname, sizeof(symbolName));
        free(demangled);
        symbolDisplacement = static_cast<uint32_t>(address - reinterpret_cast<uintptr_t>(info.dli_saddr));
    } else {
        strcpy(symbolName, "<unknown symbol>");
    }

    logLineProc(logLineProcParam, LOG_PTR " %-12s %s+%d", LogPtr(address), moduleName, symbolName, symbolDisplacement);
}

static void sLogBacktraceStackTrace(uint32_t logOptions, void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, uintptr_t registerEip, uint32_t stackFramesToSkip) {
    sLogHeader(logLineProc, logLineProcParam, "Stack Trace (Using backtrace)");

    logLineProc(logLineProcParam, "Showing %d/%d threads...", 1, sCountThreads());
    logLineProc(logLineProcParam, "");
    logLineProc(logLineProcParam, "--- Thread ID: %d [Current Thread] ---", SGetCurrentThreadId());

    void* frames[128];
    int32_t count = backtrace(frames, 128);

    // Start at registerEip (the faulting instruction, or LogMachineState's caller), which drops the handler and
    // logging frames, then skip what the caller asked for.
    int32_t first = 0;

    for (int32_t i = 0; i < count; i++) {
        if (reinterpret_cast<uintptr_t>(frames[i]) == registerEip) {
            first = i;
            break;
        }
    }

    first += static_cast<int32_t>(stackFramesToSkip);

    if (count <= first && (logOptions & LOG_OPTION_VERBOSE)) {
        logLineProc(logLineProcParam, "**** backtrace() returned %d frames", count);
    }

    for (int32_t i = first; i < count && i - first < 100; i++) {
        sLogStackFrame(logLineProc, logLineProcParam, reinterpret_cast<uintptr_t>(frames[i]));
    }

    logLineProc(logLineProcParam, "");
}

static void sLogX86ContextRegisters(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, const ucontext_t* context) {
    const greg_t* gregs = context->uc_mcontext.gregs;
    uint64_t csgsfs = static_cast<uint64_t>(gregs[REG_CSGSFS]);

    sLogHeader(logLineProc, logLineProcParam, "x64 Registers");
    logLineProc(logLineProcParam, "RAX=%016llX  RBX=%016llX  RCX=%016llX  RDX=%016llX", LogPtr(gregs[REG_RAX]), LogPtr(gregs[REG_RBX]), LogPtr(gregs[REG_RCX]), LogPtr(gregs[REG_RDX]));
    logLineProc(logLineProcParam, "RSI=%016llX  RDI=%016llX  RBP=%016llX  RSP=%016llX", LogPtr(gregs[REG_RSI]), LogPtr(gregs[REG_RDI]), LogPtr(gregs[REG_RBP]), LogPtr(gregs[REG_RSP]));
    logLineProc(logLineProcParam, "R8 =%016llX  R9 =%016llX  R10=%016llX  R11=%016llX", LogPtr(gregs[REG_R8]), LogPtr(gregs[REG_R9]), LogPtr(gregs[REG_R10]), LogPtr(gregs[REG_R11]));
    logLineProc(logLineProcParam, "R12=%016llX  R13=%016llX  R14=%016llX  R15=%016llX", LogPtr(gregs[REG_R12]), LogPtr(gregs[REG_R13]), LogPtr(gregs[REG_R14]), LogPtr(gregs[REG_R15]));
    logLineProc(logLineProcParam, "RIP=%016llX  FLG=%08X", LogPtr(gregs[REG_RIP]), static_cast<uint32_t>(gregs[REG_EFL]));
    logLineProc(logLineProcParam, "CS =%04X      GS =%04X      FS =%04X      ERR=%08X  TRP=%08X", static_cast<uint32_t>(csgsfs & 0xFFFF), static_cast<uint32_t>((csgsfs >> 16) & 0xFFFF), static_cast<uint32_t>((csgsfs >> 32) & 0xFFFF), static_cast<uint32_t>(gregs[REG_ERR]), static_cast<uint32_t>(gregs[REG_TRAPNO]));
    logLineProc(logLineProcParam, "");
}

void LoadMachineStateSymbols() {
    InterlockedIncrement(&sgSymbolsLoadCount);
}

void UnloadMachineStateSymbols() {
    InterlockedDecrement(&sgSymbolsLoadCount);
}

void LogComputerInfoHeader(uint32_t logOptions, void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, const char* headerTitleLine, const MACHINESTATETIME* time) {
    if (InterlockedIncrement(&sgRecursionLevel) == 1) {
        if (!logOptions || (logOptions & LOG_OPTION_DEFAULT)) {
            logOptions |= LOG_OPTION_EXEFILE | LOG_OPTION_DATETIME | LOG_OPTION_COMPUTERNAME | LOG_OPTION_USERNAME;

            if (headerTitleLine && *headerTitleLine) {
                logOptions |= LOG_OPTION_TITLE;
            }
        }

        sLogSeparatorLine(logLineProc, logLineProcParam, '=', 1);

        if ((logOptions & LOG_OPTION_TITLE) && headerTitleLine && *headerTitleLine) {
            logLineProc(logLineProcParam, "%s", headerTitleLine);
            logLineProc(logLineProcParam, "");
        }

        if (logOptions & LOG_OPTION_EXEFILE) {
            sLogExeFile(logLineProc, logLineProcParam);
        }

        if (logOptions & LOG_OPTION_DATETIME) {
            MACHINESTATETIME currentTime;

            if (!time) {
                GetMachineStateLocalTime(&currentTime);
                time = &currentTime;
            }

            sLogDateTimeString(logLineProc, logLineProcParam, time);
        }

        if (logOptions & LOG_OPTION_USERNAME) {
            sLogUserName(logLineProc, logLineProcParam);
        }

        if (logOptions & LOG_OPTION_COMPUTERNAME) {
            sLogComputerName(logLineProc, logLineProcParam);
        }

        sLogSeparatorLine(logLineProc, logLineProcParam, '-', 1);
    }

    InterlockedDecrement(&sgRecursionLevel);
}

int32_t LogMiniDump(void* logfile, MACHINESTATEEXCEPTION* exceptionpointers, uint32_t count, const char** strings) {
    return 0;
}

int32_t LogMiniDumpIsAvailable() {
    return 0;
}

static int sCollectModule(dl_phdr_info* info, size_t size, void* data) {
    MODULEENUM* modules = static_cast<MODULEENUM*>(data);

    if (modules->count == modules->max) {
        return 1;
    }

    uintptr_t low = UINTPTR_MAX;
    uintptr_t high = 0;

    for (uint32_t i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];

        if (phdr->p_type == PT_LOAD) {
            uintptr_t start = info->dlpi_addr + phdr->p_vaddr;
            uintptr_t end = start + phdr->p_memsz;
            low = start < low ? start : low;
            high = end > high ? end : high;
        }
    }

    if (high <= low) {
        return 0;
    }

    LOADEDMODULE* module = &modules->modules[modules->count];
    module->base = reinterpret_cast<void*>(low);
    module->size = static_cast<uint32_t>(high - low);

    if (info->dlpi_name && *info->dlpi_name) {
        SStrCopy(module->name, sGetPathLeaf(info->dlpi_name), sizeof(module->name));
    } else {
        char exePath[MAX_PATH * 4];
        StormLinux::GetExePath(exePath, sizeof(exePath));
        SStrCopy(module->name, sGetPathLeaf(exePath), sizeof(module->name));
    }

    if (!module->name[0]) {
        return 0;
    }

    modules->count++;
    return 0;
}

// The Windows line carries the PDB name/GUID/age from the CodeView record; ELF modules have none of those, so
// they are logged empty.
static void sLogModuleLine(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, void* base, const char* name, uint32_t size) {
    logLineProc(logLineProcParam, "DBG-MODULE<%08llX %08lX \"%s\" \"%s\" %lu {%08lx-%04hx-%04hx-%02x%02x%02x%02x%02x%02x%02x%02x} %lu %lu%s>", LogPtr(reinterpret_cast<uintptr_t>(base)), static_cast<unsigned long>(size), name, "", 0ul, 0ul, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0ul, 0ul, "");
}

static void sLogLoadedModules(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam) {
    static LOADEDMODULE modules[256];

    logLineProc(logLineProcParam, "");
    sLogHeader(logLineProc, logLineProcParam, "Loaded Modules");

    MODULEENUM enumeration;
    enumeration.modules = modules;
    enumeration.count = 0;
    enumeration.max = 256;
    dl_iterate_phdr(sCollectModule, &enumeration);

    qsort(modules, enumeration.count, sizeof(LOADEDMODULE), sModuleCompareProc);

    for (size_t i = 0; i < enumeration.count; ++i) {
        sLogModuleLine(logLineProc, logLineProcParam, modules[i].base, modules[i].name, modules[i].size);
    }

    if (!enumeration.count) {
        logLineProc(logLineProcParam, "****  dl_iterate_phdr couldn't enumerate modules");
    }
}

void LogMachineState(uint32_t logOptions, void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, uint32_t stackFramesToSkip, ucontext_t* context) {
    if (InterlockedIncrement(&sgRecursionLevel) == 1) {
        if (!logOptions || (logOptions & LOG_OPTION_DEFAULT)) {
            logOptions |= LOG_OPTION_DBGHELPSTACK | LOG_OPTION_MANUALSTACK | LOG_OPTION_STACK | LOG_OPTION_MODULES;

            if (context) {
                logOptions |= LOG_OPTION_REGISTERS | LOG_OPTION_CODE;
            }
        }

        uintptr_t registerEip;
        uintptr_t registerEbp;
        uintptr_t registerEsp;

        if (context) {
            registerEip = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RIP]);
            registerEbp = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RBP]);
            registerEsp = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RSP]);
            stackFramesToSkip = 0;
        } else {
            // Windows captures its own context and skips one more frame; starting at the caller's frame skips
            // LogMachineState already.
            uintptr_t* frame = static_cast<uintptr_t*>(__builtin_frame_address(0));
            registerEip = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
            registerEbp = frame[0];
            registerEsp = reinterpret_cast<uintptr_t>(frame);
            logOptions &= ~static_cast<uint32_t>(LOG_OPTION_REGISTERS | LOG_OPTION_CODE);
        }

        sLogSeparatorLine(logLineProc, logLineProcParam, '-', 1);

        if ((logOptions & LOG_OPTION_REGISTERS) && context) {
            sLogX86ContextRegisters(logLineProc, logLineProcParam, context);
        }

        if (logOptions & LOG_OPTION_MANUALSTACK) {
            sLogX86ManualStackTrace(logLineProc, logLineProcParam, registerEip, registerEbp, stackFramesToSkip);
        }

        if (logOptions & LOG_OPTION_DBGHELPSTACK) {
            sLogBacktraceStackTrace(logOptions, logLineProc, logLineProcParam, registerEip, stackFramesToSkip);
        }

        if (logOptions & LOG_OPTION_MODULES) {
            sLogLoadedModules(logLineProc, logLineProcParam);
        }

        if (logOptions & (LOG_OPTION_CODE | LOG_OPTION_STACK)) {
            sLogMemory(logOptions, logLineProc, logLineProcParam, reinterpret_cast<uint8_t*>(registerEip), reinterpret_cast<uint8_t*>(registerEsp));
        }

        sLogSeparatorLine(logLineProc, logLineProcParam, '-', 1);
        sShowOutOfMemory(logLineProc, logLineProcParam);
    }

    InterlockedDecrement(&sgRecursionLevel);
}

#endif
