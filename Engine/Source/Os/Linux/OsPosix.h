#ifndef ENGINE_OS_LINUX_OSPOSIX_H
#define ENGINE_OS_LINUX_OSPOSIX_H

// The operations where Linux and macOS differ under the shared POSIX code. Linux/OsPosix*.cpp implement them for
// Linux, Mac/OsPosix*.cpp of the same names replace those on macOS.

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ucontext.h>
#include <unistd.h>

// Threads. Ids are the kernel thread ids Linux uses for priority and affinity, and stable unique numbers on macOS.
uint32_t OsPosixThreadId();
void OsPosixSetThreadName(const char* name);
bool OsPosixSetThreadNice(pthread_t thread, uint32_t tid, int32_t nice);
bool OsPosixGetThreadNice(pthread_t thread, uint32_t tid, int32_t* nice);
bool OsPosixSetThreadAffinity(pthread_t thread, uint64_t mask);
bool OsPosixSetProcessAffinity(uint64_t mask);
uint64_t OsPosixProcessAffinity();
uint32_t OsPosixThreadCount();

// Windows takes a requested stack size as the initial commit and still reserves the executable's 1 MB default, and the
// client's code needs that depth. POSIX stacks are fixed at creation (512 KB by default for macOS secondary threads),
// so a thread gets the largest of the request, 1 MB and the system default, in whole pages.
inline void OsPosixSetThreadStackSize(pthread_attr_t* attr, size_t requested) {
    size_t stack = 0;
    pthread_attr_getstacksize(attr, &stack);

    if (stack < 1024 * 1024) {
        stack = 1024 * 1024;
    }

    if (stack < requested) {
        stack = requested;
    }

    size_t page = static_cast<size_t>(getpagesize());
    pthread_attr_setstacksize(attr, (stack + page - 1) / page * page);
}

// Counting semaphores starting at initial. Linux has unnamed POSIX semaphores, macOS only declares them (sem_init
// fails with ENOSYS), so it uses dispatch semaphores.
struct OsPosixSemaphore;
OsPosixSemaphore* OsPosixSemaphoreCreate(uint32_t initial);
void OsPosixSemaphoreDestroy(OsPosixSemaphore* semaphore);
void OsPosixSemaphoreWait(OsPosixSemaphore* semaphore);
void OsPosixSemaphorePost(OsPosixSemaphore* semaphore);

// Condition variables timed against CLOCK_MONOTONIC deadlines.
void OsPosixCondInit(pthread_cond_t* cond);
int32_t OsPosixCondTimedWait(pthread_cond_t* cond, pthread_mutex_t* mutex, const timespec* monotonicDeadline);

// Sockets and descriptors are close-on-exec and never raise SIGPIPE.
int32_t OsPosixSocket(int32_t domain, int32_t type, int32_t protocol, bool nonBlocking);
int32_t OsPosixAccept(int32_t fd, sockaddr* address, socklen_t* length);

// A descriptor pair for waking a poll() loop, the same descriptor twice where one can do both.
struct OsPosixWake {
    int32_t m_readFd;
    int32_t m_writeFd;
};

bool OsPosixWakeCreate(OsPosixWake* wake);
void OsPosixWakeSignal(const OsPosixWake* wake);
void OsPosixWakeDrain(const OsPosixWake* wake);
void OsPosixWakeDestroy(OsPosixWake* wake);

// Process.
bool OsPosixExecutablePath(char* buffer, size_t size);
bool OsPosixGameDirectory(char* buffer, size_t size);
int32_t OsPosixProcessArguments(char** argv, int32_t maxArgs, char* buffer, size_t size);
bool OsPosixDebuggerAttached();
const char* OsPosixErrorString(int32_t error, char* buffer, size_t size);
bool OsPosixRandomBytes(void* buffer, size_t size);
void OsPosixMemoryStatus(uint64_t* total, uint64_t* available);

// The OsType value of the running system.
int32_t OsPosixOsType();

// Files. An empty path takes the times of dirfd itself.
void OsPosixFileTimes(int32_t dirfd, const char* path, const struct stat& st, timespec* created, timespec* modified, timespec* accessed);

// Address space. MapAt maps read/write memory at exactly address or fails, it never replaces an existing mapping.
// EnumerateMappings reports the mappings in address order until the callback returns false.
void* OsPosixMapAt(uintptr_t address, size_t bytes);
void OsPosixEnumerateMappings(bool (*callback)(void* param, uintptr_t start, uintptr_t end), void* param);

// Copies size bytes of this process, failing instead of faulting on unmapped memory.
bool OsPosixReadMemory(const void* address, void* out, size_t size);

// Loaded modules. A null module is the main program, anything else a dlopen handle.
struct OsPosixSegment {
    uintptr_t m_start;
    uintptr_t m_size;
    bool m_writable;
    bool m_executable;
};

int32_t OsPosixModuleSegments(void* module, OsPosixSegment* segments, int32_t maxSegments);
bool OsPosixModulePath(void* module, char* buffer, size_t size);
void OsPosixEnumerateModules(void (*callback)(void* param, const char* path, uintptr_t low, uintptr_t high), void* param);

// Signal contexts.
void OsPosixContextFrame(const ucontext_t* context, uintptr_t* pc, uintptr_t* fp, uintptr_t* sp);
uint32_t OsPosixContextCodeSegment(const ucontext_t* context);
bool OsPosixContextWriteFault(const ucontext_t* context);
const char* OsPosixContextRegistersTitle();
void OsPosixLogContextRegisters(void (*logLineProc)(void*, const char*, ...), void* logLineProcParam, const ucontext_t* context);

// cpuid, false where the processor has none.
bool OsPosixCpuid(uint32_t leaf, uint32_t subleaf, uint32_t regs[4]);

#endif
