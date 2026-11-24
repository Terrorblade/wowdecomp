#ifndef WOW_SOURCE_DB_REC_CHRCLASSESREC_H
#define WOW_SOURCE_DB_REC_CHRCLASSESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class ChrClassesRec {
    public:

    enum CLASS_FLAGS : int32_t {
        USE_LOINCLOTH = 0x1,
        IS_PLAYER_CLASS = 0x2,
        DISPLAY_PET = 0x4,
        HAS_RELIC_SLOT = 0x8,
        CAN_WEAR_MAIL = 0x10,
        CAN_WEAR_PLATE = 0x20,
        BIND_STARTING_AREA = 0x40,
    };

    enum CLASS_FLAG_BITS : int32_t {
        HAS_RELIC_SLOT_BIT_INDEX = 3,
    };

    static const int32_t s_numColumns = 60;
    static const int32_t s_rowSize = 240;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_damageBonusStat;
    /* 0008 */ int32_t m_displayPower;
    /* 000C */ const char* m_petNameToken;
    /* 0010 */ const char* m_name;
    /* 0014 */ const char* m_nameFemale;
    /* 0018 */ const char* m_nameMale;
    /* 001C */ const char* m_filename;
    /* 0020 */ int32_t m_spellClassSet;
    /* 0024 */ int32_t m_flags;
    /* 0028 */ int32_t m_cinematicSequenceID;
    /* 002C */ int32_t m_requiredExpansion;

    ChrClassesRec();
    ~ChrClassesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
