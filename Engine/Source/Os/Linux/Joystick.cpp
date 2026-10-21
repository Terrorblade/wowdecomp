#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <cmath>
#include <cstdint>
#include <cstring>
#include <SDL.h>
#include "Engine/Source/Base/Clamp.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Joystick.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C2Vector.h"

// The Windows version reads DirectInput devices into DIJOYSTATE records. Here SDL_Joystick fills an equivalent
// record (8 axes in DirectInput order X, Y, Z, Rx, Ry, Rz, slider 0, slider 1 = SDL axes 0-7; 32 buttons; 4 POV hats
// in hundredths of degrees, 0xFFFFFFFF when centered), and the mapping, dead zone, mouse mode and event logic is the
// same. SDL joysticks are polled (state, not a buffer), so a press and release between two polls is not seen.

struct LinuxJoystickRange {
    int32_t m_min;
    int32_t m_max;
};

struct LinuxJoystickStick {
    int32_t m_xAxis;
    int32_t m_yAxis;
};

struct LinuxJoystickHat {
    bool m_enabled;
    int32_t m_buttons[4];
};

struct LinuxJoystickMouseMode {
    int32_t m_enabled;
    int32_t m_stick;
    float m_speed;
    int32_t m_leftButton;
    int32_t m_rightButton;
    uint32_t m_lastTime;
    C2Vector m_accumulated;
};

struct LinuxJoystickState {
    int32_t m_axes[8];
    uint32_t m_pov[4];
    uint8_t m_buttons[32];
};

struct LinuxJoystick {
    int32_t m_deviceIndex;
    char m_name[260];
    SDL_Joystick* m_device;
    bool m_relative;
    int32_t m_lastAxes[8];
    uint32_t m_axisMask;
    LinuxJoystickRange m_ranges[8];
    uint32_t m_buttonCount;
    uint32_t m_povCount;
    LinuxJoystickStick m_sticks[4];
    bool m_stickAxis[8];
    bool m_sliderAxis[8];
    LinuxJoystickHat m_hats[4];
    bool m_hatButton[32];
    LinuxJoystickMouseMode m_mouseMode;
    uint32_t m_stateIndex;
    LinuxJoystickState m_states[2];
    int32_t m_axes[2][8];
};

static const char* s_relativeDevices[] = {
    "SpaceNavigator",
    "SpaceNavigator for Notebooks",
    "SpaceTraveler USB",
    "SpaceExplorer",
    "SpacePilot",
};

static LinuxJoystickMouseMode s_defaultMouseMode = { 1, 0, 1.5f, 0, 1, 0 };

static int32_t s_sdlInitialized;

static TSGrowableArray<LinuxJoystick> s_joysticks;

static void ResetState(LinuxJoystick* joystick, LinuxJoystickState* state) {
    for (int32_t i = 0; i < 8; i++) {
        if (joystick->m_axisMask & (1 << i)) {
            state->m_axes[i] = joystick->m_ranges[i].m_min + (joystick->m_ranges[i].m_max - joystick->m_ranges[i].m_min) / 2;
        }
    }

    memset(state->m_buttons, 0, sizeof(state->m_buttons));

    for (int32_t i = 0; i < 4; i++) {
        state->m_pov[i] = 0xFFFFFFFF;
    }
}

static uint32_t ConvertHat(uint8_t hat) {
    switch (hat) {
        case SDL_HAT_UP:
            return 0;

        case SDL_HAT_RIGHTUP:
            return 4500;

        case SDL_HAT_RIGHT:
            return 9000;

        case SDL_HAT_RIGHTDOWN:
            return 13500;

        case SDL_HAT_DOWN:
            return 18000;

        case SDL_HAT_LEFTDOWN:
            return 22500;

        case SDL_HAT_LEFT:
            return 27000;

        case SDL_HAT_LEFTUP:
            return 31500;

        default:
            return 0xFFFFFFFF;
    }
}

