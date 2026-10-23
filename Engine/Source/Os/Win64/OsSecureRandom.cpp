
#include <cstdint>
#include <cstring>
#include <windows.h>
#include <wincrypt.h>
#include "Engine/Source/BlizzardCore/Source/Packages/Util/SHA1.h"
#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Storm/H/storm.h"

struct OsSecureRandomEntropy {
    /* 0000 */ DWORD m_tickCount;
    /* 0008 */ LARGE_INTEGER m_performanceCount;
    /* 0010 */ FILETIME m_systemTime;
    /* 0018 */ MEMORYSTATUSEX m_memoryStatus;
    /* 0058 */ ULARGE_INTEGER m_freeBytesAvailable;
    /* 0060 */ ULARGE_INTEGER m_totalBytes;
    /* 0068 */ ULARGE_INTEGER m_totalFreeBytes;
    /* 0070 */ DWORD m_processId;
    /* 0074 */ DWORD m_threadId;
    /* 0078 */ LONG m_counter;
    /* 007C */ void* m_heapPointer;
};

// STATIC: 0x00D41664
static volatile LONG s_counter;

// OFFSET: 0x0086D4C0
void IOsSecureRandom(uint8_t* buffer, uint32_t size) {
    typedef BOOL (WINAPI* GlobalMemoryStatusExFunc)(MEMORYSTATUSEX* buffer);

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");

    OsSecureRandomEntropy entropy;
    entropy.m_tickCount = GetTickCount();
    QueryPerformanceCounter(&entropy.m_performanceCount);
    GetSystemTimeAsFileTime(&entropy.m_systemTime);

    GlobalMemoryStatusExFunc globalMemoryStatusEx = reinterpret_cast<GlobalMemoryStatusExFunc>(GetProcAddress(kernel32, "GlobalMemoryStatusEx"));

    if (globalMemoryStatusEx) {
        entropy.m_memoryStatus.dwLength = sizeof(MEMORYSTATUSEX);
        globalMemoryStatusEx(&entropy.m_memoryStatus);
    } else {
        entropy.m_memoryStatus.dwLength = sizeof(MEMORYSTATUS);
        GlobalMemoryStatus(reinterpret_cast<MEMORYSTATUS*>(&entropy.m_memoryStatus));
    }

    GetDiskFreeSpaceExW(nullptr, &entropy.m_freeBytesAvailable, &entropy.m_totalBytes, &entropy.m_totalFreeBytes);

    entropy.m_processId = GetCurrentProcessId();
    entropy.m_threadId = GetCurrentThreadId();

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

// OFFSET: 0x0086D640
void OsSecureRandom(uint8_t* buffer, uint32_t size) {
    HCRYPTPROV provider;

    if (CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        if (CryptGenRandom(provider, size, buffer)) {
            CryptReleaseContext(provider, 0);
            return;
        }

        CryptReleaseContext(provider, 0);
    }

    IOsSecureRandom(buffer, size);
}

