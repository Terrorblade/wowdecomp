
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <windows.h>
#include "Engine/Source/Storm/Source/SOption.h"
#include "Engine/Source/Storm/H/storm.h"

// INLINED: [0x007704BE->0x007704E7, 0x00770595->0x007705BE]
int32_t STORMAPI SRegGetBaseKey(uint32_t flags, char* buffer, uint32_t bufferchars) {
    if (flags & SREG_BATTLE_NET) {
        SStrCopy(buffer, "Software\\Battle.net\\", bufferchars);
    } else {
        SStrCopy(buffer, "Software\\Blizzard Entertainment\\", bufferchars);
    }

    return 1;
}

// INLINED: [0x007704A0->0x007704FC, 0x00770589->0x007705D4]
static void BuildFullKeyName(const char* keyname, uint32_t flags, char* buffer, uint32_t bufferchars) {
    *buffer = 0;

    if (!(flags & SREG_ABSOLUTE)) {
        SRegGetBaseKey(flags, buffer, bufferchars);
    }

    SStrPack(buffer, keyname, bufferchars);
}

// OFFSET: 0x007703A0
static LSTATUS ILoadValue(HKEY parentkey, const char* subkeyname, const char* valuename, DWORD* datatype, void* buffer, DWORD bytes, DWORD* bytesread) {
    HKEY key;

    if (!g_opt.unicodeenabled) {
        LSTATUS result = RegOpenKeyExA(parentkey, subkeyname, 0, KEY_READ, &key);

        if (result) {
            return result;
        }

        *bytesread = bytes;
        result = RegQueryValueExA(key, valuename, nullptr, datatype, static_cast<BYTE*>(buffer), bytesread);
        RegCloseKey(key);

        return result;
    }

    WCHAR widename[MAX_PATH];
    SUniConvertUTF8to16(reinterpret_cast<uint16_t*>(widename), MAX_PATH, subkeyname, STORM_MAX_STR, nullptr, nullptr);

    LSTATUS result = RegOpenKeyExW(parentkey, widename, 0, KEY_READ, &key);

    if (result) {
        return result;
    }

    *bytesread = bytes;
    SUniConvertUTF8to16(reinterpret_cast<uint16_t*>(widename), MAX_PATH, valuename, STORM_MAX_STR, nullptr, nullptr);
    result = RegQueryValueExW(key, widename, nullptr, datatype, static_cast<BYTE*>(buffer), bytesread);
    RegCloseKey(key);

    return result;
}

// OFFSET: 0x00770490
static int32_t InternalLoadEntry(const char* keyname, const char* valuename, uint32_t flags, DWORD* datatype, void* buffer, DWORD bytes, DWORD* bytesread) {
    *bytesread = 0;
    *datatype = REG_DWORD;

    LSTATUS result = ERROR_CANTOPEN;
    char fullkeyname[MAX_PATH];
    BuildFullKeyName(keyname, flags, fullkeyname, MAX_PATH);

    if (!(flags & SREG_EXCLUDE_CURRENT_USER)) {
        result = ILoadValue(HKEY_CURRENT_USER, fullkeyname, valuename, datatype, buffer, bytes, bytesread);

        if (!result) {
            return 1;
        }
    }

    if (!(flags & SREG_EXCLUDE_LOCAL_MACHINE)) {
        result = ILoadValue(HKEY_LOCAL_MACHINE, fullkeyname, valuename, datatype, buffer, bytes, bytesread);
    }

    if (!result) {
        return 1;
    }

    SetLastError(result);

    return 0;
}

// OFFSET: 0x00770580
static int32_t InternalSaveEntry(const char* keyname, const char* valuename, uint32_t flags, DWORD datatype, const void* buffer, DWORD bytes) {
    char fullkeyname[MAX_PATH];
    BuildFullKeyName(keyname, flags, fullkeyname, MAX_PATH);

    WCHAR widename[MAX_PATH];
    HKEY key;
    DWORD disposition;
    LSTATUS result;

    if (!g_opt.unicodeenabled) {
        result = RegCreateKeyExA(flags & SREG_EXCLUDE_CURRENT_USER ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, fullkeyname, 0, nullptr, 0, KEY_WRITE, nullptr, &key, &disposition);
    } else {
        SUniConvertUTF8to16(reinterpret_cast<uint16_t*>(widename), MAX_PATH, fullkeyname, STORM_MAX_STR, nullptr, nullptr);
        result = RegCreateKeyExW(flags & SREG_EXCLUDE_CURRENT_USER ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, widename, 0, nullptr, 0, KEY_WRITE, nullptr, &key, &disposition);
    }

    if (!result) {
        if (!g_opt.unicodeenabled) {
            result = RegSetValueExA(key, valuename, 0, datatype, static_cast<const BYTE*>(buffer), bytes);
        } else {
            SUniConvertUTF8to16(reinterpret_cast<uint16_t*>(widename), MAX_PATH, valuename, STORM_MAX_STR, nullptr, nullptr);
            result = RegSetValueExW(key, widename, 0, datatype, static_cast<const BYTE*>(buffer), bytes);
        }

        if (!result && (flags & SREG_FLUSH_KEY)) {
            result = RegFlushKey(key);
        }

        if (result) {
            RegCloseKey(key);
        } else {
            result = RegCloseKey(key);

            if (!result) {
                return 1;
            }
        }
    }

    SetLastError(result);

    return 0;
}

