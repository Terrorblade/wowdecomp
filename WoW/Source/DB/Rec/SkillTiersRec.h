#ifndef WOW_SOURCE_DB_REC_SKILLTIERSREC_H
#define WOW_SOURCE_DB_REC_SKILLTIERSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_SKILL_TIERS = 16;

class SkillTiersRec {
    public:
    static const int32_t s_numColumns = 33;
    static const int32_t s_rowSize = 132;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_cost [NUM_SKILL_TIERS];
    /* 0044 */ int32_t m_value [NUM_SKILL_TIERS];

    SkillTiersRec();
    ~SkillTiersRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
