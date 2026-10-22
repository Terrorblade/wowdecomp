#include "HoT/Streaming/InstallFiles.h"
#include "HoT/Streaming/TextConv.h"

namespace HoT::Streaming::InstallFiles {

bool IsDiscordLibrary(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string name = Text::LowerAscii((slash == std::string::npos) ? path : path.substr(slash + 1));
    return name == "discord_game_sdk_64.dll";
}

bool IsForeignBinary(const std::string& path) {
    return false;
}

const char* ManifestPlatform() {
    return "win";
}

}
