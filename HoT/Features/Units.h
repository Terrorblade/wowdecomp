#ifndef HOT_FEATURES_UNITS_H
#define HOT_FEATURES_UNITS_H

#include <cstdint>
#include "WoW/Source/Object/WGUID.h"

class CGTooltip;
class CGUnit_C;
class ChrClassesRec;
class SpellRec;
struct lua_State;

namespace HoT {
class Module;
}

// Server side aura amounts for the aura a tooltip is showing, plus absorb totals.
namespace HoT::AuraValues {

static const uint32_t NUM_AURA_EFFECTS = 3;

struct Values {
    uint32_t m_spellId;
    int32_t m_amounts[NUM_AURA_EFFECTS];
    uint8_t m_effectMask;
    uint8_t m_stacks;
    uint8_t m_periodMask;
    uint32_t m_periodsMs[NUM_AURA_EFFECTS];
};

// effectIndex is 1 based to match $s1, false means fall back to base points.
bool GetActiveAmount(uint32_t spellId, uint32_t effectIndex, int32_t& out);
bool GetActiveAbsorb(uint32_t spellId, const SpellRec* spell, int32_t& out);
bool GetActiveStacks(uint32_t spellId, int32_t& out);
bool GetActivePeriod(uint32_t spellId, uint32_t effectIndex, uint32_t& outMs);
bool GetSlotValues(WOWGUID guid, uint8_t slot, uint32_t spellId, Values& out);

// Called right before SetTooltipUnitAura shows a unit aura.
void RecordTooltipCall(const CGUnit_C* unit, int32_t slot);

// Lives for the whole of CGTooltip::SetBuff, the aura variables resolve only inside it.
class TooltipScope {
    public:
    TooltipScope(CGTooltip* tooltip, int32_t spellId);
    ~TooltipScope();
};

Module* GetModule();

}

// Incoming heals and absorb shields per unit.
namespace HoT::HealPrediction {

uint32_t GetTotalAbsorbs(WOWGUID guid);
uint32_t GetTotalHealAbsorbs(WOWGUID guid);
uint32_t GetIncomingHeals(WOWGUID guid, WOWGUID healer);

Module* GetModule();

}

// Scaled levels: item level for players, dungeon level for creatures.
namespace HoT::UnitLevel {

bool HasPlayerItemLevel(WOWGUID guid);
uint32_t GetPlayerItemLevel(WOWGUID guid);
uint8_t GetPlayerSubClass(WOWGUID guid);
uint32_t GetUnitLevel(WOWGUID guid);

// Requests the value when missing, returns 0 until it arrives.
uint32_t GetOrRequest(WOWGUID guid);

// The cached level, or fallback while it is not known yet.
int32_t GetDisplayLevel(const CGUnit_C* unit, int32_t fallback);

// The HoT subclass record of a player, null when unknown.
const ChrClassesRec* GetSubClassRecord(const CGUnit_C* unit);

void SendRequest(WOWGUID guid);

// Pushes the scaled level for UnitLevel(token), false to fall through to the stock answer.
bool PushScaledLevel(lua_State* L);

Module* GetModule();

}

#endif
