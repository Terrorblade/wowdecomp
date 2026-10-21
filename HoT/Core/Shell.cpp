#include "HoT/Core/Shell.h"
#include "Engine/Source/Os/Gui.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <thread>

extern char** environ;
#endif

namespace HoT::Shell {

void FlashGameWindow() {
#if defined(_WIN32)
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
#else
    // No-op: SDL_FlashWindow would do this but SDL is not on the HoT include path.
#endif
}

void OpenUrl(const char* url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#else
    // xdg-open gets the URL as its only argv entry, no shell is involved. A leading '-' would be read as an option.
    if (!url || !url[0] || url[0] == '-') {
        return;
    }

    char program[] = "xdg-open";
    char* argv[] = { program, const_cast<char*>(url), nullptr };
    pid_t pid = 0;

    if (posix_spawnp(&pid, program, nullptr, nullptr, argv, environ) == 0) {
        // xdg-open can block until the browser exits, so reap it off the calling thread.
        std::thread([pid]() {
            int status = 0;
            waitpid(pid, &status, 0);
        }).detach();
    }
#endif
}

}
