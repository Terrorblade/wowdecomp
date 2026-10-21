#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstdlib>
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Linux/ThreadLinux.h"

bool Blizzard::Thread::Close(ThreadRecord* thread) {
    if (thread->m_refCount.Decrement() > 0) {
        return true;
    }

    if (thread->m_handle) {
        System_Thread::FreeLinuxHandle(System_Thread::GetLinuxHandle(thread->m_handle));
        thread->m_handle = nullptr;
    }

    System_Thread::RemoveFromRegistry(thread);
    free(thread);

    return true;
}

bool Blizzard::Thread::WaitForever(ThreadRecord* thread) {
    if (thread->m_state < 0) {
        return true;
    }

    System_Thread::ThreadWorker* worker = thread->m_worker;

    if (worker && thread != worker->m_thread) {
        if (thread == worker->m_task) {
            Blizzard::Lock::EventWaitForever(worker->m_readyEvent.m_event);
            Blizzard::Lock::EventSet(worker->m_readyEvent.m_event);
        }

        return true;
    }

    return System_Thread::WaitLinuxHandle(System_Thread::GetLinuxHandle(thread->m_handle));
}

#endif
