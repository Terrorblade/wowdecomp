#ifndef WOW_SOURCE_UI_ARENATEAMINFO_H
#define WOW_SOURCE_UI_ARENATEAMINFO_H

#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/Player.h"

struct lua_State;
class CDataStore;

enum ARENA_TEAM_COMMAND_TYPE : uint32_t {
    ARENA_TEAM_COMMAND_CREATE  = 0,
    ARENA_TEAM_COMMAND_INVITE  = 1,
    ARENA_TEAM_COMMAND_QUIT    = 3,
    ARENA_TEAM_COMMAND_FOUNDER = 14,
};

enum ARENA_TEAM_EVENT : uint8_t {
    ARENA_TEAM_EVENT_UNKNOWN_0      = 0,
    ARENA_TEAM_EVENT_UNKNOWN_1      = 1,
    ARENA_TEAM_EVENT_UNKNOWN_2      = 2,
    ARENA_TEAM_EVENT_JOINED         = 3,
    ARENA_TEAM_EVENT_LEFT           = 4,
    ARENA_TEAM_EVENT_REMOVED        = 5,
    ARENA_TEAM_EVENT_LEADER_IS      = 6,
    ARENA_TEAM_EVENT_LEADER_CHANGED = 7,
    ARENA_TEAM_EVENT_DISBANDED      = 8,
};

enum ARENA_TEAM_COMMAND_RESULT : uint32_t {
    ARENA_RESULT_SUCCESS                                     = 0,
    ARENA_RESULT_INTERNAL                                    = 1,
    ARENA_RESULT_ALREADY_IN_TEAM                             = 2,
    ARENA_RESULT_PLAYER_ALREADY_IN_TEAM                      = 3,
    ARENA_RESULT_ALREADY_INVITED                             = 4,
    ARENA_RESULT_PLAYER_ALREADY_INVITED                      = 5,
    ARENA_RESULT_NAME_INVALID                                = 6,
    ARENA_RESULT_NAME_EXISTS                                 = 7,
    ARENA_RESULT_LEADER_CANNOT_LEAVE_OR_NO_PERMISSION        = 8,
    ARENA_RESULT_NOT_IN_TEAM                                 = 9,
    ARENA_RESULT_PLAYER_NOT_IN_TEAM                          = 10,
    ARENA_RESULT_PLAYER_NOT_FOUND                            = 11,
    ARENA_RESULT_NOT_ALLIED                                  = 12,
    ARENA_RESULT_IGNORING_YOU                                = 19,
    ARENA_RESULT_TARGET_TOO_LOW                              = 21,
    ARENA_RESULT_TARGET_TOO_HIGH                             = 22,
    ARENA_RESULT_TOO_MANY_MEMBERS                            = 23,
    ARENA_RESULT_TEAM_NOT_FOUND                              = 27,
    ARENA_RESULT_TEAMS_LOCKED                                = 30,
};

enum ARENA_TEAM_SIZE : int32_t {
    TWO_VS_TWO = 2,
    THREE_VS_THREE = 3,
    FIVE_VS_FIVE = 5,
};

struct ArenaTeamMember {
    /* 0000 */ WOWGUID m_guid;
    /* 0008 */ char m_name[48];
    /* 0038 */ uint32_t m_level;
    /* 003C */ uint32_t m_class;
    /* 0040 */ uint32_t m_rank;
    /* 0044 */ int32_t m_online;
    /* 0048 */ uint32_t m_weekGames;
    /* 004C */ uint32_t m_weekWins;
    /* 0050 */ uint32_t m_seasonGames;
    /* 0054 */ uint32_t m_seasonWins;
    /* 0058 */ uint32_t m_personalRating;
    /* 005C */ float m_matchmakerRatingBefore;
    /* 0060 */ float m_matchmakerRatingAfter;
    /* 0064 */ uint32_t unused_0064;
};

struct ArenaTeam {
    /* 0000 */ uint32_t m_teamId;
    /* 0004 */ TSGrowableArray<ArenaTeamMember*> m_members;
    /* 0014 */ uint32_t m_type;
    /* 0018 */ uint32_t m_numMembers;
    /* 001C */ uint32_t m_numOnline;
    /* 0020 */ uint32_t m_rating;
    /* 0024 */ uint32_t m_weekGames;
    /* 0028 */ uint32_t m_weekWins;
    /* 002C */ uint32_t m_seasonGames;
    /* 0030 */ uint32_t m_seasonWins;
    /* 0034 */ uint32_t m_rank;

    void FreeMemberArray();
};

struct ArenaTeamSort {
    /* 0000 */ int32_t m_column;
    /* 0004 */ int32_t m_reverse;
};

class CGArenaTeamInfo {
    public:

    enum SORT_COLUMN : int32_t {
        COLUMN_NAME = 0,
        COLUMN_CLASS = 1,
        COLUMN_WEEK_GAMES = 2,
        COLUMN_WEEK_WINS = 3,
        COLUMN_SEASON_GAMES = 4,
        COLUMN_SEASON_WINS = 5,
        COLUMN_PERSONAL_RATING = 6,
        NUM_COLUMNS = 7,
    };

    static ArenaTeam m_arenaTeams[MAX_ARENA_TEAMS];
    static int32_t m_showOffline;
    static uint32_t m_nextAllowedQuery[MAX_ARENA_TEAMS];
    static ArenaTeamSort m_sortOrder[CGArenaTeamInfo::NUM_COLUMNS];
    static WOWGUID m_selectedMember;
    static void EnterWorld();
    static void InitializeGame();
    static void ShutdownGame();
    static void QueryRoster(uint32_t index);
    static void UpdateRoster(uint32_t index, int32_t reset);
    static void UpdateTeams(int32_t signal);
    static bool CheckPersonalAndTeamRating(uint32_t rating, int32_t bracket);
    static ArenaTeamMember* SelectMember(uint32_t team, uint32_t index);
    static void SetRosterShowOffline(int32_t showOffline);
    static void SortRoster(int32_t column);
    static int32_t GetSelectedMemberIndex(uint32_t team);
    static ArenaTeam* GetArenaTeam(uint32_t index);
};

void ArenaTeamInfoRegisterScriptFunctions();
void ArenaTeamInfoUnregisterScriptFunctions();
int32_t Packet_SMSG_ARENA_TEAM_STATS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_ARENA_TEAM_ROSTER(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);

#endif
