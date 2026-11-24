#ifndef WOW_SOURCE_DB_REC_STATIONERYREC_H
#define WOW_SOURCE_DB_REC_STATIONERYREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class StationeryRec {
    public:

    enum STATIONERY_FLAGS : int32_t {
        DEFAULT = 1,
    };

    static const int32_t s_numColumns = 4;
    static const int32_t s_rowSize = 16;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_itemID;
    /* 0008 */ const char* m_texture;
    /* 000C */ int32_t m_flags;

    StationeryRec();
    ~StationeryRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
