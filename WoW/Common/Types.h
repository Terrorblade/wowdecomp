#ifndef WOW_COMMON_TYPES_H
#define WOW_COMMON_TYPES_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Gx/Types.h"

enum CHARACTER_CLASS : uint32_t {
    CLASS_WARRIOR = 1,
    CLASS_PALADIN = 2,
    CLASS_HUNTER = 3,
    CLASS_ROGUE = 4,
    CLASS_PRIEST = 5,
    CLASS_DEATH_KNIGHT = 6,
    CLASS_SHAMAN = 7,
    CLASS_MAGE = 8,
    CLASS_WARLOCK = 9,
    CLASS_DRUID = 11,
    NUM_CLASSES = 11,
};

enum CHARACTER_RACE : uint32_t {
    RACE_HUMAN = 1,
    RACE_ORC = 2,
    RACE_DWARF = 3,
    RACE_NIGHT_ELF = 4,
    RACE_UNDEAD = 5,
    RACE_TAUREN = 6,
    RACE_GNOME = 7,
    RACE_TROLL = 8,
    RACE_GOBLIN = 9,
    RACE_BLOOD_ELF = 10,
    RACE_DRAENEI = 11,
    NUM_PLAYABLE_RACES = RACE_DRAENEI + 1,
    RACE_FEL_ORC = 12,
    RACE_NAGA = 13,
    RACE_BROKEN = 14,
    RACE_SKELETON = 15,
    RACE_VRYKUL = 16,
    RACE_TUSKARR = 17,
    RACE_FOREST_TROLL = 18,
    RACE_TAUNKA = 19,
    RACE_NORTHREND_SKELETON = 20,
    RACE_ICE_TROLL = 21,
    NUM_RACES = 21,
};

class CAaBspNode {
    public:
    enum : uint16_t {
        Flag_XAxis = 0,
        Flag_YAxis = 1,
        Flag_ZAxis = 2,
        Flag_AxisMask = 3,
        Flag_Leaf = 4,
        Flag_NoChild = 0xFFFF,
    };

    // Axis values select x/y/z; Flag_NoChild is a child-index sentinel.
    /* 0000 */ uint16_t m_flags;
    /* 0002 */ uint16_t m_negChild;
    /* 0004 */ uint16_t m_posChild;
    /* 0006 */ uint16_t m_nFaces;
    /* 0008 */ uint32_t m_faceStart;
    /* 000C */ float m_planeDist;
};

#endif
