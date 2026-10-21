#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"
#include "Engine/Source/Storm/Source/SCmd.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Storm/H/storm.h"

static const char* const s_errorstr[3] = {
    "Invalid argument: %s",
    "The syntax of the command is incorrect.",
    "Unable to open response file: %s",
};

void GenerateError(void (STORMAPI* errorcallback)(_CMDERROR*), uint32_t errorcode, const char* itemstring) {
    uint32_t index;

    switch (errorcode) {
        case STORM_ERROR_BAD_ARGUMENT:
            index = 0;
            break;
        case STORM_ERROR_NOT_ENOUGH_ARGUMENTS:
            index = 1;
            break;
        case STORM_WIN_ERROR_OPEN_FAILED:
            index = 2;
            break;
        default:
            return;
    }

    // There are no string resources to override the built-in text on Linux.
    char buffer[256];
    SStrCopy(buffer, s_errorstr[index], sizeof(buffer));

    char errorstr[256];

    if (strstr(buffer, "%s")) {
        snprintf(errorstr, sizeof(errorstr), buffer, itemstring);
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

int32_t ProcessFile(const char* filename, _PROCESSING* processing, CMDDEF** nextarg, int32_t (STORMAPI* extracallback)(const char*), void (STORMAPI* errorcallback)(_CMDERROR*)) {
    char path[MAX_PATH * 4];
    FILE* file = nullptr;

    if (StormLinux::ResolvePath(filename, path, sizeof(path), false)) {
        file = fopen(path, "rb");
    }

    if (!file) {
        if (errorcallback) {
            GenerateError(errorcallback, STORM_WIN_ERROR_OPEN_FAILED, filename);
        }

        return 0;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint32_t size = length > 0 ? static_cast<uint32_t>(length) : 0;
    char* data = static_cast<char*>(SMemAlloc(size + 1, __FILE__, __LINE__, 0));
    size_t bytesread = fread(data, 1, size, file);
    fclose(file);
    data[bytesread] = 0;

    const char* curr = data;
    int32_t result = ProcessString(&curr, processing, nextarg, extracallback, errorcallback);
    SMemFree(data, __FILE__, __LINE__, 0);

    return result;
}

int32_t STORMAPI SCmdProcessCommandLine(int32_t (STORMAPI* extracallback)(const char*), void (STORMAPI* errorcallback)(_CMDERROR*)) {
    return SCmdProcess(OsGetCommandLine(), 1, extracallback, errorcallback);
}

#endif
