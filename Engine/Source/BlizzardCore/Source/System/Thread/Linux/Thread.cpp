#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <execinfo.h>
#include <new>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/ThreadLocalStorage.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Linux/ThreadLinux.h"

bool System_Thread::s_initialized;

Blizzard::Lock::Mutex System_Thread::s_mutex;

Blizzard::Lock::Mutex System_Thread::s_registryMutex;

Blizzard::Thread::TLSSlot System_Thread::s_threadRecordTLS;

Blizzard::Thread::TLSSlot System_Thread::s_stackTraceEntryPointTLS;

Blizzard::Thread::ThreadRecord* System_Thread::s_mainThread;

std::set<Blizzard::Thread::ThreadRecord*, std::less<Blizzard::Thread::ThreadRecord*>, Blizzard::Memory::AllocSTL_raw<Blizzard::Thread::ThreadRecord*>>* System_Thread::s_ThreadRegistry;

int32_t System_Thread::s_DefaultStackSize;

System_Thread::ExternalTLSRecord System_Thread::s_externalTLS[32];

int32_t System_Thread::s_externalTLSCount;

namespace {

// Windows watches threads that were not created through Blizzard::Thread with a helper thread that waits on their
// handle and runs their TLS destructors when they exit. A pthread key destructor fires at that same point, so it does
// the job here; ExternalTLSRecord::m_thread holds the pthread_t, m_event and m_watcher stay null.
pthread_key_t s_externalExitKey;
pthread_once_t s_externalExitOnce = PTHREAD_ONCE_INIT;

void ExternalThreadExit(void* value) {
    System_Thread::RunExternalTLSDestructors(reinterpret_cast<HANDLE>(pthread_self()));
}

void CreateExternalExitKey() {
    pthread_key_create(&s_externalExitKey, ExternalThreadExit);
}

void* LinuxThreadStarter(void* param) {
    System_Thread::ThreadStarter(param);
    return nullptr;
}

// Windows thread priorities as nice values. Raising priority above normal needs CAP_SYS_NICE (or an RLIMIT_NICE
// allowance), so those levels fail for ordinary users, like SetThreadPriority failing.
const int32_t s_niceLevels[7] = { 19, 10, 5, 0, -5, -10, -15 };

}

void System_Thread::FreeLinuxHandle(LinuxThreadHandle* handle) {
    pthread_cond_destroy(&handle->m_cond);
    pthread_mutex_destroy(&handle->m_mutex);
    delete handle;
}

bool System_Thread::WaitLinuxHandle(LinuxThreadHandle* handle) {
    if (!handle) {
        return false;
    }

    pthread_mutex_lock(&handle->m_mutex);

    while (!handle->m_exited) {
        pthread_cond_wait(&handle->m_cond, &handle->m_mutex);
    }

    pthread_mutex_unlock(&handle->m_mutex);

    return true;
}

bool System_Thread::SetLinuxAffinity(LinuxThreadHandle* handle, uint64_t mask) {
    if (!handle || !mask) {
        return false;
    }

    cpu_set_t set;
    CPU_ZERO(&set);

    for (int32_t i = 0; i < 64; i++) {
        if (mask & (uint64_t(1) << i)) {
            CPU_SET(i, &set);
        }
    }

    return pthread_setaffinity_np(handle->m_thread, sizeof(set), &set) == 0;
}

bool Blizzard::Thread::Sleep(uint32_t milliseconds) {
    timespec duration;
    duration.tv_sec = milliseconds / 1000;
    duration.tv_nsec = static_cast<long>(milliseconds % 1000) * 1000000;

    if (milliseconds == 0) {
        sched_yield();
        return true;
    }

    while (nanosleep(&duration, &duration) == -1 && errno == EINTR) {
    }

    return true;
}

bool Blizzard::Thread::IsRunning(ThreadRecord* thread) {
    return thread->m_state >= 0;
}

