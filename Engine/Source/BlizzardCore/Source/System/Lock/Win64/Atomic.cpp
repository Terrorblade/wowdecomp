
#include <cstdint>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"

// OFFSET: 0x00428300
int32_t Blizzard::Lock::Atomic::Increment() {
    return InterlockedIncrement(reinterpret_cast<volatile LONG*>(&this->m_value));
}

// OFFSET: 0x00428310
int32_t Blizzard::Lock::Atomic::Decrement() {
    return InterlockedDecrement(reinterpret_cast<volatile LONG*>(&this->m_value));
}

