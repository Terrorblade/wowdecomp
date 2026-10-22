#include "HoT/Editor/Model/TextureImport.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include "Engine/Source/BLPFile/Types.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Editor/Model/ImageDecode.h"

namespace HoT::Editor::Model::TextureImport {

static_assert(sizeof(BLPHeader) == 0x494, "BLP2 header");

static const uint32_t MAGIC_BLP2 = 0x32504C42;
static const uint32_t MAX_TARGET = 2048;

static bool DecodeTga(const std::vector<uint8_t>& data, Image& image, std::string& error) {
    error = "unsupported or damaged tga";

    if (data.size() < 18) {
        return false;
    }

    uint8_t idLength = data[0];
    uint8_t mapType = data[1];
    uint8_t type = data[2];
    uint16_t mapFirst = static_cast<uint16_t>(data[3] | (data[4] << 8));
    uint16_t mapLength = static_cast<uint16_t>(data[5] | (data[6] << 8));
    uint8_t mapDepth = data[7];
    uint32_t width = static_cast<uint32_t>(data[12] | (data[13] << 8));
    uint32_t height = static_cast<uint32_t>(data[14] | (data[15] << 8));
    uint8_t depth = data[16];
    uint8_t descriptor = data[17];
    bool rle = type >= 9;
    uint8_t baseType = rle ? static_cast<uint8_t>(type - 8) : type;

    if (!width || !height || width > MAX_SOURCE || height > MAX_SOURCE || baseType < 1 || baseType > 3) {
        return false;
    }

    size_t pos = 18 + idLength;
    std::vector<uint8_t> palette;

    if (mapType == 1) {
        uint32_t entryBytes = (mapDepth + 7) / 8;
        size_t mapBytes = static_cast<size_t>(mapLength) * entryBytes;

        if (pos + mapBytes > data.size() || (baseType == 1 && entryBytes != 3 && entryBytes != 4)) {
            return false;
        }

        if (baseType == 1) {
            palette.assign(static_cast<size_t>(mapLength) * 4, 255);

            for (uint32_t i = 0; i < mapLength; i++) {
                memcpy(&palette[i * 4], &data[pos + i * entryBytes], entryBytes);
            }
        }

        pos += mapBytes;
    } else if (baseType == 1) {
        return false;
    }

    uint32_t bytes = (depth + 7) / 8;

    if ((baseType == 1 && bytes != 1 && bytes != 2) || (baseType == 2 && bytes != 2 && bytes != 3 && bytes != 4) || (baseType == 3 && bytes != 1)) {
        return false;
    }

    auto decode = [&](const uint8_t* p, uint8_t* out) -> bool {
        if (baseType == 1) {
            uint32_t index = bytes == 1 ? p[0] : static_cast<uint32_t>(p[0] | (p[1] << 8));

            if (index < mapFirst || index - mapFirst >= mapLength) {
                return false;
            }

            memcpy(out, &palette[(index - mapFirst) * 4], 4);
        } else if (baseType == 3) {
            out[0] = out[1] = out[2] = p[0];
            out[3] = 255;
        } else if (bytes == 2) {
            uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
            out[0] = static_cast<uint8_t>((v & 0x1F) * 255 / 31);
            out[1] = static_cast<uint8_t>(((v >> 5) & 0x1F) * 255 / 31);
            out[2] = static_cast<uint8_t>(((v >> 10) & 0x1F) * 255 / 31);
            out[3] = (descriptor & 0x0F) ? ((v & 0x8000) ? 255 : 0) : 255;
        } else {
            out[0] = p[0];
            out[1] = p[1];
            out[2] = p[2];
            out[3] = bytes == 4 ? p[3] : 255;
        }

        return true;
    };

    size_t pixels = static_cast<size_t>(width) * height;
    std::vector<uint8_t> raw(pixels * 4);
    size_t done = 0;

    while (done < pixels) {
        if (!rle) {
            if (pos + bytes > data.size() || !decode(&data[pos], &raw[done * 4])) {
                return false;
            }

            pos += bytes;
            done++;
            continue;
        }

        if (pos >= data.size()) {
            return false;
        }

        uint8_t packet = data[pos++];
        size_t run = (packet & 0x7F) + 1;

        if (done + run > pixels) {
            return false;
        }

        if (packet & 0x80) {
            uint8_t pixel[4];

            if (pos + bytes > data.size() || !decode(&data[pos], pixel)) {
                return false;
            }

            pos += bytes;

            for (size_t i = 0; i < run; i++) {
                memcpy(&raw[(done + i) * 4], pixel, 4);
            }
        } else {
            for (size_t i = 0; i < run; i++) {
                if (pos + bytes > data.size() || !decode(&data[pos], &raw[(done + i) * 4])) {
                    return false;
                }

                pos += bytes;
            }
        }

        done += run;
    }

    bool topDown = (descriptor & 0x20) != 0;
    bool rightToLeft = (descriptor & 0x10) != 0;
    image.width = width;
    image.height = height;
    image.bgra.resize(pixels * 4);

    for (uint32_t y = 0; y < height; y++) {
        uint32_t sy = topDown ? y : height - 1 - y;

        for (uint32_t x = 0; x < width; x++) {
            uint32_t sx = rightToLeft ? width - 1 - x : x;
            memcpy(&image.bgra[(static_cast<size_t>(y) * width + x) * 4], &raw[(static_cast<size_t>(sy) * width + sx) * 4], 4);
        }
    }

    error.clear();
    return true;
}

static uint32_t NearestPowerOfTwo(uint32_t value) {
    uint32_t low = 1;

    while (low * 2 <= value && low < MAX_TARGET) {
        low *= 2;
    }

    if (low >= MAX_TARGET) {
        return MAX_TARGET;
    }

    return value - low > low * 2 - value ? low * 2 : low;
}

// Premultiplied float RGBA keeps transparent edges from bleeding dark color into the filtered result.
static std::vector<float> ToPremultiplied(const Image& image) {
    size_t pixels = static_cast<size_t>(image.width) * image.height;
    std::vector<float> out(pixels * 4);

    for (size_t i = 0; i < pixels; i++) {
        float a = image.bgra[i * 4 + 3] / 255.0f;

        for (int32_t c = 0; c < 3; c++) {
            out[i * 4 + c] = image.bgra[i * 4 + c] / 255.0f * a;
        }

        out[i * 4 + 3] = a;
    }

    return out;
}

static void FromPremultiplied(const std::vector<float>& in, size_t pixels, uint8_t* out) {
    for (size_t i = 0; i < pixels; i++) {
        float a = in[i * 4 + 3];

        for (int32_t c = 0; c < 3; c++) {
            float v = a > 0.0f ? in[i * 4 + c] / a : 0.0f;
            out[i * 4 + c] = static_cast<uint8_t>(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f));
        }

        out[i * 4 + 3] = static_cast<uint8_t>(std::clamp(a * 255.0f + 0.5f, 0.0f, 255.0f));
    }
}