bool System_Thread::SetPriorityLevel(Blizzard::Thread::ThreadRecord* thread, int32_t level) {
    if (level < 0 || level > 6) {
        return false;
    }

    LinuxThreadHandle* handle = GetLinuxHandle(thread->m_handle);
    pid_t tid = handle ? handle->m_tid : gettid();

    return setpriority(PRIO_PROCESS, static_cast<id_t>(tid), s_niceLevels[level]) == 0;
}

bool System_Thread::GetPriorityLevel(Blizzard::Thread::ThreadRecord* thread, int32_t* level) {
    LinuxThreadHandle* handle = GetLinuxHandle(thread->m_handle);
    pid_t tid = handle ? handle->m_tid : gettid();

    errno = 0;
    int32_t nice = getpriority(PRIO_PROCESS, static_cast<id_t>(tid));

    if (nice == -1 && errno != 0) {
        return false;
    }

    int32_t result = 6;

    for (int32_t i = 0; i < 7; i++) {
        if (nice >= s_niceLevels[i]) {
            result = i;
            break;
        }
    }

    *level = result;

    // The Windows function reports failure even when it stores a level.
    return false;
}

void System_Thread::ShutdownThreadLocalStorage() {
    if (!s_initialized) {
        return;
    }

    // Windows wakes each watcher thread so it runs the destructors of its thread now; do that directly.
    Blizzard::Lock::ScopedMutex lock(s_mutex);

    while (s_externalTLSCount > 0) {
        s_externalTLSCount--;
        ExternalTLSRecord& record = s_externalTLS[s_externalTLSCount];

        if (record.m_destructor) {
            record.m_destructor(record.m_value);
        }
    }
}

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

void System_Thread::AddToRegistry(Blizzard::Thread::ThreadRecord* thread) {
    Blizzard::Lock::ScopedMutex lock(s_registryMutex);

    if (!s_ThreadRegistry) {
        s_ThreadRegistry = new std::set<Blizzard::Thread::ThreadRecord*, std::less<Blizzard::Thread::ThreadRecord*>, Blizzard::Memory::AllocSTL_raw<Blizzard::Thread::ThreadRecord*>>();
    }

    s_ThreadRegistry->insert(thread);
}

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

void System_Thread::RemoveFromRegistry(Blizzard::Thread::ThreadRecord* thread) {
    Blizzard::Lock::ScopedMutex lock(s_registryMutex);
    s_ThreadRegistry->erase(thread);

    if (s_ThreadRegistry->empty()) {
        delete s_ThreadRegistry;
        s_ThreadRegistry = nullptr;
    }
}

void System_Thread::InitMainThread() {
    s_mainThread = NewThread(nullptr, nullptr, nullptr);
    InternalSetLocalStorage(&s_threadRecordTLS, s_mainThread);
    s_mainThread->m_state = 1;
    s_mainThread->m_refCount.m_value = 1;
    s_mainThread->m_threadId = static_cast<uint32_t>(gettid());
}

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

void System_Thread::SetStackTraceEntryPoint(void* entryPoint) {
    InitThreadSystem();
    InternalSetLocalStorage(&s_stackTraceEntryPointTLS, entryPoint);
}

Blizzard::Thread::ThreadRecord* System_Thread::GetThreadRecord() {
    InitThreadSystem();

    return static_cast<Blizzard::Thread::ThreadRecord*>(pthread_getspecific(static_cast<pthread_key_t>(~s_threadRecordTLS.m_key)));
}

bool System_Thread::AllocateTLSSlot(Blizzard::Thread::TLSSlot* slot, Blizzard::Thread::TLSDestructor destructor) {
    InitThreadSystem();
    Blizzard::Lock::ScopedMutex lock(s_mutex);

    return slot->m_key || AllocateLocalStorage(slot, destructor);
}

void System_Thread::InitStackTraceEntryPoint() {
    void* frames[3];

    if (backtrace(frames, 3) == 3) {
        SetStackTraceEntryPoint(frames[2]);
    }
}

uint32_t System_Thread::ExternalTLSWatcher(void* param) {
    // Not used on Linux (see ExternalThreadExit); runs the destructors of the record's thread when called.
    ExternalTLSRecord* record = static_cast<ExternalTLSRecord*>(param);
    RunExternalTLSDestructors(record->m_thread);

    return 0;
}

