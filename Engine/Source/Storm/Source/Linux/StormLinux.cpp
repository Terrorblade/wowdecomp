#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"

namespace StormLinux {

enum SyncType {
    SYNC_EVENT,
    SYNC_SEMAPHORE,
    SYNC_MUTEX,
    SYNC_THREAD,
};

// All sync objects share one lock and one condition variable, which makes waiting on any mix of objects (and
// WaitForMultipleObjects' wait-all) simple: a waiter re-checks its whole set whenever any object changes state.
struct SyncObject {
    int32_t type;
    int32_t refCount;
    int32_t manualReset;
    int32_t signaled;
    int32_t count;
    int32_t maximumCount;
    pid_t owner;
    int32_t recursion;
    pid_t tid;
    int32_t priority;
};

static pthread_mutex_t s_syncLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_syncCond;
static pthread_once_t s_syncOnce = PTHREAD_ONCE_INIT;
static thread_local void* t_threadObject;

static void InitSyncCond() {
    OsPosixCondInit(&s_syncCond);
}

static void LockSync() {
    pthread_once(&s_syncOnce, InitSyncCond);
    pthread_mutex_lock(&s_syncLock);
}

static void UnlockSync() {
    pthread_mutex_unlock(&s_syncLock);
}

static SyncObject* NewObject(int32_t type) {
    SyncObject* object = new SyncObject();
    memset(object, 0, sizeof(*object));
    object->type = type;
    object->refCount = 1;
    return object;
}

static void ReleaseLocked(SyncObject* object) {
    if (--object->refCount == 0) {
        delete object;
    }
}

pid_t CurrentThreadId() {
    static thread_local pid_t tid;

    if (!tid) {
        tid = static_cast<pid_t>(OsPosixThreadId());
    }

    return tid;
}

void* SyncCreateEvent(int32_t manualReset, int32_t initialState) {
    SyncObject* object = NewObject(SYNC_EVENT);
    object->manualReset = manualReset != 0;
    object->signaled = initialState != 0;
    return object;
}

void* SyncCreateSemaphore(int32_t initialCount, int32_t maximumCount) {
    if (maximumCount <= 0 || initialCount < 0 || initialCount > maximumCount) {
        return nullptr;
    }

    SyncObject* object = NewObject(SYNC_SEMAPHORE);
    object->count = initialCount;
    object->maximumCount = maximumCount;
    return object;
}

void* SyncCreateMutex(int32_t initialOwner) {
    SyncObject* object = NewObject(SYNC_MUTEX);

    if (initialOwner) {
        object->owner = CurrentThreadId();
        object->recursion = 1;
    }

    return object;
}

void* SyncCreateThreadObject() {
    SyncObject* object = NewObject(SYNC_THREAD);
    object->refCount = 2;
    return object;
}

void SyncThreadStarted(void* handle, pid_t tid) {
    LockSync();
    static_cast<SyncObject*>(handle)->tid = tid;
    UnlockSync();
}

void SyncThreadExited(void* handle) {
    SyncObject* object = static_cast<SyncObject*>(handle);
    LockSync();
    object->signaled = 1;
    pthread_cond_broadcast(&s_syncCond);
    ReleaseLocked(object);
    UnlockSync();
}

pid_t SyncGetThreadId(void* handle) {
    SyncObject* object = static_cast<SyncObject*>(handle);

    if (!object || object->type != SYNC_THREAD) {
        return 0;
    }

    LockSync();
    pid_t tid = object->tid;
    UnlockSync();
    return tid;
}

int32_t SyncGetThreadPriority(void* handle, int32_t* priority) {
    SyncObject* object = static_cast<SyncObject*>(handle);

    if (!object || object->type != SYNC_THREAD) {
        return 0;
    }

    LockSync();
    *priority = object->priority;
    UnlockSync();
    return 1;
}

void SyncSetThreadPriorityValue(void* handle, int32_t priority) {
    SyncObject* object = static_cast<SyncObject*>(handle);

    if (!object || object->type != SYNC_THREAD) {
        return;
    }

    LockSync();
    object->priority = priority;
    UnlockSync();
}

int32_t SyncSetEvent(void* handle) {
    SyncObject* object = static_cast<SyncObject*>(handle);

    if (!object || object->type != SYNC_EVENT) {
        return 0;
    }

    LockSync();
    object->signaled = 1;
    pthread_cond_broadcast(&s_syncCond);
    UnlockSync();
    return 1;
}

int32_t SyncResetEvent(void* handle) {
    SyncObject* object = static_cast<SyncObject*>(handle);

    if (!object || object->type != SYNC_EVENT) {
        return 0;
    }

    LockSync();
    object->signaled = 0;
    UnlockSync();
    return 1;
}

int32_t SyncReleaseSemaphore(void* handle, int32_t count, int32_t* previousCount) {
    SyncObject* object = static_cast<SyncObject*>(handle);

    if (!object || object->type != SYNC_SEMAPHORE || count <= 0) {
        return 0;
    }

    LockSync();

    if (object->count > object->maximumCount - count) {
        UnlockSync();
        return 0;
    }

    if (previousCount) {
        *previousCount = object->count;
    }

    object->count += count;
    pthread_cond_broadcast(&s_syncCond);
    UnlockSync();
    return 1;
}

int32_t SyncReleaseMutex(void* handle) {
    SyncObject* object = static_cast<SyncObject*>(handle);

    if (!object || object->type != SYNC_MUTEX) {
        return 0;
    }

    LockSync();

    if (object->owner != CurrentThreadId() || object->recursion <= 0) {
        UnlockSync();
        return 0;
    }

    if (--object->recursion == 0) {
        object->owner = 0;
        pthread_cond_broadcast(&s_syncCond);
    }

    UnlockSync();
    return 1;
}

int32_t SyncClose(void* handle) {
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    LockSync();
    ReleaseLocked(static_cast<SyncObject*>(handle));
    UnlockSync();
    return 1;
}

static bool IsSignaledLocked(const SyncObject* object, pid_t self) {
    switch (object->type) {
        case SYNC_EVENT:
        case SYNC_THREAD:
            return object->signaled != 0;
        case SYNC_SEMAPHORE:
            return object->count > 0;
        case SYNC_MUTEX:
            return object->recursion == 0 || object->owner == self;
        default:
            return false;
    }
}

static void AcquireLocked(SyncObject* object, pid_t self) {
    switch (object->type) {
        case SYNC_EVENT:
            if (!object->manualReset) {
                object->signaled = 0;
            }
            break;
        case SYNC_SEMAPHORE:
            object->count--;
            break;
        case SYNC_MUTEX:
            object->owner = self;
            object->recursion++;
            break;
        default:
            break;
    }
}

uint32_t SyncWait(uint32_t count, void* const* handles, int32_t waitAll, uint32_t timeout) {
    if (count == 0 || count > STORM_MAXIMUM_WAIT_OBJECTS || !handles) {
        return STORM_WAIT_FAILED;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (!handles[i] || handles[i] == INVALID_HANDLE_VALUE) {
            return STORM_WAIT_FAILED;
        }
    }

    SyncObject* const* objects = reinterpret_cast<SyncObject* const*>(handles);
    pid_t self = CurrentThreadId();

    timespec deadline;

    if (timeout != INFINITE) {
        clock_gettime(CLOCK_MONOTONIC, &deadline);
        deadline.tv_sec += timeout / 1000;
        deadline.tv_nsec += static_cast<long>(timeout % 1000) * 1000000;

        if (deadline.tv_nsec >= 1000000000) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000;
        }
    }

