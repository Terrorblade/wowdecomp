#include "HoT/Streaming/InstallFiles.h"
#include "HoT/Streaming/TextConv.h"

namespace HoT::Streaming::InstallFiles {

bool IsDiscordLibrary(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string name = Text::LowerAscii((slash == std::string::npos) ? path : path.substr(slash + 1));
    return name == "discord_game_sdk_64.dll" || name == "discord_game_sdk.so";
}

// The manifest describes the Windows install, its PE executables and DLLs have no use next to bin/Linux/Wow64.
bool IsForeignBinary(const std::string& path) {
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of("/\\");

    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return false;
    }

    const std::string ext = Text::LowerAscii(path.substr(dot));
    return ext == ".exe" || ext == ".dll";
}

const char* ManifestPlatform() {
    return "linux";
}

}
