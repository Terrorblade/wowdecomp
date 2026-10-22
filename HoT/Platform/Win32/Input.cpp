#include "HoT/Platform/Input.h"
#include <windows.h>

namespace HoT::Platform {

bool ModifierHeld(Modifier modifier) {
    int key = modifier == Modifier::Shift ? VK_SHIFT : (modifier == Modifier::Control ? VK_CONTROL : VK_MENU);
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

}
