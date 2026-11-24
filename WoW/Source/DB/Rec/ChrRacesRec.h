#ifndef WOW_SOURCE_DB_REC_CHRRACESREC_H
#define WOW_SOURCE_DB_REC_CHRRACESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class ChrRacesRec {
    public:

    enum RACE_FLAGS : int32_t {
        NON_PLAYABLE = 0x1,
        BARE_FEET = 0x2,
        CAN_MOUNT = 0x4,
        HAS_BALD = 0x8,
    };

    static const int32_t s_numColumns = 69;
    static const int32_t s_rowSize = 276;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_flags;
    /* 0008 */ int32_t m_factionID;
    /* 000C */ int32_t m_explorationSoundID;
    /* 0010 */ int32_t m_maleDisplayID;
    /* 0014 */ int32_t m_femaleDisplayID;
    /* 0018 */ const char* m_clientPrefix;
    /* 001C */ int32_t m_baseLanguage;
    /* 0020 */ int32_t m_creatureType;
    /* 0024 */ int32_t m_resSicknessSpellID;
    /* 0028 */ int32_t m_splashSoundID;
    /* 002C */ const char* m_clientFileString;
    /* 0030 */ int32_t m_cinematicSequenceID;
    /* 0034 */ int32_t m_alliance;
    /* 0038 */ const char* m_name;
    /* 003C */ const char* m_nameFemale;
    /* 0040 */ const char* m_nameMale;
    /* 0044 */ const char* m_facialHairCustomization [2];
    /* 004C */ const char* m_hairCustomization;
    /* 0050 */ int32_t m_requiredExpansion;

    ChrRacesRec();
    ~ChrRacesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
