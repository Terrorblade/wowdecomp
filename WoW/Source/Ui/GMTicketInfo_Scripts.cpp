#include "GMTicketInfo.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Base/CDataStore.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/GMSurveyInfo.h"

// STATIC: 0x00ACFAC0
static StaticConstant s_staticConstants[7] = {
    { "STATIC_CONSTANTS", 0 },
    { "Loot", 1 },
    { "AuctionHouse", 2 },
    { "Mail", 3 },
    { "Chat", 4 },
    { "Movement", 5 },
    { "Spell", 6 },
};

// VERIFY: 030dc6aa auto
// OFFSET: 0x005AC320
static int32_t Script_RegisterStaticConstants(lua_State* L) {
    if (lua_type(L, 1) != LUA_TTABLE) {
        luaL_error(L, "Usage: RegisterStaticConstants(table)");
        return 0;
    }

    for (uint32_t i = 0; i < sizeof(s_staticConstants) / sizeof(s_staticConstants[0]); i++) {
        lua_pushstring(L, s_staticConstants[i].m_name);
        lua_pushnumber(L, s_staticConstants[i].m_value);
        lua_settable(L, 1);
    }

    return 0;
}

// VERIFY: 05686244 manual
// OFFSET: 0x005AD1C0
static int32_t Script_GetGMStatus(lua_State* L) {
    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_GM_TICKET_GET_SYSTEM_STATUS));
    msg.Finalize();
    ClientServices::Send2(&msg);
    return 0;
}

// VERIFY: bddd0a67 auto
// OFFSET: 0x005AD070
static int32_t Script_GetGMTicket(lua_State* L) {
    SendGetTicketOpcode();
    return 0;
}

// VERIFY: 61961e35 manual
// OFFSET: 0x005AC390
static int32_t Script_GMSurveyAnswerSubmit(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isstring(L, 3)) {
        luaL_error(L, "Usage: GMSurveyAnswerSubmit(question, rank, comment)");
        return 0;
    }

    uint32_t question = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;
    int32_t rank = static_cast<int32_t>(lua_tonumber(L, 2));
    const char* comment = lua_tolstring(L, 3, nullptr);

    if (question >= 10) {
        luaL_error(L, "GMSurveyAnswerSubmit: Questions limited from %d to %d", 1, 10);
        return 0;
    }

    CGGMSurveyInfo::SetAnswer(question, static_cast<uint8_t>(rank), comment);
    return 0;
}

// VERIFY: 8792c123 auto
// OFFSET: 0x005AC480
static int32_t Script_GMSurveyCommentSubmit(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GMSurveyCommentSubmit(comment)");
        return 0;
    }

    CGGMSurveyInfo::SetComment(lua_tolstring(L, 1, nullptr));
    return 0;
}

