#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Os/Linux/OsSdl.h"
#include "Engine/Source/Os/Path.h"
#include "Engine/Source/Storm/H/storm.h"

// Direct3D 9 runs on Vulkan through dxvk's native libdxvk_d3d9.so, and the HWND it takes is the SDL_Window*.

typedef IDirect3D9* (*Direct3DCreate9Proc)(UINT sdkVersion);

static const char* const s_deviceKey = "CGxDeviceD3d";

static bool InitVideo() {
    return SDL_WasInit(SDL_INIT_VIDEO) || SDL_InitSubSystem(SDL_INIT_VIDEO) == 0;
}

static SDL_Window* SdlWindow(HWND hwnd) {
    return reinterpret_cast<SDL_Window*>(hwnd);
}

static intptr_t WindowProcSdl(void* window, uint32_t message, uintptr_t wparam, intptr_t lparam) {
    return CGxDeviceD3d::WindowProcD3d(static_cast<HWND>(window), message, wparam, lparam);
}

static void SendSizeEvent(SDL_Window* window) {
    int width = 0;
    int height = 0;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);

    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_WINDOWEVENT;
    event.window.timestamp = SDL_GetTicks();
    event.window.windowID = SDL_GetWindowID(window);
    event.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    event.window.data1 = width;
    event.window.data2 = height;

    WindowProcSdl(window, SDL_WINDOWEVENT, 0, reinterpret_cast<intptr_t>(&event));
}

static void* OpenDxvk() {
    // NODELETE keeps dxvk's threads and statics alive across the load/unload pairs the client does.
    const int flags = RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE;

    char path[4096];
    OsGetExePath(path, sizeof(path));
    SStrPack(path, "libdxvk_d3d9.so", sizeof(path));

    void* lib = dlopen(path, flags);

    if (!lib) {
        CGxDevice::Log("CGxDeviceD3d::ILoadD3dLib(): %s", dlerror());
        lib = dlopen("libdxvk_d3d9.so.0", flags);
    }

    return lib;
}

void CGxDeviceD3d::IUnloadD3dLib(HINSTANCE& d3dLib, LPDIRECT3D9& d3d) {
    if (d3d) {
        d3d->Release();
        d3d = nullptr;
    }

    if (d3dLib) {
        dlclose(d3dLib);
        d3dLib = nullptr;
    }
}

int32_t CGxDeviceD3d::ILoadD3dLib(HINSTANCE& d3dLib, LPDIRECT3D9& d3d) {
    d3dLib = nullptr;
    d3d = nullptr;

    // dxvk takes its window system from the environment, and its SDL2 must be the client's.
    setenv("DXVK_WSI_DRIVER", "SDL2", 0);

    if (!InitVideo()) {
        CGxDevice::Log("CGxDeviceD3d::ILoadD3dLib(): unable to init SDL video: %s", SDL_GetError());
        return 0;
    }

    d3dLib = static_cast<HINSTANCE>(OpenDxvk());

    if (d3dLib) {
        Direct3DCreate9Proc d3dCreateProc = reinterpret_cast<Direct3DCreate9Proc>(dlsym(d3dLib, "Direct3DCreate9"));

        if (d3dCreateProc) {
            d3d = d3dCreateProc(D3D_SDK_VERSION);

            if (d3d && d3d->GetAdapterCount()) {
                return 1;
            }

            CGxDevice::Log("CGxDeviceD3d::ILoadD3dLib(): no Vulkan adapter");
        } else {
            CGxDevice::Log("CGxDeviceD3d::ILoadD3dLib(): unable to find Direct3DCreate9");
        }
    } else {
        CGxDevice::Log("CGxDeviceD3d::ILoadD3dLib(): unable to load libdxvk_d3d9.so: %s", dlerror());
    }

    CGxDeviceD3d::IUnloadD3dLib(d3dLib, d3d);

    return 0;
}

// dxvk has no hardware cursor here, so the device always draws the cursor itself.
void CGxDeviceD3d::CursorSetVisible(int32_t visible) {
    CGxDevice::CursorSetVisible(visible);
}

int32_t CGxDeviceD3d::IHwCursorCaps() {
    return 0;
}

