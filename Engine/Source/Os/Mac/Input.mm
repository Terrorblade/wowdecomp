#include <cmath>
#include <cstdint>
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Joystick.h"
#include "Engine/Source/Os/Mac/MacInput.h"
#include "Engine/Source/Os/Mac/MacOs.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C2iVector.h"

// Client input on macOS. The game view's events become the same OS_INPUT events, in the same queue,
// that the Win32 window procedure produces:
//
// - Keys are reported by position for the keys Windows reports by virtual key code (digits, keypad,
//   function and editing keys) and by the character of the active keyboard layout for the rest, as
//   MapVirtualKey does. Command acts as Control, left and right sides kept apart.
// - Coordinates are client pixels: the drawable the client renders into, origin top left.
// - Relative mouse mode detaches the cursor and reports raw motion, scaled by the client's mouse
//   speed setting, where Windows recenters the cursor and changes the system pointer speed.

static int32_t s_appActive = 1;
static C2iVector s_mousePos;
static int32_t s_queueHead;
static int32_t s_queueTail;
static OSEVENT s_queue[16];
static int32_t s_mouseSpeed = 10;
static uint32_t s_osButtonState;
static RECT s_defaultWindowRect;
static int32_t s_windowSize;
static OS_MOUSE_MODE s_osMouseMode;
static int32_t s_WindowResizeLock;
static void (*s_eventPollProc)();
static float s_wheelRemainder;
static bool s_cursorDetached;

// Modifier keys currently down, by KEY (KEY_LSHIFT .. KEY_RALT).
static bool s_modifierDown[6];

// Keys held as the client saw them, so OsInputIsKeyDown matches the events it received.
static bool s_keyDown[KEY_LAST + 1];

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

// Keyboard

