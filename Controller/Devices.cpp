#include "Controller/Devices.h"
#include <algorithm>
#include <cmath>
#include "Controller/CVars.h"
#include "Controller/Lua.h"

#if defined(CONTROLLER_HAS_SDL)
#include <SDL3/SDL.h>
#endif

namespace Controller::Devices {

static std::vector<std::unique_ptr<Device>> s_devices;
static MappedState s_combined;
static int32_t s_activeID = -1;
static bool s_initialized = false;
static bool s_stickAxisButtons = false;

static float s_rumble[4] = {};
static uint64_t s_rumbleRefresh = 0;
static bool s_rumbleActive = false;

static bool s_ledSet = false;
static float s_led[3] = {};

const std::vector<std::unique_ptr<Device>>& All() {
    return s_devices;
}

Device* Find(int32_t id) {
    for (auto& device : s_devices) {
        if (device->m_id == id) {
            return device.get();
        }
    }

    return nullptr;
}

Device* Active() {
    Device* device = Find(s_activeID);
    return device ? device : (s_devices.empty() ? nullptr : s_devices.front().get());
}

const MappedState& Combined() {
    return s_combined;
}

bool Available() {
    return s_initialized;
}

#if defined(CONTROLLER_HAS_SDL)

static SDL_Gamepad* PadOf(const Device* device) {
    return static_cast<SDL_Gamepad*>(device->m_gamepad);
}

static SDL_Joystick* JoystickOf(const Device* device) {
    return static_cast<SDL_Joystick*>(device->m_joystick);
}

static const char* DetectLabelStyle(SDL_Gamepad* gamepad) {
    if (!gamepad) {
        return "Generic";
    }

    switch (SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH)) {
        case SDL_GAMEPAD_BUTTON_LABEL_A:
            return "Letters";

        case SDL_GAMEPAD_BUTTON_LABEL_B:
            return "Reverse";

        case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
            return "Shapes";

        default:
            return "Generic";
    }
}

static int32_t ReadPowerLevel(const Device* device) {
    SDL_Joystick* joystick = JoystickOf(device);

    if (SDL_GetJoystickConnectionState(joystick) == SDL_JOYSTICK_CONNECTION_WIRED) {
        return POWER_WIRED;
    }

    int percent = -1;

    switch (SDL_GetJoystickPowerInfo(joystick, &percent)) {
        case SDL_POWERSTATE_NO_BATTERY:
        case SDL_POWERSTATE_CHARGING:
        case SDL_POWERSTATE_CHARGED:
            return POWER_WIRED;

        case SDL_POWERSTATE_ON_BATTERY:
            if (percent < 0) {
                return POWER_UNKNOWN;
            }

            return percent <= 5 ? POWER_CRITICAL : percent <= 20 ? POWER_LOW : percent <= 70 ? POWER_MEDIUM : POWER_HIGH;

        default:
            return POWER_UNKNOWN;
    }
}

static void ApplyResolvedConfig(Device* device) {
    device->m_config = Configs::Resolve(device->m_vendorID, device->m_productID, s_stickAxisButtons);
    device->m_labelStyle = device->m_config.m_labelStyle.empty() ? DetectLabelStyle(PadOf(device)) : device->m_config.m_labelStyle;
}

static void ApplyLed(Device* device) {
    if (s_ledSet) {
        SDL_SetJoystickLED(JoystickOf(device), static_cast<Uint8>(s_led[0] * 255.0f), static_cast<Uint8>(s_led[1] * 255.0f), static_cast<Uint8>(s_led[2] * 255.0f));
    }
}

static void CloseHandles(Device* device) {
    if (device->m_gamepad) {
        SDL_CloseGamepad(PadOf(device));
    } else if (device->m_joystick) {
        SDL_CloseJoystick(JoystickOf(device));
    }

    device->m_gamepad = nullptr;
    device->m_joystick = nullptr;
}

static bool OpenHandles(Device* device) {
    SDL_JoystickID id = static_cast<SDL_JoystickID>(device->m_id);

    if (SDL_IsGamepad(id)) {
        SDL_Gamepad* gamepad = SDL_OpenGamepad(id);

        if (gamepad) {
            device->m_gamepad = gamepad;
            device->m_joystick = SDL_GetGamepadJoystick(gamepad);
            return true;
        }
    }

    device->m_joystick = SDL_OpenJoystick(id);
    return device->m_joystick != nullptr;
}

