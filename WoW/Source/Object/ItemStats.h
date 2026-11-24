#ifndef WOW_SOURCE_OBJECT_ITEMSTATS_H
#define WOW_SOURCE_OBJECT_ITEMSTATS_H

#include <cstddef>
#include <cstdint>
#include "WoW/Common/ItemTypes.h"
#include "WoW/Source/Object/Types.h"

class CDataStore;
constexpr int32_t NUM_ITEM_FLAG_FIELDS = 2;

class ItemStats_C {
    public:
    static const uint32_t s_cacheVersion = 5;

    /* 0004 */ uint32_t m_class;
    /* 0008 */ uint32_t m_subClass;
    /* 000C */ uint32_t m_soundOverrideSubclass;
    /* 0010 */ uint32_t m_displayId;
    /* 0014 */ uint32_t m_quality;
    /* 0018 */ uint32_t m_flags [NUM_ITEM_FLAG_FIELDS];
    /* 0020 */ int32_t m_buyPrice;
    /* 0024 */ uint32_t m_sellPrice;
    /* 0028 */ uint32_t m_inventoryType;
    /* 002C */ uint32_t m_allowableClass;
    /* 0030 */ uint32_t m_allowableRace;
    /* 0034 */ uint32_t m_itemLevel;
    /* 0038 */ uint32_t m_requiredLevel;
    /* 003C */ uint32_t m_requiredSkill;
    /* 0040 */ uint32_t m_requiredSkillRank;
    /* 0044 */ uint32_t m_requiredSpell;
    /* 0048 */ uint32_t m_requiredHonorRank;
    /* 004C */ uint32_t m_requiredCityRank;
    /* 0050 */ uint32_t m_requiredReputationFaction;
    /* 0054 */ uint32_t m_requiredReputationRank;
    /* 0058 */ uint32_t m_maxCount;
    /* 005C */ int32_t m_stackable;
    /* 0060 */ uint32_t m_containerSlots;
    /* 0064 */ int32_t m_statsCount;
    /* 0068 */ uint32_t m_statType [NUM_ITEM_STATS];
    /* 0090 */ uint32_t m_statValue [NUM_ITEM_STATS];
    /* 00B8 */ uint32_t m_scalingStatDistribution;
    /* 00BC */ uint32_t m_scalingStatValue;
    /* 00C0 */ float m_damageMin [NUM_ITEM_DAMAGES];
    /* 00C8 */ float m_damageMax [NUM_ITEM_DAMAGES];
    /* 00D0 */ uint32_t m_damageType [NUM_ITEM_DAMAGES];
    /* 00D8 */ uint32_t m_resistances [NUM_SPELL_SCHOOLS];
    /* 00F4 */ int32_t m_delay;
    /* 00F8 */ uint32_t m_ammoType;
    /* 00FC */ float m_rangedModRange;
    /* 0100 */ uint32_t m_spellId [NUM_ITEM_SPELLS];
    /* 0114 */ uint32_t m_spellTrigger [NUM_ITEM_SPELLS];
    /* 0128 */ int32_t m_spellCharges [NUM_ITEM_SPELLS];
    /* 013C */ uint32_t m_spellCooldown [NUM_ITEM_SPELLS];
    /* 0150 */ uint32_t m_spellCategory [NUM_ITEM_SPELLS];
    /* 0164 */ uint32_t m_spellCategoryCooldown [NUM_ITEM_SPELLS];
    /* 0178 */ uint32_t m_bonding;
    /* 017C */ char* m_description;
    /* 0180 */ uint32_t m_pageText;
    /* 0184 */ uint32_t m_languageId;
    /* 0188 */ uint32_t m_pageMaterial;
    /* 018C */ uint32_t m_startQuest;
    /* 0190 */ uint32_t m_lockId;
    /* 0194 */ uint32_t m_material;
    /* 0198 */ uint32_t m_sheatheType;
    /* 019C */ uint32_t m_randomProperty;
    /* 01A0 */ uint32_t m_randomSuffix;
    /* 01A4 */ uint32_t m_block;
    /* 01A8 */ uint32_t m_itemSet;
    /* 01AC */ uint32_t m_maxDurability;
    /* 01B0 */ uint32_t m_area;
    /* 01B4 */ uint32_t m_map;
    /* 01B8 */ uint32_t m_bagFamily;
    /* 01BC */ uint32_t m_totemCategory;
    /* 01C0 */ uint32_t m_socketColor [MAX_NUM_SOCKETS];
    /* 01CC */ uint32_t m_socketContent [MAX_NUM_SOCKETS];
    /* 01D8 */ uint32_t m_socketBonus;
    /* 01DC */ uint32_t m_gemProperties;
    /* 01E0 */ uint32_t m_requiredDisenchantSkill;
    /* 01E4 */ float m_armorDamageModifier;
    /* 01E8 */ uint32_t m_duration;
    /* 01EC */ uint32_t m_itemLimitCategory;
    /* 01F0 */ uint32_t m_holidayId;
    /* 01F4 */ char* m_name [4];

    virtual ~ItemStats_C();

    ItemStats_C();
    void Pack(CDataStore* msg);
    void Unpack(CDataStore* msg);
    const char* GetName(int32_t index) const;

    enum SCALING_STAT_FLAGS : uint32_t {
        BUDGET_SHOULDER = 0x1,
        BUDGET_TRINKET = 0x2,
        BUDGET_WEAPON_ONE_HAND = 0x4,
        BUDGET_PRIMARY = 0x8,
        BUDGET_RANGED = 0x10,
        ARMOR_CLOTH_SHOULDER = 0x20,
        ARMOR_LEATHER_SHOULDER = 0x40,
        ARMOR_MAIL_SHOULDER = 0x80,
        ARMOR_PLATE_SHOULDER = 0x100,
        DAMAGE_ONE_HAND = 0x200,
        DAMAGE_TWO_HAND = 0x400,
        DAMAGE_CASTER_ONE_HAND = 0x800,
        DAMAGE_CASTER_TWO_HAND = 0x1000,
        DAMAGE_RANGED = 0x2000,
        DAMAGE_WAND = 0x4000,
        SPELL_POWER = 0x8000,
        BUDGET_TERTIARY = 0x40000,
        ARMOR_CLOTH_CLOAK = 0x80000,
        ARMOR_CLOTH_CHEST = 0x100000,
        ARMOR_LEATHER_CHEST = 0x200000,
        ARMOR_MAIL_CHEST = 0x400000,
        ARMOR_PLATE_CHEST = 0x800000,
        BUDGET_MASK = 0x4001F,
        ARMOR_MASK = 0xF801E0,
        DAMAGE_MASK = 0x7E00,
    };

};

#endif
