
#include <cstdint>
#include <cstring>
#include <new>
#include <windows.h>
#include "Engine/Source/Storm/Source/SThread.h"
#include "Engine/Source/Storm/H/storm.h"

#define MS_VC_EXCEPTION 0x406D1388

struct THREADNAME_INFO {
    /* 0000 */ DWORD dwType;
    /* 0004 */ LPCSTR szName;
    /* 0008 */ DWORD dwThreadID;
    /* 000C */ DWORD dwFlags;
};

struct PROCESSCOMPLETION {
    /* 0000 */ void (*callback)(void*);
    /* 0004 */ void* data;
    /* 0008 */ HANDLE process;
};

struct THREADLAUNCH {
    /* 0000 */ uint32_t (STORMAPI* threadProc)(void*);
    /* 0004 */ void* param;
    /* 0008 */ HANDLE threadH;
};

namespace S_Thread {
    // STATIC: 0x00CAC7B0
    SThreadTrack s_threads[s_maxthreads];

    // STATIC: 0x00CAD7B0
    int32_t s_numthreads;

    // STATIC: 0x00CAD7B4
    CInitCritSect s_threadCrit;
}

// OFFSET: 0x0076FDB0
uint32_t STORMAPI SGetCurrentThreadId() {
    return GetCurrentThreadId();
}

// OFFSET: 0x0076FDC0
uint32_t STORMAPI SGetCurrentProcessId() {
    return GetCurrentProcessId();
}

// OFFSET: 0x0076FDD0
int32_t STORMAPI SGetCurrentThreadPriority() {
    return GetThreadPriority(GetCurrentThread());
}

// OFFSET: 0x0076FDE0
int32_t STORMAPI SSetCurrentThreadPriority(int32_t priority) {
    return SetThreadPriority(GetCurrentThread(), priority);
}

// OFFSET: 0x0076FE00
int32_t STORMAPI SSetCurrentProcessAffinityMask(size_t mask) {
    return SetProcessAffinityMask(GetCurrentProcess(), mask);
}

// OFFSET: 0x0076FE20
static void SetThreadName(uint32_t threadId, const char* threadName) {
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = threadId;
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR*>(&info));
    } __except (EXCEPTION_CONTINUE_EXECUTION) {
    }
}

// OFFSET: 0x0076FEA0
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

// INLINED
void CInitCritSect::Leave() {
    this->m_critsect->Leave();
}

// OFFSET: 0x0076FEF0
static uint32_t STORMAPI ProcessCompletionCallbackThread(void* vdata) {
    PROCESSCOMPLETION* completion = static_cast<PROCESSCOMPLETION*>(vdata);
    WaitForSingleObject(completion->process, INFINITE);
    completion->callback(completion->data);
    delete completion;
    return 0;
}

// OFFSET: 0x0076FF30
uint32_t STORMAPI S_Thread::s_SLaunchThread(void* threadParam) {
    s_threadCrit.Enter();

    THREADLAUNCH* launch = static_cast<THREADLAUNCH*>(threadParam);
    uint32_t (STORMAPI* threadProc)(void*) = launch->threadProc;
    void* param = launch->param;
    uint32_t threadId = GetCurrentThreadId();
    const char* threadName = nullptr;

    for (int32_t i = 0; i < s_numthreads; i++) {
        if (s_threads[i].threadId == threadId) {
            threadName = s_threads[i].name;
            break;
        }
    }

    SMemFree(launch, __FILE__, __LINE__, 0);
    SetThreadName(threadId, threadName);
    s_threadCrit.Leave();

    uint32_t threadVal = threadProc(param);

    s_threadCrit.Enter();

    int32_t numthreads = s_numthreads;

    for (int32_t i = 0; i < numthreads; i++) {
        if (s_threads[i].live && s_threads[i].threadId == threadId) {
            memcpy(&s_threads[i], &s_threads[i + 1], (s_maxthreads - 1 - i) * sizeof(SThreadTrack));
            numthreads--;
        }
    }

    s_numthreads = numthreads;
    s_threadCrit.Leave();

    return threadVal;
}

// OFFSET: 0x007700A0
HANDLE SCreateThread(uint32_t dwStackSize, uint32_t (STORMAPI* lpStartAddress)(void*), void* lpParameter, uint32_t dwCreationFlags, uint32_t* lpThreadId, char* threadName) {
    if (!threadName) {
        threadName = const_cast<char*>("");
    }

    S_Thread::s_threadCrit.Enter();

    if (!S_Thread::s_numthreads) {
        S_Thread::SThreadTrack* track = &S_Thread::s_threads[S_Thread::s_numthreads];
        track->threadId = GetCurrentThreadId();
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

    S_Thread::s_threadCrit.Enter();

    HANDLE hThread = CreateThread(nullptr, dwStackSize, reinterpret_cast<LPTHREAD_START_ROUTINE>(S_Thread::s_SLaunchThread), launch, dwCreationFlags, reinterpret_cast<LPDWORD>(lpThreadId));
    launch->threadH = hThread;

    S_Thread::SThreadTrack* track = &S_Thread::s_threads[S_Thread::s_numthreads];
    track->threadId = *lpThreadId;
    track->threadH = launch->threadH;
    track->live = 1;
    track->suspended = (dwCreationFlags & CREATE_SUSPENDED) != 0;

    if (threadName) {
        SStrCopy(track->name, threadName, sizeof(track->name));
    }

    S_Thread::s_numthreads++;
    S_Thread::s_threadCrit.Leave();

    return hThread;
}

// OFFSET: 0x007701C0
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

// OFFSET: 0x00770260
HANDLE STORMAPI SCreateThread(uint32_t (STORMAPI* lpStartAddress)(void*), void* lpParameter, uint32_t* lpThreadId, void* linuxData, char* threadName, uint32_t stackSize) {
    return SCreateThread(stackSize, lpStartAddress, lpParameter, 0, lpThreadId, threadName);
}

// OFFSET: 0x00770290
int32_t STORMAPI SCreateProcess(const char* appName, char* commandLine, void (*callbackWhenProcessCompletes)(void*), void* callbackData) {
    STARTUPINFOW startInfo;
    memset(&startInfo, 0, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);

    PROCESS_INFORMATION processInfo = {};

    uint16_t appNameW[1040];
    uint16_t commandLineW[1040];

    if (appName) {
        SUniConvertUTF8to16(appNameW, sizeof(appNameW) / sizeof(appNameW[0]), appName, STORM_MAX_STR, nullptr, nullptr);
    }

    SUniConvertUTF8to16(commandLineW, sizeof(commandLineW) / sizeof(commandLineW[0]), commandLine, STORM_MAX_STR, nullptr, nullptr);

    if (!CreateProcessW(appName ? reinterpret_cast<LPCWSTR>(appNameW) : nullptr, reinterpret_cast<LPWSTR>(commandLineW), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startInfo, &processInfo)) {
        return 0;
    }

    if (callbackWhenProcessCompletes) {
        if (WaitForInputIdle(processInfo.hProcess, 10000)) {
            return 0;
        }

        PROCESSCOMPLETION* completion = static_cast<PROCESSCOMPLETION*>(SMemAlloc(sizeof(PROCESSCOMPLETION), __FILE__, __LINE__, 0));
        completion->data = callbackData;
        completion->callback = callbackWhenProcessCompletes;
        completion->process = processInfo.hProcess;

        uint32_t threadId;
        SCreateThread(ProcessCompletionCallbackThread, completion, &threadId, nullptr, nullptr, 0);
    }

    return 1;
}

