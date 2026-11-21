#include "HoT/Spells/DescriptionVars.h"
#include "HoT/Features/Units.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellShapeshiftFormRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/Object/ObjectClient/Bag_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Ui/MathState.h"

namespace HoT::Spells {

static const uint32_t POWER_MANA = 0;
static const uint32_t POWER_FOCUS = 2;
static const int32_t RATING_HASTE_MELEE = 17;
static const int32_t RATING_HASTE_RANGED = 18;
static const int32_t SLOT_MAIN_HAND = 15;
static const int32_t SLOT_RANGED = 17;
static const uint32_t BASE_ATTACK_TIME = 2000;

static uint32_t UnhastedSwingMs(CGPlayer_C* player, bool ranged) {
    if (!ranged) {
        SpellShapeshiftFormRec* form = g_spellShapeshiftFormDB.GetRecord(player->GetShapeshiftForm());

        if (form && form->m_combatRoundTime > 0) {
            return static_cast<uint32_t>(form->m_combatRoundTime);
        }
    }

    const CGItem_C* item = player->GetBag()->GetItemPointer(ranged ? SLOT_RANGED : SLOT_MAIN_HAND);

    if (!item || !item->IsUsable()) {
        return BASE_ATTACK_TIME;
    }

    WOWGUID none = { 0 };
    ItemStats_C* stats = g_itemDBCache.GetRecord(item->m_obj->m_entryID, none, nullptr, nullptr, false);
    return stats && stats->m_delay > 0 ? static_cast<uint32_t>(stats->m_delay) : 0;
}

static float SwingHastePct(CGPlayer_C* player, bool ranged) {
    uint32_t swing = ranged ? player->m_unit->m_unitFieldRangedattacktime : player->m_unit->m_unitFieldBaseattacktime[0];
    uint32_t base = UnhastedSwingMs(player, ranged);

    if (!swing || !base) {
        return player->GetCombatRatingPct(ranged ? RATING_HASTE_RANGED : RATING_HASTE_MELEE);
    }

    return (static_cast<float>(base) / static_cast<float>(swing) - 1.0f) * 100.0f;
}

static float SpellHastePct(CGPlayer_C* player) {
    float castSpeed = player->m_unit->m_unitModCastSpeed;
    return castSpeed > 0.0f ? (1.0f / castSpeed - 1.0f) * 100.0f : 0.0f;
}

// What $s<n> would have shown, for when no aura is in scope or the server sent nothing yet.
static float BaseEffectPoints(const SpellRec* spell, uint32_t effectIndex) {
    if (!spell || effectIndex < 1 || effectIndex > AuraValues::NUM_AURA_EFFECTS) {
        return 0.0f;
    }

    int32_t minPoints = 0;
    int32_t maxPoints = 0;
    Spell_C_GetMinMaxPoints(spell, effectIndex - 1, &minPoints, &maxPoints, 0, 0, 0, 0);
    return static_cast<float>(maxPoints);
}

static float AuraAmount(const SpellRec* spell, uint32_t effectIndex) {
    int32_t amount = 0;

    if (spell && AuraValues::GetActiveAmount(spell->m_id, effectIndex, amount)) {
        return static_cast<float>(amount);
    }

    return BaseEffectPoints(spell, effectIndex);
}

static float AuraTickSeconds(const SpellRec* spell, uint32_t effectIndex) {
    if (!spell || effectIndex < 1 || effectIndex > AuraValues::NUM_AURA_EFFECTS) {
        return 0.0f;
    }

    uint32_t periodMs = 0;

    if (!AuraValues::GetActivePeriod(spell->m_id, effectIndex, periodMs)) {
        int32_t basePeriod = spell->m_effectAuraPeriod[effectIndex - 1];
        periodMs = basePeriod > 0 ? static_cast<uint32_t>(basePeriod) : 0;
    }

    return static_cast<float>(periodMs) / 1000.0f;
}

bool GetDescriptionVariable(uint32_t op, CGPlayer_C* player, const SpellRec* spell, float& value) {
    CGUnitData* unit = player->m_unit;

    switch (op) {
        case MATHOP_HOT_HEALTH:
            value = static_cast<float>(static_cast<uint32_t>(unit->m_unitFieldHealth));
            return true;

        case MATHOP_HOT_MAX_HEALTH:
            value = static_cast<float>(unit->m_unitFieldMaxhealth);
            return true;

        case MATHOP_HOT_MANA:
            value = static_cast<float>(unit->m_unitFieldPower[POWER_MANA]);
            return true;

        case MATHOP_HOT_MAX_MANA:
            value = static_cast<float>(unit->m_unitFieldMaxpower[POWER_MANA]);
            return true;

        case MATHOP_HOT_FOCUS:
            value = static_cast<float>(unit->m_unitFieldPower[POWER_FOCUS]);
            return true;

        case MATHOP_HOT_MAX_FOCUS:
            value = static_cast<float>(unit->m_unitFieldMaxpower[POWER_FOCUS]);
            return true;

        // Item level is server pushed, a miss asks for it and the next render has the number.
        case MATHOP_HOT_ITEM_LEVEL:
        case MATHOP_HOT_ITEM_LEVEL_UPPER:
            value = static_cast<float>(UnitLevel::GetOrRequest(player->m_obj->m_guid));
            return true;

        case MATHOP_HOT_DODGE:
        case MATHOP_HOT_DODGE_UPPER:
            value = player->m_player->m_playerDodgePercentage;
            return true;

        case MATHOP_HOT_PARRY:
        case MATHOP_HOT_PARRY_UPPER:
            value = player->m_player->m_playerParryPercentage;
            return true;

        case MATHOP_HOT_AURA_1:
        case MATHOP_HOT_AURA_2:
        case MATHOP_HOT_AURA_3:
            value = AuraAmount(spell, op - MATHOP_HOT_AURA_1 + 1);
            return true;

        case MATHOP_HOT_AURA_1_UPPER:
        case MATHOP_HOT_AURA_2_UPPER:
        case MATHOP_HOT_AURA_3_UPPER:
            value = AuraAmount(spell, op - MATHOP_HOT_AURA_1_UPPER + 1);
            return true;

        case MATHOP_HOT_ABSORB:
        case MATHOP_HOT_ABSORB_UPPER: {
            int32_t amount = 0;
            value = spell && AuraValues::GetActiveAbsorb(spell->m_id, spell, amount) ? static_cast<float>(amount) : BaseEffectPoints(spell, 1);
            return true;
        }

        case MATHOP_HOT_STACKS:
        case MATHOP_HOT_STACKS_UPPER: {
            int32_t stacks = 0;
            value = spell && AuraValues::GetActiveStacks(spell->m_id, stacks) ? static_cast<float>(stacks) : 1.0f;
            return true;
        }

        case MATHOP_HOT_TICK_1:
        case MATHOP_HOT_TICK_2:
        case MATHOP_HOT_TICK_3:
            value = AuraTickSeconds(spell, op - MATHOP_HOT_TICK_1 + 1);
            return true;

        case MATHOP_HOT_TICK_1_UPPER:
        case MATHOP_HOT_TICK_2_UPPER:
        case MATHOP_HOT_TICK_3_UPPER:
            value = AuraTickSeconds(spell, op - MATHOP_HOT_TICK_1_UPPER + 1);
            return true;

        case MATHOP_HOT_MELEE_HASTE:
        case MATHOP_HOT_MELEE_HASTE_UPPER:
            value = SwingHastePct(player, false);
            return true;

        case MATHOP_HOT_RANGED_HASTE:
        case MATHOP_HOT_RANGED_HASTE_UPPER:
            value = SwingHastePct(player, true);
            return true;

        case MATHOP_HOT_SPELL_HASTE:
        case MATHOP_HOT_SPELL_HASTE_UPPER:
            value = SpellHastePct(player);
            return true;

        // The stock $rw and $RW round down and up and clamp to 1, HoT hands back the raw field so formulas work on the real value.
        case MATHOP_RANGED_DAMAGE_MIN:
            value = unit->m_unitFieldMinrangeddamage;
            return true;

        case MATHOP_RANGED_DAMAGE_MAX:
            value = unit->m_unitFieldMaxrangeddamage;
            return true;

        case MATHOP_RANGED_SPEED:
        case MATHOP_RANGED_SPEED_UPPER: {
            float seconds = static_cast<float>(unit->m_unitFieldRangedattacktime) / 1000.0f;
            value = seconds < 1.0f ? 1.0f : seconds;
            return true;
        }
    }

    return false;
}

}
