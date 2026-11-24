#ifndef WOW_SOURCE_MAGIC_MAGICCLIENT_PENDINGSPELLCASTDATA_H
#define WOW_SOURCE_MAGIC_MAGICCLIENT_PENDINGSPELLCASTDATA_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Magic/MagicClient/SpellEnums.h"

class CDataStore;

class PendingSpellCastData {
    public:

    enum CastFlags : uint32_t {
        CAST_NONE = 0x0,
        CAST_HIDDEN = 0x1,
        CAST_HAS_AMMO = 0x20,
        CAST_SUPPRESS_RECAST_MASK = 0x41,
        CAST_MISSES_FIRST = 0x400,
        CAST_HAS_POWER = 0x800,
        CAST_HAS_TRAJECTORY = 0x20000,
        CAST_SKIP_START_RECOVERY = 0x40000,
        CAST_HAS_VISUAL_OVERRIDES = 0x80000,
        CAST_HAS_RUNES = 0x200000,
        CAST_HAS_IMMUNITIES = 0x4000000,
    };

    /* 0000 */ WOWGUID m_castItem;
    /* 0008 */ WOWGUID m_caster;
    /* 0010 */ uint32_t unused_0010;
    /* 0014 */ uint32_t unused_0014;
    /* 0018 */ uint32_t m_spellId;
    /* 001C */ uint8_t m_castCount;
    /* 001D */ uint8_t unused_001D [3];
    /* 0020 */ uint32_t m_targetFlags;
    /* 0024 */ uint32_t unused_0024;
    /* 0028 */ WOWGUID m_objectTarget;
    /* 0030 */ WOWGUID m_itemTarget;
    /* 0038 */ WOWGUID unk_0038;
    /* 0040 */ WOWGUID m_sourceTarget;
    /* 0048 */ WOWGUID m_destTarget;
    /* 0050 */ C3Vector m_sourcePos;
    /* 005C */ C3Vector m_destPos;
    /* 0068 */ float unused_0068;
    /* 006C */ uint32_t unused_006C;
    /* 0070 */ uint32_t unused_0070;
    /* 0074 */ uint32_t unused_0074;
    /* 0078 */ uint32_t unused_0078;
    /* 007C */ uint32_t unused_007C;
    /* 0080 */ uint32_t unused_0080;
    /* 0084 */ uint32_t unused_0084;
    /* 0088 */ uint32_t unused_0088;
    /* 008C */ uint32_t unused_008C;
    /* 0090 */ char m_targetString [128];
    /* 0110 */ uint32_t unused_0110;
    /* 0114 */ uint32_t m_castFlags;
    /* 0118 */ uint32_t unused_0118;
    /* 011C */ float m_elevation;
    /* 0120 */ float m_missileSpeed;
    /* 0124 */ uint32_t m_glyphSlot;
    /* 0128 */ uint32_t unused_0128;
    /* 012C */ uint32_t unused_012C;

    PendingSpellCastData();
    C3Vector GetDestLocation() const;
    static void WriteTargetInfo(const PendingSpellCastData* data, CDataStore* msg);
};

class CDataStore;

void SpellGetCastTargets(PendingSpellCastData* data, CDataStore* msg);

#endif
