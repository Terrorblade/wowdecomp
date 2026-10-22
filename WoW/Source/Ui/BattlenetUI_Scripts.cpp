#include "BattlenetUI.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Ui/ChatFrame.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/WowServices/BattlenetLogin.h"
#include "WoW/Source/WowServices/Login.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

// VERIFY: f2fecb1f auto
// OFFSET: 0x00530EC0
static int32_t Script_BNConnected(lua_State* L) {
    bool connected = false;
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1) {
        int32_t state = static_cast<BattlenetLogin*>(login)->m_serviceState;

        if (state == 7 || state == 6 || state == 3) {
            connected = true;
        }
    }

    lua_pushboolean(L, connected);
    return 1;
}

// VERIFY: 512bf591 auto
// OFFSET: 0x00530F20
static int32_t Script_IsBNLogin(lua_State* L) {
    Login* login = ClientServices::LoginConnection();
    lua_pushboolean(L, login && login->GetLoginServerType() == 1);
    return 1;
}

// VERIFY: 515f5bba manual
// OFFSET: 0x005343F0
static int32_t Script_BNGetInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    uint32_t accountId = BattlenetAPI::GetPresenceIDForCurrentAccount();
    lua_pushnumber(L, accountId);
    DebugLog("BNUI: GetInfo account ID is %u", accountId);

    uint32_t toonId = BattlenetAPI::GetPresenceIDForCurrentToon();
    lua_pushnumber(L, toonId);
    DebugLog("BNUI: GetInfo toon ID is %u", toonId);

    Battlenet::Presence::FieldVal broadcast = BattlenetAPI::GetPresenceValue(toonId, BattlenetUI::s_presenceCallbacks[BN_PRESENCE_BROADCAST].m_field);

    if (broadcast.m_tag == 13) {
        lua_pushstring(L, broadcast.m_string.m_data);
        DebugLog("BNUI: GetInfo custom message is %s", broadcast.m_string.m_data);
    } else {
        lua_pushnil(L);
        DebugLog("BNUI: GetInfo custom message is <none>");
    }

    Battlenet::Presence::FieldVal value = BattlenetAPI::GetPresenceValue(toonId, BattlenetUI::s_presenceCallbacks[BN_PRESENCE_AFK].m_field);
    uint8_t isAFK = value.m_tag == 11 ? value.m_bool : 0;
    lua_pushboolean(L, isAFK);
    DebugLog("BNUI: GetInfo AFK is %d", isAFK);

    value = BattlenetAPI::GetPresenceValue(toonId, BattlenetUI::s_presenceCallbacks[BN_PRESENCE_DND].m_field);
    uint8_t isDND = value.m_tag == 11 ? value.m_bool : 0;
    lua_pushboolean(L, isDND);
    DebugLog("BNUI: GetInfo DND is %d", isDND);

    return 5;
}

// VERIFY: f2a0fb12 manual
// OFFSET: 0x00534590
static int32_t Script_BNGetNumFriends(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled() || !BattlenetUI::m_friendListLoaded) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }

    if (!BattlenetUI_IsAvailable()) {
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 2;
    }

    lua_pushnumber(L, BattlenetUI::m_friends.m_count);

    uint32_t online = 0;

    for (uint32_t i = 0; i < BattlenetUI::m_friends.m_count; i++) {
        Battlenet::Presence::FieldVal value = BattlenetAPI::GetPresenceValue(BattlenetUI::m_friends.m_data[i].m_presenceID, BattlenetUI::s_presenceCallbacks[BN_PRESENCE_ONLINE].m_field);

        if (value.m_tag == 1 && value.m_bool) {
            online++;
        }
    }

    lua_pushnumber(L, online);
    return 2;
}

// VERIFY: ba9e1544 manual
// OFFSET: 0x005349F0
static int32_t Script_BNGetNumFriendToons(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetNumFriendToons(index)");
    }

    double index;

    if (!BattlenetUI::m_friendListLoaded || (index = lua_tonumber(L, 1), index - 1 >= BattlenetUI::m_friends.m_count)) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    uint32_t presenceId = BattlenetUI::m_friends[static_cast<uint32_t>(static_cast<int64_t>(index - 1))].m_presenceID;
    uint32_t numToons = BattlenetAPI::GetNumToons(presenceId);
    uint32_t online = 0;

    for (uint32_t i = 0; i < numToons; i++) {
        Battlenet::Presence::FieldVal value = BattlenetAPI::GetPresenceValue(BattlenetAPI::GetToon(presenceId, i), BattlenetUI::s_presenceCallbacks[BN_PRESENCE_ONLINE].m_field);

        if (value.m_tag == 1 && value.m_bool) {
            online++;
        }
    }

    lua_pushnumber(L, online);
    DebugLog("BNUI: GetNumFriendToons is %d", online);
    return 1;
}

// VERIFY: 6c9894e6 auto
// OFFSET: 0x00534ED0
static int32_t Script_BNRemoveFriend(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1)) {
            luaL_error(L, "Usage: BNRemoveFriend(ID)");
        }

        if (BattlenetUI::m_friendListLoaded) {
            BattlenetAPI::RemoveFriend(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))));
        }
    }

    return 0;
}

// VERIFY: 6e07b4a2 auto
// OFFSET: 0x00534F80
static int32_t Script_BNSetFriendNote(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1)) {
            luaL_error(L, "Usage: BNSetFriendNote(ID, noteText)");
        }

        if (BattlenetUI::m_friendListLoaded) {
            if (!lua_isstring(L, 2)) {
                luaL_error(L, "Usage: BNSetFriendNote(ID, noteText)");
            }

            double presenceId = lua_tonumber(L, 1);
            const char* note = lua_tolstring(L, 2, nullptr);

            if (SStrLenUTF8(note) > 255) {
                luaL_error(L, "Friend note text too long, maximum length %d chars", 255);
            }

            BattlenetAPI::SetFriendNote(static_cast<uint32_t>(static_cast<int64_t>(presenceId)), note);
        }
    }

    return 0;
}

