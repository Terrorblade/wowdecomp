#ifndef WOW_SOURCE_DB_REC_CREATURESPELLDATAREC_H
#define WOW_SOURCE_DB_REC_CREATURESPELLDATAREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_CREATURE_SPELL_DATA_SPELLS = 4;

class CreatureSpellDataRec {
    public:
    static const int32_t s_numColumns = 9;
    static const int32_t s_rowSize = 36;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_spells [NUM_CREATURE_SPELL_DATA_SPELLS];
    /* 0014 */ int32_t m_availability [NUM_CREATURE_SPELL_DATA_SPELLS];

    CreatureSpellDataRec();
    ~CreatureSpellDataRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
