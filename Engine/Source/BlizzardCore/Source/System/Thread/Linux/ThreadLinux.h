#ifndef ENGINE_SOURCE_BLIZZARDCORE_SOURCE_SYSTEM_THREAD_LINUX_THREADLINUX_H
#define ENGINE_SOURCE_BLIZZARDCORE_SOURCE_SYSTEM_THREAD_LINUX_THREADLINUX_H

#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <pthread.h>
#include <sys/types.h>

namespace System_Thread {

// What ThreadRecord::m_handle points to on Linux. Threads run detached; waiting for one (WaitForSingleObject on the
// thread handle on Windows) waits for m_exited instead. Owned by the ThreadRecord and freed with it.
struct LinuxThreadHandle {
    pthread_t m_thread;
    pid_t m_tid;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    bool m_exited;
};

inline LinuxThreadHandle* GetLinuxHandle(void* handle) {
    return static_cast<LinuxThreadHandle*>(handle);
}

void FreeLinuxHandle(LinuxThreadHandle* handle);
bool WaitLinuxHandle(LinuxThreadHandle* handle);
bool SetLinuxAffinity(LinuxThreadHandle* handle, uint64_t mask);

}

#endif

#endif
