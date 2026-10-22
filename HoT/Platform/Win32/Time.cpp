#include "HoT/Platform/Time.h"
#include <windows.h>

namespace HoT::Platform {

int64_t Ticks() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

int64_t TicksPerSecond() {
    static const int64_t s_frequency = [] {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart;
    }();

    return s_frequency;
}

double TicksToMs(int64_t ticks) {
    static const double s_ticksPerMs = TicksPerSecond() / 1000.0;
    return ticks / s_ticksPerMs;
}

uint64_t MonotonicMs() {
    return GetTickCount64();
}

LocalTime LocalNow() {
    SYSTEMTIME st;
    GetLocalTime(&st);

    LocalTime local;
    local.m_year = st.wYear;
    local.m_month = st.wMonth;
    local.m_day = st.wDay;
    local.m_hour = st.wHour;
    local.m_minute = st.wMinute;
    local.m_second = st.wSecond;
    local.m_millisecond = st.wMilliseconds;
    return local;
}

}