// One axis of a triangle filter resample whose support widens when shrinking.
static std::vector<float> ResampleAxis(const std::vector<float>& src, uint32_t width, uint32_t height, uint32_t target, bool horizontal) {
    uint32_t length = horizontal ? width : height;
    uint32_t lines = horizontal ? height : width;
    uint32_t outWidth = horizontal ? target : width;
    std::vector<float> dst(static_cast<size_t>(horizontal ? target * height : width * target) * 4, 0.0f);
    float scale = static_cast<float>(length) / target;
    float support = (std::max)(1.0f, scale);

    for (uint32_t i = 0; i < target; i++) {
        float center = (i + 0.5f) * scale - 0.5f;
        int32_t first = static_cast<int32_t>(floorf(center - support)) + 1;
        int32_t last = static_cast<int32_t>(floorf(center + support));

        for (uint32_t line = 0; line < lines; line++) {
            float sum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float total = 0.0f;

            for (int32_t s = first; s <= last; s++) {
                float weight = 1.0f - fabsf(s - center) / support;

                if (weight <= 0.0f) {
                    continue;
                }

                int32_t clamped = std::clamp(s, 0, static_cast<int32_t>(length) - 1);
                size_t index = horizontal ? static_cast<size_t>(line) * width + clamped : static_cast<size_t>(clamped) * width + line;

                for (int32_t c = 0; c < 4; c++) {
                    sum[c] += src[index * 4 + c] * weight;
                }

                total += weight;
            }

            size_t out = horizontal ? static_cast<size_t>(line) * outWidth + i : static_cast<size_t>(i) * outWidth + line;

            for (int32_t c = 0; c < 4; c++) {
                dst[out * 4 + c] = total > 0.0f ? sum[c] / total : 0.0f;
            }
        }
    }

    return dst;
}

