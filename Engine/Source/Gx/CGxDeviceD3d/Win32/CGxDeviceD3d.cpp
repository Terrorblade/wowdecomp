#include <cstdint>
#include <cstring>
#include <windows.h>
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Os/Error.h"
#include "ThirdParty/nvapi/nvapi.h"
#include "GameVersion.h"

typedef IDirect3D9* (WINAPI* Direct3DCreate9Proc)(UINT sdkVersion);

// VERIFY: 57303b58 auto
// OFFSET: 0x0068E140
void CGxDeviceD3d::IUnloadD3dLib(HINSTANCE& d3dLib, LPDIRECT3D9& d3d) {
    if (d3d) {
        d3d->Release();
        d3d = nullptr;
    }

    if (d3dLib) {
        FreeLibrary(d3dLib);
        d3dLib = nullptr;
    }
}

// VERIFY: b07d4055 auto
// OFFSET: 0x0068E750
void CGxDeviceD3d::CursorSetVisible(int32_t visible) {
    CGxDevice::CursorSetVisible(visible);

    if (this->m_hwCursor && this->m_context) {
        POINT point;
        GetCursorPos(&point);
        ScreenToClient(this->m_hwnd, &point);

        RECT rect;
        GetClientRect(this->m_hwnd, &rect);

        if (point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom) {
            this->m_d3dDevice->ShowCursor(this->m_cursorVisible);
        }
    }
}

// VERIFY: 012b93fa manual
// OFFSET: 0x0068ED80
int32_t CGxDeviceD3d::ILoadD3dLib(HINSTANCE& d3dLib, LPDIRECT3D9& d3d) {
    d3dLib = nullptr;
    d3d = nullptr;
    d3dLib = LoadLibraryA("d3d9.dll");

    if (d3dLib) {
        Direct3DCreate9Proc d3dCreateProc = reinterpret_cast<Direct3DCreate9Proc>(GetProcAddress(d3dLib, "Direct3DCreate9"));

        if (d3dCreateProc) {
            d3d = d3dCreateProc(D3D_SDK_VERSION);

            if (d3d) {
                return 1;
            }

            CGxDevice::Log("CGxDeviceD3d::ILoadD3dLib(): unable to d3dCreateProc()");
        } else {
            CGxDevice::Log("CGxDeviceD3d::ILoadD3dLib(): unable to GetProcAddress()");
        }
    } else {
        CGxDevice::Log("CGxDeviceD3d::ILoadD3dLib(): unable to LoadLibrary()");
    }

    CGxDeviceD3d::IUnloadD3dLib(d3dLib, d3d);

    return 0;
}

// VERIFY: b78ec2a8 auto
// OFFSET: 0x0069FE10
void CGxDeviceD3d::ISetWindowFocus(int32_t focus) {
    this->m_windowFocus = focus;

    if (focus) {
        this->m_hwCursorDirty = 1;

        if (!this->m_format.m_window) {
            RECT rect;
            GetWindowRect(this->m_hwnd, &rect);
            ClipCursor(&rect);
        }
    }
}

