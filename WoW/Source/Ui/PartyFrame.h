#ifndef WOW_SOURCE_UI_PARTYFRAME_H
#define WOW_SOURCE_UI_PARTYFRAME_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/ObjectClient/Types.h"

class CDataStore;
class CGPlayer_C;
struct lua_State;

enum PARTY_COMMAND_RESULT : uint32_t {
    PARTY_RESULT_SUCCESS                                 = 0,
    PARTY_RESULT_BAD_PLAYER_NAME                         = 1,
    PARTY_RESULT_TARGET_NOT_IN_GROUP                     = 2,
    PARTY_RESULT_TARGET_NOT_IN_INSTANCE                  = 3,
    PARTY_RESULT_GROUP_FULL                              = 4,
    PARTY_RESULT_ALREADY_IN_GROUP                        = 5,
    PARTY_RESULT_NOT_IN_GROUP                            = 6,
    PARTY_RESULT_NOT_LEADER                              = 7,
    PARTY_RESULT_PLAYER_WRONG_FACTION                    = 8,
    PARTY_RESULT_IGNORING_YOU                            = 9,
    PARTY_RESULT_LFG_PENDING                             = 12,
    PARTY_RESULT_INVITE_RESTRICTED                       = 13,
    PARTY_RESULT_SWAP_FAILED_OR_INVITE_IN_COMBAT         = 14,
    PARTY_RESULT_INVITE_UNKNOWN_REALM                    = 15,
    PARTY_RESULT_INVITE_NO_PARTY_SERVER                  = 16,
    PARTY_RESULT_INVITE_PARTY_BUSY                       = 17,
    PARTY_RESULT_TARGET_AMBIGUOUS                        = 18,
    PARTY_RESULT_LFG_INVITE_RAID_LOCKED                  = 19,
    PARTY_RESULT_LFG_BOOT_LIMIT                          = 20,
    PARTY_RESULT_LFG_BOOT_COOLDOWN                       = 21,
    PARTY_RESULT_LFG_BOOT_IN_PROGRESS                    = 22,
    PARTY_RESULT_LFG_BOOT_TOO_FEW_PLAYERS                = 23,
    PARTY_RESULT_LFG_BOOT_NOT_ELIGIBLE                   = 24,
    PARTY_RESULT_RAID_DISALLOWED_BY_LEVEL                = 25,
    PARTY_RESULT_LFG_BOOT_IN_COMBAT                      = 26,
    PARTY_RESULT_VOTE_KICK_REASON_NEEDED                 = 27,
    PARTY_RESULT_LFG_BOOT_DUNGEON_COMPLETE               = 28,
    PARTY_RESULT_LFG_BOOT_LOOT_ROLLS                     = 29,
    PARTY_RESULT_LFG_TELEPORT_IN_COMBAT                  = 30,
};

enum GROUP_TYPE_FLAGS : uint8_t {
    GROUP_TYPE_FLAG_NONE           = 0x00,
    GROUP_TYPE_FLAG_BATTLEGROUND   = 0x01,
    GROUP_TYPE_FLAG_RAID           = 0x02,
    GROUP_TYPE_FLAG_LFG_RESTRICTED = 0x04,
    GROUP_TYPE_FLAG_LFG            = 0x08,
};
constexpr int32_t MAX_PARTY_MEMBER_AURAS = 64;

class CGPartyPetState {
    public:
    /* 0000 */ WOWGUID m_guid;
    /* 0008 */ char m_name[80];
    /* 0058 */ uint8_t m_powerType;
    /* 0059 */ uint8_t unused_0059;
    /* 005A */ uint16_t m_displayId;
    /* 005C */ uint32_t m_health;
    /* 0060 */ uint32_t m_maxHealth;
    /* 0064 */ uint16_t m_power;
    /* 0066 */ uint16_t m_maxPower;
    /* 0068 */ int32_t m_auras[MAX_PARTY_MEMBER_AURAS];
    /* 0168 */ uint8_t m_auraFlags[MAX_PARTY_MEMBER_AURAS];
};

