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
    // open gets the URL as its only argv entry, no shell is involved. A leading '-' would be read as an option.
    if (!url || !url[0] || url[0] == '-') {
        return;
    }

    char program[] = "/usr/bin/open";
    char* argv[] = { program, const_cast<char*>(url), nullptr };
    pid_t pid = 0;

    if (posix_spawn(&pid, program, nullptr, nullptr, argv, environ) == 0) {
        // Reap it off the calling thread so no zombie is left behind.
        std::thread([pid]() {
            int status = 0;
            waitpid(pid, &status, 0);
        }).detach();
    }
}

}
