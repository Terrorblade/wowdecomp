#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include "Engine/Source/BlizzardCore/Source/Packages/Util/SHA1.h"
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Storm/H/storm.h"

// The POSIX counterparts of the values the Windows fallback hashes (tick count, performance counter, system time,
// memory status, disk space, process and thread ids, a fresh heap pointer, a counter).
struct OsSecureRandomEntropy {
    timespec m_monotonic;
    timespec m_realtime;
    timespec m_processTime;
    uint64_t m_totalMemory;
    uint64_t m_availableMemory;
    struct statvfs m_diskStatus;
    pid_t m_processId;
    pthread_t m_threadId;
    LONG m_counter;
    void* m_heapPointer;
};

static volatile LONG s_counter;

void IOsSecureRandom(uint8_t* buffer, uint32_t size) {
    OsSecureRandomEntropy entropy;
    memset(&entropy, 0, sizeof(entropy));

    clock_gettime(CLOCK_MONOTONIC, &entropy.m_monotonic);
    clock_gettime(CLOCK_REALTIME, &entropy.m_realtime);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &entropy.m_processTime);
    OsPosixMemoryStatus(&entropy.m_totalMemory, &entropy.m_availableMemory);
    statvfs(".", &entropy.m_diskStatus);

    entropy.m_processId = getpid();
    entropy.m_threadId = pthread_self();

    entropy.m_heapPointer = SMemAlloc(1, __FILE__, __LINE__, 0);

    if (entropy.m_heapPointer) {
        SMemFree(entropy.m_heapPointer, __FILE__, __LINE__, 0);
    }

    entropy.m_counter = InterlockedIncrement(&s_counter);

    SHA1_CONTEXT context;
    uint8_t digest[20];

    SHA1_Init(&context);
    SHA1_Update(&context, buffer, size);
    SHA1_Update(&context, reinterpret_cast<const uint8_t*>(&entropy), sizeof(entropy));
    SHA1_Final(digest, &context);

    while (true) {
        uint32_t count = size > 10 ? 10 : size;
        memcpy(buffer, digest, count);
        size -= count;
        buffer += count;

        if (!size) {
            break;
        }

        SHA1_Init(&context);
        SHA1_Update(&context, digest, sizeof(digest));
        SHA1_Final(digest, &context);
    }
}

// The system generator takes the place of CryptGenRandom, the hash-based fallback is kept for when it fails.
void OsSecureRandom(uint8_t* buffer, uint32_t size) {
    if (OsPosixRandomBytes(buffer, size)) {
        return;
    }

    IOsSecureRandom(buffer, size);
}

#endif