// EnumObjects: record which axes exist and their ranges, and count buttons and hats.
static void EnumObjects(LinuxJoystick* joystick) {
    int32_t axisCount = SDL_JoystickNumAxes(joystick->m_device);

    for (int32_t axis = 0; axis < axisCount && axis < 8; axis++) {
        joystick->m_axisMask |= 1 << axis;

        if (joystick->m_relative) {
            joystick->m_ranges[axis].m_min = -300;
            joystick->m_ranges[axis].m_max = 300;
        } else {
            joystick->m_ranges[axis].m_min = -32768;
            joystick->m_ranges[axis].m_max = 32767;
        }
    }

    int32_t buttonCount = SDL_JoystickNumButtons(joystick->m_device);
    joystick->m_buttonCount = buttonCount > 0 ? buttonCount : 0;

    int32_t hatCount = SDL_JoystickNumHats(joystick->m_device);
    joystick->m_povCount = hatCount > 0 ? hatCount : 0;
}

void OsJoystickMapStick(uint32_t index, int32_t stick, int32_t xAxis, int32_t yAxis) {
    if (index >= OsJoystickCount()) {
        return;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (joystick->m_device && stick >= 0 && stick < 4 && xAxis >= 0 && xAxis < 8 && (joystick->m_axisMask & (1 << xAxis)) && yAxis >= 0 && yAxis < 8 && (joystick->m_axisMask & (1 << yAxis))) {
        joystick->m_sticks[stick].m_xAxis = xAxis;
        joystick->m_sticks[stick].m_yAxis = yAxis;
        joystick->m_stickAxis[xAxis] = true;
        joystick->m_stickAxis[yAxis] = true;
    }
}

static void EnumDevice(int32_t deviceIndex) {
    LinuxJoystick* joystick = s_joysticks.New();
    memset(joystick, 0, sizeof(LinuxJoystick));
    joystick->m_deviceIndex = deviceIndex;

    const char* name = SDL_JoystickNameForIndex(deviceIndex);
    SStrCopy(joystick->m_name, name ? name : "", sizeof(joystick->m_name));

    for (int32_t i = 0; i < 4; i++) {
        joystick->m_sticks[i].m_xAxis = -1;
        joystick->m_sticks[i].m_yAxis = -1;
    }

    for (uint32_t i = 0; i < sizeof(s_relativeDevices) / sizeof(s_relativeDevices[0]); i++) {
        if (!SStrCmpI(joystick->m_name, s_relativeDevices[i], 0x7FFFFFFF)) {
            joystick->m_relative = true;
            break;
        }
    }

    joystick->m_mouseMode = s_defaultMouseMode;

    OsJoystickMapStick(s_joysticks.Count() - 1, 0, 0, 1);
}

uint32_t OsJoystickCount() {
    if (!s_joysticks.Count()) {
        if (!s_sdlInitialized) {
            if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
                return 0;
            }

            s_sdlInitialized = 1;
        }

        int32_t count = SDL_NumJoysticks();

        for (int32_t i = 0; i < count; i++) {
            EnumDevice(i);
        }
    }

    return s_joysticks.Count();
}

