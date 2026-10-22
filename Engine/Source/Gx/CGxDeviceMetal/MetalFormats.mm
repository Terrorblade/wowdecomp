#include "Engine/Source/Gx/CGxDeviceMetal/MetalFormats.h"

namespace GxMetal {

namespace {

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
            // ABGR4Unorm stores the nibbles red-high: its r, g, b, a channels read the client's
            // alpha, red, green and blue. It cannot be rendered to with this mapping.
            info.m_pixelFormat = MTLPixelFormatABGR4Unorm;
            info.m_bytesPerUnit = 2;
            Swizzle(info, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleAlpha, MTLTextureSwizzleRed);
            return true;

        case GxTex_Argb1555:
            info.m_pixelFormat = MTLPixelFormatBGR5A1Unorm;
            info.m_bytesPerUnit = 2;
            info.m_renderable = true;
            return true;

        case GxTex_Rgb565:
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

}
