#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <SDL.h>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Gx.h"
#include "HoT/Runtime/Display.h"

static bool InitVideo() {
    return SDL_WasInit(SDL_INIT_VIDEO) || SDL_InitSubSystem(SDL_INIT_VIDEO) == 0;
}

// Windows reports 32 bpp for X8R8G8B8 modes, SDL reports 24 bits of color; count the storage like Windows.
static uint32_t ModeBpp(uint32_t format) {
    return SDL_BYTESPERPIXEL(format) * 8;
}

int32_t CGxDevice::AdapterFormats(EGxApi api, TSGrowableArray<CGxFormat>& formats) {
    formats.SetCount(0);
    formats.m_chunk = 64;
    formats.Reserve(256, 0);

    switch (api) {
        case GxApi_OpenGl:
            CGxDevice::OpenGlAdapterFormats(formats);
            break;

        case GxApi_D3d9:
            CGxDevice::D3dAdapterFormats(formats);
            break;

        case GxApi_D3d9Ex:
            CGxDevice::D3d9ExAdapterFormats(formats);
            break;

        default:
            break;
    }

    return formats.Count() != 0;
}

int32_t CGxDevice::AdjustWindowRectToWorkArea(float aspectRatio, C2iVector* adjust, tagRECT* rect) {
    if (!InitVideo()) {
        return 0;
    }

    SDL_Rect bounds;

    if (SDL_GetDisplayUsableBounds(0, &bounds) != 0) {
        return 0;
    }

    RECT work;
    work.left = bounds.x;
    work.top = bounds.y;
    work.right = bounds.x + bounds.w;
    work.bottom = bounds.y + bounds.h;

    int32_t height = rect->bottom - rect->top;
    int32_t width = rect->right - rect->left;

    rect->top = work.top;
    rect->left = work.left;
    rect->right = width + work.left + adjust->x;
    rect->bottom = height + work.top + adjust->y;

    if (rect->right > work.right) {
        rect->right = work.right;

        if (aspectRatio != 0.0f) {
            int32_t workWidth = work.right - work.left;
            int32_t maxWidth = workWidth - adjust->x;
            int32_t newWidth = maxWidth;
            float invAspect = 1.0f / aspectRatio;

            for (; newWidth >= width / 2; newWidth--) {
                float newHeight = newWidth * invAspect;

                if (newHeight - floor(newHeight) < 0.01f) {
                    break;
                }
            }

            double newHeight;

            if (newWidth < width / 2) {
                newHeight = maxWidth / aspectRatio + 0.5f;
            } else {
                workWidth = newWidth + adjust->x;
                newHeight = newWidth * invAspect;
            }

            rect->right = work.left + workWidth;
            rect->bottom = work.top + static_cast<int32_t>(adjust->y + floor(newHeight));
        }
    }

    if (rect->bottom > work.bottom) {
        rect->bottom = work.bottom;

        if (aspectRatio != 0.0f) {
            int32_t workHeight = work.bottom - work.top;
            int32_t maxHeight = workHeight - adjust->y;
            int32_t newHeight = maxHeight;

            for (; newHeight >= height / 2; newHeight--) {
                float newWidth = newHeight * aspectRatio;

                if (newWidth - floor(newWidth) < 0.01f) {
                    break;
                }
            }

            int32_t newWidth;

            if (newHeight < height / 2) {
                newWidth = static_cast<int32_t>(floor(maxHeight * aspectRatio + 0.5f));
            } else {
                workHeight = adjust->y + newHeight;
                newWidth = static_cast<int32_t>(floor(newHeight * aspectRatio));
            }

            rect->right = work.left + adjust->x + newWidth;
            rect->bottom = work.top + workHeight;
        }
    }

    int32_t spaceY = work.bottom - rect->bottom;
    int32_t spaceX = work.right - rect->right;

    rect->right += spaceX / 2;
    rect->left = spaceX / 2 + work.left;
    rect->bottom += spaceY / 2;
    rect->top = spaceY / 2 + work.top;
    rect->right -= adjust->x;
    rect->bottom -= adjust->y;

    return 1;
}

static uint16_t ReadSysfsHex(const char* path) {
    FILE* file = fopen(path, "r");

    if (!file) {
        return 0;
    }

    unsigned int value = 0;

    if (fscanf(file, "%x", &value) != 1) {
        value = 0;
    }

    fclose(file);

    return static_cast<uint16_t>(value);
}

