#ifndef HOT_EDITOR_SYSTEMFONTS_H
#define HOT_EDITOR_SYSTEMFONTS_H

#include <cstddef>
#include <string>

namespace HoT::Editor::Resources {

// The installed font file, or a common font of similar weight for the slot (0 regular, 1 bold), empty if none.
std::string FindSystemFontFile(const char* file, size_t slot);

}

#endif
