#ifndef WOW_SOURCE_DB_REC_ITEMVISUALSREC_H
#define WOW_SOURCE_DB_REC_ITEMVISUALSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_ITEM_VISUAL_SLOTS = 5;

class ItemVisualsRec {
    public:
    static const int32_t s_numColumns = 6;
    static const int32_t s_rowSize = 24;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_slot [NUM_ITEM_VISUAL_SLOTS];

    ItemVisualsRec();
    ~ItemVisualsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
