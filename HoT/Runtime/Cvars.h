#ifndef HOT_RUNTIME_CVARS_H
#define HOT_RUNTIME_CVARS_H

#include <string>

namespace HoT {
class Module;
}

namespace HoT::TalentCameraCvars {

void Register();

}

namespace HoT::FirstRunCvars {

void Register();

}

// No startup movies, and firstRunFreshInstall from whether WTF\Config.wtf existed at startup.
namespace HoT::StartupDefaults {

void ApplyFreshInstall();
void ApplyMovies();

}

namespace HoT::DebugOutputCvar {

void Register();

}

// Only the CVar for now, the background downloader it starts is not ported yet.
namespace HoT::StartupDownloaderCvar {

void Register();
bool Enabled();

}

namespace HoT::PatchChannelCvar {

void Register();
std::string Value();

}

// Registers every CVar above in the DLL's order.
namespace HoT::RuntimeCvars {

Module* GetModule();

}

#endif
