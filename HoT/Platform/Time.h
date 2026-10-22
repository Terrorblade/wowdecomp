#ifndef HOT_PLATFORM_TIME_H
#define HOT_PLATFORM_TIME_H

#include <cstdint>

namespace HoT::Platform {

struct LocalTime {
    int32_t m_year = 0;
    int32_t m_month = 0;
    int32_t m_day = 0;
    int32_t m_hour = 0;
    int32_t m_minute = 0;
    int32_t m_second = 0;
    int32_t m_millisecond = 0;
};

// High resolution monotonic counter, TicksPerSecond() ticks make a second.
int64_t Ticks();
int64_t TicksPerSecond();
double TicksToMs(int64_t ticks);

uint64_t MonotonicMs();

LocalTime LocalNow();

}

#endif
