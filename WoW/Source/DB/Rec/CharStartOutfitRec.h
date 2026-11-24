#ifndef WOW_SOURCE_DB_REC_CHARSTARTOUTFITREC_H
#define WOW_SOURCE_DB_REC_CHARSTARTOUTFITREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_CHAR_START_OUTFIT_ITEMS = 24;

class CharStartOutfitRec {
    public:
    static const int32_t s_numColumns = 77;
    static const int32_t s_rowSize = 296;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int8_t m_raceID;
    /* 0005 */ int8_t m_classID;
    /* 0006 */ int8_t m_sexID;
    /* 0007 */ int8_t m_outfitID;
    /* 0008 */ int32_t m_itemID [NUM_CHAR_START_OUTFIT_ITEMS];
    /* 0068 */ int32_t m_displayItemID [NUM_CHAR_START_OUTFIT_ITEMS];
    /* 00C8 */ int32_t m_inventoryType [NUM_CHAR_START_OUTFIT_ITEMS];

    CharStartOutfitRec();
    ~CharStartOutfitRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
