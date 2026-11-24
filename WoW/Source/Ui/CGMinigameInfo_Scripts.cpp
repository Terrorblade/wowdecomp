#include "CGMinigameInfo.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"

#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "WoW/Source/Minigame/Minigame_c.h"

// STATIC: 0x00AD05C8
static const char* s_minigameNames[] = { "TicTacToe", nullptr };

// VERIFY: fbfbc768 auto
// OFFSET: 0x005C5500
static int32_t Script_GetMinigameType(lua_State* L) {
    CMinigame_C* minigame = CGMinigameInfo::m_minigame;

    if (minigame && minigame->m_type == MINIGAME_TICTACTOE) {
        lua_pushstring(L, s_minigameNames[minigame->m_type]);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// VERIFY: 37b2ca9a auto
// OFFSET: 0x005C5550
static int32_t Script_MakeMinigameMove(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: MakeMinigameMove(moveType, param)");
        return 0;
    }

    uint8_t moveType = static_cast<int32_t>(lua_tonumber(L, 1) - 1.0);
    double param = lua_tonumber(L, 2);

    if (CGMinigameInfo::m_minigame) {
        CGMinigameInfo::m_minigame->MakeMove(moveType, static_cast<int32_t>(param - 1.0));
    }

    return 0;
}

// VERIFY: 9ea74c41 auto
// OFFSET: 0x005C55F0
static int32_t Script_GetMinigameState(lua_State* L) {
    return CGMinigameInfo::m_minigame ? CGMinigameInfo::m_minigame->PushStateToLua(L) : 0;
}

// STATIC: 0x00AD05D0
static FrameScript_Method s_ScriptFunctions[] = {
    { "GetMinigameType", &Script_GetMinigameType },
    { "MakeMinigameMove", &Script_MakeMinigameMove },
    { "GetMinigameState", &Script_GetMinigameState },
};

// VERIFY: dedde433 auto
// OFFSET: 0x005C5610
void MinigameInfoRegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_ScriptFunctions[i].m_name, s_ScriptFunctions[i].m_method);
    }
}

// VERIFY: 380c401b auto
// OFFSET: 0x005C5640
void MinigameInfoUnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_ScriptFunctions[i].m_name);
    }
}