class CGPartyMemberState {
    public:
    enum MemberFlags : uint16_t {
        MEMBER_FLAG_CONNECTED = 0x1,
        MEMBER_FLAG_PVP = 0x2,
        MEMBER_FLAG_DEAD = 0x4,
        MEMBER_FLAG_GHOST = 0x8,
        MEMBER_FLAG_PVP_FFA = 0x10,
        MEMBER_FLAG_UNKNOWN_5 = 0x20,
        MEMBER_FLAG_AFK = 0x40,
        MEMBER_FLAG_DND = 0x80,
        MEMBER_FLAG_REFER_A_FRIEND = 0x100,
        MEMBER_FLAG_IN_VEHICLE = 0x200,
        MEMBER_FLAG_CONNECTED_GHOST = 9,
        MEMBER_FLAG_DEAD_OR_GHOST = 0xC,
        MEMBER_FLAG_PVP_MASK = 0x12,
    };

    enum UpdateFlags : uint32_t {
        GROUP_UPDATE_FLAG_STATUS = 0x1,
        GROUP_UPDATE_FLAG_CUR_HP = 0x2,
        GROUP_UPDATE_FLAG_MAX_HP = 0x4,
        GROUP_UPDATE_FLAG_POWER_TYPE = 0x8,
        GROUP_UPDATE_FLAG_CUR_POWER = 0x10,
        GROUP_UPDATE_FLAG_MAX_POWER = 0x20,
        GROUP_UPDATE_FLAG_LEVEL = 0x40,
        GROUP_UPDATE_FLAG_ZONE = 0x80,
        GROUP_UPDATE_FLAG_POSITION = 0x100,
        GROUP_UPDATE_FLAG_AURAS = 0x200,
        GROUP_UPDATE_FLAG_PET_GUID = 0x400,
        GROUP_UPDATE_FLAG_PET_NAME = 0x800,
        GROUP_UPDATE_FLAG_PET_MODEL_ID = 0x1000,
        GROUP_UPDATE_FLAG_PET_CUR_HP = 0x2000,
        GROUP_UPDATE_FLAG_PET_MAX_HP = 0x4000,
        GROUP_UPDATE_FLAG_PET_POWER_TYPE = 0x8000,
        GROUP_UPDATE_FLAG_PET_CUR_POWER = 0x10000,
        GROUP_UPDATE_FLAG_PET_MAX_POWER = 0x20000,
        GROUP_UPDATE_FLAG_PET_AURAS = 0x40000,
        GROUP_UPDATE_FLAG_VEHICLE_SEAT = 0x80000,
        GROUP_UPDATE_FLAG_UNKNOWN_31 = 0x80000000,
        GROUP_UPDATE_FLAG_ALL = 0xFFFFFFFF,
    };

    /* 0000 */ WOWGUID m_guid;
    /* 0008 */ uint16_t m_flags;
    /* 000A */ uint8_t m_powerType;
    /* 000B */ uint8_t unused_000B;
    /* 000C */ uint32_t m_health;
    /* 0010 */ uint32_t m_maxHealth;
    /* 0014 */ uint16_t m_power;
    /* 0016 */ uint16_t m_maxPower;
    /* 0018 */ uint16_t m_level;
    /* 001A */ uint16_t m_zoneID;
    /* 001C */ int16_t m_posX;
    /* 001E */ int16_t m_posY;
    /* 0020 */ int32_t m_auras[MAX_PARTY_MEMBER_AURAS];
    /* 0120 */ uint8_t m_auraFlags[MAX_PARTY_MEMBER_AURAS];
    /* 0160 */ CGPartyPetState m_pet;
    /* 0308 */ uint32_t m_vehicleSeatId;
    /* 030C */ uint32_t unused_030C;
};

struct PartyReadyCheckInfo {
    /* 0000 */ WOWGUID m_guid;
    /* 0008 */ int32_t m_pending;
    /* 000C */ int32_t m_ready;
};

class CGPartyInfo {
    public:
    enum DIFFICULTY_CHANGE_RESULT : int32_t {
        DIFFICULTY_RESULT_SET_DIFFICULTY = 0,
        DIFFICULTY_RESULT_COOLDOWN = 1,
        DIFFICULTY_RESULT_WORLDSTATE = 2,
        DIFFICULTY_RESULT_ENCOUNTER = 3,
        DIFFICULTY_RESULT_COMBAT = 4,
        DIFFICULTY_RESULT_PLAYER_BUSY = 5,
        DIFFICULTY_RESULT_START_LOADING = 6,
        DIFFICULTY_RESULT_ALREADY_STARTED = 7,
        DIFFICULTY_RESULT_MAP_MESSAGE = 8,
        DIFFICULTY_RESULT_COMPLETE = 9,
    };

