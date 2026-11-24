#ifndef WOW_SOURCE_DB_REC_SPELLDIFFICULTYREC_H
#define WOW_SOURCE_DB_REC_SPELLDIFFICULTYREC_H

#include "WoW/Source/DB/Rec/MapDifficultyRec.h"

#include <cstddef>
#include <cstdint>

class SFileImpl;

class SpellDifficultyRec {
    public:
    static const int32_t s_numColumns = 5;
    static const int32_t s_rowSize = 20;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_difficultySpellID [NUM_MAP_DIFFICULTIES];

    SpellDifficultyRec();
    ~SpellDifficultyRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
