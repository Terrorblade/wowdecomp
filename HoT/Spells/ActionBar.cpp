#include "HoT/Spells/ActionBar.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/SpellEnums.h"
#include "WoW/Source/DB/Rec/SummonPropertiesRec.h"
#include "WoW/Source/Ui/ActionBarFrame.h"
#include "WoW/Source/Ui/GameUI.h"

namespace HoT::Spells {

static const int32_t SUMMON_SLOT_TOTEM_FIRE = 1;
static const uint32_t FIRST_MULTICAST_SLOT = 0x84;
static const uint32_t NUM_MULTICAST_SLOTS = 12;
static const uint32_t NUM_ACTION_SLOTS = 0x90;

uint32_t GetTotemSlotMask(const SpellRec* spell) {
    if (!spell) {
        return 0;
    }

    for (uint32_t i = 0; i < 3; i++) {
        if (spell->m_effect[i] != SPELL_EFFECT_SUMMON || !spell->m_effectMiscValueB[i]) {
            continue;
        }

        const SummonPropertiesRec* props = g_summonPropertiesDB.GetRecord(spell->m_effectMiscValueB[i]);

        if (!props || props->m_title != SUMMON_TYPE_TOTEM) {
            continue;
        }

        int32_t slot = props->m_slot - SUMMON_SLOT_TOTEM_FIRE;

        if (slot < 0 || slot > 3) {
            continue;
        }

        return 1u << slot;
    }

    return 0;
}

bool IsTotemForMultiCastSlot(int32_t slot, const SpellRec* spell) {
    if (!spell || slot < static_cast<int32_t>(FIRST_MULTICAST_SLOT) || slot >= static_cast<int32_t>(FIRST_MULTICAST_SLOT + NUM_MULTICAST_SLOTS)) {
        return false;
    }

    uint32_t mask = GetTotemSlotMask(spell);
    return mask && mask == (1u << ((slot - FIRST_MULTICAST_SLOT) % 4));
}

void OfferLearnedSpell(const SpellRec* spell, int32_t slot) {
    if (!spell || !spell->m_id || GetTotemSlotMask(spell)) {
        return;
    }

    for (uint32_t i = 0; i < NUM_ACTION_SLOTS; i++) {
        int32_t petSpell;

        if (CGActionBar::GetSpell(i, petSpell) == spell->m_id) {
            return;
        }
    }

    // The UI handler places the spell with PlaceAction, which takes whatever is on the cursor.
    CGGameUI::SetCursorSpell(spell->m_id);
    Lua::SignalEvent("HOT_PLACE_ACTION", "%u%u", static_cast<uint32_t>(spell->m_id), static_cast<uint32_t>(slot));
    CGGameUI::ClearCursor(1, 1);
}

}
