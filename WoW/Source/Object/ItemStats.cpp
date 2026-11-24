#include "ItemStats.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Common/ComponentCore/Types.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include <cstring>

// INLINED
ItemStats_C::ItemStats_C() {
    this->m_description = nullptr;
    this->m_name[0] = nullptr;
    this->m_name[1] = nullptr;
    this->m_name[2] = nullptr;
    this->m_name[3] = nullptr;
}

// VERIFY: 8d459023 manual
// OFFSET: [0x00668020, 0x00668860]
ItemStats_C::~ItemStats_C() {
    for (uint32_t i = 0; i < 4; i++) {
        if (this->m_name[i]) {
            SMemFree(this->m_name[i], __FILE__, __LINE__, 0);
        }
    }

    if (this->m_description) {
        SMemFree(this->m_description, __FILE__, __LINE__, 0);
    }
}

// VERIFY: eb115b6b manual
// OFFSET: 0x0098D910
void ItemStats_C::Unpack(CDataStore* msg) {
    char name[400];
    char description[1024];

    msg->Get(this->m_class);
    msg->Get(this->m_subClass);
    msg->Get(this->m_soundOverrideSubclass);

    for (uint32_t i = 0; i < 4; i++) {
        msg->GetString(name, sizeof(name));

        if (this->m_name[i] && name[0] && !SStrCmp(this->m_name[i], name, STORM_MAX_STR)) {
            continue;
        }

        if (this->m_name[i]) {
            UnitCombatLogInvalidateName(this->m_name[i]);
            SMemFree(this->m_name[i], __FILE__, __LINE__, 0);
        }

        this->m_name[i] = name[0] ? SStrDupA(name, __FILE__, __LINE__) : nullptr;
    }

    msg->Get(this->m_displayId);
    msg->Get(this->m_quality);

    for (uint32_t i = 0; i < 2; i++) {
        msg->Get(this->m_flags[i]);
    }

    msg->Get(reinterpret_cast<uint32_t&>(this->m_buyPrice));
    msg->Get(this->m_sellPrice);
    msg->Get(this->m_inventoryType);

    if (this->m_inventoryType > INVTYPE_RELIC) {
        this->m_inventoryType = 0;
    }

    msg->Get(this->m_allowableClass);
    msg->Get(this->m_allowableRace);
    msg->Get(this->m_itemLevel);
    msg->Get(this->m_requiredLevel);
    msg->Get(this->m_requiredSkill);
    msg->Get(this->m_requiredSkillRank);
    msg->Get(this->m_requiredSpell);
    msg->Get(this->m_requiredHonorRank);
    msg->Get(this->m_requiredCityRank);
    msg->Get(this->m_requiredReputationFaction);
    msg->Get(this->m_requiredReputationRank);
    msg->Get(this->m_maxCount);
    msg->Get(reinterpret_cast<uint32_t&>(this->m_stackable));
    msg->Get(this->m_containerSlots);
    msg->Get(reinterpret_cast<uint32_t&>(this->m_statsCount));

    memset(this->m_statValue, 0, sizeof(this->m_statValue));

    const int32_t maxStats = sizeof(this->m_statType) / sizeof(this->m_statType[0]);
    int32_t stat = 0;

    for (int32_t i = 0; i < this->m_statsCount; i++) {
        uint32_t statType = 0;
        uint32_t statValue = 0;
        msg->Get(statType);
        msg->Get(statValue);

        if (msg->m_read > msg->m_size) {
            break;
        }

        if (stat < maxStats) {
            this->m_statType[stat] = statType;
            this->m_statValue[stat] = statValue;
            stat++;
        }
    }

    this->m_statsCount = stat;

    for (; stat < maxStats; stat++) {
        this->m_statType[stat] = -1;
    }

    msg->Get(this->m_scalingStatDistribution);
    msg->Get(this->m_scalingStatValue);

    for (uint32_t i = 0; i < NUM_ITEM_DAMAGES; i++) {
        msg->Get(reinterpret_cast<uint32_t&>(this->m_damageMin[i]));
        msg->Get(reinterpret_cast<uint32_t&>(this->m_damageMax[i]));
        msg->Get(this->m_damageType[i]);
    }

    for (uint32_t i = 0; i < NUM_SPELL_SCHOOLS; i++) {
        msg->Get(this->m_resistances[i]);
    }

    msg->Get(reinterpret_cast<uint32_t&>(this->m_delay));
    msg->Get(this->m_ammoType);
    msg->Get(reinterpret_cast<uint32_t&>(this->m_rangedModRange));

    for (uint32_t i = 0; i < NUM_ITEM_SPELLS; i++) {
        msg->Get(this->m_spellId[i]);
        msg->Get(this->m_spellTrigger[i]);
        msg->Get(reinterpret_cast<uint32_t&>(this->m_spellCharges[i]));
        msg->Get(this->m_spellCooldown[i]);
        msg->Get(this->m_spellCategory[i]);
        msg->Get(this->m_spellCategoryCooldown[i]);
    }

    msg->Get(this->m_bonding);
    msg->GetString(description, sizeof(description));

    if (!(this->m_description && description[0] && !SStrCmp(this->m_description, description, STORM_MAX_STR))) {
        if (this->m_description) {
            SMemFree(this->m_description, __FILE__, __LINE__, 0);
        }

        this->m_description = description[0] ? SStrDupA(description, __FILE__, __LINE__) : nullptr;
    }

    msg->Get(this->m_pageText);
    msg->Get(this->m_languageId);
    msg->Get(this->m_pageMaterial);
    msg->Get(this->m_startQuest);
    msg->Get(this->m_lockId);
    msg->Get(this->m_material);
    msg->Get(this->m_sheatheType);
    msg->Get(this->m_randomProperty);
    msg->Get(this->m_randomSuffix);
    msg->Get(this->m_block);
    msg->Get(this->m_itemSet);
    msg->Get(this->m_maxDurability);
    msg->Get(this->m_area);
    msg->Get(this->m_map);
    msg->Get(this->m_bagFamily);
    msg->Get(this->m_totemCategory);

    for (uint32_t i = 0; i < MAX_NUM_SOCKETS; i++) {
        msg->Get(this->m_socketColor[i]);
        msg->Get(this->m_socketContent[i]);
    }

    msg->Get(this->m_socketBonus);
    msg->Get(this->m_gemProperties);
    msg->Get(this->m_requiredDisenchantSkill);
    msg->Get(reinterpret_cast<uint32_t&>(this->m_armorDamageModifier));
    msg->Get(this->m_duration);
    msg->Get(this->m_itemLimitCategory);
    msg->Get(this->m_holidayId);
}

