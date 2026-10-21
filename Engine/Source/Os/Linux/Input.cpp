#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cstdint>
#include <SDL.h>
#include "Engine/Source/Base/ConvertUTF.h"
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Joystick.h"
#include "Engine/Source/Os/Linux/OsIMESdl.h"
#include "Engine/Source/Os/Linux/OsSdl.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C2iVector.h"
#include "Controller/Controller.h"
#include "HoT/Runtime/Display.h"

// SDL events stand in for window messages (see OsSdl.h): OsWindowProc gets message = SDL event type and
// lparam = const SDL_Event*, and turns them into the same OsQueuePut events the WM_* handlers produce. Mouse
// positions are in window coordinates (client area, origin top-left), the same space as OsGetDefaultWindowRect.
// The window manager draws and drives the frame, so the Windows non-client drag/resize/caption handling has no
// counterpart, and SDL_TEXTINPUT delivers Unicode directly, so the ANSI code page tables are not needed.

static int32_t s_appActive = 1;

static C2iVector s_mousePos;

static SDL_Window* s_mouseWnd;

static int32_t s_queueHead;

static int32_t s_queueTail;

static OSEVENT s_queue[16];

static int32_t s_mouseSpeed;

static int32_t s_savedMouseSpeed;

static uint32_t s_osButtonState;

static RECT s_defaultWindowRect;

static intptr_t s_windowSize;

static OS_MOUSE_MODE s_osMouseMode;

static int32_t s_WindowResizeLock;

static int32_t s_resizableLocked;

static void (*s_eventPollProc)();

static int32_t s_sdlInitialized;

static float s_relativeRemainder[2];

// Characters from one SDL_TEXTINPUT that did not fit in the 16 entry queue (which drops its oldest entry when full).
static uint32_t s_pendingChars[64];

static uint32_t s_pendingCharCount;

static uint32_t s_pendingCharHead;

void OsQueuePut(OSINPUT id, int32_t param0, int32_t param1, int32_t param2, int32_t param3) {
    int32_t next = s_queueTail == 15 ? 0 : s_queueTail + 1;

    if (next == s_queueHead) {
        s_queueHead = s_queueHead == 15 ? 0 : s_queueHead + 1;
    }

    OSEVENT& event = s_queue[s_queueTail];
    event.id = id;
    event.param[0] = param0;
    event.param[1] = param1;
    event.param[2] = param2;
    event.param[3] = OsGetAsyncTimeMs();

    s_queueTail = next;
}

int32_t OsQueueGet(OSINPUT* id, int32_t* param0, int32_t* param1, int32_t* param2, int32_t* param3) {
    if (s_queueHead == s_queueTail) {
        return 0;
    }

    OSEVENT& event = s_queue[s_queueHead];
    *id = event.id;
    *param0 = event.param[0];
    *param1 = event.param[1];
    *param2 = event.param[2];
    *param3 = event.param[3];

    s_queueHead = s_queueHead == 15 ? 0 : s_queueHead + 1;

    return 1;
}

static int32_t QueueCount() {
    return (s_queueTail - s_queueHead + 16) % 16;
}

static void FlushPendingChars() {
    while (s_pendingCharHead < s_pendingCharCount && QueueCount() < 15) {
        OsQueuePut(OS_INPUT_CHAR, static_cast<int32_t>(s_pendingChars[s_pendingCharHead++]), 1, 0, 0);
    }

    if (s_pendingCharHead == s_pendingCharCount) {
        s_pendingCharHead = 0;
        s_pendingCharCount = 0;
    }
}

static SDL_Window* GetGxWindow() {
    return static_cast<SDL_Window*>(OsGuiGetWindow(0));
}

