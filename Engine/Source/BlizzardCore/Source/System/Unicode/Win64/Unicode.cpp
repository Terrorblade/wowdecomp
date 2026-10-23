
#include <cstdint>
#include <cstring>
#include <windows.h>
#include "Engine/Source/BlizzardCore/Source/System/Unicode/Unicode.h"

// OFFSET: 0x00433650
Blizzard::StringConversion_CodepageToUTF8::ToNative::ToNative(const char* str) : Blizzard::Memory::TempBuffer<char, 300>((str ? static_cast<uint32_t>(strlen(str)) : 0) + 1) {
    Blizzard::Memory::TempBuffer<uint16_t, 300> wide(this->m_count);
    uint32_t length;
    Blizzard::Unicode::ConvertUTF8to16(wide.m_data, wide.m_count, str, -1, &length, nullptr);
    this->m_data[WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWCH>(wide.m_data), length, this->m_data, this->m_count, "_", nullptr)] = '\0';
}

