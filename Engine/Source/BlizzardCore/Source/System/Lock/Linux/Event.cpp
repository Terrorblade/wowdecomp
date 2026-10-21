#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cerrno>
#include <cstdint>
#include <ctime>
#include <new>
#include <pthread.h>
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"

// Auto-reset event with the semantics of CreateEvent(nullptr, FALSE, FALSE, nullptr): a successful wait consumes the
// signal, and setting an already signalled event has no further effect.
struct Blizzard::Lock::EventRecord {
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    bool m_signalled;
};

namespace {

Blizzard::Lock::EventRecord* NewEvent() {
    Blizzard::Lock::EventRecord* record = new (std::nothrow) Blizzard::Lock::EventRecord;

    if (!record) {
        return nullptr;
    }

    pthread_mutex_init(&record->m_mutex, nullptr);
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&record->m_cond, &attr);
    pthread_condattr_destroy(&attr);
    record->m_signalled = false;

    return record;
}

}

void Blizzard::Lock::EventCreate(Event& event) {
    event = NewEvent();
}

void Blizzard::Lock::EventDestroy(Event& event) {
    if (!event) {
        return;
    }

    pthread_cond_destroy(&event->m_cond);
    pthread_mutex_destroy(&event->m_mutex);
    delete event;
    event = nullptr;
}

void Blizzard::Lock::EventSet(Event& event) {
    if (!event) {
        return;
    }

    pthread_mutex_lock(&event->m_mutex);
    event->m_signalled = true;
    pthread_cond_signal(&event->m_cond);
    pthread_mutex_unlock(&event->m_mutex);
}

void Blizzard::Lock::EventWaitForever(Event& event) {
    if (!event) {
        return;
    }

    pthread_mutex_lock(&event->m_mutex);

    while (!event->m_signalled) {
        pthread_cond_wait(&event->m_cond, &event->m_mutex);
    }

    event->m_signalled = false;
    pthread_mutex_unlock(&event->m_mutex);
}

Blizzard::Lock::Event* Blizzard::Lock::EventWait(uint32_t timeout, Event& event) {
    if (!event) {
        return nullptr;
    }

    if (timeout == INFINITE) {
        EventWaitForever(event);
        return &event;
    }

    timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout / 1000;
    deadline.tv_nsec += static_cast<long>(timeout % 1000) * 1000000;

    if (deadline.tv_nsec >= 1000000000) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&event->m_mutex);

    while (!event->m_signalled) {
        if (pthread_cond_timedwait(&event->m_cond, &event->m_mutex, &deadline) == ETIMEDOUT) {
            break;
        }
    }

    bool signalled = event->m_signalled;
    event->m_signalled = false;
    pthread_mutex_unlock(&event->m_mutex);

    return signalled ? &event : nullptr;
}

Blizzard::Lock::EventObject::EventObject() {
    this->m_event = NewEvent();
}

Blizzard::Lock::EventObject::~EventObject() {
    EventDestroy(this->m_event);
}

#endif
