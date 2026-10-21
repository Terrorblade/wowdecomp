#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <new>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"
#include "Engine/Source/Storm/Source/SThread.h"
#include "Engine/Source/Storm/H/storm.h"

extern char** environ;

struct PROCESSCOMPLETION {
    void (*callback)(void*);
    void* data;
    pid_t process;
};

struct THREADLAUNCH {
    uint32_t (STORMAPI* threadProc)(void*);
    void* param;
    HANDLE threadH;
    // The creator waits on started until the new thread has published its id, so SCreateThread can return it
    // the way CreateThread does.
    sem_t started;
    pid_t threadId;
};

namespace S_Thread {
    SThreadTrack s_threads[s_maxthreads];

    int32_t s_numthreads;

    CInitCritSect s_threadCrit;
}

// Priority of threads Storm did not create (the main thread); Storm threads keep theirs in their thread object.
static thread_local int32_t t_threadPriority = STORM_THREAD_PRIORITY_NORMAL;

uint32_t STORMAPI SGetCurrentThreadId() {
    return static_cast<uint32_t>(StormLinux::CurrentThreadId());
}

uint32_t STORMAPI SGetCurrentProcessId() {
    return static_cast<uint32_t>(getpid());
}

int32_t STORMAPI SGetCurrentThreadPriority() {
    int32_t priority;

    if (StormLinux::SyncGetThreadPriority(StormLinux::SyncCurrentThreadObject(), &priority)) {
        return priority;
    }

    return t_threadPriority;
}

// Thread priorities map onto per-thread nice values. Raising priority above normal needs CAP_SYS_NICE, so the
// requested value is recorded and reported back by SGetCurrentThreadPriority even when the kernel refuses it.
int32_t STORMAPI SSetCurrentThreadPriority(int32_t priority) {
    void* object = StormLinux::SyncCurrentThreadObject();

    if (object) {
        StormLinux::SyncSetThreadPriorityValue(object, priority);
    } else {
        t_threadPriority = priority;
    }

    StormLinux::ApplyThreadPriority(StormLinux::CurrentThreadId(), priority);
    return 1;
}

