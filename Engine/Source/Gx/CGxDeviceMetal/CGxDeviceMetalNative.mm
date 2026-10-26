// The Metal graphics device: the client's CGxDevice states go straight to Metal, with no Direct3D 9
// layer in between. It owns the MTLDevice and queue through GxMetal::Context, presents into the
// window's CAMetalLayer, and turns the client's state, shaders, textures, buffers and draws into
// Metal commands. WRATHMODERN_METAL_SHADERS=0 makes it report no shader profiles so the client takes
// its fixed-function path, which is useful when comparing generated shaders against the client's.
//
// The window is the platform's (OsMacCreateGameWindow), as for the shim: its content view is
// layer-backed by a CAMetalLayer and the device presents into that layer through GxMetal::Context.

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "Engine/Source/Gx/CGxDeviceMetal/CGxDeviceMetalNative.h"
#include "Engine/Source/Gx/Blit.h"
#include "Engine/Source/Gx/CGxBuf.h"
#include "Engine/Source/Gx/CGxPool.h"
#include "Engine/Source/Gx/CGxTex.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/EmergencyMem.h"
#include "Engine/Source/Gx/CGxDeviceMetal/MetalFormats.h"
#include "Engine/Source/Os/Mac/MacOs.h"
#include "Engine/Source/Os/Error.h"
#include "Engine/Source/Os/Gui.h"

namespace {

// The drawable is in pixels; the window's client size is in points and the layer scales it
void DrawableSize(void* window, int32_t* width, int32_t* height) {
    *width = 0;
    *height = 0;
    OsMacGetClientSize(window, width, height);
}

}

CGxDeviceMetalNative::CGxDeviceMetalNative() {
    this->m_hwnd = nullptr;
    this->m_ownhwnd = 0;
    this->m_metal = nullptr;
    this->m_pipelines = nullptr;
    this->m_inScene = 0;
    this->m_backBuffer = nullptr;
    this->m_depthBuffer = nullptr;
    this->m_textureDepthBuffer = nullptr;
    this->m_samples = 1;
    this->m_savedDepthTarget = GxMetal::Target();
    this->m_hwCursorDirty = 0;
    this->m_hwCursorImage = nullptr;

    for (uint32_t i = 0; i < 16; i++) {
        this->m_streamBufs[i] = nullptr;
        this->m_streamOffsets[i] = 0;
        this->m_streamStrides[i] = 0;
    }

    this->m_indexBuf = nullptr;
    this->m_streamPassId = 0;
    this->m_defaultTexture = nullptr;

    for (uint32_t i = 0; i < GxVertexBufferFormats_Last; i++) {
        this->m_formatLayouts[i] = nullptr;
    }

    // The two stream buffers the client's BufStream hands out; the old device creates them in its
    // constructor too, before any Metal device exists (the pools allocate on first lock)
    this->DeviceCreatePools();
    this->DeviceCreateStreamBufs();
}

CGxDeviceMetalNative::~CGxDeviceMetalNative() {
    this->DeviceDestroy();
}

int32_t CGxDeviceMetalNative::ICreateMetal() {
    if (this->m_metal) {
        return 1;
    }

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    if (!device) {
        GxMetal::Log("metal: no device");
        return 0;
    }

    this->m_metal = new GxMetal::Context();

    if (!this->m_metal->Init(device)) {
        delete this->m_metal;
        this->m_metal = nullptr;
        return 0;
    }

    this->m_pipelines = new GxMetal::PipelineCache();
    this->m_pipelines->Init(device);
    GxMetal::Log("metal: native device on %s", [[device name] UTF8String]);
    return 1;
}

void CGxDeviceMetalNative::IDestroyMetal() {
    this->IReleaseQueries();

    if (this->m_metal) {
        this->m_metal->EndEncoders();
        this->m_metal->Flush(true);
        this->m_bound = Bound{};
        delete this->m_metal;
        this->m_metal = nullptr;
    }

    delete this->m_pipelines;
    this->m_pipelines = nullptr;

    for (uint32_t i = 0; i < 16; i++) {
        GxMetal::Clear(this->m_streamBufs[i]);
    }

    GxMetal::Clear(this->m_indexBuf);
    this->m_streamPassId = 0;

    for (uint32_t i = 0; i < 16; i++) {
        GxMetal::Clear(this->m_hw.m_textures[i]);
    }

    GxMetal::Clear(this->m_defaultTexture);
    GxMetal::Clear(this->m_textureDepthBuffer);
}

int32_t CGxDeviceMetalNative::ICreateWindow(CGxFormat& format) {
    OsMacWindowParams params;
    params.m_width = format.m_size.x;
    params.m_height = format.m_size.y;
    params.m_fullscreen = !format.m_window;
    params.m_maximize = format.m_window && format.m_maximize == 1;

    this->m_hwnd = OsMacCreateGameWindow(params, "World of Warcraft");

    if (!this->m_hwnd) {
        return 0;
    }

    this->m_ownhwnd = 1;

    // Fullscreen renders at the format's resolution, extended to the window's aspect ratio so the image
    // is neither stretched nor letterboxed (the client takes its aspect ratio from this size), and the
    // compositor scales it; a window renders at its own size in pixels (as the shim device does)
    if (!format.m_window && format.m_size.x > 0 && format.m_size.y > 0) {
        OsMacSetDrawableSize(this->m_hwnd, 0, 0);
        int32_t windowWidth = 0;
        int32_t windowHeight = 0;
        DrawableSize(this->m_hwnd, &windowWidth, &windowHeight);

        int32_t width = format.m_size.x;
        int32_t height = format.m_size.y;

        if (windowWidth > 0 && windowHeight > 0) {
            if (static_cast<int64_t>(windowWidth) * height > static_cast<int64_t>(windowHeight) * width) {
                width = static_cast<int32_t>((static_cast<int64_t>(height) * windowWidth + windowHeight / 2) / windowHeight);
            } else {
                height = static_cast<int32_t>((static_cast<int64_t>(width) * windowHeight + windowWidth / 2) / windowWidth);
            }
        }

        OsMacSetDrawableSize(this->m_hwnd, width, height);
    } else {
        OsMacSetDrawableSize(this->m_hwnd, 0, 0);
    }

    int32_t width = 0;
    int32_t height = 0;
    DrawableSize(this->m_hwnd, &width, &height);

    if (format.m_window) {
        format.m_size.x = width;
        format.m_size.y = height;
    }

    format.m_pos.x = 0;
    format.m_pos.y = 0;

    CRect rect;
    rect.minX = 0.0f;
    rect.minY = 0.0f;
    rect.maxX = static_cast<float>(width);
    rect.maxY = static_cast<float>(height);
    this->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), 0);
    return 1;
}

void CGxDeviceMetalNative::ISetCaps(const CGxFormat& format) {
    GxMetal::FormatInfo color;
    GxMetal::WindowFormatInfo(format.m_colorFormat, color);

    this->m_caps.m_numTmus = 8;
    this->m_caps.m_pixelCenterOnEdge = 0;
    this->m_caps.m_texelCenterOnEdge = 1;
    this->m_caps.m_numStreams = 16;
    // Every stream keeps its own byte offset; with 0 the client would fold the offset of stream 0
    // into a base vertex index and read the other streams from the start of their buffers
    this->m_caps.m_streamOffset = 1;
    this->m_caps.m_colorFormat = GxCF_argb;
    this->m_caps.m_maxIndex = 65535;
    this->m_caps.m_generateMipMaps = 1;
    this->m_caps.m_texMaxSize[GxTex_2d] = 16384;
    this->m_caps.m_texMaxSize[GxTex_CubeMap] = 16384;
    this->m_caps.m_texMaxSize[GxTex_Rectangle] = 16384;
    this->m_caps.m_texMaxSize[GxTex_NonPow2] = 16384;

    // The client refuses to create a texture whose target the device does not advertise
    this->m_caps.m_texTarget[GxTex_2d] = 1;
    this->m_caps.m_texTarget[GxTex_CubeMap] = 1;
    this->m_caps.m_texTarget[GxTex_Rectangle] = 1;
    this->m_caps.m_texTarget[GxTex_NonPow2] = 1;
    this->m_caps.m_texNonPow2 = 1;
    this->m_caps.m_texFilterTrilinear = 1;
    this->m_caps.m_texFilterAnisotropic = 1;
    this->m_caps.m_maxTexAnisotropy = 16;
    this->m_caps.m_depthBias = 1;
    this->m_caps.m_colorWrite = 1;
    this->m_caps.m_maxClipPlanes = 6;
    // The client's cursor becomes the system pointer over the game view (an NSCursor), as the
    // Direct3D device's hardware cursor
    this->m_caps.m_hwCursor = 1;
    this->m_caps.m_occlusionQuery = 1;
    this->m_caps.m_pointParameters = 1;
    this->m_caps.m_pointScaleMax = 64.0f;
    this->m_caps.m_pointSprite = 1;
    this->m_caps.m_blendFactor = 1;
    this->m_caps.m_stereoAvailable = 0;
    this->m_caps.m_shaderAlphaRef = 1;

    for (int32_t i = 0; i < GxTexFormats_Last; i++) {
        GxMetal::FormatInfo info;
        this->m_caps.m_texFmt[i] = GxMetal::TexFormatInfo(static_cast<EGxTexFormat>(i), info) ? 1 : 0;
        this->m_caps.m_rttFormat[i] = info.m_renderable ? 1 : 0;
    }

    // The client checks these before it sends lights past the fourth (St0ny's #67 uses 8)
    // The translator handles vs_1_1 to vs_3_0 and ps_1_1 to ps_3_0; report the highest profile so the
    // client loads the same .bls set the Windows client does (the Reforged data ships SM3)
    // The translator handles vs_1_1 to vs_3_0 and ps_1_1 to ps_3_0; report the highest profile so the
    // client loads the same .bls set the Windows client does
    // The fixed-function path draws the client's scenes correctly; the .bls shader path is still being
    // debugged, so WRATHMODERN_METAL_SHADERS=0 falls back to the fixed pipeline for comparison
    const char* shaders = getenv("WRATHMODERN_METAL_SHADERS");
    bool useShaders = !(shaders && !strcmp(shaders, "0"));
    this->m_caps.m_shaderTargets[GxSh_Vertex] = useShaders ? 3 : 0;
    this->m_caps.m_shaderTargets[GxSh_Pixel] = useShaders ? 4 : 0;
    this->m_caps.m_shaderConstants[GxSh_Vertex] = 256;
    this->m_caps.m_shaderConstants[GxSh_Pixel] = 256;
}

bool CGxDeviceMetalNative::ICreateWindowTargets(const CGxFormat& format) {
    GxMetal::Clear(this->m_backBuffer);
    GxMetal::Clear(this->m_depthBuffer);

    int32_t width = 0;
    int32_t height = 0;
    DrawableSize(this->m_hwnd, &width, &height);

    if (width <= 0 || height <= 0) {
        return false;
    }

    GxMetal::FormatInfo color;
    GxMetal::FormatInfo depth;

    if (!GxMetal::WindowFormatInfo(format.m_colorFormat, color) || !GxMetal::WindowFormatInfo(format.m_depthFormat, depth)) {
        GxMetal::Log("metal: unsupported window format %d/%d", static_cast<int32_t>(format.m_colorFormat), static_cast<int32_t>(format.m_depthFormat));
        return false;
    }

    uint32_t samples = format.m_multisampleCount > 1 ? format.m_multisampleCount : 1;

    this->m_samples = samples;
    this->m_backBuffer = GxMetal::Texture::CreateTarget(this->m_metal, color, static_cast<uint32_t>(width), static_cast<uint32_t>(height), samples);
    this->m_depthBuffer = GxMetal::Texture::CreateTarget(this->m_metal, depth, static_cast<uint32_t>(width), static_cast<uint32_t>(height), samples);

    if (!this->m_backBuffer || !this->m_depthBuffer) {
        return false;
    }

    // The texture decides the real sample count: a format or size that cannot be multisampled comes
    // back single-sample, and the pipeline must match the attachment
    this->m_samples = this->m_backBuffer->m_samples;

    this->ISetHwTargets(this->m_backBuffer ? GxMetal::Target { this->m_backBuffer, 0, 0 } : GxMetal::Target(), this->m_depthBuffer ? GxMetal::Target { this->m_depthBuffer, 0, 0 } : GxMetal::Target());
    return true;
}

void CGxDeviceMetalNative::ISetHwTargets(const GxMetal::Target& color, const GxMetal::Target& depth) {
    if (this->m_metal) {
        this->m_metal->SetTargets(color, depth);
    }
}

void CGxDeviceMetalNative::IResetDevice() {
    this->m_inScene = 0;

    if (!this->m_metal) {
        return;
    }

    this->ITargetsFromState();
}

int32_t CGxDeviceMetalNative::DeviceCreate(GxWindowProc windowProc, const CGxFormat& format) {
    CGxFormat created = format;

    if (!this->ICreateWindow(created)) {
        return 0;
    }

    int32_t result = this->DeviceCreate(this->m_hwnd, created);
    this->m_format = created;
    return result;
}

int32_t CGxDeviceMetalNative::DeviceCreate(void* window, const CGxFormat& format) {
    this->m_hwnd = window;
    this->m_format = format;

    if (!this->ICreateMetal()) {
        return 0;
    }

    CAMetalLayer* layer = (__bridge CAMetalLayer*)OsMacGetMetalLayer(window);
    this->m_metal->SetLayer(layer);
    // The client drops render states while m_context is 0 (CGxDevice::GxRsSet), so nothing would
    // reach the device at all without this
    this->m_api = GxApi_Metal;
    this->m_context = 1;
    this->m_windowVisible = 1;
    this->m_windowFocus = 1;
    this->ISetCaps(format);

    if (!this->ICreateWindowTargets(format)) {
        return 0;
    }

    this->IResetDevice();
    this->IStateSetDefaults();
    this->DeviceSetGamma(this->m_gammaRamp);
    this->m_metal->SetVSync(format.m_vsync != 0);
    this->ICursorCreate(format);
    GxMetal::Log("metal: window %dx%d, drawing %dx%d, %u samples", format.m_size.x, format.m_size.y, this->m_backBuffer ? this->m_backBuffer->m_width : 0, this->m_backBuffer ? this->m_backBuffer->m_height : 0, this->m_samples);
    return 1;
}

void CGxDeviceMetalNative::DeviceDestroy() {
    this->ICursorDestroy();

    if (this->m_metal) {
        this->m_metal->EndEncoders();
        this->m_metal->Flush(true);
    }

    GxMetal::Clear(this->m_backBuffer);
    GxMetal::Clear(this->m_depthBuffer);
    GxMetal::Clear(this->m_textureDepthBuffer);

    if (this->m_metal) {
        this->m_metal->SetTargets(GxMetal::Target(), GxMetal::Target());
        this->m_metal->SetLayer(nil);
    }

    this->IDestroyMetal();

    if (this->m_hwnd && this->m_ownhwnd) {
        OsMacDestroyGameWindow(this->m_hwnd);
    }

    this->m_hwnd = nullptr;
    this->m_ownhwnd = 0;
    this->m_context = 0;
}

void CGxDeviceMetalNative::DeviceEvictResources() {
    // Nothing cached outside the context yet
}

// As the shim device: the game window is kept and reconfigured for the new format, the targets are
// made again for its size and sample count, and the base device takes the format (window aspect)
int32_t CGxDeviceMetalNative::DeviceSetFormat(const CGxFormat& format) {
    this->m_format = format;

    if (!this->m_metal) {
        return 0;
    }

    // Commands in flight still use the old targets; the saved depth target and the texture targets'
    // depth buffer belong to the old size and sample count
    this->m_metal->EndEncoders();
    this->m_metal->Flush(true);
    this->m_savedDepthTarget = GxMetal::Target();
    GxMetal::Clear(this->m_textureDepthBuffer);
    this->m_inScene = 0;

    CGxFormat createFormat = format;

    if (!this->ICreateWindow(createFormat) || !this->ICreateWindowTargets(createFormat) || !CGxDevice::DeviceSetFormat(createFormat)) {
        GxMetal::Log("metal: unable to set format %dx%d, %u samples", createFormat.m_size.x, createFormat.m_size.y, createFormat.m_multisampleCount);
        return 0;
    }

    this->m_format = createFormat;
    this->ISetCaps(createFormat);
    this->m_metal->SetVSync(createFormat.m_vsync != 0);
    this->ICursorCreate(createFormat);
    OsGuiSetGxWindow(this->m_hwnd);
    this->m_windowFocus = 1;
    this->m_viewportDirty = 1;
    GxMetal::Log("metal: window %dx%d, drawing %dx%d, %u samples", createFormat.m_size.x, createFormat.m_size.y, this->m_backBuffer ? this->m_backBuffer->m_width : 0, this->m_backBuffer ? this->m_backBuffer->m_height : 0, this->m_samples);
    return 1;
}

void* CGxDeviceMetalNative::DeviceWindow() {
    return this->m_hwnd;
}

void CGxDeviceMetalNative::CapsWindowSize(CRect& rect) {
    rect = this->DeviceCurWindow();
}

void CGxDeviceMetalNative::CapsWindowSizeInScreenCoords(CRect& rect) {
    rect = this->DeviceCurWindow();
}

