#include "CGWorldStateInfo.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/WorldStateUIRec.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"

// VERIFY: f57c55c3 manual
// OFFSET: 0x00548720
static int32_t Script_GetNumWorldStateUI(lua_State* L) {
    lua_pushnumber(L, CGWorldStateInfo::m_worldStateUI.m_count);
    return 1;
}

// VERIFY: 7bc72600 manual
// OFFSET: 0x00548D40
int32_t Script_GetWorldStateUIInfo(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: GetWorldStateUIInfo(index)");
        return 0;
    }

    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));
    int32_t id = index - 1 < CGWorldStateInfo::m_worldStateUI.m_count ? CGWorldStateInfo::m_worldStateUI.m_data[index - 1].m_id : 0;
    WorldStateUIRec* rec = g_worldStateUIDB.GetRecord(id);

    if (!rec) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    lua_pushnumber(L, rec->m_type);
    double state;

    if (rec->m_stateVariable) {
        WORLDSTATE* worldState = CGWorldStateInfo::m_worldStates.Ptr(rec->m_stateVariable, HASHKEY_NONE());
        state = worldState ? worldState->m_value : 0;
    } else {
        state = 1.0;
    }

    lua_pushnumber(L, state);
    char text[256];
    WorldStateParseText(rec->m_string, text, sizeof(text));
    lua_pushstring(L, text);
    lua_pushstring(L, rec->m_icon);
    lua_pushstring(L, rec->m_dynamicIcon);
    lua_pushstring(L, rec->m_tooltip);
    lua_pushstring(L, rec->m_dynamicTooltip);
    lua_pushstring(L, rec->m_extendedUi);

    for (uint32_t i = 0; i < 3; i++) {
        WORLDSTATE* worldState = CGWorldStateInfo::m_worldStates.Ptr(rec->m_extendedUistateVariable[i], HASHKEY_NONE());
        lua_pushnumber(L, worldState ? worldState->m_value : 0);
    }

    return 11;
}

// STATIC: 0x00ACD0D0
static FrameScript_Method s_ScriptFunctions[] = {
    { "GetNumWorldStateUI", &Script_GetNumWorldStateUI },
    { "GetWorldStateUIInfo", &Script_GetWorldStateUIInfo },
};

// VERIFY: 30669ada auto
// OFFSET: 0x005485B0
void WorldStateInfoRegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_ScriptFunctions[i].m_name, s_ScriptFunctions[i].m_method);
    }
}

// VERIFY: 385383dc auto
// OFFSET: 0x005485E0
void WorldStateInfoUnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_ScriptFunctions[i].m_name);
    }
}