// VERIFY: 5f5b8f58 manual
// OFFSET: 0x00535080
static int32_t Script_BNGetNumFriendInvites(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled() || !BattlenetUI::m_friendInvites) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (BattlenetUI_IsAvailable()) {
        uint32_t count = BattlenetUI::m_friendInvites->m_data.m_invites.m_count;
        lua_pushnumber(L, count);
        DebugLog("BNUI: Num friend invites: %d", count);
    } else {
        lua_pushnumber(L, 0.0);
        DebugLog("BNUI: Num friend invites: 0, NOT CONNNECTED");
    }

    return 1;
}

// VERIFY: 0258b95f manual
// OFFSET: 0x00535180
static int32_t Script_BNGetFriendInviteInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetFriendInviteInfo(index)");
    }

    double number = lua_tonumber(L, 1);
    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(number - 1));
    BSN::Array<Battlenet::Friends::FriendInvite, 100>& invites = BattlenetUI::m_friendInvites->m_data.m_invites;

    if (index >= invites.m_count) {
        return 0;
    }

    uint32_t presenceId = invites[static_cast<uint32_t>(static_cast<int64_t>(number - 1))].m_presenceId;

    if (!presenceId) {
        luaL_error(L, "No Friend at index");
    }

    Battlenet::Presence::FieldVal name = BattlenetAPI::GetPresenceValue(presenceId, BattlenetUI::s_presenceCallbacks[BN_PRESENCE_ACCOUNT_NAME].m_field);
    lua_pushnumber(L, presenceId);
    DebugLog("BNUI: Invite Info ID: %u", presenceId);

    if (name.m_tag == 18) {
        const char* givenName = name.Get<18, Battlenet::Presence::AccountName>().m_givenName.m_data;
        const char* surname = name.Get<18, Battlenet::Presence::AccountName>().m_surname.m_data;
        lua_pushstring(L, givenName);
        lua_pushstring(L, surname);
        DebugLog("BNUI: Invite Info Account name: %s %s", givenName, surname);
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
    }

    if (invites[index].m_message.m_exists) {
        const char* message = invites[index].m_message.Get().m_data;
        lua_pushstring(L, message);
        DebugLog("BNUI: Invite Info message: %s", message);
    } else {
        lua_pushnil(L);
    }

    int32_t time = invites[index].m_time;
    lua_pushnumber(L, time);
    DebugLog("BNUI: Invite Info time: %d", time);

    return 5;
}

// VERIFY: c1a1acff auto
// OFFSET: 0x00535380
static int32_t Script_BNSendFriendInvite(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isstring(L, 1)) {
            luaL_error(L, "Usage: BNSendFriendInvite(text, noteText)");
        }

        const char* text = lua_tolstring(L, 1, nullptr);
        char note[1024];

        if (lua_isstring(L, 2)) {
            const char* noteText = lua_tolstring(L, 2, nullptr);

            if (SStrLenUTF8(noteText) > 255) {
                luaL_error(L, "Invite note text too long, maximum length %d chars", 255);
            }

            if (noteText) {
                FilterChatLinks(noteText, note, 1021);
            } else {
                note[0] = '\0';
            }
        } else {
            note[0] = '\0';
        }

        BattlenetAPI::SendRIDFriendInviteByEmail(text, note, &BattlenetUI::s_friendInviteSentCB);
    }

    return 0;
}

// VERIFY: 93bf8940 auto
// OFFSET: 0x00535490
static int32_t Script_BNSendFriendInviteByID(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isstring(L, 1)) {
            luaL_error(L, "Usage: BNSendFriendInviteByID(ID, noteText)");
        }

        double presenceId = lua_tonumber(L, 1);
        char note[1024];

        if (lua_isstring(L, 2)) {
            const char* noteText = lua_tolstring(L, 2, nullptr);

            if (SStrLenUTF8(noteText) > 255) {
                luaL_error(L, "Invite note text too long, maximum length %d chars", 255);
            }

            if (noteText) {
                FilterChatLinks(noteText, note, 1021);
            } else {
                note[0] = '\0';
            }
        } else {
            note[0] = '\0';
        }

        BattlenetAPI::SendRIDFriendInviteByPresenceId(static_cast<uint32_t>(static_cast<int64_t>(presenceId)), note, &BattlenetUI::s_friendInviteSentCB);
    }

    return 0;
}

// VERIFY: 3fea367a auto
// OFFSET: 0x005355C0
static int32_t Script_BNAcceptFriendInvite(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1)) {
            luaL_error(L, "Usage: BNAcceptFriendInvite(ID)");
        }

        BattlenetAPI::AcceptFriendInvite(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))));
    }

    return 0;
}

// VERIFY: 4a1d39b8 auto
// OFFSET: 0x00535660
static int32_t Script_BNDeclineFriendInvite(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1)) {
            luaL_error(L, "Usage: BNDeclineFriendInvite(ID)");
        }

        BattlenetAPI::DeclineFriendInvite(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))));
    }

    return 0;
}

// VERIFY: c0f9de03 auto
// OFFSET: 0x00535700
static int32_t Script_BNReportFriendInvite(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1)) {
            luaL_error(L, "Usage: BNReportFriendInvite(ID)");
        }

        BattlenetAPI::ReportFriendInvite(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))));
    }

    return 0;
}

