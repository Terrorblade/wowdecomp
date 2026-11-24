#ifndef WOW_SOURCE_OBJECT_CREATURESTATS_H
#define WOW_SOURCE_OBJECT_CREATURESTATS_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Storm/H/storm.h"

class CDataStore;

enum CREATURE_CLASSIFICATION : uint32_t {
    CREATURE_CLASSIFICATION_NORMAL     = 0,
    CREATURE_CLASSIFICATION_ELITE      = 1,
    CREATURE_CLASSIFICATION_RARE_ELITE = 2,
    CREATURE_CLASSIFICATION_WORLD_BOSS = 3,
    CREATURE_CLASSIFICATION_RARE       = 4,
    CREATURE_CLASSIFICATION_TRIVIAL    = 5,
    NUM_CREATURE_CLASSIFICATIONS       = 6,
};

enum CREATURE_TYPE_FLAGS : uint32_t
{
    CREATURE_TYPE_FLAG_NONE = 0x00000000u,
    CREATURE_TYPE_FLAG_TAMEABLE                              = 0x00000001u,
    CREATURE_TYPE_FLAG_VISIBLE_TO_GHOSTS                     = 0x00000002u,
    CREATURE_TYPE_FLAG_BOSS_MOB                              = 0x00000004u,
    CREATURE_TYPE_FLAG_DO_NOT_PLAY_WOUND_ANIM                = 0x00000008u,
    CREATURE_TYPE_FLAG_NO_FACTION_TOOLTIP                    = 0x00000010u,
    CREATURE_TYPE_FLAG_MORE_AUDIBLE                          = 0x00000020u,
    CREATURE_TYPE_FLAG_NO_HARMFUL_VERTEX_COLORING            = 0x00000040u,
    CREATURE_TYPE_FLAG_INTERACT_WHILE_DEAD                   = 0x00000080u,
    CREATURE_TYPE_FLAG_SKIN_WITH_HERBALISM                   = 0x00000100u,
    CREATURE_TYPE_FLAG_SKIN_WITH_MINING                      = 0x00000200u,
    CREATURE_TYPE_FLAG_NO_DEATH_MESSAGE                      = 0x00000400u,
    CREATURE_TYPE_FLAG_ALLOW_MOUNTED_COMBAT                  = 0x00000800u,
    CREATURE_TYPE_FLAG_CAN_ASSIST                            = 0x00001000u,
    CREATURE_TYPE_FLAG_NO_PET_BAR                            = 0x00002000u,
    CREATURE_TYPE_FLAG_MASK_UID                              = 0x00004000u,
    CREATURE_TYPE_FLAG_SKIN_WITH_ENGINEERING                 = 0x00008000u,
    CREATURE_TYPE_FLAG_TAMEABLE_EXOTIC                       = 0x00010000u,
    CREATURE_TYPE_FLAG_USE_MODEL_COLLISION_SIZE              = 0x00020000u,
    CREATURE_TYPE_FLAG_ALLOW_INTERACTION_WHILE_IN_COMBAT     = 0x00040000u,
    CREATURE_TYPE_FLAG_COLLIDE_WITH_MISSILES                 = 0x00080000u,
    CREATURE_TYPE_FLAG_NO_NAME_PLATE                         = 0x00100000u,
    CREATURE_TYPE_FLAG_NO_DODGE_PARRY_BLOCK_ANIMS            = 0x00200000u,
    CREATURE_TYPE_FLAG_LINK_ALL                              = 0x00400000u,
    CREATURE_TYPE_FLAG_GOSSIP_ONLY_WITH_CREATOR              = 0x00800000u,
    CREATURE_TYPE_FLAG_NO_DEATH_SCREAM                       = 0x01000000u,
    CREATURE_TYPE_FLAG_HAS_NO_SHADOW_BLOB                    = 0x02000000u,
    CREATURE_TYPE_FLAG_TREAT_AS_RAID_UNIT                    = 0x04000000u,
    CREATURE_TYPE_FLAG_FORCE_GOSSIP                          = 0x08000000u,
    CREATURE_TYPE_FLAG_DO_NOT_SHEATHE                        = 0x10000000u,
    CREATURE_TYPE_FLAG_UNUSED_29                             = 0x20000000u,
    CREATURE_TYPE_FLAG_UNUSED_30                             = 0x40000000u,
    CREATURE_TYPE_FLAG_UNUSED_31                             = 0x80000000u,
};

class CreatureStats {
    public:
    /* 0004 */ char* m_subName;
    /* 0008 */ char* m_iconName;
    /* 000C */ uint32_t m_typeFlags;
    /* 0010 */ uint32_t m_type;
    /* 0014 */ uint32_t m_family;
    /* 0018 */ uint32_t m_rank;
    /* 001C */ uint32_t m_killCredit [2];
    /* 0024 */ uint32_t m_displayId [4];
    /* 0034 */ float m_hpModifier;
    /* 0038 */ float m_mpModifier;
    /* 003C */ bool m_racialLeader;
    /* 0040 */ uint32_t m_questItem [6];
    /* 0058 */ uint32_t m_movementId;
    /* 005C */ char* m_name [4];

    virtual ~CreatureStats();

    const char* GetName(int32_t index) const;
    void Pack(CDataStore* msg) const;
    void Unpack(CDataStore* msg);
};

// INLINED
inline CreatureStats::~CreatureStats() {
    for (uint32_t i = 0; i < 4; i++) {
        if (this->m_name[i]) {
            SMemFree(this->m_name[i], __FILE__, __LINE__, 0);
        }
    }

    if (this->m_subName) {
        SMemFree(this->m_subName, __FILE__, __LINE__, 0);
    }

    if (this->m_iconName) {
        SMemFree(this->m_iconName, __FILE__, __LINE__, 0);
    }
}

#endif
