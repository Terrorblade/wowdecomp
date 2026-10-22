#if defined(WOWDECOMP_PLATFORM_POSIX)

#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"

void Blizzard::Lock::MutexCreate(Mutex& mutex) {
    InitializeCriticalSection(&mutex);
}

void Blizzard::Lock::MutexEnter(Mutex& mutex) {
    EnterCriticalSection(&mutex);
}

void Blizzard::Lock::MutexLeave(Mutex& mutex) {
    LeaveCriticalSection(&mutex);
}

void Blizzard::Lock::MutexDestroy(Mutex& mutex) {
    DeleteCriticalSection(&mutex);
}

Blizzard::Lock::ScopedMutex::ScopedMutex(Mutex& mutex) {
    this->m_mutex = &mutex;
    MutexEnter(mutex);
}

Blizzard::Lock::ScopedMutex::~ScopedMutex() {
    MutexLeave(*this->m_mutex);
}

Blizzard::Lock::RMutex::RMutex() {
    MutexCreate(this->m_mutex);
}

Blizzard::Lock::RMutex::~RMutex() {
    MutexDestroy(this->m_mutex);
}

#endif
