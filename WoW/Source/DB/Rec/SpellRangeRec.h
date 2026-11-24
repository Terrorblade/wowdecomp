#ifndef WOW_SOURCE_DB_REC_SPELLRANGEREC_H
#define WOW_SOURCE_DB_REC_SPELLRANGEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum SPELL_RANGE_FLAGS : int32_t {
    SPELL_RANGE_FLAG_MELEE = 0x1,
    SPELL_RANGE_FLAG_RANGED = 0x2,
};

class SpellRangeRec {
    public:
    static const int32_t s_numColumns = 40;
    static const int32_t s_rowSize = 160;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ float m_rangeMin [2];
    /* 000C */ float m_rangeMax [2];
    /* 0014 */ int32_t m_flags;
    /* 0018 */ const char* m_displayName;
    /* 001C */ const char* m_displayNameShort;

    SpellRangeRec();
    ~SpellRangeRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
