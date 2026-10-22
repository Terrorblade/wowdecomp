#include <cstdint>
#include "Engine/Source/Os/System.h"

// HoT: the server picks the required client build by the platform sent at login.
#if defined(__aarch64__)
uint32_t OsGetProcessorId() {
    return 'A64';
}

uint32_t OsGetOsId() {
    return 'LxA';
}
#else
uint32_t OsGetProcessorId() {
    return 'x64';
}

uint32_t OsGetOsId() {
    return 'Lx64';
}
#endif