// The Windows client maps VK_* codes: F1-F12, the top row digits, the named keys, the numpad (VK_NUMPAD* only with
// Num Lock on, otherwise the navigation keys), and for everything else the unshifted character of the layout
// (MapVirtualKey), upper case for letters, if it is in 0x01-0xFF. Scancodes stand in for the positional VK codes.
static int32_t ConvertKeyCode(const SDL_Keysym& keysym, KEY* key) {
    SDL_Scancode scancode = keysym.scancode;

    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
        *key = static_cast<KEY>(KEY_F1 + (scancode - SDL_SCANCODE_F1));
        return 1;
    }

    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        *key = static_cast<KEY>(KEY_1 + (scancode - SDL_SCANCODE_1));
        return 1;
    }

    if (scancode == SDL_SCANCODE_0) {
        *key = KEY_0;
        return 1;
    }

    if ((scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_0) || scancode == SDL_SCANCODE_KP_PERIOD) {
        if (keysym.mod & KMOD_NUM) {
            switch (scancode) {
                case SDL_SCANCODE_KP_0:
                    *key = KEY_NUMPAD0;
                    return 1;

                case SDL_SCANCODE_KP_PERIOD:
                    *key = KEY_NUMPAD_DECIMAL;
                    return 1;

                default:
                    *key = static_cast<KEY>(KEY_NUMPAD1 + (scancode - SDL_SCANCODE_KP_1));
                    return 1;
            }
        }

        switch (scancode) {
            case SDL_SCANCODE_KP_0:
                *key = KEY_INSERT;
                return 1;

            case SDL_SCANCODE_KP_1:
                *key = KEY_END;
                return 1;

            case SDL_SCANCODE_KP_2:
                *key = KEY_DOWN;
                return 1;

            case SDL_SCANCODE_KP_3:
                *key = KEY_PAGEDOWN;
                return 1;

            case SDL_SCANCODE_KP_4:
                *key = KEY_LEFT;
                return 1;

            case SDL_SCANCODE_KP_6:
                *key = KEY_RIGHT;
                return 1;

            case SDL_SCANCODE_KP_7:
                *key = KEY_HOME;
                return 1;

            case SDL_SCANCODE_KP_8:
                *key = KEY_UP;
                return 1;

            case SDL_SCANCODE_KP_9:
                *key = KEY_PAGEUP;
                return 1;

            case SDL_SCANCODE_KP_PERIOD:
                *key = KEY_DELETE;
                return 1;

            default:
                // KP_5 without Num Lock is VK_CLEAR, which has no KEY.
                return 0;
        }
    }

    switch (scancode) {
        case SDL_SCANCODE_LSHIFT:
            *key = KEY_LSHIFT;
            return 1;

        case SDL_SCANCODE_RSHIFT:
            *key = KEY_RSHIFT;
            return 1;

        case SDL_SCANCODE_LCTRL:
            *key = KEY_LCONTROL;
            return 1;

        case SDL_SCANCODE_RCTRL:
            *key = KEY_RCONTROL;
            return 1;

        case SDL_SCANCODE_LALT:
            *key = KEY_LALT;
            return 1;

        case SDL_SCANCODE_RALT:
            *key = KEY_RALT;
            return 1;

        case SDL_SCANCODE_ESCAPE:
            *key = KEY_ESCAPE;
            return 1;

        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_RETURN2:
        case SDL_SCANCODE_KP_ENTER:
            *key = KEY_ENTER;
            return 1;

        case SDL_SCANCODE_BACKSPACE:
            *key = KEY_BACKSPACE;
            return 1;

        case SDL_SCANCODE_TAB:
            *key = KEY_TAB;
            return 1;

        case SDL_SCANCODE_LEFT:
            *key = KEY_LEFT;
            return 1;

        case SDL_SCANCODE_UP:
            *key = KEY_UP;
            return 1;

        case SDL_SCANCODE_RIGHT:
            *key = KEY_RIGHT;
            return 1;

        case SDL_SCANCODE_DOWN:
            *key = KEY_DOWN;
            return 1;

        case SDL_SCANCODE_INSERT:
            *key = KEY_INSERT;
            return 1;

        case SDL_SCANCODE_DELETE:
            *key = KEY_DELETE;
            return 1;

        case SDL_SCANCODE_HOME:
            *key = KEY_HOME;
            return 1;

        case SDL_SCANCODE_END:
            *key = KEY_END;
            return 1;

        case SDL_SCANCODE_PAGEUP:
            *key = KEY_PAGEUP;
            return 1;

        case SDL_SCANCODE_PAGEDOWN:
            *key = KEY_PAGEDOWN;
            return 1;

        case SDL_SCANCODE_CAPSLOCK:
            *key = KEY_CAPSLOCK;
            return 1;

        case SDL_SCANCODE_NUMLOCKCLEAR:
            *key = KEY_NUMLOCK;
            return 1;

        case SDL_SCANCODE_SCROLLLOCK:
            *key = KEY_SCROLLLOCK;
            return 1;

        case SDL_SCANCODE_PAUSE:
            *key = KEY_PAUSE;
            return 1;

        case SDL_SCANCODE_PRINTSCREEN:
            *key = KEY_PRINTSCREEN;
            return 1;

        case SDL_SCANCODE_SPACE:
            *key = KEY_SPACE;
            return 1;

        case SDL_SCANCODE_KP_PLUS:
            *key = KEY_NUMPAD_PLUS;
            return 1;

        case SDL_SCANCODE_KP_MINUS:
            *key = KEY_NUMPAD_MINUS;
            return 1;

        case SDL_SCANCODE_KP_MULTIPLY:
            *key = KEY_NUMPAD_MULTIPLY;
            return 1;

        case SDL_SCANCODE_KP_DIVIDE:
            *key = KEY_NUMPAD_DIVIDE;
            return 1;

        default:
            break;
    }

    SDL_Keycode keycode = keysym.sym;
    uint32_t ch = 0;

    if (!(keycode & SDLK_SCANCODE_MASK)) {
        ch = static_cast<uint32_t>(keycode);

        if (ch >= 'a' && ch <= 'z') {
            ch -= 'a' - 'A';
        }
    }

    if ((ch == 0 || ch > 0xFF) && scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        // Non-Latin layouts: the letter keys still have VK_A-VK_Z on Windows.
        ch = 'A' + (scancode - SDL_SCANCODE_A);
    }

    *key = static_cast<KEY>(ch);

    if (ch == 0 || ch > 0xFF) {
        return 0;
    }

    return 1;
}

