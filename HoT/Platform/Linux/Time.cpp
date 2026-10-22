#include "HoT/Platform/Time.h"
#include <ctime>

namespace HoT::Platform {

// Ticks are CLOCK_MONOTONIC nanoseconds.
int64_t Ticks() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}

int64_t TicksPerSecond() {
    return 1000000000;
}

double TicksToMs(int64_t ticks) {
    return ticks / 1000000.0;
}

uint64_t MonotonicMs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000ULL + static_cast<uint64_t>(ts.tv_nsec / 1000000);
}

LocalTime LocalNow() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    tm parts;
    localtime_r(&ts.tv_sec, &parts);

    LocalTime local;
    local.m_year = parts.tm_year + 1900;
    local.m_month = parts.tm_mon + 1;
    local.m_day = parts.tm_mday;
    local.m_hour = parts.tm_hour;
    local.m_minute = parts.tm_min;
    local.m_second = parts.tm_sec;
    local.m_millisecond = static_cast<int32_t>(ts.tv_nsec / 1000000);
    return local;
}

}
