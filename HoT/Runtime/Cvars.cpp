#include "HoT/Core/Modules.h"
#include "HoT/Runtime/Cvars.h"

namespace HoT::RuntimeCvars {

class RuntimeCvarsModule : public Module {
    public:
    const char* Name() const override { return "RuntimeCvars"; }

    void OnInitializeGlobal() override {
        StartupDownloaderCvar::Register();
        PatchChannelCvar::Register();
        DebugOutputCvar::Register();
        FirstRunCvars::Register();
        StartupDefaults::ApplyFreshInstall();
        TalentCameraCvars::Register();
    }

    // The client registers movie and expansionMovie with its console commands, which run just before this.
    void OnClientDBInitialized() override {
        StartupDefaults::ApplyMovies();
    }
};

Module* GetModule() {
    static RuntimeCvarsModule module;
    return &module;
}

}
