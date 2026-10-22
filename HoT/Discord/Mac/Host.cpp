#include "HoT/Discord/Host.h"
#include <cstring>
#include <filesystem>
#include <string>
#include <dlfcn.h>
#include <vector>
#include <libproc.h>
#include <strings.h>
#include <sys/param.h>
#include "HoT/Platform/Process.h"

namespace HoT::Discord::Host {

// dlopen does not look next to the exe, so try the bundle folders before the default search.
void* LoadCreateFunction() {
    static void* s_module = nullptr;

    if (!s_module) {
        std::filesystem::path exe = Platform::ExecutablePath();

        if (!s_module && !exe.empty()) {
            s_module = dlopen((exe.parent_path() / "discord_game_sdk.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
        }

        if (!s_module && !exe.empty()) {
            s_module = dlopen((exe.parent_path().parent_path() / "Frameworks" / "discord_game_sdk.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
        }

        if (!s_module) {
            s_module = dlopen("discord_game_sdk.dylib", RTLD_NOW | RTLD_LOCAL);
        }
    }

    return s_module ? dlsym(s_module, "DiscordCreate") : nullptr;
}

bool IsDiscordRunning() {
    static const char* PROCESS_NAMES[] = { "Discord", "Discord PTB", "Discord Canary", "Discord Development" };

    int count = proc_listallpids(nullptr, 0);

    if (count <= 0) {
        return false;
    }

    std::vector<pid_t> pids(static_cast<size_t>(count) + 16);
    count = proc_listallpids(pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));

    for (int i = 0; i < count; i++) {
        char name[2 * MAXCOMLEN + 1] = {};

        if (proc_name(pids[i], name, sizeof(name)) <= 0) {
            continue;
        }

        for (const char* candidate : PROCESS_NAMES) {
            if (strcasecmp(name, candidate) == 0) {
                return true;
            }
        }
    }

    return false;
}

}
