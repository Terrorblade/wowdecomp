#include "KnowledgeBase.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Ui/CKBSystem.h"

// VERIFY: 9e42057a auto
// OFFSET: 0x00551FB0
static int32_t Script_KBSystem_GetMOTD(lua_State* L) {
    lua_pushstring(L, s_motd.m_buffer);
    return 1;
}

// VERIFY: 813271c5 auto
// OFFSET: 0x00551FD0
static int32_t Script_KBSystem_GetServerStatus(lua_State* L) {
    lua_pushstring(L, s_serverStatus.m_buffer);
    return 1;
}

// VERIFY: 88390c3b auto
// OFFSET: 0x00551FF0
static int32_t Script_KBSystem_GetServerNotice(lua_State* L) {
    lua_pushstring(L, s_serverNotice.m_buffer);
    return 1;
}

// VERIFY: 64710f8f auto
// OFFSET: 0x00551720
static int32_t Script_KBSetup_BeginLoading(lua_State* L) {
    return 0;
}

// VERIFY: e0016275 auto
// OFFSET: 0x00551760
static int32_t Script_KBSetup_IsLoaded(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

// VERIFY: bc5e561c auto
// OFFSET: 0x00551B90
static int32_t Script_KBQuery_IsLoaded(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

// VERIFY: 1d0a0bbd auto
// OFFSET: 0x00551DA0
static int32_t Script_KBArticle_IsLoaded(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

// VERIFY: 3bb58cb3 auto
// OFFSET: 0x00551790
static int32_t Script_KBSetup_GetLanguageCount(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 453300cc auto
// OFFSET: 0x005518C0
static int32_t Script_KBSetup_GetCategoryCount(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 039b02dc auto
// OFFSET: 0x005519F0
static int32_t Script_KBSetup_GetArticleHeaderCount(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 516cbb9d auto
// OFFSET: 0x00551BC0
static int32_t Script_KBQuery_GetArticleHeaderCount(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 511f6008 manual
// OFFSET: 0x005517D0
static int32_t Script_KBSetup_GetLanguageData(lua_State* L) {
    return 0;
}

// VERIFY: f63ffcdf manual
// OFFSET: 0x00551900
static int32_t Script_KBSetup_GetCategoryData(lua_State* L) {
    return 0;
}

// VERIFY: 2a3a128f manual
// OFFSET: 0x00551A30
static int32_t Script_KBSetup_GetArticleHeaderData(lua_State* L) {
    return 0;
}

// VERIFY: 58c4251a auto
// OFFSET: 0x00551B40
static int32_t Script_KBSetup_GetTotalArticleCount(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 6159c4ce manual
// OFFSET: 0x00551C00
static int32_t Script_KBQuery_GetArticleHeaderData(lua_State* L) {
    return 0;
}

// VERIFY: 73c4bd52 auto
// OFFSET: 0x00551D10
static int32_t Script_KBQuery_GetTotalArticleCount(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: f4fc784c auto
// OFFSET: 0x00551D60
static int32_t Script_KBArticle_BeginLoading(lua_State* L) {
    return 0;
}

// VERIFY: 0075add2 manual
// OFFSET: 0x00551DD0
static int32_t Script_KBArticle_GetData(lua_State* L) {
    return 0;
}

// VERIFY: 52b22198 manual
// OFFSET: 0x005523A0
static int32_t Script_KBSetup_GetSubCategoryCount(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: cc04f165 manual
// OFFSET: 0x00552440
static int32_t Script_KBSetup_GetSubCategoryData(lua_State* L) {
    return 0;
}

// VERIFY: f0e5209c manual
// OFFSET: 0x00552580
static int32_t Script_KBQuery_BeginLoading(lua_State* L) {
    return 0;
}

// STATIC: 0x00ACD488
static FrameScript_Method s_apMethodMap[] = {
    { "KBSetup_BeginLoading", &Script_KBSetup_BeginLoading },
    { "KBSetup_IsLoaded", &Script_KBSetup_IsLoaded },
    { "KBSetup_GetLanguageCount", &Script_KBSetup_GetLanguageCount },
    { "KBSetup_GetLanguageData", &Script_KBSetup_GetLanguageData },
    { "KBSetup_GetCategoryCount", &Script_KBSetup_GetCategoryCount },
    { "KBSetup_GetCategoryData", &Script_KBSetup_GetCategoryData },
    { "KBSetup_GetSubCategoryCount", &Script_KBSetup_GetSubCategoryCount },
    { "KBSetup_GetSubCategoryData", &Script_KBSetup_GetSubCategoryData },
    { "KBSetup_GetArticleHeaderCount", &Script_KBSetup_GetArticleHeaderCount },
    { "KBSetup_GetArticleHeaderData", &Script_KBSetup_GetArticleHeaderData },
    { "KBSetup_GetTotalArticleCount", &Script_KBSetup_GetTotalArticleCount },
    { "KBQuery_BeginLoading", &Script_KBQuery_BeginLoading },
    { "KBQuery_IsLoaded", &Script_KBQuery_IsLoaded },
    { "KBQuery_GetArticleHeaderCount", &Script_KBQuery_GetArticleHeaderCount },
    { "KBQuery_GetArticleHeaderData", &Script_KBQuery_GetArticleHeaderData },
    { "KBQuery_GetTotalArticleCount", &Script_KBQuery_GetTotalArticleCount },
    { "KBArticle_BeginLoading", &Script_KBArticle_BeginLoading },
    { "KBArticle_IsLoaded", &Script_KBArticle_IsLoaded },
    { "KBArticle_GetData", &Script_KBArticle_GetData },
    { "KBSystem_GetMOTD", &Script_KBSystem_GetMOTD },
    { "KBSystem_GetServerStatus", &Script_KBSystem_GetServerStatus },
    { "KBSystem_GetServerNotice", &Script_KBSystem_GetServerNotice },
};

// VERIFY: a3329701 auto
// OFFSET: 0x00552010
void KnowledgeBase_RegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_apMethodMap) / sizeof(s_apMethodMap[0]); i++) {
        FrameScript_RegisterFunction(s_apMethodMap[i].m_name, s_apMethodMap[i].m_method);
    }
}

// VERIFY: 25924890 auto
// OFFSET: 0x00552040
void KnowledgeBase_UnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_apMethodMap) / sizeof(s_apMethodMap[0]); i++) {
        FrameScript_UnregisterFunction(s_apMethodMap[i].m_name);
    }
}
