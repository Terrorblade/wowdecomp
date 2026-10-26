#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/random.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Os/System.h"

bool OsPosixExecutablePath(char* buffer, size_t size) {
    if (!size) {
        return false;
    }

    ssize_t length = readlink("/proc/self/exe", buffer, size - 1);
    buffer[length > 0 ? length : 0] = '\0';
    return length > 0;
}

bool OsPosixGameDirectory(char* buffer, size_t size) {
    if (!OsPosixExecutablePath(buffer, size)) {
        return false;
    }

    char* slash = strrchr(buffer, '/');

    if (!slash) {
        return false;
    }

    slash[1] = '\0';
    return true;
}

int32_t OsPosixProcessArguments(char** argv, int32_t maxArgs, char* buffer, size_t size) {
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);

    if (fd < 0 || !size) {
        return 0;
    }

    size_t length = 0;
    ssize_t bytes;

    while (length < size - 1 && (bytes = read(fd, buffer + length, size - 1 - length)) > 0) {
        length += static_cast<size_t>(bytes);
    }

    close(fd);
    buffer[length] = '\0';

    int32_t argc = 0;

    for (size_t i = 0; i < length && argc < maxArgs; i += strlen(buffer + i) + 1) {
        argv[argc++] = buffer + i;
    }

    return argc;
}

bool OsPosixDebuggerAttached() {
    int32_t fd = open("/proc/self/status", O_RDONLY | O_CLOEXEC);

    if (fd < 0) {
        return false;
    }

    char buffer[4096];
    ssize_t length = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (length <= 0) {
        return false;
    }

    buffer[length] = 0;
    const char* tracer = strstr(buffer, "TracerPid:");

    return tracer && atoi(tracer + 10) != 0;
}

// glibc's strerror_r is the GNU one, which returns the message.
const char* OsPosixErrorString(int32_t error, char* buffer, size_t size) {
    return strerror_r(error, buffer, size);
}

bool OsPosixRandomBytes(void* buffer, size_t size) {
    size_t filled = 0;

    while (filled < size) {
        ssize_t result = getrandom(static_cast<uint8_t*>(buffer) + filled, size - filled, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        filled += static_cast<size_t>(result);
    }

    return true;
}

void OsPosixMemoryStatus(uint64_t* total, uint64_t* available) {
    struct sysinfo info;

    if (sysinfo(&info) != 0) {
        *total = 0;
        *available = 0;
        return;
    }

    *total = static_cast<uint64_t>(info.totalram) * info.mem_unit;
    *available = static_cast<uint64_t>(info.freeram + info.bufferram) * info.mem_unit;
}

int32_t OsPosixOsType() {
    return OsType_Linux;
}
