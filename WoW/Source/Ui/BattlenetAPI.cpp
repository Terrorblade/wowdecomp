#include "BattlenetAPI.h"
#include "WoW/Source/Battlenet/Tumor/TumorManager.h"
#include "WoW/Source/WowServices/BattlenetLogin.h"

// VERIFY: 47bdd12c manual
// OFFSET: 0x005312E0
uint16_t BattlenetAPI::SendChatWhisper(uint32_t presenceId, const char* message) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = message;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_SendChatWhisper", 2, args).ToUInt64());
}

// VERIFY: 1f7f4d30 manual
// OFFSET: 0x005313A0
uint16_t BattlenetAPI::LeaveChatConversation(uint8_t channel) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(channel);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_LeaveChatConversation", 1, &arg).ToUInt64());
}

// VERIFY: 0c9531cb manual
// OFFSET: 0x00531430
uint16_t BattlenetAPI::SendChatMessage(uint8_t channel, const char* message) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(channel);
    args[1] = message;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_SendChatMessage", 2, args).ToUInt64());
}

// VERIFY: c2328de4 manual
// OFFSET: 0x005314F0
uint32_t BattlenetAPI::GetNumChatMembers(uint8_t channel) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(channel);
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetNumChatMembers", 1, &arg).ToUInt64());
}

// VERIFY: 67a310d6 manual
// OFFSET: 0x00531580
uint16_t BattlenetAPI::SetFriendNote(uint32_t presenceId, const char* note) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = note;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_SetFriendNote", 2, args).ToUInt64());
}

// VERIFY: c29a1cce manual
// OFFSET: 0x00531640
uint16_t BattlenetAPI::SendRIDFriendInviteByEmail(const char* email, const char* note, GeneralFunction::Callback* callback) {
    GeneralFunction::Arg args[3];
    args[0] = email;
    args[1] = note;
    args[2] = callback;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_SendRIDFriendInviteByEmail", 3, args).ToUInt64());
}

// VERIFY: 245a930e manual
// OFFSET: 0x00531700
uint16_t BattlenetAPI::SendRIDFriendInviteByPresenceId(uint32_t presenceId, const char* note, GeneralFunction::Callback* callback) {
    GeneralFunction::Arg args[3];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = note;
    args[2] = callback;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_SendRIDFriendInviteByPresenceId", 3, args).ToUInt64());
}

// VERIFY: 5e24d7f3 manual
// OFFSET: 0x005317D0
uint16_t BattlenetAPI::RemoveFriend(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_RemoveFriend", 1, &arg).ToUInt64());
}

// VERIFY: ecba5662 manual
// OFFSET: 0x00531860
uint16_t BattlenetAPI::AcceptFriendInvite(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_AcceptFriendInvite", 1, &arg).ToUInt64());
}

// VERIFY: 8cb68fd4 manual
// OFFSET: 0x005318F0
uint16_t BattlenetAPI::DeclineFriendInvite(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_DeclineFriendInvite", 1, &arg).ToUInt64());
}

// VERIFY: a84e6f9a manual
// OFFSET: 0x00531980
uint16_t BattlenetAPI::ReportFriendInvite(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_ReportFriendInvite", 1, &arg).ToUInt64());
}

// VERIFY: 9940bbeb manual
// OFFSET: 0x00531A10
uint16_t BattlenetAPI::GetFriendsOfFriend(uint32_t presenceId, GeneralFunction::Callback* callback) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = callback;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_GetFriendsOfFriend", 2, args).ToUInt64());
}

// VERIFY: 65d4abfb manual
// OFFSET: 0x00531AD0
uint32_t BattlenetAPI::RegisterPresenceUpdateCallback(uint32_t presenceId, uint32_t field, GeneralFunction::Callback* callback) {
    GeneralFunction::Arg args[3];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = static_cast<uint64_t>(field);
    args[2] = callback;
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_RegisterPresenceUpdateCallback", 3, args).ToUInt64());
}

// VERIFY: b965982b manual
// OFFSET: 0x00531BA0
uint32_t BattlenetAPI::GetPresenceIDForCurrentAccount() {
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetPresenceIDForCurrentAccount", 0, nullptr).ToUInt64());
}

