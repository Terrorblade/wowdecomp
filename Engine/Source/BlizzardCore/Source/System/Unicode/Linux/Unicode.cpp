#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <cstring>
#include "Engine/Source/BlizzardCore/Source/System/Unicode/Unicode.h"

// Windows converts UTF-8 to the ANSI code page here. The native encoding on Linux is UTF-8, so the string is copied.
Blizzard::StringConversion_CodepageToUTF8::ToNative::ToNative(const char* str) : Blizzard::Memory::TempBuffer<char, 300>((str ? static_cast<uint32_t>(strlen(str)) : 0) + 1) {
    uint32_t length = str ? static_cast<uint32_t>(strlen(str)) : 0;

    if (length) {
        memcpy(this->m_data, str, length);
    }

    this->m_data[length] = '\0';
}

#endif
