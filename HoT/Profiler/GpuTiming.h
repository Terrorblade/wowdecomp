#ifndef HOT_PROFILER_GPUTIMING_H
#define HOT_PROFILER_GPUTIMING_H

#include <cstdint>
#include "HoT/Profiler/Profiler.h"

// GPU timestamps for the profiler, only the D3D9 devices have them.
namespace HoT::Profiler::GpuTiming {

// Whether the current device can time frames at all.
bool Available();

void Release();

// Starts timing the frame when enabled, false when the device turned out to have no timestamp queries.
bool BeginFrame(bool enabled);
void EndFrame();

int32_t BeginMark(Zone zone);
void EndMark(int32_t mark);

// Texture memory the device reports, left alone where it reports nothing.
void DeviceMemory(double& availableTexMB, double& texCacheMB, double& texCacheLimitMB);

// Implemented by Profiler.cpp, a timed frame with its per zone times indexed by Zone, or a frame that was dropped.
void OnFrameTimed(double frameMs, const double* zoneMs);
void OnFrameDropped();

}

#endif