static std::vector<float> HalveImage(const std::vector<float>& src, uint32_t width, uint32_t height) {
    uint32_t outWidth = (std::max)(width / 2, 1u);
    uint32_t outHeight = (std::max)(height / 2, 1u);
    std::vector<float> dst(static_cast<size_t>(outWidth) * outHeight * 4);

    for (uint32_t y = 0; y < outHeight; y++) {
        for (uint32_t x = 0; x < outWidth; x++) {
            uint32_t x0 = (std::min)(x * 2, width - 1);
            uint32_t x1 = (std::min)(x * 2 + 1, width - 1);
            uint32_t y0 = (std::min)(y * 2, height - 1);
            uint32_t y1 = (std::min)(y * 2 + 1, height - 1);

            for (int32_t c = 0; c < 4; c++) {
                float v = src[(static_cast<size_t>(y0) * width + x0) * 4 + c] + src[(static_cast<size_t>(y0) * width + x1) * 4 + c] + src[(static_cast<size_t>(y1) * width + x0) * 4 + c] + src[(static_cast<size_t>(y1) * width + x1) * 4 + c];
                dst[(static_cast<size_t>(y) * outWidth + x) * 4 + c] = v * 0.25f;
            }
        }
    }

    return dst;
}

// Uncompressed BLP2: color encoding 3 with ARGB8888 as preferred format, read by Lock2 with a plain copy.
static void WriteBlp(const Image& image, std::vector<uint8_t>& out) {
    uint32_t width = image.width;
    uint32_t height = image.height;
    std::vector<float> level = ToPremultiplied(image);
    bool alpha = false;

    for (size_t i = 3; i < image.bgra.size(); i += 4) {
        if (image.bgra[i] != 255) {
            alpha = true;
            break;
        }
    }

    BLPHeader header;
    memset(&header, 0, sizeof(header));
    header.m_magic = MAGIC_BLP2;
    header.m_formatVersion = 1;
    header.m_colorEncoding = 3;
    header.m_alphaSize = alpha ? 8 : 0;
    header.m_preferredFormat = 2;
    header.m_hasMips = 1;
    header.m_width = width;
    header.m_height = height;
    out.assign(sizeof(header), 0);

    for (uint32_t mip = 0; mip < 16; mip++) {
        size_t pixels = static_cast<size_t>(width) * height;
        header.m_mipOffsets[mip] = static_cast<uint32_t>(out.size());
        header.m_mipSizes[mip] = static_cast<uint32_t>(pixels * 4);
        size_t at = out.size();
        out.resize(at + pixels * 4);

        if (mip == 0) {
            memcpy(&out[at], image.bgra.data(), pixels * 4);
        } else {
            FromPremultiplied(level, pixels, &out[at]);

            if (!alpha) {
                for (size_t i = 0; i < pixels; i++) {
                    out[at + i * 4 + 3] = 255;
                }
            }
        }

        if (width == 1 && height == 1) {
            break;
        }

        level = HalveImage(level, width, height);
        width = (std::max)(width / 2, 1u);
        height = (std::max)(height / 2, 1u);
    }

    memcpy(out.data(), &header, sizeof(header));
}

