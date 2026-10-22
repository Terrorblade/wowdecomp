#include "HoT/Platform/Process.h"
#include <unistd.h>

namespace HoT::Platform {

uint32_t CurrentProcessId() {
    return static_cast<uint32_t>(getpid());
}

bool IsWine() {
    return false;
}

}