// As CGxDeviceD3d::DeviceSetRenderTarget, a target that was not made yet is made now; otherwise
// the window's buffers would stand in for it (a shadow map's colour would become the back buffer)
GxMetal::Target CGxDeviceMetalNative::ITexTarget(CGxTex* texId, uint32_t face, uint32_t level) {
    if (!texId) {
        return GxMetal::Target();
    }

    if (texId->m_needsCreation || (!texId->m_apiSpecificData && !texId->m_apiSpecificData2)) {
        this->ITexCreate(texId);
    }

    if (texId->m_needsCreation || !texId->m_apiSpecificData) {
        return GxMetal::Target();
    }

    return GxMetal::Target { static_cast<GxMetal::Texture*>(texId->m_apiSpecificData), face, level };
}

// The colour and depth targets the client set: its own texture targets where it set one, the
// window's buffers otherwise (CGxDeviceD3d's m_defColorSurface / m_defDepthStencilSurface)
void CGxDeviceMetalNative::ITargetsFromState() {
    GxMetal::Target color = this->ITexTarget(this->m_textureTarget[GxBuffers_Color].m_texture, this->m_textureTarget[GxBuffers_Color].m_plane, 0);
    GxMetal::Target depth = this->ITexTarget(this->m_textureTarget[GxBuffers_Depth].m_texture, this->m_textureTarget[GxBuffers_Depth].m_plane, 0);

    if (!color.m_texture && this->m_backBuffer) {
        color = GxMetal::Target { this->m_backBuffer, 0, 0 };
    }

    if (!depth.m_texture) {
        depth = color.m_texture && color.m_texture != this->m_backBuffer ? this->ITextureDepth(color) : (this->m_depthBuffer ? GxMetal::Target { this->m_depthBuffer, 0, 0 } : GxMetal::Target());
    }

    this->ISetHwTargets(color, depth);
}

// A texture target has its own size and one sample; the window's depth buffer cannot be used with it
GxMetal::Target CGxDeviceMetalNative::ITextureDepth(const GxMetal::Target& color) {
    if (!color.m_texture || color.Width() == 0 || color.Height() == 0) {
        return GxMetal::Target();
    }

    GxMetal::FormatInfo depthFormat;

    if (!GxMetal::WindowFormatInfo(this->m_format.m_depthFormat, depthFormat)) {
        return GxMetal::Target();
    }

    if (!this->m_textureDepthBuffer || this->m_textureDepthBuffer->m_width != color.Width() || this->m_textureDepthBuffer->m_height != color.Height()) {
        GxMetal::Clear(this->m_textureDepthBuffer);
        this->m_textureDepthBuffer = GxMetal::Texture::CreateTarget(this->m_metal, depthFormat, color.Width(), color.Height(), 1);
    }

    return this->m_textureDepthBuffer ? GxMetal::Target { this->m_textureDepthBuffer, 0, 0 } : GxMetal::Target();
}

void CGxDeviceMetalNative::DeviceSetRenderTarget(EGxBuffer buffer, CGxTex* texture, uint32_t plane) {
    if (!this->m_metal) {
        return;
    }

    CGxDevice::TextureTarget& target = this->m_textureTarget[buffer];

    if (target.m_texture == texture && target.m_plane == plane) {
        return;
    }

    CGxDevice::DeviceSetRenderTarget(buffer, texture, plane);
    this->ITargetsFromState();

    // The current window changed with the target; viewport and scissor follow it (CGxDeviceD3d)
    this->m_viewportDirty = 1;
    this->m_scissorRectDirty = 1;
}

void CGxDeviceMetalNative::DeviceSaveDepthSurface() {
    if (this->m_metal) {
        this->m_savedDepthTarget = this->m_metal->DepthTarget();
    }
}

void CGxDeviceMetalNative::DeviceRestoreDepthSurface() {
    if (this->m_metal) {
        this->m_metal->SetTargets(this->m_metal->ColorTarget(), this->m_savedDepthTarget);
    }
}

int32_t CGxDeviceMetalNative::DeviceWM(EGxWM wm, uintptr_t param1, uintptr_t param2) {
    switch (wm) {
        case GxWM_Size: {
            if (param2 == 1 || param2 == 2) {
                this->m_windowVisible = 0;
                break;
            }

            this->m_windowVisible = 1;

            if (!param1) {
                return 0;
            }

            // The client sizes its viewport and screenshots from this rectangle
            this->DeviceSetDefWindow(*reinterpret_cast<const CRect*>(param1));

            if (this->m_metal && this->m_context) {
                this->ICreateWindowTargets(this->m_format);
            }

            this->m_viewportDirty = 1;
            break;
        }
        case GxWM_DisplayChange:
            if (this->m_windowVisible && param1) {
                this->DeviceSetDefWindow(*reinterpret_cast<const CRect*>(param1));
                this->m_viewportDirty = 1;
            }

            break;

        case GxWM_SetFocus:
            this->m_windowFocus = 1;
            break;

        case GxWM_Destroy:
        case GxWM_KillFocus:
            this->m_windowFocus = 0;
            break;

        default:
            break;
    }

    return 0;
}

// The window procedure of Windows reports size changes; here they are polled once a frame
void CGxDeviceMetalNative::IPollWindow() {
    if (!this->m_hwnd || !this->m_ownhwnd) {
        return;
    }

    int32_t width;
    int32_t height;
    OsMacGetClientSize(this->m_hwnd, &width, &height);

    const CRect& window = this->DeviceDefWindow();

    if (width > 0 && height > 0 && (static_cast<float>(width) != window.maxX || static_cast<float>(height) != window.maxY)) {
        CRect rect;
        rect.minX = 0.0f;
        rect.minY = 0.0f;
        rect.maxX = static_cast<float>(width);
        rect.maxY = static_cast<float>(height);
        this->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), 0);
    }

    this->IUpdateCursor();
}