// VERIFY: 47c2f002 auto
// OFFSET: 0x005357A0
static int32_t Script_BNSetAFK(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (lua_type(L, 1) != LUA_TBOOLEAN) {
            luaL_error(L, "Usage: BNSetAFK(bool)");
        }

        bool afk = lua_toboolean(L, 1) != 0;
        Battlenet::Presence::FieldVal value;
        value.m_tag = 11;
        value.m_bool = afk;
        BattlenetAPI::SetPresenceValue(BattlenetUI::s_presenceCallbacks[BN_PRESENCE_AFK].m_field, &value);
    }

    return 0;
}

// VERIFY: bf638620 auto
// OFFSET: 0x00535860
static int32_t Script_BNSetDND(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (lua_type(L, 1) != LUA_TBOOLEAN) {
            luaL_error(L, "Usage: BNSetDND(bool)");
        }

        bool dnd = lua_toboolean(L, 1) != 0;
        Battlenet::Presence::FieldVal value;
        value.m_tag = 11;
        value.m_bool = dnd;
        BattlenetAPI::SetPresenceValue(BattlenetUI::s_presenceCallbacks[BN_PRESENCE_DND].m_field, &value);
    }

    return 0;
}

// VERIFY: 4af5fdd0 manual
// OFFSET: 0x00535920
static int32_t Script_BNSetCustomMessage(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isstring(L, 1)) {
            luaL_error(L, "Usage: BNSetCustomMessage(text)");
        }

        if (++BattlenetUI::s_broadcastCount >= 2) {
            float now = OsGetAsyncTimeSec();

            if (now - BattlenetUI::s_broadcastTime < 3.0) {
                if (BattlenetUI::s_broadcastCount >= 2) {
                    CGGameUI::DisplayError(ERR_BN_BROADCAST_THROTTLE);
                    return 0;
                }
            } else {
                BattlenetUI::s_broadcastTime = now;
                BattlenetUI::s_broadcastCount = 0;
            }
        }

        const char* text = lua_tolstring(L, 1, nullptr);

        if (SStrLenUTF8(text) > 127) {
            luaL_error(L, "Message text too long, maximum length %d chars", 127);
        }

        char message[512];
        FilterChatLinks(text, message, 509);

        Battlenet::Presence::FieldVal value;
        *value.Set<13, BSN::String<127>>() = message;
        BattlenetAPI::SetPresenceValue(BattlenetUI::s_presenceCallbacks[BN_PRESENCE_BROADCAST].m_field, &value);
    }

    return 0;
}

// VERIFY: e1af46d8 manual
// OFFSET: 0x00535AA0
static int32_t Script_BNGetCustomMessageTable(lua_State* L) {
    Login* login = ClientServices::LoginConnection();
    uint32_t i = 0;

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (lua_type(L, 1) != LUA_TTABLE) {
        luaL_error(L, "Usage: BNGetCustomMessageTable(table)");
    }

    if (!BattlenetUI::m_friendListLoaded) {
        lua_pushnil(L);
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 3;
    }

    uint32_t online = 0;
    uint32_t offline = 0;

    while (i < BattlenetUI::m_friends.m_count) {
        Battlenet::Presence::FieldVal broadcast = BattlenetAPI::GetPresenceValue(BattlenetUI::m_friends.m_data[i].m_presenceID, BattlenetUI::s_presenceCallbacks[BN_PRESENCE_BROADCAST].m_field);
        uint8_t hasMessage = 0;

        if (broadcast.m_tag == 13 && SStrLenUTF8(broadcast.m_string.m_data)) {
            hasMessage = 1;

            Battlenet::Presence::FieldVal value = BattlenetAPI::GetPresenceValue(BattlenetUI::m_friends[i].m_presenceID, BattlenetUI::s_presenceCallbacks[BN_PRESENCE_ONLINE].m_field);

            if (value.m_tag == 1 && value.m_bool) {
                online++;
            } else {
                offline++;
            }
        }

        lua_checkstack(L, 2);
        lua_pushnumber(L, ++i);
        lua_pushboolean(L, hasMessage);
        lua_settable(L, -3);
    }

    lua_pushnumber(L, online);
    lua_pushnumber(L, offline);
    return 3;
}

// VERIFY: d3aaf112 auto
// OFFSET: 0x00535C60
static int32_t Script_BNSetFocus(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        Battlenet::Presence::FieldVal value;
        value.m_tag = 0;
        BattlenetAPI::SetPresenceValue(BattlenetUI::s_presenceCallbacks[BN_PRESENCE_FOCUS].m_field, &value);
    }

    return 0;
}

// VERIFY: a26abed5 manual
// OFFSET: 0x00535CE0
static int32_t Script_BNCreateConversation(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    Battlenet::Toon::FullName first;
    Battlenet::Toon::FullName second;

    if (!BattlenetUI_IsAvailable()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNCreateConversation(id,id)");
    }

    Battlenet::Presence::FieldVal toon = BattlenetAPI::GetPresenceValue(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))), BattlenetUI::s_presenceCallbacks[BN_PRESENCE_TOON_NAME].m_field);

    if (toon.m_tag != 17) {
        return 0;
    }

    first = toon.m_toonName;

    if (!lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: BNCreateConversation(id,id)");
    }

    toon = BattlenetAPI::GetPresenceValue(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 2))), BattlenetUI::s_presenceCallbacks[BN_PRESENCE_TOON_NAME].m_field);

    if (toon.m_tag != 17) {
        return 0;
    }

    second = toon.Get<17, Battlenet::Toon::FullName>();
    BattlenetAPI::CreateChatConversation(&first, &second, BattlenetUI::s_createConversationCB);

    lua_pushboolean(L, 1);
    return 1;
}