// The reverse, for OsInputIsKeyDown: the scancode(s) whose key reports as KEY.
static int32_t ConvertKeyToScancode(KEY key, SDL_Scancode* scancodes) {
    scancodes[1] = SDL_SCANCODE_UNKNOWN;

    if (key >= KEY_F1 && key <= KEY_F12) {
        scancodes[0] = static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + (key - KEY_F1));
        return 1;
    }

    if (key >= KEY_1 && key <= KEY_9) {
        scancodes[0] = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (key - KEY_1));
        return 1;
    }

    if (key >= KEY_NUMPAD1 && key <= KEY_NUMPAD9) {
        scancodes[0] = static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1 + (key - KEY_NUMPAD1));
        return 1;
    }

    SDL_Scancode scancode;

    switch (key) {
        case KEY_0:
            scancode = SDL_SCANCODE_0;
            break;

        case KEY_LSHIFT:
            scancode = SDL_SCANCODE_LSHIFT;
            break;

        case KEY_RSHIFT:
            scancode = SDL_SCANCODE_RSHIFT;
            break;

        case KEY_LCONTROL:
            scancode = SDL_SCANCODE_LCTRL;
            break;

        case KEY_RCONTROL:
            scancode = SDL_SCANCODE_RCTRL;
            break;

        case KEY_LALT:
            scancode = SDL_SCANCODE_LALT;
            break;

        case KEY_RALT:
            scancode = SDL_SCANCODE_RALT;
            break;

        case KEY_SPACE:
            scancode = SDL_SCANCODE_SPACE;
            break;

        case KEY_NUMPAD0:
            scancode = SDL_SCANCODE_KP_0;
            break;

        case KEY_NUMPAD_PLUS:
            scancode = SDL_SCANCODE_KP_PLUS;
            break;

        case KEY_NUMPAD_MINUS:
            scancode = SDL_SCANCODE_KP_MINUS;
            break;

        case KEY_NUMPAD_MULTIPLY:
            scancode = SDL_SCANCODE_KP_MULTIPLY;
            break;

        case KEY_NUMPAD_DIVIDE:
            scancode = SDL_SCANCODE_KP_DIVIDE;
            break;

        case KEY_NUMPAD_DECIMAL:
            scancode = SDL_SCANCODE_KP_PERIOD;
            break;

        case KEY_ESCAPE:
            scancode = SDL_SCANCODE_ESCAPE;
            break;

        case KEY_ENTER:
            // VK_RETURN is both Enter keys.
            scancodes[1] = SDL_SCANCODE_KP_ENTER;
            scancode = SDL_SCANCODE_RETURN;
            break;

        case KEY_BACKSPACE:
            scancode = SDL_SCANCODE_BACKSPACE;
            break;

        case KEY_TAB:
            scancode = SDL_SCANCODE_TAB;
            break;

        case KEY_LEFT:
            scancode = SDL_SCANCODE_LEFT;
            break;

        case KEY_UP:
            scancode = SDL_SCANCODE_UP;
            break;

        case KEY_RIGHT:
            scancode = SDL_SCANCODE_RIGHT;
            break;

        case KEY_DOWN:
            scancode = SDL_SCANCODE_DOWN;
            break;

        case KEY_INSERT:
            scancode = SDL_SCANCODE_INSERT;
            break;

        case KEY_DELETE:
            scancode = SDL_SCANCODE_DELETE;
            break;

        case KEY_HOME:
            scancode = SDL_SCANCODE_HOME;
            break;

        case KEY_END:
            scancode = SDL_SCANCODE_END;
            break;

        case KEY_PAGEUP:
            scancode = SDL_SCANCODE_PAGEUP;
            break;

        case KEY_PAGEDOWN:
            scancode = SDL_SCANCODE_PAGEDOWN;
            break;

        case KEY_CAPSLOCK:
            scancode = SDL_SCANCODE_CAPSLOCK;
            break;

        case KEY_NUMLOCK:
            scancode = SDL_SCANCODE_NUMLOCKCLEAR;
            break;

        case KEY_SCROLLLOCK:
            scancode = SDL_SCANCODE_SCROLLLOCK;
            break;

        case KEY_PAUSE:
            scancode = SDL_SCANCODE_PAUSE;
            break;

        case KEY_PRINTSCREEN:
            scancode = SDL_SCANCODE_PRINTSCREEN;
            break;

        default: {
            // VkKeyScan: the key that types this character on the current layout.
            uint32_t ch = static_cast<uint32_t>(key);
            if (ch == 0 || ch > 0xFF) {
                return 0;
            }

            if (ch >= 'A' && ch <= 'Z') {
                ch += 'a' - 'A';
            }

            scancode = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(ch));
            if (scancode == SDL_SCANCODE_UNKNOWN && ch >= 'a' && ch <= 'z') {
                scancode = static_cast<SDL_Scancode>(SDL_SCANCODE_A + (ch - 'a'));
            }

            break;
        }
    }

    scancodes[0] = scancode;

    return scancode != SDL_SCANCODE_UNKNOWN;
}

