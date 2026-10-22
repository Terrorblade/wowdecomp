#include "HoT/Platform/Input.h"
#include "Engine/Source/Os/Gui.h"

namespace HoT::Platform {

// OsGuiIsModifierKeyDown reads the SDL modifier state, 0 is control, 1 is shift and 2 is alt.
bool ModifierHeld(Modifier modifier) {
    int32_t key = modifier == Modifier::Shift ? 1 : (modifier == Modifier::Control ? 0 : 2);
    return OsGuiIsModifierKeyDown(key) != 0;
}

}