static int32_t ConvertFixedKey(uint16_t keyCode, KEY* key) {
    switch (keyCode) {
        case kVK_ANSI_0: *key = KEY_0; return 1;
        case kVK_ANSI_1: *key = KEY_1; return 1;
        case kVK_ANSI_2: *key = KEY_2; return 1;
        case kVK_ANSI_3: *key = KEY_3; return 1;
        case kVK_ANSI_4: *key = KEY_4; return 1;
        case kVK_ANSI_5: *key = KEY_5; return 1;
        case kVK_ANSI_6: *key = KEY_6; return 1;
        case kVK_ANSI_7: *key = KEY_7; return 1;
        case kVK_ANSI_8: *key = KEY_8; return 1;
        case kVK_ANSI_9: *key = KEY_9; return 1;
        case kVK_F1: *key = KEY_F1; return 1;
        case kVK_F2: *key = KEY_F2; return 1;
        case kVK_F3: *key = KEY_F3; return 1;
        case kVK_F4: *key = KEY_F4; return 1;
        case kVK_F5: *key = KEY_F5; return 1;
        case kVK_F6: *key = KEY_F6; return 1;
        case kVK_F7: *key = KEY_F7; return 1;
        case kVK_F8: *key = KEY_F8; return 1;
        case kVK_F9: *key = KEY_F9; return 1;
        case kVK_F10: *key = KEY_F10; return 1;
        case kVK_F11: *key = KEY_F11; return 1;
        case kVK_F12: *key = KEY_F12; return 1;
        case kVK_F13: *key = KEY_PRINTSCREEN; return 1;
        case kVK_F14: *key = KEY_SCROLLLOCK; return 1;
        case kVK_F15: *key = KEY_PAUSE; return 1;
        case kVK_Escape: *key = KEY_ESCAPE; return 1;
        case kVK_Return: *key = KEY_ENTER; return 1;
        case kVK_ANSI_KeypadEnter: *key = KEY_ENTER; return 1;
        case kVK_Delete: *key = KEY_BACKSPACE; return 1;
        case kVK_Tab: *key = KEY_TAB; return 1;
        case kVK_Space: *key = KEY_SPACE; return 1;
        case kVK_LeftArrow: *key = KEY_LEFT; return 1;
        case kVK_UpArrow: *key = KEY_UP; return 1;
        case kVK_RightArrow: *key = KEY_RIGHT; return 1;
        case kVK_DownArrow: *key = KEY_DOWN; return 1;
        case kVK_Help: *key = KEY_INSERT; return 1;
        case kVK_ForwardDelete: *key = KEY_DELETE; return 1;
        case kVK_Home: *key = KEY_HOME; return 1;
        case kVK_End: *key = KEY_END; return 1;
        case kVK_PageUp: *key = KEY_PAGEUP; return 1;
        case kVK_PageDown: *key = KEY_PAGEDOWN; return 1;
        case kVK_CapsLock: *key = KEY_CAPSLOCK; return 1;
        case kVK_ANSI_KeypadClear: *key = KEY_NUMLOCK; return 1;
        case kVK_ANSI_Keypad0: *key = KEY_NUMPAD0; return 1;
        case kVK_ANSI_Keypad1: *key = KEY_NUMPAD1; return 1;
        case kVK_ANSI_Keypad2: *key = KEY_NUMPAD2; return 1;
        case kVK_ANSI_Keypad3: *key = KEY_NUMPAD3; return 1;
        case kVK_ANSI_Keypad4: *key = KEY_NUMPAD4; return 1;
        case kVK_ANSI_Keypad5: *key = KEY_NUMPAD5; return 1;
        case kVK_ANSI_Keypad6: *key = KEY_NUMPAD6; return 1;
        case kVK_ANSI_Keypad7: *key = KEY_NUMPAD7; return 1;
        case kVK_ANSI_Keypad8: *key = KEY_NUMPAD8; return 1;
        case kVK_ANSI_Keypad9: *key = KEY_NUMPAD9; return 1;
        case kVK_ANSI_KeypadPlus: *key = KEY_NUMPAD_PLUS; return 1;
        case kVK_ANSI_KeypadMinus: *key = KEY_NUMPAD_MINUS; return 1;
        case kVK_ANSI_KeypadMultiply: *key = KEY_NUMPAD_MULTIPLY; return 1;
        case kVK_ANSI_KeypadDivide: *key = KEY_NUMPAD_DIVIDE; return 1;
        case kVK_ANSI_KeypadDecimal: *key = KEY_NUMPAD_DECIMAL; return 1;
        case kVK_ANSI_KeypadEquals: *key = KEY_NUMPAD_EQUALS; return 1;
        case kVK_Shift: *key = KEY_LSHIFT; return 1;
        case kVK_RightShift: *key = KEY_RSHIFT; return 1;
        case kVK_Control: *key = KEY_LCONTROL; return 1;
        case kVK_RightControl: *key = KEY_RCONTROL; return 1;
        case kVK_Command: *key = KEY_LCONTROL; return 1;
        case kVK_RightCommand: *key = KEY_RCONTROL; return 1;
        case kVK_Option: *key = KEY_LALT; return 1;
        case kVK_RightOption: *key = KEY_RALT; return 1;
        default: return 0;
    }
}

// Upper case of a layout character, the way the character keys report themselves on Windows.
static uint32_t KeyCharacter(uint32_t c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }

    if (c == 0x131) {
        // Dotless i, the I key of Turkish layouts.
        return 'I';
    }

    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) {
        return c - 0x20;
    }

    return c;
}

