#include "HoT/Spells/DescriptionVars.h"
#include "HoT/Features/Units.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Ui/MathState.h"

namespace HoT::Spells {

static const uint32_t POWER_MANA = 0;
static const uint32_t POWER_FOCUS = 2;

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

static int32_t s_headlessLevel = 0;

void SetHeadlessLevel(int32_t level) {
    s_headlessLevel = level > 0 ? level : 0;
}

int32_t GetHeadlessLevel() {
    return s_headlessLevel;
}

bool GetHeadlessVariable(uint32_t op, const SpellRec* spell, float& value) {
    if (!s_headlessLevel || !spell) {
        return false;
    }

    switch (op) {
        case MATHOP_LEVEL:
        case MATHOP_LEVEL_UPPER:
            value = static_cast<float>(s_headlessLevel);
            return true;

        // Base costs, the stock ops need a unit.
        case MATHOP_POWER_COST:
        case MATHOP_POWER_COST_UPPER:
            value = static_cast<float>(spell->m_manaCost);
            return true;

        case MATHOP_POWER_COST_PER_SECOND:
        case MATHOP_POWER_COST_PER_SECOND_UPPER:
            value = static_cast<float>(spell->m_manaPerSecond);
            return true;

        // One-handed, so $HND conditionals take the common branch.
        case MATHOP_HANDS:
        case MATHOP_HANDS_UPPER:
            value = 1.0f;
            return true;

        case MATHOP_MISC_VALUE_1:
        case MATHOP_MISC_VALUE_2:
        case MATHOP_MISC_VALUE_3:
            value = static_cast<float>(spell->m_effectMiscValue[op - MATHOP_MISC_VALUE_1]);
            return true;

        case MATHOP_MISC_VALUE_1_UPPER:
        case MATHOP_MISC_VALUE_2_UPPER:
        case MATHOP_MISC_VALUE_3_UPPER:
            value = static_cast<float>(spell->m_effectMiscValue[op - MATHOP_MISC_VALUE_1_UPPER]);
            return true;

        case MATHOP_BONUS_COEFFICIENT_1:
        case MATHOP_BONUS_COEFFICIENT_2:
        case MATHOP_BONUS_COEFFICIENT_3:
            value = spell->m_effectBonusCoefficient[op - MATHOP_BONUS_COEFFICIENT_1];
            return true;

        case MATHOP_BONUS_COEFFICIENT_1_UPPER:
        case MATHOP_BONUS_COEFFICIENT_2_UPPER:
        case MATHOP_BONUS_COEFFICIENT_3_UPPER:
            value = spell->m_effectBonusCoefficient[op - MATHOP_BONUS_COEFFICIENT_1_UPPER];
            return true;

        case MATHOP_HOT_AURA_1:
        case MATHOP_HOT_AURA_2:
        case MATHOP_HOT_AURA_3:
            value = BaseEffectPoints(spell, op - MATHOP_HOT_AURA_1 + 1);
            return true;

        case MATHOP_HOT_AURA_1_UPPER:
        case MATHOP_HOT_AURA_2_UPPER:
        case MATHOP_HOT_AURA_3_UPPER:
            value = BaseEffectPoints(spell, op - MATHOP_HOT_AURA_1_UPPER + 1);
            return true;

        case MATHOP_HOT_ABSORB:
        case MATHOP_HOT_ABSORB_UPPER:
            value = BaseEffectPoints(spell, 1);
            return true;

        case MATHOP_HOT_STACKS:
        case MATHOP_HOT_STACKS_UPPER:
            value = 1.0f;
            return true;
    }

    return false;
}

static float AuraAmount(const SpellRec* spell, uint32_t effectIndex) {
    int32_t amount = 0;

    if (spell && AuraValues::GetActiveAmount(spell->m_id, effectIndex, amount)) {
        return static_cast<float>(amount);
    }

    return BaseEffectPoints(spell, effectIndex);
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
