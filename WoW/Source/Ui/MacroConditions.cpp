#include "MacroConditions.h"
#include <cstring>
#include "WoW/Source/Ui/MacroSubstring.h"
#include "WoW/Source/Ui/UIMacros.h"

#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/GameUI.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Ui/SpellBookFrame.h"
#include "WoW/Source/Ui/UIBindings.h"

// STATIC: 0x00C2438C
TSHashTable<MacroConditions, HASHKEY_STR> s_macroConditions;

// STATIC: 0x00AD14D8
static const char* s_conditionNames[MacroCondition::TYPE_COUNT] = { "", "combat", "exists", "help", "harm", "party", "raid", "dead", "flyable", "indoors", "outdoors", "swimming", "flying", "mounted", "stealth", "group", "stance", "form", "pet", "modifier", "mod", "button", "btn", "actionbar", "bar", "bonusbar", "equipped", "worn", "channeling", "spec", "vehicleui", "unithasvehicleui", "cursor" };

// VERIFY: 92beef98 auto
// OFFSET: 0x008C8DE0
static int32_t EvaluateNone(const WOWGUID* unit, const TSGrowableArray<MacroSubstring>* args) {
    return 1;
}

// STATIC: 0x00AD1560
static MACROCONDITIONFUNC s_conditionFuncs[MacroCondition::TYPE_COUNT] = { EvaluateNone, EvaluateCombat, EvaluateExists, EvaluateHelp, EvaluateHarm, EvaluateParty, EvaluateRaid, EvaluateDead, EvaluateFlyable, EvaluateIndoors, EvaluateOutdoors, EvaluateSwimming, EvaluateFlying, EvaluateMounted, EvaluateStealth, EvaluateGroup, EvaluateStance, EvaluateStance, EvaluatePet, EvaluateModifier, EvaluateModifier, EvaluateButton, EvaluateButton, EvaluateActionBar, EvaluateActionBar, EvaluateBonusBar, EvaluateEquipped, EvaluateEquipped, EvaluateChanneling, EvaluateSpec, EvaluateVehicleUI, EvaluateUnitHasVehicleUI, EvaluateCursor };

// VERIFY: 7d0b0703 manual
// OFFSET: 0x005F0DF0
int32_t EvaluateMacroOptions(const char* start, const char* end, MacroSubstring* result, MacroSubstring* target) {
    if (!start) {
        return 1;
    }

    const char* cursor = start;
    const char* conditions = nullptr;
    const char* optionStart;
    int32_t matched = 0;

    while (true) {
        while (true) {
            optionStart = cursor;

            while (cursor < end && *cursor != '[' && *cursor != ';') {
                cursor++;
            }

            if (cursor >= end || *cursor == ';') {
                break;
            }

            for (conditions = cursor + 1; conditions < end && *conditions == ' '; conditions++) {
            }

            const char* close = conditions;

            while (close < end && *close != ']') {
                close++;
            }

            const char* last = close;

            while (last > conditions && last[-1] == ' ') {
                last--;
            }

            if (!matched) {
                uint32_t length = static_cast<uint32_t>(last - conditions);

                if (length > 1023) {
                    length = 1023;
                }

                char text[1024];
                memcpy(text, conditions, length);
                text[length] = '\0';
                MacroConditions* macroConditions = s_macroConditions.Ptr(text);

                if (!macroConditions) {
                    macroConditions = s_macroConditions.New(text, 0, 0);
                    macroConditions->Parse();
                }

                matched = macroConditions->Evaluate(target);
            }

            cursor = close + 1;
        }

        if (!conditions || matched) {
            break;
        }

        if (cursor >= end) {
            return 0;
        }

        conditions = nullptr;
        cursor++;
    }

    result->m_start = optionStart;

    while (result->m_start < cursor && *result->m_start == ' ') {
        result->m_start++;
    }

    result->m_end = cursor;

    while (result->m_end > result->m_start && result->m_end[-1] == ' ') {
        result->m_end--;
    }

    return 1;
}

// VERIFY: 77048a72 manual
// OFFSET: 0x005F0BB0
void MacroConditions::Parse() {
    const char* text = this->m_key.m_str;
    const char* end = text + SStrLen(text);

    if (text == end) {
        return;
    }

    uint32_t count = 0;

    for (int32_t pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            this->m_conditions.SetCount(count);
            count = 0;
        }

        const char* cursor = text;

        while (cursor < end) {
            while (cursor < end && *cursor == ' ') {
                cursor++;
            }

            const char* tokenEnd = cursor;

            while (tokenEnd < end && *tokenEnd != ',') {
                tokenEnd++;
            }

            const char* next = tokenEnd + 1;

            while (tokenEnd > cursor && tokenEnd[-1] == ' ') {
                tokenEnd--;
            }

            if (tokenEnd - cursor >= 7 && !SStrCmp(cursor, "target=", 7)) {
                if (pass == 1) {
                    this->m_target.m_start = cursor + 7;

                    while (this->m_target.m_start < tokenEnd && *this->m_target.m_start == ' ') {
                        this->m_target.m_start++;
                    }

                    this->m_target.m_end = tokenEnd;

                    while (this->m_target.m_end > this->m_target.m_start && this->m_target.m_end[-1] == ' ') {
                        this->m_target.m_end--;
                    }
                }
            } else if (*cursor == '@') {
                if (pass == 1) {
                    this->m_target.m_start = cursor + 1;

                    while (this->m_target.m_start < tokenEnd && *this->m_target.m_start == ' ') {
                        this->m_target.m_start++;
                    }

                    this->m_target.m_end = tokenEnd;

                    while (this->m_target.m_end > this->m_target.m_start && this->m_target.m_end[-1] == ' ') {
                        this->m_target.m_end--;
                    }
                }
            } else {
                if (pass == 1) {
                    this->m_conditions.m_data[count].Parse(cursor, tokenEnd);
                }

                count++;
            }

            cursor = next;
        }
    }
}