void CGxDeviceMetalNative::SceneClear(uint32_t mask, CImVector color) {
    if (!this->m_metal || !this->m_backBuffer) {
        return;
    }

    // As CGxDeviceD3d::SceneClear: Direct3D 9 clears the viewport of the current target, as IXformSetViewport
    // places it (a texture target keeps its y unflipped). The shadow cache clears its bands this way, one tile
    // of an atlas at a time; clearing the window's size instead left the rows of a texture taller than the
    // window uncleared (depth 0, shadowed everywhere)
    const GxMetal::Target& target = this->m_metal->ColorTarget();
    if (!target.m_texture) {
        return;
    }

    const CRect& window = this->DeviceCurWindow();
    if (window.maxX <= 0.0f || window.maxY <= 0.0f) {
        return;
    }

    bool textureTarget = this->m_textureTarget[GxBuffers_Color].m_texture || this->m_textureTarget[GxBuffers_Depth].m_texture;
    float x = static_cast<float>(static_cast<int32_t>(this->m_viewport.m_x.m_l * window.maxX + 0.5f));
    float flippedY = static_cast<float>(static_cast<int32_t>((1.0f - this->m_viewport.m_y.m_h) * window.maxY + 0.5f));
    float w = static_cast<float>(static_cast<int32_t>(this->m_viewport.m_x.m_h * window.maxX - x + 0.5f));
    float h = static_cast<float>(static_cast<int32_t>((1.0f - this->m_viewport.m_y.m_l) * window.maxY - flippedY + 0.5f));
    float y = textureTarget ? static_cast<float>(static_cast<int32_t>(window.maxY * this->m_viewport.m_y.m_l)) : flippedY;

    // The window's rectangle may be in points; the target is in pixels
    float scaleX = target.Width() / window.maxX;
    float scaleY = target.Height() / window.maxY;

    // CImVector is the client's 8-bit b, g, r, a; Metal wants 0-1 r, g, b, a
    float rgba[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
    GxMetal::Rect area = {
        static_cast<int32_t>(x * scaleX + 0.5f),
        static_cast<int32_t>(y * scaleY + 0.5f),
        static_cast<int32_t>((x + w) * scaleX + 0.5f),
        static_cast<int32_t>((y + h) * scaleY + 0.5f),
    };
    this->m_metal->Clear(area, (mask & 0x1) != 0, (mask & 0x2) != 0, rgba, 1.0f);
}

void CGxDeviceMetalNative::ISceneBegin() {
    if (!this->m_metal) {
        return;
    }

    // The constants of the frame start empty, as on the shim device
    this->m_inScene = 1;
}

void CGxDeviceMetalNative::ISceneEnd() {
    this->m_inScene = 0;
}

void CGxDeviceMetalNative::ScenePresent() {
    if (!this->m_metal) {
        return;
    }

    // The client requests a screenshot by setting m_scrShotClick before presenting; the pixels are
    // read from the back buffer before it goes to the layer (CGxDeviceMetal.cpp:2906)
    int32_t screenShotClick = this->m_scrShotClick;
    CGxDevice::ScenePresent();
    this->ISceneEnd();
    this->ICursorDraw();

    if (screenShotClick) {
        this->DeviceScreenShot();
    }

    this->WaitForFPSCap();
    this->m_metal->MarkTargetsRendered();
    this->m_metal->Present(this->m_backBuffer);

    this->IPollWindow();
}

void CGxDeviceMetalNative::DeviceSetGamma(float gamma) {
    if (!this->m_metal) {
        return;
    }

    CGxGammaRamp ramp;
    ramp.Set(gamma);
    this->m_metal->SetGammaRamp(ramp.m_red, ramp.m_green, ramp.m_blue);
}

void CGxDeviceMetalNative::DeviceSetGamma(const CGxGammaRamp& ramp) {
    if (this->m_metal) {
        this->m_metal->SetGammaRamp(ramp.m_red, ramp.m_green, ramp.m_blue);
    }
}

void CGxDeviceMetalNative::DeviceReadPixels(CiRect& rect, TSGrowableArray<CImVector>& pixels) {
    int32_t minX = rect.minX;
    int32_t minY = rect.minY;
    int32_t maxX = rect.maxX;
    int32_t maxY = rect.maxY;

    if (maxX <= minX || maxY <= minY) {
        return;
    }

    pixels.SetCount(static_cast<uint32_t>((maxX - minX) * (maxY - minY)));

    if (!this->m_metal || !this->m_backBuffer) {
        memset(pixels.m_data, 0, pixels.m_count * sizeof(CImVector));
        return;
    }

    GxMetal::Rect area = { minX, minY, maxX, maxY };
    std::vector<uint8_t> bytes;
    uint32_t pitch = 0;

    if (!this->m_metal->Read(GxMetal::Target { this->m_backBuffer, 0, 0 }, area, bytes, pitch)) {
        memset(pixels.m_data, 0, pixels.m_count * sizeof(CImVector));
        return;
    }

    // The back buffer is BGRA8, which is CImVector's byte order
    for (int32_t y = 0; y < maxY - minY; y++) {
        memcpy(pixels.m_data + y * (maxX - minX), bytes.data() + y * pitch, (maxX - minX) * sizeof(CImVector));
    }
}

// As the Direct3D device, which has no readable depth: the rectangle's depths read as 0
void CGxDeviceMetalNative::DeviceReadDepths(CiRect& rect, TSGrowableArray<float>& depths) {
    this->ClampRectToWindow(rect);

    uint32_t count = (rect.maxY - rect.minY) * (rect.maxX - rect.minX);
    depths.SetCount(count);
    memset(depths.Ptr(), 0, sizeof(float) * count);
}

// The client's texture formats, substituted before creation and retried with a tolerable one when the
// GPU cannot make it (CGxDeviceMetal.cpp:30 and :56; the first table is deliberately mutable)
EGxTexFormat CGxDeviceMetalNative::s_GxTexFormatToUse[GxTexFormats_Last] = { GxTex_Unknown, GxTex_Abgr8888, GxTex_Argb8888, GxTex_Argb4444, GxTex_Argb1555, GxTex_Rgb565, GxTex_Dxt1, GxTex_Dxt3, GxTex_Dxt5, GxTex_Uv88, GxTex_Gr1616F, GxTex_R32F, GxTex_D24X8 };

const EGxTexFormat CGxDeviceMetalNative::s_tolerableTexFmtMapping[GxTexFormats_Last] = { GxTex_Unknown, GxTex_Argb4444, GxTex_Argb4444, GxTex_Argb4444, GxTex_Argb4444, GxTex_Argb4444, GxTex_Dxt1, GxTex_Dxt3, GxTex_Dxt5, GxTex_Uv88, GxTex_Gr1616F, GxTex_R32F, GxTex_D24X8 };

GxMetal::Texture*& CGxDeviceMetalNative::ITexData(CGxTex* texId) {
    return *reinterpret_cast<GxMetal::Texture**>(&texId->m_apiSpecificData);
}

// The texture a CGxTex holds after ITexForceRecreation: a valid object the client can keep bound
// (opaque green, as the shim's 8x8 m_texture3B58), re-uploaded on its next use
GxMetal::Texture* CGxDeviceMetalNative::IDefaultTexture() {
    if (!this->m_defaultTexture && this->m_metal) {
        GxMetal::FormatInfo info;

        if (GxMetal::TexFormatInfo(GxTex_Argb8888, info)) {
            this->m_defaultTexture = GxMetal::Texture::Create(this->m_metal, MTLTextureType2D, 8, 8, 1, info, false, false);
        }

        if (this->m_defaultTexture) {
            GxMetal::UploadAllocation staging = this->m_metal->AllocateUpload(8 * 8 * 4, 16);

            for (uint32_t i = 0; i < 64; i++) {
                memcpy(staging.m_data + i * 4, "\x00\xFF\x00\xFF", 4);
            }

            this->m_metal->CopyToTexture(staging, 32, 256, this->m_defaultTexture->m_texture, 0, 0, MTLOriginMake(0, 0, 0), MTLSizeMake(8, 8, 1), this->m_defaultTexture->m_use);
        }
    }

    return this->m_defaultTexture;
}

// CGxDeviceMetal.cpp:2527 - create the GPU texture of a client texture (CFR: ITexWHDStartEnd decides
// the level count; the base mip is already folded into width and height)
void CGxDeviceMetalNative::ITexCreate(CGxTex* texId) {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t baseMip = 0;
    uint32_t mipCount = 0;
    this->ITexWHDStartEnd(texId, width, height, baseMip, mipCount);

    texId->m_format = s_GxTexFormatToUse[texId->m_format];

    GxMetal::FormatInfo info;

    if (!GxMetal::TexFormatInfo(texId->m_format, info)) {
        return;
    }

    bool renderTarget = texId->m_flags.m_renderTarget != 0;
    bool autoMips = texId->m_flags.m_generateMipMaps != 0;
    GxMetal::Texture* texture = nullptr;

    if (info.m_depth) {
        texture = GxMetal::Texture::Create(this->m_metal, MTLTextureType2D, width, height, 1, info, true, false);
    } else {
        uint32_t levels = mipCount > baseMip ? mipCount - baseMip : 1;
        MTLTextureType type = texId->m_target == GxTex_CubeMap ? MTLTextureTypeCube : MTLTextureType2D;
        texture = GxMetal::Texture::Create(this->m_metal, type, width, height, levels, info, renderTarget, autoMips);

        if (!texture && !renderTarget) {
            // The GPU cannot make the format: use the client's tolerable one for every texture after
            // this, exactly as the shim does
            s_GxTexFormatToUse[texId->m_format] = s_tolerableTexFmtMapping[texId->m_format];
            texId->m_format = s_GxTexFormatToUse[texId->m_format];

            if (!GxMetal::TexFormatInfo(texId->m_format, info)) {
                return;
            }

            texture = GxMetal::Texture::Create(this->m_metal, type, width, height, levels, info, false, autoMips);
        }
    }

    if (!texture) {
        return;
    }

    GxMetal::Assign(this->ITexData(texId), texture);
    texture->Release();
    texId->m_needsCreation = 0;
}

// CGxDeviceMetal.cpp:2585 - upload the updated rectangle into every level and cube face; the client's
// user function hands out the texels through GxTex_Lock/Latch/Unlock
void CGxDeviceMetalNative::ITexUpload(CGxTex* texId) {
    GxMetal::Texture* texture = this->ITexData(texId);

    if (!texture || !texId->m_userFunc) {
        return;
    }

    uint32_t texelStrideInBytes = 0;
    const void* texels = nullptr;
    texId->m_userFunc(GxTex_Lock, texId->m_width, texId->m_height, 0, 0, texId->m_userArg, texelStrideInBytes, texels);

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t baseMip = 0;
    uint32_t mipCount = 0;
    this->ITexWHDStartEnd(texId, width, height, baseMip, mipCount);

    bool compressedData = texId->m_dataFormat >= GxTex_Dxt1 && texId->m_dataFormat <= GxTex_Dxt5;
    bool renderTarget = texId->m_flags.m_renderTarget != 0;
    uint32_t faceCount = texId->m_target == GxTex_CubeMap ? 6 : 1;
    const GxMetal::FormatInfo& info = texture->m_format;

    for (uint32_t face = 0; face < faceCount; face++) {
        uint32_t mipWidth = width;
        uint32_t mipHeight = height;
        uint32_t level = 0;

        for (uint32_t mip = baseMip; mip != mipCount; mip++, level++) {
            texels = nullptr;
            texId->m_userFunc(GxTex_Latch, mipWidth, mipHeight, face, mip, texId->m_userArg, texelStrideInBytes, texels);

            if (texels && !renderTarget) {
                int32_t left = texId->m_updateRect.minX >> mip;
                int32_t top = texId->m_updateRect.minY >> mip;
                int32_t right = texId->m_updateRect.maxX >> mip;
                int32_t bottom = texId->m_updateRect.maxY >> mip;

                if (right <= left) {
                    right = left + 1;
                }

                if (bottom <= top) {
                    bottom = top + 1;
                }

                if (texId->m_format >= GxTex_Dxt1 && texId->m_format <= GxTex_Dxt5) {
                    left &= ~3;
                    top &= ~3;
                    right = (right + 3) & ~3;
                    bottom = (bottom + 3) & ~3;

                    if (right > static_cast<int32_t>(mipWidth)) {
                        right = static_cast<int32_t>(mipWidth);
                    }

                    if (bottom > static_cast<int32_t>(mipHeight)) {
                        bottom = static_cast<int32_t>(mipHeight);
                    }
                }

                const uint8_t* source = static_cast<const uint8_t*>(texels);

                if (!texId->m_flags.m_bit15) {
                    if (compressedData) {
                        source += s_texFormatBytesPerBlock[texId->m_dataFormat] * (left >> 2) + texelStrideInBytes * (top >> 2);
                    } else {
                        source += ((left * s_texFormatBitDepth[texId->m_dataFormat]) >> 3) + texelStrideInBytes * top;
                    }
                }

                uint32_t copyWidth = static_cast<uint32_t>(right - left);
                uint32_t copyHeight = static_cast<uint32_t>(bottom - top);
                uint32_t pitch = info.RowPitch(copyWidth);
                uint32_t rows = info.RowCount(copyHeight);
                GxMetal::UploadAllocation staging = this->m_metal->AllocateUpload(pitch * rows, 16);

                C2iVector size;
                size.x = static_cast<int32_t>(copyWidth);
                size.y = static_cast<int32_t>(copyHeight);

                if (Blit(size, BlitAlpha_0, source, texelStrideInBytes, GxGetBlitFormat(texId->m_dataFormat), staging.m_data, pitch, GxGetBlitFormat(texId->m_format))) {
                    this->m_metal->CopyToTexture(staging, pitch, pitch * rows, texture->m_texture, face, level, MTLOriginMake(left, top, 0), MTLSizeMake(copyWidth, copyHeight, 1), texture->m_use);
                    texture->MarkWritten(level);
                } else {
                    static int blitFailures = 0;

                    if (blitFailures < 8) {
                        blitFailures++;
                        GxMetal::Log("upload: Blit failed %d -> %d %ux%u", static_cast<int>(texId->m_dataFormat), static_cast<int>(texId->m_format), copyWidth, copyHeight);
                    }
                }
            }

            mipWidth >>= 1;
            mipHeight >>= 1;

            if (!mipWidth) {
                mipWidth = 1;
            }

            if (!mipHeight) {
                mipHeight = 1;
            }
        }
    }

    texels = nullptr;
    texId->m_userFunc(GxTex_Unlock, texId->m_width, texId->m_height, 0, 0, texId->m_userArg, texelStrideInBytes, texels);
}

// CGxDeviceMetal.cpp:2493 - drop the GPU object of the render targets so they are made again
void CGxDeviceMetalNative::ITexForceRecreation(int32_t all) {
    for (CGxTex* texId = this->m_textures.Head(); texId; texId = this->m_textures.Next(texId)) {
        if (!texId->m_apiSpecificData || !(all || texId->m_flags.m_renderTarget)) {
            continue;
        }

        CiRect updateRect;

        if (!texId->m_needsCreation && (texId->m_apiSpecificData || texId->m_apiSpecificData2)) {
            GxMetal::Clear(this->ITexData(texId));
        }

        texId->m_apiSpecificData = this->IDefaultTexture();
        texId->m_needsCreation = 1;
        this->TexMarkForUpdate(texId, updateRect, 0);

        uint32_t texelStrideInBytes = 0;
        const void* texels = nullptr;

        if (texId->m_userFunc) {
            texId->m_userFunc(GxTex_Recreate, texId->m_width, texId->m_height, 0, 0, texId->m_userArg, texelStrideInBytes, texels);
        }
    }

    if (this->m_hwnd) {
        this->NotifyOnTextureRecreation();
    }
}

void CGxDeviceMetalNative::ITexMarkAsUpdated(CGxTex* texId) {
    if (!texId->m_needsUpdate || !this->m_metal) {
        return;
    }

    if (texId->m_needsCreation || (!texId->m_apiSpecificData && !texId->m_apiSpecificData2)) {
        this->ITexCreate(texId);
    }

    if (!texId->m_needsCreation && (texId->m_apiSpecificData || texId->m_apiSpecificData2)) {
        this->ITexUpload(texId);
        CGxDevice::ITexMarkAsUpdated(texId);
    }
}

void CGxDeviceMetalNative::TexDestroy(CGxTex* texId) {
    if (!texId->m_needsCreation && (texId->m_apiSpecificData || texId->m_apiSpecificData2)) {
        GxMetal::Clear(this->ITexData(texId));
    }

    CGxDevice::TexDestroy(texId);
}

void CGxDeviceMetalNative::DeviceCopyTex(CGxTex* source, uint32_t sourcePlane, CGxTex* dest, uint32_t destPlane) {
    if (!this->m_metal) {
        return;
    }

    GxMetal::Texture* sourceTexture = nullptr;
    GxMetal::Texture* destTexture = nullptr;

    if (source) {
        if (source->m_needsCreation || (!source->m_apiSpecificData && !source->m_apiSpecificData2)) {
            this->ITexCreate(source);
        }

        if (!source->m_needsCreation) {
            sourceTexture = this->ITexData(source);
        }
    }

    if (dest) {
        if (dest->m_needsCreation || (!dest->m_apiSpecificData && !dest->m_apiSpecificData2)) {
            this->ITexCreate(dest);
        }

        if (!dest->m_needsCreation) {
            destTexture = this->ITexData(dest);
        }
    }

    if (!sourceTexture || !destTexture) {
        return;
    }

    GxMetal::Target from = { sourceTexture, source->m_target == GxTex_CubeMap ? sourcePlane : 0, 0 };
    GxMetal::Target to = { destTexture, dest->m_target == GxTex_CubeMap ? destPlane : 0, 0 };
    this->m_metal->Stretch(from, nullptr, to, nullptr, false);
}

void CGxDeviceMetalNative::DeviceResolveDepthBuffer(CGxTex* texture) {
    if (!this->m_metal) {
        return;
    }

    if (texture && (texture->m_needsCreation || (!texture->m_apiSpecificData && !texture->m_apiSpecificData2))) {
        this->ITexCreate(texture);
    }

    GxMetal::Texture* depth = texture ? this->ITexData(texture) : nullptr;

    if (!depth || !depth->m_format.m_depth) {
        return;
    }

    // The colour target stays what it was; the texture becomes the depth target
    this->ISetHwTargets(this->m_metal->ColorTarget(), GxMetal::Target { depth, 0, 0 });
}

void CGxDeviceMetalNative::DeviceSetBaseMipLevel(uint32_t level) {
    // The recreation is lazy (only m_needsCreation is set) and ITexCreate reads the level later
    this->ITexForceRecreation(1);
    CGxDevice::DeviceSetBaseMipLevel(level);
}

void CGxDeviceMetalNative::IShaderCreate(CGxShader* shader) {
    if (shader->m_target == GxSh_Vertex) {
        this->IShaderCreateVertex(shader);
    } else if (shader->m_target == GxSh_Pixel) {
        this->IShaderCreatePixel(shader);
    }
}

void CGxDeviceMetalNative::IShaderCreateVertex(CGxShader* shader) {
    shader->m_valid = 0;

    if (!this->m_metal) {
        return;
    }

    shader->m_loaded = 1;

    if (shader->m_code.Count()) {
        GxMetal::Shader* created = GxMetal::Shader::Create(reinterpret_cast<const uint32_t*>(shader->m_code.m_data), shader->m_code.Count(), true);

        if (created) {
            shader->m_apiSpecific = created;
            shader->m_valid = 1;
        }
    }
}

void CGxDeviceMetalNative::IShaderCreatePixel(CGxShader* shader) {
    shader->m_valid = 0;

    if (!this->m_metal) {
        return;
    }

    shader->m_loaded = 1;

    if (shader->m_code.Count()) {
        GxMetal::Shader* created = GxMetal::Shader::Create(reinterpret_cast<const uint32_t*>(shader->m_code.m_data), shader->m_code.Count(), false);

        if (created) {
            shader->m_apiSpecific = created;
            shader->m_valid = 1;
        }
    }
}

void CGxDeviceMetalNative::ShaderCreate(CGxShader** shaders, EGxShTarget target, const char* path, const char* name, uint32_t permutations) {
    CGxDevice::ShaderCreate(shaders, target, path, name, permutations);

    // Only a single-permutation shader is compiled up front; the rest wait for their first bind
    if (permutations == 1 && !shaders[0]->m_loaded) {
        this->IShaderCreate(shaders[0]);
    }
}

void CGxDeviceMetalNative::ShaderDestroy(CGxShader*& shader) {
    if (!shader) {
        return;
    }

    if (shader->m_refCount == 1 && shader->m_apiSpecific) {
        GxMetal::Shader* native = static_cast<GxMetal::Shader*>(shader->m_apiSpecific);

        if (this->m_hw.m_vertexShader == native) {
            GxMetal::Clear(this->m_hw.m_vertexShader);
        }

        if (this->m_hw.m_pixelShader == native) {
            GxMetal::Clear(this->m_hw.m_pixelShader);
        }

        native->Release();
        shader->m_apiSpecific = nullptr;
        shader->m_loaded = 0;
    }

    CGxDevice::ShaderDestroy(shader);
}

void CGxDeviceMetalNative::IShaderReload(CGxShader* shader, const char* path, const char* name) {
    if (!shader || shader->m_refCount != 1) {
        return;
    }

    if (shader->m_apiSpecific) {
        static_cast<GxMetal::Shader*>(shader->m_apiSpecific)->Release();
        shader->m_apiSpecific = nullptr;
        shader->m_loaded = 0;
    }

    EGxShTarget target = shader->m_target == GxSh_Vertex ? GxSh_Vertex : GxSh_Pixel;
    this->IShaderLoad(&shader, target, path, name, 1);
    this->IShaderCreate(shader);
}

void CGxDeviceMetalNative::IBindVertexShader(CGxShader* shader) {
    GxMetal::Shader* native = nullptr;

    if (shader) {
        if (!shader->m_loaded) {
            this->IShaderCreateVertex(shader);
        }

        native = shader->m_valid ? static_cast<GxMetal::Shader*>(shader->m_apiSpecific) : nullptr;
    }

    if (native != this->m_hw.m_vertexShader) {
        GxMetal::Assign(this->m_hw.m_vertexShader, native);
        this->m_fixedVertexDirty = 1;
        this->m_fixedKeysDirty = 1;
        this->m_vsConstantsDirty = 1;
        this->m_pipelineDirty = 1;
    }

    this->m_hw.m_textures[0] = this->m_hw.m_textures[0];
}

void CGxDeviceMetalNative::IBindPixelShader(CGxShader* shader) {
    GxMetal::Shader* native = nullptr;

    if (shader) {
        if (!shader->m_loaded) {
            this->IShaderCreatePixel(shader);
        }

        native = shader->m_valid ? static_cast<GxMetal::Shader*>(shader->m_apiSpecific) : nullptr;
    }

    if (native != this->m_hw.m_pixelShader) {
        GxMetal::Assign(this->m_hw.m_pixelShader, native);
        this->m_psConstantsDirty = 1;
        this->m_pixelStateDirty = 1;
        this->m_stagesDirty = 1;
        this->m_pipelineDirty = 1;
    }
}

// CGxDeviceMetal.cpp:1929 - the client's dirty constant ranges, index 1 is the vertex file and
// index 0 the pixel file
void CGxDeviceMetalNative::IShaderConstantsFlush() {
    ShadowConstants& vertex = CGxDevice::s_shadowConstants[1];

    if (vertex.dirtyMin <= vertex.dirtyMax && vertex.dirtyMax < 256) {
        uint32_t count = vertex.dirtyMax - vertex.dirtyMin + 1;
        memcpy(&this->m_hw.m_vsConstants[vertex.dirtyMin * 4], &vertex.constants[vertex.dirtyMin * 4], count * 16);
        this->m_vsConstantsDirty = 1;
    }

    vertex.dirtyMin = 255;
    vertex.dirtyMax = 0;

    ShadowConstants& pixel = CGxDevice::s_shadowConstants[0];

    if (pixel.dirtyMin <= pixel.dirtyMax && pixel.dirtyMax < 256) {
        uint32_t count = pixel.dirtyMax - pixel.dirtyMin + 1;
        memcpy(&this->m_hw.m_psConstants[pixel.dirtyMin * 4], &pixel.constants[pixel.dirtyMin * 4], count * 16);
        this->m_psConstantsDirty = 1;
    }

    pixel.dirtyMin = 255;
    pixel.dirtyMax = 0;
}

// The vertex layout of the draw: one per predefined format, or a cached custom one built from the
// client's attribute declarations (CGxDeviceMetal.cpp:1697)
GxMetal::VertexLayout* CGxDeviceMetalNative::ILayoutFor() {
    if (this->m_primVertexFormat < GxVertexBufferFormats_Last) {
        GxMetal::VertexLayout* layout = this->m_formatLayouts[this->m_primVertexFormat];

        if (layout) {
            this->m_hw.m_layout = layout;
            return layout;
        }
    }

    GxMetal::VertexElement elements[GxVAs_Last];
    CGxBuf* streamBufs[16] = {};
    uint32_t count = 0;
    uint32_t streamCount = 0;

    for (uint32_t i = 0; i < GxVAs_Last; i++) {
        if (!(this->m_primVertexMask & (1 << i))) {
            continue;
        }

        CGxBuf* buffer = this->m_primVertexFormatBuf[i];

        if (!buffer) {
            continue;
        }

        uint32_t stream = 0;

        for (; stream < streamCount; stream++) {
            if (streamBufs[stream] == buffer) {
                break;
            }
        }

        if (stream == streamCount) {
            streamBufs[streamCount++] = buffer;
        }

        const CGxVertexAttrib& attrib = this->m_primVertexFormatAttrib[i];
        elements[count].m_attribute = attrib.m_attrib;
        elements[count].m_stream = stream;
        elements[count].m_offset = attrib.m_offset;
        elements[count].m_type = attrib.m_type;
        count++;
    }

    for (uint32_t i = 0; i < this->m_customLayouts.Count(); i++) {
        if (this->m_customLayouts.m_data[i]->Matches(elements, count)) {
            this->m_hw.m_layout = this->m_customLayouts.m_data[i];
            return this->m_customLayouts.m_data[i];
        }
    }

    GxMetal::VertexLayout* layout = new GxMetal::VertexLayout();

    if (!layout->Init(elements, count, this->m_pipelines->NextId())) {
        delete layout;
        return nullptr;
    }

    uint32_t index = this->m_customLayouts.Count();
    this->m_customLayouts.SetCount(index + 1);
    this->m_customLayouts.m_data[index] = layout;

    if (this->m_primVertexFormat < GxVertexBufferFormats_Last) {
        this->m_formatLayouts[this->m_primVertexFormat] = layout;
    }

    this->m_hw.m_layout = layout;
    return layout;
}

void CGxDeviceMetalNative::IReleaseShaders(int32_t freeShaders) {
    for (uint32_t list = 0; list < 2; list++) {
        TSHashTable<CGxShader, HASHKEY_STRI>& table = this->m_shaderList[list == 0 ? GxSh_Vertex : GxSh_Pixel];

        for (CGxShader* shader = table.m_fulllist.Head(); shader; shader = table.m_fulllist.Next(shader)) {
            if (shader->m_apiSpecific) {
                static_cast<GxMetal::Shader*>(shader->m_apiSpecific)->Release();
                shader->m_apiSpecific = nullptr;
                shader->m_loaded = 0;
            }
        }
    }

    GxMetal::Clear(this->m_hw.m_vertexShader);
    GxMetal::Clear(this->m_hw.m_pixelShader);
}

void CGxDeviceMetalNative::IReleaseLayouts() {
    for (uint32_t i = 0; i < GxVertexBufferFormats_Last; i++) {
        delete this->m_formatLayouts[i];
        this->m_formatLayouts[i] = nullptr;
    }

    for (uint32_t i = 0; i < this->m_customLayouts.Count(); i++) {
        delete this->m_customLayouts.m_data[i];
        this->m_customLayouts.m_data[i] = nullptr;
    }

    this->m_customLayouts.SetCount(0);
    this->m_hw.m_layout = nullptr;
}

namespace {

// The client's colours are Direct3D's 0xAARRGGBB
void ColorFloats(uint32_t color, float out[4]) {
    out[0] = ((color >> 16) & 0xFF) * (1.0f / 255.0f);
    out[1] = ((color >> 8) & 0xFF) * (1.0f / 255.0f);
    out[2] = (color & 0xFF) * (1.0f / 255.0f);
    out[3] = ((color >> 24) & 0xFF) * (1.0f / 255.0f);
}

// EGxBlend, in the old device's order (CGxDeviceMetal.cpp:42-44)
const MTLBlendFactor kSrcBlend[12] = {
    MTLBlendFactorOne, MTLBlendFactorOne, MTLBlendFactorSourceAlpha, MTLBlendFactorSourceAlpha,
    MTLBlendFactorDestinationColor, MTLBlendFactorDestinationColor, MTLBlendFactorDestinationColor,
    MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorSourceAlpha,
    MTLBlendFactorOne, MTLBlendFactorBlendColor,
};

const MTLBlendFactor kDstBlend[12] = {
    MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorOne,
    MTLBlendFactorZero, MTLBlendFactorSourceColor, MTLBlendFactorOne,
    MTLBlendFactorOne, MTLBlendFactorZero, MTLBlendFactorZero,
    MTLBlendFactorOne, MTLBlendFactorOneMinusBlendColor,
};

// GxRs_DepthFunc: LessEqual, Equal, GreaterEqual, Less
const MTLCompareFunction kDepthCompare[4] = { MTLCompareFunctionLessEqual, MTLCompareFunctionEqual, MTLCompareFunctionGreaterEqual, MTLCompareFunctionLess };

const MTLCullMode kCullMode[3] = { MTLCullModeNone, MTLCullModeFront, MTLCullModeBack };

// The client's COLORMASK bits are r = 1, b = 2, g = 4, a = 8 (the old device swaps g and b back)
MTLColorWriteMask ColorWriteMask(uint32_t bits) {
    uint32_t mask = 0;

    if (bits & 0x1) {
        mask |= MTLColorWriteMaskRed;
    }

    if (bits & 0x4) {
        mask |= MTLColorWriteMaskGreen;
    }

    if (bits & 0x2) {
        mask |= MTLColorWriteMaskBlue;
    }

    if (bits & 0x8) {
        mask |= MTLColorWriteMaskAlpha;
    }

    return static_cast<MTLColorWriteMask>(mask);
}

// TexGen matrix of ISetTexGen: object and eye linear generation take the inverse view (and world)
template <typename T>
bool Master(T value, EGxMasterEnables which) {
    return (value & (1 << which)) != 0;
}

}

// CGxDeviceMetal.cpp:1402 - one client render state into the native state block. Nothing here
// touches the encoder: IStateSync runs this inside the frame, and opening a pass per state change
// would end and restart the render pass for every draw.
void CGxDeviceMetalNative::IRsSendToHw(EGxRenderState which) {
    if (!this->m_metal) {
        return;
    }

    CGxStateBom& value = this->m_appRenderStates[which].m_value;
    uint32_t master = this->m_appMasterEnables;

    switch (which) {
        case GxRs_PolygonOffset:
            // Direct3D applies the bias after the divide; the fixed vertex shader adds it to z
            this->m_hw.m_depthBias = -value.m_data.f[0];
            this->m_vertexStateDirty = 1;
            break;

        case GxRs_MatDiffuse:
        case GxRs_MatEmissive:
        case GxRs_MatSpecular:
        case GxRs_MatSpecularExp: {
            // All four material states are applied together, as the old device does
            CImVector diffuse;
            CImVector emissive;
            CImVector specular;
            memcpy(&diffuse, &this->m_appRenderStates[GxRs_MatDiffuse].m_value.m_data.u[0], sizeof(diffuse));
            memcpy(&emissive, &this->m_appRenderStates[GxRs_MatEmissive].m_value.m_data.u[0], sizeof(emissive));
            memcpy(&specular, &this->m_appRenderStates[GxRs_MatSpecular].m_value.m_data.u[0], sizeof(specular));
            float power = this->m_appRenderStates[GxRs_MatSpecularExp].m_value.m_data.f[0];
            this->ISetMaterial(diffuse, emissive, specular, power);
            this->m_appRenderStates[GxRs_MatDiffuse].m_dirty = 0;
            this->m_appRenderStates[GxRs_MatEmissive].m_dirty = 0;
            this->m_appRenderStates[GxRs_MatSpecular].m_dirty = 0;
            this->m_appRenderStates[GxRs_MatSpecularExp].m_dirty = 0;
            break;
        }

        case GxRs_NormalizeNormals:
            this->m_hw.m_normalizeNormals = value.m_data.i[0] != 0;
            this->m_fixedKeysDirty = 1;
            break;

        case GxRs_BlendingMode: {
            int32_t mode = value.m_data.i[0];

            if (mode < 2) {
                this->m_hw.m_blendEnable = false;
                this->m_hw.m_srcBlend = MTLBlendFactorOne;
                this->m_hw.m_dstBlend = MTLBlendFactorZero;
            } else {
                int32_t index = mode < 12 ? mode : 11;
                this->m_hw.m_blendEnable = true;
                this->m_hw.m_srcBlend = kSrcBlend[index];
                this->m_hw.m_dstBlend = kDstBlend[index];
            }

            this->m_pipelineDirty = 1;
            break;
        }

        case GxRs_AlphaRef:
            this->m_hw.m_alphaTest = value.m_data.i[0] > 0;
            this->m_hw.m_alphaRef = static_cast<uint32_t>(value.m_data.i[0]) & 0xFF;
            this->m_pixelStateDirty = 1;
            break;

        case GxRs_FogStart:
            this->m_hw.m_fogStart = value.m_data.f[0];
            this->m_fixedVertexDirty = 1;
            break;

        case GxRs_FogEnd:
            this->m_hw.m_fogEnd = value.m_data.f[0];
            this->m_fixedVertexDirty = 1;
            break;

        case GxRs_FogColor:
            ColorFloats(value.m_data.u[0], this->m_hw.m_fogColor);
            this->m_pixelStateDirty = 1;
            break;

        case GxRs_Lighting:
            this->m_hw.m_lighting = Master(master, GxMasterEnable_Lighting) && value.m_data.i[0] != 0;
            this->m_fixedKeysDirty = 1;
            break;

        case GxRs_Fog:
            this->m_hw.m_fog = Master(master, GxMasterEnable_Fog) && value.m_data.i[0] != 0;
            this->m_fixedKeysDirty = 1;
            break;

        case GxRs_DepthTest:
        case GxRs_DepthFunc: {
            bool test = Master(master, GxMasterEnable_DepthTest) && this->m_appRenderStates[GxRs_DepthTest].m_value.m_data.i[0] != 0;
            int32_t func = this->m_appRenderStates[GxRs_DepthFunc].m_value.m_data.i[0];
            func = func < 0 ? 0 : (func > 3 ? 3 : func);
            this->m_hw.m_depthCompare = test ? kDepthCompare[func] : MTLCompareFunctionAlways;
            this->m_depthDirty = 1;
            this->m_appRenderStates[GxRs_DepthTest].m_dirty = 0;
            this->m_appRenderStates[GxRs_DepthFunc].m_dirty = 0;
            break;
        }

        case GxRs_DepthWrite:
            this->m_hw.m_depthWrite = Master(master, GxMasterEnable_DepthWrite) && value.m_data.i[0] != 0;
            this->m_depthDirty = 1;
            break;

        case GxRs_ColorWrite: {
            uint32_t bits = Master(master, GxMasterEnable_ColorWrite) ? value.m_data.u[0] : 0;
            this->m_hw.m_colorWrite = ColorWriteMask(bits);
            this->m_pipelineDirty = 1;
            break;
        }

        case GxRs_Culling: {
            int32_t mode = Master(master, GxMasterEnable_Culling) ? value.m_data.i[0] : 0;
            mode = mode < 0 ? 0 : (mode > 2 ? 2 : mode);
            this->m_hw.m_cullMode = kCullMode[mode];
            break;
        }

        case GxRs_ClipPlaneMask:
            this->m_hw.m_clipPlaneMask = value.m_data.u[0] & 0x3F;
            this->m_vertexStateDirty = 1;
            break;

        case GxRs_Multisample:
            break;

        case GxRs_ScissorTest:
            this->m_hw.m_scissorTest = value.m_data.i[0] != 0;
            this->m_vertexStateDirty = 1;
            break;

        case GxRs_Texture0:
        case GxRs_Texture1:
        case GxRs_Texture2:
        case GxRs_Texture3:
        case GxRs_Texture4:
        case GxRs_Texture5:
        case GxRs_Texture6:
        case GxRs_Texture7:
        case GxRs_Texture8:
        case GxRs_Texture9:
        case GxRs_Texture10:
        case GxRs_Texture11:
        case GxRs_Texture12:
        case GxRs_Texture13:
        case GxRs_Texture14:
        case GxRs_Texture15:
            this->ISetTexture(static_cast<uint32_t>(which - GxRs_Texture0), static_cast<CGxTex*>(value.m_data.p));
            break;

        case GxRs_ColorOp0:
        case GxRs_ColorOp1:
        case GxRs_ColorOp2:
        case GxRs_ColorOp3:
        case GxRs_ColorOp4:
        case GxRs_ColorOp5:
        case GxRs_ColorOp6:
        case GxRs_ColorOp7:
            this->ISetColorOp(static_cast<uint32_t>(which - GxRs_ColorOp0), value.m_data.u[0]);
            break;

        case GxRs_AlphaOp0:
        case GxRs_AlphaOp1:
        case GxRs_AlphaOp2:
        case GxRs_AlphaOp3:
        case GxRs_AlphaOp4:
        case GxRs_AlphaOp5:
        case GxRs_AlphaOp6:
        case GxRs_AlphaOp7:
            this->ISetAlphaOp(static_cast<uint32_t>(which - GxRs_AlphaOp0), value.m_data.u[0]);
            break;

        case GxRs_TexGen0:
        case GxRs_TexGen1:
        case GxRs_TexGen2:
        case GxRs_TexGen3:
        case GxRs_TexGen4:
        case GxRs_TexGen5:
        case GxRs_TexGen6:
        case GxRs_TexGen7:
            this->ISetTexGen(static_cast<uint32_t>(which - GxRs_TexGen0), value.m_data.i[0]);
            break;

        case GxRs_TextureShader0:
        case GxRs_TextureShader1:
        case GxRs_TextureShader2:
        case GxRs_TextureShader3:
        case GxRs_TextureShader4:
        case GxRs_TextureShader5:
        case GxRs_TextureShader6:
        case GxRs_TextureShader7:
            // The old device only reads this in IXformSetTex; a change alone marks the transform dirty
            this->m_xforms[GxXform_Tex0 + (which - GxRs_TextureShader0)].m_dirty = 1;
            break;

        case GxRs_TextureCoord0:
        case GxRs_TextureCoord1:
        case GxRs_TextureCoord2:
        case GxRs_TextureCoord3:
        case GxRs_TextureCoord4:
        case GxRs_TextureCoord5:
        case GxRs_TextureCoord6:
        case GxRs_TextureCoord7: {
            uint32_t tmu = static_cast<uint32_t>(which - GxRs_TextureCoord0);
            this->ISetTexCoord(tmu, value.m_data.u[0]);
            break;
        }

        case GxRs_VertexShader:
            this->IBindVertexShader(static_cast<CGxShader*>(value.m_data.p));
            this->m_pipelineDirty = 1;
            break;

        case GxRs_PixelShader:
            this->IBindPixelShader(static_cast<CGxShader*>(value.m_data.p));
            this->m_pipelineDirty = 1;
            this->m_stagesDirty = 1;
            break;

        case GxRs_PointScale:
            this->m_hw.m_pointSize = value.m_data.f[0];
            this->m_vertexStateDirty = 1;
            break;

        case GxRs_PointScaleAttenuation:
            this->m_hw.m_pointScale = value.m_data.f[0] != 1.0f || value.m_data.f[1] != 0.0f || value.m_data.f[2] != 0.0f;
            this->m_hw.m_pointScaleAttenuation[0] = value.m_data.f[0];
            this->m_hw.m_pointScaleAttenuation[1] = value.m_data.f[1];
            this->m_hw.m_pointScaleAttenuation[2] = value.m_data.f[2];
            this->m_fixedVertexDirty = 1;
            break;

        case GxRs_PointScaleMin: {
            float size = value.m_data.f[0];
            this->m_hw.m_pointSizeMin = size > 1.0f ? size : 1.0f;
            this->m_vertexStateDirty = 1;
            break;
        }

        case GxRs_PointScaleMax: {
            float size = value.m_data.f[0];
            this->m_hw.m_pointSizeMax = size < this->m_caps.m_pointScaleMax ? size : this->m_caps.m_pointScaleMax;
            this->m_vertexStateDirty = 1;
            break;
        }

        case GxRs_PointSprite:
            this->m_hw.m_pointSprite = value.m_data.i[0] != 0;
            this->m_stagesDirty = 1;
            break;

        case GxRs_BlendFactor: {
            uint32_t factor = static_cast<uint32_t>(value.m_data.f[0] * 255.0f);
            ColorFloats((factor << 24) | (factor << 16) | (factor << 8) | factor, this->m_hw.m_blendColor);
            break;
        }

        case GxRs_ColorMaterial:
            this->m_fixedKeysDirty = 1;
            break;

        default:
            break;
    }
}

void CGxDeviceMetalNative::ISetMaterial(CImVector diffuse, CImVector emissive, CImVector specular, float specularExp) {
    this->m_hw.m_materialDiffuse[0] = diffuse.r * (1.0f / 255.0f);
    this->m_hw.m_materialDiffuse[1] = diffuse.g * (1.0f / 255.0f);
    this->m_hw.m_materialDiffuse[2] = diffuse.b * (1.0f / 255.0f);
    this->m_hw.m_materialDiffuse[3] = diffuse.a * (1.0f / 255.0f);

    // Direct3D tracks a separate ambient material; the client never sets one, so it follows the diffuse
    memcpy(this->m_hw.m_materialAmbient, this->m_hw.m_materialDiffuse, sizeof(this->m_hw.m_materialAmbient));
    this->m_hw.m_materialEmissive[0] = emissive.r * (1.0f / 255.0f);
    this->m_hw.m_materialEmissive[1] = emissive.g * (1.0f / 255.0f);
    this->m_hw.m_materialEmissive[2] = emissive.b * (1.0f / 255.0f);
    this->m_hw.m_materialEmissive[3] = emissive.a * (1.0f / 255.0f);
    this->m_hw.m_materialSpecular[0] = specular.r * (1.0f / 255.0f);
    this->m_hw.m_materialSpecular[1] = specular.g * (1.0f / 255.0f);
    this->m_hw.m_materialSpecular[2] = specular.b * (1.0f / 255.0f);
    this->m_hw.m_materialSpecular[3] = specular.a * (1.0f / 255.0f);
    this->m_hw.m_materialPower = specularExp;
    this->m_hw.m_specularEnable = specularExp > 0.0f && (specular.r | specular.g | specular.b) != 0;
    this->m_fixedVertexDirty = 1;
}

void CGxDeviceMetalNative::ISetTexture(uint32_t tmu, CGxTex* texId) {
    if (tmu >= 16) {
        return;
    }

    if (texId) {
        // A texture the client marks for update is created and uploaded before it is drawn; the state
        // keeps the GPU texture, so it must be looked up after the creation, not before
        this->ITexMarkAsUpdated(texId);
    }

    GxMetal::Assign(this->m_hw.m_textures[tmu], texId ? this->ITexData(texId) : nullptr);
    this->m_samplerDirty[tmu] = 1;

    if (texId) {
        if (tmu < 8) {
            HwStage& stage = this->m_hw.m_stages[tmu];
            GxMetal::Texture* texture = this->ITexData(texId);

            this->m_hw.m_samplers[tmu].m_filter = static_cast<uint8_t>(texId->m_flags.m_filter);
            this->m_hw.m_samplers[tmu].m_wrapU = static_cast<uint8_t>(texId->m_flags.m_wrapU);
            this->m_hw.m_samplers[tmu].m_wrapV = static_cast<uint8_t>(texId->m_flags.m_wrapV);
            this->m_hw.m_samplers[tmu].m_maxAnisotropy = static_cast<uint8_t>(texId->m_flags.m_maxAnisotropy > 1 ? texId->m_flags.m_maxAnisotropy : 1);
            this->m_hw.m_samplers[tmu].m_compare = texture && texture->Kind() == GxMetal::SAMPLERKIND_DEPTH ? 1 : 0;

            if (stage.m_colorOp == GxMetal::StageOp_Disable) {
                // A stage that was off and gets a texture takes the client's own operations again
                stage.m_colorOp = static_cast<uint8_t>(this->m_appRenderStates[GxRs_ColorOp0 + tmu].m_value.m_data.u[0]);
                stage.m_alphaOp = static_cast<uint8_t>(this->m_appRenderStates[GxRs_AlphaOp0 + tmu].m_value.m_data.u[0]);
                this->m_appRenderStates[GxRs_ColorOp0 + tmu].m_dirty = 0;
                this->m_appRenderStates[GxRs_AlphaOp0 + tmu].m_dirty = 0;
            }
        }
    } else if (tmu < 8) {
        HwStage& stage = this->m_hw.m_stages[tmu];
        stage.m_colorOp = GxMetal::StageOp_Disable;
        stage.m_alphaOp = GxMetal::StageOp_Disable;
    }

    this->m_samplerDirty[tmu] = 1;
    this->m_stagesDirty = 1;
    this->m_fixedKeysDirty = 1;
}

void CGxDeviceMetalNative::ISetColorOp(uint32_t tmu, uint32_t op) {
    if (tmu < 8) {
        this->m_hw.m_stages[tmu].m_colorOp = static_cast<uint8_t>(op <= GxMetal::StageOp_Disable ? op : GxMetal::StageOp_Disable);
        this->m_stagesDirty = 1;
    }
}

void CGxDeviceMetalNative::ISetAlphaOp(uint32_t tmu, uint32_t op) {
    if (tmu < 8) {
        this->m_hw.m_stages[tmu].m_alphaOp = static_cast<uint8_t>(op <= GxMetal::StageOp_Disable ? op : GxMetal::StageOp_Disable);
        this->m_stagesDirty = 1;
    }
}

void CGxDeviceMetalNative::ISetTexCoord(uint32_t tmu, uint32_t coord) {
    if (tmu < 8) {
        this->ISetTexCoordIndex(tmu, this->m_appRenderStates[GxRs_TexGen0 + tmu].m_value.m_data.i[0], coord);
    }
}

void CGxDeviceMetalNative::ISetTexCoordIndex(uint32_t tmu, int32_t texGen, uint32_t coord) {
    if (tmu >= 8) {
        return;
    }

    HwStage& stage = this->m_hw.m_stages[tmu];

    switch (static_cast<EGxTexGen>(texGen)) {
        case GxTexGen_Disabled:
            stage.m_texGen = GxMetal::TexGen_None;
            stage.m_texcoordIndex = static_cast<uint8_t>(coord & 0x7);
            break;
        case GxTexGen_ObjectLinear:
        case GxTexGen_World:
        case GxTexGen_View:
            stage.m_texGen = GxMetal::TexGen_ViewPosition;
            break;
        case GxTexGen_ReflectionMap:
        case GxTexGen_SphereMap:
            stage.m_texGen = GxMetal::TexGen_ViewReflection;
            break;
        case GxTexGen_Normal:
            stage.m_texGen = GxMetal::TexGen_ViewNormal;
            break;
        default:
            break;
    }

    this->m_fixedKeysDirty = 1;
}

void CGxDeviceMetalNative::ISetTexGen(uint32_t tmu, int32_t texGen) {
    if (tmu >= 8) {
        return;
    }

    this->ISetTexCoordIndex(tmu, texGen, this->m_appRenderStates[GxRs_TextureCoord0 + tmu].m_value.m_data.u[0]);

    CGxMatrixStack& stack = this->m_texGen[tmu];
    uint32_t level = stack.m_level;
    C44Matrix matrix;
    EGxTexGen gen = static_cast<EGxTexGen>(texGen);

    if (gen == GxTexGen_ObjectLinear || gen == GxTexGen_World) {
        matrix = this->m_xforms[GxXform_View].m_mtx[this->m_xforms[GxXform_View].m_level].AffineInverse();

        if (gen == GxTexGen_ObjectLinear) {
            matrix = matrix * this->m_xforms[GxXform_World].m_mtx[this->m_xforms[GxXform_World].m_level].AffineInverse();
        }
    } else if (gen == GxTexGen_SphereMap) {
        matrix.a0 = 0.5f;
        matrix.b1 = 0.5f;
        matrix.d0 = 0.5f;
        matrix.d1 = 0.5f;
    } else {
        stack.Identity();
        return;
    }

    stack.m_dirty = 1;
    stack.m_flags[level] &= ~CGxMatrixStack::F_Identity;
    stack.m_mtx[level] = matrix;
    this->m_fixedKeysDirty = 1;
}

namespace {

GxMetal::Buffer* PoolBuffer(CGxPool* pool) {
    return pool ? static_cast<GxMetal::Buffer*>(pool->m_apiSpecific) : nullptr;
}

}

GxMetal::Buffer* CGxDeviceMetalNative::ICreateBuffer(CGxPool* pool) {
    if (!pool || !this->m_metal || pool->m_size <= 0) {
        return nullptr;
    }

    // Stream and dynamic pools are written while the GPU may still read them, so their buffers follow
    // the lock's rules; static pools are only written when the serials say the GPU is done with them.
    // Index buffers keep a CPU copy of their contents because triangle fans are expanded on the CPU.
    bool dynamic = pool->m_usage == GxPoolUsage_Stream || pool->m_usage == GxPoolUsage_Dynamic;
    bool keepShadow = pool->m_target == GxPoolTarget_Index;
    GxMetal::Buffer* buffer = new GxMetal::Buffer(this->m_metal, static_cast<uint32_t>(pool->m_size), dynamic, keepShadow);

    if (!buffer->Valid()) {
        buffer->Release();
        return nullptr;
    }

    return buffer;
}

int32_t CGxDeviceMetalNative::CreatePoolAPI(CGxPool* pool) {
    if (!pool) {
        return 0;
    }

    if (pool->m_target == GxPoolTarget_Vertex || pool->m_target == GxPoolTarget_Index) {
        pool->m_apiSpecific = this->ICreateBuffer(pool);
    }

    return 1;
}

void CGxDeviceMetalNative::IReleasePool(CGxPool* pool) {
    if (!pool) {
        return;
    }

    if (pool->m_usage == GxPoolUsage_Stream) {
        pool->m_offset = 0;
    }

    pool->Invalidate();
    GxMetal::Clear(reinterpret_cast<GxMetal::Buffer*&>(pool->m_apiSpecific));
}

void CGxDeviceMetalNative::IReleasePools(int32_t freePools) {
    for (CGxPool* pool = this->m_poolList.Head(); pool; pool = this->m_poolList.Next(pool)) {
        if (freePools || pool->m_usage == GxPoolUsage_Stream || pool->m_usage == GxPoolUsage_Dynamic) {
            this->IReleasePool(pool);
        }
    }

    if (freePools) {
        GxMetal::Clear(this->m_indexBuf);
        this->m_streamPassId = 0;
    }
}

void CGxDeviceMetalNative::PoolSizeSet(CGxPool* pool, uint32_t size) {
    if (!pool || !this->m_metal) {
        return;
    }

    this->IReleasePool(pool);
    pool->m_size = static_cast<int32_t>(size);
    this->CreatePoolAPI(pool);
}

void CGxDeviceMetalNative::PoolDestroy(CGxPool* pool) {
    if (pool) {
        this->IReleasePool(pool);
        CGxDevice::PoolDestroy(pool);
    }
}

char* CGxDeviceMetalNative::IBufLock(CGxBuf* buf) {
    CGxPool* pool = buf->m_pool;
    EmergencyMem& emergency = this->m_emergencyMem[pool->m_target];

    if (!this->m_metal) {
        return static_cast<char*>(emergency.Lock(buf->m_size));
    }

    GxMetal::BufferLock kind = GxMetal::BufferLock_Normal;

    if (pool->m_usage == GxPoolUsage_Stream) {
        // A stream pool appends the buffer in item-sized slots and wraps to the start when it is full
        uint32_t index = (buf->m_itemSize + pool->m_offset - 1) - (buf->m_itemSize + pool->m_offset - 1) % buf->m_itemSize;

        if (index + buf->m_size > static_cast<uint32_t>(pool->m_size)) {
            kind = GxMetal::BufferLock_Discard;
            pool->Discard();
            buf->m_index = 0;
            pool->m_offset = static_cast<int32_t>(buf->m_size);
        } else {
            kind = GxMetal::BufferLock_NoOverwrite;
            buf->m_index = index;
            pool->m_offset = static_cast<int32_t>(index + buf->m_size);
        }
    } else if (pool->m_usage == GxPoolUsage_Dynamic) {
        kind = GxMetal::BufferLock_NoOverwrite;
    }

    if (!pool->m_apiSpecific) {
        this->CreatePoolAPI(pool);
    }

    GxMetal::Buffer* buffer = PoolBuffer(pool);

    if (!buffer) {
        return static_cast<char*>(emergency.Lock(buf->m_size));
    }

    char* data = buffer->Lock(buf->m_index, buf->m_size, kind);

    if (!data) {
        // Buffer::Lock pushed no record for a failed lock, and Unlock() pops the last record: calling
        // it here would pop a live one
        return static_cast<char*>(emergency.Lock(buf->m_size));
    }

    return data;
}

void CGxDeviceMetalNative::IBufUnlock(CGxBuf* buf) {
    CGxPool* pool = buf->m_pool;
    EmergencyMem& emergency = this->m_emergencyMem[pool->m_target];

    if (emergency.m_lock) {
        emergency.Unlock();
        buf->m_valid = 0;
        this->m_intF5C = 1;
        return;
    }

    GxMetal::Buffer* buffer = PoolBuffer(pool);

    if (buffer) {
        buffer->Unlock();
    }

    buf->m_valid = buffer != nullptr;
}

char* CGxDeviceMetalNative::BufLock(CGxBuf* buf) {
    CGxDevice::BufLock(buf);
    return this->IBufLock(buf);
}

int32_t CGxDeviceMetalNative::BufUnlock(CGxBuf* buf, uint32_t size) {
    CGxDevice::BufUnlock(buf, size);
    this->IBufUnlock(buf);
    return 1;
}

void CGxDeviceMetalNative::BufData(CGxBuf* buf, const void* data, uint32_t size, uint32_t offset) {
    CGxDevice::BufData(buf, data, size, offset);

    char* destination = this->IBufLock(buf);

    if (!destination) {
        return;
    }

    memcpy(destination + offset, data, size);
    this->IBufUnlock(buf);
}

void CGxDeviceMetalNative::ISetVertexBuffer(uint32_t stream, GxMetal::Buffer* buffer, uint32_t offset, uint32_t stride) {
    if (stream >= 16) {
        return;
    }

    if (!this->m_caps.m_streamOffset) {
        offset = 0;
    }

    if (this->m_streamBufs[stream] != buffer || this->m_streamOffsets[stream] != offset || this->m_streamStrides[stream] != stride) {
        GxMetal::Assign(this->m_streamBufs[stream], buffer);
        this->m_streamOffsets[stream] = offset;
        this->m_streamStrides[stream] = stride;
    }
}

// CGxDeviceMetal.cpp:3227 - copy a rectangle of the window's back buffer into a texture
int32_t CGxDeviceMetalNative::TexCopy(CGxTex* dest, const C2iVector& destPos, const C2iVector& sourcePos, const C2iVector& size, uint32_t level, uint32_t plane) {
    if (!this->m_metal || !this->m_backBuffer) {
        return 0;
    }

    if (dest->m_needsCreation || (!dest->m_apiSpecificData && !dest->m_apiSpecificData2)) {
        this->ITexCreate(dest);
    }

    GxMetal::Texture* texture = this->ITexData(dest);

    if (dest->m_needsCreation || !texture) {
        return 0;
    }

    GxMetal::Rect from = { sourcePos.x, sourcePos.y, sourcePos.x + size.x, sourcePos.y + size.y };
    GxMetal::Rect to = { destPos.x, destPos.y, destPos.x + size.x, destPos.y + size.y };
    GxMetal::Target source = { this->m_backBuffer, 0, 0 };
    GxMetal::Target target = { texture, dest->m_target == GxTex_CubeMap ? plane : 0, level };
    return this->m_metal->Stretch(source, &from, target, &to, false) ? 1 : 0;
}

// CGxDeviceMetal.cpp:3260 - the same from the back buffer, scaling when the rectangles differ in size
int32_t CGxDeviceMetalNative::TexStretch(CGxTex* dest, const CiRect* destRect, const CiRect* sourceRect, uint32_t level, uint32_t face) {
    if (!this->m_metal || !this->m_backBuffer) {
        return 0;
    }

    if (dest->m_needsCreation || (!dest->m_apiSpecificData && !dest->m_apiSpecificData2)) {
        this->ITexCreate(dest);
    }

    GxMetal::Texture* texture = this->ITexData(dest);

    if (dest->m_needsCreation || !texture) {
        return 0;
    }

    GxMetal::Rect from = { 0, 0, 0, 0 };
    GxMetal::Rect to = { 0, 0, 0, 0 };

    if (sourceRect) {
        from.left = sourceRect->minX;
        from.top = sourceRect->minY;
        from.right = sourceRect->maxX;
        from.bottom = sourceRect->maxY;
    }

    if (destRect) {
        to.left = destRect->minX;
        to.top = destRect->minY;
        to.right = destRect->maxX;
        to.bottom = destRect->maxY;
    }

    // The shim asks for D3DTEXF_NONE only when both rectangles are given and the sizes match; Metal
    // picks blit or a quad by itself, so this only selects the sampler of a scaling copy
    bool linear = !(destRect && sourceRect && (to.right - to.left) == (from.right - from.left) && (to.bottom - to.top) == (from.bottom - from.top));
    GxMetal::Target source = { this->m_backBuffer, 0, 0 };
    GxMetal::Target target = { texture, dest->m_target == GxTex_CubeMap ? face : 0, level };
    return this->m_metal->Stretch(source, sourceRect ? &from : nullptr, target, destRect ? &to : nullptr, linear) ? 1 : 0;
}

void CGxDeviceMetalNative::XformSetView(const C44Matrix& matrix) {

    CGxDevice::XformSetView(matrix);
    this->m_fixedVertexDirty = 1;
}

// CGxDeviceMetal.cpp:596 - the client projects into Direct3D's [0, 1] depth with its own matrices; the
// fix-up below maps them onto the same range Metal expects (the shim forwards this matrix untouched)
void CGxDeviceMetalNative::XformSetProjection(const C44Matrix& matrix) {
    CGxDevice::XformSetProjection(matrix);

    C44Matrix projNative = matrix;

    if (!(fabs(projNative.c3 - 1.0f) < 0.00000023841858f) && !(fabs(projNative.c3) < 0.00000023841858f)) {
        projNative *= 1.0f / projNative.c3;
    }

    if (projNative.d3 == 0.0f) {
        float minZ = -(projNative.d2 / (projNative.c2 + 1.0f));
        float maxZ = -(projNative.d2 / (projNative.c2 - 1.0f));
        projNative.c2 = maxZ / (maxZ - minZ);
        projNative.d2 = maxZ * minZ / (minZ - maxZ);
    } else {
        float invC2 = 1.0f / projNative.c2;
        float minZ = (-1.0f - projNative.d2) * invC2;
        float maxZ = (1.0f - projNative.d2) * invC2;
        projNative.c2 = 1.0f / (maxZ - minZ);
        projNative.d2 = minZ / (minZ - maxZ);
    }

    if (!(this->m_appMasterEnables & (1 << GxMasterEnable_NormalProjection)) && projNative.d3 != 1.0f) {
        C44Matrix scale;
        scale.a0 = 0.2f;
        scale.b1 = 0.2f;
        scale.c2 = 0.2f;
        projNative = projNative * scale;
    }


    this->m_xforms[GxXform_Projection].m_dirty = 1;
    this->m_projNative = projNative;
    this->m_fixedVertexDirty = 1;
}

// CGxDeviceMetal.cpp:2812 - the state the client never pushes, then the client's own defaults
void CGxDeviceMetalNative::IStateSetDefaults() {
    memset(&this->m_hw, 0, sizeof(this->m_hw));

    this->m_hw.m_blendEnable = false;
    this->m_hw.m_srcBlend = MTLBlendFactorOne;
    this->m_hw.m_dstBlend = MTLBlendFactorZero;
    this->m_hw.m_colorWrite = MTLColorWriteMaskAll;
    this->m_hw.m_depthCompare = MTLCompareFunctionLessEqual;
    this->m_hw.m_depthWrite = true;
    this->m_hw.m_cullMode = MTLCullModeNone;
    this->m_hw.m_fillMode = MTLTriangleFillModeFill;
    this->m_hw.m_pointSize = 1.0f;
    this->m_hw.m_pointSizeMin = 1.0f;
    this->m_hw.m_pointSizeMax = this->m_caps.m_pointScaleMax;

    for (uint32_t i = 0; i < 8; i++) {
        this->m_hw.m_stages[i].m_colorOp = GxMetal::StageOp_Disable;
        this->m_hw.m_stages[i].m_alphaOp = GxMetal::StageOp_Disable;
    }

    for (uint32_t i = 0; i < 16; i++) {
        this->m_samplerDirty[i] = 1;
    }

    this->m_primVertexDirty = 0xFFFFFFFF;
    this->m_primIndexDirty = 0;
    this->m_pipelineDirty = 1;
    this->m_depthDirty = 1;
    this->m_fixedKeysDirty = 1;
    this->m_stagesDirty = 1;
    this->m_vertexStateDirty = 1;
    this->m_pixelStateDirty = 1;
    this->m_fixedVertexDirty = 1;
    this->m_ambientOnlyMode = 0;

    for (uint32_t i = 0; i < 16; i++) {
        this->m_hw.m_streams[i].m_buffer = nullptr;
        this->m_hw.m_streams[i].m_offset = 0;
        this->m_hw.m_streams[i].m_stride = 0;
    }

    // The client's IRsInit values run through the same translation as the frame's own state
    this->IRsForceUpdate();
    this->IRsSync(0);

    // The client may have set its matrices before the device existed; fill the native state once
    for (uint32_t i = 0; i < 11; i++) {
        this->m_xforms[i].m_dirty = 1;
    }

    this->IStateSyncXforms();
}

// CGxDeviceMetal.cpp:1790 - everything the draw needs, in the order the old device syncs it
void CGxDeviceMetalNative::IStateSync() {
    if (!this->m_metal) {
        return;
    }

    // The client writes its constant files through dirty ranges; copy them before the state sync
    this->IShaderConstantsFlush();
    this->IRsSync(0);

    bool hardwareVertexShader = this->m_hwRenderStates[GxRs_VertexShader].m_data.i[0] != 0;
    bool appVertexShader = this->m_appRenderStates[GxRs_VertexShader].m_value.m_data.i[0] != 0;

    if (!hardwareVertexShader || !appVertexShader) {
        this->IStateSyncLights();
        this->IStateSyncMaterial();
        this->IStateSyncXforms();
    }

    this->IStateSyncEnables();
    this->IStateSyncClipPlanes();
    this->IStateSyncScissorRect();
    this->IStateSyncVertexPtrs();

    if (this->m_primIndexDirty) {
        this->m_primIndexDirty = 0;
        GxMetal::Assign(this->m_indexBuf, this->m_primIndexBuf ? PoolBuffer(this->m_primIndexBuf->m_pool) : nullptr);
    }

    if (this->m_viewportDirty) {
        this->IXformSetViewport();
        this->m_viewportDirty = 0;
    }
}

// CGxDeviceMetal.cpp:727 - the master enables the client switches (wireframe among them)
void CGxDeviceMetalNative::IStateSyncEnables() {
    if (this->m_appMasterEnables == this->m_hwMasterEnables) {
        return;
    }

    this->m_hw.m_fillMode = (this->m_appMasterEnables & (1 << GxMasterEnable_PolygonFill)) ? MTLTriangleFillModeFill : MTLTriangleFillModeLines;
    this->m_hwMasterEnables = this->m_appMasterEnables;
}

// CGxDeviceMetal.cpp:741 - the client's world-space clip planes
void CGxDeviceMetalNative::IStateSyncClipPlanes() {
    if (!this->m_clipPlaneMask) {
        return;
    }

    for (uint32_t i = 0; i < 6; i++) {
        if (this->m_clipPlaneMask & (1 << i)) {
            this->m_hw.m_clipPlanes[i][0] = this->m_clipPlane[i].n.x;
            this->m_hw.m_clipPlanes[i][1] = this->m_clipPlane[i].n.y;
            this->m_hw.m_clipPlanes[i][2] = this->m_clipPlane[i].n.z;
            this->m_hw.m_clipPlanes[i][3] = this->m_clipPlane[i].d;
        }
    }

    this->m_hw.m_clipPlaneMask = this->m_clipPlaneMask;
    this->m_clipPlaneMask = 0;
    this->m_vertexStateDirty = 1;
}

// CGxDeviceMetal.cpp:755 - normalized client rectangle to target pixels, y down
// As CGxDeviceD3d, relative to the current window (the texture while rendering into one)
void CGxDeviceMetalNative::IStateSyncScissorRect() {
    if (!this->m_scissorRectDirty) {
        return;
    }

    const CRect& window = this->DeviceCurWindow();
    int32_t width = static_cast<int32_t>(window.maxX);
    int32_t height = static_cast<int32_t>(window.maxY);
    bool textureTarget = this->m_textureTarget[GxBuffers_Color].m_texture || this->m_textureTarget[GxBuffers_Depth].m_texture;

    int32_t left = static_cast<int32_t>(this->m_scissorRect.minX * width + 0.5f);
    int32_t right = static_cast<int32_t>(this->m_scissorRect.maxX * width + 1.0f);
    int32_t top = textureTarget ? static_cast<int32_t>(this->m_scissorRect.minY * height + 0.5f) : static_cast<int32_t>((1.0f - this->m_scissorRect.maxY) * height + 0.5f);
    int32_t bottom = textureTarget ? static_cast<int32_t>(1 + this->m_scissorRect.maxY * height) : static_cast<int32_t>(1 + (1.0f - this->m_scissorRect.minY) * height);

    left = left < 0 ? 0 : (left > width ? width : left);
    right = right < left ? left : (right > width ? width : right);
    top = top < 0 ? 0 : (top > height ? height : top);
    bottom = bottom < top ? top : (bottom > height ? height : bottom);

    this->m_hw.m_scissor.left = left;
    this->m_hw.m_scissor.top = top;
    this->m_hw.m_scissor.right = right;
    this->m_hw.m_scissor.bottom = bottom;
    this->m_vertexStateDirty = 1;
    this->m_scissorRectDirty = 0;
}

// CGxDeviceMetal.cpp:1298 - projection, view and world into the state block, then the texture matrices
void CGxDeviceMetalNative::IStateSyncXforms() {
    CGxMatrixStack& projection = this->m_xforms[GxXform_Projection];

    if (projection.m_dirty) {
        memcpy(this->m_hw.m_projection, &this->m_projNative, sizeof(this->m_hw.m_projection));
        projection.m_dirty = 0;
        this->m_fixedVertexDirty = 1;
    }

    CGxMatrixStack& view = this->m_xforms[GxXform_View];

    if (view.m_dirty) {
        memcpy(this->m_hw.m_view, &view.m_mtx[view.m_level], sizeof(this->m_hw.m_view));
        view.m_dirty = 0;
        this->m_fixedVertexDirty = 1;
    }

    CGxMatrixStack& world = this->m_xforms[GxXform_World];

    if (world.m_dirty) {
        this->IXformSetWorld();
    }

    for (uint32_t i = 0; i < 8; i++) {
        if (this->m_xforms[GxXform_Tex0 + i].m_dirty || this->m_texGen[i].m_dirty) {
            this->IXformSetTex(i);
        }
    }
}

// CGxDeviceMetal.cpp:1187 - the client's light slots; with lighting off the client gets ambient-only
// shading, which eight disabled lights plus a white global ambient reproduce
void CGxDeviceMetalNative::IStateSyncLights() {
    bool lighting = this->m_appRenderStates[GxRs_Lighting].m_value.m_data.i[0] != 0;

    if (lighting) {
        this->m_ambientOnlyMode = 0;
        this->m_hw.m_lighting = (this->m_appMasterEnables & (1 << GxMasterEnable_Lighting)) != 0;
        this->m_hw.m_globalAmbient[0] = 0.0f;
        this->m_hw.m_globalAmbient[1] = 0.0f;
        this->m_hw.m_globalAmbient[2] = 0.0f;
        this->m_hw.m_globalAmbient[3] = 0.0f;

        for (uint32_t i = 0; i < sizeof(this->m_lights) / sizeof(this->m_lights[0]); i++) {
            CGxApiLight& api = this->m_lights[i];
            HwLight& hw = this->m_hw.m_lights[i];

            if (!api.m_flags) {
                continue;
            }

            if (api.m_flags & 0x1) {
                if (!api.m_enable) {
                    api.m_flags &= ~1u;
                    hw.m_enabled = false;
                    continue;
                }

                hw.m_enabled = true;
            } else if (!api.m_enable) {
                continue;
            }

            hw.m_type = api.m_dir.w == 1.0f ? GxMetal::LightType_Point : GxMetal::LightType_Directional;
            hw.m_position[0] = api.m_dir.x;
            hw.m_position[1] = api.m_dir.y;
            hw.m_position[2] = api.m_dir.z;
            hw.m_position[3] = api.m_dir.w;
            hw.m_direction[0] = api.m_dir.x;
            hw.m_direction[1] = api.m_dir.y;
            hw.m_direction[2] = api.m_dir.z;
            hw.m_direction[3] = api.m_dir.w;
            hw.m_ambient[0] = api.m_ambColor.x * (1.0f / 255.0f);
            hw.m_ambient[1] = api.m_ambColor.y * (1.0f / 255.0f);
            hw.m_ambient[2] = api.m_ambColor.z * (1.0f / 255.0f);
            hw.m_ambient[3] = 0.0f;
            hw.m_diffuse[0] = api.m_dirColor.x * (1.0f / 255.0f);
            hw.m_diffuse[1] = api.m_dirColor.y * (1.0f / 255.0f);
            hw.m_diffuse[2] = api.m_dirColor.z * (1.0f / 255.0f);
            hw.m_diffuse[3] = 1.0f;
            hw.m_specular[0] = api.m_specColor.x * (1.0f / 255.0f);
            hw.m_specular[1] = api.m_specColor.y * (1.0f / 255.0f);
            hw.m_specular[2] = api.m_specColor.z * (1.0f / 255.0f);
            hw.m_specular[3] = 0.0f;
            hw.m_attenuation[0] = api.m_constantAttenuation;
            hw.m_attenuation[1] = api.m_linearAttenuation;
            hw.m_attenuation[2] = api.m_quadraticAttenuation;
            api.m_flags = 0;
        }

        this->m_fixedVertexDirty = 1;
        this->m_fixedKeysDirty = 1;
        return;
    }

    bool vertexColors = (this->m_primVertexMask & (1 << GxVA_Color0)) != 0;
    uint32_t diffuse = this->m_appRenderStates[GxRs_MatDiffuse].m_value.m_data.u[0];

    if (vertexColors || diffuse == 0xFFFFFFFF) {
        this->m_ambientOnlyMode = 0;
        this->m_hw.m_lighting = false;
        this->m_hw.m_globalAmbient[0] = 0.0f;
        this->m_hw.m_globalAmbient[1] = 0.0f;
        this->m_hw.m_globalAmbient[2] = 0.0f;
        this->m_hw.m_globalAmbient[3] = 0.0f;
    } else {
        if (!this->m_ambientOnlyMode) {
            this->m_ambientOnlyMode = 1;

            for (uint32_t i = 0; i < sizeof(this->m_lights) / sizeof(this->m_lights[0]); i++) {
                this->m_hw.m_lights[i].m_enabled = false;
                this->m_lights[i].m_flags = this->m_lights[i].m_enable ? (this->m_lights[i].m_flags | 0x1) : (this->m_lights[i].m_flags & ~1u);
            }
        }

        this->m_hw.m_lighting = true;
        this->m_hw.m_globalAmbient[0] = 1.0f;
        this->m_hw.m_globalAmbient[1] = 1.0f;
        this->m_hw.m_globalAmbient[2] = 1.0f;
        this->m_hw.m_globalAmbient[3] = 1.0f;
    }

    this->m_fixedVertexDirty = 1;
    this->m_fixedKeysDirty = 1;
}

// CGxDeviceMetal.cpp:1279 - which colour source each material term takes; the client always has
// vertex colours, so D3DRS_COLORVERTEX is on
void CGxDeviceMetalNative::IStateSyncMaterial() {
    bool vertexColors = (this->m_primVertexMask & (1 << GxVA_Color0)) != 0;

    if (vertexColors) {
        int32_t colorMaterial = this->m_appRenderStates[GxRs_ColorMaterial].m_value.m_data.i[0];
        this->m_hw.m_ambientSource = colorMaterial == 0 ? GxMetal::MaterialSource_Color0 : GxMetal::MaterialSource_Material;
        this->m_hw.m_diffuseSource = colorMaterial == 0 ? GxMetal::MaterialSource_Color0 : GxMetal::MaterialSource_Material;
        this->m_hw.m_specularSource = colorMaterial == 1 ? GxMetal::MaterialSource_Color0 : GxMetal::MaterialSource_Material;
        this->m_hw.m_emissiveSource = colorMaterial == 2 ? GxMetal::MaterialSource_Color0 : GxMetal::MaterialSource_Material;
    } else {
        this->m_hw.m_ambientSource = GxMetal::MaterialSource_Material;
        this->m_hw.m_diffuseSource = GxMetal::MaterialSource_Material;
        this->m_hw.m_specularSource = GxMetal::MaterialSource_Material;
        this->m_hw.m_emissiveSource = GxMetal::MaterialSource_Material;
    }

    this->m_fixedKeysDirty = 1;
}

// CGxDeviceMetal.cpp:1720 - the vertex streams of the draw
void CGxDeviceMetalNative::IStateSyncVertexPtrs() {
    if (!this->m_primVertexDirty) {
        return;
    }

    if (this->m_primVertexFormat != GxVertexBufferFormats_Last && this->m_primVertexBuf) {
        // One known interleaved format: a single stream over the whole vertex
        this->ISetVertexBuffer(0, PoolBuffer(this->m_primVertexBuf->m_pool), this->m_primVertexBuf->m_index, this->m_primVertexSize);
        this->m_primVertexDirty = 0;
        return;
    }

    // Attributes from several buffers: one stream per distinct buffer, the stride is the sum of its
    // attributes' sizes (CGxDeviceMetal.cpp:1752-1763)
    CGxBuf* buffers[GxVAs_Last] = {};
    uint32_t strides[GxVAs_Last] = {};
    uint32_t count = 0;

    for (uint32_t i = 0; i < GxVAs_Last; i++) {
        if (!(this->m_primVertexMask & (1 << i)) || !this->m_primVertexFormatBuf[i]) {
            continue;
        }

        uint32_t stream = 0;

        for (; stream < count; stream++) {
            if (buffers[stream] == this->m_primVertexFormatBuf[i]) {
                break;
            }
        }

        if (stream == count) {
            buffers[count++] = this->m_primVertexFormatBuf[i];
        }

        static const uint32_t kAttribSizes[7] = { 4, 4, 4, 8, 12, 4, 4 };
        uint32_t type = this->m_primVertexFormatAttrib[i].m_type;
        strides[stream] += kAttribSizes[type < 7 ? type : 0];
    }

    for (uint32_t stream = 0; stream < count; stream++) {
        this->ISetVertexBuffer(stream, PoolBuffer(buffers[stream]->m_pool), buffers[stream]->m_index, strides[stream]);
    }

    this->m_primVertexDirty = 0;
}

void CGxDeviceMetalNative::IXformSetWorld() {
    CGxMatrixStack& world = this->m_xforms[GxXform_World];
    memcpy(this->m_hw.m_world, &world.m_mtx[world.m_level], sizeof(this->m_hw.m_world));
    world.m_dirty = 0;
    this->m_fixedVertexDirty = 1;
}

// CGxDeviceMetal.cpp:1835 - the texture matrix of a stage; the client's TextureShader value decides
// whether the texture transform composes with the texgen matrix, and how many components it produces
void CGxDeviceMetalNative::IXformSetTex(uint32_t tmu) {
    if (tmu >= 8) {
        return;
    }

    CGxMatrixStack& xform = this->m_xforms[GxXform_Tex0 + tmu];
    CGxMatrixStack& texGen = this->m_texGen[tmu];
    HwStage& stage = this->m_hw.m_stages[tmu];
    int32_t mode = this->m_appRenderStates[GxRs_TextureShader0 + tmu].m_value.m_data.i[0];
    C44Matrix matrix;

    switch (mode) {
        case 0: {
            matrix = texGen.m_mtx[texGen.m_level];
            bool identity = (texGen.m_flags[texGen.m_level] & CGxMatrixStack::F_Identity) != 0;
            stage.m_transformCount = identity ? 0 : 3;
            stage.m_projected = 0;
            break;
        }
        case 1: {
            C44Matrix product = texGen.m_mtx[texGen.m_level] * xform.m_mtx[xform.m_level];
            matrix = product;

            if (this->m_appRenderStates[GxRs_TexGen0 + tmu].m_value.m_data.i[0]) {
                stage.m_transformCount = 3;
            } else {
                stage.m_transformCount = 2;
                // Pack a 2D transform: the translation moves into the w row
                matrix.c0 = product.d0;
                matrix.c1 = product.d1;
            }

            stage.m_projected = 0;
            break;
        }
        case 2: {
            matrix = texGen.m_mtx[texGen.m_level] * xform.m_mtx[xform.m_level];
            stage.m_transformCount = 3;
            stage.m_projected = 3;
            break;
        }
        default:
            matrix.Identity();
            stage.m_transformCount = 0;
            stage.m_projected = 0;
            break;
    }

    memcpy(stage.m_transform, &matrix, sizeof(stage.m_transform));
    xform.m_dirty = 0;
    texGen.m_dirty = 0;
    this->m_fixedKeysDirty = 1;
}

// CGxDeviceMetal.cpp:1910 - the client's normalized viewport to Metal's pixels
// As CGxDeviceD3d: the viewport is relative to the current window, which is the texture's size while
// the client renders into a texture (CGxDevice::DeviceSetRenderTarget), not the game window's
void CGxDeviceMetalNative::IXformSetViewport() {
    const CRect& window = this->DeviceCurWindow();
    float windowWidth = window.maxX;
    float windowHeight = window.maxY;

    if (windowWidth <= 0.0f || windowHeight <= 0.0f) {
        return;
    }

    float x = this->m_viewport.m_x.m_l * windowWidth;
    float y = (1.0f - this->m_viewport.m_y.m_h) * windowHeight;
    float w = this->m_viewport.m_x.m_h * windowWidth - x;
    float h = (1.0f - this->m_viewport.m_y.m_l) * windowHeight - y;

    if (this->m_textureTarget[GxBuffers_Color].m_texture || this->m_textureTarget[GxBuffers_Depth].m_texture) {
        y = windowHeight * this->m_viewport.m_y.m_l;
    }

    // Direct3D puts pixel centres at integers, Metal at half-integers; slightly less than a half keeps
    // exact-texel quads stable (the shim's kPixelCenterOffset)
    const float pixelCentre = 0.5f - 1.0f / 128.0f;
    this->m_hw.m_viewport[0] = x + pixelCentre;
    this->m_hw.m_viewport[1] = y + pixelCentre;
    this->m_hw.m_viewport[2] = w;
    this->m_hw.m_viewport[3] = h;

    float minZ = this->m_viewport.m_z.m_l;
    float maxZ = this->m_viewport.m_z.m_h;
    minZ = minZ < 0.0f ? 0.0f : (minZ > 1.0f ? 1.0f : minZ);
    maxZ = maxZ < minZ ? minZ : (maxZ > 1.0f ? 1.0f : maxZ);
    this->m_hw.m_viewport[4] = minZ;
    this->m_hw.m_viewport[5] = maxZ;
    this->m_vertexStateDirty = 1;
}

// CGxDeviceMetal.cpp:2790 - the client's batch; count != 0 means indexed
void CGxDeviceMetalNative::Draw(CGxBatch* batch, int32_t count) {
    if (!this->m_metal || this->m_intF5C) {
        return;
    }


    this->IStateSync();

    
    uint32_t baseVertexIndex = 0;

    if (!this->m_caps.m_streamOffset && this->m_primVertexFormatBuf[0] && this->m_primVertexFormatBuf[0]->m_itemSize) {
        baseVertexIndex = this->m_primVertexFormatBuf[0]->m_index / this->m_primVertexFormatBuf[0]->m_itemSize;
    }

    uint32_t primitiveCount = this->PrimCalcCount(batch->m_primType, batch->m_count);

    if (!this->IDrawPrepare(batch)) {
        return;
    }


    this->IDraw(batch, count != 0, baseVertexIndex, primitiveCount);
}

// The state block the translated shaders read is built from HwState; see the header for the fields
bool CGxDeviceMetalNative::IDrawPrepare(const CGxBatch* batch) {
    GxMetal::VertexLayout* layout = this->ILayoutFor();

    if (!layout) {
        return false;
    }

    for (uint32_t i = 0; i < 16; i++) {
        GxMetal::Texture* texture = this->m_hw.m_textures[i];

        if (texture && texture->m_mipsDirty) {
            this->m_metal->GenerateMipmaps(texture);
        }
    }

    // Mipmap generation can end the render encoder, so take it only after the blits.
    id<MTLRenderCommandEncoder> encoder = this->m_metal->RenderEncoder();

    if (!encoder) {
        return false;
    }

    if (this->m_metal->PassId() != this->m_bound.m_passId) {
        this->m_bound.m_passId = this->m_metal->PassId();
        this->m_bound.m_vertexFunction = 0;
        this->m_bound.m_fragmentFunction = 0;
        this->m_bound.m_pipeline = nil;
        for (uint32_t i = 0; i < 16; i++) {
            this->m_bound.m_textures[i] = nil;
            this->m_bound.m_samplers[i] = nil;
        }

        this->m_bound.m_pipeline = nil;
        this->m_bound.m_depthStencil = nil;
        this->m_vsConstantsDirty = true;
        this->m_psConstantsDirty = true;
        this->m_vertexStateDirty = true;
        this->m_pixelStateDirty = true;
        this->m_fixedVertexDirty = true;

        for (uint32_t i = 0; i < 16; i++) {
            this->m_samplerDirty[i] = 1;
        }
    }

    GxMetal::CompiledFunction vertex;
    GxMetal::ShaderInfo vertexInfo;

    if (this->m_hw.m_vertexShader) {

        vertex = this->m_pipelines->VertexShaderFunction(this->m_hw.m_vertexShader, layout->m_signature);
    } else {
        GxMetal::FixedVertexKey key;
        memcpy(key.m_attributes, layout->m_signature.m_class, GxMetal::ATTRIB_COUNT);
        memcpy(key.m_texcoordComponents, layout->m_texcoordComponents, 8);
        key.m_lighting = this->m_hw.m_lighting ? 1 : 0;

        for (uint32_t i = 0; i < GxMetal::kMaxLights; i++) {
            key.m_lightTypes[i] = this->m_hw.m_lights[i].m_enabled ? this->m_hw.m_lights[i].m_type : static_cast<uint8_t>(GxMetal::LightType_None);
        }

        key.m_diffuseSource = this->m_hw.m_diffuseSource;
        key.m_ambientSource = this->m_hw.m_ambientSource;
        key.m_specularSource = this->m_hw.m_specularSource;
        key.m_emissiveSource = this->m_hw.m_emissiveSource;
        key.m_normalizeNormals = this->m_hw.m_normalizeNormals ? 1 : 0;

        for (uint32_t i = 0; i < 8; i++) {
            key.m_texcoordIndex[i] = this->m_hw.m_stages[i].m_texcoordIndex;
            key.m_texgen[i] = this->m_hw.m_stages[i].m_texGen;
            key.m_transformCount[i] = this->m_hw.m_stages[i].m_transformCount;
        }

        key.m_fog = this->m_hw.m_fog ? 1 : 0;
        key.m_pointScale = this->m_hw.m_pointScale ? 1 : 0;
        vertex = this->m_pipelines->FixedVertexFunction(key);
    }

    if (!vertex.m_function) {
        return false;
    }

    
    GxMetal::CompiledFunction fragment;
    GxMetal::ShaderInfo pixelInfo;

    if (this->m_hw.m_pixelShader) {

        GxMetal::PixelShaderVariant variant;
        pixelInfo = this->m_hw.m_pixelShader->m_info;

        for (uint32_t i = 0; i < 16; i++) {
            GxMetal::SamplerKind kind = GxMetal::SAMPLERKIND_2D;
            GxMetal::Texture* texture = this->m_hw.m_textures[i];

            if (pixelInfo.m_major >= 2) {
                kind = static_cast<GxMetal::SamplerKind>((pixelInfo.m_declaredKinds >> (i * 2)) & 3);

                if (kind == GxMetal::SAMPLERKIND_2D && texture && texture->Kind() == GxMetal::SAMPLERKIND_DEPTH) {
                    kind = GxMetal::SAMPLERKIND_DEPTH;
                }
            } else if (texture) {
                kind = texture->Kind();
            }

            variant.m_samplerKinds |= static_cast<uint32_t>(kind) << (i * 2);

            if (this->m_hw.m_stages[i].m_projected && i < 4) {
                variant.m_projected |= static_cast<uint16_t>(this->m_hw.m_stages[i].m_projected & 7) << (i * 3);
            }
        }

        variant.m_pointSprite = this->m_hw.m_pointSprite ? 1 : 0;
        fragment = this->m_pipelines->PixelShaderFunction(this->m_hw.m_pixelShader, variant);
    } else {
        GxMetal::FixedPixelKey key;

        for (uint32_t i = 0; i < 8; i++) {
            GxMetal::FixedPixelStage& stage = key.m_stages[i];
            stage.m_colorOp = this->m_hw.m_stages[i].m_colorOp;
            stage.m_alphaOp = this->m_hw.m_stages[i].m_alphaOp;

            if (this->m_hw.m_textures[i]) {
                stage.m_textureBound = 1;
                stage.m_samplerKind = static_cast<uint8_t>(this->m_hw.m_textures[i]->Kind());
            }

            stage.m_projected = this->m_hw.m_stages[i].m_projected;
        }

        key.m_specularEnable = this->m_hw.m_specularEnable ? 1 : 0;
        key.m_pointSprite = this->m_hw.m_pointSprite ? 1 : 0;
        fragment = this->m_pipelines->FixedPixelFunction(key);
    }

    if (!fragment.m_function) {
        return false;
    }

    GxMetal::PipelineKey key;
    key.m_vertexFunction = vertex.m_id;
    key.m_fragmentFunction = fragment.m_id;
    key.m_layout = layout->m_id;

    for (uint32_t i = 0; i < 16; i++) {
        key.m_strides[i] = static_cast<uint16_t>(this->m_streamStrides[i]);
    }

    id<MTLTexture> colorTexture = this->m_metal->ColorTarget().m_texture ? this->m_metal->ColorTarget().m_texture->m_texture : nil;
    id<MTLTexture> depthTexture = this->m_metal->DepthTarget().m_texture ? this->m_metal->DepthTarget().m_texture->m_texture : nil;
    key.m_colorFormat = colorTexture ? static_cast<uint32_t>(colorTexture.pixelFormat) : 0;
    key.m_depthFormat = depthTexture ? static_cast<uint32_t>(depthTexture.pixelFormat) : 0;
    // The target decides the sample count: a client texture target is single-sample even while the
    // window's buffers are multisampled
    {
        GxMetal::Texture* colorTarget = this->m_metal->ColorTarget().m_texture;
        key.m_samples = static_cast<uint8_t>(colorTarget ? colorTarget->m_samples : this->m_samples);
    }
    key.m_blendEnable = this->m_hw.m_blendEnable ? 1 : 0;
    key.m_srcBlend = static_cast<uint8_t>(this->m_hw.m_srcBlend);
    key.m_dstBlend = static_cast<uint8_t>(this->m_hw.m_dstBlend);
    key.m_writeMask = static_cast<uint8_t>(this->m_hw.m_colorWrite);

    id<MTLRenderPipelineState> pipeline = this->m_pipelines->Pipeline(key, vertex, fragment, *layout);

    if (!pipeline) {
        return false;
    }

    [encoder setRenderPipelineState:pipeline];
    [encoder setDepthStencilState:this->m_pipelines->DepthStencil(this->m_hw.m_depthCompare, this->m_hw.m_depthWrite, false)];
    [encoder setCullMode:this->m_hw.m_cullMode];
    [encoder setTriangleFillMode:this->m_hw.m_fillMode];
    [encoder setDepthBias:this->m_hw.m_depthBias slopeScale:0.0f clamp:0.0f];
    [encoder setBlendColorRed:this->m_hw.m_blendColor[0] green:this->m_hw.m_blendColor[1] blue:this->m_hw.m_blendColor[2] alpha:this->m_hw.m_blendColor[3]];

    MTLViewport viewport;
    viewport.originX = this->m_hw.m_viewport[0];
    viewport.originY = this->m_hw.m_viewport[1];
    viewport.width = this->m_hw.m_viewport[2] > 0.0f ? this->m_hw.m_viewport[2] : 1.0f;
    viewport.height = this->m_hw.m_viewport[3] > 0.0f ? this->m_hw.m_viewport[3] : 1.0f;
    viewport.znear = this->m_hw.m_viewport[4];
    viewport.zfar = this->m_hw.m_viewport[5];
    [encoder setViewport:viewport];

    // The encoder keeps the last scissor rectangle (a partial clear sets one too), so a draw without
    // the scissor test covers the whole target; Metal requires the rectangle inside the target, which
    // can be smaller than the window the client computed it for
    int32_t targetWidth = static_cast<int32_t>(this->m_metal->ColorTarget().Width());
    int32_t targetHeight = static_cast<int32_t>(this->m_metal->ColorTarget().Height());
    int32_t scissorLeft = 0;
    int32_t scissorTop = 0;
    int32_t scissorRight = targetWidth;
    int32_t scissorBottom = targetHeight;

    if (this->m_hw.m_scissorTest) {
        scissorLeft = std::clamp(this->m_hw.m_scissor.left, 0, targetWidth);
        scissorTop = std::clamp(this->m_hw.m_scissor.top, 0, targetHeight);
        scissorRight = std::clamp(this->m_hw.m_scissor.right, scissorLeft, targetWidth);
        scissorBottom = std::clamp(this->m_hw.m_scissor.bottom, scissorTop, targetHeight);
    }

    // An empty rectangle draws nothing, as Direct3D's
    if (scissorRight <= scissorLeft || scissorBottom <= scissorTop) {
        return false;
    }

    MTLScissorRect scissor;
    scissor.x = static_cast<NSUInteger>(scissorLeft);
    scissor.y = static_cast<NSUInteger>(scissorTop);
    scissor.width = static_cast<NSUInteger>(scissorRight - scissorLeft);
    scissor.height = static_cast<NSUInteger>(scissorBottom - scissorTop);
    [encoder setScissorRect:scissor];

    for (uint32_t stream = 0; stream < 16; stream++) {
        if (!(layout->m_streamMask & (1u << stream))) {
            continue;
        }

        GxMetal::Buffer* buffer = this->m_streamBufs[stream];

        if (!buffer || !buffer->Valid()) {
            return false;
        }

        [encoder setVertexBuffer:buffer->m_buffer offset:this->m_streamOffsets[stream] atIndex:stream];
        buffer->m_use.m_lastUse = buffer->m_use.m_lastRenderUse = this->m_metal->CurrentSerial();
    }

    uint32_t vertexConstants = this->m_hw.m_vertexShader ? (this->m_hw.m_vertexShader->m_info.m_constantCount ? this->m_hw.m_vertexShader->m_info.m_constantCount : 1) : 1;

    if (this->m_vsConstantsDirty || this->m_bound.m_vertexFunction != vertex.m_id) {
        [encoder setVertexBytes:this->m_hw.m_vsConstants length:vertexConstants * 16 atIndex:GxMetal::VS_BUFFER_FLOAT_CONSTANTS];
        [encoder setVertexBytes:this->m_vsIntConstants length:sizeof(this->m_vsIntConstants) atIndex:GxMetal::VS_BUFFER_INT_CONSTANTS];
        [encoder setVertexBytes:this->m_vsBoolConstants length:sizeof(this->m_vsBoolConstants) atIndex:GxMetal::VS_BUFFER_BOOL_CONSTANTS];
        this->m_vsConstantsDirty = false;
    }

    uint32_t pixelConstants = this->m_hw.m_pixelShader ? (pixelInfo.m_constantCount ? pixelInfo.m_constantCount : 1) : 1;

    if (this->m_psConstantsDirty || this->m_bound.m_fragmentFunction != fragment.m_id) {
        [encoder setFragmentBytes:this->m_hw.m_psConstants length:pixelConstants * 16 atIndex:GxMetal::PS_BUFFER_FLOAT_CONSTANTS];
        [encoder setFragmentBytes:this->m_psIntConstants length:sizeof(this->m_psIntConstants) atIndex:GxMetal::PS_BUFFER_INT_CONSTANTS];
        [encoder setFragmentBytes:this->m_psBoolConstants length:sizeof(this->m_psBoolConstants) atIndex:GxMetal::PS_BUFFER_BOOL_CONSTANTS];
        this->m_psConstantsDirty = false;
    }

    if (this->m_vertexStateDirty || this->m_bound.m_vertexFunction != vertex.m_id) {
        GxMetal::VertexStateBlock block = {};
        block.m_posFixup[2] = this->m_hw.m_depthBias;
        memcpy(block.m_clipPlanes, this->m_hw.m_clipPlanes, sizeof(block.m_clipPlanes));
        block.m_flags[0] = this->m_hw.m_clipPlaneMask;
        block.m_pointSize[0] = this->m_hw.m_pointSize;
        block.m_pointSize[1] = this->m_hw.m_pointSizeMin;
        block.m_pointSize[2] = this->m_hw.m_pointSizeMax;
        [encoder setVertexBytes:&block length:sizeof(block) atIndex:GxMetal::VS_BUFFER_STATE];
        this->m_vertexStateDirty = false;
    }

    if (!this->m_hw.m_vertexShader) {
        this->IDrawFixedVertexState();
    }

    if (this->m_pixelStateDirty || this->m_bound.m_fragmentFunction != fragment.m_id) {
        GxMetal::PixelStateBlock block = {};
        memcpy(block.m_fogColor, this->m_hw.m_fogColor, sizeof(block.m_fogColor));
        bool fog = this->m_hw.m_fog && (!this->m_hw.m_pixelShader || pixelInfo.m_major < 3);
        block.m_flags[0] = fog ? 1 : 0;
        block.m_flags[1] = this->m_hw.m_alphaTest ? GxMetal::SHADERCOMPARE_GREATEREQUAL : GxMetal::SHADERCOMPARE_ALWAYS;
        block.m_flags[2] = this->m_hw.m_alphaRef;
        block.m_textureFactor[0] = 1.0f;
        block.m_textureFactor[1] = 1.0f;
        block.m_textureFactor[2] = 1.0f;
        block.m_textureFactor[3] = 1.0f;
        [encoder setFragmentBytes:&block length:sizeof(block) atIndex:GxMetal::PS_BUFFER_STATE];
        this->m_pixelStateDirty = false;
    }


    
    this->m_bound.m_vertexFunction = vertex.m_id;
    this->m_bound.m_fragmentFunction = fragment.m_id;

    uint32_t samplerMask = this->m_hw.m_pixelShader ? pixelInfo.m_samplerMask : 0xFF;
    this->IDrawBindTextures(this->m_hw.m_pixelShader ? &pixelInfo : nullptr, samplerMask);
    this->m_metal->MarkTargetsRendered();
    return true;
}

// The samplers a pixel shader reads; a stage without a texture of the expected kind gets the
// context's opaque placeholder, as the shim does
void CGxDeviceMetalNative::IDrawBindTextures(const GxMetal::ShaderInfo* pixelInfo, uint32_t samplerMask) {
    id<MTLRenderCommandEncoder> encoder = this->m_metal->RenderEncoder();

    if (!encoder) {
        return;
    }

    for (uint32_t i = 0; i < 16; i++) {
        if (!(samplerMask & (1u << i))) {
            continue;
        }

        GxMetal::Texture* texture = this->m_hw.m_textures[i];
        GxMetal::SamplerKind textureKind = texture ? texture->Kind() : GxMetal::SAMPLERKIND_2D;
        GxMetal::SamplerKind expected = textureKind;

        if (pixelInfo && pixelInfo->m_major >= 2) {
            expected = static_cast<GxMetal::SamplerKind>((pixelInfo->m_declaredKinds >> (i * 2)) & 3);

            if (expected == GxMetal::SAMPLERKIND_2D && texture && texture->Kind() == GxMetal::SAMPLERKIND_DEPTH) {
                expected = GxMetal::SAMPLERKIND_DEPTH;
            }
        }

        id<MTLTexture> bound = (texture && texture->Kind() == expected) ? texture->SampleTexture() : this->m_metal->DummyTexture(expected);
        GxMetal::SamplerKey samplerKey = this->m_hw.m_samplers[i];
        samplerKey.m_compare = expected == GxMetal::SAMPLERKIND_DEPTH ? 1 : 0;
        id<MTLSamplerState> sampler = this->m_pipelines->Sampler(samplerKey);

        if (this->m_samplerDirty[i] || this->m_bound.m_textures[i] != bound) {
            [encoder setFragmentTexture:bound atIndex:i];
            this->m_bound.m_textures[i] = bound;
        }

        if (this->m_samplerDirty[i] || this->m_bound.m_samplers[i] != sampler) {
            [encoder setFragmentSamplerState:sampler atIndex:i];
            this->m_bound.m_samplers[i] = sampler;
        }

        this->m_samplerDirty[i] = 0;

        if (texture) {
            texture->m_use.m_lastUse = texture->m_use.m_lastRenderUse = this->m_metal->CurrentSerial();
        }
    }
}

// The fixed-function vertex block: matrices, material, eight lights in view space, fog and point size
void CGxDeviceMetalNative::IDrawFixedVertexState() {
    if (!this->m_fixedVertexDirty) {
        return;
    }

    id<MTLRenderCommandEncoder> encoder = this->m_metal->RenderEncoder();

    if (!encoder) {
        return;
    }

    GxMetal::FixedVertexStateBlock block = {};
    C44Matrix world;
    C44Matrix view;
    memcpy(&world, this->m_hw.m_world, sizeof(world));
    memcpy(&view, this->m_hw.m_view, sizeof(view));
    C44Matrix worldView = world * view;
    C44Matrix normal = worldView.Inverse().Transpose();

    memcpy(block.m_world, this->m_hw.m_world, sizeof(block.m_world));
    memcpy(block.m_worldView, &worldView, sizeof(block.m_worldView));
    memcpy(block.m_normal, &normal, sizeof(block.m_normal));
    memcpy(block.m_projection, this->m_hw.m_projection, sizeof(block.m_projection));

    for (uint32_t i = 0; i < 8; i++) {
        memcpy(block.m_texture[i], this->m_hw.m_stages[i].m_transform, sizeof(block.m_texture[i]));
    }

    memcpy(block.m_globalAmbient, this->m_hw.m_globalAmbient, sizeof(block.m_globalAmbient));
    memcpy(block.m_materialDiffuse, this->m_hw.m_materialDiffuse, sizeof(block.m_materialDiffuse));
    memcpy(block.m_materialAmbient, this->m_hw.m_materialAmbient, sizeof(block.m_materialAmbient));
    memcpy(block.m_materialSpecular, this->m_hw.m_materialSpecular, sizeof(block.m_materialSpecular));
    memcpy(block.m_materialEmissive, this->m_hw.m_materialEmissive, sizeof(block.m_materialEmissive));
    block.m_materialPower[0] = this->m_hw.m_materialPower;

    for (uint32_t i = 0; i < GxMetal::kMaxLights; i++) {
        const HwLight& light = this->m_hw.m_lights[i];
        GxMetal::FixedLightBlock& target = block.m_lights[i];
        memcpy(target.m_diffuse, light.m_diffuse, sizeof(target.m_diffuse));
        memcpy(target.m_specular, light.m_specular, sizeof(target.m_specular));
        memcpy(target.m_ambient, light.m_ambient, sizeof(target.m_ambient));

        // Positions and directions reach the shader in view space
        if (light.m_type == GxMetal::LightType_Point) {
            C3Vector position(light.m_direction[0], light.m_direction[1], light.m_direction[2]);
            C3Vector transformed = position * view;
            target.m_position[0] = transformed.x;
            target.m_position[1] = transformed.y;
            target.m_position[2] = transformed.z;
            target.m_position[3] = 1.0f;
        }

        C3Vector direction(light.m_direction[0], light.m_direction[1], light.m_direction[2]);
        C3Vector rotated = direction * view;
        target.m_direction[0] = rotated.x;
        target.m_direction[1] = rotated.y;
        target.m_direction[2] = rotated.z;
        target.m_direction[3] = 0.0f;
        target.m_range = 10000.0f;
        memcpy(target.m_attenuation, light.m_attenuation, sizeof(target.m_attenuation));
    }

    float fogRange = this->m_hw.m_fogEnd - this->m_hw.m_fogStart;
    block.m_fog[0] = this->m_hw.m_fogStart;
    block.m_fog[1] = this->m_hw.m_fogEnd;
    block.m_fog[2] = fabsf(fogRange) > 0.0001f ? 1.0f / fogRange : 0.0f;
    block.m_pointScale[0] = this->m_hw.m_pointScaleAttenuation[0];
    block.m_pointScale[1] = this->m_hw.m_pointScaleAttenuation[1];
    block.m_pointScale[2] = this->m_hw.m_pointScaleAttenuation[2];
    block.m_pointScale[3] = this->m_hw.m_viewport[3];

    [encoder setVertexBytes:&block length:sizeof(block) atIndex:GxMetal::VS_BUFFER_FIXED_FUNCTION];
    this->m_fixedVertexDirty = false;
}

// CGxDeviceMetal.cpp:2790 / Device.mm:1755 - one draw; Metal has no triangle fans, so those are
// expanded into an index list in the upload heap
void CGxDeviceMetalNative::IDraw(const CGxBatch* batch, bool indexed, uint32_t baseVertexIndex, uint32_t primitiveCount) {
    id<MTLRenderCommandEncoder> encoder = this->m_metal->RenderEncoder();

    if (!encoder) {
        return;
    }

    
    if (indexed && (!this->m_indexBuf || !this->m_indexBuf->Valid())) {
        return;
    }

    MTLPrimitiveType primitive;
    uint32_t count;

    switch (batch->m_primType) {
        case GxPrim_Points:
            primitive = MTLPrimitiveTypePoint;
            count = primitiveCount;
            break;
        case GxPrim_Lines:
            primitive = MTLPrimitiveTypeLine;
            count = primitiveCount * 2;
            break;
        case GxPrim_LineStrip:
            primitive = MTLPrimitiveTypeLineStrip;
            count = primitiveCount + 1;
            break;
        case GxPrim_Triangles:
            primitive = MTLPrimitiveTypeTriangle;
            count = primitiveCount * 3;
            break;
        case GxPrim_TriangleStrip:
            primitive = MTLPrimitiveTypeTriangleStrip;
            count = primitiveCount + 2;
            break;
        case GxPrim_TriangleFan:
            primitive = MTLPrimitiveTypeTriangle;
            count = primitiveCount * 3;
            break;
        default:
            return;
    }

    if (!count) {
        return;
    }

    // Indexed: the index buffer's base within its pool plus the batch's start, as DrawIndexedPrimitive's
    // StartIndex. Not indexed: the first vertex is the base vertex index, as DrawPrimitive's
    // StartVertex (CGxDeviceD3d::IDraw); the last index buffer's offset belongs to another draw
    uint32_t first = indexed ? (this->m_primIndexBuf ? (this->m_primIndexBuf->m_index >> 1) : 0) + batch->m_start : baseVertexIndex;

    if (batch->m_primType == GxPrim_TriangleFan) {
        // Metal has no triangle fans; expand into a scratch index list in the upload heap
        GxMetal::UploadAllocation fan = this->m_metal->AllocateUpload(primitiveCount * 3 * 4, 4);

        if (!fan.m_data) {
            return;
        }

        uint32_t* out = reinterpret_cast<uint32_t*>(fan.m_data);
        const uint16_t* shadow = (indexed && this->m_indexBuf) ? reinterpret_cast<const uint16_t*>(this->m_indexBuf->m_shadow.data()) : nullptr;
        uint32_t available = (indexed && this->m_indexBuf) ? this->m_indexBuf->m_length / 2 : 0;

        // The draw adds the base vertex itself, so a fan without indices counts from 0
        uint32_t fanFirst = indexed ? first : 0;

        for (uint32_t i = 0; i < primitiveCount; i++) {
            uint32_t a = fanFirst;
            uint32_t b = fanFirst + i + 1;
            uint32_t c = fanFirst + i + 2;
            out[i * 3 + 0] = (shadow && a < available) ? shadow[a] : a;
            out[i * 3 + 1] = (shadow && b < available) ? shadow[b] : b;
            out[i * 3 + 2] = (shadow && c < available) ? shadow[c] : c;
        }

        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:primitiveCount * 3 indexType:MTLIndexTypeUInt32 indexBuffer:fan.m_buffer indexBufferOffset:fan.m_offset instanceCount:1 baseVertex:baseVertexIndex baseInstance:0];
        return;
    }

    if (indexed) {
        // Some skins have a section past the end of their indices (the Azuremyst sky); such a draw is
        // undefined in D3D9 and would read past the buffer in Metal, so it is skipped
        if ((static_cast<uint64_t>(first) + count) * 2 > this->m_indexBuf->m_length) {
            return;
        }

        this->m_indexBuf->m_use.m_lastUse = this->m_indexBuf->m_use.m_lastRenderUse = this->m_metal->CurrentSerial();
        [encoder drawIndexedPrimitives:primitive indexCount:count indexType:MTLIndexTypeUInt16 indexBuffer:this->m_indexBuf->m_buffer indexBufferOffset:first * 2 instanceCount:1 baseVertex:baseVertexIndex baseInstance:0];
        return;
    }

    [encoder drawPrimitives:primitive vertexStart:first vertexCount:count];
}

