#ifndef WOW_SOURCE_DB_REC_MAPREC_H
#define WOW_SOURCE_DB_REC_MAPREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum MAP_FLAGS : int32_t {
    MAP_FLAG_NONE               = 0x00000000,
    MAP_FLAG_DYNAMIC_DIFFICULTY = 0x00000100,
};

enum MAP_INSTANCE_TYPE : int32_t {
    MAP_INSTANCE_TYPE_NOT_INSTANCED      = 0,
    MAP_INSTANCE_TYPE_PARTY_DUNGEON      = 1,
    MAP_INSTANCE_TYPE_RAID_DUNGEON       = 2,
    MAP_INSTANCE_TYPE_PVP_BATTLEFIELD    = 3,
    MAP_INSTANCE_TYPE_ARENA_BATTLEFIELD  = 4,
};

enum MAP_ID : int32_t {
    MAP_PVP_ZONE01       = 30,
    MAP_PVP_ZONE03       = 489,
    MAP_PVP_ZONE04       = 529,
    MAP_EXPANSION01      = 530,
    MAP_HELLFIRE_RAMPART = 543,
    MAP_NETHERSTORM_BG   = 566,
    MAP_NORTHREND        = 571,
    MAP_UTGARDE_PINNACLE = 575,
    MAP_NORTHREND_BG     = 607,
    MAP_ISLE_OF_CONQUEST = 628,
};

class MapRec {
    public:
    static const int32_t s_numColumns = 66;
    static const int32_t s_rowSize = 264;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_directory;
    /* 0008 */ int32_t m_instanceType;
    /* 000C */ int32_t m_flags;
    /* 0010 */ int32_t m_pvp;
    /* 0014 */ const char* m_mapName;
    /* 0018 */ int32_t m_areaTableID;
    /* 001C */ const char* m_mapDescription0;
    /* 0020 */ const char* m_mapDescription1;
    /* 0024 */ int32_t m_loadingScreenID;
    /* 0028 */ float m_minimapIconScale;
    /* 002C */ int32_t m_corpseMapID;
    /* 0030 */ float m_corpse [2];
    /* 0038 */ int32_t m_timeOfDayOverride;
    /* 003C */ int32_t m_expansionID;
    /* 0040 */ int32_t m_raidOffset;
    /* 0044 */ int32_t m_maxPlayers;

    MapRec();
    ~MapRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
