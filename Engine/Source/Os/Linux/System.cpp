#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pwd.h>
#include <spawn.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Os/Processor.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"

extern char** environ;

static int32_t s_sleepInBackground = 1;

static const char* s_osTypeNames[] = { "Unknown", "Win95", "Win95OSR2", "Win98", "Win98SE", "WinME", "WinNT4", "Win2000", "WinXP", "Win2003", "Win9X_Other", "WinNT_Other", "MacOS9", "MacOSX", "Linux", "WinVista" };

static const uint8_t s_hexDigitValues[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static int32_t s_haveProcessorFeatures;
static int32_t s_processorVendor;
static uint32_t s_processorFeatures;
static int32_t s_processorSockets;
static int32_t s_processorCores;
static int32_t s_processorCount;
static uintptr_t s_textStart;
static uintptr_t s_textEnd;
static int32_t s_cpuLogEnabled;
static uint32_t s_backgroundSleepMs;
static uint64_t s_cpuTicksPerSecond;
static int32_t s_gotPowerProfFrequency;
static int32_t s_checkedPowerProfInfo;
static char* s_commandLine;

static const char s_vendorIds[] = "GenuineIntel" "AuthenticAMD" "CyrixInstead" "CentaurHauls";

// Appends one argument the way a Windows command line carries it: quoted when it contains whitespace (or is empty).
// SStrTokenize does not unescape, so embedded quotes are left as they are.
static void AppendArgument(char*& dst, const char* arg) {
    bool quote = !*arg || strpbrk(arg, " \t\n\r");

    if (quote) {
        *dst++ = '"';
    }

    size_t length = strlen(arg);
    memcpy(dst, arg, length);
    dst += length;

    if (quote) {
        *dst++ = '"';
    }
}

void OsSetCommandLine(int argc, char** argv) {
    size_t size = 1;

    for (int i = 0; i < argc; i++) {
        size += strlen(argv[i]) + 3;
    }

    char* commandLine = static_cast<char*>(malloc(size));
    char* dst = commandLine;

    for (int i = 0; i < argc; i++) {
        if (i) {
            *dst++ = ' ';
        }

        AppendArgument(dst, argv[i]);
    }

    *dst = '\0';

    free(s_commandLine);
    s_commandLine = commandLine;
}

// Without OsSetCommandLine (e.g. before main), the arguments come from the process itself.
const char* OsGetCommandLine() {
    if (s_commandLine) {
        return s_commandLine;
    }

    static char buffer[32768];
    char* argv[1024];
    int32_t argc = OsPosixProcessArguments(argv, 1024, buffer, sizeof(buffer));

    if (!argc) {
        return "";
    }

    OsSetCommandLine(argc, argv);

    return s_commandLine;
}

int32_t IOsGetProcessorFeatures(CpuIdInfo* info) {
    memset(info, 0, sizeof(CpuIdInfo));

    int32_t result = 0;

    // The EFLAGS.ID probe of the Windows code is unnecessary, every x86_64 processor has cpuid and arm64 has a stand-in.
    uint32_t regs[4];

    if (OsPosixCpuid(0, 0, regs) && regs[0]) {
        uint32_t maxStandard = regs[0];
        info->m_maxStandard = regs[0];
        memcpy(&info->m_vendor[0], &regs[1], 4);
        memcpy(&info->m_vendor[4], &regs[3], 4);
        memcpy(&info->m_vendor[8], &regs[2], 4);
        result = 1;

        if (maxStandard >= 4) {
            OsPosixCpuid(4, 0, regs);
            info->m_standard4a = regs[0];
        }

        OsPosixCpuid(1, 0, regs);
        info->m_standard1d = regs[3];
        info->m_standard1b = regs[1];

        OsPosixCpuid(0x80000000, 0, regs);

        if (regs[0] > 0x80000000) {
            result = 2;
            uint32_t maxExtended = regs[0];
            info->m_maxExtended = regs[0];

            if (maxExtended >= 0x80000008) {
                OsPosixCpuid(0x80000008, 0, regs);
                info->m_extended8c = regs[2];
            }

            if (maxExtended >= 0x80000002) {
                OsPosixCpuid(0x80000002, 0, regs);
                memcpy(&info->m_brand[0], regs, 16);
            }

            if (maxExtended >= 0x80000003) {
                OsPosixCpuid(0x80000003, 0, regs);
                memcpy(&info->m_brand[16], regs, 16);
            }

            if (maxExtended >= 0x80000004) {
                OsPosixCpuid(0x80000004, 0, regs);
                memcpy(&info->m_brand[32], regs, 16);
            }

            OsPosixCpuid(0x80000001, 0, regs);
            info->m_extended1d = regs[3];
            info->m_extended1c = regs[2];
        }
    }

    return result;
}

void OsSystemEnableCpuLog() {
    s_cpuLogEnabled = 1;
}

void IOsSystemCpuLog(const CpuIdInfo* info) {
    if (!s_cpuLogEnabled) {
        return;
    }

    OsCreateDirectory("Logs", 0);

    HSLOG log;
    SLogCreate("Logs\\cpu.log", 0, &log);

    if (s_processorVendor == 4) {
        SLogWrite(log, "UNABLE TO IDENTIFY CPU");
        SLogClose(log);
        return;
    }

    SLogWrite(log, "vendor: %d", s_processorVendor);
    SLogWrite(log, "features: %08X", s_processorFeatures & 0x7FFFFFFF);
    SLogWrite(log, "sockets: %d", s_processorSockets);
    SLogWrite(log, "cores: %d", s_processorCores);
    SLogWrite(log, "processors: %d", s_processorCount);

    char vendor[16];
    strncpy(vendor, info->m_vendor, 12);
    vendor[12] = '\0';
    SLogWrite(log, "vendor id string= %s", vendor);

    SLogWrite(log, "standard (%d): 1b=%08X 1d=%08x 4a=%08X", info->m_maxStandard, info->m_standard1b, info->m_standard1d, info->m_standard4a);
    SLogWrite(log, "extended (%d): 1c=%08X 1d=%08x 8c=%08X", info->m_maxExtended & 0x7FFFFFFF, info->m_extended1c, info->m_extended1d, info->m_extended8c);

    const char* brand = info->m_brand;

    while (*brand == ' ') {
        brand++;
    }

    SLogWrite(log, "processor brand string= %s", brand);
    SLogClose(log);
}

uint32_t OsGetNumberOfProcessors() {
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    if (count <= 0) {
        return 1;
    }

    return static_cast<uint32_t>(count);
}

int32_t OsSleepInBackground() {
    return s_sleepInBackground;
}

void OsSetSleepInBackground(int32_t sleep) {
    s_sleepInBackground = sleep;
}

uint32_t OsGetBackgroundSleepMs() {
    return s_backgroundSleepMs;
}

void OsSetBackgroundSleepMs(uint32_t milliseconds) {
    s_backgroundSleepMs = milliseconds;
}

OsType OsGetVersion() {
    return static_cast<OsType>(OsPosixOsType());
}

// The type name, as on Windows, followed by the kernel release and machine ("Linux 6.12.0 x86_64").
void OsGetVersionString(char* buffer, int32_t size) {
    const char* name = s_osTypeNames[OsGetVersion()];

    if (!name) {
        name = s_osTypeNames[0];
    }

    struct utsname info;

    if (uname(&info)) {
        SStrCopy(buffer, name, size);
        return;
    }

    SStrPrintf(buffer, size, "%s %s %s", name, info.release, info.machine);
}

// The login protocol tags the client as "x86" / "Win"; the Linux build keeps them so servers treat it as the Windows
// client it is ported from.
uint32_t OsGetProcessorId() {
    return 0x783836;
}

uint32_t OsGetOsId() {
    return 0x57696E;
}

// GetComputerNameA: on success *size is the length without the terminator; on a short buffer it fails with the
// required size (including the terminator) in *size.
int32_t OsGetComputerName(char* buffer, uint32_t* size) {
    char name[256];

    if (gethostname(name, sizeof(name))) {
        return 0;
    }

    name[sizeof(name) - 1] = '\0';

    // Like the NetBIOS name, only the part before the first dot.
    char* dot = strchr(name, '.');

    if (dot) {
        *dot = '\0';
    }

    uint32_t length = static_cast<uint32_t>(strlen(name));

    if (!buffer || *size <= length) {
        *size = length + 1;
        return 0;
    }

    memcpy(buffer, name, length + 1);
    *size = length;

    return 1;
}

// GetUserNameA: on success *size is the length including the terminator; on a short buffer it fails with the
// required size in *size.
int32_t OsGetUserName(char* buffer, uint32_t* size) {
    const char* name = nullptr;
    char storage[1024];
    struct passwd entry;
    struct passwd* result = nullptr;

    if (!getpwuid_r(getuid(), &entry, storage, sizeof(storage), &result) && result) {
        name = result->pw_name;
    }

    if (!name) {
        name = getenv("USER");
    }

    if (!name) {
        return 0;
    }

    uint32_t length = static_cast<uint32_t>(strlen(name));

    if (!buffer || *size <= length) {
        *size = length + 1;
        return 0;
    }

    memcpy(buffer, name, length + 1);
    *size = length + 1;

    return 1;
}

uint64_t OsGetPhysicalMemory() {
    uint64_t total;
    uint64_t available;
    OsPosixMemoryStatus(&total, &available);

    return total;
}

// Named kernel events have no Linux counterpart; the only caller creates one to mark the client as running and never
// uses the handle. A distinct non-null handle keeps the "created" result.
void* CreateNamedEventA(const char* name, int32_t manualReset) {
    struct NamedEvent {
        int32_t m_manualReset;
        char m_name[MAX_PATH];
    };

    NamedEvent* event = static_cast<NamedEvent*>(calloc(1, sizeof(NamedEvent)));

    if (event) {
        event->m_manualReset = manualReset;

        if (name) {
            SStrCopy(event->m_name, name, sizeof(event->m_name));
        }
    }

    return event;
}

// The executable segment(s) of the main program play the part of the PE .text section.
void OsFindTextSectionBounds() {
    s_textStart = 0;
    s_textEnd = 0;

    OsPosixSegment segments[32];
    int32_t count = OsPosixModuleSegments(nullptr, segments, 32);

    for (int32_t i = 0; i < count; i++) {
        if (!segments[i].m_executable) {
            continue;
        }

        uintptr_t start = segments[i].m_start;
        uintptr_t end = start + segments[i].m_size;

        if (!s_textStart || start < s_textStart) {
            s_textStart = start;
        }

        if (end > s_textEnd) {
            s_textEnd = end;
        }
    }

    if (!s_textStart || !s_textEnd) {
        SErrDisplayAppFatal("Unable to find .text section");
    }
}

void OsValidateFunctionPointer(const void* ptr) {
    if (!s_textStart || !s_textEnd) {
        OsFindTextSectionBounds();
    }

    if (reinterpret_cast<uintptr_t>(ptr) < s_textStart || reinterpret_cast<uintptr_t>(ptr) >= s_textEnd) {
        char buffer[64];
        SStrPrintf(buffer, sizeof(buffer), "Invalid function pointer: %p", ptr);
        SErrDisplayAppFatal(buffer);
    }
}

int32_t IOsParseProcessorFrequency(const char* brand) {
    if (!brand) {
        return 0;
    }

    const char* p = brand + strlen(brand) - 1;

    if (p <= brand || !*p || *p != 'z') {
        return 0;
    }

    p--;

    if (p <= brand || !*p || *p != 'H') {
        return 0;
    }

    p--;

    if (p <= brand) {
        return 0;
    }

    double scale;

    if (*p == 'G') {
        scale = 1000000000.0;
    } else if (*p == 'M') {
        scale = 1000000.0;
    } else if (*p == 'T') {
        scale = 1000000000000.0;
    } else {
        return 0;
    }

    char number[48];
    memset(number, ' ', sizeof(number));
    number[47] = '\0';

    p--;
    char* dst = &number[46];

    while (p > brand && *p && ((*p >= '0' && *p <= '9') || *p == '.')) {
        *dst = *p;
        p--;
        dst--;
    }

    double value = atof(number);

    if (value == 0.0) {
        return 0;
    }

    s_cpuTicksPerSecond = static_cast<int64_t>(value * scale);

    return 1;
}

// The equivalent of CallNtPowerInformation's maximum processor frequency is cpufreq's cpuinfo_max_freq (kHz).
int32_t IOsGetPowerProfFrequency() {
    int32_t result = 0;
    FILE* file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");

    if (file) {
        unsigned long long frequency;

        if (fscanf(file, "%llu", &frequency) == 1 && frequency) {
            s_cpuTicksPerSecond = static_cast<uint64_t>(frequency) * 1000;
            result = 1;
        }

        fclose(file);
    }

    return result;
}

int32_t OsIsRemoteSession() {
    return 0;
}

// ShellExecute "open" becomes xdg-open. It runs in the background of a short-lived shell, so no child is left to reap
// and the call does not wait for the browser.
int32_t OsLaunchURL(const char* url) {
    if (!url || !*url) {
        return 0;
    }

    if (SStrLen(url) >= 1024) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return 0;
    }

    char file[1024];
    char* dst = file;
    const char* src = url + 1;

    for (char c = *url; c; c = *src++, dst++) {
        if (c == '%') {
            char high = *src++;

            if (!high) {
                break;
            }

            char low = *src++;

            if (!low) {
                break;
            }

            *dst = (s_hexDigitValues[high & 0x7F] << 4) | s_hexDigitValues[low & 0x7F];
        } else {
            *dst = c;
        }
    }

    *dst = '\0';

    char shell[] = "/bin/sh";
    char option[] = "-c";
    char script[] = "command -v xdg-open >/dev/null 2>&1 || exit 1; xdg-open \"$1\" </dev/null >/dev/null 2>&1 &";
    char name[] = "sh";
    char* argv[] = { shell, option, script, name, file, nullptr };

    pid_t pid;

    if (posix_spawn(&pid, shell, nullptr, nullptr, argv, environ)) {
        return 0;
    }

    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return 0;
        }
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

