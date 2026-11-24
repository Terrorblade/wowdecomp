#ifndef WOW_SOURCE_DB_REC_ITEMGROUPSOUNDSREC_H
#define WOW_SOURCE_DB_REC_ITEMGROUPSOUNDSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_ITEM_GROUP_SOUNDS = 4;

class ItemGroupSoundsRec {
    public:
    static const int32_t s_numColumns = 5;
    static const int32_t s_rowSize = 20;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_sound [NUM_ITEM_GROUP_SOUNDS];

    ItemGroupSoundsRec();
    ~ItemGroupSoundsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
