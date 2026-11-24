#ifndef WOW_SOURCE_DB_REC_LFGDUNGEONSREC_H
#define WOW_SOURCE_DB_REC_LFGDUNGEONSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum LFG_DUNGEON_FLAGS : int32_t {
    LFG_DUNGEON_FLAG_NONE       = 0x00000000,
    LFG_DUNGEON_FLAG_LFD_LISTED = 0x00000002,
    LFG_DUNGEON_FLAG_HOLIDAY    = 0x00000004,
};

class LfgDungeonsRec {
    public:
    static const int32_t s_numColumns = 49;
    static const int32_t s_rowSize = 196;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_minLevel;
    /* 000C */ int32_t m_maxLevel;
    /* 0010 */ int32_t m_targetLevel;
    /* 0014 */ int32_t m_targetLevelMin;
    /* 0018 */ int32_t m_targetLevelMax;
    /* 001C */ int32_t m_mapID;
    /* 0020 */ int32_t m_difficulty;
    /* 0024 */ int32_t m_flags;
    /* 0028 */ int32_t m_typeID;
    /* 002C */ int32_t m_faction;
    /* 0030 */ const char* m_textureFilename;
    /* 0034 */ int32_t m_expansionLevel;
    /* 0038 */ int32_t m_orderIndex;
    /* 003C */ int32_t m_groupID;
    /* 0040 */ const char* m_description;

    LfgDungeonsRec();
    ~LfgDungeonsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
