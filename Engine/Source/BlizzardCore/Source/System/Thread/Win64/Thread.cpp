
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <process.h>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/ThreadLocalStorage.h"

// STATIC: 0x00B33C58
bool System_Thread::s_initialized;

// STATIC: 0x00B33718
Blizzard::Lock::Mutex System_Thread::s_mutex;

// STATIC: 0x00B33930
Blizzard::Lock::Mutex System_Thread::s_registryMutex;

// STATIC: 0x00B3832C
Blizzard::Thread::TLSSlot System_Thread::s_threadRecordTLS;

// STATIC: 0x00B3831C
Blizzard::Thread::TLSSlot System_Thread::s_stackTraceEntryPointTLS;

// STATIC: 0x00B33710
Blizzard::Thread::ThreadRecord* System_Thread::s_mainThread;

// STATIC: 0x00B3394C
std::set<Blizzard::Thread::ThreadRecord*, std::less<Blizzard::Thread::ThreadRecord*>, Blizzard::Memory::AllocSTL_raw<Blizzard::Thread::ThreadRecord*>>* System_Thread::s_ThreadRegistry;

// STATIC: 0x00B38020
int32_t System_Thread::s_DefaultStackSize;

// STATIC: 0x00B33958
System_Thread::ExternalTLSRecord System_Thread::s_externalTLS[32];

// STATIC: 0x00B382B0
int32_t System_Thread::s_externalTLSCount;

// OFFSET: 0x004284E0
bool Blizzard::Thread::Sleep(uint32_t milliseconds) {
    ::Sleep(milliseconds);
    return true;
}

// OFFSET: 0x00428CA0
bool Blizzard::Thread::IsRunning(ThreadRecord* thread) {
    return thread->m_state >= 0;
}

// OFFSET: 0x00428CB0
bool System_Thread::SetPriorityLevel(Blizzard::Thread::ThreadRecord* thread, int32_t level) {
    switch (level) {
        case 0:
            return SetThreadPriority(thread->m_handle, THREAD_PRIORITY_IDLE) != 0;
        case 1:
            return SetThreadPriority(thread->m_handle, THREAD_PRIORITY_LOWEST) != 0;
        case 2:
            return SetThreadPriority(thread->m_handle, THREAD_PRIORITY_BELOW_NORMAL) != 0;
        case 3:
            return SetThreadPriority(thread->m_handle, THREAD_PRIORITY_NORMAL) != 0;
        case 4:
            return SetThreadPriority(thread->m_handle, THREAD_PRIORITY_ABOVE_NORMAL) != 0;
        case 5:
            return SetThreadPriority(thread->m_handle, THREAD_PRIORITY_HIGHEST) != 0;
        case 6:
            return SetThreadPriority(thread->m_handle, THREAD_PRIORITY_TIME_CRITICAL) != 0;
        default:
            return false;
    }
}

// OFFSET: 0x00428D80
bool System_Thread::GetPriorityLevel(Blizzard::Thread::ThreadRecord* thread, int32_t* level) {
    switch (GetThreadPriority(thread->m_handle)) {
        case THREAD_PRIORITY_IDLE:
            *level = 0;
        case THREAD_PRIORITY_LOWEST:
            *level = 1;
        case THREAD_PRIORITY_BELOW_NORMAL:
            *level = 2;
        case THREAD_PRIORITY_NORMAL:
            *level = 3;
        case THREAD_PRIORITY_ABOVE_NORMAL:
            *level = 4;
        case THREAD_PRIORITY_HIGHEST:
            *level = 5;
        case THREAD_PRIORITY_TIME_CRITICAL:
            *level = 6;
    }

    return false;
}

// OFFSET: 0x00428E20
void System_Thread::ShutdownThreadLocalStorage() {
    if (!s_initialized) {
        return;
    }

    Blizzard::Lock::MutexEnter(s_mutex);

    while (s_externalTLSCount > 0) {
        for (int32_t i = 0; i < s_externalTLSCount; i++) {
            HANDLE watcher = s_externalTLS[i].m_watcher;
            HANDLE event = s_externalTLS[i].m_event;

            if (watcher) {
                Blizzard::Lock::MutexLeave(s_mutex);
                SignalObjectAndWait(event, watcher, INFINITE, TRUE);
                Blizzard::Lock::MutexEnter(s_mutex);
            }
        }

        if (s_externalTLSCount <= 0) {
            break;
        }

        Blizzard::Lock::MutexLeave(s_mutex);
        Sleep(10);
        Blizzard::Lock::MutexEnter(s_mutex);
    }

    Blizzard::Lock::MutexLeave(s_mutex);
}

