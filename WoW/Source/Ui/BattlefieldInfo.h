#ifndef WOW_SOURCE_UI_BATTLEFIELDINFO_H
#define WOW_SOURCE_UI_BATTLEFIELDINFO_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Net/Types.h"

class BattlemasterListRec;
class CGPartyMemberState;
class CGPartyPetState;
class CGObject_C;
struct lua_State;

struct BattlefieldScore {
    /* 0000 */ WOWGUID m_guid;
    /* 0008 */ uint32_t m_killingBlows;
    /* 000C */ uint32_t m_honorableKills;
    /* 0010 */ uint32_t m_deaths;
    /* 0014 */ uint32_t m_bonusHonor;
    /* 0018 */ uint32_t m_damageDone;
    /* 001C */ uint32_t m_healingDone;
    /* 0020 */ int32_t m_faction;
    /* 0024 */ int32_t m_stats[9];
};

struct BattlefieldScoreSortCriteria {
    /* 0000 */ int32_t m_column;
    /* 0004 */ int32_t m_descending;
};

struct WorldPvPQueueStatus {
    /* 0000 */ int32_t m_queueID;
    /* 0004 */ int32_t m_areaID;
    /* 0008 */ int32_t m_status;
    /* 000C */ int32_t m_expireTime;
};

struct BattlefieldStatus {
    /* 0000 */ uint64_t m_queueData;
    /* 0008 */ uint64_t m_joinedQueueData;
    /* 0010 */ uint32_t m_bgTypeID;
    /* 0014 */ uint32_t m_mapID;
    /* 0018 */ uint32_t m_status;
    /* 001C */ uint32_t m_minLevel;
    /* 0020 */ uint32_t m_maxLevel;
    /* 0024 */ uint32_t m_instanceID;
    /* 0028 */ uint32_t m_portExpiration;
    /* 002C */ uint32_t m_estimatedWaitTime;
    /* 0030 */ uint32_t m_queueStartTime;
    /* 0034 */ uint32_t m_teamSize;
    /* 0038 */ int32_t m_rated;
    /* 003C */ uint32_t unused_003C;
};

class CDataStore;