    LockSync();

    for (uint32_t i = 0; i < count; i++) {
        objects[i]->refCount++;
    }

    uint32_t result = STORM_WAIT_TIMEOUT;

    while (true) {
        if (waitAll) {
            bool all = true;

            for (uint32_t i = 0; i < count && all; i++) {
                all = IsSignaledLocked(objects[i], self);
            }

            if (all) {
                for (uint32_t i = 0; i < count; i++) {
                    AcquireLocked(objects[i], self);
                }

                result = STORM_WAIT_OBJECT_0;
                break;
            }
        } else {
            bool found = false;

            for (uint32_t i = 0; i < count; i++) {
                if (IsSignaledLocked(objects[i], self)) {
                    AcquireLocked(objects[i], self);
                    result = STORM_WAIT_OBJECT_0 + i;
                    found = true;
                    break;
                }
            }

            if (found) {
                break;
            }
        }

        if (timeout == 0) {
            break;
        }

        if (timeout == INFINITE) {
            pthread_cond_wait(&s_syncCond, &s_syncLock);
        } else if (OsPosixCondTimedWait(&s_syncCond, &s_syncLock, &deadline) == ETIMEDOUT) {
            // One last check so an object signalled right at the deadline is not missed.
            timeout = 0;
        }
    }

    for (uint32_t i = 0; i < count; i++) {
        ReleaseLocked(objects[i]);
    }

    UnlockSync();
    return result;
}

void* SyncCurrentThreadObject() {
    return t_threadObject;
}

void SyncSetCurrentThreadObject(void* handle) {
    t_threadObject = handle;
}

uint32_t GetTickCount() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<uint32_t>(static_cast<uint64_t>(now.tv_sec) * 1000 + static_cast<uint64_t>(now.tv_nsec) / 1000000);
}

