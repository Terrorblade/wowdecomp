#include "HoT/Editor/Model/TextureImport.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#if defined(_WIN32)
#include <windows.h>
#include <wincodec.h>
#else
#include <cstdlib>
#include <cwctype>
#include "HoT/Streaming/TextConv.h"
#include "ThirdParty/zlib/zlib.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "Engine/Source/BLPFile/Types.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

namespace HoT::Editor::Model::TextureImport {

static_assert(sizeof(BLPHeader) == 0x494, "BLP2 header");

static const uint32_t MAGIC_BLP2 = 0x32504C42;
static const uint32_t MAX_SOURCE = 16384;
static const uint32_t MAX_TARGET = 2048;

struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> bgra;
};

#if defined(_WIN32)
template <class T>
static void SafeRelease(T*& object) {
    if (object) {
        object->Release();
        object = nullptr;
    }
}

static bool ReadDiskFile(const std::wstring& path, std::vector<uint8_t>& data) {
    FILE* file = _wfopen(path.c_str(), L"rb");

    if (!file) {
        return false;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    bool ok = size >= 0 && size < 0x10000000;

    if (ok) {
        data.resize(static_cast<size_t>(size));
        ok = !size || fread(data.data(), 1, data.size(), file) == data.size();
    }

    fclose(file);
    return ok;
}

static bool DecodeWic(const std::wstring& path, Image& image, std::string& error) {
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninitialize = SUCCEEDED(init);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool ok = false;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    }

    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }

    if (SUCCEEDED(hr)) {
        hr = factory->CreateFormatConverter(&converter);
    }

    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }

    UINT width = 0;
    UINT height = 0;

    if (SUCCEEDED(hr)) {
        hr = converter->GetSize(&width, &height);
    }

    if (SUCCEEDED(hr)) {
        if (!width || !height || width > MAX_SOURCE || height > MAX_SOURCE) {
            error = "image size is not supported";
        } else {
            image.width = width;
            image.height = height;
            image.bgra.resize(static_cast<size_t>(width) * height * 4);
            hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(image.bgra.size()), image.bgra.data());
            ok = SUCCEEDED(hr);
        }
    }

    if (!ok && error.empty()) {
        error = "the image could not be decoded";
    }

    SafeRelease(converter);
    SafeRelease(frame);
    SafeRelease(decoder);
    SafeRelease(factory);

    if (uninitialize) {
        CoUninitialize();
    }

    return ok;
}
#else
static bool ReadDiskFile(const std::wstring& path, std::vector<uint8_t>& data) {
    FILE* file = fopen(Streaming::Text::NarrowUtf8(path).c_str(), "rb");

    if (!file) {
        return false;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    bool ok = size >= 0 && size < 0x10000000;

    if (ok) {
        data.resize(static_cast<size_t>(size));
        ok = !size || fread(data.data(), 1, data.size(), file) == data.size();
    }

    fclose(file);
    return ok;
}

static uint32_t ReadBe32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

static uint32_t ReadLe16(const uint8_t* p) {
    return static_cast<uint32_t>(p[0] | (p[1] << 8));
}

static uint32_t ReadLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static uint8_t PaethPredictor(int32_t a, int32_t b, int32_t c) {
    int32_t p = a + b - c;
    int32_t pa = std::abs(p - a);
    int32_t pb = std::abs(p - b);
    int32_t pc = std::abs(p - c);
    return static_cast<uint8_t>(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}

// PNG through the in-tree zlib: every colour type and bit depth, Adam7 included, tRNS honoured.
static bool DecodePng(const std::vector<uint8_t>& data, Image& image, std::string& error) {
    static const uint8_t s_signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

    if (data.size() < 8 || memcmp(data.data(), s_signature, 8)) {
        return false;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;
    uint32_t colorType = 0;
    uint32_t interlace = 0;
    bool haveHeader = false;
    std::vector<uint8_t> palette;
    std::vector<uint8_t> paletteAlpha;
    uint32_t key[3] = {};
    bool haveKey = false;
    std::vector<uint8_t> compressed;

    for (size_t pos = 8; pos + 12 <= data.size();) {
        uint32_t length = ReadBe32(&data[pos]);
        uint32_t type = ReadBe32(&data[pos + 4]);
        const uint8_t* chunk = &data[pos + 8];

        if (length > data.size() - pos - 12) {
            return false;
        }

        if (type == 0x49484452 && length >= 13) {
            width = ReadBe32(chunk);
            height = ReadBe32(chunk + 4);
            depth = chunk[8];
            colorType = chunk[9];
            interlace = chunk[12];
            haveHeader = chunk[10] == 0 && chunk[11] == 0 && interlace <= 1;
        } else if (type == 0x504C5445) {
            palette.assign(chunk, chunk + length);
        } else if (type == 0x74524E53) {
            if (colorType == 3) {
                paletteAlpha.assign(chunk, chunk + length);
            } else if (colorType == 0 && length >= 2) {
                key[0] = (chunk[0] << 8) | chunk[1];
                haveKey = true;
            } else if (colorType == 2 && length >= 6) {
                for (uint32_t i = 0; i < 3; i++) {
                    key[i] = (chunk[i * 2] << 8) | chunk[i * 2 + 1];
                }

                haveKey = true;
            }
        } else if (type == 0x49444154) {
            compressed.insert(compressed.end(), chunk, chunk + length);
        } else if (type == 0x49454E44) {
            break;
        }

        pos += 12 + static_cast<size_t>(length);
    }

    uint32_t channels = colorType == 0 ? 1 : colorType == 2 ? 3 : colorType == 3 ? 1 : colorType == 4 ? 2 : colorType == 6 ? 4 : 0;
    bool depthOk = colorType == 0 ? (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16)
        : colorType == 3 ? (depth == 1 || depth == 2 || depth == 4 || depth == 8)
        : (depth == 8 || depth == 16);

    if (!haveHeader || !channels || !depthOk || (colorType == 3 && palette.size() < 3)) {
        return false;
    }

    if (!width || !height || width > MAX_SOURCE || height > MAX_SOURCE) {
        error = "image size is not supported";
        return false;
    }

    static const uint32_t s_adam7[7][4] = {
        { 0, 0, 8, 8 }, { 4, 0, 8, 8 }, { 0, 4, 4, 8 }, { 2, 0, 4, 4 }, { 0, 2, 2, 4 }, { 1, 0, 2, 2 }, { 0, 1, 1, 2 },
    };
    static const uint32_t s_single[1][4] = { { 0, 0, 1, 1 } };
    const uint32_t (*passes)[4] = interlace ? s_adam7 : s_single;
    uint32_t passCount = interlace ? 7 : 1;
    uint32_t bitsPerPixel = channels * depth;
    size_t filterStride = (std::max)(bitsPerPixel / 8, 1u);
    size_t total = 0;

    for (uint32_t p = 0; p < passCount; p++) {
        size_t passWidth = (width - passes[p][0] + passes[p][2] - 1) / passes[p][2];
        size_t passHeight = (height - passes[p][1] + passes[p][3] - 1) / passes[p][3];

        if (width > passes[p][0] && height > passes[p][1]) {
            total += passHeight * (1 + (passWidth * bitsPerPixel + 7) / 8);
        }
    }

    std::vector<uint8_t> raw(total);
    z_stream stream = {};
    stream.next_in = compressed.data();
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = raw.data();
    stream.avail_out = static_cast<uInt>(raw.size());

    if (inflateInit(&stream) != Z_OK) {
        return false;
    }

    int32_t result = Z_OK;

    while (result == Z_OK && stream.avail_out) {
        result = inflate(&stream, Z_NO_FLUSH);
    }

    bool filled = stream.avail_out == 0;
    inflateEnd(&stream);

    if (!filled) {
        return false;
    }

    image.width = width;
    image.height = height;
    image.bgra.assign(static_cast<size_t>(width) * height * 4, 255);

    auto sample = [&](const uint8_t* row, size_t x, uint32_t c) -> uint32_t {
        if (depth == 16) {
            const uint8_t* p = row + (x * channels + c) * 2;
            return (p[0] << 8) | p[1];
        }

        if (depth == 8) {
            return row[x * channels + c];
        }

        size_t bit = x * depth;
        return (row[bit / 8] >> (8 - depth - bit % 8)) & ((1u << depth) - 1);
    };

    auto to8 = [&](uint32_t value) -> uint8_t {
        return static_cast<uint8_t>(depth == 16 ? (value * 255 + 32767) / 65535 : depth == 8 ? value : value * 255 / ((1u << depth) - 1));
    };

    size_t offset = 0;

    for (uint32_t p = 0; p < passCount; p++) {
        if (width <= passes[p][0] || height <= passes[p][1]) {
            continue;
        }

        size_t passWidth = (width - passes[p][0] + passes[p][2] - 1) / passes[p][2];
        size_t passHeight = (height - passes[p][1] + passes[p][3] - 1) / passes[p][3];
        size_t rowBytes = (passWidth * bitsPerPixel + 7) / 8;
        std::vector<uint8_t> previous(rowBytes, 0);

        for (size_t y = 0; y < passHeight; y++) {
            uint8_t filter = raw[offset];
            uint8_t* row = &raw[offset + 1];
            offset += 1 + rowBytes;

            for (size_t i = 0; i < rowBytes; i++) {
                int32_t a = i >= filterStride ? row[i - filterStride] : 0;
                int32_t b = previous[i];
                int32_t c = i >= filterStride ? previous[i - filterStride] : 0;

                switch (filter) {
                    case 0:
                        break;
                    case 1:
                        row[i] = static_cast<uint8_t>(row[i] + a);
                        break;
                    case 2:
                        row[i] = static_cast<uint8_t>(row[i] + b);
                        break;
                    case 3:
                        row[i] = static_cast<uint8_t>(row[i] + ((a + b) >> 1));
                        break;
                    case 4:
                        row[i] = static_cast<uint8_t>(row[i] + PaethPredictor(a, b, c));
                        break;
                    default:
                        return false;
                }
            }

            memcpy(previous.data(), row, rowBytes);

            for (size_t x = 0; x < passWidth; x++) {
                size_t dx = passes[p][0] + x * passes[p][2];
                size_t dy = passes[p][1] + y * passes[p][3];
                uint8_t* out = &image.bgra[(dy * width + dx) * 4];

                if (colorType == 3) {
                    uint32_t index = sample(row, x, 0);

                    if (index * 3 + 2 >= palette.size()) {
                        return false;
                    }

                    out[0] = palette[index * 3 + 2];
                    out[1] = palette[index * 3 + 1];
                    out[2] = palette[index * 3];
                    out[3] = index < paletteAlpha.size() ? paletteAlpha[index] : 255;
                } else if (colorType == 0 || colorType == 4) {
                    uint32_t gray = sample(row, x, 0);
                    out[0] = out[1] = out[2] = to8(gray);
                    out[3] = colorType == 4 ? to8(sample(row, x, 1)) : (haveKey && gray == key[0] ? 0 : 255);
                } else {
                    uint32_t r = sample(row, x, 0);
                    uint32_t g = sample(row, x, 1);
                    uint32_t b = sample(row, x, 2);
                    out[0] = to8(b);
                    out[1] = to8(g);
                    out[2] = to8(r);
                    out[3] = colorType == 6 ? to8(sample(row, x, 3)) : (haveKey && r == key[0] && g == key[1] && b == key[2] ? 0 : 255);
                }
            }
        }
    }

    return true;
}

static uint8_t MaskChannel(uint32_t value, uint32_t mask) {
    if (!mask) {
        return 255;
    }

    uint32_t shift = 0;

    while (!(mask & (1u << shift))) {
        shift++;
    }

    uint32_t max = mask >> shift;
    return static_cast<uint8_t>(((static_cast<uint64_t>((value & mask) >> shift)) * 255 + max / 2) / max);
}

// Uncompressed BMP (1/4/8 bit palette, 16/24/32 bit, bitfields); RLE BMPs fail as undecodable.
static bool DecodeBmp(const std::vector<uint8_t>& data, Image& image, std::string& error) {
    if (data.size() < 26 || data[0] != 'B' || data[1] != 'M') {
        return false;
    }

    uint32_t pixelOffset = ReadLe32(&data[10]);
    uint32_t headerSize = ReadLe32(&data[14]);
    int32_t width;
    int32_t height;
    uint32_t bits;
    uint32_t compression = 0;
    uint32_t paletteCount = 0;

    if (headerSize == 12) {
        width = static_cast<int16_t>(ReadLe16(&data[18]));
        height = static_cast<int16_t>(ReadLe16(&data[20]));
        bits = ReadLe16(&data[24]);
    } else if (headerSize >= 40 && data.size() >= 54) {
        width = static_cast<int32_t>(ReadLe32(&data[18]));
        height = static_cast<int32_t>(ReadLe32(&data[22]));
        bits = ReadLe16(&data[28]);
        compression = ReadLe32(&data[30]);
        paletteCount = ReadLe32(&data[46]);
    } else {
        return false;
    }

    bool topDown = height < 0;
    uint32_t w = static_cast<uint32_t>(width);
    uint32_t h = topDown ? 0u - static_cast<uint32_t>(height) : static_cast<uint32_t>(height);

    if (width <= 0 || !h || w > MAX_SOURCE || h > MAX_SOURCE) {
        error = "image size is not supported";
        return false;
    }

    bool bitfields = compression == 3 || compression == 6;

    if ((compression != 0 && !bitfields) || (bitfields && bits != 16 && bits != 32) || (bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32)) {
        return false;
    }

    uint32_t masks[4] = {};

    if (bits == 16 && !bitfields) {
        masks[0] = 0x7C00;
        masks[1] = 0x03E0;
        masks[2] = 0x001F;
    } else if (bits == 32 && !bitfields) {
        masks[0] = 0x00FF0000;
        masks[1] = 0x0000FF00;
        masks[2] = 0x000000FF;
    } else if (bitfields) {
        if (data.size() < 66) {
            return false;
        }

        masks[0] = ReadLe32(&data[54]);
        masks[1] = ReadLe32(&data[58]);
        masks[2] = ReadLe32(&data[62]);

        if ((headerSize >= 56 || compression == 6) && data.size() >= 70) {
            masks[3] = ReadLe32(&data[66]);
        }
    }

    std::vector<uint8_t> palette;

    if (bits <= 8) {
        uint32_t entryBytes = headerSize == 12 ? 3 : 4;
        uint32_t count = paletteCount && paletteCount <= (1u << bits) ? paletteCount : (1u << bits);
        size_t start = 14 + static_cast<size_t>(headerSize);

        if (start + static_cast<size_t>(count) * entryBytes > data.size()) {
            return false;
        }

        palette.assign(static_cast<size_t>(1u << bits) * 3, 0);

        for (uint32_t i = 0; i < count; i++) {
            memcpy(&palette[i * 3], &data[start + i * entryBytes], 3);
        }
    }

    size_t stride = ((static_cast<size_t>(w) * bits + 31) / 32) * 4;

    if (pixelOffset > data.size() || stride * h > data.size() - pixelOffset) {
        return false;
    }

    image.width = w;
    image.height = h;
    image.bgra.resize(static_cast<size_t>(w) * h * 4);

    for (uint32_t y = 0; y < h; y++) {
        const uint8_t* row = &data[pixelOffset + stride * (topDown ? y : h - 1 - y)];
        uint8_t* out = &image.bgra[static_cast<size_t>(y) * w * 4];

        for (uint32_t x = 0; x < w; x++, out += 4) {
            if (bits <= 8) {
                size_t bit = static_cast<size_t>(x) * bits;
                uint32_t index = (row[bit / 8] >> (8 - bits - bit % 8)) & ((1u << bits) - 1);
                memcpy(out, &palette[index * 3], 3);
                out[3] = 255;
            } else if (bits == 24) {
                memcpy(out, &row[x * 3], 3);
                out[3] = 255;
            } else {
                uint32_t value = bits == 16 ? ReadLe16(&row[x * 2]) : ReadLe32(&row[x * 4]);
                out[0] = MaskChannel(value, masks[2]);
                out[1] = MaskChannel(value, masks[1]);
                out[2] = MaskChannel(value, masks[0]);
                out[3] = MaskChannel(value, masks[3]);
            }
        }
    }

    return true;
}

// No WIC here: png and bmp are decoded by hand (png through the in-tree zlib). The tree has no JPEG decoder (only
// the libjpeg compressor), so jpg/jpeg fail like a WIC decode failure.
static bool DecodeWic(const std::wstring& path, Image& image, std::string& error) {
    std::vector<uint8_t> data;
    bool ok = ReadDiskFile(path, data) && (DecodePng(data, image, error) || (error.empty() && DecodeBmp(data, image, error)));

    if (!ok && error.empty()) {
        error = "the image could not be decoded";
    }

    return ok;
}
#endif

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
    } else if (!DecodeWic(path, image, error)) {
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