    enum ASSIGNMENT : uint8_t {
        MAINTANK = 0,
        MAINASSIST = 1,
    };

    enum ASSIGNMENT_FLAGS : int32_t {
        ASSISTANT = 0x1,
        MAIN_TANK = 2,
        MAIN_ASSIST = 4,
    };

    enum LFG_FLAGS : uint8_t {
        LFG_FLAG_NONE = 0x0,
        LFG_FLAG_DUNGEON_COMPLETE = 0x2,
    };

    static void ChangeRaidDifficulty(int32_t heroic);
    static void LeaveWorld();
    static uint32_t m_memberRoles[4];
    static uint8_t m_memberAssignments[4];
    static uint32_t m_nextDifficultyChange;
    static uint8_t m_playerAssignments;
    static uint8_t m_playerLFGFlags;
    static uint8_t m_playerRole;
    static void InitializeGame();
    static void ShutdownGame();
    static void CheckClearLFGSlot();
    static int32_t IsMemberOrPet(const WOWGUID& guid);
    static void RegisterScriptFunctions();
    static void UnregisterScriptFunctions();
    static WOWGUID m_members[4];
    static uint32_t m_dungeonDifficulty;
    static uint32_t m_raidDifficulty;
    static int32_t m_dynamicDifficulty;
    static int32_t m_lfgRestricted;
    static int32_t m_leaderIndex;
    static int32_t m_lootThreshold;
    static uint32_t m_realNumMembers;
    static int32_t m_optOutOfLoot;
    static int32_t m_lfgSlot;
    static WOWGUID m_leader;
    static WOWGUID m_realLeader;
    static LOOT_METHOD m_lootMethod;
    static WOWGUID m_lootMaster;

    static bool InParty();

    static uint32_t GetNumPartyMembers();
    static uint32_t GetGroupRole(const WOWGUID& guid);
    static int32_t IsMember(const WOWGUID& guid);
    static uint8_t GetPartyAssignments(const WOWGUID& guid);
    static int32_t GetPartyAssignments(lua_State* L, int32_t assignment);
    static uint32_t m_lastAssignmentTime;
    static void RemoveAll(int32_t a1);
    static void AddMember(WOWGUID guid, int32_t online, const CGPartyMemberState* state, uint8_t assignments, uint32_t role, const PartyReadyCheckInfo* readyCheck);
    static void GetReadyCheckInfo(PartyReadyCheckInfo* info, uint32_t index);
    static void SetReadyCheckInfo(const PartyReadyCheckInfo* info, uint32_t index);
    static void ResetLootMethod();
    static void SetLeader(WOWGUID leader);
    static void SetLfgRestricted(int32_t restricted);
    static void SetLfgSlot(int32_t slot);
    static int32_t SetLootMethod(LOOT_METHOD method, WOWGUID master, int32_t threshold, int32_t force);
    static void UpdateRealStats(uint32_t count, WOWGUID leader);
    static void SetOptOutOfLoot(int32_t optOut);
    static void EnableMember(CGPlayer_C* player, int32_t enable);
    static int32_t IsMemberPet(const WOWGUID& guid, int32_t* index);
    static CGPartyMemberState* GetRemoteStats(const WOWGUID& guid);
    static void ClearRecruitAFriend(WOWGUID guid);
    static CGPartyMemberState* GetRemoteStatsFromPet(const WOWGUID& petGuid);
    static CGPartyMemberState* GetMemberState(uint32_t index);
    static int32_t IsPlayerOrPetInParty(const WOWGUID& guid);
    static CGPartyPetState* GetRemotePetStats(const WOWGUID& guid);
};

class CGPartyMemberStateRepository {
    public:
    static CGPartyPetState* FindPetState(const WOWGUID& guid);
    static CGPartyMemberState* GetState(const WOWGUID& guid);
};

extern CGPartyMemberState s_partyMemberStates[4];

WOWGUID GetPartyMemberGUID(uint32_t index);
WOWGUID GetPartyPetGUID(uint32_t index);


int32_t IsGuidInMyRaid(const WOWGUID& guid);
int32_t CanSendPartyCommand();

int32_t Packet_SMSG_INSTANCE_SAVE_CREATED(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_PLAYER_DIFFICULTY_CHANGE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t CCommand_SetDifficulty(const char* command, const char* arguments);

#endif