static void Open(SDL_JoystickID id) {
    if (Find(static_cast<int32_t>(id))) {
        return;
    }

    auto device = std::make_unique<Device>();
    device->m_id = static_cast<int32_t>(id);

    if (!OpenHandles(device.get())) {
        return;
    }

    SDL_Joystick* joystick = JoystickOf(device.get());
    const char* name = device->m_gamepad ? SDL_GetGamepadName(PadOf(device.get())) : SDL_GetJoystickName(joystick);
    device->m_name = name ? name : "Gamepad";
    device->m_vendorID = SDL_GetJoystickVendor(joystick);
    device->m_productID = SDL_GetJoystickProduct(joystick);
    device->m_powerLevel = ReadPowerLevel(device.get());
    ApplyResolvedConfig(device.get());
    ApplyLed(device.get());

    if (s_activeID < 0) {
        s_activeID = device->m_id;
    }

    s_devices.push_back(std::move(device));
    Lua::Signal(Lua::EVENT_CONNECTED, nullptr);
}

static void Close(SDL_JoystickID id) {
    for (auto it = s_devices.begin(); it != s_devices.end(); ++it) {
        if ((*it)->m_id != static_cast<int32_t>(id)) {
            continue;
        }

        CloseHandles(it->get());
        s_devices.erase(it);

        if (s_activeID == static_cast<int32_t>(id)) {
            s_activeID = s_devices.empty() ? -1 : s_devices.front()->m_id;
        }

        Lua::Signal(Lua::EVENT_DISCONNECTED, nullptr);
        return;
    }
}

static void Reopen(Device* device) {
    CloseHandles(device);

    if (OpenHandles(device)) {
        ApplyResolvedConfig(device);
        ApplyLed(device);
    }
}

static float NormalizeAxis(Sint16 value) {
    return std::max(-1.0f, std::min(1.0f, value / 32767.0f));
}

static void ReadRaw(Device* device) {
    SDL_Joystick* joystick = JoystickOf(device);
    int buttons = std::max(0, SDL_GetNumJoystickButtons(joystick));
    int hats = std::max(0, SDL_GetNumJoystickHats(joystick));
    int axes = std::max(0, SDL_GetNumJoystickAxes(joystick));

    device->m_rawButtons.assign(buttons + hats * 4, 0);
    device->m_rawAxes.assign(axes, 0.0f);

    for (int i = 0; i < buttons; i++) {
        device->m_rawButtons[i] = SDL_GetJoystickButton(joystick, i) ? 1 : 0;
    }

    for (int i = 0; i < hats; i++) {
        Uint8 hat = SDL_GetJoystickHat(joystick, i);
        uint8_t* out = &device->m_rawButtons[buttons + i * 4];
        out[0] = (hat & SDL_HAT_UP) != 0;
        out[1] = (hat & SDL_HAT_RIGHT) != 0;
        out[2] = (hat & SDL_HAT_DOWN) != 0;
        out[3] = (hat & SDL_HAT_LEFT) != 0;
    }

    for (int i = 0; i < axes; i++) {
        device->m_rawAxes[i] = NormalizeAxis(SDL_GetJoystickAxis(joystick, i));
    }
}

struct PadButtonMap {
    SDL_GamepadButton m_sdl;
    Button m_button;
};

static const PadButtonMap s_padButtons[] = {
    { SDL_GAMEPAD_BUTTON_SOUTH, BUTTON_PAD1 },
    { SDL_GAMEPAD_BUTTON_EAST, BUTTON_PAD2 },
    { SDL_GAMEPAD_BUTTON_WEST, BUTTON_PAD3 },
    { SDL_GAMEPAD_BUTTON_NORTH, BUTTON_PAD4 },
    { SDL_GAMEPAD_BUTTON_TOUCHPAD, BUTTON_PAD5 },
    { SDL_GAMEPAD_BUTTON_MISC2, BUTTON_PAD6 },
    { SDL_GAMEPAD_BUTTON_LEFT_STICK, BUTTON_LSTICK },
    { SDL_GAMEPAD_BUTTON_RIGHT_STICK, BUTTON_RSTICK },
    { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, BUTTON_LSHOULDER },
    { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, BUTTON_RSHOULDER },
    { SDL_GAMEPAD_BUTTON_DPAD_UP, BUTTON_DPAD_UP },
    { SDL_GAMEPAD_BUTTON_DPAD_DOWN, BUTTON_DPAD_DOWN },
    { SDL_GAMEPAD_BUTTON_DPAD_LEFT, BUTTON_DPAD_LEFT },
    { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, BUTTON_DPAD_RIGHT },
    { SDL_GAMEPAD_BUTTON_START, BUTTON_FORWARD },
    { SDL_GAMEPAD_BUTTON_BACK, BUTTON_BACK },
    { SDL_GAMEPAD_BUTTON_GUIDE, BUTTON_SYSTEM },
    { SDL_GAMEPAD_BUTTON_MISC1, BUTTON_SOCIAL },
    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, BUTTON_PADDLE1 },
    { SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, BUTTON_PADDLE2 },
    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, BUTTON_PADDLE3 },
    { SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, BUTTON_PADDLE4 },
};