static int32_t ConvertButton(uint8_t sdlButton, MOUSEBUTTON* button) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT:
            *button = MOUSE_BUTTON_LEFT;
            return 1;

        case SDL_BUTTON_MIDDLE:
            *button = MOUSE_BUTTON_MIDDLE;
            return 1;

        case SDL_BUTTON_RIGHT:
            *button = MOUSE_BUTTON_RIGHT;
            return 1;

        case SDL_BUTTON_X1:
            *button = MOUSE_BUTTON_XBUTTON1;
            return 1;

        case SDL_BUTTON_X2:
            *button = MOUSE_BUTTON_XBUTTON2;
            return 1;

        default:
            // Further buttons are the extra-button bits 5 and up (MOUSE_BUTTON_XBUTTON3...), as the Windows
            // WM_USER + 3 extra-button path reports them.
            if (sdlButton > SDL_BUTTON_X2 && sdlButton <= 15) {
                *button = static_cast<MOUSEBUTTON>(1u << (sdlButton - 1));
                return 1;
            }

            break;
    }

    *button = MOUSE_BUTTON_NONE;
    return 0;
}

static void RestoreMouse() {
    SDL_Window* window = s_mouseWnd ? s_mouseWnd : GetGxWindow();
    if (window) {
        SDL_WarpMouseInWindow(window, s_mousePos.x, s_mousePos.y);
    }
}

