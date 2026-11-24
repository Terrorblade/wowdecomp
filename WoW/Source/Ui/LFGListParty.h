#ifndef WOW_SOURCE_UI_LFGLISTPARTY_H
#define WOW_SOURCE_UI_LFGLISTPARTY_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/Ui/LFGRoles.h"
#include "WoW/Source/Ui/TalentInfo.h"

class LFGListParty;

class LFGListPlayer : public TSHashObject<LFGListPlayer, CHashKeyGUID> {
    public:
    /* 0020 */ int32_t m_index;
    /* 0024 */ int32_t m_partyIndex;
    /* 0028 */ LFGListParty* m_party;
    /* 002C */ uint8_t m_level;
    /* 002D */ uint8_t m_class;
    /* 002E */ uint8_t m_race;
    /* 002F */ uint8_t m_talents[MAX_TALENT_TABS];
    /* 0034 */ uint32_t m_armor;
    /* 0038 */ uint32_t m_spellDamage;
    /* 003C */ uint32_t m_spellHeal;
    /* 0040 */ uint32_t m_critMelee;
    /* 0044 */ uint32_t m_critRanged;
    /* 0048 */ uint32_t m_critSpell;
    /* 004C */ float m_mp5;
    /* 0050 */ float m_mp5Combat;
    /* 0054 */ uint32_t m_attackPower;
    /* 0058 */ uint32_t m_agility;
    /* 005C */ uint32_t m_maxHealth;
    /* 0060 */ uint32_t m_maxMana;
    /* 0064 */ uint32_t unk_0064;
    /* 0068 */ float m_avgItemLevel;
    /* 006C */ uint32_t m_defenseSkill;
    /* 0070 */ uint32_t m_dodge;
    /* 0074 */ uint32_t m_block;
    /* 0078 */ uint32_t m_parry;
    /* 007C */ uint32_t m_haste;
    /* 0080 */ uint32_t m_expertise;
    /* 0084 */ int32_t m_areaID;
    /* 0088 */ uint8_t unk_0088;
    /* 008C */ uint32_t m_leader;
    /* 0090 */ uint8_t m_roles;
    /* 0091 */ char m_comment[256];
    /* 0198 */ uint64_t unk_0198;
    /* 01A0 */ uint32_t m_encounterMask;

    LFGListPlayer() : m_index(-1), m_partyIndex(-1), m_party(nullptr), m_leader(0) {
    }

    LFGListPlayer(const LFGListPlayer& source);
};

class LFGListParty : public TSHashObject<LFGListParty, CHashKeyGUID> {
    public:
    /* 0020 */ int32_t m_index;
    /* 0024 */ char m_comment[256];
    /* 0124 */ uint8_t m_roles[3];
    /* 0128 */ uint64_t unk_0128;
    /* 0130 */ uint32_t m_encounterMask;
    /* 0134 */ TSGrowableArray<LFGListPlayer*> m_members;

    LFGListParty() : m_index(-1) {
    }

    LFGListParty(const LFGListParty& source);
    void AddPlayer(LFGListPlayer* player);
    void RemovePlayer(LFGListPlayer* player);
    LFGListPlayer* GetMember(uint32_t index);
    uint32_t GetNumMembers();
};

#endif