static void MapFromGamepad(Device* device, MappedState& state, float* axes) {
    SDL_Gamepad* gamepad = PadOf(device);

    for (const PadButtonMap& map : s_padButtons) {
        if (SDL_GetGamepadButton(gamepad, map.m_sdl)) {
            state.m_buttons[map.m_button] = true;
        }
    }

    axes[AXIS_LSTICK_X] = NormalizeAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX));
    axes[AXIS_LSTICK_Y] = -NormalizeAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
    axes[AXIS_RSTICK_X] = NormalizeAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX));
    axes[AXIS_RSTICK_Y] = -NormalizeAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY));
    axes[AXIS_LTRIGGER] = NormalizeAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    axes[AXIS_RTRIGGER] = NormalizeAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
}

static void MapFromRawConfig(Device* device, MappedState& state, float* axes) {
    for (const ResolvedRawButton& mapping : device->m_config.m_rawButtons) {
        if (mapping.m_rawIndex < 0 || mapping.m_rawIndex >= static_cast<int32_t>(device->m_rawButtons.size()) || !device->m_rawButtons[mapping.m_rawIndex]) {
            continue;
        }

        if (mapping.m_button >= 0) {
            state.m_buttons[mapping.m_button] = true;
        }

        if (mapping.m_axis >= 0) {
            axes[mapping.m_axis] = mapping.m_axisValue;
        }
    }

    for (const ResolvedRawAxis& mapping : device->m_config.m_rawAxes) {
        if (mapping.m_axis >= 0 && mapping.m_rawIndex >= 0 && mapping.m_rawIndex < static_cast<int32_t>(device->m_rawAxes.size())) {
            axes[mapping.m_axis] = device->m_rawAxes[mapping.m_rawIndex];
        }
    }
}

static void MapFromJoystick(Device* device, MappedState& state, float* axes) {
    SDL_Joystick* joystick = JoystickOf(device);
    int buttons = std::max(0, SDL_GetNumJoystickButtons(joystick));

    for (int i = 0; i < buttons && i < BUTTON_FORWARD; i++) {
        if (device->m_rawButtons[i]) {
            state.m_buttons[i] = true;
        }
    }

    if (static_cast<int>(device->m_rawButtons.size()) >= buttons + 4) {
        state.m_buttons[BUTTON_DPAD_UP] |= device->m_rawButtons[buttons] != 0;
        state.m_buttons[BUTTON_DPAD_RIGHT] |= device->m_rawButtons[buttons + 1] != 0;
        state.m_buttons[BUTTON_DPAD_DOWN] |= device->m_rawButtons[buttons + 2] != 0;
        state.m_buttons[BUTTON_DPAD_LEFT] |= device->m_rawButtons[buttons + 3] != 0;
    }

    for (int i = 0; i < AXIS_COUNT && i < static_cast<int>(device->m_rawAxes.size()); i++) {
        axes[i] = (i == AXIS_LSTICK_Y || i == AXIS_RSTICK_Y) ? -device->m_rawAxes[i] : device->m_rawAxes[i];
    }
}

static float ApplyDeadzone(float value, float deadzone) {
    if (deadzone <= 0.0f || deadzone >= 1.0f) {
        return value;
    }

    float magnitude = std::fabs(value);

    if (magnitude <= deadzone) {
        return 0.0f;
    }

    return std::copysign((magnitude - deadzone) / (1.0f - deadzone), value);
}

