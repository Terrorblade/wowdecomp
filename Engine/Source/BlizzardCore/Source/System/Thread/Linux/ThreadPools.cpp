#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstdlib>
#include <sched.h>
#include "Engine/Source/BlizzardCore/Source/System/Hardware/Hardware.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/ThreadLocalStorage.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Linux/ThreadLinux.h"

System_Thread::ThreadWorker::~ThreadWorker() {
    Blizzard::Lock::EventSet(this->m_wakeEvent.m_event);
    Blizzard::Lock::EventWaitForever(this->m_readyEvent.m_event);

    if (this->m_thread) {
        Blizzard::Lock::EventSet(this->m_wakeEvent.m_event);
        WaitLinuxHandle(GetLinuxHandle(this->m_thread->m_handle));
        Blizzard::Thread::Close(this->m_thread);
        this->m_thread = nullptr;
    }
}

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

            // The mask is sign-extended like the DWORD_PTR argument of SetThreadAffinityMask.
            if (affinity > 0) {
                SetLinuxAffinity(GetLinuxHandle(worker->m_thread->m_handle), static_cast<uint64_t>(static_cast<int64_t>(affinity)));
            } else {
                Blizzard::Hardware::GetProcessorFeatures();

                if (affinity <= static_cast<int32_t>(System_Hardware::s_processorFeatures.m_processorCount)) {
                    SetLinuxAffinity(GetLinuxHandle(worker->m_thread->m_handle), static_cast<uint64_t>(static_cast<int64_t>(affinity)));
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

bool System_Thread::ThreadWorker::Start(Blizzard::Thread::ThreadPoolRecord* pool, int32_t flags, int32_t stackSize) {
    if (!Blizzard::Thread::Create(Executive, this, &this->m_thread, "Blizzard Thread Pool Worker", flags, stackSize)) {
        return false;
    }

    if (this->m_thread) {
        GetPriorityLevel(this->m_thread, &this->m_priority);
    }

    if (this->m_thread) {
        // GetProcessAffinityMask's system mask: every online CPU, one bit each.
        uint64_t systemMask = 0;
        cpu_set_t set;
        CPU_ZERO(&set);

        if (sched_getaffinity(0, sizeof(set), &set) == 0) {
            for (int32_t i = 0; i < 64; i++) {
                if (CPU_ISSET(i, &set)) {
                    systemMask |= uint64_t(1) << i;
                }
            }
        }

        this->m_affinity = static_cast<int32_t>(systemMask);
    }

    this->m_pool = pool;

    return true;
}

#endif
