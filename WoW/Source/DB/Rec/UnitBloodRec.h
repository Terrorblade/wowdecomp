#ifndef WOW_SOURCE_DB_REC_UNITBLOODREC_H
#define WOW_SOURCE_DB_REC_UNITBLOODREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_UNIT_BLOOD_GROUND_BLOOD = 5;

class UnitBloodRec {
    public:
    static const int32_t s_numColumns = 10;
    static const int32_t s_rowSize = 40;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_combatBloodSpurtFront [2];
    /* 000C */ int32_t m_combatBloodSpurtBack [2];
    /* 0014 */ const char* m_groundBlood [NUM_UNIT_BLOOD_GROUND_BLOOD];

    UnitBloodRec();
    ~UnitBloodRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