// VERIFY: 730cce7c manual
// OFFSET: 0x00531C00
uint32_t BattlenetAPI::GetPresenceIDForCurrentToon() {
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetPresenceIDForCurrentToon", 0, nullptr).ToUInt64());
}

// VERIFY: 210a9c9d manual
// OFFSET: 0x00531C60
uint32_t BattlenetAPI::GetAccountPresenceId(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetAccountPresenceId", 1, &arg).ToUInt64());
}

// VERIFY: 66cae904 manual
// OFFSET: 0x00531CF0
const char* BattlenetAPI::GetNameForPresenceId(uint32_t presenceId, bool full) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = full;
    return g_tumorManager->CallFunction("BattlenetAPI_GetNameForPresenceId", 2, args).ToString();
}

// VERIFY: 0a05bad6 manual
// OFFSET: 0x00531DB0
const char* BattlenetAPI::GetExactNameForPresenceId(uint32_t presenceId, bool full) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = full;
    return g_tumorManager->CallFunction("BattlenetAPI_GetExactNameForPresenceId", 2, args).ToString();
}

// VERIFY: 19def57a manual
// OFFSET: 0x00531E70
const char* BattlenetAPI::GetToonNameForPresenceId(uint32_t presenceId, bool full) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = full;
    return g_tumorManager->CallFunction("BattlenetAPI_GetToonNameForPresenceId", 2, args).ToString();
}

// VERIFY: 76fcefe5 manual
// OFFSET: 0x00531F30
uint32_t BattlenetAPI::GetNumToons(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetNumToons", 1, &arg).ToUInt64());
}

// VERIFY: 460cf366 manual
// OFFSET: 0x00531FC0
uint32_t BattlenetAPI::GetToon(uint32_t presenceId, uint32_t index) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = static_cast<uint64_t>(index);
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetToon", 2, args).ToUInt64());
}

// VERIFY: e101be0a manual
// OFFSET: 0x00532070
uint32_t BattlenetAPI::GetFocusedToon(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetFocusedToon", 1, &arg).ToUInt64());
}

// VERIFY: a79c3fbd manual
// OFFSET: 0x00532100
uint16_t BattlenetAPI::AddRIDBlock(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_AddRIDBlock", 1, &arg).ToUInt64());
}

// VERIFY: 1f3eb23f manual
// OFFSET: 0x00532190
uint16_t BattlenetAPI::AddCIDBlock(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_AddCIDBlock", 1, &arg).ToUInt64());
}

// VERIFY: bf8a1772 manual
// OFFSET: 0x00532220
uint16_t BattlenetAPI::RemoveRIDBlock(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_RemoveRIDBlock", 1, &arg).ToUInt64());
}

// VERIFY: f3c6ff02 manual
// OFFSET: 0x005322B0
uint16_t BattlenetAPI::RemoveCIDBlock(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_RemoveCIDBlock", 1, &arg).ToUInt64());
}

// VERIFY: 60f4858d manual
// OFFSET: 0x00532340
uint16_t BattlenetAPI::ReportPlayer(uint32_t presenceId, int32_t reason, const char* note) {
    GeneralFunction::Arg args[3];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = static_cast<int64_t>(reason);
    args[2] = note;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_ReportPlayer", 3, args).ToUInt64());
}

// VERIFY: 654a4514 manual
// OFFSET: 0x00532400
void BattlenetAPI::SetSetting(const char* name, GeneralFunction::Arg value, bool notify) {
    GeneralFunction::Arg args[3];
    args[0] = name;
    args[1] = value;
    args[2] = notify;
    g_tumorManager->CallFunction("BattlenetAPI_SetSetting", 3, args);
}

// VERIFY: 75a56b4f manual
// OFFSET: 0x00532500
const char* BattlenetAPI::GetErrorString(uint16_t error) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(error);
    return g_tumorManager->CallFunction("BattlenetAPI_GetErrorString", 1, &arg).ToString();
}

