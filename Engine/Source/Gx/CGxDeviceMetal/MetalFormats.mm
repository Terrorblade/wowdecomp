#include "Engine/Source/Gx/CGxDeviceMetal/MetalFormats.h"

namespace GxMetal {

namespace {

bool s_packedFormats = true;

void Expanded(FormatInfo& info, Packed16 layout) {
    info.m_pixelFormat = MTLPixelFormatBGRA8Unorm;
    info.m_bytesPerUnit = 4;
    info.m_expand = layout;
}

uint8_t Widen4(uint32_t v) {
    return static_cast<uint8_t>((v << 4) | v);
}

uint8_t Widen5(uint32_t v) {
    return static_cast<uint8_t>((v << 3) | (v >> 2));
}

uint8_t Widen6(uint32_t v) {
    return static_cast<uint8_t>((v << 2) | (v >> 4));
}

void Swizzle(FormatInfo& info, MTLTextureSwizzle r, MTLTextureSwizzle g, MTLTextureSwizzle b, MTLTextureSwizzle a) {
    info.m_swizzle = MTLTextureSwizzleChannelsMake(r, g, b, a);
    info.m_swizzled = true;
}

void Depth(FormatInfo& info) {
    info.m_pixelFormat = MTLPixelFormatDepth32Float;
    info.m_bytesPerUnit = 4;
    info.m_depth = true;
    info.m_renderable = true;
}

}

bool TexFormatInfo(EGxTexFormat format, FormatInfo& info) {
    info = FormatInfo();

    switch (format) {
        case GxTex_Abgr8888:
            info.m_pixelFormat = MTLPixelFormatRGBA8Unorm;
            info.m_bytesPerUnit = 4;
            info.m_renderable = true;
            return true;

        case GxTex_Argb8888:
            info.m_pixelFormat = MTLPixelFormatBGRA8Unorm;
            info.m_bytesPerUnit = 4;
            info.m_renderable = true;
            return true;

        case GxTex_Argb4444:
            if (!s_packedFormats) {
                Expanded(info, Packed16::Argb4444);
                return true;
            }

            // ABGR4Unorm stores the nibbles red-high: its r, g, b, a channels read the client's
            // alpha, red, green and blue. It cannot be rendered to with this mapping.
            info.m_pixelFormat = MTLPixelFormatABGR4Unorm;
            info.m_bytesPerUnit = 2;
            Swizzle(info, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleAlpha, MTLTextureSwizzleRed);
            return true;

        case GxTex_Argb1555:
            if (!s_packedFormats) {
                Expanded(info, Packed16::Argb1555);
                info.m_renderable = true;
                return true;
            }

            info.m_pixelFormat = MTLPixelFormatBGR5A1Unorm;
            info.m_bytesPerUnit = 2;
            info.m_renderable = true;
            return true;

        case GxTex_Rgb565:
            if (!s_packedFormats) {
                Expanded(info, Packed16::Rgb565);
                info.m_renderable = true;
                Swizzle(info, MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleOne);
                return true;
            }

            // Same bit layout: red in the high bits
            info.m_pixelFormat = MTLPixelFormatB5G6R5Unorm;
            info.m_bytesPerUnit = 2;
            info.m_renderable = true;
            return true;

        case GxTex_Dxt1:
            info.m_pixelFormat = MTLPixelFormatBC1_RGBA;
            info.m_bytesPerUnit = 8;
            info.m_compressed = true;
            return true;

        case GxTex_Dxt3:
            info.m_pixelFormat = MTLPixelFormatBC2_RGBA;
            info.m_bytesPerUnit = 16;
            info.m_compressed = true;
            return true;

        case GxTex_Dxt5:
            info.m_pixelFormat = MTLPixelFormatBC3_RGBA;
            info.m_bytesPerUnit = 16;
            info.m_compressed = true;
            return true;

        case GxTex_Uv88:
            // Signed bump map offsets; blue and alpha read as one
            info.m_pixelFormat = MTLPixelFormatRG8Snorm;
            info.m_bytesPerUnit = 2;
            Swizzle(info, MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleOne, MTLTextureSwizzleOne);
            return true;

        case GxTex_Gr1616F:
            info.m_pixelFormat = MTLPixelFormatRG16Float;
            info.m_bytesPerUnit = 4;
            info.m_renderable = true;
            Swizzle(info, MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleOne, MTLTextureSwizzleOne);
            return true;

        case GxTex_R32F:
            info.m_pixelFormat = MTLPixelFormatR32Float;
            info.m_bytesPerUnit = 4;
            info.m_renderable = true;
            Swizzle(info, MTLTextureSwizzleRed, MTLTextureSwizzleOne, MTLTextureSwizzleOne, MTLTextureSwizzleOne);
            return true;

        case GxTex_D24X8:
            Depth(info);
            return true;

        default:
            return false;
    }
}

bool WindowFormatInfo(CGxFormat::Format format, FormatInfo& info) {
    info = FormatInfo();

    switch (format) {
        case CGxFormat::Fmt_Rgb565:
            if (!s_packedFormats) {
                info.m_pixelFormat = MTLPixelFormatBGRA8Unorm;
                info.m_bytesPerUnit = 4;
                info.m_renderable = true;
                info.m_noAlpha = true;
                Swizzle(info, MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleOne);
                return true;
            }

            info.m_pixelFormat = MTLPixelFormatB5G6R5Unorm;
            info.m_bytesPerUnit = 2;
            info.m_renderable = true;
            info.m_noAlpha = true;
            return true;

        case CGxFormat::Fmt_ArgbX888:
            info.m_pixelFormat = MTLPixelFormatBGRA8Unorm;
            info.m_bytesPerUnit = 4;
            info.m_renderable = true;
            info.m_noAlpha = true;
            Swizzle(info, MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleOne);
            return true;

        case CGxFormat::Fmt_Argb8888:
            info.m_pixelFormat = MTLPixelFormatBGRA8Unorm;
            info.m_bytesPerUnit = 4;
            info.m_renderable = true;
            return true;

        case CGxFormat::Fmt_Argb2101010:
            info.m_pixelFormat = MTLPixelFormatBGR10A2Unorm;
            info.m_bytesPerUnit = 4;
            info.m_renderable = true;
            return true;

        case CGxFormat::Fmt_Ds160:
        case CGxFormat::Fmt_Ds24X:
        case CGxFormat::Fmt_Ds248:
        case CGxFormat::Fmt_Ds320:
            // The client never enables stencil itself; the plane carries the tags code drawing with Metal
            // (mods) sets through GxMetalSetStencilTag, such as the water classes of CoAVolFog
            Depth(info);
            info.m_pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
            info.m_bytesPerUnit = 8;
            return true;

        default:
            return false;
    }
}

void InitFormats(id<MTLDevice> device) {
    s_packedFormats = [device supportsFamily:MTLGPUFamilyApple1];
}

bool PackedFormatsSupported() {
    return s_packedFormats;
}

void ExpandPacked16(Packed16 layout, uint32_t width, uint32_t height, const uint8_t* in, uint32_t inStride, uint8_t* out, uint32_t outStride) {
    for (uint32_t y = 0; y < height; y++) {
        const uint16_t* src = reinterpret_cast<const uint16_t*>(in + y * inStride);
        uint8_t* dst = out + y * outStride;

        for (uint32_t x = 0; x < width; x++, dst += 4) {
            uint32_t texel = src[x];

            switch (layout) {
                case Packed16::Argb4444:
                    dst[0] = Widen4(texel & 0xF);
                    dst[1] = Widen4((texel >> 4) & 0xF);
                    dst[2] = Widen4((texel >> 8) & 0xF);
                    dst[3] = Widen4(texel >> 12);
                    break;

                case Packed16::Argb1555:
                    dst[0] = Widen5(texel & 0x1F);
                    dst[1] = Widen5((texel >> 5) & 0x1F);
                    dst[2] = Widen5((texel >> 10) & 0x1F);
                    dst[3] = (texel & 0x8000) ? 0xFF : 0x00;
                    break;

                case Packed16::Rgb565:
                    dst[0] = Widen5(texel & 0x1F);
                    dst[1] = Widen6((texel >> 5) & 0x3F);
                    dst[2] = Widen5(texel >> 11);
                    dst[3] = 0xFF;
                    break;

                default:
                    break;
            }
        }
    }
}

}
