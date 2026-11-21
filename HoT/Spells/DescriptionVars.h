#ifndef HOT_SPELLS_DESCRIPTIONVARS_H
#define HOT_SPELLS_DESCRIPTIONVARS_H

#include <cstdint>

class CGPlayer_C;
class SpellRec;

namespace HoT::Spells {

// Resolves the HoT spell description variables and the unrounded ranged ones, false leaves the op to the stock switch.
bool GetDescriptionVariable(uint32_t op, CGPlayer_C* player, const SpellRec* spell, float& value);

// Level the parser renders at with no player, set by headless tools. 0 keeps the stock no-player behaviour.
void SetHeadlessLevel(int32_t level);
int32_t GetHeadlessLevel();

// The spell-only variables with no player, false while no headless level is set.
bool GetHeadlessVariable(uint32_t op, const SpellRec* spell, float& value);

}

#endif
