#include "HoT/Core/Shell.h"
#include <spawn.h>
#include <sys/wait.h>
#include <thread>

extern char** environ;

namespace HoT::Shell {

// No-op: SDL_FlashWindow would do this but SDL is not on the HoT include path.
void FlashGameWindow() {
}

void OpenUrl(const char* url) {
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
}

}
