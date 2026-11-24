#ifndef WOW_SOURCE_DB_REC_AREAGROUPREC_H
#define WOW_SOURCE_DB_REC_AREAGROUPREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_AREA_GROUP_AREAS = 6;

class AreaGroupRec {
    public:
    static const int32_t s_numColumns = 8;
    static const int32_t s_rowSize = 32;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_areaID [NUM_AREA_GROUP_AREAS];
    /* 001C */ int32_t m_nextAreaID;

    AreaGroupRec();
    ~AreaGroupRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