// VERIFY: 72a452b9 manual
// OFFSET: 0x00535EB0
static int32_t Script_BNInviteToConversation(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    Battlenet::Toon::FullName toon;

    if (!BattlenetUI_IsAvailable()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNInviteToConversation(channel,id)");
    }

    uint8_t channel = static_cast<uint8_t>(static_cast<int64_t>(lua_tonumber(L, 1) - 1.0));

    if (channel >= 6) {
        luaL_error(L, "Invalid Channel");
    }

    if (!lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: BNInviteToConversation(channel,id)");
    }

    Battlenet::Presence::FieldVal value = BattlenetAPI::GetPresenceValue(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 2))), BattlenetUI::s_presenceCallbacks[BN_PRESENCE_TOON_NAME].m_field);

    if (value.m_tag != 17) {
        return 0;
    }

    toon = value.Get<17, Battlenet::Toon::FullName>();
    BattlenetAPI::SendChatConversationInvite(channel, &toon, BattlenetUI::s_inviteToConversationCB);

    return 0;
}

// VERIFY: f58e7e6b manual
// OFFSET: 0x00536030
static int32_t Script_BNLeaveConversation(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNLeaveConversation(channel)");
    }

    if (!BattlenetUI_IsAvailable()) {
        return 0;
    }

    double channel = lua_tonumber(L, 1) - 1.0;

    if (channel >= 6) {
        luaL_error(L, "Invalid Channel");
    }

    BattlenetAPI::LeaveChatConversation(static_cast<uint8_t>(static_cast<int64_t>(channel)));
    return 0;
}

// VERIFY: 626f8a47 auto
// OFFSET: 0x00536110
static int32_t Script_BNSendConversationMessage(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1) || !lua_isstring(L, 2)) {
            luaL_error(L, "Usage: BNSendConversationMessage(channel,text)");
        }

        if (BattlenetUI_IsAvailable()) {
            int32_t channelNumber = static_cast<int32_t>(lua_tonumber(L, 1) - 1.0);
            uint8_t channel = channelNumber;
            const char* text = lua_tolstring(L, 2, nullptr);
            char message[1026];
            FilterChatLinks(text, message, 1021);

            if (channel >= 6) {
                luaL_error(L, "Invalid Channel");
            }

            BattlenetAPI::SendChatMessage(channelNumber, message);
        }
    }

    return 0;
}

// VERIFY: 84553829 manual
// OFFSET: 0x00536220
static int32_t Script_BNGetNumConversationMembers(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetNumConversationMembers(channel)");
    }

    double count;

    if (BattlenetUI_IsAvailable()) {
        int32_t channel = static_cast<int32_t>(lua_tonumber(L, 1) - 1.0);

        if (static_cast<uint8_t>(channel) >= 6) {
            luaL_error(L, "Invalid Channel");
        }

        count = BattlenetAPI::GetNumChatMembers(channel);
    } else {
        count = 0.0;
    }

    lua_pushnumber(L, count);
    return 1;
}

// VERIFY: 7432c6ae manual
// OFFSET: 0x00536330
static int32_t Script_BNGetConversationInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetConversationInfo(channel)");
    }

    double channel = lua_tonumber(L, 1) - 1.0;

    if (BattlenetAPI::GetChatChannelType(static_cast<uint8_t>(static_cast<int64_t>(channel))) == 2) {
        lua_pushstring(L, "conversation");
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 3cb99631 manual
// OFFSET: 0x00536400
static int32_t Script_BNGetNumBlocked(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (!BattlenetUI_IsAvailable()) {
        lua_pushnumber(L, 0.0);
        DebugLog("BNUI: Num Blocked is 0. NOT CONNECTED");
        return 1;
    }

    Battlenet::Presence::BlockList blockList = BattlenetAPI::GetBlockList();
    lua_pushnumber(L, blockList.m_accounts.m_count);
    DebugLog("BNUI: Num Blocked is %d", blockList.m_accounts.m_count);
    return 1;
}

// VERIFY: e9831e9e auto
// OFFSET: 0x005364E0
static int32_t Script_BNIsBlocked(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNIsBlocked(ID)");
    }

    uint32_t presenceId = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));
    bool blocked = BattlenetAPI::IsRIDBlock(presenceId);
    lua_pushboolean(L, blocked);
    DebugLog("BNUI: Is Blocked for ID %u %s", presenceId, blocked ? "true" : "false");

    return 1;
}

// VERIFY: ede7e82b auto
// OFFSET: 0x005365B0
static int32_t Script_BNSetBlocked(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1) || lua_type(L, 2) != LUA_TBOOLEAN) {
            luaL_error(L, "Usage: BNSetBlocked(ID, true/false)");
        }

        uint32_t presenceId = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));

        if (lua_toboolean(L, 2)) {
            BattlenetAPI::AddRIDBlock(presenceId);
            DebugLog("BNUI: Adding block for ID %u", presenceId);
            return 0;
        }

        BattlenetAPI::RemoveRIDBlock(presenceId);
        DebugLog("BNUI: Removing block for ID %u", presenceId);
    }

    return 0;
}

// VERIFY: a99f894a manual
// OFFSET: 0x005366A0
static int32_t Script_BNSetSelectedBlock(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNSetSelectedBlock(index)");
    }

    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;
    Battlenet::Presence::BlockList blockList = BattlenetAPI::GetBlockList();

    if (index >= blockList.m_accounts.m_count) {
        luaL_error(L, "Block index %d too large, only %d blocked.", index + 1, blockList.m_accounts.m_count);
    }

    uint32_t presenceId = blockList.m_accounts.m_data[index];

    if (!presenceId) {
        SErrDisplayAppFatalMessage("", "", 0);
    }

    BattlenetUI::s_selectedBlock = presenceId;
    return 0;
}

