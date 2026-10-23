
#include <cstdint>
#include <cstdlib>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/System/Hardware/Hardware.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/ThreadLocalStorage.h"

// OFFSET: 0x0044B7B0
System_Thread::ThreadWorker::~ThreadWorker() {
    Blizzard::Lock::EventSet(this->m_wakeEvent.m_event);
    Blizzard::Lock::EventWaitForever(this->m_readyEvent.m_event);

    if (this->m_thread) {
        Blizzard::Lock::EventSet(this->m_wakeEvent.m_event);
        WaitForSingleObject(this->m_thread->m_handle, INFINITE);
        Blizzard::Thread::Close(this->m_thread);
        this->m_thread = nullptr;
    }
}

// OFFSET: 0x0044B860
uint32_t System_Thread::ThreadWorker::Executive(void* param) {
    ThreadWorker* worker = static_cast<ThreadWorker*>(param);

    Blizzard::Lock::EventSet(worker->m_readyEvent.m_event);
    Blizzard::Lock::EventWaitForever(worker->m_wakeEvent.m_event);

    while (worker->m_taskFunction) {
        worker->m_task->m_state = 1;
        worker->m_task->m_result = worker->m_taskFunction(worker->m_taskParam);

        if (worker->m_thread) {
            SetPriorityLevel(worker->m_thread, worker->m_priority);
        }

        if (worker->m_thread) {
            int32_t affinity = worker->m_affinity;

            if (affinity > 0) {
                SetThreadAffinityMask(worker->m_thread->m_handle, affinity);
            } else {
                Blizzard::Hardware::GetProcessorFeatures();

                if (affinity <= static_cast<int32_t>(System_Hardware::s_processorFeatures.m_processorCount)) {
                    SetThreadAffinityMask(worker->m_thread->m_handle, affinity);
                }
            }
        }

        worker->m_task->m_state = -1;
        Blizzard::Thread::Close(worker->m_task);
        worker->m_task = nullptr;
        worker->m_taskFunction = nullptr;
        worker->m_taskParam = nullptr;

        {
            Blizzard::Lock::ScopedMutex lock(worker->m_pool->m_mutex.m_mutex);
            worker->m_pool->m_idle.push_back(worker);
        }

        Blizzard::Lock::EventSet(worker->m_readyEvent.m_event);
        Blizzard::Lock::EventWaitForever(worker->m_wakeEvent.m_event);
    }

    Blizzard::Lock::EventSet(worker->m_readyEvent.m_event);

    return 0;
}

// OFFSET: 0x00450D80
bool System_Thread::ThreadWorker::Start(Blizzard::Thread::ThreadPoolRecord* pool, int32_t flags, int32_t stackSize) {
    if (!Blizzard::Thread::Create(Executive, this, &this->m_thread, "Blizzard Thread Pool Worker", flags, stackSize)) {
        return false;
    }

    if (this->m_thread) {
        GetPriorityLevel(this->m_thread, &this->m_priority);
    }

    if (this->m_thread) {
        DWORD_PTR processMask;
        DWORD_PTR systemMask;
        GetProcessAffinityMask(this->m_thread->m_handle, &processMask, &systemMask);
        this->m_affinity = static_cast<int32_t>(systemMask);
    }

    this->m_pool = pool;

    return true;
}

