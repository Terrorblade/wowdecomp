#include "WoW/Source/Ui/MODIFIEDCLICK.h"
#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/UIBindings.h"

// VERIFY: dfca5dad manual
// OFFSET: 0x0055D6B0
int32_t MODIFIEDCLICK::IsActive(BINDING_SET set) {
    const char* button = this->m_data[set].m_button;
    uint32_t modifiers = this->m_data[set].m_modifiers;

    if (modifiers) {
        int32_t key = KEY_LSHIFT;

        while (!((1 << key) & modifiers) || !CGGameUI::IsKeyDown(static_cast<KEY>(key))) {
            if (++key > KEY_RALT) {
                return 0;
            }
        }

        if (!button) {
            return 1;
        }
    } else if (!button) {
        return 0;
    }

    const char* current = CSimpleTop::s_instance->m_mouseButtonName;

    return !current || !SStrCmp(current, button, STORM_MAX_STR);
}

// VERIFY: 19f20e5a manual
// OFFSET: 0x0055E180
int32_t MODIFIEDCLICK::SetBinding(BINDING_SET set, const char* binding) {
    const char* button = nullptr;
    uint32_t modifiers = 0;

    if (binding && *binding && SStrCmpI(binding, "NONE", STORM_MAX_STR)) {
        CGUIBindings::AddModifiers(modifiers, binding);

        if (!SStrCmpI(binding, "BUTTON", 6)) {
            binding += 6;
            button = GetMouseButtonName(IndexToMouseButton(SStrToInt(binding)));

            if (*binding) {
                binding++;
            }
        }

        if (*binding) {
            return 0;
        }
    }

    if (set) {
        this->m_data[set].m_flags &= ~MODIFIEDCLICK_DATA::DEFAULT;
    } else {
        this->m_data[set].m_flags |= MODIFIEDCLICK_DATA::DEFAULT;
    }

    this->m_data[set].m_button = button;
    this->m_data[set].m_modifiers = modifiers;

    return 1;
}

// VERIFY: 1da97044 manual
// OFFSET: 0x0055E230
void MODIFIEDCLICK::GetBinding(BINDING_SET set, char* binding, int32_t maxLength, uint32_t* flags) {
    uint32_t modifiers = this->m_data[set].m_modifiers;
    const char* button = this->m_data[set].m_button;

    if (!modifiers && !button) {
        SStrCopy(binding, "NONE", maxLength);
    } else {
        if (modifiers) {
            CGUIBindings::AddMetaPrefix(modifiers, binding, maxLength);
        }

        if (button) {
            SStrPrintf(binding, maxLength, "BUTTON%d", MouseButtonToIndex(StringToMouseButton(button)));
        } else {
            binding[-1] = '\0';
        }
    }

    if (flags) {
        *flags = this->m_data[set].m_flags;
    }
}