static void SaveMouse(SDL_Window* window, int32_t x, int32_t y) {
    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        return;
    }

    if (window) {
        s_mouseWnd = window;
    }

    s_mousePos.x = x;
    s_mousePos.y = y;
}

// The window manager resizes the window, so the lock makes it non-resizable instead of refusing the client's own
// frame drag. Only undo what the lock did. OsInputGet reapplies it, as the lock can be set before the window exists.
static void ApplyWindowResizeLock(SDL_Window* window) {
    if (!window) {
        return;
    }

    if (s_WindowResizeLock) {
        if (!s_resizableLocked && (SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE)) {
            SDL_SetWindowResizable(window, SDL_FALSE);
            s_resizableLocked = 1;
        }
    } else if (s_resizableLocked) {
        SDL_SetWindowResizable(window, SDL_TRUE);
        s_resizableLocked = 0;
    }
}

void OsInputSetWindowResizeLock(int32_t resizeLock) {
    s_WindowResizeLock = resizeLock;
    ApplyWindowResizeLock(GetGxWindow());
}

float OsInputGetMouseSpeed() {
    return s_mouseSpeed * 0.1f;
}

int32_t OsInputIsKeyDown(KEY key) {
    SDL_Scancode scancodes[2];
    if (!ConvertKeyToScancode(key, scancodes)) {
        return 0;
    }

    int32_t count;
    const uint8_t* state = SDL_GetKeyboardState(&count);

    for (int32_t i = 0; i < 2; i++) {
        if (scancodes[i] != SDL_SCANCODE_UNKNOWN && scancodes[i] < count && state[scancodes[i]]) {
            return 1;
        }
    }

    return 0;
}

int32_t OsInputIsMouseButtonDown(MOUSEBUTTON button) {
    uint32_t state = SDL_GetMouseState(nullptr, nullptr);

    switch (button) {
        case MOUSE_BUTTON_LEFT:
            return (state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

        case MOUSE_BUTTON_MIDDLE:
            return (state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;

        case MOUSE_BUTTON_RIGHT:
            return (state & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;

        case MOUSE_BUTTON_XBUTTON1:
            return (state & SDL_BUTTON(SDL_BUTTON_X1)) != 0;

        case MOUSE_BUTTON_XBUTTON2:
            return (state & SDL_BUTTON(SDL_BUTTON_X2)) != 0;

        default:
            return 0;
    }
}

void OsInputInitialize() {
    // Closing the window is OS_INPUT_CLOSE (the client asks to confirm); SDL_QUIT is left for termination requests
    // (SIGINT/SIGTERM), which are the WM_QUIT / OS_INPUT_SHUTDOWN case.
    SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
    SDL_SetHint(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, "1");

    if (!s_sdlInitialized && SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        s_sdlInitialized = 1;
    }

    // OS_INPUT_CHAR comes from SDL_TEXTINPUT, so text input is always on (see OsIME.cpp).
    SDL_StartTextInput();

    // There is no system mouse speed to read or change (SPI_GETMOUSESPEED); 10 is the Windows default. The speed
    // scales relative mouse motion instead.
    s_savedMouseSpeed = 10;
    s_mouseSpeed = s_savedMouseSpeed;
}

void OsInputDestroy() {
    // Windows restores the Num Lock state and the system mouse speed here; neither is changed on Linux.
    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        s_osMouseMode = OS_MOUSE_MODE_NORMAL;
    }

    if (s_sdlInitialized) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
        s_sdlInitialized = 0;
    }
}

static void ProcessMouseEvent(int32_t x, int32_t y, MOUSEBUTTON button, OSINPUT id) {
    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        x = s_mousePos.x;
        y = s_mousePos.y;
    }

    OsQueuePut(id, button, x, y, 0);
}