// The Metal query is made on first use: the client creates its queries before the device has a context
GxMetal::Query* CGxDeviceMetalNative::IQueryData(CGxQuery* query) {
    if (!query || !this->m_metal) {
        return nullptr;
    }

    if (!query->m_apiSpecific) {
        query->m_apiSpecific = new GxMetal::Query(this->m_metal, GxMetal::Query::Type_Occlusion);
    }

    return static_cast<GxMetal::Query*>(query->m_apiSpecific);
}

void CGxDeviceMetalNative::IReleaseQueries() {
    for (CGxQuery* query = this->m_queryList.Head(); query; query = this->m_queryList.Next(query)) {
        if (query->m_apiSpecific) {
            static_cast<GxMetal::Query*>(query->m_apiSpecific)->Release();
            query->m_apiSpecific = nullptr;
        }
    }
}

void CGxDeviceMetalNative::QueryDestroy(CGxQuery*& query) {
    if (query && query->m_apiSpecific) {
        static_cast<GxMetal::Query*>(query->m_apiSpecific)->Release();
        query->m_apiSpecific = nullptr;
    }

    CGxDevice::QueryDestroy(query);
}

bool CGxDeviceMetalNative::QueryBegin(CGxQuery* query) {
    GxMetal::Query* native = this->IQueryData(query);

    if (!native) {
        return false;
    }

    native->Begin();
    return true;
}