// VERIFY: 4ec1f3a8 manual
// OFFSET: 0x00536790
static int32_t Script_BNGetSelectedBlock(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (BattlenetUI::s_selectedBlock) {
        Battlenet::Presence::BlockList blockList = BattlenetAPI::GetBlockList();

        for (uint32_t i = 0; i < blockList.m_accounts.m_count; i++) {
            if (blockList.m_accounts.m_data[i] == BattlenetUI::s_selectedBlock) {
                lua_pushnumber(L, i + 1);
                return 1;
            }
        }
    }

    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 3d19868a manual
// OFFSET: 0x00536890
static int32_t Script_BNGetNumBlockedToons(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (!BattlenetUI_IsAvailable()) {
        lua_pushnumber(L, 0.0);
        DebugLog("BNUI: Num Blocked toons is 0. NOT CONNECTED");
        return 1;
    }

    Battlenet::Presence::BlockList blockList = BattlenetAPI::GetBlockList();
    lua_pushnumber(L, blockList.m_toons.m_count);
    DebugLog("BNUI: Num Blocked toons is %d", blockList.m_toons.m_count);
    return 1;
}

// VERIFY: fa351982 manual
// OFFSET: 0x00536970
static int32_t Script_BNGetBlockedToonInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetBlockedToonInfo(index)");
    }

    Battlenet::Presence::BlockList blockList = BattlenetAPI::GetBlockList();
    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1) - 1.0));

    if (index >= blockList.m_toons.m_count) {
        luaL_error(L, "Invalid Index");
    }

    uint32_t presenceId = blockList.m_toons.m_data[index];
    lua_pushnumber(L, presenceId);

    const char* name = BattlenetAPI::GetExactNameForPresenceId(presenceId, false);
    lua_pushstring(L, name);
    DebugLog("BNUI: Blocked Toon info. index %d, ID %u, name: %s", index + 1, presenceId, name);

    return 2;
}

// VERIFY: b9dcb576 auto
// OFFSET: 0x00536A90
static int32_t Script_BNIsToonBlocked(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNIsToonBlocked(ID)");
    }

    uint32_t presenceId = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));
    bool blocked = BattlenetAPI::IsCIDBlock(presenceId);
    lua_pushboolean(L, blocked);
    DebugLog("BNUI: Is Toon Blocked for ID %u %s", presenceId, blocked ? "true" : "false");

    return 1;
}

// VERIFY: a38c9f58 auto
// OFFSET: 0x00536B60
static int32_t Script_BNSetToonBlocked(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1) || lua_type(L, 2) != LUA_TBOOLEAN) {
            luaL_error(L, "Usage: BNSetToonBlocked(ID, true/false)");
        }

        uint32_t presenceId = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));

        if (lua_toboolean(L, 2)) {
            BattlenetAPI::AddCIDBlock(presenceId);
            DebugLog("BNUI: Adding block for toon ID %u", presenceId);
            return 0;
        }

        BattlenetAPI::RemoveCIDBlock(presenceId);
        DebugLog("BNUI: Removing block for toon ID %u", presenceId);
    }

    return 0;
}

// VERIFY: ae48a591 manual
// OFFSET: 0x00536C50
static int32_t Script_BNSetSelectedToonBlock(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNSetSelectedToonBlock(index)");
    }

    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;
    Battlenet::Presence::BlockList blockList = BattlenetAPI::GetBlockList();

    if (index >= blockList.m_toons.m_count) {
        luaL_error(L, "Toon Block index %d too large, only %d blocked.", index + 1, blockList.m_toons.m_count);
    }

    uint32_t presenceId = blockList.m_toons.m_data[index];

    if (!presenceId) {
        SErrDisplayAppFatalMessage("", "", 0);
    }

    BattlenetUI::s_selectedToonBlock = presenceId;
    return 0;
}

// VERIFY: 7ac120a9 manual
// OFFSET: 0x00536D40
static int32_t Script_BNGetSelectedToonBlock(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    if (BattlenetUI::s_selectedToonBlock) {
        Battlenet::Presence::BlockList blockList = BattlenetAPI::GetBlockList();

        for (uint32_t i = 0; i < blockList.m_toons.m_count; i++) {
            if (blockList.m_toons.m_data[i] == BattlenetUI::s_selectedToonBlock) {
                lua_pushnumber(L, i + 1);
                return 1;
            }
        }
    }

    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 3d16158e manual
// OFFSET: 0x00536E40
static int32_t Script_BNReportPlayer(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1) || !lua_isstring(L, 2)) {
            luaL_error(L, "Usage: BNReportPlayer(ID,typeText,note)");
        }

        const char* type = lua_tolstring(L, 2, nullptr);
        const char* note = nullptr;

        if (lua_isstring(L, 3)) {
            note = lua_tolstring(L, 3, nullptr);

            if (SStrLenUTF8(note, 509) > 127) {
                luaL_error(L, "Report note is too long.");
            }
        }

        int32_t reason;

        if (!SStrCmpI(type, "SPAM", STORM_MAX_STR)) {
            reason = 0;
        } else if (!SStrCmpI(type, "ABUSE", STORM_MAX_STR)) {
            reason = 1;
        } else if (!SStrCmpI(type, "THREAT", STORM_MAX_STR)) {
            reason = 2;
        } else {
            if (SStrCmpI(type, "NAME", STORM_MAX_STR)) {
                luaL_error(L, "Unknown Reason");
            }

            reason = 3;
        }

        if (!BattlenetAPI::ReportPlayer(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))), reason, note)) {
            CGChat::AddChatMessage(FrameScript_GetText("BNET_REPORT_SENT", -1, GENDER_NOT_APPLICABLE), 0, nullptr, 0, nullptr, nullptr, nullptr, WOWGUID(), 0, WOWGUID(), 0, false, nullptr);
        }
    }

    return 0;
}

// VERIFY: 32894792 auto
// OFFSET: 0x00537010
static int32_t Script_BNFeaturesEnabledAndConnected(lua_State* L) {
    Login* login = ClientServices::LoginConnection();
    lua_pushboolean(L, login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled() && static_cast<BattlenetLogin*>(login)->IsConnected());
    return 1;
}