// VERIFY: 910a8d74 manual
// OFFSET: 0x005325A0
uint32_t BattlenetAPI::RegisterEvent(const char* name, GeneralFunction::Callback* callback) {
    GeneralFunction::Arg args[2];
    args[0] = name;
    args[1] = callback;
    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_RegisterEvent", 2, args).ToUInt64());
}

// VERIFY: a4abcd2e manual
// OFFSET: 0x00532650
void BattlenetAPI::SetAccountNameFormatString(const char* format) {
    GeneralFunction::Arg arg;
    arg = format;
    g_tumorManager->CallFunction("BattlenetAPI_SetAccountNameFormatString", 1, &arg);
}

// VERIFY: fc61362b manual
// OFFSET: 0x00532E10
uint16_t BattlenetAPI::CreateChatConversation(const Battlenet::Toon::FullName* first, const Battlenet::Toon::FullName* second, GeneralFunction::Callback* callback) {
    GeneralFunction::Arg args[3];

    BSN::BSNEncodedObjectPtr<Battlenet::Toon::FullName, Protocol, 1> firstObject(first);
    firstObject.IncRef();
    args[0] = &firstObject;

    BSN::BSNEncodedObjectPtr<Battlenet::Toon::FullName, Protocol, 1> secondObject(second);
    secondObject.IncRef();
    args[1] = &secondObject;

    args[2] = callback;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_CreateChatConversation", 3, args).ToUInt64());
}

// VERIFY: 24a8a358 manual
// OFFSET: 0x00532FD0
uint16_t BattlenetAPI::SendChatConversationInvite(uint8_t channel, const Battlenet::Toon::FullName* toon, GeneralFunction::Callback* callback) {
    GeneralFunction::Arg args[3];
    args[0] = static_cast<uint64_t>(channel);

    BSN::BSNEncodedObjectPtr<Battlenet::Toon::FullName, Protocol, 1> toonObject(toon);
    toonObject.IncRef();
    args[1] = &toonObject;

    args[2] = callback;
    return static_cast<uint16_t>(g_tumorManager->CallFunction("BattlenetAPI_SendChatConversationInvite", 3, args).ToUInt64());
}

// VERIFY: ea67e7f3 manual
// OFFSET: 0x00533130
int32_t BattlenetAPI::GetChatChannelType(uint8_t channel) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(channel);
    return static_cast<int32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetChatChannelType", 1, &arg).ToInt64());
}

// VERIFY: 99bd6eb2 manual
// OFFSET: 0x005331C0
Battlenet::Chat::MemberList BattlenetAPI::GetChatMemberList(uint8_t channel) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(channel);
    BSN::BSNEncodedObject<Battlenet::Chat::MemberList, Protocol> memberList(g_tumorManager->CallFunction("BattlenetAPI_GetChatMemberList", 1, &arg).ToObject());
    return memberList.m_data;
}

// VERIFY: 738a2741 manual
// OFFSET: 0x00533280
bool BattlenetAPI::IsRIDFriend(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return g_tumorManager->CallFunction("BattlenetAPI_IsRIDFriend", 1, &arg).ToBool();
}

// VERIFY: f22d0ef8 manual
// OFFSET: 0x00533320
bool BattlenetAPI::IsCIDFriend(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return g_tumorManager->CallFunction("BattlenetAPI_IsCIDFriend", 1, &arg).ToBool();
}

// VERIFY: 198d8bb9 manual
// OFFSET: 0x005333C0
bool BattlenetAPI::HasRIDFriendship(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return g_tumorManager->CallFunction("BattlenetAPI_HasRIDFriendship", 1, &arg).ToBool();
}

// VERIFY: 6968d226 manual
// OFFSET: 0x00533460
Battlenet::Friends::FriendInvite BattlenetAPI::GetFriendInvite(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    BSN::BSNEncodedObject<Battlenet::Friends::FriendInvite, Protocol> invite(g_tumorManager->CallFunction("BattlenetAPI_GetFriendInvite", 1, &arg).ToObject());
    return invite.m_data;
}

