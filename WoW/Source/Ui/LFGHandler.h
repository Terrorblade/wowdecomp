#ifndef WOW_SOURCE_UI_LFGHANDLER_H
#define WOW_SOURCE_UI_LFGHANDLER_H

#include <cstdint>

enum LFG_LIST_UPDATE_FLAGS : uint32_t {
    LFG_LIST_UPDATE_CHARACTER_INFO = 0x1,
    LFG_LIST_UPDATE_COMMENT = 0x2,
    LFG_LIST_UPDATE_LEADER = 0x4,
    LFG_LIST_UPDATE_GROUP_GUID = 0x8,
    LFG_LIST_UPDATE_ROLES = 0x10,
    LFG_LIST_UPDATE_AREA = 0x20,
    LFG_LIST_UPDATE_STATUS = 0x40,
    LFG_LIST_UPDATE_ENCOUNTERS = 0x80,
};

class CDataStore;
class LFGLIST;
class LFGListParty;
class LFGListPlayer;

namespace LFGHandler {
    void ReadGroupInfo(CDataStore* msg, LFGListParty* party);
    void ReadPlayerInfo(CDataStore* msg, LFGLIST* list, LFGListPlayer* player);
}

#endif
