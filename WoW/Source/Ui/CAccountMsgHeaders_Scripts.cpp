#include "CAccountMsgHeaders.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "WoW/Source/Ui/CAccountMsgBody.h"

// VERIFY: 1ee4db68 auto
// OFFSET: 0x0054FDE0
static int32_t Script_AccountMsg_GetNumTotalMsgs(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_GetNumTotalMsgs() failed because the headers are not loaded");
        return 0;
    }

    lua_pushnumber(L, g_accountMsgHeaders.m_count);
    return 1;
}

// VERIFY: 8e6492ed auto
// OFFSET: 0x0054FE20
static int32_t Script_AccountMsg_GetHeaderSubject(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_GetHeaderSubject() failed because the headers are not loaded");
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: AccountMsg_GetHeaderSubject(number)");
        return 0;
    }

    double index = lua_tonumber(L, 1);

    if (index < 0.0 || index >= g_accountMsgHeaders.m_count) {
        luaL_error(L, "Usage: AccountMsg_GetHeaderSubject(number) failed because the number is out of range");
        return 0;
    }

    lua_pushstring(L, g_accountMsgHeaders.GetHeaderSubject(static_cast<int32_t>(index)));
    return 1;
}

// VERIFY: e42cdc6e auto
// OFFSET: 0x0054FEC0
static int32_t Script_AccountMsg_GetHeaderPriority(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_GetHeaderPriority() failed because the headers are not loaded");
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: AccountMsg_GetHeaderPriority(number)");
        return 0;
    }

    double index = lua_tonumber(L, 1);

    if (index < 0.0 || index >= g_accountMsgHeaders.m_count) {
        luaL_error(L, "Usage: AccountMsg_GetHeaderPriority(number) failed because the number is out of range");
        return 0;
    }

    lua_pushnumber(L, g_accountMsgHeaders.GetHeaderPriority(static_cast<int32_t>(index)));
    return 1;
}

// VERIFY: 8d652036 auto
// OFFSET: 0x0054FF70
static int32_t Script_AccountMsg_LoadBody(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_LoadBody() failed because the headers are not loaded");
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: AccountMsg_LoadBody(number)");
        return 0;
    }

    double index = lua_tonumber(L, 1);

    if (index < 0.0 || index >= g_accountMsgHeaders.m_count) {
        luaL_error(L, "Usage: AccountMsg_LoadBody(number) failed because the number is out of range");
        return 0;
    }

    if (!g_accountMsgBody.LoadBody(g_accountMsgHeaders.GetMessageId(static_cast<int32_t>(index)))) {
        luaL_error(L, "AccountMsg_LoadHeaders() failed");
        return 0;
    }

    return 0;
}

// VERIFY: 7f42e21d auto
// OFFSET: 0x00550020
static int32_t Script_AccountMsg_GetBody(lua_State* L) {
    if (g_accountMsgBody.m_state != 2) {
        luaL_error(L, "AccountMsg_GetBody() failed because the body was not loaded");
        return 0;
    }

    if (!g_accountMsgBody.m_iterator.m_node) {
        g_accountMsgBody._GetRoot(g_accountMsgBody.m_iterator);
    }

    if (!g_accountMsgBody.m_iterator.m_node) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, g_accountMsgBody._GetBodyString(g_accountMsgBody.m_iterator.m_node));
    return 1;
}

// VERIFY: 0b82481a auto
// OFFSET: 0x00550360
static int32_t Script_AccountMsg_GetNumUnreadMsgs(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_GetNumUnreadMsgs() failed because the headers are not loaded");
        return 0;
    }

    int32_t count = 0;

    for (int32_t i = 0; i < g_accountMsgHeaders.m_count; i++) {
        if (!g_accountMsgHeaders.m_opened.m_data[i]) {
            count++;
        }
    }

    lua_pushnumber(L, count);
    return 1;
}

// VERIFY: fd796aeb auto
// OFFSET: 0x005503E0
static int32_t Script_AccountMsg_GetNumUnreadUrgentMsgs(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_GetNumUnreadMsgs() failed because the headers are not loaded");
        return 0;
    }

    lua_pushnumber(L, g_accountMsgHeaders.GetNumUnreadUrgentMsgs());
    return 1;
}

