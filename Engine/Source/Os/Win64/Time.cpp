
#include <cstdint>
#include <ctime>
#include <windows.h>
#include "Engine/Source/Os/Time.h"

// OFFSET: 0x0086D450
int32_t OsGetTimeZone() {
    TIME_ZONE_INFORMATION information;

    if (GetTimeZoneInformation(&information) == TIME_ZONE_ID_INVALID) {
        return 0;
    }

    return -information.Bias;
}

// OFFSET: 0x0086D490
void OsGetTimeStamp(char* buffer, uint32_t size) {
    __time32_t now;
    _time32(&now);
    strftime(buffer, size, "%m%d%y_%H%M%S", _localtime32(&now));
}

