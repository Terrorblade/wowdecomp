#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cmath>
#include <cstdint>
#include <ctime>
#include <sched.h>
#include "Engine/Source/BlizzardCore/Source/System/Time/Time.h"

// Timestamps are nanoseconds since 2000-01-01 00:00:00 UTC (FromFileTime's offset); this is that epoch in Unix seconds.
static const int64_t s_unixEpoch2000 = 946684800;

const int32_t System_Time::s_daysBeforeMonth[13] = { 0, 31, 59, 90, 120, 151, 182, 212, 243, 273, 304, 334, 365 };

uint64_t System_Time::timeScaleCycles = 0xFFFFFFFFFFFFFFFF;

bool System_Time::s_usePerformanceCounter;

Blizzard::Lock::Atomic System_Time::s_tickLock;

int64_t System_Time::s_absBegin;

uint32_t System_Time::s_lastTickCount;

double System_Time::timeScaleMilliseconds;

Blizzard::Time::Timestamp System_Time::s_gmBegin;

uint64_t System_Time::s_tickCountHigh;

double System_Time::timeScaleNanoseconds;

double System_Time::timeScaleSeconds;

double System_Time::timeScaleMicroseconds;

// CLOCK_MONOTONIC stands in for QueryPerformanceCounter (frequency 1 GHz) and GetTickCount (milliseconds, 32-bit).
static int64_t QueryCounter() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}

static DWORD TickCount() {
    return static_cast<DWORD>(QueryCounter() / 1000000);
}

static Blizzard::Time::Timestamp RealtimeNow() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (static_cast<int64_t>(ts.tv_sec) - s_unixEpoch2000) * 1000000000 + ts.tv_nsec;
}

Blizzard::Time::Timestamp Blizzard::Time::FromFileTime(uint64_t fileTime) {
    if (fileTime <= 0x0077A5D6AA8BF851) {
        return static_cast<Timestamp>(0x8000000000000000);
    }

    if (fileTime <= 0x030701FFA04E87AE) {
        return static_cast<Timestamp>(fileTime * 100 + 0x5143382561530000);
    }

    return 0x7FFFFFFFFFFFFFFF;
}

double System_Time::MeasureDrift(int64_t frequency, LARGE_INTEGER* counter, DWORD* tickCount) {
    DWORD tick;
    int64_t now;

    do {
        sched_yield();
        tick = TickCount();
        now = QueryCounter();
    } while (tick == *tickCount);

    double counterSeconds = static_cast<double>(now - counter->QuadPart) / frequency;
    double tickSeconds = static_cast<double>(tick - *tickCount);
    counter->QuadPart = now;
    *tickCount = tick;

    return fabs(counterSeconds - tickSeconds * 0.001) / counterSeconds;
}

uint64_t System_Time::GetTickCount64() {
    while (s_tickLock.Increment() != 1) {
        s_tickLock.Decrement();
        sched_yield();
    }

    DWORD tick = TickCount();

    if (tick < s_lastTickCount) {
        s_tickCountHigh += 0x100000000;
    }

    s_lastTickCount = tick;
    s_tickLock.Decrement();

    return s_tickCountHigh + s_lastTickCount;
}

void Blizzard::Time::BreakTime(Timestamp time, TimeRec& rec) {
    int32_t nanosecond = static_cast<int32_t>(time % 1000000000);
    int64_t seconds = time / 1000000000;

    if (nanosecond < 0) {
        nanosecond += 1000000000;
        seconds--;
    }

    time_t unixTime = static_cast<time_t>(seconds + s_unixEpoch2000);
    tm broken = {};
    gmtime_r(&unixTime, &broken);

    rec.day = broken.tm_mday;
    rec.hour = broken.tm_hour;
    rec.minute = broken.tm_min;
    rec.second = broken.tm_sec;
    rec.nanosecond = nanosecond;
    rec.weekday = broken.tm_wday;
    rec.year = broken.tm_year + 1900;
    rec.month = broken.tm_mon + 1;

    bool leap = !(rec.year % 400) || (rec.year % 100 && !(rec.year % 4));
    rec.yearday = System_Time::s_daysBeforeMonth[rec.month - 1] + rec.day - 1;

    if (leap && rec.month > 2) {
        rec.yearday++;
    }
}

Blizzard::Time::Timestamp Blizzard::Time::MakeTime(const TimeRec& rec) {
    tm broken = {};
    broken.tm_year = rec.year - 1900;
    broken.tm_mon = rec.month - 1;
    broken.tm_mday = rec.day;
    broken.tm_hour = rec.hour;
    broken.tm_min = rec.minute;
    broken.tm_sec = rec.second;

    int64_t seconds = static_cast<int64_t>(timegm(&broken)) - s_unixEpoch2000;

    // Same range as the FILETIME path on Windows: about +-292 years around 2000 fit in a signed nanosecond count.
    if (seconds <= -9223372036LL) {
        return static_cast<Timestamp>(0x8000000000000000);
    }

    if (seconds >= 9223372036LL) {
        return 0x7FFFFFFFFFFFFFFF;
    }

    return seconds * 1000000000 + rec.nanosecond;
}

bool System_Time::CalibrateFrequency(LARGE_INTEGER* frequency) {
    frequency->QuadPart = 1000000000;

    DWORD start = TickCount();
    LARGE_INTEGER counter;
    counter.QuadPart = QueryCounter();

    DWORD tick;

    do {
        sched_yield();
        tick = TickCount();
        counter.QuadPart = QueryCounter();
    } while (tick == start);

    int32_t attempts = 0;

    while (MeasureDrift(frequency->QuadPart, &counter, &tick) >= 0.1) {
        if (++attempts >= 3) {
            return false;
        }
    }

    return true;
}

bool System_Time::InitFrequency(LARGE_INTEGER* frequency) {
    // CLOCK_MONOTONIC is always available and stable, like QueryPerformanceFrequency on Vista and later.
    frequency->QuadPart = 1000000000;
    return true;
}

bool System_Time::TimeInit() {
    LARGE_INTEGER frequency;
    s_usePerformanceCounter = InitFrequency(&frequency);

    if (!s_usePerformanceCounter) {
        timeScaleCycles = 0;
        return false;
    }

    timeScaleCycles = frequency.QuadPart;
    s_absBegin = QueryCounter();
    s_gmBegin = RealtimeNow();

    double scale = 1.0 / static_cast<double>(timeScaleCycles);
    timeScaleNanoseconds = 1000000000.0 * scale;
    timeScaleMicroseconds = 1000000.0 * scale;
    timeScaleMilliseconds = 1000.0 * scale;
    timeScaleSeconds = scale;

    return true;
}

Blizzard::Time::Timestamp Blizzard::Time::GetTimestamp() {
    if (System_Time::timeScaleCycles == 0xFFFFFFFFFFFFFFFF) {
        System_Time::TimeInit();
    }

    if (System_Time::s_usePerformanceCounter) {
        uint64_t elapsed = static_cast<uint64_t>(QueryCounter() - System_Time::s_absBegin);
        return System_Time::s_gmBegin + static_cast<Timestamp>(static_cast<double>(elapsed) * System_Time::timeScaleNanoseconds);
    }

    return RealtimeNow();
}

int64_t Blizzard::Time::Milliseconds() {
    if (System_Time::timeScaleCycles == 0xFFFFFFFFFFFFFFFF) {
        System_Time::TimeInit();
    }

    if (!System_Time::s_usePerformanceCounter) {
        return System_Time::GetTickCount64();
    }

    return static_cast<int64_t>(static_cast<double>(QueryCounter()) * System_Time::timeScaleMilliseconds);
}

#endif
