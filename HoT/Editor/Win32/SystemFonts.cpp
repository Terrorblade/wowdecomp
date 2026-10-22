#include "HoT/Editor/SystemFonts.h"
#include <windows.h>

namespace HoT::Editor::Resources {

std::string FindSystemFontFile(const char* file, size_t slot) {
    char directory[MAX_PATH];
    UINT length = GetWindowsDirectoryA(directory, sizeof(directory));

    if (!length || length >= sizeof(directory)) {
        return std::string();
    }

    return std::string(directory) + "\\Fonts\\" + file;
}

}
