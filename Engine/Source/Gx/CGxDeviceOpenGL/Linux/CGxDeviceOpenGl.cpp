#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <cstring>
#include <SDL.h>
#include "Engine/Source/Gx/CGxDeviceOpenGL/CGxDeviceOpenGl.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Os/Linux/OsSdl.h"

// The window, GL context and display mode handling of the OpenGL device: an SDL window with an SDL GL context
// stands in for the Win32 window class, WGL pixel formats and ChangeDisplaySettings. Window messages arrive as SDL
// events through the window proc registered with OsSdlSetWindowProc.

extern int32_t s_deviceChanging;

static const char* const s_deviceKey = "CGxDeviceOpenGl";

#define GL_MAX_SAMPLES_EXT 0x8D57

static bool InitVideo() {
    return SDL_WasInit(SDL_INIT_VIDEO) || SDL_InitSubSystem(SDL_INIT_VIDEO) == 0;
}

static void FormatBits(const CGxFormat& format, int32_t& redBits, int32_t& greenBits, int32_t& blueBits, int32_t& alphaBits, int32_t& depthBits, int32_t& stencilBits) {
    redBits = 5;
    greenBits = 6;
    blueBits = 5;
    alphaBits = 0;
    depthBits = 16;
    stencilBits = 0;

    switch (format.m_colorFormat) {
        case CGxFormat::Fmt_ArgbX888:
            redBits = 8;
            greenBits = 8;
            blueBits = 8;
            alphaBits = 0;
            break;

        case CGxFormat::Fmt_Argb8888:
            redBits = 8;
            greenBits = 8;
            blueBits = 8;
            alphaBits = 8;
            break;

        case CGxFormat::Fmt_Argb2101010:
            redBits = 10;
            greenBits = 10;
            blueBits = 10;
            alphaBits = 2;
            break;

        default:
            break;
    }

    switch (format.m_depthFormat) {
        case CGxFormat::Fmt_Ds24X:
            depthBits = 24;
            stencilBits = 0;
            break;

        case CGxFormat::Fmt_Ds248:
            depthBits = 24;
            stencilBits = 8;
            break;

        case CGxFormat::Fmt_Ds320:
            depthBits = 32;
            stencilBits = 0;
            break;

        default:
            break;
    }
}

// The pixel format is fixed when the window is created, so this is the SetupPixelFormat counterpart and runs
// before SDL_CreateWindow. SDL treats the sizes as minimums.
static void SetGlAttributes(const CGxFormat& format) {
    int32_t redBits;
    int32_t greenBits;
    int32_t blueBits;
    int32_t alphaBits;
    int32_t depthBits;
    int32_t stencilBits;
    FormatBits(format, redBits, greenBits, blueBits, alphaBits, depthBits, stencilBits);

    SDL_GL_ResetAttributes();
    // The renderer uses the fixed function pipeline and ARB programs.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, redBits);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, greenBits);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, blueBits);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, alphaBits);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depthBits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencilBits);

    if (format.m_multisampleCount > 1) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, static_cast<int>(format.m_multisampleCount));
    } else {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }
}

static void SendWindowEvent(SDL_Window* window, uint8_t windowEvent, int32_t data1, int32_t data2) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_WINDOWEVENT;
    event.window.timestamp = SDL_GetTicks();
    event.window.windowID = SDL_GetWindowID(window);
    event.window.event = windowEvent;
    event.window.data1 = data1;
    event.window.data2 = data2;

    CGxDeviceOpenGl::WindowProcGl(window, SDL_WINDOWEVENT, 0, reinterpret_cast<intptr_t>(&event));
}

