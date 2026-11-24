#ifndef WOW_SOURCE_DB_REC_SPELLCATEGORYREC_H
#define WOW_SOURCE_DB_REC_SPELLCATEGORYREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum SPELL_CATEGORY_ID : int32_t {
    SPELL_CATEGORY_ITEM_COMBAT_CONSUMABLE = 4,
    SPELL_CATEGORY_UNKNOWN_9 = 9,
    SPELL_CATEGORY_UNKNOWN_10 = 10,
    SPELL_CATEGORY_ITEM_FOOD = 11,
};

enum SPELL_CATEGORY_FLAGS : int32_t {
    SPELL_CATEGORY_FLAG_COOLDOWN_MODIFIES_ITEM = 0x1,
    SPELL_CATEGORY_FLAG_COOLDOWN_IS_GLOBAL = 0x2,
    SPELL_CATEGORY_FLAG_COOLDOWN_EVENT_ON_LEAVE_COMBAT = 0x4,
};

class SpellCategoryRec {
    public:
    static const int32_t s_numColumns = 2;
    static const int32_t s_rowSize = 8;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_flags;

    SpellCategoryRec();
    ~SpellCategoryRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
