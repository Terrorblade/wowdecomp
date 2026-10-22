#include "HoT/Discord/Host.h"
#include <cstring>
#include <filesystem>
#include <string>
#include <dlfcn.h>
#include <cctype>
#include <fstream>
#include <strings.h>
#include "HoT/Platform/Process.h"

namespace HoT::Discord::Host {

// LoadLibrary looks next to the exe first, dlopen does not, so try that directory before the default search.
void* LoadCreateFunction() {
    static void* s_module = nullptr;

    if (!s_module) {
        std::filesystem::path exe = Platform::ExecutablePath();

        if (!s_module && !exe.empty()) {
            s_module = dlopen((exe.parent_path() / "discord_game_sdk.so").c_str(), RTLD_NOW | RTLD_LOCAL);
        }

        if (!s_module) {
            s_module = dlopen("discord_game_sdk.so", RTLD_NOW | RTLD_LOCAL);
        }
    }

    return s_module ? dlsym(s_module, "DiscordCreate") : nullptr;
}

bool IsDiscordRunning() {
    // /proc/<pid>/comm stands in for the process snapshot, the kernel cuts it to 15 characters, so compare that much.
    static const char* PROCESS_NAMES[] = { "Discord", "DiscordPTB", "DiscordCanary", "DiscordDevelopment" };
    static const size_t COMM_LENGTH = 15;

    std::error_code ec;

    for (std::filesystem::directory_iterator it("/proc", ec), end; !ec && it != end; it.increment(ec)) {
        const std::string pid = it->path().filename().string();

        if (pid.empty() || !isdigit(static_cast<unsigned char>(pid[0]))) {
            continue;
        }

        std::ifstream comm(it->path() / "comm");
        std::string name;

        if (!comm || !std::getline(comm, name)) {
            continue;
        }

        for (const char* candidate : PROCESS_NAMES) {
            if (strncasecmp(name.c_str(), candidate, COMM_LENGTH) == 0 && name.size() == std::min(strlen(candidate), COMM_LENGTH)) {
                return true;
            }
        }
    }

    return false;
}

}