uint32_t OsJoystickOpen(uint32_t index) {
    if (index >= OsJoystickCount()) {
        return 0xFFFFFFFF;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (joystick->m_device) {
        SDL_JoystickClose(joystick->m_device);
        joystick->m_device = nullptr;
    }

    joystick->m_device = SDL_JoystickOpen(joystick->m_deviceIndex);
    if (!joystick->m_device) {
        return 0xFFFFFFFF;
    }

    EnumObjects(joystick);
    ResetState(joystick, &joystick->m_states[0]);

    return index;
}

void OsJoystickClose(uint32_t index) {
    if (index >= OsJoystickCount()) {
        return;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (joystick->m_device) {
        SDL_JoystickClose(joystick->m_device);
        joystick->m_device = nullptr;
    }
}

char* OsJoystickGetName(uint32_t index, char* buffer, uint32_t size) {
    if (index >= OsJoystickCount()) {
        return nullptr;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (!joystick->m_device) {
        return nullptr;
    }

    SStrCopy(buffer, joystick->m_name, size);

    return buffer;
}

void OsJoystickMapSlider(uint32_t index, int32_t axis) {
    if (index >= OsJoystickCount()) {
        return;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (joystick->m_device && axis >= 0 && axis < 8) {
        joystick->m_sliderAxis[axis] = true;
    }
}

void OsJoystickMapHat(uint32_t index, int32_t hat, int32_t up, int32_t right, int32_t down, int32_t left) {
    if (index >= OsJoystickCount()) {
        return;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (joystick->m_device && hat >= 0 && hat < 4 && up >= 0 && up < 32 && right >= 0 && right < 32 && down >= 0 && down < 32 && left >= 0 && left < 32) {
        joystick->m_hats[hat].m_buttons[0] = up;
        joystick->m_hats[hat].m_buttons[1] = right;
        joystick->m_hats[hat].m_buttons[3] = left;
        joystick->m_hats[hat].m_enabled = true;
        joystick->m_hats[hat].m_buttons[2] = down;
        joystick->m_hatButton[up] = true;
        joystick->m_hatButton[right] = true;
        joystick->m_hatButton[down] = true;
        joystick->m_hatButton[left] = true;
    }
}

void OsJoystickSetMouseMode(uint32_t index, int32_t enable) {
    if (index >= OsJoystickCount()) {
        return;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (!joystick->m_device) {
        return;
    }

    LinuxJoystickMouseMode* mouseMode = &joystick->m_mouseMode;

    if (enable == mouseMode->m_enabled) {
        return;
    }

    mouseMode->m_enabled = enable;
    mouseMode->m_lastTime = 0;
    mouseMode->m_accumulated.x = 0.0f;
    mouseMode->m_accumulated.y = 0.0f;

    if (!enable) {
        return;
    }

    LinuxJoystickState* state = &joystick->m_states[joystick->m_stateIndex];
    int32_t* axes = joystick->m_axes[joystick->m_stateIndex];
    LinuxJoystickStick* stick = &joystick->m_sticks[mouseMode->m_stick];

    if (axes[stick->m_xAxis] || axes[stick->m_yAxis]) {
        OsQueuePut(OS_INPUT_14, mouseMode->m_stick, 0, 0, 0);
    }

    int32_t x;
    int32_t y;

    if (state->m_buttons[mouseMode->m_leftButton]) {
        OsInputGetMousePosition(&x, &y);
        OsQueuePut(OS_INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, x, y, 0);
    }

    if (state->m_buttons[mouseMode->m_rightButton]) {
        OsInputGetMousePosition(&x, &y);
        OsQueuePut(OS_INPUT_MOUSE_UP, MOUSE_BUTTON_RIGHT, x, y, 0);
    }
}

void OsJoystickConfigureMouseMode(uint32_t index, int32_t stick, int32_t leftButton, int32_t rightButton, float speed) {
    if (index >= OsJoystickCount()) {
        return;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (joystick->m_device && stick >= 0 && stick < 4 && joystick->m_sticks[stick].m_xAxis >= 0 && joystick->m_sticks[stick].m_yAxis >= 0) {
        joystick->m_mouseMode.m_speed = speed;
        joystick->m_mouseMode.m_stick = stick;
        joystick->m_mouseMode.m_leftButton = leftButton;
        joystick->m_mouseMode.m_rightButton = rightButton;
    }
}

// GetDeviceState: read the current SDL state into the DirectInput-shaped record.
static int32_t ReadState(LinuxJoystick* joystick, LinuxJoystickState* state) {
    if (!SDL_JoystickGetAttached(joystick->m_device)) {
        return 0;
    }

    SDL_JoystickUpdate();

    for (int32_t axis = 0; axis < 8; axis++) {
        if (!(joystick->m_axisMask & (1 << axis))) {
            continue;
        }

        int32_t value = SDL_JoystickGetAxis(joystick->m_device, axis);

        if (joystick->m_relative) {
            int32_t delta = value - joystick->m_lastAxes[axis];
            joystick->m_lastAxes[axis] = value;
            state->m_axes[axis] = Clamp(delta, joystick->m_ranges[axis].m_min, joystick->m_ranges[axis].m_max);
        } else {
            state->m_axes[axis] = value;
        }
    }

    for (uint32_t button = 0; button < 32; button++) {
        state->m_buttons[button] = button < joystick->m_buttonCount && SDL_JoystickGetButton(joystick->m_device, button) ? 0x80 : 0;
    }

    for (uint32_t hat = 0; hat < 4; hat++) {
        state->m_pov[hat] = hat < joystick->m_povCount ? ConvertHat(SDL_JoystickGetHat(joystick->m_device, hat)) : 0xFFFFFFFF;
    }

    return 1;
}

static void PollJoystick(uint32_t index, int32_t active) {
    if (index >= OsJoystickCount()) {
        return;
    }

    LinuxJoystick* joystick = &s_joysticks[index];

    if (!joystick->m_device) {
        return;
    }

    uint32_t previous = joystick->m_stateIndex;
    uint32_t current = previous == 0;
    joystick->m_stateIndex = current;

    LinuxJoystickState* previousState = &joystick->m_states[previous];
    LinuxJoystickState* state = &joystick->m_states[current];
    int32_t* previousAxes = joystick->m_axes[previous];
    int32_t* axes = joystick->m_axes[current];

    // The DirectInput device is foreground-exclusive, so an inactive client reads a centered, released state.
    if (!active || !ReadState(joystick, state)) {
        ResetState(joystick, state);
    }

    const int32_t* values = state->m_axes;

    for (int32_t i = 0; i < 4; i++) {
        int32_t xAxis = joystick->m_sticks[i].m_xAxis;
        int32_t yAxis = joystick->m_sticks[i].m_yAxis;

        if (xAxis >= 0 && yAxis >= 0) {
            float x = ((static_cast<float>(values[xAxis]) - joystick->m_ranges[xAxis].m_min) / (joystick->m_ranges[xAxis].m_max - joystick->m_ranges[xAxis].m_min) - 0.5f) * 2.0f;
            float y = ((static_cast<float>(values[yAxis]) - joystick->m_ranges[yAxis].m_min) / (joystick->m_ranges[yAxis].m_max - joystick->m_ranges[yAxis].m_min) - 0.5f) * 2.0f;
            float length = sqrtf(y * y + x * x);

            if (length <= 0.15f) {
                axes[xAxis] = 0;
                axes[yAxis] = 0;
            } else {
                float scale = (length - 0.15f) / (length - length * 0.15f);
                axes[xAxis] = static_cast<int32_t>(x * scale * 32767.0f);
                axes[yAxis] = static_cast<int32_t>(scale * y * 32767.0f);
            }
        }
    }

    for (int32_t axis = 0; axis < 8; axis++) {
        if ((joystick->m_axisMask & (1 << axis)) && !joystick->m_stickAxis[axis]) {
            float value = (static_cast<float>(values[axis]) - joystick->m_ranges[axis].m_min) / (joystick->m_ranges[axis].m_max - joystick->m_ranges[axis].m_min);

            if (!joystick->m_sliderAxis[axis]) {
                value = (value - 0.5f) * 2.0f;
            }

            if (value > 0.15f) {
                axes[axis] = static_cast<int32_t>((value - 0.15f) * 1.1764705f * 32767.0f);
            } else if (value >= -0.15f) {
                axes[axis] = 0;
            } else {
                axes[axis] = static_cast<int32_t>((value + 0.15f) * 1.1764705f * 32767.0f);
            }
        }
    }

    for (int32_t i = 0; i < 4; i++) {
        LinuxJoystickHat* hat = &joystick->m_hats[i];

        if (hat->m_enabled) {
            uint32_t directions = state->m_buttons[hat->m_buttons[0]] != 0;

            if (state->m_buttons[hat->m_buttons[1]]) {
                directions |= 2;
            }

            if (state->m_buttons[hat->m_buttons[2]]) {
                directions |= 4;
            }

            if (state->m_buttons[hat->m_buttons[3]]) {
                directions |= 8;
            }

            switch (directions) {
                case 1:
                    state->m_pov[i] = 0;
                    break;

                case 2:
                    state->m_pov[i] = 9000;
                    break;

                case 3:
                    state->m_pov[i] = 4500;
                    break;

                case 4:
                    state->m_pov[i] = 18000;
                    break;

                case 6:
                    state->m_pov[i] = 13500;
                    break;

                case 8:
                    state->m_pov[i] = 27000;
                    break;

                case 9:
                    state->m_pov[i] = 31500;
                    break;

                case 12:
                    state->m_pov[i] = 22500;
                    break;

                default:
                    state->m_pov[i] = 0xFFFFFFFF;
                    break;
            }
        }
    }

    LinuxJoystickMouseMode* mouseMode = &joystick->m_mouseMode;

    if (mouseMode->m_enabled) {
        uint32_t now = OsGetAsyncTimeMs();

        if (mouseMode->m_lastTime) {
            float x = axes[joystick->m_sticks[0].m_xAxis] * (1.0f / 32767.0f);
            float y = (1.0f / 32767.0f) * axes[joystick->m_sticks[0].m_yAxis];
            float y2 = y * y;
            float x2 = x * x;

            if (y2 + x2 > 0.0f) {
                RECT rect;
                OsGetDefaultWindowRect(&rect);

                float distance = static_cast<int32_t>(now - mouseMode->m_lastTime) * 0.001f * (rect.right - rect.left) * mouseMode->m_speed;

                float dx = x * (x2 * x) * distance;
                mouseMode->m_accumulated.x = x < 0.0f ? mouseMode->m_accumulated.x - dx : dx + mouseMode->m_accumulated.x;

                float dy = distance * (y * (y2 * y));
                mouseMode->m_accumulated.y = y < 0.0f ? mouseMode->m_accumulated.y - dy : dy + mouseMode->m_accumulated.y;

                int32_t moveX = static_cast<int32_t>(mouseMode->m_accumulated.x);
                int32_t moveY = static_cast<int32_t>(mouseMode->m_accumulated.y);

                if (moveX || moveY) {
                    mouseMode->m_accumulated.x -= moveX;
                    mouseMode->m_accumulated.y -= moveY;

                    int32_t mouseX;
                    int32_t mouseY;
                    OsInputGetMousePosition(&mouseX, &mouseY);
                    OsInputSetMousePosition(moveX + mouseX, moveY + mouseY);
                }
            }
        }

        mouseMode->m_lastTime = now;
    }

    for (int32_t i = 0; i < 4; i++) {
        int32_t xAxis = joystick->m_sticks[i].m_xAxis;
        int32_t yAxis = joystick->m_sticks[i].m_yAxis;

        if (xAxis >= 0 && yAxis >= 0 && (!mouseMode->m_enabled || i != mouseMode->m_stick)) {
            if (previousAxes[xAxis] != axes[xAxis] || previousAxes[yAxis] != axes[yAxis]) {
                OsQueuePut(OS_INPUT_14, i, axes[xAxis], axes[yAxis], 0);
            }
        }
    }

    for (int32_t axis = 0; axis < 8; axis++) {
        if ((joystick->m_axisMask & (1 << axis)) && !joystick->m_stickAxis[axis]) {
            if (!mouseMode->m_enabled || (axis != joystick->m_sticks[mouseMode->m_stick].m_xAxis && axis != joystick->m_sticks[mouseMode->m_stick].m_yAxis)) {
                if (previousAxes[axis] != axes[axis]) {
                    OsQueuePut(OS_INPUT_15, axis, axes[axis], 0, 0);
                }
            }
        }
    }

    for (uint32_t button = 0; button < 32; button++) {
        if (joystick->m_hatButton[button]) {
            continue;
        }

        uint8_t down = state->m_buttons[button];
        uint8_t wasDown = previousState->m_buttons[button];

        if (wasDown == down) {
            continue;
        }

        if (mouseMode->m_enabled && (static_cast<int32_t>(button) == mouseMode->m_leftButton || static_cast<int32_t>(button) == mouseMode->m_rightButton)) {
            int32_t x;
            int32_t y;
            OsInputGetMousePosition(&x, &y);

            MOUSEBUTTON mouseButton = static_cast<int32_t>(button) != mouseMode->m_leftButton ? MOUSE_BUTTON_RIGHT : MOUSE_BUTTON_LEFT;
            OsQueuePut(down ? OS_INPUT_MOUSE_DOWN : OS_INPUT_MOUSE_UP, mouseButton, x, y, 0);
        } else {
            OsQueuePut(down ? OS_INPUT_16 : OS_INPUT_17, button, 0, 0, 0);
        }
    }

    for (uint32_t i = 0; i < 4; i++) {
        uint32_t pov = state->m_pov[i];

        if (previousState->m_pov[i] != pov) {
            int32_t direction = 0;

            if (pov != 0xFFFFFFFF) {
                direction = 8 * pov / 36000 + 1;
            }

            OsQueuePut(OS_INPUT_18, i, direction, 0, 0);
        }
    }
}

void OsPollJoysticks(int32_t active) {
    for (uint32_t i = 0; i < s_joysticks.Count(); i++) {
        PollJoystick(i, active);
    }
}

#endif