bool CGxDeviceMetalNative::QueryEnd(CGxQuery* query) {
    GxMetal::Query* native = this->IQueryData(query);

    if (!native) {
        return false;
    }

    native->End();
    return true;
}

// As the Direct3D device: Unk0 is the size of the result, Unk1 whether it is available (without
// flushing); the call itself reports false for Unk1, which the client ignores
bool CGxDeviceMetalNative::QueryGetParam(CGxQuery* query, EGxQueryParam param, uint32_t& data) {
    GxMetal::Query* native = this->IQueryData(query);

    if (!native) {
        return false;
    }

    switch (param) {
        case GxQueryParam_Unk0:
            data = sizeof(uint32_t);
            return true;

        case GxQueryParam_Unk1:
            data = native->Result(nullptr, false);
            return false;

        default:
            return false;
    }
}

// Waits for the result, as the Direct3D device's D3DGETDATA_FLUSH loop does
bool CGxDeviceMetalNative::QueryGetData(CGxQuery* query, uint32_t& data) {
    GxMetal::Query* native = this->IQueryData(query);

    if (!native) {
        return false;
    }

    while (!native->Result(&data, true)) {
        if (native->m_state == GxMetal::Query::State_Building) {
            return false;
        }

        this->m_metal->WaitForSerial(native->m_serial);
    }

    return true;
}