int32_t STORMAPI SSetCurrentProcessAffinityMask(size_t mask) {
    if (!mask) {
        return 0;
    }

    cpu_set_t set;
    CPU_ZERO(&set);

    for (uint32_t cpu = 0; cpu < sizeof(mask) * 8 && cpu < CPU_SETSIZE; cpu++) {
        if (mask & (static_cast<size_t>(1) << cpu)) {
            CPU_SET(cpu, &set);
        }
    }

    // sched_setaffinity(0) only covers the calling thread; SetProcessAffinityMask covers every thread of the
    // process, so apply it to each of /proc/self/task.
    int32_t result = sched_setaffinity(0, sizeof(set), &set) == 0;

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

static void SetThreadName(uint32_t threadId, const char* threadName) {
    if (!threadName || !*threadName) {
        return;
    }

    // pthread names are limited to 15 characters plus the terminator.
    char name[16];
    SStrCopy(name, threadName, sizeof(name));
    pthread_setname_np(pthread_self(), name);
}

int32_t CInitCritSect::Enter() {
    int32_t result = 0;

    if (!this->m_critsect) {
        while (InterlockedExchange(&this->m_spinLock, 1)) {
        }

        if (!this->m_critsect) {
            this->m_critsect = reinterpret_cast<CCritSect*>(this->m_critsectData);
            new (this->m_critsectData) CCritSect();
            result = 1;
        }

        this->m_spinLock = 0;
    }

    this->m_critsect->Enter();
    return result;
}

void CInitCritSect::Leave() {
    this->m_critsect->Leave();
}

static uint32_t STORMAPI ProcessCompletionCallbackThread(void* vdata) {
    PROCESSCOMPLETION* completion = static_cast<PROCESSCOMPLETION*>(vdata);

    int status;

    while (waitpid(completion->process, &status, 0) == -1 && errno == EINTR) {
    }

    if (completion->callback) {
        completion->callback(completion->data);
    }

    SMemFree(completion, __FILE__, __LINE__, 0);
    return 0;
}

uint32_t STORMAPI S_Thread::s_SLaunchThread(void* threadParam) {
    THREADLAUNCH* launch = static_cast<THREADLAUNCH*>(threadParam);
    uint32_t threadId = SGetCurrentThreadId();
    HANDLE threadH = launch->threadH;

    StormLinux::SyncSetCurrentThreadObject(threadH);
    StormLinux::SyncThreadStarted(threadH, static_cast<pid_t>(threadId));
    launch->threadId = static_cast<pid_t>(threadId);
    sem_post(&launch->started);

    s_threadCrit.Enter();

    uint32_t (STORMAPI* threadProc)(void*) = launch->threadProc;
    void* param = launch->param;
    const char* threadName = nullptr;

    for (int32_t i = 0; i < s_numthreads; i++) {
        if (s_threads[i].threadId == threadId) {
            threadName = s_threads[i].name;
            break;
        }
    }

    sem_destroy(&launch->started);
    SMemFree(launch, __FILE__, __LINE__, 0);
    SetThreadName(threadId, threadName);
    s_threadCrit.Leave();

    uint32_t threadVal = threadProc(param);

    s_threadCrit.Enter();

    int32_t numthreads = s_numthreads;

    for (int32_t i = 0; i < numthreads; i++) {
        if (s_threads[i].live && s_threads[i].threadId == threadId) {
            memmove(&s_threads[i], &s_threads[i + 1], (s_maxthreads - 1 - i) * sizeof(SThreadTrack));
            numthreads--;
        }
    }

    s_numthreads = numthreads;
    s_threadCrit.Leave();

    StormLinux::SyncSetCurrentThreadObject(nullptr);
    StormLinux::SyncThreadExited(threadH);

    return threadVal;
}

static void* LaunchThreadEntry(void* threadParam) {
    S_Thread::s_SLaunchThread(threadParam);
    return nullptr;
}

// dwCreationFlags: CREATE_SUSPENDED has no pthread equivalent and no caller uses it, so flags are ignored.
HANDLE SCreateThread(uint32_t dwStackSize, uint32_t (STORMAPI* lpStartAddress)(void*), void* lpParameter, uint32_t dwCreationFlags, uint32_t* lpThreadId, char* threadName) {
    (void)dwCreationFlags;

    if (!threadName) {
        threadName = const_cast<char*>("");
    }

    S_Thread::s_threadCrit.Enter();

    if (!S_Thread::s_numthreads) {
        S_Thread::SThreadTrack* track = &S_Thread::s_threads[S_Thread::s_numthreads];
        track->threadId = SGetCurrentThreadId();
        track->threadH = nullptr;
        track->live = 1;
        track->suspended = 0;
        SStrCopy(track->name, "main", sizeof(track->name));
        S_Thread::s_numthreads++;
    }

    S_Thread::s_threadCrit.Leave();

    THREADLAUNCH* launch = static_cast<THREADLAUNCH*>(SMemAlloc(sizeof(THREADLAUNCH), __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY));
    launch->threadProc = lpStartAddress;
    launch->param = lpParameter;
    launch->threadH = StormLinux::SyncCreateThreadObject();
    sem_init(&launch->started, 0, 0);

    S_Thread::s_threadCrit.Enter();

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (dwStackSize) {
        size_t stackSize = dwStackSize < PTHREAD_STACK_MIN ? PTHREAD_STACK_MIN : dwStackSize;
        pthread_attr_setstacksize(&attr, stackSize);
    }

    HANDLE hThread = launch->threadH;
    pthread_t thread;

    if (pthread_create(&thread, &attr, LaunchThreadEntry, launch) != 0) {
        pthread_attr_destroy(&attr);
        S_Thread::s_threadCrit.Leave();

        // Drop both references: the thread never ran.
        StormLinux::SyncThreadExited(hThread);
        StormLinux::SyncClose(hThread);
        sem_destroy(&launch->started);
        SMemFree(launch, __FILE__, __LINE__, 0);
        return nullptr;
    }

    pthread_attr_destroy(&attr);

    // The new thread posts before it touches s_threadCrit, so waiting here while holding it is safe.
    while (sem_wait(&launch->started) == -1 && errno == EINTR) {
    }

    uint32_t threadId = static_cast<uint32_t>(launch->threadId);

    if (lpThreadId) {
        *lpThreadId = threadId;
    }

    S_Thread::SThreadTrack* track = &S_Thread::s_threads[S_Thread::s_numthreads];
    track->threadId = threadId;
    track->threadH = hThread;
    track->live = 1;
    track->suspended = 0;

    if (threadName) {
        SStrCopy(track->name, threadName, sizeof(track->name));
    }

    S_Thread::s_numthreads++;
    S_Thread::s_threadCrit.Leave();

    return hThread;
}

int32_t STORMAPI SGetThreadName(uint32_t threadId, char* buffer, int32_t bufferchars) {
    bool found = false;

    if (!buffer || bufferchars < 1) {
        return 0;
    }

    if (bufferchars > static_cast<int32_t>(sizeof(S_Thread::s_threads[0].name))) {
        bufferchars = sizeof(S_Thread::s_threads[0].name);
    }

    *buffer = 0;

    S_Thread::s_threadCrit.Enter();

    for (int32_t i = 0; i < S_Thread::s_numthreads; i++) {
        if (S_Thread::s_threads[i].threadId == threadId) {
            SStrCopy(buffer, S_Thread::s_threads[i].name, bufferchars);
            found = true;
            break;
        }
    }

    S_Thread::s_threadCrit.Leave();

    return found;
}

HANDLE STORMAPI SCreateThread(uint32_t (STORMAPI* lpStartAddress)(void*), void* lpParameter, uint32_t* lpThreadId, void* linuxData, char* threadName, uint32_t stackSize) {
    return SCreateThread(stackSize, lpStartAddress, lpParameter, 0, lpThreadId, threadName);
}

// Splits a Windows command line into argv following the CommandLineToArgvW quoting rules (quotes group, \" is a
// literal quote, backslashes are literal otherwise).
static int32_t SplitCommandLine(char* buffer, char** argv, int32_t maxargs) {
    int32_t argc = 0;
    char* src = buffer;
    char* dst = buffer;

    while (*src && argc < maxargs - 1) {
        while (*src == ' ' || *src == '\t') {
            src++;
        }

        if (!*src) {
            break;
        }

        argv[argc++] = dst;
        bool quoted = false;

        while (*src && (quoted || (*src != ' ' && *src != '\t'))) {
            if (*src == '\\') {
                int32_t backslashes = 0;

                while (*src == '\\') {
                    backslashes++;
                    src++;
                }

                if (*src == '"') {
                    for (int32_t i = 0; i < backslashes / 2; i++) {
                        *dst++ = '\\';
                    }

                    if (backslashes & 1) {
                        *dst++ = *src++;
                    }
                } else {
                    for (int32_t i = 0; i < backslashes; i++) {
                        *dst++ = '\\';
                    }
                }
            } else if (*src == '"') {
                quoted = !quoted;
                src++;
            } else {
                *dst++ = *src++;
            }
        }

        bool more = *src != 0;
        *dst++ = 0;

        if (more) {
            src++;
        }
    }

    argv[argc] = nullptr;
    return argc;
}

// CreateProcess looks for appName relative to the current directory; a Windows command line's program is also
// searched next to the executable. Both are resolved case-insensitively here.
static bool FindProgram(const char* name, char* out, size_t outSize) {
    if (StormLinux::ResolvePath(name, out, outSize, false) && access(out, X_OK) == 0) {
        return true;
    }

    if (SStrChr(name, '\\') || SStrChr(name, '/')) {
        return false;
    }

    char exePath[MAX_PATH];
    StormLinux::GetExePath(exePath, sizeof(exePath));
    char* slash = SStrChrR(exePath, '/');

    if (!slash) {
        return false;
    }

    slash[1] = 0;
    SStrPack(exePath, name, sizeof(exePath));

    return StormLinux::ResolvePath(exePath, out, outSize, false) && access(out, X_OK) == 0;
}

int32_t STORMAPI SCreateProcess(const char* appName, char* commandLine, void (*callbackWhenProcessCompletes)(void*), void* callbackData) {
    char buffer[4160];
    buffer[0] = 0;

    if (commandLine) {
        SStrCopy(buffer, commandLine, sizeof(buffer));
    }

    char* argv[128];
    int32_t argc = SplitCommandLine(buffer, argv, 128);
    const char* program = appName ? appName : (argc ? argv[0] : nullptr);

    if (!program || !*program) {
        return 0;
    }

    char programPath[MAX_PATH * 4];

    if (!FindProgram(program, programPath, sizeof(programPath))) {
        return 0;
    }

    char* defaultArgv[2] = { const_cast<char*>(program), nullptr };
    pid_t pid;

    if (posix_spawn(&pid, programPath, nullptr, nullptr, argc ? argv : defaultArgv, environ) != 0) {
        return 0;
    }

    // The waiting thread also reaps the child when there is no callback, so it does not linger as a zombie.
    PROCESSCOMPLETION* completion = static_cast<PROCESSCOMPLETION*>(SMemAlloc(sizeof(PROCESSCOMPLETION), __FILE__, __LINE__, 0));
    completion->data = callbackData;
    completion->callback = callbackWhenProcessCompletes;
    completion->process = pid;

    uint32_t threadId;
    HANDLE thread = SCreateThread(ProcessCompletionCallbackThread, completion, &threadId, nullptr, nullptr, 0);

    if (thread) {
        StormLinux::SyncClose(thread);
    } else {
        SMemFree(completion, __FILE__, __LINE__, 0);
    }

    return 1;
}

#endif
