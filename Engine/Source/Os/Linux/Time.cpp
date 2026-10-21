#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <ctime>
#include "Engine/Source/Os/Time.h"

// Minutes east of UTC, as -TIME_ZONE_INFORMATION::Bias. Like Bias, this excludes daylight saving time.
int32_t OsGetTimeZone() {
    tzset();
    return static_cast<int32_t>(-timezone / 60);
}

void OsGetTimeStamp(char* buffer, uint32_t size) {
    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    strftime(buffer, size, "%m%d%y_%H%M%S", &local);
}

#endif