static SDL_Window* CreateGameWindow(CGxFormat& format, CGxDeviceOpenGl* device) {
    // Windows shows the window from IDevSetFocus unless its style is WS_VISIBLE (maximize == 1).
    uint32_t flags = SDL_WINDOW_OPENGL;

    if (!format.m_window) {
        flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
    } else if (format.m_maximize == 1) {
        flags |= SDL_WINDOW_BORDERLESS;
    } else if (format.m_maximize == 2) {
        flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
    } else {
        flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    }

    float aspectRatio = 0.0f;

    if (format.m_size.x && format.m_size.y) {
        aspectRatio = static_cast<float>(format.m_size.x) / static_cast<float>(format.m_size.y);
    }

    // SDL window sizes are client sizes; the window manager adds the frame.
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
            RECT workRect;
            workRect.left = 0;
            workRect.top = 0;
            workRect.right = format.m_size.x;
            workRect.bottom = format.m_size.y;

            if (CGxDevice::AdjustWindowRectToWorkArea(aspectRatio, &CGxDevice::s_clientAdjust, &workRect)) {
                format.m_pos.x = workRect.left;
                format.m_pos.y = workRect.top;
                format.m_size.x = workRect.right - workRect.left;
                format.m_size.y = workRect.bottom - workRect.top;
            } else {
                format.m_pos.x = 0;
                format.m_pos.y = 0;
            }
        }
    }

    // CW_USEDEFAULT has no SDL equivalent for sizes.
    int32_t width = format.m_size.x ? format.m_size.x : 640;
    int32_t height = format.m_size.y ? format.m_size.y : 480;
    int32_t x = format.m_window ? format.m_pos.x : static_cast<int32_t>(SDL_WINDOWPOS_UNDEFINED_DISPLAY(0));
    int32_t y = format.m_window ? format.m_pos.y : static_cast<int32_t>(SDL_WINDOWPOS_UNDEFINED_DISPLAY(0));

    SetGlAttributes(format);

    SDL_Window* window = SDL_CreateWindow("World of Warcraft", x, y, width, height, flags);

    if (!window) {
        CGxDevice::Log("SDL_CreateWindow failed, multisample = %d: %s", format.m_multisampleCount, SDL_GetError());
        return nullptr;
    }

    SDL_SetWindowData(window, s_deviceKey, device);
    OsSdlSetWindowProc(window, CGxDeviceOpenGl::WindowProcGl);

    // The device has no hardware cursor (m_hwCursor is 0) and draws the game's cursor itself, so the system cursor
    // must not show over the window. SDL hides it only while it is over SDL windows.
    SDL_ShowCursor(SDL_DISABLE);

    // CreateWindowEx delivers WM_SIZE before it returns; SDL reports no initial size.
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
    SendWindowEvent(window, SDL_WINDOWEVENT_SIZE_CHANGED, drawableWidth, drawableHeight);

    return window;
}

void CGxDeviceOpenGl::DeviceDestroyPbuffer() {
}

// There are no pbuffers. Render targets use the FBO path (m_renderTargetMode 1) when the driver has
// EXT_framebuffer_object, with the render target format the pbuffer would have offered; without it the device
// offers no render target formats, as the Windows device does without WGL_ARB_pbuffer.
void CGxDeviceOpenGl::DeviceCreatePbuffer() {
    if (!this->m_glContext || !glExtFramebufferObject) {
        return;
    }

    this->m_renderTargetMode = 1;
    this->m_caps.m_rttFormat[GxTex_Argb8888] = 1;
}

void CGxDeviceOpenGl::DeviceQueryPbuffer() {
}

void CGxDeviceOpenGl::IDevRestoreDesktopMode() {
    if (!this->m_window) {
        return;
    }

    SDL_SetWindowGrab(this->m_window, SDL_FALSE);
    SDL_SetWindowFullscreen(this->m_window, 0);
    SDL_MinimizeWindow(this->m_window);
}