static void ReadMapped(Device* device) {
    MappedState state;
    float axes[AXIS_COUNT] = {};

    if (device->m_config.m_rawMapped) {
        MapFromRawConfig(device, state, axes);
    } else if (device->m_gamepad) {
        MapFromGamepad(device, state, axes);
    } else {
        MapFromJoystick(device, state, axes);
    }

    float scaled[AXIS_COUNT];

    for (int32_t i = 0; i < AXIS_COUNT; i++) {
        const ResolvedAxis& config = device->m_config.m_axes[i];
        scaled[i] = std::max(-1.0f, std::min(1.0f, (axes[i] + config.m_shift) * config.m_scale));

        float value = ApplyDeadzone(scaled[i], config.m_deadzone);
        state.m_axes[i] = value;

        if (config.m_buttonThreshold > 0.0f) {
            if (config.m_buttonPos >= 0 && value > config.m_buttonThreshold) {
                state.m_buttons[config.m_buttonPos] = true;
            }

            if (config.m_buttonNeg >= 0 && value < -config.m_buttonThreshold) {
                state.m_buttons[config.m_buttonNeg] = true;
            }
        }
    }

    for (int32_t i = 0; i < STICK_PHYSICAL_COUNT; i++) {
        const ResolvedStick& config = device->m_config.m_sticks[i];
        float x = config.m_axisX >= 0 ? scaled[config.m_axisX] : 0.0f;
        float y = config.m_axisY >= 0 ? scaled[config.m_axisY] : 0.0f;

        if (std::fabs(x) < config.m_deadzoneX) {
            x = 0.0f;
        }

        if (std::fabs(y) < config.m_deadzoneY) {
            y = 0.0f;
        }

        float length = std::sqrt(x * x + y * y);
        StickState& stick = state.m_sticks[i];

        if (length <= config.m_deadzone || length <= 0.0f) {
            stick = StickState();
            continue;
        }

        float scaledLength = std::min(1.0f, (length - config.m_deadzone) / (1.0f - std::min(config.m_deadzone, 0.99f)));
        stick.m_x = x / length * scaledLength;
        stick.m_y = y / length * scaledLength;
        stick.m_len = scaledLength;
    }

    device->m_state = state;
}

static bool HasActivity(const MappedState& previous, const MappedState& current) {
    for (int32_t i = 0; i < BUTTON_COUNT; i++) {
        if (current.m_buttons[i] && !previous.m_buttons[i]) {
            return true;
        }
    }

    for (int32_t i = 0; i < STICK_PHYSICAL_COUNT; i++) {
        if (current.m_sticks[i].m_len > 0.3f) {
            return true;
        }
    }

    return current.m_axes[AXIS_LTRIGGER] > 0.3f || current.m_axes[AXIS_RTRIGGER] > 0.3f;
}

static void BuildCombined() {
    MappedState combined;

    for (auto& device : s_devices) {
        const MappedState& state = device->m_state;

        for (int32_t i = 0; i < BUTTON_COUNT; i++) {
            combined.m_buttons[i] |= state.m_buttons[i];
        }

        for (int32_t i = 0; i < AXIS_COUNT; i++) {
            if (std::fabs(state.m_axes[i]) > std::fabs(combined.m_axes[i])) {
                combined.m_axes[i] = state.m_axes[i];
            }
        }

        for (int32_t i = 0; i < STICK_PHYSICAL_COUNT; i++) {
            if (state.m_sticks[i].m_len > combined.m_sticks[i].m_len) {
                combined.m_sticks[i] = state.m_sticks[i];
            }
        }
    }

    s_combined = combined;
}

static void ApplyRumble(Device* device, uint32_t duration) {
    float strength = CVars::VibrationStrength();
    Uint16 low = static_cast<Uint16>(std::min(1.0f, s_rumble[0] * strength) * 65535.0f);
    Uint16 high = static_cast<Uint16>(std::min(1.0f, s_rumble[1] * strength) * 65535.0f);
    Uint16 left = static_cast<Uint16>(std::min(1.0f, s_rumble[2] * strength) * 65535.0f);
    Uint16 right = static_cast<Uint16>(std::min(1.0f, s_rumble[3] * strength) * 65535.0f);

    SDL_RumbleJoystick(JoystickOf(device), low, high, duration);
    SDL_RumbleJoystickTriggers(JoystickOf(device), left, right, duration);
}

static void RefreshRumble() {
    if (!s_rumbleActive) {
        return;
    }

    uint64_t now = SDL_GetTicks();

    if (now < s_rumbleRefresh) {
        return;
    }

    s_rumbleRefresh = now + 500;

    for (auto& device : s_devices) {
        ApplyRumble(device.get(), 1000);
    }
}

bool Initialize() {
    if (s_initialized) {
        return true;
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC) && !SDL_Init(SDL_INIT_GAMEPAD)) {
        return false;
    }

    for (const std::string& mapping : Configs::LoadSDLMappings()) {
        SDL_AddGamepadMapping(mapping.c_str());
    }

    s_initialized = true;
    s_stickAxisButtons = CVars::StickAxisButtons();

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);

    for (int i = 0; ids && i < count; i++) {
        Open(ids[i]);
    }

    SDL_free(ids);
    return true;
}

void Shutdown() {
    if (!s_initialized) {
        return;
    }

    for (auto& device : s_devices) {
        CloseHandles(device.get());
    }

    s_devices.clear();
    s_combined = MappedState();
    s_activeID = -1;
    s_initialized = false;
    SDL_Quit();
}