// VERIFY: e840aec2 manual
// OFFSET: 0x005AC750
static int32_t Script_GMSurveyQuestion(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: GMSurveyGetQuestion(index)");
        return 0;
    }

    uint32_t index = static_cast<int32_t>(lua_tonumber(L, 1));
    const char* question;

    if (index >= 1 && index <= 10 && (question = CGGMSurveyInfo::GetQuestion(index))) {
        lua_pushstring(L, question);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: f8da87e2 manual
// OFFSET: 0x005AC7D0
static int32_t Script_GMSurveyAnswer(lua_State* L) {
    if (!lua_isnumber(L, 1) && !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: GMSurveyGetAnswer(questionIndex, answerIndex)");
        return 0;
    }

    uint32_t index = static_cast<int32_t>(lua_tonumber(L, 1));
    double answerIndex = lua_tonumber(L, 2);
    const char* answer;

    if (index >= 1 && index <= 10 && (answer = CGGMSurveyInfo::GetAnswer(index, static_cast<int32_t>(answerIndex)))) {
        lua_pushstring(L, answer);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: ea936260 manual
// OFFSET: 0x005AC870
static int32_t Script_GMSurveyNumAnswers(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: GMSurveyGetNumAnswers(questionIndex)");
        return 0;
    }

    int32_t count = 0;
    uint32_t index = static_cast<int32_t>(lua_tonumber(L, 1));

    if (index >= 1 && index <= 10) {
        count = CGGMSurveyInfo::GetNumAnswers(index);
    }

    lua_pushnumber(L, count);
    return 1;
}

// VERIFY: ebaa1e68 auto
// OFFSET: 0x005AD020
static int32_t Script_GMReportLag(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: GMReportLag(number)");
        return 0;
    }

    CGGMReportLag::Submit(static_cast<int32_t>(lua_tonumber(L, 1)));
    return 0;
}

// VERIFY: e36f68e8 auto
// OFFSET: 0x005AD080
static int32_t Script_NewGMTicket(lua_State* L) {
    if (!lua_isstring(L, 1) || lua_type(L, 2) != LUA_TBOOLEAN) {
        luaL_error(L, "Usage: NewGMTicket(ticketDescription, needResponse)");
        return 0;
    }

    if (CGGameUI::CanPerformAction(UI_ACTION_GM_TICKET)) {
        const char* text = lua_tolstring(L, 1, nullptr);
        CGGMTicketInfo::CreateTicket(text, lua_toboolean(L, 2), 0);
    }

    return 0;
}

// VERIFY: a547b787 auto
// OFFSET: 0x005AD0F0
static int32_t Script_UpdateGMTicket(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: UpdateGMTicket(ticketDescription)");
        return 0;
    }

    if (CGGameUI::CanPerformAction(UI_ACTION_GM_TICKET)) {
        SendGMTicketUpdateText(lua_tolstring(L, 1, nullptr));
    }

    return 0;
}

// VERIFY: d47bd341 auto
// OFFSET: 0x005AD140
static int32_t Script_DeleteGMTicket(lua_State* L) {
    SendGMTicketDeleteTicket();
    return 0;
}

// VERIFY: 438ebb53 auto
// OFFSET: 0x005AD150
static int32_t Script_GMResponseNeedMoreHelp(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GMResponseNeedMoreHelp(newTicketDescription)");
        return 0;
    }

    if (CGGameUI::CanPerformAction(UI_ACTION_GM_TICKET)) {
        CGGMTicketInfo::CreateTicket(lua_tolstring(L, 1, nullptr), 1, 1);
    }

    return 0;
}

// VERIFY: 5ad04fb9 auto
// OFFSET: 0x005AD1B0
static int32_t Script_GMResponseResolve(lua_State* L) {
    SendGMResponseResolve();
    return 0;
}

// VERIFY: 082cae19 auto
// OFFSET: 0x005AD230
static int32_t Script_GMSurveySubmit(lua_State* L) {
    CGGMSurveyInfo::Submit();
    return 0;
}

// STATIC: 0x00ACFAF8
static FrameScript_Method s_ScriptFunctions[] = {
    { "GetGMTicket", &Script_GetGMTicket },
    { "NewGMTicket", &Script_NewGMTicket },
    { "UpdateGMTicket", &Script_UpdateGMTicket },
    { "DeleteGMTicket", &Script_DeleteGMTicket },
    { "GMResponseNeedMoreHelp", &Script_GMResponseNeedMoreHelp },
    { "GMResponseResolve", &Script_GMResponseResolve },
    { "GetGMStatus", &Script_GetGMStatus },
    { "GMSurveyQuestion", &Script_GMSurveyQuestion },
    { "GMSurveyNumAnswers", &Script_GMSurveyNumAnswers },
    { "GMSurveyAnswer", &Script_GMSurveyAnswer },
    { "GMSurveyAnswerSubmit", &Script_GMSurveyAnswerSubmit },
    { "GMSurveyCommentSubmit", &Script_GMSurveyCommentSubmit },
    { "GMSurveySubmit", &Script_GMSurveySubmit },
    { "GMReportLag", &Script_GMReportLag },
    { "RegisterStaticConstants", &Script_RegisterStaticConstants },
};

// VERIFY: 030f1972 auto
// OFFSET: 0x005AC4D0
void GMTicketInfoRegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_ScriptFunctions[i].m_name, s_ScriptFunctions[i].m_method);
    }
}

// VERIFY: 1edba6c1 auto
// OFFSET: 0x005AC500
void GMTicketInfoUnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_ScriptFunctions[i].m_name);
    }
}