// The character a key produces in the current layout without modifiers, as MapVirtualKey reports
// it: a single character up to 0xFF, or none.
static int32_t ConvertLayoutKey(uint16_t keyCode, KEY* key) {
    static TISInputSourceRef s_source;
    static const UCKeyboardLayout* s_layout;

    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();

    if (source != s_source) {
        if (s_source) {
            CFRelease(s_source);
        }

        s_source = source;
        CFDataRef data = source ? static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData)) : nullptr;
        s_layout = data ? reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(data)) : nullptr;
    } else if (source) {
        CFRelease(source);
    }

    if (!s_layout) {
        return 0;
    }

    UInt32 deadKeys = 0;
    UniChar characters[4];
    UniCharCount length = 0;

    if (UCKeyTranslate(s_layout, keyCode, kUCKeyActionDisplay, 0, LMGetKbdType(), kUCKeyTranslateNoDeadKeysMask, &deadKeys, 4, &length, characters) != noErr || length != 1) {
        return 0;
    }

    uint32_t c = KeyCharacter(characters[0]);

    if (c < 0x20 || c > 0xFF) {
        return 0;
    }

    *key = static_cast<KEY>(c);

    return 1;
}

static int32_t ConvertKeyCode(uint16_t keyCode, KEY* key) {
    return ConvertFixedKey(keyCode, key) || ConvertLayoutKey(keyCode, key);
}

static void PutKey(KEY key, bool down, int32_t repeat) {
    if (key <= KEY_LAST) {
        s_keyDown[key] = down;
    }

    OsQueuePut(down ? OS_INPUT_KEY_DOWN : OS_INPUT_KEY_UP, key, repeat, 0, 0);
}

void OsMacInputKeyDown(NSEvent* event) {
    KEY key;

    if (!ConvertKeyCode(event.keyCode, &key)) {
        return;
    }

    // Caps Lock arrives as a modifier change, never as a key press.
    if (key == KEY_CAPSLOCK) {
        return;
    }

    PutKey(key, true, 1);
}

void OsMacInputKeyUp(NSEvent* event) {
    KEY key;

    if (!ConvertKeyCode(event.keyCode, &key) || key == KEY_CAPSLOCK) {
        return;
    }

    PutKey(key, false, 1);
}

// Device-dependent modifier bits (NX_DEVICE*KEYMASK) tell the left key from the right.
static const NSEventModifierFlags kDeviceLeftControl = 0x0001;
static const NSEventModifierFlags kDeviceLeftShift = 0x0002;
static const NSEventModifierFlags kDeviceRightShift = 0x0004;
static const NSEventModifierFlags kDeviceLeftCommand = 0x0008;
static const NSEventModifierFlags kDeviceRightCommand = 0x0010;
static const NSEventModifierFlags kDeviceLeftOption = 0x0020;
static const NSEventModifierFlags kDeviceRightOption = 0x0040;
static const NSEventModifierFlags kDeviceRightControl = 0x2000;

static void UpdateModifier(KEY key, bool down) {
    if (s_modifierDown[key] != down) {
        s_modifierDown[key] = down;
        PutKey(key, down, 1);
    }
}

static void UpdateModifiers(NSEventModifierFlags flags) {
    // Command is Control: a control key counts as down while either key of its side is.
    UpdateModifier(KEY_LSHIFT, (flags & kDeviceLeftShift) != 0);
    UpdateModifier(KEY_RSHIFT, (flags & kDeviceRightShift) != 0);
    UpdateModifier(KEY_LCONTROL, (flags & (kDeviceLeftControl | kDeviceLeftCommand)) != 0);
    UpdateModifier(KEY_RCONTROL, (flags & (kDeviceRightControl | kDeviceRightCommand)) != 0);
    UpdateModifier(KEY_LALT, (flags & kDeviceLeftOption) != 0);
    UpdateModifier(KEY_RALT, (flags & kDeviceRightOption) != 0);
}

void OsMacInputFlagsChanged(NSEvent* event) {
    if (event.keyCode == kVK_CapsLock) {
        // Windows reports every press of Caps Lock as a press and release.
        PutKey(KEY_CAPSLOCK, true, 1);
        PutKey(KEY_CAPSLOCK, false, 1);
        return;
    }

    UpdateModifiers(event.modifierFlags);
}

