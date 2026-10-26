#include <crt_externs.h>
#include <cstdlib>
#include <cstring>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Os/System.h"

bool OsPosixExecutablePath(char* buffer, size_t size) {
    if (!size) {
        return false;
    }

    char path[4096];
    uint32_t length = sizeof(path);
    buffer[0] = '\0';

    if (_NSGetExecutablePath(path, &length) != 0) {
        return false;
    }

    char resolved[PATH_MAX];
    const char* name = realpath(path, resolved) ? resolved : path;

    if (strlen(name) >= size) {
        return false;
    }

    strcpy(buffer, name);
    return true;
}

bool OsPosixGameDirectory(char* buffer, size_t size) {
    if (!OsPosixExecutablePath(buffer, size)) {
        return false;
    }

    char* bundle = strstr(buffer, ".app/Contents/MacOS/");

    if (bundle) {
        *bundle = '\0';
    }

    char* slash = strrchr(buffer, '/');

    if (!slash) {
        return false;
    }

    slash[1] = '\0';
    return true;
}

int32_t OsPosixProcessArguments(char** argv, int32_t maxArgs, char* buffer, size_t size) {
    int32_t count = *_NSGetArgc();
    char** args = *_NSGetArgv();
    int32_t argc = 0;
    size_t used = 0;

    for (int32_t i = 0; i < count && argc < maxArgs; i++) {
        size_t length = strlen(args[i]) + 1;

        if (used + length > size) {
            break;
        }

        memcpy(buffer + used, args[i], length);
        argv[argc++] = buffer + used;
        used += length;
    }

    return argc;
}

bool OsPosixDebuggerAttached() {
    kinfo_proc info;
    memset(&info, 0, sizeof(info));
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    size_t size = sizeof(info);

    if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0) {
        return false;
    }

    return (info.kp_proc.p_flag & P_TRACED) != 0;
}

// The XSI strerror_r fills the buffer and returns a status.
const char* OsPosixErrorString(int32_t error, char* buffer, size_t size) {
    return strerror_r(error, buffer, size) == 0 ? buffer : nullptr;
}

bool OsPosixRandomBytes(void* buffer, size_t size) {
    arc4random_buf(buffer, size);
    return true;
}

void OsPosixMemoryStatus(uint64_t* total, uint64_t* available) {
    *total = 0;
    *available = 0;

    uint64_t memory = 0;
    size_t size = sizeof(memory);

    if (sysctlbyname("hw.memsize", &memory, &size, nullptr, 0) == 0) {
        *total = memory;
    }

    vm_statistics64_data_t stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&stats), &count) == KERN_SUCCESS) {
        *available = static_cast<uint64_t>(stats.free_count + stats.inactive_count) * vm_page_size;
    }
}

int32_t OsPosixOsType() {
    return OsType_MacOSX;
}
