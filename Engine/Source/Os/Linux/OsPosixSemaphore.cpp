#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <cstdlib>
#include <semaphore.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

struct OsPosixSemaphore {
    sem_t m_sem;
};

OsPosixSemaphore* OsPosixSemaphoreCreate(uint32_t initial) {
    OsPosixSemaphore* semaphore = static_cast<OsPosixSemaphore*>(malloc(sizeof(OsPosixSemaphore)));

    if (semaphore && sem_init(&semaphore->m_sem, 0, initial) != 0) {
        free(semaphore);
        return nullptr;
    }

    return semaphore;
}

void OsPosixSemaphoreDestroy(OsPosixSemaphore* semaphore) {
    sem_destroy(&semaphore->m_sem);
    free(semaphore);
}

void OsPosixSemaphoreWait(OsPosixSemaphore* semaphore) {
    while (sem_wait(&semaphore->m_sem) == -1 && errno == EINTR) {
    }
}

void OsPosixSemaphorePost(OsPosixSemaphore* semaphore) {
    sem_post(&semaphore->m_sem);
}

#endif