uint32_t OsGetProcessorFeaturesEx(int32_t& vendor) {
    if (s_haveProcessorFeatures) {
        vendor = s_processorVendor;
        return s_processorFeatures;
    }

    s_haveProcessorFeatures = 1;

    uint32_t features = 0;

    s_processorCount = OsGetNumberOfProcessors();

    s_processorCores = 1;
    s_processorSockets = 1;

    CpuIdInfo info;

    if (IOsGetProcessorFeatures(&info)) {
        if (info.m_standard1d & 0x10) {
            features = 1;
        }

        if (info.m_standard1d & 0x800000) {
            features |= 2;
        }

        if (info.m_standard1d & 0x2000000) {
            features |= 4;
        }

        if (info.m_extended1d & 0x80000000) {
            features |= 8;
        }

        if (info.m_standard1d & 0x4000000) {
            features |= 0x10;
        }

        if (!strncmp(info.m_vendor, s_vendorIds, 12)) {
            s_processorVendor = 1;

            if (info.m_maxStandard >= 4 && (info.m_standard4a >> 26)) {
                features |= 0x40;
                s_processorCores = static_cast<uint8_t>(info.m_standard4a >> 26) + 1;
            }
        } else if (!strncmp(info.m_vendor, s_vendorIds + 12, 12)) {
            s_processorVendor = 2;

            uint8_t cores = static_cast<uint8_t>(info.m_extended8c);

            if (cores) {
                features |= 0x40;
                s_processorCores = cores + 1;
            }
        }
    } else {
        s_processorVendor = 4;
    }

    s_processorFeatures = features | 0x80000000;

    IOsSystemCpuLog(&info);

    if (!IOsParseProcessorFrequency(info.m_brand)) {
        if (!s_checkedPowerProfInfo) {
            s_gotPowerProfFrequency = IOsGetPowerProfFrequency();
            s_checkedPowerProfInfo = 1;
        }

        if (!s_gotPowerProfFrequency) {
            s_cpuTicksPerSecond = OsGetAsyncClocksPerSecond();
        }
    }

    vendor = s_processorVendor;
    return s_processorFeatures;
}

uint64_t OsGetProcessorTicksPerSecond() {
    if (!s_cpuTicksPerSecond) {
        int32_t vendor;
        OsGetProcessorFeaturesEx(vendor);
    }

    return s_cpuTicksPerSecond;
}

#endif
