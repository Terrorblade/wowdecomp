#if defined(WOWDECOMP_PLATFORM_MAC)

#include <cstdio>
#include <CoreFoundation/CoreFoundation.h>
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"

namespace StormLinux {

// A bundle started from Finder has no terminal, so the box is a CoreFoundation alert. It works from any thread and
// without AppKit being up, which fatal errors during startup need.
int32_t ShowErrorDialog(const char* title, const char* text, ErrorDialogButtons buttons, int32_t defaultResult) {
    fprintf(stderr, "\n%s\n%s\n\n", title, text);
    fflush(stderr);

    CFStringRef defaultButton = nullptr;
    CFStringRef alternateButton = nullptr;
    CFStringRef otherButton = nullptr;
    int32_t results[3] = { defaultResult, defaultResult, defaultResult };

    switch (buttons) {
        case ERROR_DIALOG_OK:
            defaultButton = CFSTR("OK");
            results[0] = SERR_IDOK;
            break;
        case ERROR_DIALOG_OK_CANCEL:
            defaultButton = CFSTR("Cancel");
            alternateButton = CFSTR("OK");
            results[0] = SERR_IDCANCEL;
            results[1] = SERR_IDOK;
            break;
        case ERROR_DIALOG_YES_NO:
            defaultButton = CFSTR("No");
            alternateButton = CFSTR("Yes");
            results[0] = SERR_IDNO;
            results[1] = SERR_IDYES;
            break;
        case ERROR_DIALOG_YES_NO_CANCEL:
            defaultButton = CFSTR("No");
            alternateButton = CFSTR("Yes");
            otherButton = CFSTR("Cancel");
            results[0] = SERR_IDNO;
            results[1] = SERR_IDYES;
            results[2] = SERR_IDCANCEL;
            break;
    }

    CFStringRef cfTitle = CFStringCreateWithCString(kCFAllocatorDefault, title, kCFStringEncodingUTF8);
    CFStringRef cfText = CFStringCreateWithCString(kCFAllocatorDefault, text, kCFStringEncodingUTF8);

    if (!cfText) {
        cfText = CFStringCreateWithCString(kCFAllocatorDefault, text, kCFStringEncodingWindowsLatin1);
    }

    CFOptionFlags response = kCFUserNotificationDefaultResponse;
    SInt32 shown = CFUserNotificationDisplayAlert(0, kCFUserNotificationStopAlertLevel, nullptr, nullptr, nullptr, cfTitle ? cfTitle : CFSTR("Error"), cfText ? cfText : CFSTR(""), defaultButton, alternateButton, otherButton, &response);

    if (cfTitle) {
        CFRelease(cfTitle);
    }

    if (cfText) {
        CFRelease(cfText);
    }

    if (shown != 0) {
        return defaultResult;
    }

    switch (response & 0x3) {
        case kCFUserNotificationAlternateResponse:
            return results[1];
        case kCFUserNotificationOtherResponse:
            return results[2];
        default:
            return results[0];
    }
}

}

#endif
