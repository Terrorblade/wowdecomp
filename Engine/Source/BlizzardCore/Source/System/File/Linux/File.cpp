#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"

void System_File::Stacked::Throttle(int64_t bytes, int64_t elapsed) {
    if (s_throttleDelay > 0) {
        Blizzard::Thread::Sleep(s_throttleDelay);
    }

    s_throttleDebt += static_cast<int64_t>(static_cast<double>(bytes) / static_cast<double>(s_throttleRate) * 1000000000.0f) - elapsed;

    if (s_throttleDebt < -1000000000) {
        s_throttleDebt = -1000000000;
        return;
    }

    if (s_throttleDebt > 10000000) {
        int32_t milliseconds = static_cast<int32_t>(s_throttleDebt / 1000000);
        s_throttleDebt -= milliseconds * 1000000;
        Blizzard::Thread::Sleep(milliseconds);
    }
}

#endif
