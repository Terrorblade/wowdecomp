// Pixel formats of the Metal graphics device: the client's texture formats (EGxTexFormat) and
// window formats (CGxFormat::Format) mapped straight to Metal pixel formats (Objective-C++ only).
//
// The mapping keeps the channel layout the client writes: Blit() produces the Direct3D-style
// layouts (A8R8G8B8 stored B, G, R, A; A4R4G4B4 red in the high nibble ...), so each format here is
// the Metal format with the same memory layout, swizzled where Metal names the channels otherwise.

#ifndef ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALFORMATS_H
#define ENGINE_SOURCE_GX_CGXDEVICEMETAL_METALFORMATS_H

#import <Metal/Metal.h>

#include <cstdint>
#include "Engine/Source/Gx/CGxFormat.h"
#include "Engine/Source/Gx/Types.h"

namespace GxMetal {

struct FormatInfo {
    MTLPixelFormat m_pixelFormat = MTLPixelFormatInvalid;
    MTLTextureSwizzleChannels m_swizzle = { MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleAlpha };
    bool m_swizzled = false;
    bool m_depth = false;
    bool m_compressed = false;
    bool m_renderable = false;
    // The format stores no alpha (X8R8G8B8 and the like): alpha reads as one and is never written
    bool m_noAlpha = false;
    // Bytes per pixel, or per 4x4 block for compressed formats
    uint32_t m_bytesPerUnit = 0;

    uint32_t RowPitch(uint32_t width) const { return this->m_compressed ? ((width + 3) / 4) * this->m_bytesPerUnit : width * this->m_bytesPerUnit; }
    uint32_t RowCount(uint32_t height) const { return this->m_compressed ? (height + 3) / 4 : height; }
};

// Texture formats. GxTex_D24X8 is a depth texture; Apple GPUs have no 24-bit depth, so it is
// Depth32Float, which is at least as precise and can be sampled.
bool TexFormatInfo(EGxTexFormat format, FormatInfo& info);

// Window (back buffer) formats; depth formats all map to Depth32Float as above
bool WindowFormatInfo(CGxFormat::Format format, FormatInfo& info);

}

#endif