void CGxDeviceMetalNative::StereoSetConvergence(float convergence) {
}

float CGxDeviceMetalNative::StereoGetConvergence() {
    return 0.0f;
}

void CGxDeviceMetalNative::StereoSetSeparation(float separation) {
}

float CGxDeviceMetalNative::StereoGetSeparation() {
    return 0.0f;
}

bool CGxDeviceMetalNative::StereoEnabled() {
    return false;
}

void CGxDeviceMetalNative::DeviceUnk81(uint32_t a, uint32_t b) {
}

void CGxDeviceMetalNative::DeviceUnk82() {
}

void CGxDeviceMetalNative::DeviceUnk83(uint32_t a, uint32_t b) {
}

// The device the client creates for GxApi_Metal
CGxDevice* CreateCGxDeviceMetal() {
    return new CGxDeviceMetalNative();
}

// Cursor

static float s_cursorScale = 1.0f;

// The client's 32x32 cursor (A8R8G8B8, stored B, G, R, A) as an NSCursor of scale points per pixel;
// the image carries a bitmap at twice that so Retina displays show it at full resolution
static NSCursor* MakeCursor(const uint32_t* cursor, uint32_t hotspotX, uint32_t hotspotY, float scale) {
    const uint32_t width = 32;
    const uint32_t height = 32;

    NSBitmapImageRep* source = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr pixelsWide:width pixelsHigh:height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:width * 4 bitsPerPixel:32];
    const uint8_t* in = reinterpret_cast<const uint8_t*>(cursor);
    uint8_t* out = source.bitmapData;

    for (uint32_t i = 0; i < width * height; i++) {
        out[i * 4 + 0] = in[i * 4 + 2];
        out[i * 4 + 1] = in[i * 4 + 1];
        out[i * 4 + 2] = in[i * 4 + 0];
        out[i * 4 + 3] = in[i * 4 + 3];
    }

    NSSize size = NSMakeSize(width * scale, height * scale);
    NSBitmapImageRep* scaled = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr pixelsWide:static_cast<NSInteger>(ceil(size.width * 2.0)) pixelsHigh:static_cast<NSInteger>(ceil(size.height * 2.0)) bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:0 bytesPerRow:0 bitsPerPixel:32];
    scaled.size = size;

    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithBitmapImageRep:scaled];
    NSGraphicsContext.currentContext = context;
    context.imageInterpolation = NSImageInterpolationHigh;
    [[NSColor clearColor] set];
    NSRectFillUsingOperation(NSMakeRect(0, 0, size.width, size.height), NSCompositingOperationCopy);
    [source drawInRect:NSMakeRect(0, 0, size.width, size.height) fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0 respectFlipped:YES hints:nil];
    [NSGraphicsContext restoreGraphicsState];

    NSImage* image = [[NSImage alloc] initWithSize:size];
    [image addRepresentation:scaled];

    return [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(hotspotX * scale, hotspotY * scale)];
}

