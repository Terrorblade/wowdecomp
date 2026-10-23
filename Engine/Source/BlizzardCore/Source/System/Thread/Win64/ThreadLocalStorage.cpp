
#include <cstdint>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/ThreadLocalStorage.h"

// OFFSET: 0x00433760
bool System_Thread::AllocateLocalStorage(Blizzard::Thread::TLSSlot* slot, Blizzard::Thread::TLSDestructor destructor) {
    InitThreadSystem();
    slot->m_key = ~TlsAlloc();

    if (~slot->m_key == TLS_OUT_OF_INDEXES) {
        return false;
    }

    slot->m_destructor = destructor;
    Blizzard::Lock::MutexEnter(s_mutex);
    s_slotList[s_slotListUsed++] = slot;
    Blizzard::Lock::MutexLeave(s_mutex);

    return true;
}

// OFFSET: 0x0042E700
void System_Thread::RunTLSDestructors() {
    if (!s_initialized) {
        return;
    }

    Blizzard::Lock::ScopedMutex lock(s_mutex);

    for (uint32_t i = 0; i < s_slotListUsed; i++) {
        Blizzard::Thread::TLSSlot* slot = s_slotList[i];
        Blizzard::Thread::TLSDestructor destructor = slot->m_destructor;

        if (destructor) {
            void* value = TlsGetValue(~slot->m_key);

            if (value) {
                destructor(value);
            }
        }
    }
}

// OFFSET: 0x00453720
void* Blizzard::Thread::RegisterLocalStorage(TLSSlot* slot, void* (*constructor)(void*), void* param, TLSDestructor destructor) {
    void* value;

    if (!slot->m_key) {
        if (!System_Thread::AllocateTLSSlot(slot, destructor)) {
            return nullptr;
        }
    } else {
        value = TlsGetValue(~slot->m_key);

        if (value) {
            return value;
        }
    }

    value = constructor(param);
    System_Thread::InternalSetLocalStorage(slot, value);

    return value;
}

