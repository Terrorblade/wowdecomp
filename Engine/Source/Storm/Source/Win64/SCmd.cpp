
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include "Engine/Source/Storm/Source/SCmd.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Storm/H/storm.h"

#define SCMD_STRING_INVALID_ARGUMENT 0x5201
#define SCMD_STRING_SYNTAX_INCORRECT 0x5202
#define SCMD_STRING_RESPONSE_FILE 0x5203

// STATIC: 0x00A39F84
static const char* const s_errorstr[3] = {
    "Invalid argument: %s",
    "The syntax of the command is incorrect.",
    "Unable to open response file: %s",
};

// OFFSET: 0x00772C90
void GenerateError(void (STORMAPI* errorcallback)(_CMDERROR*), uint32_t errorcode, const char* itemstring) {
    uint32_t index;
    UINT stringid;

    switch (errorcode) {
        case STORM_ERROR_BAD_ARGUMENT:
            index = 0;
            stringid = SCMD_STRING_INVALID_ARGUMENT;
            break;
        case STORM_ERROR_NOT_ENOUGH_ARGUMENTS:
            index = 1;
            stringid = SCMD_STRING_SYNTAX_INCORRECT;
            break;
        case ERROR_OPEN_FAILED:
            index = 2;
            stringid = SCMD_STRING_RESPONSE_FILE;
            break;
        default:
            return;
    }

    char buffer[256];
    buffer[0] = 0;
    LoadStringA(StormGetInstance(), stringid, buffer, sizeof(buffer));

    if (!buffer[0]) {
        SStrCopy(buffer, s_errorstr[index], sizeof(buffer));
    }

    char errorstr[256];

    if (strstr(buffer, "%s")) {
        wsprintfA(errorstr, buffer, itemstring);
    } else {
        SStrCopy(errorstr, buffer, sizeof(errorstr));
    }

    if (errorstr[0]) {
        SStrPack(errorstr, "\n", sizeof(errorstr));
    }

    SErrSetLastError(errorcode);

    _CMDERROR data;
    data.errorcode = errorcode;
    data.itemstr = itemstring;
    data.errorstr = errorstr;
    errorcallback(&data);
}

// OFFSET: 0x00772DA0
int32_t ProcessFile(const char* filename, _PROCESSING* processing, CMDDEF** nextarg, int32_t (STORMAPI* extracallback)(const char*), void (STORMAPI* errorcallback)(_CMDERROR*)) {
    HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        if (errorcallback) {
            GenerateError(errorcallback, ERROR_OPEN_FAILED, filename);
        }

        return 0;
    }

    DWORD size = GetFileSize(file, nullptr);
    char* data = static_cast<char*>(SMemAlloc(size + 1, __FILE__, __LINE__, 0));
    DWORD bytesread;
    ReadFile(file, data, size, &bytesread, nullptr);
    CloseHandle(file);
    data[bytesread] = 0;

    const char* curr = data;
    int32_t result = ProcessString(&curr, processing, nextarg, extracallback, errorcallback);
    SMemFree(data, __FILE__, __LINE__, 0);

    return result;
}

// OFFSET: 0x00773990
int32_t STORMAPI SCmdProcessCommandLine(int32_t (STORMAPI* extracallback)(const char*), void (STORMAPI* errorcallback)(_CMDERROR*)) {
    return SCmdProcess(GetCommandLineA(), 1, extracallback, errorcallback);
}

