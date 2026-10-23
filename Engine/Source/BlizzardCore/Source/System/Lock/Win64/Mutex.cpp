
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"

// OFFSET: 0x004283A0
void Blizzard::Lock::MutexCreate(Mutex& mutex) {
    InitializeCriticalSection(&mutex);
}

// OFFSET: 0x004283B0
void Blizzard::Lock::MutexEnter(Mutex& mutex) {
    EnterCriticalSection(&mutex);
}

// OFFSET: 0x004283C0
void Blizzard::Lock::MutexLeave(Mutex& mutex) {
    LeaveCriticalSection(&mutex);
}

// OFFSET: 0x004283D0
void Blizzard::Lock::MutexDestroy(Mutex& mutex) {
    DeleteCriticalSection(&mutex);
    mutex.DebugInfo = nullptr;
}

// INLINED
Blizzard::Lock::ScopedMutex::ScopedMutex(Mutex& mutex) {
    this->m_mutex = &mutex;
    MutexEnter(mutex);
}

// OFFSET: 0x0042B3A0
Blizzard::Lock::ScopedMutex::~ScopedMutex() {
    MutexLeave(*this->m_mutex);
}

// INLINED
Blizzard::Lock::RMutex::RMutex() {
    MutexCreate(this->m_mutex);
}

// OFFSET: 0x00421640
Blizzard::Lock::RMutex::~RMutex() {
    MutexDestroy(this->m_mutex);
}

