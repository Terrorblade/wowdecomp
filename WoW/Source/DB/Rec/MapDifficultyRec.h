#ifndef WOW_SOURCE_DB_REC_MAPDIFFICULTYREC_H
#define WOW_SOURCE_DB_REC_MAPDIFFICULTYREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum MAP_DIFFICULTY : int32_t {
    MAP_DIFFICULTY_DEFAULT = 0,
    DUNGEON_DIFFICULTY_NORMAL = 0,
    DUNGEON_DIFFICULTY_HEROIC = 1,
    RAID_DIFFICULTY_10MAN_NORMAL = 0,
    RAID_DIFFICULTY_25MAN_NORMAL = 1,
    RAID_DIFFICULTY_10MAN_HEROIC = 2,
    RAID_DIFFICULTY_25MAN_HEROIC = 3,
};
constexpr int32_t NUM_MAP_DIFFICULTIES = 4;
constexpr int32_t MAX_MAP_DIFFICULTY_ID = NUM_MAP_DIFFICULTIES - 1;
constexpr int32_t RAID_DIFFICULTY_SIZE_MASK = 0x1;
constexpr int32_t RAID_DIFFICULTY_HEROIC_OFFSET = 2;

class MapDifficultyRec {
    public:
    static const int32_t s_numColumns = 23;
    static const int32_t s_rowSize = 92;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_mapID;
    /* 0008 */ int32_t m_difficulty;
    /* 000C */ const char* m_message;
    /* 0010 */ int32_t m_raidDuration;
    /* 0014 */ int32_t m_maxPlayers;
    /* 0018 */ const char* m_difficultystring;

    MapDifficultyRec();
    ~MapDifficultyRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