// VERIFY: 0dd03862 manual
// OFFSET: 0x00533560
bool BattlenetAPI::UnregisterPresenceUpdateCallback(uint32_t handle) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(handle);
    return g_tumorManager->CallFunction("BattlenetAPI_UnregisterPresenceUpdateCallback", 1, &arg).ToBool();
}

// VERIFY: 81ff8cb8 manual
// OFFSET: 0x00533600
bool BattlenetAPI::SetPresenceValue(uint32_t field, const Battlenet::Presence::FieldVal* value) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(field);

    BSN::BSNEncodedObjectPtr<Battlenet::Presence::FieldVal, Protocol, 1> valueObject(value);
    valueObject.IncRef();
    args[1] = &valueObject;

    return g_tumorManager->CallFunction("BattlenetAPI_SetPresenceValue", 2, args).ToBool();
}

// VERIFY: 0b35f414 manual
// OFFSET: 0x00533740
Battlenet::Presence::FieldVal BattlenetAPI::GetPresenceValue(uint32_t presenceId, uint32_t field) {
    GeneralFunction::Arg args[2];
    args[0] = static_cast<uint64_t>(presenceId);
    args[1] = static_cast<uint64_t>(field);
    BSN::BSNEncodedObject<Battlenet::Presence::FieldVal, Protocol> value(g_tumorManager->CallFunction("BattlenetAPI_GetPresenceValue", 2, args).ToObject());
    return value.m_data;
}

// VERIFY: d7d36627 manual
// OFFSET: 0x00533880
uint32_t BattlenetAPI::GetPresenceIDForToon(const Battlenet::Toon::FullName* toon) {
    GeneralFunction::Arg arg;

    BSN::BSNEncodedObjectPtr<Battlenet::Toon::FullName, Protocol, 1> toonObject(toon);
    toonObject.IncRef();
    arg = &toonObject;

    return static_cast<uint32_t>(g_tumorManager->CallFunction("BattlenetAPI_GetPresenceIDForToon", 1, &arg).ToUInt64());
}

// VERIFY: 46fb4745 manual
// OFFSET: 0x00533980
bool BattlenetAPI::IsSelf(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return g_tumorManager->CallFunction("BattlenetAPI_IsSelf", 1, &arg).ToBool();
}

// VERIFY: 768550a9 manual
// OFFSET: 0x00533A20
Battlenet::Presence::BlockList BattlenetAPI::GetBlockList() {
    BSN::BSNEncodedObject<Battlenet::Presence::BlockList, Protocol> blockList(g_tumorManager->CallFunction("BattlenetAPI_GetBlockList", 0, nullptr).ToObject());
    return blockList.m_data;
}

// VERIFY: 33c87192 manual
// OFFSET: 0x00533AA0
bool BattlenetAPI::IsRIDBlock(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return g_tumorManager->CallFunction("BattlenetAPI_IsRIDBlock", 1, &arg).ToBool();
}

// VERIFY: 49652734 manual
// OFFSET: 0x00533B40
bool BattlenetAPI::IsCIDBlock(uint32_t presenceId) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(presenceId);
    return g_tumorManager->CallFunction("BattlenetAPI_IsCIDBlock", 1, &arg).ToBool();
}

// VERIFY: bd641ff4 manual
// OFFSET: 0x00533BE0
GeneralFunction::Arg BattlenetAPI::GetSetting(const char* name) {
    GeneralFunction::Arg arg;
    arg = name;
    return g_tumorManager->CallFunction("BattlenetAPI_GetSetting", 1, &arg);
}

// VERIFY: f2e9a292 manual
// OFFSET: 0x00533C40
bool BattlenetAPI::IsRIDEnabled() {
    return g_tumorManager && g_tumorManager->CallFunction("BattlenetAPI_IsRIDEnabled", 0, nullptr).ToBool();
}

// VERIFY: 2090a458 manual
// OFFSET: 0x00533CA0
bool BattlenetAPI::UnregisterEvent(uint32_t handle) {
    GeneralFunction::Arg arg;
    arg = static_cast<uint64_t>(handle);
    return g_tumorManager->CallFunction("BattlenetAPI_UnregisterEvent", 1, &arg).ToBool();
}
