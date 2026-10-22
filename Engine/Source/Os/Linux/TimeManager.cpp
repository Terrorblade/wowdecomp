#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <cstdint>
#include <ctime>
#include <new>
#include <sched.h>
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"

// QueryPerformanceCounter maps to CLOCK_MONOTONIC in nanoseconds (frequency 1e9) and GetTickCount to the same clock in
// milliseconds, truncated to 32 bits.

static OsTimeManager* s_OsTimeMgr;

static const int64_t s_monotonicFrequency = 1000000000;

static bool ReadMonotonic(int64_t* count) {
    timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now)) {
        return false;
    }

    *count = static_cast<int64_t>(now.tv_sec) * s_monotonicFrequency + now.tv_nsec;
    return true;
}

static int64_t MonotonicCount() {
    int64_t count = 0;
    ReadMonotonic(&count);
    return count;
}

static uint32_t TickCount() {
    return static_cast<uint32_t>(MonotonicCount() / 1000000);
}

// The Windows calibration checks that QueryPerformanceCounter is monotonic across processors and agrees with
// GetTickCount. CLOCK_MONOTONIC guarantees both, so only its availability is tested.
OsTimeManager::TimingMethod OsTimeManager::Calibrate() {
    timespec resolution;

    if (clock_getres(CLOCK_MONOTONIC, &resolution)) {
        this->m_performanceFrequency = 0;
        this->m_timingTestError = 1;
        return SystemMethod1;
    }

    this->m_performanceFrequency = s_monotonicFrequency;

    int64_t count;

    if (!ReadMonotonic(&count)) {
        this->m_timingTestError = 2;
        return SystemMethod1;
    }

    this->m_timingTestError = 0;

    return SystemMethod2;
}

uint32_t OsTimeGetTestError() {
    return s_OsTimeMgr->m_timingTestError;
}

OsTimeManager::TimingMethod OsTimeGetTimingMethod() {
    return s_OsTimeMgr->m_timingMethod;
}

const char* GetTimingMethodName(OsTimeManager::TimingMethod method) {
    switch (method) {
        case OsTimeManager::NotSet:
            return "[Not Set]";

        case OsTimeManager::BestAvailable:
            return "[Best Available]";

        case OsTimeManager::SystemMethod1:
            return "GetTickCount";

        case OsTimeManager::SystemMethod2:
            return "QueryPerformanceCounter";

        default:
            return "[Unknown]";
    }
}

uint64_t OsTimeManager::Snapshot() {
    switch (this->m_timingMethod) {
        case SystemMethod1:
            break;

        case SystemMethod2: {
            int64_t count = MonotonicCount();
            return static_cast<int64_t>(static_cast<double>(count) * this->m_scaleToMs + this->m_timeBegin);
        }

        default:
            break;
    }

    int64_t ticks = TickCount();
    return static_cast<int64_t>(static_cast<double>(ticks) * this->m_scaleToMs + this->m_timeBegin);
}

uint32_t OsGetAsyncTimeMs() {
    return static_cast<uint32_t>(s_OsTimeMgr->Snapshot());
}

uint64_t OsGetAsyncTimeClocks() {
    switch (s_OsTimeMgr->m_timingMethod) {
        case OsTimeManager::SystemMethod1:
            break;

        case OsTimeManager::SystemMethod2:
            return MonotonicCount();

        default:
            break;
    }

    return TickCount();
}

int64_t OsGetAsyncClocksPerSecond() {
    switch (s_OsTimeMgr->m_timingMethod) {
        case OsTimeManager::SystemMethod1:
            break;

        case OsTimeManager::SystemMethod2:
            return s_OsTimeMgr->m_performanceFrequency;

        default:
            break;
    }

    return 1000;
}

OsTimeManager::OsTimeManager(OsTimeManager::TimingMethod method) {
    this->m_timingMethod = NotSet;

    OsTimeManager::TimingMethod calibrated = this->Calibrate();
    if (method != BestAvailable && calibrated != method) {
        if (method == SystemMethod2 && calibrated == SystemMethod1) {
            this->m_timingTestError = 5;
        }

        calibrated = method;
    }

    // Forcing SystemMethod2 without a working clock would divide by zero below.
    if (calibrated == SystemMethod2 && !this->m_performanceFrequency) {
        calibrated = SystemMethod1;
    }

    this->m_timingMethod = calibrated;

    int64_t clocksPerSecond;
    switch (calibrated) {
        case SystemMethod1:
            clocksPerSecond = 1000;
            break;

        case SystemMethod2:
            clocksPerSecond = this->m_performanceFrequency;
            break;

        default:
            clocksPerSecond = 1000;
            break;
    }

    this->m_scaleToMs = 1000.0 / static_cast<double>(clocksPerSecond);
    this->m_timeBegin = 0.0;
}

void OsTimeManagerCreate(OsTimeManager::TimingMethod method) {
    s_OsTimeMgr = new (SMemAlloc(sizeof(OsTimeManager), __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY)) OsTimeManager(method);
}

void OsTimeManagerDestroy() {
    if (s_OsTimeMgr) {
        delete s_OsTimeMgr;
        s_OsTimeMgr = nullptr;
    }
}

// Sleep(0) yields the rest of the time slice.
void OsSleep(uint32_t milliseconds) {
    if (!milliseconds) {
        sched_yield();
        return;
    }

    timespec request;
    request.tv_sec = milliseconds / 1000;
    request.tv_nsec = static_cast<long>(milliseconds % 1000) * 1000000;

    while (nanosleep(&request, &request) && errno == EINTR) {
    }
}

#endif
