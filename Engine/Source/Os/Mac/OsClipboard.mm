#include <cstdint>
#include <cstring>
#import <AppKit/AppKit.h>
#include "Engine/Source/Os/OsClipboard.h"
#include "Engine/Source/Storm/H/storm.h"

// Text on the general pasteboard, UTF-8 in and out like the client's strings.

char* IOsClipboardGetString(void* window) {
    @autoreleasepool {
        NSString* text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];

        if (!text) {
            return nullptr;
        }

        const char* utf8 = text.UTF8String;
        uint32_t size = static_cast<uint32_t>(strlen(utf8) + 1);
        char* string = static_cast<char*>(SMemAlloc(size, __FILE__, __LINE__, 0));
        memcpy(string, utf8, size);

        return string;
    }
}

void OsClipboardFreeString(char* string) {
    if (string) {
        SMemFree(string, __FILE__, __LINE__, 0);
    }
}

int32_t IOsClipboardPutString(const char* string, void* window) {
    @autoreleasepool {
        NSString* text = string ? [NSString stringWithUTF8String:string] : nil;

        if (!text) {
            return 0;
        }

        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];

        return [pasteboard setString:text forType:NSPasteboardTypeString] ? 1 : 0;
    }
}
