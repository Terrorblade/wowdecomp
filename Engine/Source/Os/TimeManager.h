#ifndef ENGINE_SOURCE_OS_TIMEMANAGER_H
#define ENGINE_SOURCE_OS_TIMEMANAGER_H

#include <cstdint>
constexpr int32_t TIME_MILLISECONDS_PER_SECOND = 1000;
constexpr float TIME_MILLISECONDS_PER_SECOND_FLOAT = 1000.0f;
constexpr float TIME_SECONDS_PER_MILLISECOND_FLOAT = 0.001f;

class OsTimeManager {
    public:
    enum TimingMethod {
        NotSet = -1,
        BestAvailable = 0,
        SystemMethod1 = 1,
        SystemMethod2 = 2,
    };

    enum TimingTestError : uint32_t {
        TimingTestError_None = 0,
        TimingTestError_NoPerformanceCounter = 1,
        TimingTestError_ZeroFrequency = 2,
        TimingTestError_TickDrift = 3,
        TimingTestError_NotMonotonic = 4,
        TimingTestError_MethodForced = 5,
    };

    /* 0000 */ double m_scaleToMs;
    /* 0008 */ OsTimeManager::TimingMethod m_timingMethod;
    /* 000C */ uint32_t m_timingTestError;
    /* 0010 */ int64_t m_performanceFrequency;
    /* 0018 */ double m_timeBegin;

    OsTimeManager(OsTimeManager::TimingMethod method);
    OsTimeManager::TimingMethod Calibrate();
    uint64_t Snapshot();
};

uint32_t OsTimeGetTestError();
OsTimeManager::TimingMethod OsTimeGetTimingMethod();
const char* GetTimingMethodName(OsTimeManager::TimingMethod method);
uint32_t OsGetAsyncTimeMs();
uint64_t OsGetAsyncTimeClocks();
int64_t OsGetAsyncClocksPerSecond();
void OsTimeManagerCreate(OsTimeManager::TimingMethod method);
void OsTimeManagerDestroy();
void OsSleep(uint32_t milliseconds);

#endif
