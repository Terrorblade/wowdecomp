#include <cstring>
#include "Engine/Source/Os/Linux/OsPosix.h"

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

// Shared by Linux and macOS since what matters is the processor. arm64 has no cpuid, so it reports a processor
// with just the SSE and SSE2 that sse2neon translates.
bool OsPosixCpuid(uint32_t leaf, uint32_t subleaf, uint32_t regs[4]) {
#if defined(__x86_64__) || defined(__i386__)
    __cpuid_count(leaf, subleaf, regs[0], regs[1], regs[2], regs[3]);
    return true;
#else
    regs[0] = 0;
    regs[1] = 0;
    regs[2] = 0;
    regs[3] = 0;

    if (leaf == 0) {
        regs[0] = 1;
        memcpy(&regs[1], "ARM6", 4);
        memcpy(&regs[3], "4 NE", 4);
        memcpy(&regs[2], "ON  ", 4);
    } else if (leaf == 1) {
        regs[3] = 0x2000000 | 0x4000000;
    } else if (leaf == 0x80000000) {
        regs[0] = 0x80000001;
    }

    return true;
#endif
}
