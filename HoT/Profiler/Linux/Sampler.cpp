#include "HoT/Profiler/Sampler.h"

// No sampler here yet, the report just leaves the section out.
namespace HoT::Profiler::Sampler {

void Start() {
}

void Stop() {
}

bool Report(std::vector<Entry>& entries, uint32_t& total, double& seconds) {
    return false;
}

}