static int32_t HandleMouseDown(const SDL_MouseButtonEvent& event) {
    MOUSEBUTTON button;
    if (!ConvertButton(event.button, &button)) {
        return 0;
    }

    // SDL captures the mouse while a button is down (SDL_HINT_MOUSE_AUTO_CAPTURE), like SetCapture.
    s_osButtonState |= button;

    ProcessMouseEvent(event.x, event.y, button, OS_INPUT_MOUSE_DOWN);

    return 1;
}

static int32_t HandleMouseUp(const SDL_MouseButtonEvent& event) {
    MOUSEBUTTON button;
    if (!ConvertButton(event.button, &button)) {
        return 0;
    }

    s_osButtonState &= ~button;

    ProcessMouseEvent(event.x, event.y, button, OS_INPUT_MOUSE_UP);

    return 1;
}

void OsInputSetEventPollProc(void (*pollProc)()) {
    // Windows runs this from a timer while DefWindowProc is in a modal loop (system menu, caption drag). SDL never
    // blocks in a modal loop on Linux, so it is only stored.
    s_eventPollProc = pollProc;
}

int32_t OsInputGet(OSINPUT* id, int32_t* param0, int32_t* param1, int32_t* param2, int32_t* param3) {
    *id = static_cast<OSINPUT>(-1);

    SDL_Window* window = GetGxWindow();

    if (s_WindowResizeLock != s_resizableLocked) {
        ApplyWindowResizeLock(window);
    }

    if (s_windowSize && window && !(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)) {
        s_windowSize = 0;

        int32_t width = 0;
        int32_t height = 0;
        SDL_GetWindowSize(window, &width, &height);

        if (width != s_defaultWindowRect.right || height != s_defaultWindowRect.bottom) {
            s_defaultWindowRect.left = 0;
            s_defaultWindowRect.top = 0;
            s_defaultWindowRect.right = width;
            s_defaultWindowRect.bottom = height;

            *id = OS_INPUT_SIZE;
            *param0 = width;
            *param1 = height;
            *param2 = 0;
            *param3 = 0;

            return 1;
        }
    }

    if (s_pendingCharCount && s_queueHead == s_queueTail) {
        FlushPendingChars();
    }

    if (s_queueHead == s_queueTail) {
        OsPollJoysticks(s_appActive);
        // Controller: gamepads post their buttons into the queue here.
        Controller::Poll(s_appActive);

        SDL_Event event;

        while (s_queueHead == s_queueTail) {
            if (!SDL_PollEvent(&event)) {
                return 0;
            }

            if (event.type == SDL_QUIT) {
                *id = OS_INPUT_SHUTDOWN;
                return 1;
            }

            if (!OsGuiProcessMessage(&event)) {
                OsSdlDispatchEvent(&event);
            }

            if (event.type == SDL_TEXTEDITING_EXT) {
                SDL_free(event.editExt.text);
            } else if (event.type == SDL_DROPFILE || event.type == SDL_DROPTEXT) {
                SDL_free(event.drop.file);
            }
        }
    }

    OsQueueGet(id, param0, param1, param2, param3);

    return 1;
}

void OsInputSetMouseMode(OS_MOUSE_MODE mode) {
    if (mode >= OS_MOUSE_MODES) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return;
    }

    if (mode == s_osMouseMode) {
        return;
    }

    if (mode == OS_MOUSE_MODE_RELATIVE) {
        s_osMouseMode = mode;
        s_relativeRemainder[0] = 0.0f;
        s_relativeRemainder[1] = 0.0f;
        SDL_SetRelativeMouseMode(SDL_TRUE);
    } else if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        s_osMouseMode = OS_MOUSE_MODE_NORMAL;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        RestoreMouse();
    }
}

void OsInputSetMouseSpeed(float speed) {
    float clamped = speed < 0.1f ? 0.1f : (speed >= 2.0f ? 2.0f : speed);
    s_mouseSpeed = static_cast<int32_t>(clamped * 10.0f);
}