// VERIFY: ed1e6ea4 auto
// OFFSET: 0x00537070
static int32_t Script_BNFeaturesEnabled(lua_State* L) {
    Login* login = ClientServices::LoginConnection();
    lua_pushboolean(L, login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled());
    return 1;
}

// VERIFY: 3eed4bc9 manual
// OFFSET: 0x005370D0
static int32_t Script_BNGetNumFOF(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetNumFOF(ID,mutual,non)");
    }

    double presenceId = lua_tonumber(L, 1);
    Battlenet::Friends::FriendsOfFriendList& list = BattlenetUI::s_friendsOfFriend->m_data;

    if (presenceId != list.m_presenceId) {
        DebugLog("BNUI: BNGetNumFOF requested for ID %u. Current FOF ID is %u. Num FOF is %d", presenceId, list.m_presenceId, list.m_friends.m_count);
        luaL_error(L, "Incorrect ID");
    }

    uint32_t mutual = 0;
    uint32_t nonMutual = 0;

    for (uint32_t i = 0; i < list.m_friends.m_count; i++) {
        if (!BattlenetAPI::IsSelf(list.m_friends.m_data[i])) {
            if (BattlenetAPI::IsRIDFriend(list.m_friends[i])) {
                mutual++;
            } else {
                nonMutual++;
            }
        }
    }

    lua_pushnumber(L, mutual);
    lua_pushnumber(L, nonMutual);
    DebugLog("BNUI: BNGetNumFOF is %d mutual, %d non-mutual", mutual, nonMutual);

    return 2;
}

// VERIFY: c5215d9a manual
// OFFSET: 0x00537240
static int32_t Script_BNGetFOFInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 4)) {
        luaL_error(L, "Usage: BNGetFOFInfo(ID, mutual, non, index)");
    }

    uint32_t presenceId = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));
    Battlenet::Friends::FriendsOfFriendList& list = BattlenetUI::s_friendsOfFriend->m_data;

    if (presenceId != list.m_presenceId) {
        luaL_error(L, "Incorrect ID");
    }

    int32_t mutual = lua_toboolean(L, 2);
    int32_t nonMutual = lua_toboolean(L, 3);
    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 4))) - 1;

    if (!mutual && !nonMutual) {
        luaL_error(L, "Must select mutual and/or non.");
    }

    uint32_t current = 0;
    uint8_t isMutual = 0;
    uint32_t i;

    for (i = 0; i < list.m_friends.m_count; i++) {
        if (BattlenetAPI::IsSelf(list.m_friends.m_data[i])) {
            continue;
        }

        if (BattlenetAPI::IsRIDFriend(list.m_friends[i])) {
            if (mutual) {
                if (current == index) {
                    isMutual = 1;
                    break;
                }

                current++;
            }
        } else if (nonMutual) {
            if (current == index) {
                break;
            }

            current++;
        }
    }

    if (i >= list.m_friends.m_count) {
        luaL_error(L, "No friend at that index.");
    }

    uint32_t friendId = list.m_friends[i];
    lua_pushnumber(L, friendId);

    const char* givenName = "none";
    const char* surname = "none";
    Battlenet::Presence::FieldVal name = BattlenetAPI::GetPresenceValue(friendId, BattlenetUI::s_presenceCallbacks[BN_PRESENCE_ACCOUNT_NAME].m_field);

    if (name.m_tag == 18) {
        givenName = name.m_accountName.m_givenName.m_data;
        surname = name.m_accountName.m_surname.m_data;
        lua_pushstring(L, givenName);
        lua_pushstring(L, surname);
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
    }

    lua_pushboolean(L, isMutual);
    DebugLog("BNUI: BNGetFOFInfo for ID %u index %d is ID %u, Acct name %s %s % mutual friend.", presenceId, index + 1, friendId, givenName, surname, isMutual ? "Is" : "Is not");

    return 4;
}

// VERIFY: 27fc51cd manual
// OFFSET: 0x00537510
static int32_t Script_BNSetSelectedFriend(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNSetSelectedFriend(index)");
    }

    if (!BattlenetUI::m_friendListLoaded) {
        return 0;
    }

    double index = lua_tonumber(L, 1);

    if (index - 1 >= BattlenetUI::m_friends.m_count) {
        luaL_error(L, "Friend index %d too large, only %d friends.", index, BattlenetUI::m_friends.m_count);
    }

    uint32_t presenceId = BattlenetUI::m_friends[static_cast<uint32_t>(static_cast<int64_t>(index - 1))].m_presenceID;

    if (!presenceId) {
        SErrDisplayAppFatalMessage("", "", 0);
    }

    BattlenetUI::s_selectedFriend = presenceId;
    return 0;
}

// VERIFY: e95ac14a auto
// OFFSET: 0x00537600
static int32_t Script_BNGetSelectedFriend(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled() && BattlenetUI::m_friendListLoaded && BattlenetUI::s_selectedFriend) {
        int32_t index = BattlenetUI::FindFriendIndex(BattlenetUI::s_selectedFriend);

        if (index < 0) {
            lua_pushnumber(L, 0.0);
        } else {
            lua_pushnumber(L, index + 1);
        }

        return 1;
    }

    lua_pushnumber(L, 0.0);
    return 1;
}

// VERIFY: 4582bd29 manual
// OFFSET: 0x005376C0
static int32_t Script_BNSetMatureLanguageFilter(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (lua_type(L, 1) != LUA_TBOOLEAN) {
            luaL_error(L, "Usage: BNSetMatureLanguageFilter(true/false)");
        }

        if (!BattlenetUI_IsAvailable()) {
            luaL_error(L, "not connected to Battle.net");
        }

        int32_t enabled = lua_toboolean(L, 1);
        GeneralFunction::Arg value;
        value = static_cast<int64_t>(enabled);
        BattlenetAPI::SetSetting("Chat.ProfanityFilterEnabled", value, true);
        DebugLog("BNUI: Set mature language filter %s", enabled ? "on" : "off");
    }

    return 0;
}

