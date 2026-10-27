#include "HoT/Editor/Model/TextureExport.h"
#include <algorithm>
#include <cstring>
#include "Engine/Source/BLPFile/blp.h"
#include "ThirdParty/zlib/zlib.h"

namespace HoT::Editor::Model::TextureExport {

static const uint32_t MAX_SIZE = 8192;

static void PutBe32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

static void PutChunk(std::vector<uint8_t>& out, const char* type, const uint8_t* data, size_t size) {
    PutBe32(out, static_cast<uint32_t>(size));
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);

    if (size) {
        out.insert(out.end(), data, data + size);
    }

    uLong crc = crc32(0L, &out[start], static_cast<uInt>(out.size() - start));
    PutBe32(out, static_cast<uint32_t>(crc));
}

static bool EncodePng(const std::vector<uint8_t>& bgra, uint32_t width, uint32_t height, std::vector<uint8_t>& png, std::string& error) {
    size_t row = static_cast<size_t>(width) * 4 + 1;
    std::vector<uint8_t> raw(row * height);

    for (uint32_t y = 0; y < height; y++) {
        uint8_t* out = &raw[y * row];
        const uint8_t* in = &bgra[static_cast<size_t>(y) * width * 4];
        out[0] = 0;

        for (uint32_t x = 0; x < width; x++) {
            out[1 + x * 4] = in[x * 4 + 2];
            out[2 + x * 4] = in[x * 4 + 1];
            out[3 + x * 4] = in[x * 4];
            out[4 + x * 4] = in[x * 4 + 3];
        }
    }

    uLongf packedSize = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> packed(packedSize);

    if (compress2(packed.data(), &packedSize, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK) {
        error = "png compression failed";
        return false;
    }

    static const uint8_t s_signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    uint8_t header[13];
    header[0] = static_cast<uint8_t>(width >> 24);
    header[1] = static_cast<uint8_t>(width >> 16);
    header[2] = static_cast<uint8_t>(width >> 8);
    header[3] = static_cast<uint8_t>(width);
    header[4] = static_cast<uint8_t>(height >> 24);
    header[5] = static_cast<uint8_t>(height >> 16);
    header[6] = static_cast<uint8_t>(height >> 8);
    header[7] = static_cast<uint8_t>(height);
    header[8] = 8;
    header[9] = 6;
    header[10] = 0;
    header[11] = 0;
    header[12] = 0;

    png.assign(s_signature, s_signature + 8);
    PutChunk(png, "IHDR", header, sizeof(header));
    PutChunk(png, "IDAT", packed.data(), packedSize);
    PutChunk(png, "IEND", nullptr, 0);
    return true;
}

// Bytes mip 0 needs, 0 when the encoding cannot be decoded here.
static size_t NeededBytes(const BLPHeader& header) {
    size_t pixels = static_cast<size_t>(header.m_width) * header.m_height;
    size_t blocks = static_cast<size_t>((header.m_width + 3) / 4) * ((header.m_height + 3) / 4);

    switch (header.m_colorEncoding) {
        case COLOR_PAL:
            switch (header.m_alphaSize) {
                case ALPHA_0:
                    return pixels;
                case ALPHA_1:
                    return pixels + (pixels + 7) / 8;
                case ALPHA_4:
                    return pixels + (pixels + 1) / 2;
                case ALPHA_8:
                    return pixels * 2;
                default:
                    return 0;
            }

        case COLOR_DXT:
            if (header.m_preferredFormat == PIXEL_DXT1) {
                return blocks * 8;
            }

            return header.m_preferredFormat == PIXEL_DXT3 || header.m_preferredFormat == PIXEL_DXT5 ? blocks * 16 : 0;

        case COLOR_RAW:
            return pixels * 4;

        default:
            return 0;
    }
}

bool BlpToPng(const std::vector<uint8_t>& blp, std::vector<uint8_t>& png, std::string& error) {
    if (blp.size() < sizeof(BLPHeader)) {
        error = "not a BLP2 file";
        return false;
    }

    std::vector<uint8_t> data = blp;
    BLPHeader header;
    memcpy(&header, data.data(), sizeof(header));

    if (!CBLPFile::ValidateHeader(header) || !header.m_width || !header.m_height || header.m_width > MAX_SIZE || header.m_height > MAX_SIZE) {
        error = "not a BLP2 file";
        return false;
    }

    if (header.m_colorEncoding == COLOR_JPEG) {
        error = "JPEG BLPs are not supported";
        return false;
    }

    size_t needed = NeededBytes(header);

    if (!needed) {
        error = "unsupported BLP encoding";
        return false;
    }

    if (header.m_mipSizes[0] < needed || header.m_mipOffsets[0] > data.size() || data.size() - header.m_mipOffsets[0] < needed) {
        error = "damaged BLP";
        return false;
    }

    uint32_t width = header.m_width;
    uint32_t height = header.m_height;
    size_t pixels = static_cast<size_t>(width) * height;
    std::vector<uint8_t> bgra((std::max)(pixels * 4, static_cast<size_t>(header.m_mipSizes[0])));
    CBLPFile file;
    uint32_t stride = 0;

    if (!file.Source(data.data()) || !file.Lock2("export", PIXEL_ARGB8888, 0, bgra.data(), stride)) {
        error = "the BLP could not be decoded";
        return false;
    }

    bgra.resize(pixels * 4);

    if (header.m_alphaSize == ALPHA_0) {
        for (size_t i = 0; i < pixels; i++) {
            bgra[i * 4 + 3] = 255;
        }
    }

    return EncodePng(bgra, width, height, png, error);
}

}
