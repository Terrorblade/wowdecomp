#ifndef WOW_SOURCE_UI_CGAUTOCOMPLETENAME_H
#define WOW_SOURCE_UI_CGAUTOCOMPLETENAME_H

#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Object/WGUID.h"

struct lua_State;

enum AUTOCOMPLETE_FLAGS : uint32_t {
    AUTOCOMPLETE_FLAG_NONE = 0x00000000,
    AUTOCOMPLETE_FLAG_IN_GROUP = 0x00000001,
    AUTOCOMPLETE_FLAG_IN_GUILD = 0x00000002,
    AUTOCOMPLETE_FLAG_FRIEND = 0x00000004,
    AUTOCOMPLETE_FLAG_BNET = 0x00000008,
    AUTOCOMPLETE_FLAG_INTERACTED_WITH = 0x00000010,
    AUTOCOMPLETE_FLAG_ONLINE = 0x00000020,
};

struct AUTOCOMPLETE_NAMENODE : public TSLinkedNode<AUTOCOMPLETE_NAMENODE> {
    /* 0008 */ char m_name[308];
    /* 013C */ uint32_t m_flags;
    /* 0140 */ uint32_t m_lastUsed;
    /* 0144 */ uint32_t m_presenceId;
    /* 0148 */ WOWGUID m_guid;
};

class CGAutoCompleteName {
    public:
    static int32_t RemoveFlagsFromAllNodes(uint32_t flags);
    static TSList<AUTOCOMPLETE_NAMENODE, TSGetLink<AUTOCOMPLETE_NAMENODE>> m_list;
    static bool s_resortNamesOnRecency;
    static int32_t RemoveFlagsByName(const char* name, uint32_t flags, bool characters, bool battlenet);
    static bool s_whenEditingFromCenter;
    static bool s_useContext;

    static void ResortAll();
    static int32_t GetPresenceID(const char* name, uint32_t* presenceId);
    static void AddFlags(const WOWGUID& guid, uint32_t flags, int32_t add, const char* name);
    static int32_t AddFlags(const uint32_t& presenceId, uint32_t flags, int32_t add, const char* name);
    static int32_t RemoveFlags(const WOWGUID& guid, uint32_t flags);
    static int32_t RemoveFlags(const uint32_t& presenceId, uint32_t flags);
    static int32_t SetName(const uint32_t& presenceId, const char* name, bool overwrite);
    static AUTOCOMPLETE_NAMENODE* FindNodeByName(const char* name, bool characters, bool battlenet);
    static AUTOCOMPLETE_NAMENODE* AddPlayerToList(const uint32_t& presenceId, uint32_t flags, uint32_t lastUsed, const char* name);
    static void RefreshName(const WOWGUID& guid);
    static int32_t SetName(const WOWGUID& guid, const char* name, bool overwrite);
    static int32_t NodeMatchesParams(AUTOCOMPLETE_NAMENODE* node, const char* text, uint32_t include, uint32_t exclude, int32_t cursor, bool allowFullMatch);
    static int32_t GetMatchingResults(lua_State* L, const char* text, uint32_t include, uint32_t exclude, int32_t numReturns, int32_t cursor, bool allowFullMatch);
};

void AutoCompleteRegisterScriptFunctions();
void AutoCompleteUnregisterScriptFunctions();

#endif
