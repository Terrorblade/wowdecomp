#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "HoT/Runtime/Display.h"

// The adapter queries of CGxDevice for macOS: the Metal device and the main display stand for the
// Direct3D adapter and the Windows display device.

static const CGxFormat::Format s_gxDepthFormats[4] = { CGxFormat::Fmt_Ds160, CGxFormat::Fmt_Ds24X, CGxFormat::Fmt_Ds248, CGxFormat::Fmt_Ds320 };
static const uint32_t s_multisampleCounts[4] = { 1, 2, 4, 8 };

int32_t CGxDevice::AdapterFormats(EGxApi api, TSGrowableArray<CGxFormat>& formats) {
    formats.SetCount(0);
    formats.m_chunk = 64;
    formats.Reserve(256, 0);

    if (api != GxApi_Metal) {
        return 0;
    }

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    if (!device) {
        return 0;
    }

    static const CGxFormat::Format s_colorFormats[2] = { CGxFormat::Fmt_ArgbX888, CGxFormat::Fmt_Argb8888 };

    for (CGxFormat::Format colorFormat : s_colorFormats) {
        for (uint32_t depth = 0; depth < 4; depth++) {
            for (uint32_t samples : s_multisampleCounts) {
                if (samples > 1 && ![device supportsTextureSampleCount:samples]) {
                    continue;
                }

                CGxFormat format;
                memset(&format, 0, sizeof(format));
                format.m_colorFormat = colorFormat;
                format.m_depthFormat = s_gxDepthFormats[depth];
                format.m_multisampleCount = samples;

                if (!CGxDevice::FormatExists(formats, format)) {
                    *formats.New() = format;
                }
            }
        }
    }

    return formats.Count() != 0;
}

int32_t CGxDevice::DeviceAdapterID(uint16_t& vendorID, uint16_t& deviceID, uint32_t& driverVersionHi, uint32_t& driverVersionLo) {
    // Metal has no adapter identifiers; the GPU's registry id stands in for them and the driver has
    // no version number to report
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    vendorID = 0x106B;  // Apple
    deviceID = device ? static_cast<uint16_t>(device.registryID & 0xFFFF) : 0xFFFF;
    driverVersionHi = 0;
    driverVersionLo = 0;

    CGxDevice::Log("CGxDevice::DeviceAdapterID(): RET: 1, VID: %x, DID: %x", vendorID, deviceID);
    return device ? 1 : 0;
}

int32_t CGxDevice::DeviceAdapterInfer(uint16_t& deviceID) {
    // Metal devices always have hardware transform and lighting, eight texture units and shader
    // model 3, which the Windows classification reports as its highest tier
    deviceID = 3;
    CGxDevice::Log("CGxDevice::DeviceAdapterInfer(): RET: 1, DID: 3");
    return 1;
}

int32_t CGxDevice::AdapterDesktopMode(CGxMonitorMode& mode) {
    CGDisplayModeRef current = CGDisplayCopyDisplayMode(CGMainDisplayID());
    if (!current) {
        return 0;
    }

    // Pixels, which on Retina displays are twice the points of the desktop resolution
    mode.m_size.x = static_cast<int32_t>(CGDisplayModeGetPixelWidth(current));
    mode.m_size.y = static_cast<int32_t>(CGDisplayModeGetPixelHeight(current));
    double refresh = CGDisplayModeGetRefreshRate(current);
    mode.m_refreshRate = refresh > 0.0 ? static_cast<uint32_t>(refresh + 0.5) : 60;
    mode.m_bpp = 32;

    CGDisplayModeRelease(current);
    return 1;
}

static int CGxMonitorModeSort(const void* a, const void* b) {
    const CGxMonitorMode* modeA = static_cast<const CGxMonitorMode*>(a);
    const CGxMonitorMode* modeB = static_cast<const CGxMonitorMode*>(b);

    return modeA->m_size.x * modeA->m_size.y - modeB->m_size.x * modeB->m_size.y;
}