void OsInputGetMousePosition(int32_t* x, int32_t* y) {
    int32_t mouseX;
    int32_t mouseY;

    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        // The cursor is hidden and goes back to the saved position when relative mode ends.
        mouseX = s_mousePos.x;
        mouseY = s_mousePos.y;
    } else {
        SDL_GetMouseState(&mouseX, &mouseY);
        SaveMouse(GetGxWindow(), mouseX, mouseY);
    }

    if (x) {
        *x = mouseX;
    }

    if (y) {
        *y = mouseY;
    }
}

void OsInputSetMousePosition(int32_t x, int32_t y) {
    SDL_Window* window = GetGxWindow();

    SaveMouse(window, x, y);

    // In relative mode a warp would come back as relative motion; the position is restored when it ends.
    if (window && s_osMouseMode != OS_MOUSE_MODE_RELATIVE) {
        SDL_WarpMouseInWindow(window, x, y);
    }
}

int32_t OsGetDefaultWindowRect(tagRECT* rect) {
    if (!rect) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (!s_defaultWindowRect.right || !s_defaultWindowRect.bottom) {
        SDL_Window* window = GetGxWindow();
        if (!window) {
            return 0;
        }

        int32_t width = 0;
        int32_t height = 0;
        SDL_GetWindowSize(window, &width, &height);

        s_defaultWindowRect.left = 0;
        s_defaultWindowRect.top = 0;
        s_defaultWindowRect.right = width;
        s_defaultWindowRect.bottom = height;
    }

    *rect = s_defaultWindowRect;

    return 1;
}

static void PostText(const char* text) {
    // OS_INPUT_CHAR carries one Unicode code point (the Windows code converts WM_CHAR through the code page tables
    // to Unicode, and the consumers encode it with sputu8), with the WM_CHAR repeat count (1) as param1.
    for (const char* c = text; *c;) {
        int32_t chars = 0;
        uint32_t ch = sgetu8(reinterpret_cast<const uint8_t*>(c), &chars);

        if (chars <= 0) {
            break;
        }

        c += chars;

        if (ch < 0x20 || ch >= 0x110000) {
            continue;
        }

        if (s_pendingCharCount < sizeof(s_pendingChars) / sizeof(s_pendingChars[0])) {
            s_pendingChars[s_pendingCharCount++] = ch;
        }
    }

    FlushPendingChars();
}

static void HandleWindowEvent(SDL_Window* window, const SDL_WindowEvent& event) {
    switch (event.event) {
        case SDL_WINDOWEVENT_DISPLAY_CHANGED:
            // HoT: drop HoT's cached display modes when the display configuration changes.
            HoT::Display::InvalidateMonitorModes();
            [[fallthrough]];

        case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            // WM_SIZE / WM_DISPLAYCHANGE: OsInputGet reads the new size once the window is not minimized.
            s_windowSize = 1;
            break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST: {
            // WM_ACTIVATE. Losing focus with a button down also loses the capture (WM_CAPTURECHANGED).
            int32_t minimized = window && (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED);
            int32_t hadButtons = s_osButtonState != 0;

            s_appActive = event.event == SDL_WINDOWEVENT_FOCUS_GAINED && !minimized ? 1 : 0;
            s_osButtonState = 0;
            OsQueuePut(OS_INPUT_FOCUS, s_appActive != 0, 0, 0, 0);

            if (!s_appActive && hadButtons) {
                int32_t x;
                int32_t y;
                SDL_GetMouseState(&x, &y);
                OsQueuePut(OS_INPUT_CAPTURE_CHANGED, 0, x, y, 0);
            }

            break;
        }

        case SDL_WINDOWEVENT_CLOSE:
            OsQueuePut(OS_INPUT_CLOSE, 0, 0, 0, 0);
            break;

        default:
            break;
    }
}

