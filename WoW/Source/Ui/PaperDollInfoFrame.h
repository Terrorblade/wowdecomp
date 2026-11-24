#ifndef WOW_SOURCE_UI_PAPERDOLLINFOFRAME_H
#define WOW_SOURCE_UI_PAPERDOLLINFOFRAME_H

#include <cstdint>
#include "WoW/Source/Net/Types.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Object/Player.h"

struct lua_State;

class CGPlayer_C;
class CGItem_C;
class ItemStats_C;

struct InspectArenaTeam {
    /* 0000 */ uint32_t m_teamId;
    /* 0004 */ uint32_t m_rating;
    /* 0008 */ uint32_t m_weekGames;
    /* 000C */ uint32_t m_weekWins;
    /* 0010 */ uint32_t m_seasonGames;
    /* 0014 */ uint32_t m_personalRating;
};

class CGCharacterInfo {
    public:
    static TSFixedArray<int32_t> m_invAlertStatus;
    static int32_t m_hasHonorData;
    static int32_t m_honorRequestPending;
    static uint32_t m_inspectLifetimeHonorableKills;
    static uint32_t m_inspectYesterdayHonor;
    static uint32_t m_inspectTodayHonor;
    static uint16_t m_inspectYesterdayHonorableKills;
    static uint8_t m_inspectLifetimeHighestRank;
    static uint16_t m_inspectTodayHonorableKills;
    static InspectArenaTeam m_inspectArenaTeams[MAX_ARENA_TEAMS];
    static int32_t CalculateInventoryAlertStatus(CGPlayer_C* player, uint32_t slot);
    static void RegisterMirrorHandlers(WOWGUID guid);
    static WOWGUID m_inspectTarget;
    static int32_t CanInspect(const CGPlayer_C* unit);
    static int32_t GetActivePlayerAmmoID(const CGPlayer_C* player);
    static void PickupInventoryItem(int32_t slot);
    static int32_t PutItemInBackpack(uint32_t slot);
    static void EnterWorld();
    static void LeaveWorld();
    static void SetInspectTarget(const WOWGUID& guid);
    static void InitializeGame();
    static void ShutdownGame();
    static void UpdateInventoryAlerts();
    static void UpdateItem(WOWGUID guid);
    static void UseItem(int32_t slot, WOWGUID* target);
    static void PickupBag(uint32_t slot);
    static void SocketInventoryItem(int32_t slot);
    static void RequestInspectHonorData();
    static bool CanEquipItemInSlot(const ItemStats_C* stats, uint32_t slot);
    static bool CanEquipItem(CGItem_C* item, uint32_t slot);
};

void GuildNameCallback(int32_t id, const WOWGUID& guid, void* param, bool success);
void CharacterInfoRegisterScriptFunctions();
void CharacterInfoUnregisterScriptFunctions();

class CDataStore;

int32_t Packet_MSG_INSPECT_HONOR_STATS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_MSG_INSPECT_ARENA_TEAMS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);

int32_t PlayerCharacterPointsUpdateHandler(WOWGUID guid, uint32_t fieldOffset, uint32_t fieldSize, const void* oldValue, void* param);
int32_t PlayerEquipmentChangedHandler(WOWGUID guid, uint32_t fieldOffset, uint32_t fieldSize, const void* oldValue, void* param);

int32_t ResolveInventorySlotArg(lua_State* L, int32_t index, int32_t& slot);
int32_t GetUnitAndSlotFromLua(lua_State* L, const char*& unit, int32_t& slot);
void UnitInventoryChangedCallback(int32_t id, const WOWGUID& guid, void* param, bool success);

#endif
