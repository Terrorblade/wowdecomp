#ifndef WOW_SOURCE_DB_REC_DURABILITYCOSTSREC_H
#define WOW_SOURCE_DB_REC_DURABILITYCOSTSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class DurabilityCostsRec {
    public:

    enum SUBCLASS_COST_COUNTS : int32_t {
        NUM_WEAPON_SUBCLASS_COSTS = 21,
        NUM_ARMOR_SUBCLASS_COSTS = 8,
    };

    static const int32_t s_numColumns = 30;
    static const int32_t s_rowSize = 120;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_weaponSubClassCost [DurabilityCostsRec::NUM_WEAPON_SUBCLASS_COSTS];
    /* 0058 */ int32_t m_armorSubClassCost [DurabilityCostsRec::NUM_ARMOR_SUBCLASS_COSTS];

    DurabilityCostsRec();
    ~DurabilityCostsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