// VERIFY: 4da68567 auto
// OFFSET: 0x0098DDB0
void ItemStats_C::Pack(CDataStore* msg) {
    msg->Put(this->m_class);
    msg->Put(this->m_subClass);
    msg->Put(this->m_soundOverrideSubclass);

    for (uint32_t i = 0; i < 4; i++) {
        if (this->m_name[i]) {
            msg->PutString(this->m_name[i]);
        } else {
            msg->PutString("");
        }
    }

    msg->Put(this->m_displayId);
    msg->Put(this->m_quality);

    for (uint32_t i = 0; i < 2; i++) {
        msg->Put(this->m_flags[i]);
    }

    msg->Put(static_cast<uint32_t>(this->m_buyPrice));
    msg->Put(this->m_sellPrice);
    msg->Put(this->m_inventoryType);
    msg->Put(this->m_allowableClass);
    msg->Put(this->m_allowableRace);
    msg->Put(this->m_itemLevel);
    msg->Put(this->m_requiredLevel);
    msg->Put(this->m_requiredSkill);
    msg->Put(this->m_requiredSkillRank);
    msg->Put(this->m_requiredSpell);
    msg->Put(this->m_requiredHonorRank);
    msg->Put(this->m_requiredCityRank);
    msg->Put(this->m_requiredReputationFaction);
    msg->Put(this->m_requiredReputationRank);
    msg->Put(this->m_maxCount);
    msg->Put(static_cast<uint32_t>(this->m_stackable));
    msg->Put(this->m_containerSlots);
    msg->Put(static_cast<uint32_t>(this->m_statsCount));

    for (int32_t stat = 0; stat < this->m_statsCount; stat++) {
        msg->Put(this->m_statType[stat]);
        msg->Put(this->m_statValue[stat]);
    }

    msg->Put(this->m_scalingStatDistribution);
    msg->Put(this->m_scalingStatValue);

    for (uint32_t i = 0; i < NUM_ITEM_DAMAGES; i++) {
        msg->Put(reinterpret_cast<uint32_t&>(this->m_damageMin[i]));
        msg->Put(reinterpret_cast<uint32_t&>(this->m_damageMax[i]));
        msg->Put(this->m_damageType[i]);
    }

    for (uint32_t i = 0; i < NUM_SPELL_SCHOOLS; i++) {
        msg->Put(this->m_resistances[i]);
    }

    msg->Put(static_cast<uint32_t>(this->m_delay));
    msg->Put(this->m_ammoType);
    msg->Put(reinterpret_cast<uint32_t&>(this->m_rangedModRange));

    for (uint32_t i = 0; i < NUM_ITEM_SPELLS; i++) {
        msg->Put(this->m_spellId[i]);
        msg->Put(this->m_spellTrigger[i]);
        msg->Put(static_cast<uint32_t>(this->m_spellCharges[i]));
        msg->Put(this->m_spellCooldown[i]);
        msg->Put(this->m_spellCategory[i]);
        msg->Put(this->m_spellCategoryCooldown[i]);
    }

    msg->Put(this->m_bonding);
    msg->PutString(this->m_description ? this->m_description : "");
    msg->Put(this->m_pageText);
    msg->Put(this->m_languageId);
    msg->Put(this->m_pageMaterial);
    msg->Put(this->m_startQuest);
    msg->Put(this->m_lockId);
    msg->Put(this->m_material);
    msg->Put(this->m_sheatheType);
    msg->Put(this->m_randomProperty);
    msg->Put(this->m_randomSuffix);
    msg->Put(this->m_block);
    msg->Put(this->m_itemSet);
    msg->Put(this->m_maxDurability);
    msg->Put(this->m_area);
    msg->Put(this->m_map);
    msg->Put(this->m_bagFamily);
    msg->Put(this->m_totemCategory);

    for (uint32_t i = 0; i < MAX_NUM_SOCKETS; i++) {
        msg->Put(this->m_socketColor[i]);
        msg->Put(this->m_socketContent[i]);
    }

    msg->Put(this->m_socketBonus);
    msg->Put(this->m_gemProperties);
    msg->Put(this->m_requiredDisenchantSkill);
    msg->Put(reinterpret_cast<uint32_t&>(this->m_armorDamageModifier));
    msg->Put(this->m_duration);
    msg->Put(this->m_itemLimitCategory);
    msg->Put(this->m_holidayId);
}

// VERIFY: 3cdca47e auto
// OFFSET: 0x004FD200
const char* ItemStats_C::GetName(int32_t index) const {
    const char* name = this->m_name[index];

    if (!name || !*name) {
        return this->m_name[0];
    }

    return name;
}
