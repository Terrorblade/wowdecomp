#include <mutex>
#include <string>
#include "HoT/Core/CVars.h"
#include "HoT/Runtime/Cvars.h"

namespace HoT::PatchChannelCvar {

static std::mutex s_mutex;
static std::string s_value;

static void OnChanged(const char* value) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_value = value ? value : "";
}

void Register() {
    CVars::RegisterHidden("patchChannelOverride", nullptr, "", OnChanged);
}

std::string Value() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_value;
}

}
