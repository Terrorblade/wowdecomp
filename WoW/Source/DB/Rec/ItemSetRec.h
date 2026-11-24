#ifndef WOW_SOURCE_DB_REC_ITEMSETREC_H
#define WOW_SOURCE_DB_REC_ITEMSETREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_ITEM_SET_SPELLS = 8;
constexpr int32_t NUM_ITEM_SET_ITEMS = 17;

class ItemSetRec {
    public:
    static const int32_t s_numColumns = 53;
    static const int32_t s_rowSize = 212;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_itemID [NUM_ITEM_SET_ITEMS];
    /* 004C */ int32_t m_setSpellID [NUM_ITEM_SET_SPELLS];
    /* 006C */ int32_t m_setThreshold [NUM_ITEM_SET_SPELLS];
    /* 008C */ int32_t m_requiredSkill;
    /* 0090 */ int32_t m_requiredSkillRank;

    ItemSetRec();
    ~ItemSetRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
