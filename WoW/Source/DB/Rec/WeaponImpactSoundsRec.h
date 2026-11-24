#ifndef WOW_SOURCE_DB_REC_WEAPONIMPACTSOUNDSREC_H
#define WOW_SOURCE_DB_REC_WEAPONIMPACTSOUNDSREC_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/SoundInterface/Types.h"

class SFileImpl;
constexpr int32_t NUM_WEAPON_IMPACT_SOUNDS = 10;

class WeaponImpactSoundsRec {
    public:
    static const int32_t s_numColumns = 23;
    static const int32_t s_rowSize = 92;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_weaponSubClassID;
    /* 0008 */ PARRYMATERIALS m_parrySoundType;
    /* 000C */ int32_t m_impactSoundID [NUM_WEAPON_IMPACT_SOUNDS];
    /* 0034 */ int32_t m_critImpactSoundID [NUM_WEAPON_IMPACT_SOUNDS];

    WeaponImpactSoundsRec();
    ~WeaponImpactSoundsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
