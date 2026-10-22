#include "HoT/Platform/Process.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace HoT::Platform {

std::filesystem::path ExecutablePath() {
    std::error_code ec;
    std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path() : exe;
}

std::filesystem::path GameDirectory() {
    std::filesystem::path exe = ExecutablePath();

    if (exe.empty()) {
        std::error_code ec;
        return std::filesystem::current_path(ec);
    }

    return exe.parent_path();
}

// /proc/self/exe stats the running image even after it was renamed over, so this holds only before a replace.
bool IsRunningExecutable(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::equivalent(path, std::filesystem::path("/proc/self/exe"), ec);
}

uint64_t CurrentThreadId() {
    return static_cast<uint64_t>(syscall(SYS_gettid));
}

// Linux nice values are per thread when given the thread id, one step up is the closest match to BELOW_NORMAL.
void LowerThreadPriority() {
    id_t tid = static_cast<id_t>(syscall(SYS_gettid));
    setpriority(PRIO_PROCESS, tid, getpriority(PRIO_PROCESS, tid) + 1);
}

void SetBackgroundThreadPriority() {
    setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 19);
}

// Working set is VmRSS, private bytes is approximated by anonymous resident memory plus swap.
bool ProcessMemory(uint64_t& workingSet, uint64_t& privateBytes) {
    FILE* status = fopen("/proc/self/status", "r");

    if (!status) {
        return false;
    }

    char line[256];
    unsigned long long rssKB = 0;
    unsigned long long anonKB = 0;
    unsigned long long swapKB = 0;

    while (fgets(line, sizeof(line), status)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            rssKB = strtoull(line + 6, nullptr, 10);
        } else if (strncmp(line, "RssAnon:", 8) == 0) {
            anonKB = strtoull(line + 8, nullptr, 10);
        } else if (strncmp(line, "VmSwap:", 7) == 0) {
            swapKB = strtoull(line + 7, nullptr, 10);
        }
    }

    fclose(status);
    workingSet = rssKB * 1024;
    privateBytes = (anonKB + swapKB) * 1024;
    return true;
}

}