// VERIFY: 284cb5b4 manual
// OFFSET: 0x0068EB20
ATOM CGxDeviceD3d::IRegisterWindowClass() {
    HINSTANCE instance = GetModuleHandleA(nullptr);

    WNDCLASSEXA windowClass;
    memset(&windowClass, 0, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = CGxDeviceD3d::WindowProcD3d;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = "GxWindowClassD3d";
    windowClass.hIcon = static_cast<HICON>(LoadImageA(instance, "BlizzardIcon.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    windowClass.hCursor = LoadCursorA(instance, "BlizzardCursor.cur");

    if (!windowClass.hCursor) {
        windowClass.hCursor = LoadCursorA(instance, IDC_ARROW);
    }

    return RegisterClassExA(&windowClass);
}

// VERIFY: 9db942aa manual
// OFFSET: 0x0068EBB0
int32_t CGxDeviceD3d::ICreateWindow(CGxFormat& format) {
    HINSTANCE instance = GetModuleHandleA(nullptr);
    DWORD style;

    if (!format.m_window) {
        style = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU;
    } else if (format.m_maximize == 1) {
        style = WS_POPUP | WS_VISIBLE;
    } else if (format.m_maximize == 2) {
        style = WS_POPUP;
    } else {
        style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }

    float aspectRatio = 0.0f;

    if (format.m_aspect) {
        int32_t width = format.m_size.x;

        if (width) {
            int32_t height = format.m_size.y;

            if (height) {
                aspectRatio = static_cast<float>(width) / height;
            }
        }
    }

    RECT clientRect;
    clientRect.right = format.m_size.x;
    clientRect.left = 0;
    clientRect.top = 0;
    clientRect.bottom = format.m_size.y;
    AdjustWindowRectEx(&clientRect, style, FALSE, 0);

    CGxDevice::s_clientAdjust.x = clientRect.right - format.m_size.x - clientRect.left;
    CGxDevice::s_clientAdjust.y = clientRect.bottom - format.m_size.y - clientRect.top;

    if (format.m_window) {
        if (format.m_maximize == 1) {
            format.m_pos.x = 0;
            format.m_pos.y = 0;
            format.m_size.x = GetSystemMetrics(SM_CXSCREEN);
            format.m_size.y = GetSystemMetrics(SM_CYSCREEN);
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

    int32_t width = format.m_size.x;
    int32_t height = format.m_size.y;

    if (!width) {
        width = CW_USEDEFAULT;
    }

    if (!height) {
        height = CW_USEDEFAULT;
    }

    if (format.m_window && format.m_maximize != 1 && format.m_size.x && format.m_size.y) {
        width += CGxDevice::s_clientAdjust.x;
        height += CGxDevice::s_clientAdjust.y;
    }

    this->m_hwnd = CreateWindowExA(WS_EX_APPWINDOW, "GxWindowClassD3d", CLIENT_NAME, style, format.m_pos.x, format.m_pos.y, width, height, nullptr, nullptr, instance, this);

    if (this->m_hwnd && format.m_maximize != 2) {
        ShowWindow(this->m_hwnd, SW_SHOWNORMAL);
    }

    return this->m_hwnd != nullptr;
}

// VERIFY: bc1bdcba manual
// OFFSET: 0x006A0360
LRESULT CALLBACK CGxDeviceD3d::WindowProcD3d(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    CGxDeviceD3d* device = reinterpret_cast<CGxDeviceD3d*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (message) {
        case WM_CREATE:
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTA*>(lparam)->lpCreateParams));
            return 0;

        case WM_DESTROY:
            device->DeviceWM(GxWM_Destroy, 0, 0);
            return 0;

        case WM_SIZE: {
            CRect rect;
            rect.minY = 0.0f;
            rect.minX = 0.0f;
            rect.maxY = HIWORD(lparam);
            rect.maxX = LOWORD(lparam);

            uint32_t resizeType = 0;

            if (wparam == SIZE_MINIMIZED) {
                resizeType = Gx_SizeMinimized;
            } else if (wparam == SIZE_MAXHIDE) {
                resizeType = Gx_SizeMaxHide;
            }

            device->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), resizeType);
            break;
        }

        case WM_ACTIVATE:
            if (LOWORD(wparam) == WA_INACTIVE) {
                if (!device->IDevIsWindowed()) {
                    CRect rect;
                    rect.minY = 0.0f;
                    rect.minX = 0.0f;
                    rect.maxY = 0.0f;
                    rect.maxX = 0.0f;
                    device->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), Gx_SizeMinimized);
                }
            } else if (LOWORD(wparam) == WA_ACTIVE && !device->IDevIsWindowed()) {
                CRect rect;
                rect.minY = 0.0f;
                rect.minX = 0.0f;
                rect.maxY = 0.0f;
                rect.maxX = 0.0f;
                GxCapsWindowSizeInScreenCoords(rect);
                device->DeviceWM(GxWM_Size, reinterpret_cast<uintptr_t>(&rect), 3);
            }

            break;

        case WM_SETFOCUS:
            device->DeviceWM(GxWM_SetFocus, 0, 0);
            return 0;

        case WM_KILLFOCUS:
            device->DeviceWM(GxWM_KillFocus, 0, 0);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            BeginPaint(hwnd, &paint);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_ERASEBKGND:
            return 0;

        case WM_SETCURSOR:
            if (!device) {
                return DefWindowProcA(hwnd, message, wparam, lparam);
            }

            if (device->m_d3dDevice && LOWORD(lparam) == HTCLIENT) {
                SetCursor(nullptr);
                device->m_d3dDevice->ShowCursor(device->m_cursorVisible && device->m_hwCursor);
                return 1;
            }

            break;

        case WM_DISPLAYCHANGE:
            if (!device->IDevIsWindowed()) {
                CRect rect;
                rect.minY = 0.0f;
                rect.minX = 0.0f;
                rect.maxY = HIWORD(lparam);
                rect.maxX = LOWORD(lparam);
                device->DeviceWM(GxWM_DisplayChange, reinterpret_cast<uintptr_t>(&rect), 0);
            }

            break;

        case WM_SYSCOMMAND:
            if (wparam == SC_SCREENSAVE || wparam == SC_MONITORPOWER) {
                return 0;
            }

            break;

        case WM_SIZING: {
            RECT* sizingRect = reinterpret_cast<RECT*>(lparam);

            CiRect rect;
            rect.minX = sizingRect->left;
            rect.minY = sizingRect->top;
            rect.maxX = sizingRect->right;
            rect.maxY = sizingRect->bottom;

            if (rect.maxX - rect.minX <= 0 || rect.maxY - rect.minY <= 0) {
                return 0;
            }

            switch (wparam) {
                case WMSZ_LEFT:
                    CGxDevice::ClampRectLeft(rect);
                    CGxDevice::ClampRectTopByAspect(rect);
                    break;

                case WMSZ_RIGHT:
                    CGxDevice::ClampRectRight(rect);
                    CGxDevice::ClampRectBottomByAspect(rect);
                    break;

                case WMSZ_TOP:
                    CGxDevice::ClampRectTop(rect);
                    CGxDevice::ClampRectLeftByAspect(rect);
                    break;

                case WMSZ_TOPLEFT:
                    CGxDevice::ClampRectTop(rect);
                    CGxDevice::ClampRectLeft(rect);
                    CGxDevice::ClampRectTopByAspect(rect);
                    CGxDevice::ClampRectLeftByAspect(rect);
                    break;

                case WMSZ_TOPRIGHT:
                    CGxDevice::ClampRectTop(rect);
                    CGxDevice::ClampRectRight(rect);
                    CGxDevice::ClampRectTopByAspect(rect);
                    CGxDevice::ClampRectRightByAspect(rect);
                    break;

                case WMSZ_BOTTOM:
                    CGxDevice::ClampRectBottom(rect);
                    CGxDevice::ClampRectRightByAspect(rect);
                    break;

                case WMSZ_BOTTOMLEFT:
                    CGxDevice::ClampRectBottom(rect);
                    CGxDevice::ClampRectLeft(rect);
                    CGxDevice::ClampRectLeftByAspect(rect);
                    CGxDevice::ClampRectBottomByAspect(rect);
                    break;

                case WMSZ_BOTTOMRIGHT:
                    CGxDevice::ClampRectBottom(rect);
                    CGxDevice::ClampRectRight(rect);
                    CGxDevice::ClampRectBottomByAspect(rect);
                    CGxDevice::ClampRectRightByAspect(rect);
                    break;

                default:
                    break;
            }

            sizingRect->left = rect.minX;
            sizingRect->bottom = rect.maxY;
            sizingRect->top = rect.minY;
            sizingRect->right = rect.maxX;

            return 1;
        }

        default:
            break;
    }

    if (device && device->m_windowProc) {
        return device->m_windowProc(hwnd, message, wparam, lparam);
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

bool CGxDeviceD3d::IProbeLock(char* data, bool write) {
    __try {
        volatile char* probe = data;

        if (write) {
            *probe = 0;
        } else {
            *probe = *probe;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return true;
}

void CGxDeviceD3d::IUnregisterWindowClass() {
    if (this->m_hwndClass) {
        UnregisterClassA(reinterpret_cast<const char*>(static_cast<uintptr_t>(this->m_hwndClass)), GetModuleHandleA(nullptr));
        this->m_hwndClass = 0;
    }
}

void CGxDeviceD3d::IHideWindow() {
    if (this->m_hwnd) {
        ShowWindow(this->m_hwnd, SW_HIDE);
    }
}

void CGxDeviceD3d::IDestroyWindow() {
    if (this->m_hwnd) {
        DestroyWindow(this->m_hwnd);
    }

    this->m_hwnd = nullptr;
}

void CGxDeviceD3d::IReadSystemGammaRamp() {
    HDC dc = GetDC(nullptr);

    if (GetDeviceGammaRamp(dc, &this->m_systemGammaRamp) && &this->m_gammaRamp != &this->m_systemGammaRamp) {
        this->m_gammaRamp = this->m_systemGammaRamp;
    }

    ReleaseDC(nullptr, dc);
}

void CGxDeviceD3d::IStereoInit(const CGxFormat& format) {
    if (NvAPI_Initialize() != NVAPI_OK) {
        return;
    }

    this->m_d3dStereoEnabled = 0;
    this->m_d3dNVAPI = 1;

    NvU8 stereoEnabled;

    switch (NvAPI_Stereo_IsEnabled(&stereoEnabled)) {
        case NVAPI_NOT_SUPPORTED:
            CGxDevice::Log("No stereoscopic device present.");
            break;

        case NVAPI_NO_IMPLEMENTATION:
            CGxDevice::Log("No stereoscopic driver present.");
            break;

        case NVAPI_OK:
            this->m_d3dStereoRestore = this->m_d3dStereoEnabled;
            this->m_caps.m_stereoAvailable = 1;

            if (format.m_stereoEnabled) {
                if (!stereoEnabled) {
                    NvAPI_Status status = NvAPI_Stereo_Enable();

                    if (status != NVAPI_OK) {
                        NvAPI_ShortString message;
                        NvAPI_GetErrorMessage(status, message);
                        CGxDevice::Log("Failed to enable stereoscopic mode: %s", message);
                    }
                }
            } else if (stereoEnabled) {
                NvAPI_Stereo_Disable();
            }

            break;

        default:
            break;
    }
}

void CGxDeviceD3d::IStereoCreateHandle() {
    if (!this->m_d3dNVAPI) {
        return;
    }

    NvU8 stereoEnabled = 0;

    if (NvAPI_Stereo_IsEnabled(&stereoEnabled) == NVAPI_OK && stereoEnabled) {
        NvAPI_Stereo_CreateConfigurationProfileRegistryKey(NVAPI_STEREO_DEFAULT_REGISTRY_PROFILE);
        NvAPI_Stereo_CreateHandleFromIUnknown(this->m_d3dDevice, reinterpret_cast<StereoHandle*>(&this->m_d3dStereoHandle));

        if (this->m_d3dStereoHandle) {
            this->m_d3dStereoEnabled = 1;
            this->m_d3dStereoDirty = 1;
        }
    }
}

void CGxDeviceD3d::IStereoUpdate() {
    if (!this->m_d3dStereoHandle || !this->m_d3dStereoDirty) {
        return;
    }

    this->m_d3dStereoDirty = 0;

    NvAPI_Status status = NvAPI_Stereo_SetConvergence(static_cast<StereoHandle>(this->m_d3dStereoHandle), this->m_d3dStereoConvergence);
    CGxDevice::Log("NvAPI_Stereo_SetConvergence: status=%d convergence=%g", status, this->m_d3dStereoConvergence);

    status = NvAPI_Stereo_SetSeparation(static_cast<StereoHandle>(this->m_d3dStereoHandle), this->m_d3dStereoSeparation);
    CGxDevice::Log("NvAPI_Stereo_SetSeparation: status=%d separation=%g", status, this->m_d3dStereoSeparation);
}

void CGxDeviceD3d::IStereoDestroyHandle() {
    if (this->m_d3dStereoHandle) {
        NvAPI_Stereo_DestroyHandle(static_cast<StereoHandle>(this->m_d3dStereoHandle));
        this->m_d3dStereoHandle = nullptr;
    }
}

void CGxDeviceD3d::IStereoRestore() {
    if (!this->m_d3dNVAPI) {
        return;
    }

    NvU8 stereoEnabled;

    if (NvAPI_Stereo_IsEnabled(&stereoEnabled) == NVAPI_OK) {
        if (this->m_d3dStereoRestore) {
            if (!stereoEnabled) {
                NvAPI_Stereo_Enable();
            }
        } else if (stereoEnabled) {
            NvAPI_Stereo_Disable();
        }
    }
}

int32_t CGxDeviceD3d::IHwCursorCaps() {
    return this->m_d3dCaps.CursorCaps & D3DCURSORCAPS_COLOR;
}

void CGxDeviceD3d::IClientToScreen(POINT* points, uint32_t count) {
    MapWindowPoints(this->m_hwnd, nullptr, points, count);
}
