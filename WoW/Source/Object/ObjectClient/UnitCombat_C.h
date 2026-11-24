#ifndef WOW_SOURCE_OBJECT_OBJECTCLIENT_UNITCOMBAT_C_H
#define WOW_SOURCE_OBJECT_OBJECTCLIENT_UNITCOMBAT_C_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Object/ObjectClient/Types.h"
#include "WoW/Source/Net/Types.h"

class CDataStore;

enum HIT_INFO_FLAGS : uint32_t {
    HIT_INFO_FLAG_NONE = 0x00000000u,
    HIT_INFO_FLAG_DEBUG_DATA = 0x00000001u,
    HIT_INFO_FLAG_AFFECTS_VICTIM = 0x00000002u,
    HIT_INFO_FLAG_OFFHAND = 0x00000004u,
    HIT_INFO_FLAG_UNUSED_3 = 0x00000008u,
    HIT_INFO_FLAG_MISS = 0x00000010u,
    HIT_INFO_FLAG_FULL_ABSORB = 0x00000020u,
    HIT_INFO_FLAG_PARTIAL_ABSORB = 0x00000040u,
    HIT_INFO_FLAG_FULL_RESIST = 0x00000080u,
    HIT_INFO_FLAG_PARTIAL_RESIST = 0x00000100u,
    HIT_INFO_FLAG_CRITICAL = 0x00000200u,
    HIT_INFO_FLAG_UNUSED_10 = 0x00000400u,
    HIT_INFO_FLAG_UNUSED_11 = 0x00000800u,
    HIT_INFO_FLAG_UNUSED_12 = 0x00001000u,
    HIT_INFO_FLAG_BLOCK = 0x00002000u,
    HIT_INFO_FLAG_SUPPRESS_MISS_WORLD_TEXT = 0x00004000u,
    HIT_INFO_FLAG_LARGE_BLOOD_SPURT = 0x00008000u,
    HIT_INFO_FLAG_GLANCING = 0x00010000u,
    HIT_INFO_FLAG_CRUSHING = 0x00020000u,
    HIT_INFO_FLAG_SKIP_ATTACKER_ROUND = 0x00040000u,
    HIT_INFO_FLAG_UNUSED_19 = 0x00080000u,
    HIT_INFO_FLAG_UNUSED_20 = 0x00100000u,
    HIT_INFO_FLAG_UNKNOWN_21 = 0x00200000u,
    HIT_INFO_FLAG_UNUSED_22 = 0x00400000u,
    HIT_INFO_FLAG_RAGE_GAIN = 0x00800000u,
    HIT_INFO_FLAG_FORCE_DAMAGE_EFFECTS = 0x01000000u,
    HIT_INFO_FLAG_UNUSED_25 = 0x02000000u,
    HIT_INFO_FLAG_UNUSED_26 = 0x04000000u,
    HIT_INFO_FLAG_UNUSED_27 = 0x08000000u,
    HIT_INFO_FLAG_UNUSED_28 = 0x10000000u,
    HIT_INFO_FLAG_UNUSED_29 = 0x20000000u,
    HIT_INFO_FLAG_UNUSED_30 = 0x40000000u,
    HIT_INFO_FLAG_UNUSED_31 = 0x80000000u,
};

class ATTACKROUNDINFO {
    public:
    static constexpr uint32_t MAX_DAMAGE_TYPES = 2;
    /* 0000 */ WOWGUID m_attacker;
    /* 0008 */ WOWGUID m_target;
    /* 0010 */ uint32_t m_hitInfo;
    /* 0014 */ int32_t m_damage;
    /* 0018 */ int32_t m_overkill;
    /* 001C */ uint32_t m_schoolMask [MAX_DAMAGE_TYPES];
    /* 0024 */ float unk_0024 [2];
    /* 002C */ float unk_002C [2];
    /* 0034 */ float m_subDamage [MAX_DAMAGE_TYPES];
    /* 003C */ uint32_t m_subDamageInt [MAX_DAMAGE_TYPES];
    /* 0044 */ uint32_t m_absorbed [MAX_DAMAGE_TYPES];
    /* 004C */ uint32_t m_resisted [MAX_DAMAGE_TYPES];
    /* 0054 */ VICTIMSTATES m_victimState;
    /* 0058 */ uint32_t unk_0058;
    /* 005C */ uint32_t m_meleeSpellId;
    /* 0060 */ uint32_t m_blocked;
    /* 0064 */ uint32_t m_rageGained;
    /* 0068 */ uint32_t unk_0068;
    /* 006C */ float unk_006C [8];
    /* 008C */ uint32_t unk_008C [5];

    ATTACKROUNDINFO();
    ATTACKROUNDINFO& operator=(const ATTACKROUNDINFO& other);
    void Unpack(CDataStore& msg);
};

void TargetAttacker(const WOWGUID& attacker);
int32_t Packet_Group_29(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);

void UnitCombatClientInitialize();
void UnitCombatClientShutdown();

#endif
