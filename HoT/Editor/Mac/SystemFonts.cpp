#include "HoT/Editor/SystemFonts.h"
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <strings.h>

namespace HoT::Editor::Resources {

// Look for the Segoe file in the macOS font folders (it may have been copied from a Windows install), else use a
// bundled sans font of similar weight.
static std::string FindFontFile(const char* file) {
    std::vector<std::filesystem::path> roots;
    const char* home = getenv("HOME");

    if (home && *home) {
        roots.emplace_back(std::filesystem::path(home) / "Library/Fonts");
    }

    roots.emplace_back("/Library/Fonts");
    roots.emplace_back("/System/Library/Fonts");

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
        { "Arial.ttf", "Helvetica.ttc", "Geneva.ttf" },
        { "Arial Bold.ttf", "Helvetica.ttc", "Geneva.ttf" },
    };

    std::string path = FindFontFile(file);

    for (size_t i = 0; path.empty() && slot < sizeof(s_fallbacks) / sizeof(s_fallbacks[0]) && i < 3; i++) {
        path = FindFontFile(s_fallbacks[slot][i]);
    }

    return path;
}

}