// OFFSET: 0x00770720
int32_t STORMAPI SRegLoadString(const char* keyname, const char* valuename, uint32_t flags, char* buffer, uint32_t buffersize) {
    if (!keyname || !*keyname || !valuename || !*valuename || !buffer || !buffersize) {
        SErrSetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    DWORD datatype;
    DWORD bytesread;

    if (!g_opt.unicodeenabled) {
        if (!InternalLoadEntry(keyname, valuename, flags, &datatype, buffer, buffersize, &bytesread)) {
            return 0;
        }

        switch (datatype) {
            case REG_SZ:
                buffer[buffersize - 1 < bytesread ? buffersize - 1 : bytesread] = 0;
                break;
            case REG_DWORD:
                SStrPrintf(buffer, buffersize, "%u", *reinterpret_cast<uint32_t*>(buffer));
                break;
        }

        return 1;
    }

    WCHAR widebuffer[4096];

    if (!InternalLoadEntry(keyname, valuename, flags, &datatype, widebuffer, 4096, &bytesread)) {
        return 0;
    }

    switch (datatype) {
        case REG_SZ: {
            uint32_t dstchars;
            SUniConvertUTF16to8(buffer, buffersize, reinterpret_cast<uint16_t*>(widebuffer), bytesread >> 1, &dstchars, nullptr);
            buffer[buffersize - 1 < dstchars ? buffersize - 1 : dstchars] = 0;
            break;
        }
        case REG_DWORD:
            SStrPrintf(buffer, buffersize, "%u", *reinterpret_cast<uint32_t*>(widebuffer));
            break;
    }

    return 1;
}

// OFFSET: 0x00770840
int32_t STORMAPI SRegLoadValue(const char* keyname, const char* valuename, uint32_t flags, uint32_t* value) {
    if (!keyname || !*keyname || !valuename || !*valuename || !value) {
        SErrSetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    char buffer[256];
    buffer[0] = 0;
    DWORD datatype;
    DWORD bytesread;

    if (!InternalLoadEntry(keyname, valuename, flags, &datatype, buffer, sizeof(buffer), &bytesread)) {
        return 0;
    }

    switch (datatype) {
        case REG_SZ:
            *value = strtoul(buffer, nullptr, 0);
            break;
        case REG_DWORD:
            *value = *reinterpret_cast<uint32_t*>(buffer);
            break;
    }

    return 1;
}

// OFFSET: 0x007708F0
int32_t STORMAPI SRegSaveString(const char* keyname, const char* valuename, uint32_t flags, const char* string) {
    if (!keyname || !*keyname || !valuename || !*valuename || !string) {
        SErrSetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    WCHAR widestring[MAX_PATH];
    const void* data;
    DWORD bytes;

    if (!g_opt.unicodeenabled) {
        bytes = static_cast<DWORD>(SStrLen(string) + 1);
        data = string;
    } else {
        SUniConvertUTF8to16(reinterpret_cast<uint16_t*>(widestring), MAX_PATH, string, STORM_MAX_STR, nullptr, nullptr);
        bytes = static_cast<DWORD>((wcslen(widestring) + 1) * sizeof(WCHAR));
        data = widestring;
    }

    return InternalSaveEntry(keyname, valuename, flags, REG_SZ, data, bytes);
}

// OFFSET: 0x007709A0
int32_t STORMAPI SRegSaveValue(const char* keyname, const char* valuename, uint32_t flags, uint32_t value) {
    if (!keyname || !*keyname || !valuename || !*valuename) {
        SErrSetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    return InternalSaveEntry(keyname, valuename, flags, REG_DWORD, &value, sizeof(value));
}

