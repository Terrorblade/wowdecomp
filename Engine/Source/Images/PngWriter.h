#ifndef ENGINE_SOURCE_IMAGES_PNGWRITER_H
#define ENGINE_SOURCE_IMAGES_PNGWRITER_H

#include <cstdint>
#include <vector>

bool PngEncodeBgra(const void* pixels, uint32_t width, uint32_t height, std::vector<uint8_t>& out);
int32_t PngWriteBgra(const char* fileName, const void* pixels, uint32_t width, uint32_t height);

#endif
