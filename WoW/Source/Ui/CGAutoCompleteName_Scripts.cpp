#include "CGAutoCompleteName.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"

// VERIFY: c189cafe auto
// OFFSET: 0x0057B3A0
static int32_t Script_GetAutoCompleteResults(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
        luaL_error(L, "Usage: GetAutoCompleteResults(text, includeBitField, excludeBitField, numReturns[, cursorPosition[, allowFullMatch]])");
        return 0;
    }

    const char* text = lua_tolstring(L, 1, nullptr);
    uint32_t include = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 2)));
    uint32_t exclude = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 3)));
    int32_t numReturns = static_cast<int32_t>(lua_tonumber(L, 4));
    int32_t cursor;

    if (!lua_isnumber(L, 5) || (cursor = static_cast<int32_t>(lua_tonumber(L, 5))) < 0 || cursor > static_cast<int32_t>(SStrLenUTF8(text))) {
        cursor = SStrLenUTF8(text);
    }

    bool allowFullMatch = lua_toboolean(L, 6) != 0;
    return CGAutoCompleteName::GetMatchingResults(L, text, include, exclude, numReturns, cursor, allowFullMatch);
}

// VERIFY: 6b80b8e7 auto
// OFFSET: 0x0057B500
static int32_t Script_GetAutoCompletePresenceID(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetAutoCompletePresenceID(\"name\")");
        return 0;
    }

    uint32_t presenceId;

    if (!CGAutoCompleteName::GetPresenceID(lua_tolstring(L, 1, nullptr), &presenceId)) {
        return 0;
    }

    lua_pushnumber(L, presenceId);
    return 1;
}

// STATIC: 0x00ACEB50
static FrameScript_Method s_ScriptFunctions[] = {
    { "GetAutoCompleteResults", &Script_GetAutoCompleteResults },
    { "GetAutoCompletePresenceID", &Script_GetAutoCompletePresenceID },
};

// VERIFY: 9bd82136 auto
// OFFSET: 0x0057ADB0
void AutoCompleteRegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_ScriptFunctions[i].m_name, s_ScriptFunctions[i].m_method);
    }
}

// VERIFY: 2eb82548 auto
// OFFSET: 0x0057ADE0
void AutoCompleteUnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_ScriptFunctions[i].m_name);
    }
}