void CGxDeviceD3d::ISetWindowFocus(int32_t focus) {
    this->m_windowFocus = focus;

    if (focus) {
        this->m_hwCursorDirty = 1;

        if (!this->m_format.m_window && this->m_hwnd) {
            SDL_SetWindowGrab(SdlWindow(this->m_hwnd), SDL_TRUE);
        }
    }
}

// Initializing SDL video stands in for registering the window class.
ATOM CGxDeviceD3d::IRegisterWindowClass() {
    return InitVideo() ? 1 : 0;
}

void CGxDeviceD3d::IUnregisterWindowClass() {
    this->m_hwndClass = 0;
}

int32_t CGxDeviceD3d::ICreateWindow(CGxFormat& format) {
    uint32_t flags = SDL_WINDOW_VULKAN;

    if (!format.m_window || format.m_maximize == 1) {
        flags |= SDL_WINDOW_BORDERLESS;
    } else if (format.m_maximize == 2) {
        flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
    } else {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    float aspectRatio = 0.0f;

    if (format.m_aspect && format.m_size.x && format.m_size.y) {
        aspectRatio = static_cast<float>(format.m_size.x) / format.m_size.y;
    }

    // SDL window sizes are client sizes, the window manager adds the frame.
    CGxDevice::s_clientAdjust.x = 0;
    CGxDevice::s_clientAdjust.y = 0;

    if (format.m_window) {
        if (format.m_maximize == 1) {
            SDL_Rect bounds;

            if (SDL_GetDisplayBounds(0, &bounds) == 0) {
                format.m_pos.x = bounds.x;
                format.m_pos.y = bounds.y;
                format.m_size.x = bounds.w;
                format.m_size.y = bounds.h;
            } else {
                format.m_pos.x = 0;
                format.m_pos.y = 0;
            }
        } else {
            RECT windowRect;
            windowRect.left = 0;
            windowRect.top = 0;
            windowRect.right = format.m_size.x;
            windowRect.bottom = format.m_size.y;

            if (CGxDevice::AdjustWindowRectToWorkArea(aspectRatio, &CGxDevice::s_clientAdjust, &windowRect)) {
                format.m_pos.x = windowRect.left;
                format.m_pos.y = windowRect.top;
                format.m_size.x = windowRect.right - windowRect.left;
                format.m_size.y = windowRect.bottom - windowRect.top;
            } else {
                format.m_pos.x = 0;
                format.m_pos.y = 0;
            }
        }
    }

    int32_t width = format.m_size.x ? format.m_size.x : 640;
    int32_t height = format.m_size.y ? format.m_size.y : 480;
    int32_t x = format.m_window ? format.m_pos.x : static_cast<int32_t>(SDL_WINDOWPOS_UNDEFINED_DISPLAY(0));
    int32_t y = format.m_window ? format.m_pos.y : static_cast<int32_t>(SDL_WINDOWPOS_UNDEFINED_DISPLAY(0));

    SDL_Window* window = SDL_CreateWindow("World of Warcraft", x, y, width, height, flags);

    if (!window) {
        CGxDevice::Log("CGxDeviceD3d::ICreateWindow(): SDL_CreateWindow failed: %s", SDL_GetError());
        return 0;
    }

    this->m_hwnd = reinterpret_cast<HWND>(window);
    SDL_SetWindowData(window, s_deviceKey, this);
    OsSdlSetWindowProc(window, WindowProcSdl);
    SDL_ShowCursor(SDL_DISABLE);

    // CreateWindowEx delivers WM_SIZE before it returns, SDL reports no initial size.
    SendSizeEvent(window);

    return 1;
}

void CGxDeviceD3d::IClientToScreen(POINT* points, uint32_t count) {
    int x = 0;
    int y = 0;
    SDL_GetWindowPosition(SdlWindow(this->m_hwnd), &x, &y);

    for (uint32_t i = 0; i < count; i++) {
        points[i].x += x;
        points[i].y += y;
    }
}

void CGxDeviceD3d::IHideWindow() {
    if (this->m_hwnd) {
        SDL_HideWindow(SdlWindow(this->m_hwnd));
    }
}

void CGxDeviceD3d::IDestroyWindow() {
    if (this->m_hwnd) {
        // dxvk's swapchain uses the window until the device is released.
        if (this->m_d3dDevice) {
            this->IDestroyD3d();
        }

        SDL_Window* window = SdlWindow(this->m_hwnd);
        this->DeviceWM(GxWM_Destroy, 0, 0);
        OsSdlSetWindowProc(window, nullptr);
        SDL_SetWindowData(window, s_deviceKey, nullptr);
        SDL_DestroyWindow(window);
    }

    this->m_hwnd = nullptr;
}

// SDL only has per-window gamma ramps, so the system ramp stays linear like on the OpenGL device.
void CGxDeviceD3d::IReadSystemGammaRamp() {
    this->m_gammaRamp = this->m_systemGammaRamp;
}

bool CGxDeviceD3d::IProbeLock(char* data, bool write) {
    return true;
}

void CGxDeviceD3d::IStereoInit(const CGxFormat& format) {
}

void CGxDeviceD3d::IStereoCreateHandle() {
}

void CGxDeviceD3d::IStereoUpdate() {
}

void CGxDeviceD3d::IStereoDestroyHandle() {
}

void CGxDeviceD3d::IStereoRestore() {
}

// The window messages the Windows proc handles, from SDL window events: SIZE_CHANGED and friends for WM_SIZE,
// FOCUS_GAINED/LOST for WM_ACTIVATE with WM_SETFOCUS/WM_KILLFOCUS, DISPLAY_CHANGED for WM_DISPLAYCHANGE.
LRESULT CALLBACK CGxDeviceD3d::WindowProcD3d(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    SDL_Window* window = SdlWindow(hwnd);
    CGxDeviceD3d* device = window ? static_cast<CGxDeviceD3d*>(SDL_GetWindowData(window, s_deviceKey)) : nullptr;
    const SDL_Event* event = reinterpret_cast<const SDL_Event*>(lparam);

    if (device && message == SDL_WINDOWEVENT && event) {
        switch (event->window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MAXIMIZED: {
                int width = 0;
                int height = 0;
                SDL_Vulkan_GetDrawableSize(window, &width, &height);

                CRect rect;
                rect.minY = 0.0f;
                rect.minX = 0.0f;
                rect.maxY = static_cast<float>(height);
                rect.maxX = static_cast<float>(width);
                device->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), 0);
                break;
            }

            case SDL_WINDOWEVENT_MINIMIZED: {
                CRect rect;
                rect.minY = 0.0f;
                rect.minX = 0.0f;
                rect.maxY = 0.0f;
                rect.maxX = 0.0f;
                device->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), 1);
                break;
            }

            case SDL_WINDOWEVENT_FOCUS_GAINED:
                if (!device->IDevIsWindowed()) {
                    CRect rect;
                    rect.minY = 0.0f;
                    rect.minX = 0.0f;
                    rect.maxY = 0.0f;
                    rect.maxX = 0.0f;
                    GxCapsWindowSizeInScreenCoords(rect);
                    device->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), 3);
                }

                device->DeviceWM(GxWM_SetFocus, 0, 0);
                break;

            case SDL_WINDOWEVENT_FOCUS_LOST:
                if (!device->IDevIsWindowed()) {
                    SDL_SetWindowGrab(window, SDL_FALSE);

                    CRect rect;
                    rect.minY = 0.0f;
                    rect.minX = 0.0f;
                    rect.maxY = 0.0f;
                    rect.maxX = 0.0f;
                    device->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), 1);
                }

                device->DeviceWM(GxWM_KillFocus, 0, 0);
                break;

            case SDL_WINDOWEVENT_DISPLAY_CHANGED:
                if (!device->IDevIsWindowed()) {
                    SDL_DisplayMode mode;

                    if (SDL_GetCurrentDisplayMode(event->window.data1, &mode) == 0) {
                        CRect rect;
                        rect.minY = 0.0f;
                        rect.minX = 0.0f;
                        rect.maxY = static_cast<float>(mode.h);
                        rect.maxX = static_cast<float>(mode.w);
                        device->DeviceWM(GxWM_DisplayChange, reinterpret_cast<uintptr_t>(&rect), 0);
                    }
                }

                break;

            default:
                break;
        }
    }

    if (device && device->m_windowProc) {
        return device->m_windowProc(window, message, wparam, lparam);
    }

    return 0;
}

#endif
