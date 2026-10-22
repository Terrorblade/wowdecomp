#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include "Engine/Source/Storm/Source/SMsg.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Storm/H/storm.h"

static TSList<WNDREC> s_wndlist;

static RECT s_defaultwindowrect;

static HWND s_defaultwindow;

static WNDREC* FindWindow(HWND window) {
    for (WNDREC* rec = s_wndlist.Head(); !TSList<WNDREC>::IsEnd(rec); rec = s_wndlist.RawNext(rec)) {
        if (rec->window == window) {
            return rec;
        }
    }

    return nullptr;
}

HWND STORMAPI SMsgGetDefaultWindow() {
    return s_defaultwindow;
}

static void DeleteWindow(HWND window) {
    if (window == s_defaultwindow) {
        s_defaultwindow = nullptr;
        s_defaultwindowrect.left = 0;
        s_defaultwindowrect.top = 0;
        s_defaultwindowrect.right = 0;
        s_defaultwindowrect.bottom = 0;
    }

    WNDREC* rec = FindWindow(window);

    if (rec) {
        SEvtUnregisterType('SMSG', reinterpret_cast<uintptr_t>(window));
        SEvtUnregisterType('SMSH', reinterpret_cast<uintptr_t>(window));
        SEvtUnregisterType('SMSI', reinterpret_cast<uintptr_t>(window));
        SEvtUnregisterType('SMSK', reinterpret_cast<uintptr_t>(window));
        SEvtUnregisterType('SMSJ', reinterpret_cast<uintptr_t>(window));
        s_wndlist.DeleteNode(rec);
    }
}

int32_t STORMAPI SMsgDestroy() {
    WNDREC* rec;

    while ((rec = s_wndlist.Head()) != nullptr) {
        DeleteWindow(rec->window);
    }

    return 1;
}

// OutputDebugStringA has no Linux counterpart; stderr is where a developer would look.
void SDebugPrintf(const char* format, ...) {
    char buffer[4096];
    va_list arglist;
    va_start(arglist, format);
    vsnprintf(buffer, sizeof(buffer), format, arglist);
    va_end(arglist);
    fputs(buffer, stderr);
}

#endif