// VERIFY: 197ffe0c manual
// OFFSET: 0x005377C0
static int32_t Script_BNGetMatureLanguageFilter(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!BattlenetUI_IsAvailable()) {
        luaL_error(L, "not connected to Battle.net");
    }

    GeneralFunction::Arg setting = BattlenetAPI::GetSetting("Chat.ProfanityFilterEnabled");
    bool enabled = setting.ToBool();
    DebugLog("BNUI: mature language filter is %s", enabled ? "on" : "off");
    lua_pushboolean(L, enabled);

    return 1;
}

// VERIFY: a2d38f77 auto
// OFFSET: 0x005378A0
static int32_t Script_BNIsSelf(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNIsSelf(presenceID)");
    }

    lua_pushboolean(L, BattlenetAPI::IsSelf(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)))));
    return 1;
}

// VERIFY: 216d7baa auto
// OFFSET: 0x00537950
static int32_t Script_BNIsFriend(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNIsFriend(presenceID)");
    }

    lua_pushboolean(L, BattlenetAPI::HasRIDFriendship(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)))));
    return 1;
}

// VERIFY: 2e29ab78 auto
// OFFSET: 0x00537A00
static int32_t Script_BNGetMaxPlayersInConversation(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    lua_pushnumber(L, 12.0);
    return 1;
}

// VERIFY: ea8efcda manual
// OFFSET: 0x00539BF0
static int32_t Script_BNGetFriendInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetFriendInfo(index)");
    }

    if (!BattlenetUI::m_friendListLoaded) {
        return 0;
    }

    double index = lua_tonumber(L, 1);

    if (index - 1 >= BattlenetUI::m_friends.m_count) {
        luaL_error(L, "Friend index %d too large, only %d friends.", index, BattlenetUI::m_friends.m_count);
    }

    return BattlenetUI::BNGetFriendInfoHelper(L, static_cast<uint32_t>(static_cast<int64_t>(index - 1)));
}

// VERIFY: 9d2f21a7 auto
// OFFSET: 0x00539CC0
static int32_t Script_BNGetFriendInfoByID(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetFriendInfoByID(ID)");
    }

    int32_t index;

    if (BattlenetUI::m_friendListLoaded && (index = BattlenetUI::FindFriendIndex(static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))))) >= 0) {
        return BattlenetUI::BNGetFriendInfoHelper(L, index);
    }

    return 0;
}

// VERIFY: e2c4f578 manual
// OFFSET: 0x00539D70
static int32_t Script_BNGetFriendToonInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: BNGetToonInfo(friendIndex,toonIndex)");
    }

    if (!BattlenetUI::m_friendListLoaded) {
        return 0;
    }

    uint32_t friendIndex = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1) - 1));
    double toonNumber = lua_tonumber(L, 2);
    uint32_t toonIndex = static_cast<uint32_t>(static_cast<int64_t>(toonNumber - 1));

    if (friendIndex >= BattlenetUI::m_friends.m_count) {
        luaL_error(L, "Friend index %d too large, only %d friends.", friendIndex + 1, BattlenetUI::m_friends.m_count);
    }

    uint32_t presenceId = BattlenetUI::m_friends[friendIndex].m_presenceID;
    uint32_t numToons = BattlenetAPI::GetNumToons(presenceId);

    if (toonIndex >= numToons) {
        luaL_error(L, "Toon index %d too large, only %d toons.", toonNumber, numToons);
    }

    uint32_t i;
    uint32_t online = 0;

    for (i = 0; i < numToons; i++) {
        Battlenet::Presence::FieldVal value = BattlenetAPI::GetPresenceValue(BattlenetAPI::GetToon(presenceId, i), BattlenetUI::s_presenceCallbacks[BN_PRESENCE_ONLINE].m_field);

        if (value.m_tag == 1 && value.Get<1, bool>()) {
            if (online == toonIndex) {
                break;
            }

            online++;
        }
    }

    if (i >= numToons) {
        luaL_error(L, "Couldn't find a toon at friend index %d, online toon index %d", friendIndex + 1, i + 1);
    }

    uint32_t toon = BattlenetAPI::GetToon(presenceId, i);

    if (!toon) {
        luaL_error(L, "Couldn't find a toon at friend index %d, toon index %d, online toon index %d", friendIndex + 1, i + 1, toonNumber - 1);
    }

    return BattlenetUI::BNGetToonInfoHelper(L, toon);
}

// VERIFY: 0ae9bf15 auto
// OFFSET: 0x00539F90
static int32_t Script_BNGetToonInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetToonInfo(ID)");
    }

    return BattlenetUI::BNGetToonInfoHelper(L, static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))));
}

// VERIFY: 06b242a0 auto
// OFFSET: 0x0053A030
static int32_t Script_BNSendWhisper(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1) || !lua_isstring(L, 2)) {
            luaL_error(L, "Usage: BNSendWhisper(id,text)");
        }

        if (BattlenetUI_IsAvailable()) {
            double presenceId = lua_tonumber(L, 1);
            const char* text = lua_tolstring(L, 2, nullptr);
            char message[1024];
            FilterChatLinks(text, message, 1021);

            Battlenet::Presence::FieldVal toon = BattlenetAPI::GetPresenceValue(static_cast<uint32_t>(static_cast<int64_t>(presenceId)), BattlenetUI::s_presenceCallbacks[BN_PRESENCE_TOON_NAME].m_field);

            if (toon.m_tag == 17) {
                BattlenetAPI::SendChatWhisper(static_cast<uint32_t>(static_cast<int64_t>(presenceId)), message);
                return 0;
            }

            BattlenetUI::DisplayError(318, static_cast<uint32_t>(static_cast<int64_t>(presenceId)));
        }
    }

    return 0;
}