void SleepMs(uint32_t ms) {
    if (!ms) {
        sched_yield();
        return;
    }

    timespec delay;
    delay.tv_sec = ms / 1000;
    delay.tv_nsec = static_cast<long>(ms % 1000) * 1000000;

    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
    }
}

void GetExePath(char* buffer, size_t buffersize) {
    if (!buffer || !buffersize) {
        return;
    }

    OsPosixExecutablePath(buffer, buffersize);
}

static bool AppendComponent(char* out, size_t outSize, size_t* length, const char* component, size_t componentLength) {
    bool needSlash = *length > 0 && out[*length - 1] != '/';

    if (*length + (needSlash ? 1 : 0) + componentLength + 1 > outSize) {
        return false;
    }

    if (needSlash) {
        out[(*length)++] = '/';
    }

    memcpy(&out[*length], component, componentLength);
    *length += componentLength;
    out[*length] = 0;
    return true;
}

// Looks for a case-insensitive match of name in directory dir; writes the real spelling into match.
static bool FindCaseInsensitive(const char* dir, const char* name, char* match, size_t matchSize) {
    DIR* handle = opendir(*dir ? dir : ".");

    if (!handle) {
        return false;
    }

    bool found = false;

    while (dirent* entry = readdir(handle)) {
        if (!strcasecmp(entry->d_name, name) && strlen(entry->d_name) < matchSize) {
            strcpy(match, entry->d_name);
            found = true;
            break;
        }
    }

    closedir(handle);
    return found;
}

// Missing components are passed through unchanged, which is what forCreate asks for; opening such a path for
// reading fails the way it does on Windows.
bool ResolvePath(const char* path, char* out, size_t outSize, bool forCreate) {
    (void)forCreate;

    if (!path || !out || !outSize) {
        return false;
    }

    out[0] = 0;

    // Drop a drive letter: "C:\foo" names "/foo", "C:foo" names "foo".
    if (isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        path += 2;
    }

    size_t length = 0;
    bool absolute = path[0] == '\\' || path[0] == '/';

    if (absolute) {
        out[length++] = '/';
        out[length] = 0;
    }

    bool exists = true;
    const char* curr = path;

    while (*curr) {
        while (*curr == '\\' || *curr == '/') {
            curr++;
        }

        if (!*curr) {
            break;
        }

        const char* end = curr;

        while (*end && *end != '\\' && *end != '/') {
            end++;
        }

        size_t componentLength = static_cast<size_t>(end - curr);
        char component[256];

        if (componentLength >= sizeof(component)) {
            return false;
        }

        memcpy(component, curr, componentLength);
        component[componentLength] = 0;

        if (exists && strcmp(component, ".") && strcmp(component, "..")) {
            char candidate[4096];
            size_t candidateLength = length;

            if (length >= sizeof(candidate)) {
                return false;
            }

            memcpy(candidate, out, length + 1);

            struct stat st;

            if (!AppendComponent(candidate, sizeof(candidate), &candidateLength, component, componentLength) || lstat(candidate, &st) != 0) {
                char match[256];

                if (FindCaseInsensitive(out, component, match, sizeof(match))) {
                    strcpy(component, match);
                    componentLength = strlen(component);
                } else {
                    exists = false;
                }
            }
        }

        if (!AppendComponent(out, outSize, &length, component, componentLength)) {
            return false;
        }

        curr = end;
    }

    if (!length) {
        if (outSize < 2) {
            return false;
        }

        out[0] = '.';
        out[1] = 0;
    }

    return true;
}

void ApplyThreadPriority(pid_t tid, int32_t priority) {
    int32_t nice;

    if (priority <= STORM_THREAD_PRIORITY_IDLE) {
        nice = 19;
    } else if (priority >= STORM_THREAD_PRIORITY_TIME_CRITICAL) {
        nice = -10;
    } else if (priority <= STORM_THREAD_PRIORITY_LOWEST) {
        nice = 10;
    } else if (priority == STORM_THREAD_PRIORITY_BELOW_NORMAL) {
        nice = 5;
    } else if (priority == STORM_THREAD_PRIORITY_NORMAL) {
        nice = 0;
    } else if (priority == STORM_THREAD_PRIORITY_ABOVE_NORMAL) {
        nice = -5;
    } else {
        nice = -10;
    }

    // The pthread is only known for the calling thread, which is enough where ids do not name threads.
    if (tid) {
        OsPosixSetThreadNice(tid == CurrentThreadId() ? pthread_self() : pthread_t(), static_cast<uint32_t>(tid), nice);
    }
}

}

#endif
