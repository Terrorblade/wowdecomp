#ifndef HOT_EDITOR_MODEL_IMAGEDECODE_H
#define HOT_EDITOR_MODEL_IMAGEDECODE_H

#include <cstdint>
#include <string>
#include <vector>

// The per OS image decoders behind TextureImport.
namespace HoT::Editor::Model::TextureImport {

static const uint32_t MAX_SOURCE = 16384;

struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> bgra;
};

bool ReadDiskFile(const std::wstring& path, std::vector<uint8_t>& data);

// png, jpg, bmp and gif through the OS decoder, as 32 bit BGRA.
bool DecodeImageFile(const std::wstring& path, Image& image, std::string& error);

}

#endif
