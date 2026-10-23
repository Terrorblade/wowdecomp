#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#include "Engine/Source/Base/Clamp.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Joystick.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C2Vector.h"

// Joysticks on macOS. The client's joystick logic (dead zones, response curve, hats, the stick-driven
// mouse mode and the OS_INPUT_JOYSTICK_* events) is the Windows version's; only the device layer differs.
// Controllers come from the GameController framework and are read into the DirectInput joystick
// state the logic works on, laid out the way DirectInput presents a gamepad:
//
//   axes     0 X, 1 Y (left stick, down positive), 2 Z (left trigger), 3 Rx, 4 Ry (right stick),
//            5 Rz (right trigger); range -32768..32767
//   buttons  0 A, 1 B, 2 X, 3 Y, 4 left shoulder, 5 right shoulder, 6 options, 7 menu,
//            8 left stick, 9 right stick
//   hat 0    directional pad, in hundredths of a degree clockwise from up

struct MacJoystickState {
    int32_t m_axes[8];
    uint8_t rgbButtons[32];
    uint32_t rgdwPOV[4];
};

struct MacJoystickRange {
    int32_t m_min;
    int32_t m_max;
};

struct MacJoystickStick {
    int32_t m_xAxis;
    int32_t m_yAxis;
};

struct MacJoystickHat {
    bool m_enabled;
    int32_t m_buttons[4];
};

struct MacJoystickMouseMode {
    int32_t m_enabled;
    int32_t m_stick;
    float m_speed;
    int32_t m_leftButton;
    int32_t m_rightButton;
    uint32_t m_lastTime;
    C2Vector m_accumulated;
};

struct MacJoystick {
    GCController* m_controller;
    char m_name[256];
    bool m_open;
    uint32_t m_axisMask;
    MacJoystickRange m_ranges[8];
    MacJoystickStick m_sticks[4];
    bool m_stickAxis[8];
    bool m_sliderAxis[8];
    MacJoystickHat m_hats[4];
    bool m_hatButton[32];
    MacJoystickMouseMode m_mouseMode;
    uint32_t m_stateIndex;
    MacJoystickState m_states[2];
    int32_t m_axes[2][8];
};

static const MacJoystickMouseMode s_defaultMouseMode = { 1, 0, 1.5f, 0, 1, 0 };
static std::vector<MacJoystick> s_joysticks;
static bool s_enumerated;

static void ResetState(MacJoystick* joystick, MacJoystickState* state) {
    for (int32_t i = 0; i < 8; i++) {
        if (joystick->m_axisMask & (1 << i)) {
            state->m_axes[i] = joystick->m_ranges[i].m_min + (joystick->m_ranges[i].m_max - joystick->m_ranges[i].m_min) / 2;
        }
    }

    memset(state->rgbButtons, 0, sizeof(state->rgbButtons));

    for (int32_t i = 0; i < 4; i++) {
        state->rgdwPOV[i] = 0xFFFFFFFF;
    }
}

static int32_t AxisValue(float value) {
    return static_cast<int32_t>(lroundf(Clamp(value, -1.0f, 1.0f) * 32767.0f));
}

static int32_t TriggerValue(float value) {
    // A trigger rests at the bottom of the range, like a DirectInput throttle axis.
    return static_cast<int32_t>(lroundf(Clamp(value, 0.0f, 1.0f) * 65535.0f)) - 32768;
}

static uint32_t HatValue(GCControllerDirectionPad* pad) {
    int32_t x = pad.right.pressed ? 1 : (pad.left.pressed ? -1 : 0);
    int32_t y = pad.up.pressed ? 1 : (pad.down.pressed ? -1 : 0);

    if (!x && !y) {
        return 0xFFFFFFFF;
    }

    static const uint32_t s_directions[3][3] = {
        // y = -1, 0, 1 for x = -1
        { 22500, 27000, 31500 },
        // x = 0
        { 18000, 0xFFFFFFFF, 0 },
        // x = 1
        { 13500, 9000, 4500 },
    };

    return s_directions[x + 1][y + 1];
}

static bool ReadState(MacJoystick* joystick, MacJoystickState* state) {
    GCExtendedGamepad* gamepad = joystick->m_controller.extendedGamepad;

    if (!gamepad) {
        return false;
    }

    state->m_axes[0] = AxisValue(gamepad.leftThumbstick.xAxis.value);
    state->m_axes[1] = AxisValue(-gamepad.leftThumbstick.yAxis.value);
    state->m_axes[2] = TriggerValue(gamepad.leftTrigger.value);
    state->m_axes[3] = AxisValue(gamepad.rightThumbstick.xAxis.value);
    state->m_axes[4] = AxisValue(-gamepad.rightThumbstick.yAxis.value);
    state->m_axes[5] = TriggerValue(gamepad.rightTrigger.value);
    state->m_axes[6] = 0;
    state->m_axes[7] = 0;

    memset(state->rgbButtons, 0, sizeof(state->rgbButtons));

    GCControllerButtonInput* buttons[10] = {
        gamepad.buttonA,
        gamepad.buttonB,
        gamepad.buttonX,
        gamepad.buttonY,
        gamepad.leftShoulder,
        gamepad.rightShoulder,
        gamepad.buttonOptions,
        gamepad.buttonMenu,
        gamepad.leftThumbstickButton,
        gamepad.rightThumbstickButton,
    };

    for (int32_t i = 0; i < 10; i++) {
        state->rgbButtons[i] = buttons[i] && buttons[i].pressed ? 0x80 : 0;
    }

    state->rgdwPOV[0] = HatValue(gamepad.dpad);
    state->rgdwPOV[1] = 0xFFFFFFFF;
    state->rgdwPOV[2] = 0xFFFFFFFF;
    state->rgdwPOV[3] = 0xFFFFFFFF;

    return true;
}