static void HandleKey(const SDL_KeyboardEvent& event) {
    int32_t down = event.type == SDL_KEYDOWN;

    if (!down && (event.keysym.scancode == SDL_SCANCODE_LSHIFT || event.keysym.scancode == SDL_SCANCODE_RSHIFT)) {
        // As on Windows, any Shift release releases both Shift keys.
        OsQueuePut(OS_INPUT_KEY_UP, KEY_LSHIFT, 0, 0, 0);
        OsQueuePut(OS_INPUT_KEY_UP, KEY_RSHIFT, 0, 0, 0);
        return;
    }

    KEY key;
    if (!ConvertKeyCode(event.keysym, &key)) {
        return;
    }

    // param1 is the WM_KEYDOWN repeat count, 1 per message (auto-repeat sends further key downs).
    OsQueuePut(down ? OS_INPUT_KEY_DOWN : OS_INPUT_KEY_UP, key, 1, 0, 0);

    // Alt+F4 falls through to DefWindowProc on Windows, which turns it into SC_CLOSE.
    if (down && key == KEY_F4 && OsGuiIsModifierKeyDown(2)) {
        OsQueuePut(OS_INPUT_CLOSE, 0, 0, 0, 0);
    }
}

static void HandleMouseMotion(SDL_Window* window, const SDL_MouseMotionEvent& event) {
    if (s_osMouseMode != OS_MOUSE_MODE_NORMAL) {
        // Relative motion, scaled by the mouse speed the client sets (the system pointer speed on Windows).
        float scale = s_mouseSpeed * 0.1f;
        float dx = event.xrel * scale + s_relativeRemainder[0];
        float dy = event.yrel * scale + s_relativeRemainder[1];
        int32_t x = static_cast<int32_t>(dx);
        int32_t y = static_cast<int32_t>(dy);
        s_relativeRemainder[0] = dx - x;
        s_relativeRemainder[1] = dy - y;

        if (x || y) {
            OsQueuePut(OS_INPUT_MOUSE_MOVE_RELATIVE, 0, x, y, 0);
        }

        return;
    }

    OsQueuePut(OS_INPUT_MOUSE_MOVE, 0, event.x, event.y, 0);
    SaveMouse(window, event.x, event.y);
}

static void HandleMouseWheel(const SDL_MouseWheelEvent& event) {
    int32_t delta = event.y;

    if (event.direction == SDL_MOUSEWHEEL_FLIPPED) {
        delta = -delta;
    }

    if (!delta) {
        return;
    }

    int32_t x;
    int32_t y;

    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        x = s_mousePos.x;
        y = s_mousePos.y;
    } else {
        x = event.mouseX;
        y = event.mouseY;
    }

    // WM_MOUSEWHEEL reports WHEEL_DELTA (120) per notch, positive away from the user.
    OsQueuePut(OS_INPUT_MOUSE_WHEEL, delta * 120, x, y, 0);
}

intptr_t OsWindowProc(void* window, uint32_t message, uintptr_t wparam, intptr_t lparam) {
    const SDL_Event* event = reinterpret_cast<const SDL_Event*>(lparam);
    if (!event) {
        return 0;
    }

    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);

    switch (message) {
        case SDL_WINDOWEVENT:
            HandleWindowEvent(sdlWindow, event->window);
            return 0;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            HandleKey(event->key);
            return 0;

        case SDL_TEXTEDITING:
        case SDL_TEXTEDITING_EXT:
            OsIMEProcessSdlEvent(event);
            return 0;

        case SDL_TEXTINPUT:
            if (!OsIMEProcessSdlEvent(event)) {
                PostText(event->text.text);
            }

            return 0;

        case SDL_KEYMAPCHANGED:
            // WM_INPUTLANGCHANGE.
            OsQueuePut(OS_INPUT_IME, 0, 0, 0, 0);
            return 0;

        case SDL_MOUSEMOTION:
            HandleMouseMotion(sdlWindow, event->motion);
            return 0;

        case SDL_MOUSEBUTTONDOWN:
            HandleMouseDown(event->button);
            return 0;

        case SDL_MOUSEBUTTONUP:
            HandleMouseUp(event->button);
            return 0;

        case SDL_MOUSEWHEEL:
            HandleMouseWheel(event->wheel);
            return 0;

        default:
            return 0;
    }
}

void OsInputEnableSecureMode(int32_t enable) {
}

#endif
