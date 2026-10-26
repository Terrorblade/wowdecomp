
#include <cstdint>
#include <cwchar>
#include <malloc.h>
#include <windows.h>
#include "Engine/Source/Os/Path.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00D41618
static CommandLineUTF8 s_commandLineUTF8;

// OFFSET: 0x0086BBD0
uint32_t GetModuleFileNameUTF8(void* module, char* path, uint32_t size) {
    uint32_t wideSize = size * 3 * sizeof(uint16_t);
    uint16_t* wide = static_cast<uint16_t*>(_alloca(wideSize));
    wide[0] = 0;
    GetModuleFileNameW(static_cast<HMODULE>(module), reinterpret_cast<LPWSTR>(wide), wideSize);

    uint32_t written;
    SUniConvertUTF16to8(path, size, wide, 0x7FFFFFFF, &written, nullptr);

    if (size - 1 >= written) {
        path[written] = '\0';
    } else {
        path[size - 1] = '\0';
    }

    return written;
}

// OFFSET: 0x0086BDB0
CommandLineUTF8::CommandLineUTF8() {
    const wchar_t* commandLine = GetCommandLineW();
    if (!commandLine) {
        commandLine = L"";
    }

    size_t length = wcslen(commandLine);
    if (length + 1 < length || length + 1 > SIZE_MAX / sizeof(uint32_t)) {
        commandLine = L"";
        length = 0;
    }

    uint32_t size = static_cast<uint32_t>((length + 1) * sizeof(uint32_t));
    m_commandLine = static_cast<char*>(SMemAlloc(size, __FILE__, __LINE__, 0));

    uint32_t written;
    SUniConvertUTF16to8(m_commandLine, size, reinterpret_cast<const uint16_t*>(commandLine), 0x7FFFFFFF, &written, nullptr);

    if (size - 1 >= written) {
        m_commandLine[written] = '\0';
    } else {
        m_commandLine[size - 1] = '\0';
    }
}

// INLINED
CommandLineUTF8::~CommandLineUTF8() {
    delete[] m_commandLine;
}

void OsGetGameDirectory(char* path, uint32_t size) {
    OsGetExePath(path, size);
}

