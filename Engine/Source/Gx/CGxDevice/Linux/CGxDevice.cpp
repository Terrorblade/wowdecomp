#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <SDL.h>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
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

// The Direct3D devices are not built on Linux; CreateGxDevice creates the OpenGL device for those APIs, so they
// offer the OpenGL formats.
int32_t CGxDevice::D3dAdapterFormats(TSGrowableArray<CGxFormat>& formats) {
    return CGxDevice::OpenGlAdapterFormats(formats);
}

int32_t CGxDevice::D3d9ExAdapterFormats(TSGrowableArray<CGxFormat>& formats) {
    return CGxDevice::OpenGlAdapterFormats(formats);
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

#endif