// The PCI IDs of the primary display adapter come from the DRM nodes in sysfs: the card the firmware booted on
// (boot_vga) is the primary one, otherwise the first card with PCI IDs. The driver version is not reported.
int32_t CGxDevice::DeviceAdapterID(uint16_t& vendorID, uint16_t& deviceID, uint32_t& driverVersionHi, uint32_t& driverVersionLo) {
    int32_t result = 0;
    vendorID = 0xFFFF;
    deviceID = 0xFFFF;
    driverVersionHi = 0;
    driverVersionLo = 0;

    uint16_t firstVendor = 0;
    uint16_t firstDevice = 0;

    for (int32_t i = 0; i < 16; i++) {
        char path[128];

        snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/vendor", i);
        uint16_t vendor = ReadSysfsHex(path);

        snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/device", i);
        uint16_t id = ReadSysfsHex(path);

        if (!vendor || !id) {
            continue;
        }

        if (!firstVendor) {
            firstVendor = vendor;
            firstDevice = id;
        }

        snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/boot_vga", i);

        if (ReadSysfsHex(path) == 1) {
            firstVendor = vendor;
            firstDevice = id;
            break;
        }
    }

    if (firstVendor && firstDevice) {
        vendorID = firstVendor;
        deviceID = firstDevice;
        result = 1;
    }

    CGxDevice::Log("CGxDevice::DeviceAdapterID(): RET: %d, VID: %x, DID: %x, DVER: %x.%x", result, vendorID, deviceID, driverVersionHi, driverVersionLo);

    return result;
}

// Windows infers a hardware class from the D3D9 caps. Every OpenGL 2.0 driver has hardware T&L, more than two
// texture units and pixel shader 2.0 class programs, so report the highest class whenever a display is available.
int32_t CGxDevice::DeviceAdapterInfer(uint16_t& deviceID) {
    int32_t result = 0;
    deviceID = 0;

    if (InitVideo() && SDL_GetNumVideoDisplays() > 0) {
        deviceID = 3;
        result = 1;
    }

    CGxDevice::Log("CGxDevice::DeviceAdapterInfer(): RET: %d, DID: %x", result, deviceID);

    return result;
}

static int CGxMonitorModeSort(const void* a, const void* b) {
    const CGxMonitorMode* modeA = static_cast<const CGxMonitorMode*>(a);
    const CGxMonitorMode* modeB = static_cast<const CGxMonitorMode*>(b);

    return modeA->m_size.x * modeA->m_size.y - modeB->m_size.x * modeB->m_size.y;
}

int32_t CGxDevice::AdapterDesktopMode(CGxMonitorMode& mode) {
    if (!InitVideo()) {
        return 0;
    }

    SDL_DisplayMode displayMode;

    if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0) {
        return 0;
    }

    mode.m_size.x = displayMode.w;
    mode.m_size.y = displayMode.h;
    mode.m_refreshRate = displayMode.refresh_rate;
    mode.m_bpp = ModeBpp(displayMode.format);

    return 1;
}

static const D3DFORMAT s_d3dAdapterFormats[] = { D3DFMT_R5G6B5, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8 };
static const D3DFORMAT s_d3dDepthFormats[] = { D3DFMT_D16, D3DFMT_D24X8, D3DFMT_D24S8, D3DFMT_D32 };
static const CGxFormat::Format s_gxDepthFormats[] = { CGxFormat::Fmt_Ds160, CGxFormat::Fmt_Ds24X, CGxFormat::Fmt_Ds248, CGxFormat::Fmt_Ds320 };
static const uint32_t s_multisampleCounts[] = { 1, 2, 4, 6, 8, 10, 12, 14, 16 };

static D3DMULTISAMPLE_TYPE MultisampleType(uint32_t count) {
    return count == 1 ? D3DMULTISAMPLE_NONE : static_cast<D3DMULTISAMPLE_TYPE>(count);
}