void OsMacInputCharacter(uint32_t character) {
    if (character < 0x20 || character == 0x7F || (character >= 0xF700 && character <= 0xF8FF)) {
        // Control characters and the private-use codes AppKit gives function keys.
        return;
    }

    OsQueuePut(OS_INPUT_CHAR, static_cast<int32_t>(character), 1, 0, 0);
}

// Mouse

static void ClientPoint(NSView* view, NSEvent* event, int32_t* x, int32_t* y) {
    NSPoint point = [view convertPoint:event.locationInWindow fromView:nil];
    NSRect bounds = view.bounds;

    int32_t width;
    int32_t height;
    OsMacGetClientSize((__bridge void*)view.window, &width, &height);

    double scaleX = bounds.size.width > 0 ? width / bounds.size.width : 1.0;
    double scaleY = bounds.size.height > 0 ? height / bounds.size.height : 1.0;

    *x = static_cast<int32_t>(floor(point.x * scaleX));
    *y = static_cast<int32_t>(floor((bounds.size.height - point.y) * scaleY));
}

static MOUSEBUTTON ConvertButton(NSEvent* event) {
    switch (event.buttonNumber) {
        case 0:
            return MOUSE_BUTTON_LEFT;
        case 1:
            return MOUSE_BUTTON_RIGHT;
        case 2:
            return MOUSE_BUTTON_MIDDLE;
        case 3:
            return MOUSE_BUTTON_XBUTTON1;
        case 4:
            return MOUSE_BUTTON_XBUTTON2;
        default:
            // Buttons 5 and up are XBUTTON3 (1 << 5) onwards, as the Windows version numbers the extra
            // buttons of its mouse driver.
            return event.buttonNumber < 15 ? static_cast<MOUSEBUTTON>(1u << event.buttonNumber) : MOUSE_BUTTON_NONE;
    }
}

static void SaveMouse(int32_t x, int32_t y) {
    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        return;
    }

    s_mousePos.x = x;
    s_mousePos.y = y;
}

void OsMacInputMouseButton(NSView* view, NSEvent* event, bool down) {
    MOUSEBUTTON button = ConvertButton(event);

    if (button == MOUSE_BUTTON_NONE) {
        return;
    }

    if (down) {
        s_osButtonState |= button;
    } else {
        s_osButtonState &= ~button;
    }

    int32_t x;
    int32_t y;

    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        x = s_mousePos.x;
        y = s_mousePos.y;
    } else {
        ClientPoint(view, event, &x, &y);
    }

    OsQueuePut(down ? OS_INPUT_MOUSE_DOWN : OS_INPUT_MOUSE_UP, button, x, y, 0);
}

void OsMacInputMouseMoved(NSView* view, NSEvent* event) {
    if (s_osMouseMode != OS_MOUSE_MODE_NORMAL) {
        double scale = s_mouseSpeed * 0.1;
        int32_t dx = static_cast<int32_t>(lround(event.deltaX * scale));
        int32_t dy = static_cast<int32_t>(lround(event.deltaY * scale));

        if (dx || dy) {
            OsQueuePut(OS_INPUT_MOUSE_MOVE_RELATIVE, 0, dx, dy, 0);
        }

        return;
    }

    int32_t x;
    int32_t y;
    ClientPoint(view, event, &x, &y);
    OsQueuePut(OS_INPUT_MOUSE_MOVE, 0, x, y, 0);
    SaveMouse(x, y);
}

void OsMacInputScrollWheel(NSView* view, NSEvent* event) {
    // Windows reports 120 per wheel notch, positive away from the user. Trackpads report points;
    // 16 of them count as a notch. The physical direction is used, not the natural-scrolling one.
    double delta = event.hasPreciseScrollingDeltas ? event.scrollingDeltaY / 16.0 : event.scrollingDeltaY;

    if (event.isDirectionInvertedFromDevice) {
        delta = -delta;
    }

    s_wheelRemainder += static_cast<float>(delta);
    int32_t notches = static_cast<int32_t>(s_wheelRemainder);

    if (!notches) {
        return;
    }

    s_wheelRemainder -= notches;

    int32_t x;
    int32_t y;

    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        x = s_mousePos.x;
        y = s_mousePos.y;
    } else {
        ClientPoint(view, event, &x, &y);
    }

    OsQueuePut(OS_INPUT_MOUSE_WHEEL, notches * 120, x, y, 0);
}