int32_t Packet_MSG_BATTLEGROUND_PLAYER_POSITIONS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_LIST(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_STATUS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_MSG_PVP_LOG_DATA(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_GROUP_JOINED_BATTLEGROUND(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_Group_3(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_ARENA_OPPONENT_UPDATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_PORT_DENIED(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_MGR_ENTRY_INVITE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_MGR_ENTERED(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_MGR_QUEUE_REQUEST_RESPONSE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_MGR_EJECT_PENDING(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_MGR_EJECTED(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_MGR_QUEUE_INVITE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_BATTLEFIELD_MGR_STATE_CHANGE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t QSortBattlefields(const void* a, const void* b);
void BattlefieldNameCacheCallback();
void WorldMapNameUpdateCallback(int32_t id, const WOWGUID& guid, void* param, bool success);
int32_t QSortScoreData(const void* a, const void* b);
bool IsOnTransport(CGObject_C* object);
bool IsCurrentMapArena();

class CGBattlefieldInfo {
    public:

    enum QUEUE_STATUS : int32_t {
        QUEUE_NONE = 0,
        QUEUE_QUEUED = 1,
        QUEUE_CONFIRM = 2,
        QUEUE_ACTIVE = 3,
    };

    enum GROUP_JOIN_RESULT : int32_t {
        IN_NON_RANDOM_BG = -15,
        IN_RANDOM_BG = -14,
        LFG = -13,
        UNKNOWN_MINUS_12 = -12,
        UNKNOWN_MINUS_11 = -11,
        RANGE_INDEX = -10,
        XP_GAIN = -9,
        NOT_IN_BATTLEGROUND = -8,
        TEAM_LEFT_QUEUE = -7,
        QUEUED_FOR_RATED = -6,
        CANNOT_QUEUE_FOR_RATED = -5,
        TOO_MANY_QUEUES = -4,
        ARENA_TEAM_PARTY_SIZE = -3,
        DESERTERS = -2,
        UNKNOWN_MINUS_1 = -1,
    };

    enum EJECTION_REASON : int32_t {
        UNKNOWN_1 = 1,
        UNKNOWN_2 = 2,
        UNKNOWN_4 = 4,
        UNKNOWN_5 = 5,
        UNKNOWN_7 = 7,
        PLAYER_EXIT = 8,
        UNKNOWN_9 = 9,
        LOW_LEVEL = 10,
        UNKNOWN_11 = 11,
    };

    enum SCORE_SORT_COLUMN : int32_t {
        COLUMN_KILLING_BLOWS = 0,
        COLUMN_DEATHS = 1,
        COLUMN_BONUS_HONOR = 2,
        COLUMN_NAME = 3,
        COLUMN_CLASS = 4,
        COLUMN_HONORABLE_KILLS = 5,
        COLUMN_DAMAGE_DONE = 6,
        COLUMN_HEALING_DONE = 7,
        COLUMN_TEAM = 8,
        COLUMN_STAT1 = 9,
        COLUMN_STAT2 = 10,
        COLUMN_STAT3 = 11,
        COLUMN_STAT4 = 12,
        COLUMN_STAT5 = 13,
        COLUMN_STAT6 = 14,
        COLUMN_STAT7 = 15,
        COLUMN_STAT8 = 16,
        NUM_COLUMNS = 17,
    };

    static void JoinBattlefield(uint32_t index, int32_t asGroup, int32_t isRated);
    static void RequestBattlegroundInstanceInfo(uint32_t index);
    static void SetBattlemaster(WOWGUID battlemaster);
    static void EnterWorld();
    static void LeaveWorld();
    static void InitializeGame();
    static void ShutdownGame();
    static WOWGUID m_arenaOpponentPets[5][2];
    static WOWGUID m_arenaOpponents[5];
    static CGPartyMemberState m_arenaOpponentStats[5];
    static int32_t m_instanceType;
    static int32_t m_activeIndex;
    static BattlefieldStatus m_status[2];
    static uint32_t m_activeMapID;
    static WOWGUID m_flagCarriers[2];
    static WOWGUID m_playerPositionGuids[40];
    static int32_t m_worldStateStats[8];
    static C2Vector m_playerPositions[40];

    static int32_t GetPlayerPosition(uint32_t index, C3Vector* position);
    static BattlefieldScore* m_scores[80];
    static uint32_t m_minLevel;
    static uint32_t m_maxLevel;
    static WOWGUID m_battlemaster;
    static uint32_t m_numScoreEntries;
    static uint32_t m_nextLogUpdate;
    static int32_t m_nameCachePending;
    static uint32_t m_numPlayerPositions;
    static uint32_t m_positionUpdateTime;
    static C2Vector m_flagPositions[2];
    static int32_t m_logFilterFaction;
    static int32_t m_allowJoin;
    static int32_t m_selectedInstance;
    static int32_t m_bgTypeID;
    static uint8_t m_hasHolidayWinToday;
    static uint8_t m_hasRandomWinToday;
    static int32_t m_holidayLossHonor;
    static int32_t m_holidayWinHonor;
    static int32_t m_holidayWinArena;
    static int32_t m_randomLossHonor;
    static int32_t m_randomWinHonor;
    static int32_t m_randomWinArena;
    static TSGrowableArray<int32_t> m_instances;
    static uint8_t m_arenaFaction;
    static uint32_t m_numScores;
    static uint32_t m_numWorldStateStats;
    static uint32_t m_numVehicles;
    static WOWGUID m_vehicleGuids[40];
    static uint32_t m_instanceStartTime;
    static uint32_t m_instanceExpiration;
    static char m_teamNames[2][96];
    static uint32_t m_teamRatings[2];
    static uint32_t m_newTeamRatings[2];
    static uint32_t m_avgMatchmakingRating[2];
    static int32_t m_mapDone;
    static uint32_t m_winner;
    static int32_t m_arenaOpponentSeen[5];
    static WorldPvPQueueStatus m_worldPvPQueues[1];
    static BattlefieldScoreSortCriteria m_scoreSortCriteria[NUM_COLUMNS];
    static TSGrowableArray<int32_t> m_battlegroundTypes;

    static void AddArenaOpponent(const WOWGUID& guid);
    static void OnArenaUnitOutOfRange(const WOWGUID& guid);
    static void OnArenaUnitDestroyed(const WOWGUID& guid);
    static int32_t GetBattlegroundType(int32_t index);
    static bool GetBracketForLevel(uint32_t level, const BattlemasterListRec* rec, int32_t* bracket);
    static void AddVehicle(const WOWGUID& guid);
    static WOWGUID GetVehicleGuid(uint32_t index);
    static uint32_t GetNumFlagCarriers();
    static WOWGUID GetFlagCarrier(uint32_t index);
    static int32_t GetFlagPosition(uint32_t index, C3Vector* position);
    static void RequestScoreData();
    static void SetScoreSortColumn(int32_t column);
    static const char* GetFlagToken(uint32_t index);
    static void BattlefieldMgrEntryInviteResponse(int32_t battleId, uint8_t accept);
    static void BattlefieldMgrQueueRequest(int32_t battleId);
    static void BattlefieldMgrQueueInviteResponse(int32_t battleId, uint8_t accept);
    static void BattlefieldMgrExitRequest(int32_t battleId);
    static void AddArenaOpponentPet(const WOWGUID& petGuid, const WOWGUID& ownerGuid);
    static void AddArenaOpponentToSlot(const WOWGUID& guid, int32_t slot);
    static void AddArenaOpponentPetToSlot(const WOWGUID& petGuid, const WOWGUID& ownerGuid, int32_t slot, int32_t registerNew);
    static WOWGUID GetArenaOpponent(uint32_t index);
    static CGPartyMemberState* GetRemoteStats(const WOWGUID& guid);
    static CGPartyMemberState* GetRemoteStatsFromPet(const WOWGUID& petGuid);
    static CGPartyPetState* GetRemotePetStats(const WOWGUID& guid);
    static WOWGUID GetArenaOpponentPet(uint32_t index);
    static int32_t GetArenaOpponentIndex(const WOWGUID& guid, int32_t* index);
    static void RemoveVehicle(const WOWGUID& guid);
    static void RequestPlayerPositions();
};

uint32_t GetArenaPetGUIDCount();

int32_t Script_CanHearthAndResurrectFromArea(lua_State* L);
int32_t Script_RequestBattlegroundInstanceInfo(lua_State* L);
void BattlefieldInfoRegisterScriptFunctions();
void BattlefieldInfoUnregisterScriptFunctions();

#endif
