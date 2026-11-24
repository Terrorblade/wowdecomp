#ifndef WOW_SOURCE_DB_REC_CREATUREMODELDATAREC_H
#define WOW_SOURCE_DB_REC_CREATUREMODELDATAREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class CreatureModelDataRec {
    public:

    enum MODEL_FLAGS : int32_t {
        NO_FOOTPRINT_PARTICLES = 0x1,
        NO_BREATH_PARTICLES = 2,
        IS_PLAYER_MODEL = 0x4,
        NO_TORSO_SPLIT = 0x8,
        NO_ATTACHED_WEAPONS = 0x10,
        NO_FOOTPRINT_TRAIL_TEXTURES = 0x20,
        DISABLE_HIGHLIGHT = 0x40,
        CAN_MOUNT_WHILE_TRANSFORMED = 0x80,
        FORCE_PROJECTED_TEXTURES = 0x200,
        CAN_JUMP_IN_PLACE_AS_MOUNT = 0x400,
    };

    static const int32_t s_numColumns = 28;
    static const int32_t s_rowSize = 112;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_flags;
    /* 0008 */ const char* m_modelName;
    /* 000C */ int32_t m_sizeClass;
    /* 0010 */ float m_modelScale;
    /* 0014 */ int32_t m_bloodID;
    /* 0018 */ int32_t m_footprintTextureID;
    /* 001C */ float m_footprintTextureLength;
    /* 0020 */ float m_footprintTextureWidth;
    /* 0024 */ float m_footprintParticleScale;
    /* 0028 */ int32_t m_foleyMaterialID;
    /* 002C */ int32_t m_footstepShakeSize;
    /* 0030 */ int32_t m_deathThudShakeSize;
    /* 0034 */ int32_t m_soundID;
    /* 0038 */ float m_collisionWidth;
    /* 003C */ float m_collisionHeight;
    /* 0040 */ float m_mountHeight;
    /* 0044 */ float m_geoBoxMinX;
    /* 0048 */ float m_geoBoxMinY;
    /* 004C */ float m_geoBoxMinZ;
    /* 0050 */ float m_geoBoxMaxX;
    /* 0054 */ float m_geoBoxMaxY;
    /* 0058 */ float m_geoBoxMaxZ;
    /* 005C */ float m_worldEffectScale;
    /* 0060 */ float m_attachedEffectScale;
    /* 0064 */ float m_missileCollisionRadius;
    /* 0068 */ float m_missileCollisionPush;
    /* 006C */ float m_missileCollisionRaise;

    CreatureModelDataRec();
    ~CreatureModelDataRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
