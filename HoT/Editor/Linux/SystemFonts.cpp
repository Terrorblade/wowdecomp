#include "HoT/Editor/SystemFonts.h"
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <strings.h>

namespace HoT::Editor::Resources {

// No Windows font directory here: look for the Segoe file in the usual font directories (it may have been copied
// from a Windows install), else use a common sans font of similar weight.
static std::string FindFontFile(const char* file) {
    std::vector<std::filesystem::path> roots;
    const char* dataHome = getenv("XDG_DATA_HOME");
    const char* home = getenv("HOME");

    if (dataHome && *dataHome) {
        roots.emplace_back(std::filesystem::path(dataHome) / "fonts");
    }

    if (home && *home) {
        roots.emplace_back(std::filesystem::path(home) / ".local/share/fonts");
        roots.emplace_back(std::filesystem::path(home) / ".fonts");
    }

    roots.emplace_back("/usr/local/share/fonts");
    roots.emplace_back("/usr/share/fonts");

    for (const std::filesystem::path& root : roots) {
        std::error_code error;
        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, error);

        for (; !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
            if (strcasecmp(it->path().filename().c_str(), file) == 0 && it->is_regular_file(error)) {
                return it->path().string();
            }
        }
    }

    return std::string();
}

std::string FindSystemFontFile(const char* file, size_t slot) {
    static const char* const s_fallbacks[][3] = {
        { "DejaVuSans.ttf", "LiberationSans-Regular.ttf", "NotoSans-Regular.ttf" },
        { "DejaVuSans-Bold.ttf", "LiberationSans-Bold.ttf", "NotoSans-SemiBold.ttf" },
    };

    std::string path = FindFontFile(file);

    for (size_t i = 0; path.empty() && slot < sizeof(s_fallbacks) / sizeof(s_fallbacks[0]) && i < 3; i++) {
        path = FindFontFile(s_fallbacks[slot][i]);
    }

    return path;
}

}