void OsJoystickMapStick(uint32_t index, int32_t stick, int32_t xAxis, int32_t yAxis) {
    if (index >= OsJoystickCount()) {
        return;
    }

    MacJoystick* joystick = &s_joysticks[index];

    if (stick >= 0 && stick < 4 && xAxis >= 0 && xAxis < 8 && (joystick->m_axisMask & (1 << xAxis)) && yAxis >= 0 && yAxis < 8 && (joystick->m_axisMask & (1 << yAxis))) {
        joystick->m_sticks[stick].m_xAxis = xAxis;
        joystick->m_sticks[stick].m_yAxis = yAxis;
        joystick->m_stickAxis[xAxis] = true;
        joystick->m_stickAxis[yAxis] = true;
    }
}

uint32_t OsJoystickCount() {
    if (!s_enumerated) {
        s_enumerated = true;

        @autoreleasepool {
            for (GCController* controller in [GCController controllers]) {
                if (!controller.extendedGamepad) {
                    continue;
                }

                MacJoystick joystick = {};
                joystick.m_controller = controller;
                SStrCopy(joystick.m_name, controller.vendorName ? controller.vendorName.UTF8String : "Game Controller", sizeof(joystick.m_name));

                for (int32_t i = 0; i < 4; i++) {
                    joystick.m_sticks[i].m_xAxis = -1;
                    joystick.m_sticks[i].m_yAxis = -1;
                }

                // Six axes, X Y Z Rx Ry Rz, all over the full DirectInput range.
                joystick.m_axisMask = 0x3F;

                for (int32_t i = 0; i < 6; i++) {
                    joystick.m_ranges[i].m_min = -32768;
                    joystick.m_ranges[i].m_max = 32767;
                }

                joystick.m_mouseMode = s_defaultMouseMode;
                s_joysticks.push_back(joystick);

                OsJoystickMapStick(static_cast<uint32_t>(s_joysticks.size() - 1), 0, 0, 1);
            }
        }
    }

    return static_cast<uint32_t>(s_joysticks.size());
}

uint32_t OsJoystickOpen(uint32_t index) {
    if (index >= OsJoystickCount()) {
        return 0xFFFFFFFF;
    }

    MacJoystick* joystick = &s_joysticks[index];

    if (!joystick->m_controller.extendedGamepad) {
        return 0xFFFFFFFF;
    }

    joystick->m_open = true;
    ResetState(joystick, &joystick->m_states[0]);

    return index;
}

void OsJoystickClose(uint32_t index) {
    if (index >= OsJoystickCount()) {
        return;
    }

    s_joysticks[index].m_open = false;
}

char* OsJoystickGetName(uint32_t index, char* buffer, uint32_t size) {
    if (index >= OsJoystickCount() || !s_joysticks[index].m_open) {
        return nullptr;
    }

    SStrCopy(buffer, s_joysticks[index].m_name, size);

    return buffer;
}

void OsJoystickMapSlider(uint32_t index, int32_t axis) {
    if (index >= OsJoystickCount()) {
        return;
    }

    if (s_joysticks[index].m_open && axis >= 0 && axis < 8) {
        s_joysticks[index].m_sliderAxis[axis] = true;
    }
}

