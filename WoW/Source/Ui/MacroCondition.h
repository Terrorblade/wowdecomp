#ifndef WOW_SOURCE_UI_MACROCONDITION_H
#define WOW_SOURCE_UI_MACROCONDITION_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Ui/MacroSubstring.h"

class MacroCondition {
    public:

    enum TYPE : int32_t {
        EMPTY_NAME = 0,
        COMBAT = 1,
        EXISTS = 2,
        HELP = 3,
        HARM = 4,
        PARTY = 5,
        RAID = 6,
        DEAD = 7,
        FLYABLE = 8,
        INDOORS = 9,
        OUTDOORS = 10,
        SWIMMING = 11,
        FLYING = 12,
        MOUNTED = 13,
        STEALTH = 14,
        GROUP = 15,
        STANCE = 16,
        FORM = 17,
        PET = 18,
        MODIFIER = 19,
        MOD = 20,
        BUTTON = 21,
        BTN = 22,
        ACTIONBAR = 23,
        BAR = 24,
        BONUSBAR = 25,
        EQUIPPED = 26,
        WORN = 27,
        CHANNELING = 28,
        SPEC = 29,
        VEHICLEUI = 30,
        UNIT_HAS_VEHICLE_UI = 31,
        CURSOR = 32,
        TYPE_COUNT = 33,
    };

    /* 0000 */ int32_t m_type;
    /* 0004 */ TSGrowableArray<MacroSubstring> m_args;
    /* 0014 */ int32_t m_negate;

    MacroCondition();
    void Parse(const char* start, const char* end);
};

// INLINED
inline MacroCondition::MacroCondition() {
    this->m_type = EMPTY_NAME;
    this->m_negate = 0;
}

#endif