uint32_t __stdcall System_Thread::ThreadStarter(void* param) {
    Blizzard::Thread::ThreadRecord* thread = static_cast<Blizzard::Thread::ThreadRecord*>(param);
    LinuxThreadHandle* handle = GetLinuxHandle(thread->m_handle);
    __atomic_store_n(&handle->m_tid, gettid(), __ATOMIC_RELEASE);

    InternalSetLocalStorage(&s_threadRecordTLS, thread);
    InitStackTraceEntryPoint();

    char name[16];
    Blizzard::String::Copy(name, thread->m_name, sizeof(name));
    pthread_setname_np(pthread_self(), name);

    while (!thread->m_state) {
        Blizzard::Thread::Sleep(1);
    }

    uint32_t result = thread->m_function(thread->m_param);
    thread->m_result = result;
    RunTLSDestructors();
    thread->m_state = -1;

    pthread_mutex_lock(&handle->m_mutex);
    handle->m_exited = true;
    pthread_cond_broadcast(&handle->m_cond);
    pthread_mutex_unlock(&handle->m_mutex);

    Blizzard::Thread::Close(thread);

    return result;
}

bool Blizzard::Thread::Create(uint32_t (*function)(void*), void* param, ThreadRecord** thread, const char* name, int32_t flags, int32_t stackSize) {
    System_Thread::InitThreadSystem();
    ThreadRecord* record = System_Thread::NewThread(function, param, name);
    *thread = record;

    System_Thread::LinuxThreadHandle* handle = new System_Thread::LinuxThreadHandle;
    pthread_mutex_init(&handle->m_mutex, nullptr);
    pthread_cond_init(&handle->m_cond, nullptr);
    handle->m_exited = false;
    handle->m_tid = 0;
    record->m_handle = handle;

    if (!stackSize) {
        stackSize = System_Thread::s_DefaultStackSize;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (stackSize > 0) {
        pthread_attr_setstacksize(&attr, stackSize < PTHREAD_STACK_MIN ? PTHREAD_STACK_MIN : static_cast<size_t>(stackSize));
    }

    // CREATE_SUSPENDED (flags & 1) needs no counterpart: ThreadStarter spins until m_state is set, which is how a
    // suspended thread is released.
    if (pthread_create(&handle->m_thread, &attr, LinuxThreadStarter, record) != 0) {
        // _beginthreadex failing leaves a null handle and the record unstarted.
        pthread_attr_destroy(&attr);
        System_Thread::FreeLinuxHandle(handle);
        record->m_handle = nullptr;
        (*thread)->m_state = (flags & 1) == 0;
        return true;
    }

    pthread_attr_destroy(&attr);

    // _beginthreadex returns the thread id with the handle; the thread publishes its tid at start, so wait for it.
    while (!__atomic_load_n(&handle->m_tid, __ATOMIC_ACQUIRE)) {
        sched_yield();
    }

    record->m_threadId = static_cast<uint32_t>(handle->m_tid);
    (*thread)->m_state = (flags & 1) == 0;

    return true;
}

void System_Thread::RegisterExternalTLSDestructor(void* value, Blizzard::Thread::TLSDestructor destructor) {
    uint32_t threadId = static_cast<uint32_t>(gettid());
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

    ExternalTLSRecord& record = s_externalTLS[s_externalTLSCount++];
    record.m_thread = reinterpret_cast<HANDLE>(pthread_self());
    record.m_watcher = nullptr;
    record.m_event = nullptr;
    record.m_threadId = threadId;
    record.m_value = value;
    record.m_destructor = destructor;

    if (i == s_externalTLSCount - 1) {
        pthread_once(&s_externalExitOnce, CreateExternalExitKey);
        pthread_setspecific(s_externalExitKey, reinterpret_cast<void*>(1));
    }
}

void System_Thread::InternalSetLocalStorage(const Blizzard::Thread::TLSSlot* slot, const void* value) {
    pthread_setspecific(static_cast<pthread_key_t>(~slot->m_key), value);

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

#endif
