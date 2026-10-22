#include "Tutorial.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Ui/GameUI.h"

// VERIFY: 9c38a03a manual
// OFFSET: 0x00530750
static int32_t Script_FlagTutorial(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: FlagTutorial(\"tutorial\")");
        return 0;
    }

    uint32_t tutorial = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;

    if (tutorial <= 59) {
        CGTutorial::FlagTutorial(tutorial);
    }

    return 0;
}

// VERIFY: d7a5866c manual
// OFFSET: 0x005308D0
static int32_t Script_TriggerTutorial(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: TriggerTutorial(\"tutorial\")");
        return 0;
    }

    uint32_t tutorial = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;

    if (tutorial <= 59) {
        CGTutorial::TriggerTutorial(tutorial);
    }

    return 0;
}

// VERIFY: f778febd auto
// OFFSET: 0x005307A0
static int32_t Script_IsTutorialFlagged(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: Trigger(\"tutorial\")");
        return 0;
    }

    uint32_t tutorial = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;

    if (tutorial > 59) {
        return 0;
    }

    if (IsTutorialFlagged(tutorial)) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: c71cc525 auto
// OFFSET: 0x00530700
static int32_t Script_CanResetTutorials(lua_State* L) {
    for (uint32_t i = CGTutorial::m_tutorialShown.m_bits.m_count; i; ) {
        i--;

        if (CGTutorial::m_tutorialShown.m_bits.m_data[i]) {
            lua_pushnumber(L, 1.0);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

// VERIFY: 7bfe935d auto
// OFFSET: 0x00530820
static int32_t Script_ClearTutorials(lua_State* L) {
    CGTutorial::ClearTutorials();
    return 0;
}

// VERIFY: 545db5cf auto
// OFFSET: 0x00530830
static int32_t Script_ResetTutorials(lua_State* L) {
    CGTutorial::ResetTutorials();
    return 0;
}

// VERIFY: 53babe68 manual
// OFFSET: 0x005301D0
static int32_t Script_GetNextCompleatedTutorial(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: GetNextCompleatedTutorial(\"tutorial\")");
        return 0;
    }

    int32_t tutorial = static_cast<int32_t>(lua_tonumber(L, 1));
    int32_t next = CGTutorial::GetNextCompletedTutorial(tutorial - 1);

    if (next == 60) {
        return 0;
    }

    lua_pushnumber(L, next);
    return 1;
}

// VERIFY: 15b806a3 manual
// OFFSET: 0x00530240
static int32_t Script_GetPrevCompleatedTutorial(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: GetPrevCompleatedTutorial(\"tutorial\")");
        return 0;
    }

    int32_t tutorial = static_cast<int32_t>(lua_tonumber(L, 1));
    int32_t prev = CGTutorial::GetPrevCompletedTutorial(tutorial - 1);

    if (!prev) {
        return 0;
    }

    lua_pushnumber(L, prev);
    return 1;
}

// STATIC: 0x00ACC868
static FrameScript_Method s_ScriptFunctions[] = {
    { "CanResetTutorials", &Script_CanResetTutorials },
    { "FlagTutorial", &Script_FlagTutorial },
    { "IsTutorialFlagged", &Script_IsTutorialFlagged },
    { "TriggerTutorial", &Script_TriggerTutorial },
    { "ClearTutorials", &Script_ClearTutorials },
    { "ResetTutorials", &Script_ResetTutorials },
    { "GetNextCompleatedTutorial", &Script_GetNextCompleatedTutorial },
    { "GetPrevCompleatedTutorial", &Script_GetPrevCompleatedTutorial },
};

// VERIFY: 901ffa76 auto
// OFFSET: 0x005302B0
void TutorialRegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_ScriptFunctions[i].m_name, s_ScriptFunctions[i].m_method);
    }
}

// VERIFY: 32936793 auto
// OFFSET: 0x005302E0
void TutorialUnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_ScriptFunctions[i].m_name);
    }
}
