#if defined(WOWDECOMP_PLATFORM_MAC)

#include <csignal>
#include <cstring>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Client.h"

// Finder starts a bundle with "/" as the working directory, so the game data is looked up next to Wow64.app.
static void EnterGameDirectory() {
    char path[4096];

    if (!OsPosixExecutablePath(path, sizeof(path))) {
        return;
    }

    char* bundle = strstr(path, ".app/Contents/MacOS/");

    if (!bundle) {
        return;
    }

    *bundle = '\0';
    char* slash = strrchr(path, '/');

    if (slash) {
        *slash = '\0';
        chdir(path[0] ? path : "/");
    }
}

// As on Linux, the main thread's 8 MB stack stands in for the Windows fiber. Sockets get SO_NOSIGPIPE, but writes to
// pipes still raise SIGPIPE, which must not end the process.
int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    EnterGameDirectory();
    OsSetCommandLine(argc, argv);
    StormRtlInitialize();

    return CommonMain();
}

#endif
