#include <cstdint>
#import <AppKit/AppKit.h>
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/Os/Mac/MacOs.h"
#include "Engine/Source/Storm/H/storm.h"

// The game window handle is the NSWindow the graphics device created (see Os/Mac/MacOs.h). The
// Win32 dialog framework (COsDialog and its controls) belongs to Windows tools the client does not
// use; only the window and message box functions exist here.

static void* s_GxDevWindow;

void OsGuiSetWindowTitle(void* window, const char* title) {
    NSWindow* nsWindow = (__bridge NSWindow*)window;

    if (nsWindow && title) {
        nsWindow.title = [NSString stringWithUTF8String:title] ?: @"";
    }
}

void* OsGuiGetWindow(int32_t type) {
    switch (type) {
        case 0:
            return s_GxDevWindow;
        case 1:
            return (__bridge void*)[NSApp keyWindow];
        case 2:
            return NSApp.active ? (__bridge void*)[NSApp mainWindow] : nullptr;
        default:
            return nullptr;
    }
}

void OsGuiSetGxWindow(void* window) {
    s_GxDevWindow = window;
}

int32_t OsGuiMessageBox(void* parentWindow, int32_t style, const char* message, const char* title) {
    // Styles 0 OK, 1 OK/Cancel, 2 Yes/No, 3 Yes/No/Cancel; returns 0 for OK or Yes, 1 for No, 2 for
    // Cancel, as the Windows version maps MessageBox results.
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        alert.messageText = title && *title ? [NSString stringWithUTF8String:title] : @"";
        alert.informativeText = message ? [NSString stringWithUTF8String:message] ?: @"" : @"";

        switch (style) {
            case 1:
                [alert addButtonWithTitle:@"OK"];
                [alert addButtonWithTitle:@"Cancel"];
                break;
            case 2:
                [alert addButtonWithTitle:@"Yes"];
                [alert addButtonWithTitle:@"No"];
                break;
            case 3:
                [alert addButtonWithTitle:@"Yes"];
                [alert addButtonWithTitle:@"No"];
                [alert addButtonWithTitle:@"Cancel"];
                break;
            default:
                [alert addButtonWithTitle:@"OK"];
                break;
        }

        NSModalResponse response = [alert runModal];

        switch (response) {
            case NSAlertFirstButtonReturn:
                return 0;
            case NSAlertSecondButtonReturn:
                return style == 1 ? 2 : 1;
            default:
                return 2;
        }
    }
}

int32_t OsGuiIsModifierKeyDown(int32_t key) {
    NSEventModifierFlags flags = [NSEvent modifierFlags];

    switch (key) {
        case 0:
            return (flags & (NSEventModifierFlagControl | NSEventModifierFlagCommand)) != 0;
        case 1:
            return (flags & NSEventModifierFlagShift) != 0;
        case 2:
            return (flags & NSEventModifierFlagOption) != 0;
        default:
            return 0;
    }
}

void OsGuiBeep() {
    NSBeep();
}

void OsMacRequestAttention() {
    // AppKit ignores the request while the application is active, so no check is needed for that.
    // NSInformationalRequest bounces the Dock icon once, as a flashed Windows taskbar button stops
    // once the window is activated.
    void (^request)() = ^{
        [NSApp requestUserAttention:NSInformationalRequest];
    };

    if ([NSThread isMainThread]) {
        request();
    } else {
        dispatch_async(dispatch_get_main_queue(), request);
    }
}

int32_t OsGuiProcessMessage(void* message) {
    return 0;
}
