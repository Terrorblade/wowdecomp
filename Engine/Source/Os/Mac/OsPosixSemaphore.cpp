#include <dispatch/dispatch.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

// An OsPosixSemaphore is a dispatch_semaphore_t.

OsPosixSemaphore* OsPosixSemaphoreCreate(uint32_t initial) {
    return reinterpret_cast<OsPosixSemaphore*>(dispatch_semaphore_create(static_cast<long>(initial)));
}

void OsPosixSemaphoreDestroy(OsPosixSemaphore* semaphore) {
    dispatch_release(reinterpret_cast<dispatch_semaphore_t>(semaphore));
}

void OsPosixSemaphoreWait(OsPosixSemaphore* semaphore) {
    dispatch_semaphore_wait(reinterpret_cast<dispatch_semaphore_t>(semaphore), DISPATCH_TIME_FOREVER);
}

void OsPosixSemaphorePost(OsPosixSemaphore* semaphore) {
    dispatch_semaphore_signal(reinterpret_cast<dispatch_semaphore_t>(semaphore));
}
