#include "HoT/Character/AnimationFixes.h"
#include "HoT/Character/HD/HDCharacters.h"
#include "HoT/Character/PlayerRenderLimit.h"
#include "HoT/Core/Modules.h"
#include "HoT/DB/Refresh.h"
#include "HoT/Discord/Discord.h"
#include "HoT/Editor/Editor.h"
#include "HoT/Features/Features.h"
#include "HoT/Features/GemSocket.h"
#include "HoT/Game/Dev.h"
#include "HoT/Game/Session.h"
#include "HoT/Features/Items.h"
#include "HoT/Features/Units.h"
#include "HoT/Lua/Builtins.h"
#include "HoT/MSDF/MSDF.h"
#include "HoT/Models/ModernM2.h"
#include "HoT/Profiler/Profiler.h"
#include "HoT/Runtime/Cvars.h"
#include "HoT/Runtime/LauncherSettings.h"
#include "HoT/Runtime/LinkWatchdog.h"
#include "HoT/Runtime/LuaRuntime.h"
#include "HoT/Runtime/WorldResume.h"
#include "HoT/Spells/ActionRepeat.h"
#include "HoT/Streaming/BackgroundDownloader.h"
#include "HoT/Streaming/Channels.h"
#include "HoT/Spells/AutoRepeatDeadzone.h"
#include "HoT/Ui/Atlas.h"
#include "HoT/Ui/ComboPoints.h"
#include "HoT/Ui/GlueScripts.h"

namespace HoT {

std::vector<Module*>& Modules() {
    static std::vector<Module*> modules = {
        Session::GetModule(),
        DB::GetRefreshModule(),
        RuntimeCvars::GetModule(),
        LauncherSettings::GetModule(),
        LuaRuntime::GetModule(),
        LinkWatchdog::GetModule(),
        WorldResume::GetModule(),
        Streaming::GetModule(),
        Streaming::Channels::GetModule(),
        Discord::GetModule(),
        Editor::GetModule(),
        MSDF::GetModule(),
        Models::GetModule(),
        Builtins::GetModule(),
        Dev::GetModule(),
        Profiler::GetModule(),
        Feature::GetModule(),
        AuraValues::GetModule(),
        CooldownRate::GetModule(),
        Crafting::GetModule(),
        GameTele::GetModule(),
        GemSocket::GetModule(),
        GroupFinder::GetModule(),
        HealPrediction::GetModule(),
        ItemGen::GetModule(),
        Items::GetModule(),
        ItemLevel::GetModule(),
        LootWindow::GetModule(),
        Minigame::GetModule(),
        MovementForce::GetModule(),
        PlayerSettings::GetModule(),
        QuestLog::GetModule(),
        Talent::GetModule(),
        TestFrame::GetModule(),
        TowerDefence::GetModule(),
        Transmog::GetModule(),
        UnitLevel::GetModule(),
        Animation::GetModule(),
        PlayerRenderLimit::GetModule(),
        HD::GetModule(),
        ActionRepeat::GetModule(),
        AutoRepeatDeadzone::GetModule(),
        Atlas::GetModule(),
        ComboPoints::GetModule(),
        GlueScripts::GetModule(),
    };

    return modules;
}

}