// VERIFY: ddf7e51b manual
// OFFSET: 0x00550430
static int32_t Script_AccountMsg_GetIndexHighestPriorityUnreadMsg(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_GetHeaderSubject() failed because the headers are not loaded");
        return 0;
    }

    int32_t index = g_accountMsgHeaders.GetIndexHighestPriorityUnreadMsg();
    lua_pushnumber(L, index);
    lua_pushnumber(L, g_accountMsgHeaders.GetHeaderPriority(index));
    return 2;
}

// VERIFY: 13b3d088 auto
// OFFSET: 0x005504E0
static int32_t Script_AccountMsg_GetIndexNextUnreadMsg(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_GetHeaderSubject() failed because the headers are not loaded");
        return 0;
    }

    int32_t index = -1;

    if (lua_isnumber(L, 1)) {
        index = static_cast<int32_t>(lua_tonumber(L, 1));
    }

    int32_t next = g_accountMsgHeaders.GetIndexNextUnreadMsg(index);
    lua_pushnumber(L, next);
    lua_pushnumber(L, g_accountMsgHeaders.GetHeaderPriority(next));
    return 2;
}

// VERIFY: 99c56524 auto
// OFFSET: 0x005505B0
static int32_t Script_AccountMsg_SetMsgRead(lua_State* L) {
    if (g_accountMsgHeaders.m_state != 2) {
        luaL_error(L, "AccountMsg_SetMsgRead() failed because the headers are not loaded");
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: AccountMsg_SetMsgRead(number)");
        return 0;
    }

    int32_t index = static_cast<int32_t>(lua_tonumber(L, 1));

    if (index < 0 || index >= g_accountMsgHeaders.m_count) {
        luaL_error(L, "Usage: AccountMsg_SetMsgRead(number) failed because the number is out of range");
        return 0;
    }

    if (!g_accountMsgHeaders.SetMsgRead(index)) {
        luaL_error(L, "Usage: AccountMsg_SetMsgRead(number) failed due to an URL error");
        return 0;
    }

    return 0;
}

// VERIFY: 74473c23 auto
// OFFSET: 0x00550910
static int32_t Script_AccountMsg_LoadHeaders(lua_State* L) {
    if (!g_accountMsgHeaders.LoadHeaders()) {
        luaL_error(L, "AccountMsg_LoadHeaders() failed");
        return 0;
    }

    return 0;
}

// STATIC: 0x00ACD418
static FrameScript_Method s_scriptFunctions[] = {
    { "AccountMsg_LoadHeaders", &Script_AccountMsg_LoadHeaders },
    { "AccountMsg_GetNumTotalMsgs", &Script_AccountMsg_GetNumTotalMsgs },
    { "AccountMsg_GetNumUnreadMsgs", &Script_AccountMsg_GetNumUnreadMsgs },
    { "AccountMsg_GetNumUnreadUrgentMsgs", &Script_AccountMsg_GetNumUnreadUrgentMsgs },
    { "AccountMsg_GetIndexHighestPriorityUnreadMsg", &Script_AccountMsg_GetIndexHighestPriorityUnreadMsg },
    { "AccountMsg_GetIndexNextUnreadMsg", &Script_AccountMsg_GetIndexNextUnreadMsg },
    { "AccountMsg_GetHeaderSubject", &Script_AccountMsg_GetHeaderSubject },
    { "AccountMsg_GetHeaderPriority", &Script_AccountMsg_GetHeaderPriority },
    { "AccountMsg_LoadBody", &Script_AccountMsg_LoadBody },
    { "AccountMsg_GetBody", &Script_AccountMsg_GetBody },
    { "AccountMsg_SetMsgRead", &Script_AccountMsg_SetMsgRead },
};

// VERIFY: be07ae82 auto
// OFFSET: 0x00550080
void AccountMsg_RegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_scriptFunctions) / sizeof(s_scriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_scriptFunctions[i].m_name, s_scriptFunctions[i].m_method);
    }
}

// VERIFY: 9920deda auto
// OFFSET: 0x005500B0
void AccountMsg_UnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_scriptFunctions) / sizeof(s_scriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_scriptFunctions[i].m_name);
    }
}
