#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <pthread.h>
#include "Engine/Source/Os/Tls.h"

// pthread_key_t is an unsigned int index, like a Windows TLS index. 0xFFFFFFFF is TLS_OUT_OF_INDEXES.
uint32_t OsTlsAlloc() {
    pthread_key_t key;

    if (pthread_key_create(&key, nullptr)) {
        return 0xFFFFFFFF;
    }

    return static_cast<uint32_t>(key);
}

void OsTlsFree(uint32_t index) {
    pthread_key_delete(static_cast<pthread_key_t>(index));
}

void* OsTlsGetValue(uint32_t index) {
    return pthread_getspecific(static_cast<pthread_key_t>(index));
}

int32_t OsTlsSetValue(uint32_t index, void* value) {
    return pthread_setspecific(static_cast<pthread_key_t>(index), value) == 0;
}

#endif
