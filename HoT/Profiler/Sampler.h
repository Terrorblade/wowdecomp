#ifndef HOT_PROFILER_SAMPLER_H
#define HOT_PROFILER_SAMPLER_H

#include <cstdint>
#include <string>
#include <vector>

// Statistical CPU sampling of the main thread while the profiler is open, only Windows has it.
namespace HoT::Profiler::Sampler {

struct Entry {
    std::string m_name;
    uint32_t m_self = 0;
    uint32_t m_inclusive = 0;
};

// Starts sampling the calling thread in the background, clearing earlier samples.
void Start();
void Stop();

// Samples gathered since Start, merged by function and sorted by self samples, false where sampling is unavailable.
bool Report(std::vector<Entry>& entries, uint32_t& total, double& seconds);

}

#endif