// VERIFY: 2daf6f26 manual
// OFFSET: 0x0053A150
static int32_t Script_BNGetConversationMemberInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: BNGetConversationMemberInfo(channel,index)");
    }

    if (!BattlenetUI_IsAvailable()) {
        return 0;
    }

    uint8_t channel = static_cast<uint8_t>(static_cast<int64_t>(lua_tonumber(L, 1) - 1.0));
    double indexNumber = lua_tonumber(L, 2);
    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(indexNumber - 1));

    if (channel >= 6) {
        luaL_error(L, "Invalid Channel");
    }

    Battlenet::Chat::MemberList members = BattlenetAPI::GetChatMemberList(channel);

    if (index >= members.m_members.m_count) {
        luaL_error(L, "Invalid Index");
    }

    lua_pushnumber(L, BattlenetAPI::GetAccountPresenceId(members.m_members[static_cast<uint32_t>(static_cast<int64_t>(indexNumber - 1))]));
    lua_pushnumber(L, members.m_members[index]);

    const char* name = BattlenetUI::GetNameForPresenceIdHelper(members.m_members[index]);
    lua_pushstring(L, name);
    DebugLog("BNUI: ConversationMemberInfo name: %s", name);

    return 3;
}

// VERIFY: 4f7df3a4 manual
// OFFSET: 0x0053A300
static int32_t Script_BNListConversation(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (login && login->GetLoginServerType() == 1 && !login->IsTrialAccount() && BattlenetAPI::IsRIDEnabled()) {
        if (!lua_isnumber(L, 1)) {
            luaL_error(L, "Usage: BNListConversation(channel)");
        }

        double channelNumber = lua_tonumber(L, 1) - 1.0;
        uint8_t channel = static_cast<uint8_t>(static_cast<int64_t>(channelNumber));

        if (BattlenetAPI::GetChatChannelType(channel) == 2) {
            Battlenet::Chat::MemberList members = BattlenetAPI::GetChatMemberList(static_cast<uint8_t>(static_cast<int64_t>(channelNumber)));

            ChatConversation conversation;
            conversation.m_presenceID = BattlenetAPI::GetPresenceIDForCurrentAccount();
            conversation.m_index = channel;
            conversation.m_inactive = 0;

            const char* delimiter = FrameScript_GetText("PLAYER_LIST_DELIMITER", -1, GENDER_NOT_APPLICABLE);
            uint32_t delimiterLength = SStrLen(delimiter);
            char text[256];
            bool started = false;

            for (uint32_t i = 0; i < members.m_members.m_count; i++) {
                if (started) {
                    const char* name = BattlenetUI::GetNameForPresenceIdHelper(members.m_members.m_data[i]);

                    if (SStrLen(name) + delimiterLength + SStrLen(text) < 256) {
                        SStrPrintf(text, sizeof(text), "%s%s%s", text, delimiter, name);
                    } else {
                        CGChat::AddChatMessage(text, CHAT_MSG_BN_CONVERSATION_LIST, nullptr, 0, nullptr, nullptr, nullptr, WOWGUID(), 0, WOWGUID(), 0, false, &conversation);
                        text[0] = '\0';
                        SStrCopyUTF8(text, name, sizeof(text), sizeof(text));
                    }
                } else {
                    SStrCopyUTF8(text, BattlenetUI::GetNameForPresenceIdHelper(members.m_members.m_data[i]), sizeof(text), sizeof(text));
                    started = true;
                }
            }

            if (started) {
                CGChat::AddChatMessage(text, CHAT_MSG_BN_CONVERSATION_LIST, nullptr, 0, nullptr, nullptr, nullptr, WOWGUID(), 0, WOWGUID(), 0, false, &conversation);
            }
        }
    }

    return 0;
}

// VERIFY: 02e4cc86 manual
// OFFSET: 0x0053A540
static int32_t Script_BNGetBlockedInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNGetBlockedInfo(index)");
    }

    Battlenet::Presence::BlockList blockList = BattlenetAPI::GetBlockList();
    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1) - 1.0));

    if (index >= blockList.m_accounts.m_count) {
        luaL_error(L, "Invalid Index");
    }

    uint32_t presenceId = blockList.m_accounts.m_data[index];
    lua_pushnumber(L, presenceId);

    const char* name = BattlenetUI::GetNameForPresenceIdHelper(presenceId);
    lua_pushstring(L, name);
    DebugLog("BNUI: Blocked info. index %d, ID %u, name: %s", index + 1, presenceId, name);

    return 2;
}

// VERIFY: c59a8d80 auto
// OFFSET: 0x0053A660
static int32_t Script_BNRequestFOFInfo(lua_State* L) {
    Login* login = ClientServices::LoginConnection();

    if (!login || login->GetLoginServerType() != 1 || login->IsTrialAccount() || !BattlenetAPI::IsRIDEnabled()) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: BNRequestFOF(ID)");
    }

    double presenceId = lua_tonumber(L, 1);
    uint16_t error = BattlenetAPI::GetFriendsOfFriend(static_cast<uint32_t>(static_cast<int64_t>(presenceId)), BattlenetUI::s_friendsOfFriendCB);
    lua_pushboolean(L, error == 0);
    BattlenetUI::DisplayError(error, static_cast<uint32_t>(static_cast<int64_t>(presenceId)));
    DebugLog("BNUI: Sending Request for FOF for ID %d %s", static_cast<uint32_t>(static_cast<int64_t>(presenceId)), error ? "failed" : "succeeded");

    return 1;
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
