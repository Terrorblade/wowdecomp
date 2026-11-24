#ifndef WOW_SOURCE_DB_REC_TALENTREC_H
#define WOW_SOURCE_DB_REC_TALENTREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t MAX_TALENT_RANKS = 9;
constexpr int32_t MAX_TALENT_PREREQS = 3;

enum TALENT_FLAGS : int32_t {
    TALENT_FLAG_NONE        = 0x00000000,
    TALENT_FLAG_EXCEPTIONAL = 0x00000001,
};

class TalentRec {
    public:
    static const int32_t s_numColumns = 23;
    static const int32_t s_rowSize = 92;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_tabID;
    /* 0008 */ int32_t m_tierID;
    /* 000C */ int32_t m_columnIndex;
    /* 0010 */ int32_t m_spellRank [MAX_TALENT_RANKS];
    /* 0034 */ int32_t m_prereqTalent [MAX_TALENT_PREREQS];
    /* 0040 */ int32_t m_prereqRank [MAX_TALENT_PREREQS];
    /* 004C */ int32_t m_flags;
    /* 0050 */ int32_t m_requiredSpellID;
    /* 0054 */ int32_t m_categoryMask [2];

    TalentRec();
    ~TalentRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