// The same enumeration as the Windows D3D9 device, answered by dxvk.
int32_t CGxDevice::D3dAdapterFormats(TSGrowableArray<CGxFormat>& formats) {
    HINSTANCE d3dLib = nullptr;
    LPDIRECT3D9 d3d = nullptr;

    if (!CGxDeviceD3d::ILoadD3dLib(d3dLib, d3d)) {
        return 0;
    }

    for (D3DFORMAT adapterFormat : s_d3dAdapterFormats) {
        uint32_t modeCount = d3d->GetAdapterModeCount(0, adapterFormat);

        for (uint32_t mode = 0; mode < modeCount; mode++) {
            D3DDISPLAYMODE displayMode;

            if (d3d->EnumAdapterModes(0, adapterFormat, mode, &displayMode) != D3D_OK) {
                continue;
            }

            CGxFormat format;
            memset(&format, 0, sizeof(format));

            switch (displayMode.Format) {
                case D3DFMT_A8R8G8B8:
                    format.m_colorFormat = CGxFormat::Fmt_Argb8888;
                    break;

                case D3DFMT_X8R8G8B8:
                    format.m_colorFormat = CGxFormat::Fmt_ArgbX888;
                    break;

                case D3DFMT_R5G6B5:
                    format.m_colorFormat = CGxFormat::Fmt_Rgb565;
                    break;

                default:
                    continue;
            }

            for (uint32_t depth = 0; depth < 4; depth++) {
                if (FAILED(d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, displayMode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, s_d3dDepthFormats[depth]))) {
                    continue;
                }

                format.m_depthFormat = s_gxDepthFormats[depth];

                for (uint32_t samples : s_multisampleCounts) {
                    if (FAILED(d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, displayMode.Format, FALSE, MultisampleType(samples), nullptr))) {
                        continue;
                    }

                    if (FAILED(d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, s_d3dDepthFormats[depth], FALSE, MultisampleType(samples), nullptr))) {
                        continue;
                    }

                    format.m_multisampleCount = samples;

                    if (!CGxDevice::FormatExists(formats, format)) {
                        *formats.New() = format;
                    }
                }
            }
        }
    }

    CGxDeviceD3d::IUnloadD3dLib(d3dLib, d3d);

    return formats.Count() != 0;
}

// The Direct3D 9Ex device is not built here.
int32_t CGxDevice::D3d9ExAdapterFormats(TSGrowableArray<CGxFormat>& formats) {
    return 0;
}

int32_t CGxDevice::AdapterMonitorModes(TSGrowableArray<CGxMonitorMode>& modes) {
    // HoT: reuse the modes until the display changes, enumerating them is slow.
    if (HoT::Display::GetCachedMonitorModes(modes)) {
        return 1;
    }

    modes.SetCount(0);

    if (!InitVideo()) {
        return 0;
    }

    int32_t count = SDL_GetNumDisplayModes(0);

    for (int32_t i = 0; i < count; i++) {
        SDL_DisplayMode displayMode;

        if (SDL_GetDisplayMode(0, i, &displayMode) != 0) {
            continue;
        }

        uint32_t bpp = ModeBpp(displayMode.format);

        if (displayMode.w >= 640 && displayMode.h >= 480 && bpp >= 16) {
            CGxMonitorMode* mode = modes.New();
            mode->m_size.x = displayMode.w;
            mode->m_size.y = displayMode.h;
            mode->m_bpp = bpp;
            mode->m_refreshRate = displayMode.refresh_rate;
        }
    }

    qsort(modes.Ptr(), modes.Count(), sizeof(CGxMonitorMode), CGxMonitorModeSort);
    HoT::Display::CacheMonitorModes(modes);

    return modes.Count() != 0;
}

int32_t CGxDevice::DeviceRenderAspect(float& aspect) {
    return 0;
}

// Direct3D 9 through dxvk when it finds a Vulkan adapter, the OpenGL device otherwise.
static bool D3dAvailable() {
    static int32_t s_available = -1;

    if (s_available < 0) {
        HINSTANCE d3dLib = nullptr;
        LPDIRECT3D9 d3d = nullptr;
        s_available = CGxDeviceD3d::ILoadD3dLib(d3dLib, d3d);
        CGxDeviceD3d::IUnloadD3dLib(d3dLib, d3d);
        CGxDevice::Log("Direct3D 9 through dxvk: %s", s_available ? "available" : "unavailable, using OpenGL");
    }

    return s_available != 0;
}

EGxApi GxPosixDefaultApi() {
    return D3dAvailable() ? GxApi_D3d9 : GxApi_OpenGl;
}

int32_t GxPosixApiSupported(EGxApi api) {
    return api == GxApi_OpenGl || (api == GxApi_D3d9 && D3dAvailable());
}

CGxDevice* GxPosixCreateDevice(EGxApi api) {
    if (api == GxApi_D3d9 && D3dAvailable()) {
        return CreateCGxDeviceD3d();
    }

    return CreateCGxDeviceOpenGl();
}

#endif
