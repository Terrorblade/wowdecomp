#if defined(WOWDECOMP_PLATFORM_LINUX)

#ifndef ENGINE_SOURCE_STORM_SOURCE_LINUX_STORMLINUX_H
#define ENGINE_SOURCE_STORM_SOURCE_LINUX_STORMLINUX_H

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

// Linux stand-ins for the kernel objects behind Storm's HANDLEs (events, semaphores, mutexes, threads) plus a few
// path/time helpers shared by the Storm platform files. A HANDLE returned from here points to a StormLinux::SyncObject.

#define STORM_WAIT_OBJECT_0 0x00000000u
#define STORM_WAIT_ABANDONED_0 0x00000080u
#define STORM_WAIT_TIMEOUT 0x00000102u
#define STORM_WAIT_FAILED 0xFFFFFFFFu
#define STORM_MAXIMUM_WAIT_OBJECTS 64

// Windows error codes Storm stores in its last-error slot.
#define STORM_WIN_ERROR_FILE_NOT_FOUND 2
#define STORM_WIN_ERROR_PATH_NOT_FOUND 3
#define STORM_WIN_ERROR_ACCESS_DENIED 5
#define STORM_WIN_ERROR_INVALID_HANDLE 6
#define STORM_WIN_ERROR_NOT_ENOUGH_MEMORY 8
#define STORM_WIN_ERROR_WRITE_FAULT 29
#define STORM_WIN_ERROR_READ_FAULT 30
#define STORM_WIN_ERROR_HANDLE_EOF 38
#define STORM_WIN_ERROR_FILE_EXISTS 80
#define STORM_WIN_ERROR_INVALID_PARAMETER 87
#define STORM_WIN_ERROR_OPEN_FAILED 110
#define STORM_WIN_ERROR_DISK_FULL 112
#define STORM_WIN_ERROR_ALREADY_EXISTS 183
#define STORM_WIN_ERROR_CANTOPEN 1011

// Windows thread priorities (SSetCurrentThreadPriority / SGetCurrentThreadPriority take and return these).
#define STORM_THREAD_PRIORITY_IDLE (-15)
#define STORM_THREAD_PRIORITY_LOWEST (-2)
#define STORM_THREAD_PRIORITY_BELOW_NORMAL (-1)
#define STORM_THREAD_PRIORITY_NORMAL 0
#define STORM_THREAD_PRIORITY_ABOVE_NORMAL 1
#define STORM_THREAD_PRIORITY_HIGHEST 2
#define STORM_THREAD_PRIORITY_TIME_CRITICAL 15

namespace StormLinux {
    void* SyncCreateEvent(int32_t manualReset, int32_t initialState);
    void* SyncCreateSemaphore(int32_t initialCount, int32_t maximumCount);
    void* SyncCreateMutex(int32_t initialOwner);
    // A thread object starts unsignalled with two references: the caller's handle and the thread's own, which
    // SyncThreadExited drops after signalling it.
    void* SyncCreateThreadObject();
    void SyncThreadStarted(void* handle, pid_t tid);
    void SyncThreadExited(void* handle);
    pid_t SyncGetThreadId(void* handle);
    int32_t SyncGetThreadPriority(void* handle, int32_t* priority);
    void SyncSetThreadPriorityValue(void* handle, int32_t priority);

    int32_t SyncSetEvent(void* handle);
    int32_t SyncResetEvent(void* handle);
    int32_t SyncReleaseSemaphore(void* handle, int32_t count, int32_t* previousCount);
    int32_t SyncReleaseMutex(void* handle);
    int32_t SyncClose(void* handle);

    // WaitForSingleObject / WaitForMultipleObjects: returns STORM_WAIT_OBJECT_0 + index, STORM_WAIT_TIMEOUT or
    // STORM_WAIT_FAILED (count 0, too many or null handles).
    uint32_t SyncWait(uint32_t count, void* const* handles, int32_t waitAll, uint32_t timeout);

    // Thread-local pointer to the calling thread's own thread object (null for threads Storm did not create).
    void* SyncCurrentThreadObject();
    void SyncSetCurrentThreadObject(void* handle);

    pid_t CurrentThreadId();
    uint32_t GetTickCount();
    void SleepMs(uint32_t ms);

    // Full path of the running executable ("" if unknown).
    void GetExePath(char* buffer, size_t buffersize);

    // Maps a Windows-style path ('\\' separators, optional drive letter, case-insensitive) onto the Linux
    // filesystem. Existing components are matched case-insensitively; with forCreate a missing last component is
    // kept as given. Returns false if the result does not fit in out.
    bool ResolvePath(const char* path, char* out, size_t outSize, bool forCreate);

    // Converts a Windows thread priority to a nice value and applies it to the thread (best effort: raising
    // priority needs CAP_SYS_NICE).
    void ApplyThreadPriority(pid_t tid, int32_t priority);
}

#endif

#endif
