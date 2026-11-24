#ifndef WOW_SOURCE_DB_REC_PETPERSONALITYREC_H
#define WOW_SOURCE_DB_REC_PETPERSONALITYREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum PET_PERSONALITY_ID : int32_t {
    PET_PERSONALITY_ID_STANDARD = 1,
};

class PetPersonalityRec {
    public:
    static const int32_t s_numColumns = 24;
    static const int32_t s_rowSize = 96;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_happinessThreshold [3];
    /* 0014 */ float m_happinessDamage [3];

    PetPersonalityRec();
    ~PetPersonalityRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