static void DetachCursor(bool detach) {
    if (detach == s_cursorDetached) {
        return;
    }

    s_cursorDetached = detach;
    CGAssociateMouseAndMouseCursorPosition(detach ? false : true);

    if (detach) {
        [NSCursor hide];
    } else {
        [NSCursor unhide];
        OsMacRefreshGameCursor();
    }
}

// Window

void OsMacInputFocus(bool active) {
    s_appActive = active ? 1 : 0;
    s_osButtonState = 0;

    // Release everything held: key-up events for keys released while another application was
    // active never arrive.
    for (int32_t key = 0; key <= KEY_LAST; key++) {
        if (s_keyDown[key]) {
            PutKey(static_cast<KEY>(key), false, 1);
        }
    }

    for (bool& down : s_modifierDown) {
        down = false;
    }

    if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        DetachCursor(active);
    }

    OsQueuePut(OS_INPUT_FOCUS, s_appActive, 0, 0, 0);
}

void OsMacInputResized() {
    s_windowSize = 1;
}

void OsMacInputClose() {
    OsQueuePut(OS_INPUT_CLOSE, 0, 0, 0, 0);
}

// Client interface

void OsInputSetWindowResizeLock(int32_t resizeLock) {
    s_WindowResizeLock = resizeLock;

    NSWindow* window = (__bridge NSWindow*)OsGuiGetWindow(0);

    if (window && (window.styleMask & NSWindowStyleMaskTitled)) {
        if (resizeLock) {
            window.styleMask &= ~NSWindowStyleMaskResizable;
        } else {
            window.styleMask |= NSWindowStyleMaskResizable;
        }
    }
}

float OsInputGetMouseSpeed() {
    return s_mouseSpeed * 0.1f;
}

int32_t OsInputIsKeyDown(KEY key) {
    if (key > KEY_LAST) {
        return 0;
    }

    return s_keyDown[key] ? 1 : 0;
}

int32_t OsInputIsMouseButtonDown(MOUSEBUTTON button) {
    NSUInteger pressed = [NSEvent pressedMouseButtons];

    switch (button) {
        case MOUSE_BUTTON_LEFT:
            return (pressed & 0x1) != 0;
        case MOUSE_BUTTON_RIGHT:
            return (pressed & 0x2) != 0;
        case MOUSE_BUTTON_MIDDLE:
            return (pressed & 0x4) != 0;
        case MOUSE_BUTTON_XBUTTON1:
            return (pressed & 0x8) != 0;
        case MOUSE_BUTTON_XBUTTON2:
            return (pressed & 0x10) != 0;
        default:
            return 0;
    }
}

void OsInputInitialize() {
    s_mouseSpeed = 10;
}

void OsInputDestroy() {
    DetachCursor(false);
}

void OsInputSetEventPollProc(void (*pollProc)()) {
    s_eventPollProc = pollProc;
}

