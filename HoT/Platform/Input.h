#ifndef HOT_PLATFORM_INPUT_H
#define HOT_PLATFORM_INPUT_H

namespace HoT::Platform {

enum class Modifier {
    Shift,
    Control,
    Alt,
};

bool ModifierHeld(Modifier modifier);

}

#endif
