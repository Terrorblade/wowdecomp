#ifndef WOW_SOURCE_UI_GUILDINFO_H
#define WOW_SOURCE_UI_GUILDINFO_H

#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Ui/GuildBankFrame.h"

class CDataStore;
struct lua_State;

enum GUILD_COMMAND_TYPE : uint32_t {
    GUILD_COMMAND_CREATE     = 0,
    GUILD_COMMAND_INVITE     = 1,
    GUILD_COMMAND_QUIT       = 3,
    GUILD_COMMAND_UNKNOWN_5  = 5,
    GUILD_COMMAND_FOUNDER    = 14,
    GUILD_COMMAND_UNKNOWN_19 = 19,
    GUILD_COMMAND_UNKNOWN_20 = 20,
};

enum GUILD_COMMAND_RESULT : uint32_t {
    GUILD_RESULT_SUCCESS                                    = 0,
    GUILD_RESULT_INTERNAL                                   = 1,
    GUILD_RESULT_ALREADY_IN_GUILD                           = 2,
    GUILD_RESULT_PLAYER_ALREADY_IN_GUILD                    = 3,
    GUILD_RESULT_ALREADY_INVITED                            = 4,
    GUILD_RESULT_PLAYER_ALREADY_INVITED                     = 5,
    GUILD_RESULT_NAME_INVALID                               = 6,
    GUILD_RESULT_NAME_EXISTS                                = 7,
    GUILD_RESULT_LEADER_CANNOT_LEAVE_OR_NO_PERMISSION       = 8,
    GUILD_RESULT_NOT_IN_GUILD                               = 9,
    GUILD_RESULT_PLAYER_NOT_IN_GUILD                        = 10,
    GUILD_RESULT_PLAYER_NOT_FOUND                           = 11,
    GUILD_RESULT_NOT_ALLIED                                 = 12,
    GUILD_RESULT_RANK_TOO_HIGH                              = 13,
    GUILD_RESULT_RANK_TOO_LOW                               = 14,
    GUILD_RESULT_RANKS_LOCKED                               = 17,
    GUILD_RESULT_RANK_IN_USE                                = 18,
    GUILD_RESULT_IGNORING_YOU                               = 19,
    GUILD_RESULT_REFRESH_ROSTER                             = 20,
    GUILD_RESULT_WITHDRAW_LIMIT                             = 25,
    GUILD_RESULT_NOT_ENOUGH_MONEY                           = 26,
    GUILD_RESULT_BANK_FULL                                  = 28,
    GUILD_RESULT_ITEM_NOT_FOUND                             = 29,
};

enum GUILD_RANK_RIGHTS : uint32_t {
    GUILD_RIGHT_GUILDCHAT_LISTEN = 0x1,
    GUILD_RIGHT_GUILDCHAT_SPEAK = 0x2,
    GUILD_RIGHT_OFFICERCHAT_LISTEN = 0x4,
    GUILD_RIGHT_OFFICERCHAT_SPEAK = 0x8,
    GUILD_RIGHT_INVITE = 0x10,
    GUILD_RIGHT_REMOVE = 0x20,
    GUILD_RIGHT_PROMOTE = 0x80,
    GUILD_RIGHT_DEMOTE = 0x100,
    GUILD_RIGHT_EDIT_MOTD = 0x1000,
    GUILD_RIGHT_EDIT_PUBLIC_NOTE = 0x2000,
    GUILD_RIGHT_VIEW_OFFICER_NOTE = 0x4000,
    GUILD_RIGHT_EDIT_OFFICER_NOTE = 0x8000,
    GUILD_RIGHT_EDIT_GUILD_INFO = 0x10000,
    GUILD_RIGHT_UNKNOWN_17 = 0x20000,
    GUILD_RIGHT_BANK_REPAIR = 0x40000,
    GUILD_RIGHT_WITHDRAW_BANK_MONEY = 0x80000,
    GUILD_RIGHT_EDIT_GUILD_EVENT = 0x100000,
};

enum GUILDBANK_TAB_RIGHTS : uint32_t {
    GUILDBANK_TAB_RIGHT_VIEW = 0x1,
    GUILDBANK_TAB_RIGHT_DEPOSIT = 0x2,
    GUILDBANK_TAB_RIGHT_UPDATE_TEXT = 0x4,
};
constexpr uint32_t NUM_GUILD_RANK_FLAGS = 17;
constexpr uint32_t MAX_GUILD_RANKS = 10;

enum GUILD_MEMBER_STATUS : uint8_t {
    GUILD_MEMBER_STATUS_ONLINE = 0x1,
    GUILD_MEMBER_STATUS_AFK = 0x2,
    GUILD_MEMBER_STATUS_DND = 0x4,
};

