#ifndef HOT_EDITOR_MODEL_TEXTUREEXPORT_H
#define HOT_EDITOR_MODEL_TEXTUREEXPORT_H

#include <cstdint>
#include <string>
#include <vector>

// Client textures to png files an image editor opens.
namespace HoT::Editor::Model::TextureExport {

// Decodes mip 0 of a BLP2 file (palette, DXT or raw) into a 32 bit RGBA png.
bool BlpToPng(const std::vector<uint8_t>& blp, std::vector<uint8_t>& png, std::string& error);

}

#endif