void CGxDeviceMetalNative::ICursorCreate(const CGxFormat& format) {
    CGxDevice::ICursorCreate(format);

    // The device starts out with no cursor image; the client unlocks one shortly after
    this->m_hwCursorDirty = 1;
    this->IUpdateCursor();
}

void CGxDeviceMetalNative::ICursorDestroy() {
    CGxDevice::ICursorDestroy();

    OsMacSetGameCursor(nullptr, true);

    if (this->m_hwCursorImage) {
        CFBridgingRelease(this->m_hwCursorImage);
        this->m_hwCursorImage = nullptr;
    }
}

void CGxDeviceMetalNative::ICursorDraw() {
    if (!this->m_hwCursor) {
        // The software cursor is a draw of its own after the frame's scene
        this->ISceneBegin();
        CGxDevice::ICursorDraw();
        this->ISceneEnd();
        return;
    }

    if (!this->m_hwCursorDirty) {
        return;
    }

    if (this->m_hwCursorImage) {
        CFBridgingRelease(this->m_hwCursorImage);
    }

    this->m_hwCursorImage = (void*)CFBridgingRetain(MakeCursor(this->m_cursor, this->m_cursorHotspotX, this->m_cursorHotspotY, s_cursorScale));
    this->m_hwCursorDirty = 0;
    this->IUpdateCursor();
}

void CGxDeviceMetalNative::CursorSetVisible(int32_t visible) {
    CGxDevice::CursorSetVisible(visible);
    this->IUpdateCursor();
}

void CGxDeviceMetalNative::CursorUnlock(uint32_t x, uint32_t y) {
    CGxDevice::CursorUnlock(x, y);
    this->m_hwCursorDirty = 1;
}

// Over the game view the pointer shows the client's cursor, or nothing when the client hides it or
// draws a software cursor, as WM_SETCURSOR does on Windows
void CGxDeviceMetalNative::IUpdateCursor() {
    bool visible = this->m_hwCursor && this->m_cursorVisible && this->m_hwCursorImage;
    OsMacSetGameCursor(this->m_hwCursorImage, visible);
}

void GxCursorSetScale(float scale) {
    scale = std::clamp(scale, 0.5f, 4.0f);

    if (scale == s_cursorScale) {
        return;
    }

    s_cursorScale = scale;

    CGxDeviceMetalNative* device = dynamic_cast<CGxDeviceMetalNative*>(g_theGxDevicePtr);

    if (device && device->m_hwCursor) {
        device->m_hwCursorDirty = 1;
        device->ICursorDraw();
    }
}
