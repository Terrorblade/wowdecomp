#include "HoT/Core/Shell.h"
#include "Engine/Source/Os/Gui.h"
#include <windows.h>
#include <shellapi.h>

namespace HoT::Shell {

void FlashGameWindow() {
    HWND window = static_cast<HWND>(OsGuiGetWindow(0));

    if (!window || GetForegroundWindow() == window) {
        return;
    }

    FLASHWINFO info = {};
    info.cbSize = sizeof(info);
    info.hwnd = window;
    info.dwFlags = FLASHW_TIMERNOFG | FLASHW_TRAY;
    info.uCount = static_cast<UINT>(-1);
    info.dwTimeout = 500;
    FlashWindowEx(&info);
}

void OpenUrl(const char* url) {
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

}