static std::wstring LowerExtension(const std::wstring& name) {
    size_t dot = name.find_last_of(L'.');
    std::wstring extension = dot == std::wstring::npos ? std::wstring() : name.substr(dot);

    for (wchar_t& c : extension) {
        c = static_cast<wchar_t>(towlower(c));
    }

    return extension;
}

bool IsImportExtension(const std::wstring& name) {
    std::wstring extension = LowerExtension(name);
    return extension == L".png" || extension == L".jpg" || extension == L".jpeg" || extension == L".bmp" || extension == L".tga" || extension == L".blp";
}

bool ConvertFile(const std::wstring& path, std::vector<uint8_t>& blp, std::string& message, std::string& error) {
    std::wstring extension = LowerExtension(path);
    Image image;

    if (extension == L".blp") {
        std::vector<uint8_t> data;

        if (!ReadDiskFile(path, data)) {
            error = "the file could not be read";
            return false;
        }

        BLPHeader header;

        if (data.size() < 0x94 || (memcpy(&header, data.data(), 0x94), header.m_magic != MAGIC_BLP2 || header.m_formatVersion != 1)) {
            error = "not a BLP2 file";
            return false;
        }

        blp.swap(data);
        message = "copied BLP " + std::to_string(header.m_width) + "x" + std::to_string(header.m_height);
        return true;
    }

    if (extension == L".tga") {
        std::vector<uint8_t> data;

        if (!ReadDiskFile(path, data)) {
            error = "the file could not be read";
            return false;
        }

        if (!DecodeTga(data, image, error)) {
            return false;
        }
    } else if (!DecodeImageFile(path, image, error)) {
        return false;
    }

    uint32_t sourceWidth = image.width;
    uint32_t sourceHeight = image.height;
    uint32_t width = NearestPowerOfTwo(image.width);
    uint32_t height = NearestPowerOfTwo(image.height);

    if (width != image.width || height != image.height) {
        std::vector<float> pixels = ToPremultiplied(image);

        if (width != image.width) {
            pixels = ResampleAxis(pixels, image.width, image.height, width, true);
        }

        if (height != image.height) {
            pixels = ResampleAxis(pixels, width, image.height, height, false);
        }

        image.width = width;
        image.height = height;
        image.bgra.resize(static_cast<size_t>(width) * height * 4);
        FromPremultiplied(pixels, static_cast<size_t>(width) * height, image.bgra.data());
    }

    WriteBlp(image, blp);
    message = std::to_string(sourceWidth) + "x" + std::to_string(sourceHeight);

    if (sourceWidth != width || sourceHeight != height) {
        message += " resized to " + std::to_string(width) + "x" + std::to_string(height);
    }

    message += ", uncompressed BGRA with mipmaps";
    return true;
}

bool ReadInfo(const std::string& gamePath, BlpInfo& info) {
    SFileImpl* file = nullptr;

    if (!SFile::OpenEx(nullptr, gamePath.c_str(), 0, &file) || !file) {
        return false;
    }

    BLPHeader header;
    uint32_t size = SFileGetFileSize(file, nullptr);
    bool ok = size >= 0x94 && SFileReadFile(file, &header, 0x94, nullptr, nullptr, 0);
    SFileCloseFile(file);

    if (!ok || header.m_magic != MAGIC_BLP2 || header.m_formatVersion != 1) {
        return false;
    }

    info.m_width = header.m_width;
    info.m_height = header.m_height;
    info.m_alphaBits = header.m_alphaSize;
    info.m_mips = 0;

    for (uint32_t i = 0; i < 16 && header.m_mipSizes[i]; i++) {
        info.m_mips++;
    }

    switch (header.m_colorEncoding) {
        case 0:
            info.m_format = "JPEG";
            break;
        case 1:
            info.m_format = "PALETTE";
            break;
        case 2:
            info.m_format = header.m_preferredFormat == 0 ? "DXT1" : header.m_preferredFormat == 1 ? "DXT3" : header.m_preferredFormat == 7 ? "DXT5" : "DXT";
            break;
        case 3:
            info.m_format = "ARGB8888";
            break;
        default:
            info.m_format = "UNKNOWN";
            break;
    }

    return true;
}

}
