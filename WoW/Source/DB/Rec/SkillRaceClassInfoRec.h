#ifndef WOW_SOURCE_DB_REC_SKILLRACECLASSINFOREC_H
#define WOW_SOURCE_DB_REC_SKILLRACECLASSINFOREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum SKILL_RACE_CLASS_FLAGS : int32_t {
    SKILL_RACE_CLASS_FLAG_NONE              = 0x00000000,
    SKILL_RACE_CLASS_FLAG_SHOW_UNLEARNED    = 0x00000001,
    SKILL_RACE_CLASS_FLAG_HIDDEN            = 0x00000002,
    SKILL_RACE_CLASS_FLAG_SHOW_AT_MIN_LEVEL = 0x00000004,
    SKILL_RACE_CLASS_FLAG_SHOW_TIER_COST    = 0x00000008,
    SKILL_RACE_CLASS_FLAG_ABANDONABLE       = 0x00000020,
    SKILL_RACE_CLASS_FLAG_NO_SPELLBOOK_SKILL = 0x00000080,
    SKILL_RACE_CLASS_FLAG_SINGLE_RANK       = 0x00000400,
};

class SkillRaceClassInfoRec {
    public:
    static const int32_t s_numColumns = 8;
    static const int32_t s_rowSize = 32;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_skillID;
    /* 0008 */ int32_t m_raceMask;
    /* 000C */ int32_t m_classMask;
    /* 0010 */ int32_t m_flags;
    /* 0014 */ int32_t m_minLevel;
    /* 0018 */ int32_t m_skillTierID;
    /* 001C */ int32_t m_skillCostIndex;

    SkillRaceClassInfoRec();
    ~SkillRaceClassInfoRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