int32_t CGxDevice::AdapterMonitorModes(TSGrowableArray<CGxMonitorMode>& modes) {
    // HoT: reuse the modes until the display changes, enumerating them is slow.
    if (HoT::Display::GetCachedMonitorModes(modes)) {
        return 1;
    }

    modes.SetCount(0);

    CGDirectDisplayID display = CGMainDisplayID();
    CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);
    uint32_t refreshRate = 60;
    if (current) {
        double refresh = CGDisplayModeGetRefreshRate(current);
        refreshRate = refresh > 0.0 ? static_cast<uint32_t>(refresh + 0.5) : 60;
    }

    // Fullscreen scales the rendered image to the display without changing its mode, so every
    // resolution the display offers, at its pixel size, is available
    NSDictionary* options = @{ (__bridge NSString*)kCGDisplayShowDuplicateLowResolutionModes: @YES };
    CFArrayRef list = CGDisplayCopyAllDisplayModes(display, (__bridge CFDictionaryRef)options);

    auto add = [&](int32_t width, int32_t height) {
        if (width < 640 || height < 480) {
            return;
        }
        for (uint32_t i = 0; i < modes.Count(); i++) {
            if (modes[i].m_size.x == width && modes[i].m_size.y == height) {
                return;
            }
        }
        CGxMonitorMode* mode = modes.New();
        mode->m_size.x = width;
        mode->m_size.y = height;
        mode->m_bpp = 32;
        mode->m_refreshRate = refreshRate;
    };

    if (list) {
        for (CFIndex i = 0; i < CFArrayGetCount(list); i++) {
            CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(list, i);
            if (CGDisplayModeIsUsableForDesktopGUI(mode)) {
                add(static_cast<int32_t>(CGDisplayModeGetPixelWidth(mode)), static_cast<int32_t>(CGDisplayModeGetPixelHeight(mode)));
                add(static_cast<int32_t>(CGDisplayModeGetWidth(mode)), static_cast<int32_t>(CGDisplayModeGetHeight(mode)));
            }
        }
        CFRelease(list);
    }

    if (current) {
        add(static_cast<int32_t>(CGDisplayModeGetPixelWidth(current)), static_cast<int32_t>(CGDisplayModeGetPixelHeight(current)));
        CGDisplayModeRelease(current);
    }

    // The standard resolutions Windows drivers list, as far as the display fits them. The client's
    // default formats are 4:3 sizes such as 1024x768 that a Mac display does not report, and the
    // gxResolution callback rejects a default missing from this list, leaving the CVar without a
    // value on a fresh install.
    static const int32_t s_standard[][2] = {
        { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 }, { 1280, 720 }, { 1280, 800 },
        { 1280, 960 }, { 1280, 1024 }, { 1366, 768 }, { 1440, 900 }, { 1600, 900 }, { 1600, 1200 },
        { 1680, 1050 }, { 1920, 1080 }, { 1920, 1200 }, { 2560, 1440 }, { 2560, 1600 },
    };
    int32_t maxWidth = 0;
    int32_t maxHeight = 0;
    for (uint32_t i = 0; i < modes.Count(); i++) {
        maxWidth = std::max(maxWidth, modes[i].m_size.x);
        maxHeight = std::max(maxHeight, modes[i].m_size.y);
    }
    for (const auto& size : s_standard) {
        if (size[0] <= maxWidth && size[1] <= maxHeight) {
            add(size[0], size[1]);
        }
    }

    qsort(modes.Ptr(), modes.Count(), sizeof(CGxMonitorMode), CGxMonitorModeSort);
    HoT::Display::CacheMonitorModes(modes);

    return modes.Count() != 0;
}

// Fullscreen renders the resolution extended to the display's aspect ratio (CGxDeviceMetalNative::ICreateWindow), so the
// image the device draws into decides the aspect ratio the interface lays out for.
int32_t CGxDevice::DeviceRenderAspect(float& aspect) {
    if (!g_theGxDevicePtr) {
        return 0;
    }

    const CRect& window = g_theGxDevicePtr->DeviceDefWindow();

    if (window.maxX - window.minX < 1.0f || window.maxY - window.minY < 1.0f) {
        return 0;
    }

    aspect = (window.maxX - window.minX) / (window.maxY - window.minY);
    return 1;
}

// Every graphics API the client asks for gets the Metal device.
EGxApi GxPosixDefaultApi() {
    return GxApi_Metal;
}

int32_t GxPosixApiSupported(EGxApi api) {
    return api == GxApi_Metal;
}

CGxDevice* GxPosixCreateDevice(EGxApi api) {
    return CreateCGxDeviceMetal();
}
