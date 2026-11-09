#include "HoT/Core/CVars.h"
#include "HoT/Runtime/Cvars.h"

namespace HoT::FirstRunCvars {

// Storage for the glue screens' one-off first-run prompts in GlueXML/HotFirstRun.lua. The streamer
// also holds character select reloads until firstRunHdPatchOffered is set.
void Register() {
    CVars::Register("firstRunVideoShown", "Set once the first-run video setup has been shown.", "0");
    CVars::Register("firstRunHdPatchOffered", "Set once the HD patch has been offered on the first connection.", "0");

    // Recomputed every launch by StartupDefaults, a launch that never writes its config would lose the flags above.
    CVars::Register("firstRunFreshInstall", "Set when this launch found no config, i.e. a new install.", "0");
}

}
