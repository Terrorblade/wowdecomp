#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <cstring>
#include <SDL.h>
#include "Engine/Source/Os/OsClipboard.h"
#include "Engine/Source/Storm/H/storm.h"

// The SDL clipboard is per display connection rather than per window, and is UTF-8 on both ends, which is the
// encoding these functions take and return (the Windows versions convert from/to the ANSI code page).

char* IOsClipboardGetString(void* window) {
    if (!SDL_HasClipboardText()) {
        return nullptr;
    }

    char* text = SDL_GetClipboardText();
    if (!text) {
        return nullptr;
    }

    uint32_t size = static_cast<uint32_t>(strlen(text) + 1);
    char* string = static_cast<char*>(SMemAlloc(size, __FILE__, __LINE__, 0));
    memcpy(string, text, size);

    SDL_free(text);
    return string;
}

void OsClipboardFreeString(char* string) {
    if (string) {
        SMemFree(string, __FILE__, __LINE__, 0);
    }
}

int32_t IOsClipboardPutString(const char* string, void* window) {
    return SDL_SetClipboardText(string ? string : "") == 0 ? 1 : 0;
}

#endif
