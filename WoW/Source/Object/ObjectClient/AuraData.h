#ifndef WOW_SOURCE_OBJECT_OBJECTCLIENT_AURADATA_H
#define WOW_SOURCE_OBJECT_OBJECTCLIENT_AURADATA_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"

class CDataStore;
class CGUnit_C;

enum AURA_FLAGS : uint8_t {
    AURA_FLAG_EFFECT_0 = 0x1,
    AURA_FLAG_EFFECT_1 = 0x2,
    AURA_FLAG_EFFECT_2 = 0x4,
    AURA_FLAG_SELF_CAST = 0x8,
    AURA_FLAG_CANCELABLE = 0x10,
    AURA_FLAG_HAS_DURATION = 0x20,
    AURA_FLAG_UNKNOWN_6 = 0x40,
    AURA_FLAG_HARMFUL = 0x80,
    AURA_FLAG_EFFECT_MASK = AURA_FLAG_EFFECT_0 | AURA_FLAG_EFFECT_1 | AURA_FLAG_EFFECT_2,
};
constexpr int32_t AURA_HARMFUL_FLAG_SHIFT = 7;

#pragma pack(push, 4)
class AuraData {
    public:
    /* 0000 */ WOWGUID m_creator;
    /* 0008 */ int32_t m_spellId;
    /* 000C */ uint8_t m_flags;
    /* 000D */ uint8_t m_level;
    /* 000E */ uint8_t m_stackCount;
    /* 000F */ uint8_t unused_000F;
    /* 0010 */ int32_t m_duration;
    /* 0014 */ int32_t m_endTime;

    void Unpack(CGUnit_C* unit, uint32_t time, CDataStore* msg);
};

class SortedAuraInfo {
    public:
    /* 0000 */ uint32_t m_auraIndex;
    /* 0004 */ int32_t m_spellId;
};
#pragma pack(pop)

#endif
