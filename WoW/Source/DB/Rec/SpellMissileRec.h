#ifndef WOW_SOURCE_DB_REC_SPELLMISSILEREC_H
#define WOW_SOURCE_DB_REC_SPELLMISSILEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum SPELL_MISSILE_FLAGS : int32_t {
    SPELL_MISSILE_FLAG_TRAJECTORY = 0x1,
    SPELL_MISSILE_FLAG_UNKNOWN_1 = 0x2,
    SPELL_MISSILE_FLAG_COLLIDE_WITH_ATTACKABLE_PLAYERS = 0x4,
    SPELL_MISSILE_FLAG_COLLIDE_WITH_NON_ATTACKABLE_PLAYERS = 0x8,
    SPELL_MISSILE_FLAG_COLLIDE_WITH_ATTACKABLE_CREATURES = 0x10,
    SPELL_MISSILE_FLAG_COLLIDE_WITH_NON_ATTACKABLE_CREATURES = 0x20,
    SPELL_MISSILE_FLAG_PLAYER_COLLISION_MASK = SPELL_MISSILE_FLAG_COLLIDE_WITH_ATTACKABLE_PLAYERS
        | SPELL_MISSILE_FLAG_COLLIDE_WITH_NON_ATTACKABLE_PLAYERS,
    SPELL_MISSILE_FLAG_CREATURE_COLLISION_MASK = SPELL_MISSILE_FLAG_COLLIDE_WITH_ATTACKABLE_CREATURES
        | SPELL_MISSILE_FLAG_COLLIDE_WITH_NON_ATTACKABLE_CREATURES,
    SPELL_MISSILE_FLAG_UNIT_COLLISION_MASK = SPELL_MISSILE_FLAG_PLAYER_COLLISION_MASK
        | SPELL_MISSILE_FLAG_CREATURE_COLLISION_MASK,
};

class SpellMissileRec {
    public:
    static const int32_t s_numColumns = 15;
    static const int32_t s_rowSize = 60;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_flags;
    /* 0008 */ float m_defaultPitchMin;
    /* 000C */ float m_defaultPitchMax;
    /* 0010 */ float m_defaultSpeedMin;
    /* 0014 */ float m_defaultSpeedMax;
    /* 0018 */ float m_randomizeFacingMin;
    /* 001C */ float m_randomizeFacingMax;
    /* 0020 */ float m_randomizePitchMin;
    /* 0024 */ float m_randomizePitchMax;
    /* 0028 */ float m_randomizeSpeedMin;
    /* 002C */ float m_randomizeSpeedMax;
    /* 0030 */ float m_gravity;
    /* 0034 */ float m_maxDuration;
    /* 0038 */ float m_collisionRadius;

    SpellMissileRec();
    ~SpellMissileRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
