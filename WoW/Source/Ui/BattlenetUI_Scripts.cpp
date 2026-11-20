#include "BattlenetUI.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: f2fecb1f auto
// OFFSET: 0x00530EC0
static int32_t Script_BNConnected(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

// VERIFY: 512bf591 auto
// OFFSET: 0x00530F20
static int32_t Script_IsBNLogin(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

// VERIFY: 515f5bba manual
// OFFSET: 0x005343F0
static int32_t Script_BNGetInfo(lua_State* L) {
    return 0;
}

// VERIFY: f2a0fb12 manual
// OFFSET: 0x00534590
static int32_t Script_BNGetNumFriends(lua_State* L) {
    lua_pushnumber(L, 0.0);
    lua_pushnumber(L, 0.0);
    return 2;
}

// VERIFY: ba9e1544 manual
// OFFSET: 0x005349F0
static int32_t Script_BNGetNumFriendToons(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 6c9894e6 auto
// OFFSET: 0x00534ED0
static int32_t Script_BNRemoveFriend(lua_State* L) {
    return 0;
}

// VERIFY: 6e07b4a2 auto
// OFFSET: 0x00534F80
static int32_t Script_BNSetFriendNote(lua_State* L) {
    return 0;
}

// VERIFY: 5f5b8f58 manual
// OFFSET: 0x00535080
static int32_t Script_BNGetNumFriendInvites(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 0258b95f manual
// OFFSET: 0x00535180
static int32_t Script_BNGetFriendInviteInfo(lua_State* L) {
    return 0;
}

// VERIFY: c1a1acff auto
// OFFSET: 0x00535380
static int32_t Script_BNSendFriendInvite(lua_State* L) {
    return 0;
}

// VERIFY: 93bf8940 auto
// OFFSET: 0x00535490
static int32_t Script_BNSendFriendInviteByID(lua_State* L) {
    return 0;
}

// VERIFY: 3fea367a auto
// OFFSET: 0x005355C0
static int32_t Script_BNAcceptFriendInvite(lua_State* L) {
    return 0;
}

// VERIFY: 4a1d39b8 auto
// OFFSET: 0x00535660
static int32_t Script_BNDeclineFriendInvite(lua_State* L) {
    return 0;
}

// VERIFY: c0f9de03 auto
// OFFSET: 0x00535700
static int32_t Script_BNReportFriendInvite(lua_State* L) {
    return 0;
}

// VERIFY: 47c2f002 auto
// OFFSET: 0x005357A0
static int32_t Script_BNSetAFK(lua_State* L) {
    return 0;
}

// VERIFY: bf638620 auto
// OFFSET: 0x00535860
static int32_t Script_BNSetDND(lua_State* L) {
    return 0;
}

// VERIFY: 4af5fdd0 manual
// OFFSET: 0x00535920
static int32_t Script_BNSetCustomMessage(lua_State* L) {
    return 0;
}

// VERIFY: e1af46d8 manual
// OFFSET: 0x00535AA0
static int32_t Script_BNGetCustomMessageTable(lua_State* L) {
    return 0;
}

// VERIFY: d3aaf112 auto
// OFFSET: 0x00535C60
static int32_t Script_BNSetFocus(lua_State* L) {
    return 0;
}

// VERIFY: a26abed5 manual
// OFFSET: 0x00535CE0
static int32_t Script_BNCreateConversation(lua_State* L) {
    return 0;
}

// VERIFY: 72a452b9 manual
// OFFSET: 0x00535EB0
static int32_t Script_BNInviteToConversation(lua_State* L) {
    return 0;
}

// VERIFY: f58e7e6b manual
// OFFSET: 0x00536030
static int32_t Script_BNLeaveConversation(lua_State* L) {
    return 0;
}

// VERIFY: 626f8a47 auto
// OFFSET: 0x00536110
static int32_t Script_BNSendConversationMessage(lua_State* L) {
    return 0;
}

// VERIFY: 84553829 manual
// OFFSET: 0x00536220
static int32_t Script_BNGetNumConversationMembers(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 7432c6ae manual
// OFFSET: 0x00536330
static int32_t Script_BNGetConversationInfo(lua_State* L) {
    return 0;
}

// VERIFY: 3cb99631 manual
// OFFSET: 0x00536400
static int32_t Script_BNGetNumBlocked(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: e9831e9e auto
// OFFSET: 0x005364E0
static int32_t Script_BNIsBlocked(lua_State* L) {
    return 0;
}

// VERIFY: ede7e82b auto
// OFFSET: 0x005365B0
static int32_t Script_BNSetBlocked(lua_State* L) {
    return 0;
}

// VERIFY: a99f894a manual
// OFFSET: 0x005366A0
static int32_t Script_BNSetSelectedBlock(lua_State* L) {
    return 0;
}

// VERIFY: 4ec1f3a8 manual
// OFFSET: 0x00536790
static int32_t Script_BNGetSelectedBlock(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 3d19868a manual
// OFFSET: 0x00536890
static int32_t Script_BNGetNumBlockedToons(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: fa351982 manual
// OFFSET: 0x00536970
static int32_t Script_BNGetBlockedToonInfo(lua_State* L) {
    return 0;
}

// VERIFY: b9dcb576 auto
// OFFSET: 0x00536A90
static int32_t Script_BNIsToonBlocked(lua_State* L) {
    return 0;
}

// VERIFY: a38c9f58 auto
// OFFSET: 0x00536B60
static int32_t Script_BNSetToonBlocked(lua_State* L) {
    return 0;
}

// VERIFY: ae48a591 manual
// OFFSET: 0x00536C50
static int32_t Script_BNSetSelectedToonBlock(lua_State* L) {
    return 0;
}

// VERIFY: 7ac120a9 manual
// OFFSET: 0x00536D40
static int32_t Script_BNGetSelectedToonBlock(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 3d16158e manual
// OFFSET: 0x00536E40
static int32_t Script_BNReportPlayer(lua_State* L) {
    return 0;
}

// VERIFY: 32894792 auto
// OFFSET: 0x00537010
static int32_t Script_BNFeaturesEnabledAndConnected(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

// VERIFY: ed1e6ea4 auto
// OFFSET: 0x00537070
static int32_t Script_BNFeaturesEnabled(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

// VERIFY: 3eed4bc9 manual
// OFFSET: 0x005370D0
static int32_t Script_BNGetNumFOF(lua_State* L) {
    return 0;
}

// VERIFY: c5215d9a manual
// OFFSET: 0x00537240
static int32_t Script_BNGetFOFInfo(lua_State* L) {
    return 0;
}

// VERIFY: 27fc51cd manual
// OFFSET: 0x00537510
static int32_t Script_BNSetSelectedFriend(lua_State* L) {
    return 0;
}

// VERIFY: e95ac14a auto
// OFFSET: 0x00537600
static int32_t Script_BNGetSelectedFriend(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 4582bd29 manual
// OFFSET: 0x005376C0
static int32_t Script_BNSetMatureLanguageFilter(lua_State* L) {
    return 0;
}

// VERIFY: 197ffe0c manual
// OFFSET: 0x005377C0
static int32_t Script_BNGetMatureLanguageFilter(lua_State* L) {
    return 0;
}

// VERIFY: a2d38f77 auto
// OFFSET: 0x005378A0
static int32_t Script_BNIsSelf(lua_State* L) {
    return 0;
}

// VERIFY: 216d7baa auto
// OFFSET: 0x00537950
static int32_t Script_BNIsFriend(lua_State* L) {
    return 0;
}

// VERIFY: 2e29ab78 auto
// OFFSET: 0x00537A00
static int32_t Script_BNGetMaxPlayersInConversation(lua_State* L) {
    return 0;
}

// VERIFY: ea8efcda manual
// OFFSET: 0x00539BF0
static int32_t Script_BNGetFriendInfo(lua_State* L) {
    return 0;
}

// VERIFY: 9d2f21a7 auto
// OFFSET: 0x00539CC0
static int32_t Script_BNGetFriendInfoByID(lua_State* L) {
    return 0;
}

// VERIFY: e2c4f578 manual
// OFFSET: 0x00539D70
static int32_t Script_BNGetFriendToonInfo(lua_State* L) {
    return 0;
}

// VERIFY: 0ae9bf15 auto
// OFFSET: 0x00539F90
static int32_t Script_BNGetToonInfo(lua_State* L) {
    return 0;
}

// VERIFY: 06b242a0 auto
// OFFSET: 0x0053A030
static int32_t Script_BNSendWhisper(lua_State* L) {
    return 0;
}

// VERIFY: 2daf6f26 manual
// OFFSET: 0x0053A150
static int32_t Script_BNGetConversationMemberInfo(lua_State* L) {
    return 0;
}

// VERIFY: 4f7df3a4 manual
// OFFSET: 0x0053A300
static int32_t Script_BNListConversation(lua_State* L) {
    return 0;
}

// VERIFY: 02e4cc86 manual
// OFFSET: 0x0053A540
static int32_t Script_BNGetBlockedInfo(lua_State* L) {
    return 0;
}

// VERIFY: c59a8d80 auto
// OFFSET: 0x0053A660
static int32_t Script_BNRequestFOFInfo(lua_State* L) {
    return 0;
}

// STATIC: 0x00ACCAE8
static FrameScript_Method s_ScriptFunctions[] = {
    { "BNGetInfo", &Script_BNGetInfo },
    { "BNGetNumFriends", &Script_BNGetNumFriends },
    { "BNGetFriendInfo", &Script_BNGetFriendInfo },
    { "BNGetFriendInfoByID", &Script_BNGetFriendInfoByID },
    { "BNGetNumFriendToons", &Script_BNGetNumFriendToons },
    { "BNGetFriendToonInfo", &Script_BNGetFriendToonInfo },
    { "BNGetToonInfo", &Script_BNGetToonInfo },
    { "BNRemoveFriend", &Script_BNRemoveFriend },
    { "BNSetFriendNote", &Script_BNSetFriendNote },
    { "BNSetSelectedFriend", &Script_BNSetSelectedFriend },
    { "BNGetSelectedFriend", &Script_BNGetSelectedFriend },
    { "BNGetNumFriendInvites", &Script_BNGetNumFriendInvites },
    { "BNGetFriendInviteInfo", &Script_BNGetFriendInviteInfo },
    { "BNSendFriendInvite", &Script_BNSendFriendInvite },
    { "BNSendFriendInviteByID", &Script_BNSendFriendInviteByID },
    { "BNAcceptFriendInvite", &Script_BNAcceptFriendInvite },
    { "BNDeclineFriendInvite", &Script_BNDeclineFriendInvite },
    { "BNReportFriendInvite", &Script_BNReportFriendInvite },
    { "BNSetAFK", &Script_BNSetAFK },
    { "BNSetDND", &Script_BNSetDND },
    { "BNSetCustomMessage", &Script_BNSetCustomMessage },
    { "BNGetCustomMessageTable", &Script_BNGetCustomMessageTable },
    { "BNSetFocus", &Script_BNSetFocus },
    { "BNSendWhisper", &Script_BNSendWhisper },
    { "BNCreateConversation", &Script_BNCreateConversation },
    { "BNInviteToConversation", &Script_BNInviteToConversation },
    { "BNLeaveConversation", &Script_BNLeaveConversation },
    { "BNSendConversationMessage", &Script_BNSendConversationMessage },
    { "BNGetNumConversationMembers", &Script_BNGetNumConversationMembers },
    { "BNGetConversationMemberInfo", &Script_BNGetConversationMemberInfo },
    { "BNGetConversationInfo", &Script_BNGetConversationInfo },
    { "BNListConversation", &Script_BNListConversation },
    { "BNGetNumBlocked", &Script_BNGetNumBlocked },
    { "BNGetBlockedInfo", &Script_BNGetBlockedInfo },
    { "BNIsBlocked", &Script_BNIsBlocked },
    { "BNSetBlocked", &Script_BNSetBlocked },
    { "BNSetSelectedBlock", &Script_BNSetSelectedBlock },
    { "BNGetSelectedBlock", &Script_BNGetSelectedBlock },
    { "BNGetNumBlockedToons", &Script_BNGetNumBlockedToons },
    { "BNGetBlockedToonInfo", &Script_BNGetBlockedToonInfo },
    { "BNIsToonBlocked", &Script_BNIsToonBlocked },
    { "BNSetToonBlocked", &Script_BNSetToonBlocked },
    { "BNSetSelectedToonBlock", &Script_BNSetSelectedToonBlock },
    { "BNGetSelectedToonBlock", &Script_BNGetSelectedToonBlock },
    { "BNReportPlayer", &Script_BNReportPlayer },
    { "BNConnected", &Script_BNConnected },
    { "BNFeaturesEnabledAndConnected", &Script_BNFeaturesEnabledAndConnected },
    { "IsBNLogin", &Script_IsBNLogin },
    { "BNFeaturesEnabled", &Script_BNFeaturesEnabled },
    { "BNRequestFOFInfo", &Script_BNRequestFOFInfo },
    { "BNGetNumFOF", &Script_BNGetNumFOF },
    { "BNGetFOFInfo", &Script_BNGetFOFInfo },
    { "BNSetMatureLanguageFilter", &Script_BNSetMatureLanguageFilter },
    { "BNGetMatureLanguageFilter", &Script_BNGetMatureLanguageFilter },
    { "BNIsSelf", &Script_BNIsSelf },
    { "BNIsFriend", &Script_BNIsFriend },
    { "BNGetMaxPlayersInConversation", &Script_BNGetMaxPlayersInConversation },
};

// VERIFY: e50e8083 auto
// OFFSET: 0x00530F60
void BattlenetUI_RegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_ScriptFunctions[i].m_name, s_ScriptFunctions[i].m_method);
    }
}

// INLINED
void BattlenetUI_UnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_ScriptFunctions[i].m_name);
    }
}
