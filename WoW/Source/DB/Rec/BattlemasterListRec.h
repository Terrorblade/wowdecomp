#ifndef WOW_SOURCE_DB_REC_BATTLEMASTERLISTREC_H
#define WOW_SOURCE_DB_REC_BATTLEMASTERLISTREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_BATTLEMASTER_LIST_MAPS = 8;

enum BATTLEMASTER_LIST_ID : int32_t {
    BATTLEMASTER_LIST_RANDOM_BATTLEGROUND = 32,
};

class BattlemasterListRec {
    public:
    static const int32_t s_numColumns = 32;
    static const int32_t s_rowSize = 128;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_mapID [NUM_BATTLEMASTER_LIST_MAPS];
    /* 0024 */ int32_t m_instanceType;
    /* 0028 */ int32_t m_groupsAllowed;
    /* 002C */ const char* m_name;
    /* 0030 */ int32_t m_maxGroupSize;
    /* 0034 */ int32_t m_holidayWorldState;
    /* 0038 */ int32_t m_minLevel;
    /* 003C */ int32_t m_maxLevel;

    BattlemasterListRec();
    ~BattlemasterListRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
