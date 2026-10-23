#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <cstring>
#include <SDL.h>
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Linux/OsIMESdl.h"
#include "Engine/Source/Os/OsIME.h"
#include "Engine/Source/Storm/H/storm.h"

// The IME lives outside the client on Linux (IBus, fcitx, ...) and SDL only reports its preedit text (TEXTEDITING),
// the cursor/selection inside it, and the committed text (TEXTINPUT). There is no conversion mode or candidate list
// API, so those functions report "not available" like the Windows ones do without an input context; the IME shows
// its own candidate window. Text input stays on all the time because OS_INPUT_CHAR also comes from TEXTINPUT; while
// no edit box has enabled the IME, composition is not reported and committed text arrives as characters.

static uint32_t s_enableCount;

static int32_t s_composing;

static char s_composition[1024];

static char s_compositionResult[1024];

static int32_t s_compositionCursor;

static int32_t s_compositionLength;

static void CopyString(char* buffer, uint32_t bufferSize, const char* source) {
    if (!buffer || !bufferSize) {
        return;
    }

    memset(buffer, 0, bufferSize);
    SStrCopy(buffer, source, bufferSize);
}

static void ResetComposition() {
    s_composing = 0;
    s_composition[0] = '\0';
    s_compositionCursor = 0;
    s_compositionLength = 0;
}

static void UpdateComposition(const char* text, int32_t start, int32_t length) {
    if (!text || !*text) {
        if (s_composing) {
            s_composition[0] = '\0';
            s_compositionCursor = 0;
            s_compositionLength = 0;
            OsQueuePut(OS_INPUT_IME, 2, 0, 2, 0);

            s_composing = 0;
            OsQueuePut(OS_INPUT_IME, 6, 0, 0, 0);
        }

        return;
    }

    if (!s_composing) {
        s_composing = 1;
        OsQueuePut(OS_INPUT_IME, 1, 0, 0, 0);
    }

    CopyString(s_composition, sizeof(s_composition), text);
    s_compositionCursor = start;
    s_compositionLength = length;

    OsQueuePut(OS_INPUT_IME, 2, 0, 1 | 2, 0);
}

int32_t OsIMEProcessSdlEvent(const SDL_Event* event) {
    switch (event->type) {
        case SDL_TEXTEDITING:
            if (s_enableCount) {
                UpdateComposition(event->edit.text, event->edit.start, event->edit.length);
            }

            return 1;

        case SDL_TEXTEDITING_EXT:
            if (s_enableCount) {
                UpdateComposition(event->editExt.text, event->editExt.start, event->editExt.length);
            }

            return 1;

        case SDL_TEXTINPUT:
            if (!s_composing) {
                return 0;
            }

            // A commit while composing is the GCS_RESULTSTR composition update followed by WM_IME_ENDCOMPOSITION.
            CopyString(s_compositionResult, sizeof(s_compositionResult), event->text.text);
            s_composition[0] = '\0';
            s_compositionCursor = 0;
            s_compositionLength = 0;
            OsQueuePut(OS_INPUT_IME, 2, 0, 4 | 2, 0);

            s_composing = 0;
            OsQueuePut(OS_INPUT_IME, 6, 0, 0, 0);
            return 1;

        default:
            return 0;
    }
}

OS_IME_LANGUAGEMODE OsIMEGetLanguageMode() {
    // No way to query the conversion mode of an external IME.
    return OS_IME_MODE_ROMAN;
}

int32_t OsIMEGetCompositionString(char* buffer, uint32_t bufferSize) {
    CopyString(buffer, bufferSize, "");

    if (!s_enableCount) {
        return 0;
    }

    CopyString(buffer, bufferSize, s_composition);
    return 1;
}

int32_t OsIMEGetCompositionResult(char* buffer, uint32_t bufferSize) {
    CopyString(buffer, bufferSize, "");

    if (!s_enableCount) {
        return 0;
    }

    CopyString(buffer, bufferSize, s_compositionResult);
    return 1;
}

int32_t OsIMEGetClauseInfo(uint32_t& clauseStart, uint32_t& clauseEnd, uint32_t& cursor) {
    // SDL reports one target segment (start/length, in characters) rather than a clause list. Like Windows with a
    // single clause, report no clause info when there is no segment.
    if (!s_enableCount || !s_composing || s_compositionLength <= 0) {
        return 0;
    }

    clauseStart = s_compositionCursor;
    clauseEnd = s_compositionCursor + s_compositionLength;
    cursor = s_compositionCursor;

    return 1;
}

void OsIMEEnable(int32_t enable) {
    if (enable) {
        s_enableCount++;
    } else if (s_enableCount) {
        if (--s_enableCount == 0) {
            ResetComposition();
        }
    }
}

void OsIMESync() {
    if (!SDL_IsTextInputActive()) {
        SDL_StartTextInput();
    }
}

void OsIMESetLanguageMode(int32_t native) {
    // The conversion mode belongs to the external IME.
}

void OsIMEInitialize() {
    ResetComposition();
    s_compositionResult[0] = '\0';

    SDL_SetHint(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, "1");
    SDL_StartTextInput();
}

void OsIMEDestroy() {
    ResetComposition();
    s_compositionResult[0] = '\0';
}

int32_t OsIMEGetCandidates(uint32_t listMask, uint32_t& pageStart, uint32_t& pageSize, uint32_t& count, uint32_t& selection, TSGrowableArray<OsIMECandidate>& candidates) {
    // SDL2 exposes no candidate list; the IME draws its own.
    candidates.Clear();
    return 0;
}

#endif