void CGxDeviceOpenGl::IDevSetFocus(int32_t focus, const CGxFormat& format) {
    this->m_windowFocus = focus;

    if (!this->m_window) {
        return;
    }

    if (!format.m_window || format.m_maximize == 2) {
        if (focus) {
            SDL_ShowWindow(this->m_window);

            if (SDL_GetWindowFlags(this->m_window) & SDL_WINDOW_MINIMIZED) {
                SDL_RestoreWindow(this->m_window);
            }

            if (!format.m_window) {
                SDL_DisplayMode mode;

                if (SDL_GetDisplayMode(0, static_cast<int>(format.m_apiSpecificModeID), &mode) == 0) {
                    SDL_SetWindowDisplayMode(this->m_window, &mode);
                }

                SDL_SetWindowFullscreen(this->m_window, SDL_WINDOW_FULLSCREEN);
            } else {
                // maximize == 2 keeps the desktop mode (Windows applies mode 0 of the enumeration here).
                SDL_SetWindowFullscreen(this->m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            }

            // ClipCursor
            SDL_SetWindowGrab(this->m_window, SDL_TRUE);
        } else {
            this->IDevRestoreDesktopMode();
        }
    } else if (focus) {
        SDL_ShowWindow(this->m_window);
    }
}

int32_t CGxDeviceOpenGl::SetFormatMode(CGxFormat& format) {
    if (format.m_window) {
        format.m_apiSpecificModeID = 0;
        return 1;
    }

    if (!InitVideo()) {
        return 0;
    }

    uint32_t bpp = (format.m_colorFormat != CGxFormat::Fmt_Rgb565 ? 16 : 0) + 16;
    int32_t count = SDL_GetNumDisplayModes(0);

    for (int32_t modeNum = 0; modeNum < count; modeNum++) {
        SDL_DisplayMode mode;

        if (SDL_GetDisplayMode(0, modeNum, &mode) != 0) {
            return 0;
        }

        if (SDL_BYTESPERPIXEL(mode.format) * 8 == bpp && static_cast<uint32_t>(mode.refresh_rate) == format.m_refreshRate && mode.w == format.m_size.x && mode.h == format.m_size.y) {
            format.m_apiSpecificModeID = modeNum;
            return 1;
        }
    }

    return 0;
}

void CGxDeviceOpenGl::IDevRemoveGlContext() {
    this->DeviceDestroyPbuffer();

    if (this->m_glContext) {
        this->ICursorDestroy();
        this->DeviceDestroyPoolAPI();
        this->ITexForceRecreation();
        this->IShaderForceRecreation();
        this->IQueryRelease();

        if (this->m_fbo && glDeleteFramebuffersEXT) {
            glDeleteFramebuffersEXT(1, &this->m_fbo);
        }

        this->m_fbo = 0;
        this->m_boundFbo = 0;
        this->m_renderTargetMode = 0;

        UnbindGlExtensions();
        SDL_GL_MakeCurrent(this->m_window, nullptr);
        SDL_GL_DeleteContext(this->m_glContext);
        this->m_glContext = nullptr;
    }

    if (this->m_gammaSet && this->m_window) {
        SDL_SetWindowGammaRamp(this->m_window, this->m_systemGammaRamp.m_red, this->m_systemGammaRamp.m_green, this->m_systemGammaRamp.m_blue);
        this->m_gammaSet = 0;
    }

    this->m_context = 0;
}

// DestroyWindow: the window gets WM_DESTROY before it goes away.
void CGxDeviceOpenGl::IDevDestroyWindow() {
    if (!this->m_window) {
        return;
    }

    this->DeviceWM(GxWM_Destroy, 0, 0);

    OsSdlSetWindowProc(this->m_window, nullptr);
    SDL_SetWindowData(this->m_window, s_deviceKey, nullptr);
    SDL_DestroyWindow(this->m_window);
    this->m_window = nullptr;
}

void CGxDeviceOpenGl::DeviceDestroy() {
    CGxDevice::DeviceDestroy();
    this->DeviceDestroyPbuffer();
    s_deviceChanging = 1;

    if (this->m_window) {
        if (this->m_ownhwnd) {
            this->IDevDestroyWindow();
        }

        if (this->m_window && !this->m_ownhwnd) {
            this->IDevRemoveGlContext();
        }
    }

    s_deviceChanging = 0;
}

void CGxDeviceOpenGl::DeviceSetGamma(float gamma) {
    CGxDevice::DeviceSetGamma(gamma);

    if (!this->m_format.m_window && this->m_window) {
        if (SDL_SetWindowGammaRamp(this->m_window, this->m_gammaRamp.m_red, this->m_gammaRamp.m_green, this->m_gammaRamp.m_blue) == 0) {
            this->m_gammaSet = 1;
        }
    }
}

void CGxDeviceOpenGl::DeviceSetGamma(const CGxGammaRamp& ramp) {
    CGxDevice::DeviceSetGamma(ramp);

    if (!this->m_format.m_window && this->m_window) {
        if (SDL_SetWindowGammaRamp(this->m_window, this->m_gammaRamp.m_red, this->m_gammaRamp.m_green, this->m_gammaRamp.m_blue) == 0) {
            this->m_gammaSet = 1;
        }
    }
}

void* CGxDeviceOpenGl::DeviceWindow() {
    return this->m_window;
}

int32_t CGxDeviceOpenGl::IDevAttachGlContext(const CGxFormat& format) {
    if (this->m_window && (this->m_glContext = SDL_GL_CreateContext(this->m_window)) != nullptr) {
        SDL_GL_MakeCurrent(this->m_window, this->m_glContext);
        this->m_context = 1;

        // Windows binds the extensions once from a dummy context; here they come from the real one.
        BindWglExtensions();
        BindGlExtensions();

        int32_t redBits;
        int32_t greenBits;
        int32_t blueBits;
        int32_t alphaBits;
        int32_t depthBits;
        int32_t stencilBits;
        FormatBits(format, redBits, greenBits, blueBits, alphaBits, depthBits, stencilBits);

        int actualDepth = 0;
        int actualStencil = 0;
        int actualAlpha = 0;
        SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &actualDepth);
        SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &actualStencil);
        SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &actualAlpha);

        if (actualDepth != depthBits || actualStencil != stencilBits || actualAlpha != alphaBits) {
            CGxDevice::Log("IDevAttachGlContext(): got alpha = %d, depth = %d, stencil = %d for requested %d, %d, %d", actualAlpha, actualDepth, actualStencil, alphaBits, depthBits, stencilBits);
        }

        if (wglExtSwapControl) {
            SDL_GL_SetSwapInterval(format.m_vsync != 0);
        }

        this->ISetCaps(format);
        this->DeviceCreatePbuffer();
        this->ICursorCreate(format);
        this->IStateSetContextDefaults();
        this->IRsSync(1);
        this->IShaderConstantsClear();
        this->m_glVendor = glGetString(GL_VENDOR);
        this->m_glRenderer = glGetString(GL_RENDERER);
        this->m_glVersion = glGetString(GL_VERSION);

        return 1;
    }

    if (this->m_window) {
        CGxDevice::Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
    }

    this->IDevRemoveGlContext();
    return 0;
}

