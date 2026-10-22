#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include "Engine/Source/BlizzardCore/Source/System/Hardware/Hardware.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/Os/Linux/OsPosix.h"

Blizzard::Hardware::ProcessorFeatures System_Hardware::s_processorFeatures;

bool System_Hardware::s_initialized;

Blizzard::Lock::Atomic System_Hardware::s_initCount;

// x86_64 always has CPUID, so the EFLAGS.ID probe of the Windows code is skipped. arm64 gets the OsPosixCpuid stand-in.
static void Cpuid(uint32_t regs[4], uint32_t leaf, uint32_t subleaf) {
    OsPosixCpuid(leaf, subleaf, regs);
}

bool System_Hardware::ReadCPUID(CPUIDInfo* info) {
    memset(info, 0, sizeof(*info));
    bool result = false;
    uint32_t maxLeaf = 0;
    uint32_t maxExtLeaf = 0;

    uint32_t regs[4];
    Cpuid(regs, 0, 0);

    if (!regs[0]) {
        return result;
    }

    maxLeaf = regs[0];
    info->m_maxLeaf = regs[0];
    info->m_vendor[0] = regs[1];
    info->m_vendor[1] = regs[3];
    info->m_vendor[2] = regs[2];

    Cpuid(regs, 0x80000000, 0);

    if (regs[0] <= 0x80000000) {
        return result;
    }

    maxExtLeaf = regs[0];
    info->m_maxExtLeaf = regs[0];
    result = true;

    if (maxLeaf >= 4) {
        Cpuid(regs, 4, 0);
        info->m_leaf4Eax = regs[0];
    }

    Cpuid(regs, 1, 0);
    info->m_leaf1Edx = regs[3];
    info->m_leaf1Ebx = regs[1];

    if (maxExtLeaf >= 0x80000008) {
        Cpuid(regs, 0x80000008, 0);
        info->m_ext8Ecx = regs[2];
    }

    if (maxExtLeaf >= 0x80000004) {
        Cpuid(regs, 0x80000004, 0);
        info->m_ext4Eax = regs[0];
    }

    Cpuid(regs, 0x80000001, 0);
    info->m_ext1Edx = regs[3];
    info->m_ext1Ecx = regs[2];

    return result;
}

void System_Hardware::InitProcessorFeatures() {
    uint32_t features = 0;
    long processors = sysconf(_SC_NPROCESSORS_ONLN);
    s_processorFeatures.m_processorCount = processors > 0 ? static_cast<uint32_t>(processors) : 0;

    if (processors <= 0) {
        s_processorFeatures.m_processorCount = 1;
    }

    s_processorFeatures.m_coreCount = 1;

    CPUIDInfo info;

    if (ReadCPUID(&info)) {
        if (info.m_leaf1Edx & 0x10) {
            features = 0x1;
        }

        if (info.m_leaf1Edx & 0x800000) {
            features |= 0x2;
        }

        if (info.m_leaf1Edx & 0x2000000) {
            features |= 0x4;
        }

        if (info.m_ext1Edx & 0x80000000) {
            features |= 0x8;
        }

        const char* vendors = "GenuineIntel" "AuthenticAMD" "CyrixInstead" "CentaurHalls";
        const char* vendor = reinterpret_cast<const char*>(info.m_vendor);

        if (!strncmp(vendor, vendors, 12)) {
            s_processorFeatures.m_vendor = 1;

            if (info.m_leaf1Edx & 0x4000000) {
                features |= 0x10;
            }

            if (info.m_maxLeaf >= 4) {
                uint8_t cores = static_cast<uint8_t>(info.m_leaf4Eax >> 26);

                if (cores) {
                    s_processorFeatures.m_features = features | 0x40;
                    s_processorFeatures.m_coreCount = cores + 1;
                    return;
                }
            }
        } else if (!strncmp(vendor, vendors + 12, 12)) {
            s_processorFeatures.m_vendor = 2;

            if (info.m_leaf1Edx & 0x4000000) {
                features |= 0x10;
            }

            uint8_t cores = static_cast<uint8_t>(info.m_ext8Ecx);

            if (cores) {
                s_processorFeatures.m_features = features | 0x40;
                s_processorFeatures.m_coreCount = cores + 1;
                return;
            }
        } else if (!strncmp(vendor, vendors + 24, 12)) {
        } else if (!strncmp(vendor, vendors + 36, 12)) {
        }
    }

    s_processorFeatures.m_features = features;
}

uint32_t Blizzard::Hardware::GetProcessorFeatures() {
    if (!System_Hardware::s_initialized) {
        if (System_Hardware::s_initCount.Increment() == 1) {
            System_Hardware::InitProcessorFeatures();
            System_Hardware::s_initialized = true;
            return System_Hardware::s_processorFeatures.m_features;
        }

        do {
            usleep(1000);
        } while (!System_Hardware::s_initialized);
    }

    return System_Hardware::s_processorFeatures.m_features;
}

#endif
