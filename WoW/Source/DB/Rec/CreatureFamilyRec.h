#ifndef WOW_SOURCE_DB_REC_CREATUREFAMILYREC_H
#define WOW_SOURCE_DB_REC_CREATUREFAMILYREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_CREATURE_FAMILY_SKILL_LINES = 2;

class CreatureFamilyRec {
    public:
    static const int32_t s_numColumns = 28;
    static const int32_t s_rowSize = 112;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ float m_minScale;
    /* 0008 */ int32_t m_minScaleLevel;
    /* 000C */ float m_maxScale;
    /* 0010 */ int32_t m_maxScaleLevel;
    /* 0014 */ int32_t m_skillLine [NUM_CREATURE_FAMILY_SKILL_LINES];
    /* 001C */ int32_t m_petFoodMask;
    /* 0020 */ int32_t m_petTalentType;
    /* 0024 */ int32_t m_categoryEnumID;
    /* 0028 */ const char* m_name;
    /* 002C */ const char* m_iconFile;

    CreatureFamilyRec();
    ~CreatureFamilyRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