// OFFSET: 0x0042E7A0
void System_Thread::RunExternalTLSDestructors(HANDLE thread) {
    Blizzard::Lock::ScopedMutex lock(s_mutex);

    for (int32_t i = s_externalTLSCount; i != 0;) {
        i--;
        ExternalTLSRecord& record = s_externalTLS[i];

        if (record.m_thread == thread) {
            if (record.m_destructor) {
                record.m_destructor(record.m_value);
            }

            s_externalTLSCount--;
            record = s_externalTLS[s_externalTLSCount];
        }
    }
}

// OFFSET: 0x00448260
void System_Thread::AddToRegistry(Blizzard::Thread::ThreadRecord* thread) {
    Blizzard::Lock::ScopedMutex lock(s_registryMutex);

    if (!s_ThreadRegistry) {
        s_ThreadRegistry = new std::set<Blizzard::Thread::ThreadRecord*, std::less<Blizzard::Thread::ThreadRecord*>, Blizzard::Memory::AllocSTL_raw<Blizzard::Thread::ThreadRecord*>>();
    }

    s_ThreadRegistry->insert(thread);
}

// OFFSET: 0x00449D70
Blizzard::Thread::ThreadRecord* System_Thread::NewThread(uint32_t (*function)(void*), void* param, const char* name) {
    if (!name) {
        name = "";
    }

    uint32_t length = static_cast<uint32_t>(strlen(name));
    Blizzard::Thread::ThreadRecord* thread = static_cast<Blizzard::Thread::ThreadRecord*>(malloc(sizeof(Blizzard::Thread::ThreadRecord) + length + 1));
    memset(thread, 0, sizeof(Blizzard::Thread::ThreadRecord));
    thread->m_param = param;
    thread->m_function = function;
    thread->m_state = 0;
    thread->m_refCount.m_value = 2;
    Blizzard::String::Copy(thread->m_name, name, length + 1);
    AddToRegistry(thread);

    return thread;
}

// OFFSET: 0x00449DF0
void System_Thread::RemoveFromRegistry(Blizzard::Thread::ThreadRecord* thread) {
    Blizzard::Lock::ScopedMutex lock(s_registryMutex);
    s_ThreadRegistry->erase(thread);

    if (s_ThreadRegistry->empty()) {
        delete s_ThreadRegistry;
        s_ThreadRegistry = nullptr;
    }
}

// OFFSET: 0x0044B6E0
void System_Thread::InitMainThread() {
    s_mainThread = NewThread(nullptr, nullptr, nullptr);
    InternalSetLocalStorage(&s_threadRecordTLS, s_mainThread);
    s_mainThread->m_state = 1;
    s_mainThread->m_refCount.m_value = 1;
}

// OFFSET: 0x0044CCE0
void System_Thread::InitThreadSystem() {
    if (s_initialized) {
        return;
    }

    s_initialized = true;
    Blizzard::Lock::MutexCreate(s_mutex);
    Blizzard::Lock::MutexCreate(s_registryMutex);
    AllocateLocalStorage(&s_threadRecordTLS, nullptr);
    AllocateLocalStorage(&s_stackTraceEntryPointTLS, nullptr);
    InitMainThread();
}

// OFFSET: 0x0044CD30
void System_Thread::SetStackTraceEntryPoint(void* entryPoint) {
    InitThreadSystem();
    InternalSetLocalStorage(&s_stackTraceEntryPointTLS, entryPoint);
}

// OFFSET: 0x0044CD90
Blizzard::Thread::ThreadRecord* System_Thread::GetThreadRecord() {
    InitThreadSystem();

    return static_cast<Blizzard::Thread::ThreadRecord*>(TlsGetValue(~s_threadRecordTLS.m_key));
}

// OFFSET: 0x0044CDF0
bool System_Thread::AllocateTLSSlot(Blizzard::Thread::TLSSlot* slot, Blizzard::Thread::TLSDestructor destructor) {
    InitThreadSystem();
    Blizzard::Lock::ScopedMutex lock(s_mutex);

    return slot->m_key || AllocateLocalStorage(slot, destructor);
}

// OFFSET: 0x0044CEC0
void System_Thread::InitStackTraceEntryPoint() {
    void* entryPoint;

    if (RtlCaptureStackBackTrace(2, 1, &entryPoint, nullptr) == 1) {
        SetStackTraceEntryPoint(entryPoint);
    }
}