enum GUILD_ROSTER_SORT_COLUMN : int32_t {
    GUILD_ROSTER_SORT_RANK = 0,
    GUILD_ROSTER_SORT_LEVEL = 1,
    GUILD_ROSTER_SORT_NAME = 2,
    GUILD_ROSTER_SORT_ZONE = 3,
    GUILD_ROSTER_SORT_CLASS = 4,
    GUILD_ROSTER_SORT_GROUP = 5,
    GUILD_ROSTER_SORT_ONLINE = 6,
    GUILD_ROSTER_SORT_NOTE = 7,
    NUM_GUILD_ROSTER_SORT_COLUMNS = 8,
};

struct GuildRankInfo {
    /* 0000 */ uint32_t m_rights;
    /* 0004 */ int32_t m_bankMoneyPerDay;
    /* 0008 */ uint32_t m_bankTabRights[6];
    /* 0020 */ int32_t m_bankTabStacksPerDay[6];
};

struct GuildMemberInfo {
    /* 0000 */ WOWGUID m_guid;
    /* 0008 */ char m_name[48];
    /* 0038 */ int32_t m_level;
    /* 003C */ int32_t m_class;
    /* 0040 */ int32_t m_gender;
    /* 0044 */ int32_t m_zoneID;
    /* 0048 */ int32_t m_rank;
    /* 004C */ int32_t m_online;
    /* 0050 */ uint8_t m_status;
    /* 0051 */ uint8_t unk_0051;
    /* 0052 */ char m_note[128];
    /* 00D2 */ char m_officerNote[128];
    /* 0152 */ uint8_t unused_0152[2];
    /* 0154 */ float m_lastOnline;
};

struct GuildRosterSortCriteria {
    /* 0000 */ int32_t m_column;
    /* 0004 */ int32_t m_descending;
};

struct GuildEventLogEntry {

    enum EVENT_TYPE : uint32_t {
        EVENT_INVITE = 1,
        EVENT_JOIN = 2,
        EVENT_PROMOTE = 3,
        EVENT_DEMOTE = 4,
        EVENT_REMOVE = 5,
        EVENT_QUIT = 6,
    };

    /* 0000 */ uint32_t m_type;
    /* 0004 */ uint32_t unused_0004;
    /* 0008 */ WOWGUID m_player1;
    /* 0010 */ WOWGUID m_player2;
    /* 0018 */ uint32_t m_rank;
    /* 001C */ uint32_t m_time;
};

struct GuildRankFlag {
    /* 0000 */ uint32_t m_mask;
    /* 0004 */ int32_t m_enabled;
};

class CGGuildInfo {
    public:
    static uint32_t GetGuildBankTabPermissionsByRankID(uint8_t tab, uint32_t rank);
    static int32_t GetNumGuildBankTabItemWithdrawalsByRankID(uint8_t tab, uint32_t rank);
    static void SetMOTD(const char* motd);
    static void SaveToFile();
    static void QueryRosterAndRanks();
    static void InvalidatePlayer(const WOWGUID& guid);
    static char m_infoText[2004];
    static char m_motd[516];
    static GuildRosterSortCriteria m_sortCriteria[NUM_GUILD_ROSTER_SORT_COLUMNS];
    static uint32_t m_numMembers;
    static uint32_t m_numOnlineMembers;
    static int32_t m_showOffline;
    static WOWGUID m_selectedMember;
    static int32_t m_nextAllowedQuery;
    static int32_t m_pendingEventLogNames;
    static uint8_t m_saveToFile;
    static TSGrowableArray<GuildMemberInfo*> m_roster;
    static GuildRankInfo m_rankInfo[MAX_GUILD_RANKS];
    static int32_t m_numEventLogEntries;
    static uint32_t m_numRanks;
    static GuildEventLogEntry m_eventLog[100];
    static void SortRoster(int32_t column);
    static void EnterWorld();
    static void InitializeGame();
    static void ShutdownGame();
    static void QueryPermissions();
    static void SetShowOffline(int32_t show);
    static int32_t GetSelectedMemberIndex();
    static bool CanEditGuildEvent();
    static GuildRankInfo& GetRankInfo(uint32_t rank);
};

extern int32_t s_guildControlRank;

int32_t Packet_SMSG_GUILD_ROSTER(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_MSG_GUILD_PERMISSIONS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_MSG_GUILD_EVENT_LOG_QUERY(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t QSortRoster(const void* a, const void* b);
void GuildInfoCacheCallback(int32_t id, const WOWGUID& guid, void* param, bool success);
void UpdateGuildRoster(int32_t a1);

int32_t Script_GuildRoster(lua_State* L);
void GuildInfoRegisterScriptFunctions();
void GuildInfoUnregisterScriptFunctions();

#endif