void OsJoystickMapHat(uint32_t index, int32_t hat, int32_t up, int32_t right, int32_t down, int32_t left) {
    if (index >= OsJoystickCount()) {
        return;
    }

    MacJoystick* joystick = &s_joysticks[index];

    if (joystick->m_open && hat >= 0 && hat < 4 && up >= 0 && up < 32 && right >= 0 && right < 32 && down >= 0 && down < 32 && left >= 0 && left < 32) {
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

    MacJoystick* joystick = &s_joysticks[index];

    if (!joystick->m_open) {
        return;
    }

    MacJoystickMouseMode* mouseMode = &joystick->m_mouseMode;

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

    MacJoystickState* state = &joystick->m_states[joystick->m_stateIndex];
    int32_t* axes = joystick->m_axes[joystick->m_stateIndex];
    MacJoystickStick* stick = &joystick->m_sticks[mouseMode->m_stick];

    if (axes[stick->m_xAxis] || axes[stick->m_yAxis]) {
        OsQueuePut(OS_INPUT_JOYSTICK_STICK, mouseMode->m_stick, 0, 0, 0);
    }

    int32_t x;
    int32_t y;

    if (state->rgbButtons[mouseMode->m_leftButton]) {
        OsInputGetMousePosition(&x, &y);
        OsQueuePut(OS_INPUT_MOUSE_UP, MOUSE_BUTTON_LEFT, x, y, 0);
    }

    if (state->rgbButtons[mouseMode->m_rightButton]) {
        OsInputGetMousePosition(&x, &y);
        OsQueuePut(OS_INPUT_MOUSE_UP, MOUSE_BUTTON_RIGHT, x, y, 0);
    }
}

void OsJoystickConfigureMouseMode(uint32_t index, int32_t stick, int32_t leftButton, int32_t rightButton, float speed) {
    if (index >= OsJoystickCount()) {
        return;
    }

    MacJoystick* joystick = &s_joysticks[index];

    if (joystick->m_open && stick >= 0 && stick < 4 && joystick->m_sticks[stick].m_xAxis >= 0 && joystick->m_sticks[stick].m_yAxis >= 0) {
        joystick->m_mouseMode.m_speed = speed;
        joystick->m_mouseMode.m_stick = stick;
        joystick->m_mouseMode.m_leftButton = leftButton;
        joystick->m_mouseMode.m_rightButton = rightButton;
    }
}

static void PollJoystick(uint32_t index, int32_t active) {
    MacJoystick* joystick = &s_joysticks[index];

    if (!joystick->m_open) {
        return;
    }

    uint32_t previous = joystick->m_stateIndex;
    uint32_t current = previous == 0;
    joystick->m_stateIndex = current;

    MacJoystickState* previousState = &joystick->m_states[previous];
    MacJoystickState* state = &joystick->m_states[current];
    int32_t* previousAxes = joystick->m_axes[previous];
    int32_t* axes = joystick->m_axes[current];

    // The device is read only while the client is active, like a DirectInput foreground device.
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
        MacJoystickHat* hat = &joystick->m_hats[i];

        if (hat->m_enabled) {
            uint32_t directions = state->rgbButtons[hat->m_buttons[0]] != 0;

            if (state->rgbButtons[hat->m_buttons[1]]) {
                directions |= 2;
            }

            if (state->rgbButtons[hat->m_buttons[2]]) {
                directions |= 4;
            }

            if (state->rgbButtons[hat->m_buttons[3]]) {
                directions |= 8;
            }

            switch (directions) {
                case 1:
                    state->rgdwPOV[i] = 0;
                    break;
                case 2:
                    state->rgdwPOV[i] = 9000;
                    break;
                case 3:
                    state->rgdwPOV[i] = 4500;
                    break;
                case 4:
                    state->rgdwPOV[i] = 18000;
                    break;
                case 6:
                    state->rgdwPOV[i] = 13500;
                    break;
                case 8:
                    state->rgdwPOV[i] = 27000;
                    break;
                case 9:
                    state->rgdwPOV[i] = 31500;
                    break;
                case 12:
                    state->rgdwPOV[i] = 22500;
                    break;
                default:
                    state->rgdwPOV[i] = 0xFFFFFFFF;
                    break;
            }
        }
    }

    MacJoystickMouseMode* mouseMode = &joystick->m_mouseMode;

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
                OsQueuePut(OS_INPUT_JOYSTICK_STICK, i, axes[xAxis], axes[yAxis], 0);
            }
        }
    }

    for (int32_t axis = 0; axis < 8; axis++) {
        if ((joystick->m_axisMask & (1 << axis)) && !joystick->m_stickAxis[axis]) {
            if (!mouseMode->m_enabled || (axis != joystick->m_sticks[mouseMode->m_stick].m_xAxis && axis != joystick->m_sticks[mouseMode->m_stick].m_yAxis)) {
                if (previousAxes[axis] != axes[axis]) {
                    OsQueuePut(OS_INPUT_JOYSTICK_AXIS, axis, axes[axis], 0, 0);
                }
            }
        }
    }

    for (uint32_t button = 0; button < 32; button++) {
        if (joystick->m_hatButton[button]) {
            continue;
        }

        uint8_t down = state->rgbButtons[button];
        uint8_t wasDown = previousState->rgbButtons[button];

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
            OsQueuePut(down ? OS_INPUT_JOYSTICK_BUTTON_DOWN : OS_INPUT_JOYSTICK_BUTTON_UP, button, 0, 0, 0);
        }
    }

    for (uint32_t i = 0; i < 4; i++) {
        uint32_t pov = state->rgdwPOV[i];

        if (previousState->rgdwPOV[i] != pov) {
            int32_t direction = 0;

            if (pov != 0xFFFFFFFF) {
                direction = 8 * pov / 36000 + 1;
            }

            OsQueuePut(OS_INPUT_JOYSTICK_HAT, i, direction, 0, 0);
        }
    }
}

void OsPollJoysticks(int32_t active) {
    for (uint32_t i = 0; i < s_joysticks.size(); i++) {
        PollJoystick(i, active);
    }
}