bool Update() {
    if (!s_initialized) {
        return false;
    }

    if (s_stickAxisButtons != CVars::StickAxisButtons()) {
        s_stickAxisButtons = CVars::StickAxisButtons();
        ReloadConfigs();
    }

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_JOYSTICK_ADDED:
                Open(event.jdevice.which);
                break;

            case SDL_EVENT_JOYSTICK_REMOVED:
                Close(event.jdevice.which);
                break;

            case SDL_EVENT_GAMEPAD_ADDED: {
                Device* device = Find(static_cast<int32_t>(event.gdevice.which));

                if (device && !device->m_gamepad) {
                    Reopen(device);
                }

                break;
            }

            case SDL_EVENT_JOYSTICK_BATTERY_UPDATED: {
                Device* device = Find(static_cast<int32_t>(event.jbattery.which));

                if (device) {
                    int32_t level = ReadPowerLevel(device);

                    if (level != device->m_powerLevel) {
                        device->m_powerLevel = level;

                        if (device == Active()) {
                            Lua::Signal(Lua::EVENT_POWER_CHANGED, "%d", level);
                        }
                    }
                }

                break;
            }

            default:
                break;
        }
    }

    bool activity = false;

    for (auto& device : s_devices) {
        MappedState previous = device->m_state;
        ReadRaw(device.get());
        ReadMapped(device.get());

        if (HasActivity(previous, device->m_state)) {
            activity = true;
            s_activeID = device->m_id;
        }
    }

    BuildCombined();
    RefreshRumble();

    return activity;
}

void ReloadConfigs() {
    for (auto& device : s_devices) {
        ApplyResolvedConfig(device.get());
    }
}

bool AddMapping(const char* mapping) {
    if (!s_initialized || !mapping || SDL_AddGamepadMapping(mapping) < 0) {
        return false;
    }

    Configs::SaveSDLMapping(mapping);

    for (auto& device : s_devices) {
        if (!device->m_gamepad && SDL_IsGamepad(static_cast<SDL_JoystickID>(device->m_id))) {
            Reopen(device.get());
        }
    }

    return true;
}

void SetVibration(const char* type, float intensity) {
    intensity = std::max(0.0f, std::min(1.0f, intensity));

    if (type && !SDL_strcasecmp(type, "Low")) {
        s_rumble[0] = intensity;
    } else if (type && !SDL_strcasecmp(type, "High")) {
        s_rumble[1] = intensity;
    } else if (type && !SDL_strcasecmp(type, "LTrigger")) {
        s_rumble[2] = intensity;
    } else if (type && !SDL_strcasecmp(type, "RTrigger")) {
        s_rumble[3] = intensity;
    } else {
        s_rumble[0] = intensity;
        s_rumble[1] = intensity;
    }

    s_rumbleActive = s_rumble[0] > 0.0f || s_rumble[1] > 0.0f || s_rumble[2] > 0.0f || s_rumble[3] > 0.0f;
    s_rumbleRefresh = 0;

    if (!s_rumbleActive) {
        StopVibration();
    }
}

void StopVibration() {
    for (float& value : s_rumble) {
        value = 0.0f;
    }

    s_rumbleActive = false;

    for (auto& device : s_devices) {
        ApplyRumble(device.get(), 0);
    }
}

void SetLed(float r, float g, float b) {
    s_led[0] = std::max(0.0f, std::min(1.0f, r));
    s_led[1] = std::max(0.0f, std::min(1.0f, g));
    s_led[2] = std::max(0.0f, std::min(1.0f, b));
    s_ledSet = true;

    for (auto& device : s_devices) {
        ApplyLed(device.get());
    }
}

void ClearLed() {
    s_ledSet = false;

    for (auto& device : s_devices) {
        SDL_SetJoystickLED(JoystickOf(device.get()), 0, 0, 0);
    }
}

#else

bool Initialize() {
    return false;
}

void Shutdown() {
}

bool Update() {
    return false;
}

void ReloadConfigs() {
}

bool AddMapping(const char* mapping) {
    return false;
}

void SetVibration(const char* type, float intensity) {
}

void StopVibration() {
}

void SetLed(float r, float g, float b) {
    s_led[0] = r;
    s_led[1] = g;
    s_led[2] = b;
    s_ledSet = true;
}

void ClearLed() {
    s_ledSet = false;
}

#endif

bool GetLed(float& r, float& g, float& b) {
    r = s_led[0];
    g = s_led[1];
    b = s_led[2];
    return s_ledSet;
}

}
