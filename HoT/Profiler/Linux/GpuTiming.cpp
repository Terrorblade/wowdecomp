#include "HoT/Profiler/GpuTiming.h"

// There is no D3D9 device here, so GPU timing reports as unavailable like a device without timestamp queries.
namespace HoT::Profiler::GpuTiming {

bool Available() {
    return false;
}

void Release() {
}

bool BeginFrame(bool enabled) {
    return true;
}

void EndFrame() {
}

int32_t BeginMark(Zone zone) {
    return -1;
}

void EndMark(int32_t mark) {
}

void DeviceMemory(double& availableTexMB, double& texCacheMB, double& texCacheLimitMB) {
}

}
