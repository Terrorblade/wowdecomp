#include <cstdint>
#include <cstring>
#include <vector>
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include "Engine/Source/Base/ConvertUTF.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Mac/MacInput.h"
#include "Engine/Source/Os/OsIME.h"
#include "Engine/Source/Storm/H/storm.h"

// The client's IME interface on macOS. The game view's text input client reports composition (marked
// text) here; the client reads it back through the same calls and events as the Win32 IMM ones:
// OS_INPUT_IME 1 when composition starts, 2 with GCS-style flags when it changes, 6 when it ends.
// macOS draws its own candidate window, so no candidate list is handed to the client.

static uint32_t s_enableCount;
static std::vector<uint16_t> s_composition;
static std::vector<uint16_t> s_result;
static uint32_t s_selectionStart;
static uint32_t s_selectionLength;

// Composition flags of OS_INPUT_IME 2, after GCS_CURSORPOS, GCS_COMPSTR and GCS_RESULTSTR.
static const int32_t kChangedCursor = 0x1;
static const int32_t kChangedComposition = 0x2;
static const int32_t kChangedResult = 0x4;

static int32_t CopyUTF8(const std::vector<uint16_t>& text, char* buffer, uint32_t bufferSize) {
    if (!buffer || !bufferSize) {
        return 0;
    }

    memset(buffer, 0, bufferSize);
    uint32_t written = 0;

    if (!text.empty()) {
        ConvertUTF16toUTF8(buffer, bufferSize - 1, text.data(), static_cast<uint32_t>(text.size()), &written, nullptr);
    }

    buffer[written] = '\0';

    return 1;
}

bool OsMacIMEEnabled() {
    return s_enableCount > 0;
}

void OsMacIMEBeginComposition() {
    s_composition.clear();
    s_result.clear();
    OsQueuePut(OS_INPUT_IME, 1, 0, 0, 0);
}

void OsMacIMESetComposition(const uint16_t* text, uint32_t length, uint32_t selectionStart, uint32_t selectionLength) {
    s_composition.assign(text, text + length);
    s_selectionStart = selectionStart;
    s_selectionLength = selectionLength;
    OsQueuePut(OS_INPUT_IME, 2, 0, kChangedCursor | kChangedComposition, 0);
}

void OsMacIMECommit(const uint16_t* text, uint32_t length) {
    s_result.assign(text, text + length);
    s_composition.clear();
    s_selectionStart = 0;
    s_selectionLength = 0;
    OsQueuePut(OS_INPUT_IME, 2, 0, kChangedResult, 0);
}

void OsMacIMEEndComposition() {
    s_composition.clear();
    OsQueuePut(OS_INPUT_IME, 6, 0, 0, 0);
}

OS_IME_LANGUAGEMODE OsIMEGetLanguageMode() {
    // Native input of an input method: 1 Japanese, 2 Korean, 3 traditional Chinese, 0 otherwise
    // (simplified Chinese included), as the Windows version reads the keyboard layout.
    TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();

    if (!source) {
        return OS_IME_MODE_ROMAN;
    }

    OS_IME_LANGUAGEMODE mode = OS_IME_MODE_ROMAN;
    CFStringRef type = static_cast<CFStringRef>(TISGetInputSourceProperty(source, kTISPropertyInputSourceType));
    CFArrayRef languages = static_cast<CFArrayRef>(TISGetInputSourceProperty(source, kTISPropertyInputSourceLanguages));

    bool inputMethod = type && (CFEqual(type, kTISTypeKeyboardInputMode) || CFEqual(type, kTISTypeKeyboardInputMethodModeEnabled));

    if (inputMethod && languages && CFArrayGetCount(languages) > 0) {
        NSString* language = (__bridge NSString*)CFArrayGetValueAtIndex(languages, 0);

        if ([language hasPrefix:@"ja"]) {
            mode = OS_IME_MODE_JAPANESE;
        } else if ([language hasPrefix:@"ko"]) {
            mode = OS_IME_MODE_KOREAN;
        } else if ([language hasPrefix:@"zh-Hant"]) {
            mode = OS_IME_MODE_CHINESE;
        }
    }

    CFRelease(source);

    return mode;
}

int32_t OsIMEGetCompositionString(char* buffer, uint32_t bufferSize) {
    return CopyUTF8(s_composition, buffer, bufferSize);
}

int32_t OsIMEGetCompositionResult(char* buffer, uint32_t bufferSize) {
    return CopyUTF8(s_result, buffer, bufferSize);
}

int32_t OsIMEGetClauseInfo(uint32_t& clauseStart, uint32_t& clauseEnd, uint32_t& cursor) {
    if (s_composition.empty()) {
        return 0;
    }

    // The clause being converted is the selection inside the marked text.
    uint32_t length = static_cast<uint32_t>(s_composition.size());
    uint32_t start = s_selectionStart > length ? length : s_selectionStart;
    uint32_t end = start + s_selectionLength > length ? length : start + s_selectionLength;

    clauseStart = start;
    clauseEnd = end;
    cursor = end;

    return 1;
}

void OsIMEEnable(int32_t enable) {
    if (enable) {
        s_enableCount++;
    } else if (s_enableCount) {
        s_enableCount--;
    }

    if (!s_enableCount) {
        // Abandon an unfinished composition when the client stops taking input.
        [[NSTextInputContext currentInputContext] discardMarkedText];
    }
}

void OsIMESync() {
}

void OsIMESetLanguageMode(int32_t native) {
    // Input sources are chosen by the user on macOS; the client cannot switch an input method's mode.
}

void OsIMEInitialize() {
    s_enableCount = 0;
}

void OsIMEDestroy() {
    s_composition.clear();
    s_result.clear();
}

int32_t OsIMEGetCandidates(uint32_t listMask, uint32_t& pageStart, uint32_t& pageSize, uint32_t& count, uint32_t& selection, TSGrowableArray<OsIMECandidate>& candidates) {
    candidates.Clear();

    return 0;
}