// WindowProcGl for SDL events. The SDL window events stand in for the window messages the Windows proc handles:
// SIZE_CHANGED/RESIZED/RESTORED/MAXIMIZED/MINIMIZED for WM_SIZE, FOCUS_GAINED/LOST for WM_ACTIVATE plus
// WM_SETFOCUS/WM_KILLFOCUS and DISPLAY_CHANGED for WM_DISPLAYCHANGE. WM_DESTROY is sent by IDevDestroyWindow. SDL
// has no WM_SIZING, so interactive resizes are not clamped to the aspect ratio. Every event is forwarded to
// m_windowProc (OsWindowProc), which does the input side of window events itself.
intptr_t CGxDeviceOpenGl::WindowProcGl(void* window, uint32_t message, uintptr_t wparam, intptr_t lparam) {
    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);
    CGxDeviceOpenGl* device = sdlWindow ? static_cast<CGxDeviceOpenGl*>(SDL_GetWindowData(sdlWindow, s_deviceKey)) : nullptr;
    const SDL_Event* event = reinterpret_cast<const SDL_Event*>(lparam);

    if (device && message == SDL_WINDOWEVENT && event) {
        switch (event->window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MAXIMIZED: {
                int width = 0;
                int height = 0;
                SDL_GL_GetDrawableSize(sdlWindow, &width, &height);

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

int32_t CGxDeviceOpenGl::DeviceCreate(void* window, const CGxFormat& format) {
    s_deviceChanging = 1;
    this->m_ownhwnd = 0;

    // Windows reads the desktop gamma ramp here; SDL only has per-window ramps, so the system ramp stays linear.
    this->m_gammaRamp = this->m_systemGammaRamp;

    // The caller's window must have been created with SDL_WINDOW_OPENGL.
    this->m_window = static_cast<SDL_Window*>(window);

    if (this->IDevAttachGlContext(format)) {
        s_deviceChanging = 0;

        if (CGxDevice::DeviceCreate(window, format)) {
            return 1;
        }
    }

    this->DeviceDestroy();
    s_deviceChanging = 0;
    return 0;
}

int32_t CGxDeviceOpenGl::DeviceSetFormat(const CGxFormat& format) {
    CGxDevice::Log("CGxDeviceOpenGl::DeviceSetFormat():");
    this->Log(format);

    s_deviceChanging = 1;
    this->IDevRemoveGlContext();
    this->IDevDestroyWindow();

    CGxFormat newFormat = format;

    if (this->SetFormatMode(newFormat) && (this->m_window = CreateGameWindow(newFormat, this)) != nullptr) {
        this->IDevSetFocus(1, newFormat);

        if (this->IDevAttachGlContext(newFormat)) {
            s_deviceChanging = 0;

            if (CGxDevice::DeviceSetFormat(newFormat)) {
                this->DeviceSetGamma(this->m_gammaRamp);
                return 1;
            }
        }
    }

    s_deviceChanging = 0;
    return 0;
}

int32_t CGxDeviceOpenGl::DeviceCreate(GxWindowProc windowProc, const CGxFormat& format) {
    this->m_ownhwnd = 1;

    // See DeviceCreate(void*, ...) for the gamma ramp.
    this->m_gammaRamp = this->m_systemGammaRamp;

    // Initializing SDL video stands in for registering the window class.
    if (InitVideo() && CGxDevice::DeviceCreate(windowProc, format)) {
        return 1;
    }

    this->DeviceDestroy();
    return 0;
}

static void AddFormat(TSGrowableArray<CGxFormat>& formats, CGxFormat::Format colorFormat, CGxFormat::Format depthFormat, uint32_t samples) {
    CGxFormat format;
    memset(&format, 0, sizeof(format));
    format.m_colorFormat = colorFormat;
    format.m_depthFormat = depthFormat;
    format.m_multisampleCount = samples;

    if (!CGxDevice::FormatExists(formats, format)) {
        *formats.New() = format;
    }
}

// GLX/EGL configs cannot be listed through SDL. Like the Windows code this makes a throwaway window and context,
// asks it for the multisample limit and offers the 32-bit color formats with the usual depth/stencil
// combinations, which every desktop driver provides.
int32_t CGxDevice::OpenGlAdapterFormats(TSGrowableArray<CGxFormat>& formats) {
    if (!InitVideo()) {
        return formats.Count() != 0;
    }

    SDL_Window* currentWindow = SDL_GL_GetCurrentWindow();
    SDL_GLContext currentContext = SDL_GL_GetCurrentContext();

    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    SDL_Window* window = SDL_CreateWindow("adfmt", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);

    if (!window) {
        return formats.Count() != 0;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);

    if (context) {
        SDL_GL_MakeCurrent(window, context);

        while (glGetError() != GL_NO_ERROR) {
        }

        GLint maxSamples = 0;
        glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamples);

        if (glGetError() != GL_NO_ERROR) {
            maxSamples = 0;
        }

        static const CGxFormat::Format colorFormats[] = { CGxFormat::Fmt_ArgbX888, CGxFormat::Fmt_Argb8888 };
        static const CGxFormat::Format depthFormats[] = { CGxFormat::Fmt_Ds160, CGxFormat::Fmt_Ds24X, CGxFormat::Fmt_Ds248 };

        for (CGxFormat::Format colorFormat : colorFormats) {
            for (CGxFormat::Format depthFormat : depthFormats) {
                AddFormat(formats, colorFormat, depthFormat, 1);

                for (uint32_t samples = 2; samples <= 16 && static_cast<GLint>(samples) <= maxSamples; samples *= 2) {
                    AddFormat(formats, colorFormat, depthFormat, samples);
                }
            }
        }

        SDL_GL_MakeCurrent(currentWindow, currentContext);
        SDL_GL_DeleteContext(context);
    }

    SDL_DestroyWindow(window);

    return formats.Count() != 0;
}

#endif