// OFFSET: 0x0044CF50
uint32_t System_Thread::ExternalTLSWatcher(void* param) {
    ExternalTLSRecord* record = static_cast<ExternalTLSRecord*>(param);

    Blizzard::Lock::MutexEnter(s_mutex);
    Blizzard::Thread::ThreadRecord* thread = GetThreadRecord();

    if (thread && thread->m_worker) {
        thread = thread->m_worker->m_task;
    }

    HANDLE handles[2];
    handles[0] = record->m_thread;
    handles[1] = record->m_event;
    record->m_watcher = thread->m_handle;
    Blizzard::Lock::MutexLeave(s_mutex);

    MsgWaitForMultipleObjects(2, handles, FALSE, INFINITE, 0);
    RunExternalTLSDestructors(handles[0]);
    CloseHandle(handles[0]);
    CloseHandle(handles[1]);

    return 0;
}

// OFFSET: 0x0044DEE0
uint32_t __stdcall System_Thread::ThreadStarter(void* param) {
    Blizzard::Thread::ThreadRecord* thread = static_cast<Blizzard::Thread::ThreadRecord*>(param);

    InternalSetLocalStorage(&s_threadRecordTLS, thread);
    InitStackTraceEntryPoint();

    while (!thread->m_state) {
        Sleep(1);
    }

    uint32_t result = thread->m_function(thread->m_param);
    thread->m_result = result;
    RunTLSDestructors();
    thread->m_state = -1;
    Blizzard::Thread::Close(thread);

    return result;
}

// OFFSET: 0x00450AC0
bool Blizzard::Thread::Create(uint32_t (*function)(void*), void* param, ThreadRecord** thread, const char* name, int32_t flags, int32_t stackSize) {
    System_Thread::InitThreadSystem();
    ThreadRecord* record = System_Thread::NewThread(function, param, name);
    *thread = record;

    uint32_t initFlags = (flags & 1) ? CREATE_SUSPENDED : 0;

    if (!stackSize) {
        stackSize = System_Thread::s_DefaultStackSize;
    }

    if (stackSize > 0) {
        initFlags |= STACK_SIZE_PARAM_IS_A_RESERVATION;
    }

    record->m_handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, stackSize, System_Thread::ThreadStarter, record, initFlags, &record->m_threadId));
    (*thread)->m_state = (flags & 1) == 0;

    return true;
}

// OFFSET: 0x00450BF0
void System_Thread::RegisterExternalTLSDestructor(void* value, Blizzard::Thread::TLSDestructor destructor) {
    uint32_t threadId = GetCurrentThreadId();
    Blizzard::Lock::ScopedMutex lock(s_mutex);

    if (s_externalTLSCount >= 32) {
        return;
    }

    int32_t i;

    for (i = 0; i < s_externalTLSCount; i++) {
        if (s_externalTLS[i].m_threadId == threadId) {
            break;
        }
    }

    if (i < s_externalTLSCount) {
        ExternalTLSRecord& record = s_externalTLS[s_externalTLSCount++];
        record.m_thread = s_externalTLS[i].m_thread;
        record.m_watcher = s_externalTLS[i].m_watcher;
        record.m_event = s_externalTLS[i].m_event;
        record.m_threadId = threadId;
        record.m_value = value;
        record.m_destructor = destructor;
    } else {
        HANDLE thread = nullptr;

        if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &thread, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            ExternalTLSRecord& record = s_externalTLS[s_externalTLSCount];
            record.m_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            record.m_value = value;
            record.m_thread = thread;
            record.m_watcher = nullptr;
            record.m_threadId = threadId;
            record.m_destructor = destructor;
            s_externalTLSCount++;

            if (!Blizzard::Thread::CreateDetached(ExternalTLSWatcher, &record, "Blizzard TLS Destructor")) {
                s_externalTLSCount--;
                CloseHandle(thread);
            }
        }
    }
}

// OFFSET: 0x00451C40
void System_Thread::InternalSetLocalStorage(const Blizzard::Thread::TLSSlot* slot, const void* value) {
    TlsSetValue(~slot->m_key, const_cast<void*>(value));

    if (slot->m_destructor && value) {
        Blizzard::Thread::ThreadRecord* thread = GetThreadRecord();

        if (thread && thread->m_worker) {
            thread = thread->m_worker->m_task;
        }

        if (!thread) {
            RegisterExternalTLSDestructor(const_cast<void*>(value), slot->m_destructor);
        }
    }
}

