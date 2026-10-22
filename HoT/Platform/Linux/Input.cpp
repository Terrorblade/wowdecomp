#include "HoT/Platform/Input.h"
#include "Engine/Source/Os/Gui.h"

namespace HoT::Platform {

// OsGuiIsModifierKeyDown takes 0 for control, 1 for shift and 2 for alt.
bool ModifierHeld(Modifier modifier) {
    int32_t key = modifier == Modifier::Shift ? 1 : (modifier == Modifier::Control ? 0 : 2);
    return OsGuiIsModifierKeyDown(key) != 0;
}

}
