#ifndef WOW_SOURCE_DB_REC_LIQUIDTYPEREC_H
#define WOW_SOURCE_DB_REC_LIQUIDTYPEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum LIQUID_TYPE_FLAGS : int32_t {
    LIQUID_TYPE_FLAG_NONE                    = 0x00000000,
    LIQUID_TYPE_FLAG_RIPPLE                  = 0x00000001,
    LIQUID_TYPE_FLAG_SPLASH                  = 0x00000002,
    LIQUID_TYPE_FLAG_WATER_WALKING           = 0x00000004,
    LIQUID_TYPE_FLAG_PARTICLES               = 0x00000008,
    LIQUID_TYPE_FLAG_PARTICLE_FOG            = 0x00000010,
    LIQUID_TYPE_FLAG_WMO_FOG_CONTROL         = 0x00000020,
    LIQUID_TYPE_FLAG_NO_INTERIOR_FOG_BLEND   = 0x00000040,
    LIQUID_TYPE_FLAG_OUTDOOR_AMBIENCE_ONLY   = 0x00000080,
    LIQUID_TYPE_FLAG_WMO_FOG_CONTROL_SLIME   = 0x00000100,
    LIQUID_TYPE_FLAG_FORCE_EXTERIOR_LIGHTING = 0x00000200,
};

enum LIQUID_BASIC_TYPE : int32_t {
    LIQUID_BASIC_TYPE_WATER = 0,
    LIQUID_BASIC_TYPE_OCEAN = 1,
    LIQUID_BASIC_TYPE_MAGMA = 2,
    LIQUID_BASIC_TYPE_SLIME = 3,
    NUM_LIQUID_BASIC_TYPES  = 4,
};

enum LIQUID_TYPE_ID : int32_t {
    LIQUID_TYPE_WATER              = 1,
    LIQUID_TYPE_OCEAN              = 2,
    LIQUID_TYPE_MAGMA              = 3,
    LIQUID_TYPE_SLIME              = 4,
    LIQUID_TYPE_WMO_WATER          = 13,
    LIQUID_TYPE_WMO_OCEAN          = 14,
    LIQUID_TYPE_GREEN_LAVA         = 15,
    LIQUID_TYPE_WMO_WATER_INTERIOR = 17,
    LIQUID_TYPE_WMO_MAGMA          = 19,
    LIQUID_TYPE_WMO_SLIME          = 20,
    NUM_BASIC_LIQUID_TYPES         = 21,
};

enum LIQUID_PARTICLE_MOVEMENT : int32_t {
    LIQUID_PARTICLE_MOVEMENT_DEFAULT = 0,
    LIQUID_PARTICLE_MOVEMENT_SINK = 1,
    LIQUID_PARTICLE_MOVEMENT_RISE = 2,
};

class LiquidTypeRec {
    public:
    static const int32_t s_numColumns = 45;
    static const int32_t s_rowSize = 180;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_flags;
    /* 000C */ int32_t m_soundBank;
    /* 0010 */ int32_t m_soundID;
    /* 0014 */ int32_t m_spellID;
    /* 0018 */ float m_maxDarkenDepth;
    /* 001C */ float m_fogDarkenIntensity;
    /* 0020 */ float m_ambDarkenIntensity;
    /* 0024 */ float m_dirDarkenIntensity;
    /* 0028 */ int32_t m_lightID;
    /* 002C */ float m_particleScale;
    /* 0030 */ int32_t m_particleMovement;
    /* 0034 */ int32_t m_particleTexSlots;
    /* 0038 */ int32_t m_materialID;
    /* 003C */ const char* m_texture [6];
    /* 0054 */ int32_t m_color [2];
    /* 005C */ float m_float [18];
    /* 00A4 */ int32_t m_int [4];

    LiquidTypeRec();
    ~LiquidTypeRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
