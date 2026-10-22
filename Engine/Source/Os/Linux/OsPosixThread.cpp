#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

uint32_t OsPosixThreadId() {
    return static_cast<uint32_t>(gettid());
}

void OsPosixSetThreadName(const char* name) {
    pthread_setname_np(pthread_self(), name);
}

// Linux nice values are per thread when given the thread id.
bool OsPosixSetThreadNice(pthread_t thread, uint32_t tid, int32_t nice) {
    return setpriority(PRIO_PROCESS, static_cast<id_t>(tid), nice) == 0;
}

bool OsPosixGetThreadNice(pthread_t thread, uint32_t tid, int32_t* nice) {
    errno = 0;
    int32_t value = getpriority(PRIO_PROCESS, static_cast<id_t>(tid));

    if (value == -1 && errno != 0) {
        return false;
    }

    *nice = value;
    return true;
}

bool OsPosixSetThreadAffinity(pthread_t thread, uint64_t mask) {
    cpu_set_t set;
    CPU_ZERO(&set);

    for (int32_t i = 0; i < 64; i++) {
        if (mask & (uint64_t(1) << i)) {
            CPU_SET(i, &set);
        }
    }

    return pthread_setaffinity_np(thread, sizeof(set), &set) == 0;
}

bool OsPosixSetProcessAffinity(uint64_t mask) {
    cpu_set_t set;
    CPU_ZERO(&set);

    for (uint32_t cpu = 0; cpu < 64 && cpu < CPU_SETSIZE; cpu++) {
        if (mask & (uint64_t(1) << cpu)) {
            CPU_SET(cpu, &set);
        }
    }

    // sched_setaffinity(0) only covers the calling thread; SetProcessAffinityMask covers every thread of the
    // process, so apply it to each of /proc/self/task.
    bool result = sched_setaffinity(0, sizeof(set), &set) == 0;

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/task", static_cast<int>(getpid()));
    DIR* dir = opendir(path);

    if (dir) {
        while (dirent* entry = readdir(dir)) {
            pid_t tid = static_cast<pid_t>(atoi(entry->d_name));

            if (tid > 0) {
                sched_setaffinity(tid, sizeof(set), &set);
            }
        }

        closedir(dir);
    }

    return result;
}

uint64_t OsPosixProcessAffinity() {
    uint64_t mask = 0;
    cpu_set_t set;
    CPU_ZERO(&set);

    if (sched_getaffinity(0, sizeof(set), &set) == 0) {
        for (int32_t i = 0; i < 64; i++) {
            if (CPU_ISSET(i, &set)) {
                mask |= uint64_t(1) << i;
            }
        }
    }

    return mask;
}

uint32_t OsPosixThreadCount() {
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

void OsPosixCondInit(pthread_cond_t* cond) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(cond, &attr);
    pthread_condattr_destroy(&attr);
}

int32_t OsPosixCondTimedWait(pthread_cond_t* cond, pthread_mutex_t* mutex, const timespec* monotonicDeadline) {
    return pthread_cond_timedwait(cond, mutex, monotonicDeadline);
}