int32_t OsInputGet(OSINPUT* id, int32_t* param0, int32_t* param1, int32_t* param2, int32_t* param3) {
    *id = static_cast<OSINPUT>(-1);

    NSWindow* window = (__bridge NSWindow*)OsGuiGetWindow(0);

    if (s_windowSize && window && !window.miniaturized) {
        s_windowSize = 0;

        int32_t width;
        int32_t height;
        OsMacGetClientSize((__bridge void*)window, &width, &height);

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

    if (s_queueHead == s_queueTail) {
        OsPollJoysticks(s_appActive);

        if (!OsMacPumpEvents()) {
            *id = OS_INPUT_SHUTDOWN;
            return 1;
        }

        if (s_queueHead == s_queueTail) {
            return 0;
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

        if (s_appActive) {
            DetachCursor(true);
        }
    } else if (s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        s_osMouseMode = OS_MOUSE_MODE_NORMAL;
        DetachCursor(false);

        // Put the cursor back where relative mode took it, as the Windows version restores it.
        OsInputSetMousePosition(s_mousePos.x, s_mousePos.y);
    }
}

void OsInputSetMouseSpeed(float speed) {
    float clamped = speed < 0.1f ? 0.1f : (speed >= 2.0f ? 2.0f : speed);
    s_mouseSpeed = static_cast<int32_t>(clamped * 10.0f);
}

void OsInputGetMousePosition(int32_t* x, int32_t* y) {
    NSWindow* window = (__bridge NSWindow*)OsGuiGetWindow(0);
    int32_t px = s_mousePos.x;
    int32_t py = s_mousePos.y;

    if (window && s_osMouseMode != OS_MOUSE_MODE_RELATIVE) {
        NSView* view = window.contentView;
        NSPoint point = [view convertPoint:[window mouseLocationOutsideOfEventStream] fromView:nil];
        NSRect bounds = view.bounds;

        int32_t width;
        int32_t height;
        OsMacGetClientSize((__bridge void*)window, &width, &height);

        px = static_cast<int32_t>(floor(point.x * (bounds.size.width > 0 ? width / bounds.size.width : 1.0)));
        py = static_cast<int32_t>(floor((bounds.size.height - point.y) * (bounds.size.height > 0 ? height / bounds.size.height : 1.0)));
        SaveMouse(px, py);
    }

    if (x) {
        *x = px;
    }

    if (y) {
        *y = py;
    }
}

void OsInputSetMousePosition(int32_t x, int32_t y) {
    SaveMouse(x, y);

    NSWindow* window = (__bridge NSWindow*)OsGuiGetWindow(0);

    if (!window || s_osMouseMode == OS_MOUSE_MODE_RELATIVE) {
        return;
    }

    NSView* view = window.contentView;
    NSRect bounds = view.bounds;

    int32_t width;
    int32_t height;
    OsMacGetClientSize((__bridge void*)window, &width, &height);

    NSPoint local = NSMakePoint(x * (width > 0 ? bounds.size.width / width : 1.0), bounds.size.height - y * (height > 0 ? bounds.size.height / height : 1.0));
    NSPoint screen = [window convertPointToScreen:[view convertPoint:local toView:nil]];

    // Quartz display coordinates have their origin at the top left of the main display.
    CGFloat mainHeight = NSMaxY([NSScreen screens].firstObject.frame);
    CGWarpMouseCursorPosition(CGPointMake(screen.x, mainHeight - screen.y));
    CGAssociateMouseAndMouseCursorPosition(true);
}

int32_t OsGetDefaultWindowRect(tagRECT* rect) {
    if (!rect) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (!s_defaultWindowRect.right || !s_defaultWindowRect.bottom) {
        NSWindow* window = (__bridge NSWindow*)OsGuiGetWindow(0);

        if (!window) {
            return 0;
        }

        int32_t width;
        int32_t height;
        OsMacGetClientSize((__bridge void*)window, &width, &height);
        s_defaultWindowRect.left = 0;
        s_defaultWindowRect.top = 0;
        s_defaultWindowRect.right = width;
        s_defaultWindowRect.bottom = height;
    }

    *rect = s_defaultWindowRect;

    return 1;
}

intptr_t OsWindowProc(void* window, uint32_t message, uintptr_t wparam, intptr_t lparam) {
    // Window messages do not exist on macOS; the game view delivers its events above.
    return 0;
}

void OsInputEnableSecureMode(int32_t enable) {
}