// VERIFY: 32005717 auto
// OFFSET: 0x005EF5C0
int32_t MacroConditions::Evaluate(MacroSubstring* target) {
    uint32_t count = this->m_conditions.m_count;

    if (count) {
        WOWGUID unit;
        char token[32];
        uint32_t length = static_cast<uint32_t>(this->m_target.m_end - this->m_target.m_start);

        if (length > 31) {
            length = 31;
        }

        memcpy(token, this->m_target.m_start, length);
        token[length] = '\0';
        Script_GetGUIDFromToken(token, unit, true);

        for (uint32_t i = 0; i < count; i++) {
            MacroCondition* condition = &this->m_conditions.m_data[i];
            int32_t result = s_conditionFuncs[condition->m_type](&unit, &condition->m_args);

            if (condition->m_negate) {
                result = !result;
            }

            if (!result) {
                return 0;
            }
        }
    }

    *target = this->m_target;
    return 1;
}

// VERIFY: c63507a8 manual
// OFFSET: 0x005F0040
void MacroCondition::Parse(const char* start, const char* end) {
    MacroSubstring option;
    option.m_start = nullptr;
    option.m_end = nullptr;
    const char* cursor = start;

    if (end - start <= 2 || SStrCmp(start, "no", 2)) {
        this->m_negate = 0;
    } else {
        cursor = start + 2;
        this->m_negate = 1;
    }

    option.SetText(cursor, end, ':', &cursor);
    int32_t type = 0;

    for (; type < MacroCondition::TYPE_COUNT; type++) {
        if (option == s_conditionNames[type]) {
            this->m_type = type;
            break;
        }
    }

    if (type == MacroCondition::TYPE_COUNT) {
        char text[1024];
        uint32_t length = static_cast<uint32_t>(option.m_end - option.m_start);

        if (length > 1023) {
            length = 1023;
        }

        memcpy(text, option.m_start, length);
        text[length] = '\0';
        CGGameUI::DisplayError(ERR_UNKNOWN_MACRO_OPTION_S, text);
        return;
    }

    if (cursor + 1 >= end) {
        return;
    }

    uint32_t count = 1;

    for (const char* c = cursor; c < end; c++) {
        if (*c == '/') {
            count++;
        }
    }

    this->m_args.SetCount(count);

    for (uint32_t i = 0; i < count; i++) {
        MacroSubstring* arg = &this->m_args.m_data[i];
        arg->m_start = cursor + 1;

        while (arg->m_start < end && *arg->m_start == ' ') {
            arg->m_start++;
        }

        arg->m_end = arg->m_start;

        while (arg->m_end < end && *arg->m_end != '/') {
            arg->m_end++;
        }

        cursor = arg->m_end;

        while (arg->m_end > arg->m_start && arg->m_end[-1] == ' ') {
            arg->m_end--;
        }
    }
}

// VERIFY: ecf7f26a manual
// OFFSET: 0x005EEFB0
int32_t EvaluateStance(const WOWGUID* unit, const TSGrowableArray<MacroSubstring>* args) {
    uint32_t form = CGSpellBook::GetShapeshiftForm(1);

    if (!args->m_count) {
        return form != 0;
    }

    for (uint32_t i = args->m_count; i; i--) {
        const MacroSubstring& arg = args->m_data[i - 1];
        uint32_t value = 0;

        for (const char* c = arg.m_start; c < arg.m_end && *c >= '0' && *c <= '9'; c++) {
            value = *c + 10 * value - '0';
        }

        if (value == form) {
            return 1;
        }
    }

    return 0;
}

// VERIFY: aa9e3f6d manual
// OFFSET: 0x005EF040
int32_t EvaluateModifier(const WOWGUID* unit, const TSGrowableArray<MacroSubstring>* args) {
    CGUIBindings* bindings = CGUIBindings::s_bindings;

    if (!args->m_count) {
        return bindings->IsModifiedClick(nullptr);
    }

    for (uint32_t i = args->m_count; i; i--) {
        const MacroSubstring& arg = args->m_data[i - 1];
        char action[1024];
        uint32_t length = static_cast<uint32_t>(arg.m_end - arg.m_start);

        if (length > 1023) {
            length = 1023;
        }

        memcpy(action, arg.m_start, length);
        action[length] = '\0';

        if (bindings->IsModifiedClick(action)) {
            return 1;
        }
    }

    return 0;
}
