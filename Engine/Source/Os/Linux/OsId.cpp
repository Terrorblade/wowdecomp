#include <cstdint>
#include "Engine/Source/Os/System.h"

// HoT: the server picks the required client build by the platform sent at login.
#if defined(__aarch64__)
uint32_t OsGetProcessorId() {
    return 0x413634; // A64
}

uint32_t OsGetOsId() {
    return 0x4C7841; // LxA
}
#else
uint32_t OsGetProcessorId() {
    return 0x783634; // x64
}

uint32_t OsGetOsId() {
    return 0x4C783634; // Lx64
}
#endif
