#include "Controller/Input.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#endif
#include "Controller/Buttons.h"
#include "Controller/CVars.h"
#include "Controller/Devices.h"
#include "Controller/Frames.h"
#include "Controller/Lua.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/UIUtil/InputControl.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/UIBindings.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace Controller::Input {

enum CaptureType {
    CAPTURE_NONE = 0,
    CAPTURE_FRAME,
    CAPTURE_LEFT_CLICK,
    CAPTURE_RIGHT_CLICK,
    CAPTURE_KEY,
};

struct Capture {
    CaptureType m_type = CAPTURE_NONE;
    KEY m_key = KEY_NONE;
};

static const uint32_t MOVE_FORWARD = 0x10;
static const uint32_t MOVE_BACKWARD = 0x20;
static const uint32_t STRAFE_LEFT = 0x40;
static const uint32_t STRAFE_RIGHT = 0x80;
static const uint32_t GAMEPAD_LOOK = 0x2000000;
static const uint32_t MOUSE_LOOK = 0x3;
static const uint32_t CURSOR_HIDDEN = 0x40;

static const float LOOK_SCALE_X = 800.0f;
static const float LOOK_SCALE_Y = 600.0f;
static const float MOVE_START = 0.25f;
static const float MOVE_SECTOR = 0.383f;
static const float WALK_HYSTERESIS = 0.05f;
static const float WHEEL_NOTCHES_PER_SECOND = 12.0f;
static const int32_t WHEEL_DELTA_NOTCH = 120;
static const int32_t MOUSE_MOVE_TOLERANCE = 4;

static bool s_initialized = false;
static bool s_appActive = true;
static bool s_padActive = false;
static bool s_cursorManual = false;
static bool s_cursorAuto = false;
static bool s_autoSuppressed = false;
static bool s_allowHover = false;
static bool s_hidCursor = false;
static bool s_looking = false;
static bool s_walking = false;
static uint32_t s_moveBits = 0;

static bool s_posted[BUTTON_COUNT];
static KEY s_postedKey[BUTTON_COUNT];
static Capture s_capture[BUTTON_COUNT];

static std::unordered_map<uint32_t, int32_t> s_syntheticKeys;
static int32_t s_syntheticMouseDowns = 0;

static bool s_haveCursor = false;
static int32_t s_cursorX = 0;
static int32_t s_cursorY = 0;
static float s_cursorHeld = 0.0f;
static float s_cursorAccumX = 0.0f;
static float s_cursorAccumY = 0.0f;
static float s_wheelAccum = 0.0f;

static bool InGame() {
    return CGGameUI::m_initialized && ClntObjMgrGetActivePlayer();
}

static CGUnit_C* Mover() {
    return static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(CGUnit_C::s_activeMover, TYPE_UNIT, __FILE__, __LINE__));
}

static bool CursorModeActive() {
    if (!CVars::Enabled()) {
        return false;
    }

    if (!InGame()) {
        return true;
    }

    return s_cursorManual || (CVars::CursorAutoEnable() && s_cursorAuto && !s_autoSuppressed);
}

static void PostKey(KEY key, bool down) {
    s_syntheticKeys[static_cast<uint32_t>(key)]++;
    OsQueuePut(down ? OS_INPUT_KEY_DOWN : OS_INPUT_KEY_UP, key, 1, 0, 0);
}

static bool ConsumeSyntheticKey(KEY key) {
    auto it = s_syntheticKeys.find(static_cast<uint32_t>(key));

    if (it == s_syntheticKeys.end() || it->second <= 0) {
        return false;
    }

    it->second--;
    return true;
}

static void PostClick(MOUSEBUTTON button, bool down) {
    int32_t x;
    int32_t y;
    OsInputGetMousePosition(&x, &y);

    if (down) {
        s_syntheticMouseDowns++;
    }

    OsQueuePut(down ? OS_INPUT_MOUSE_DOWN : OS_INPUT_MOUSE_UP, button, x, y, 0);
}

static bool AnyClickHeld() {
    for (const Capture& capture : s_capture) {
        if (capture.m_type == CAPTURE_LEFT_CLICK || capture.m_type == CAPTURE_RIGHT_CLICK) {
            return true;
        }
    }

    return false;
}

static void StopLook() {
    if (!s_looking) {
        return;
    }

    s_looking = false;
    CGInputControl* input = CGInputControl::GetActive();

    if (!input) {
        return;
    }

    uint32_t time = OsGetAsyncTimeMs();

    if (input->UnsetControlBit(GAMEPAD_LOOK, time, 0)) {
        input->UpdatePlayer(time, 1);
    }
}

static void SetMoveBits(uint32_t bits) {
    CGInputControl* input = CGInputControl::GetActive();

    if (!input || bits == s_moveBits) {
        return;
    }

    uint32_t time = OsGetAsyncTimeMs();
    static const uint32_t s_bits[] = { MOVE_FORWARD, MOVE_BACKWARD, STRAFE_LEFT, STRAFE_RIGHT };

    for (uint32_t bit : s_bits) {
        bool want = (bits & bit) != 0;
        bool have = (s_moveBits & bit) != 0;

        if (want == have) {
            continue;
        }

        if (want && !CGGameUI::CanPerformAction(0)) {
            continue;
        }

        input->ToggleControlBit(bit, want, time, 0);
        s_moveBits = want ? (s_moveBits | bit) : (s_moveBits & ~bit);
    }
}

static void SetWalking(bool walk) {
    if (walk == s_walking) {
        return;
    }

    CGUnit_C* mover = Mover();

    if (!mover || mover->m_unit->m_unitFieldHealth <= 0 || !CGInputControl::CanMove(mover)) {
        return;
    }

    if (((mover->m_movementData.m_flags & MOVEMENTFLAG_WALKING) != 0) != walk) {
        mover->ToggleRunModeLocal(OsGetAsyncTimeMs());
    }

    s_walking = walk;
}

static void StopMovement() {
    SetMoveBits(0);
    SetWalking(false);
}

static void ShowCursor() {
    CGInputControl* input = CGInputControl::GetActive();

    if (!s_hidCursor || !input) {
        return;
    }

    s_hidCursor = false;
    input->m_mouseModeFlags &= ~CURSOR_HIDDEN;
    input->UpdateMouseMode(0);
}

static void CenterCursor() {
    RECT rect;

    if (!OsGetDefaultWindowRect(&rect)) {
        return;
    }

    s_cursorX = (rect.right - rect.left) / 2;
    s_cursorY = (rect.bottom - rect.top) / 2;
    s_haveCursor = true;
    OsInputSetMousePosition(s_cursorX, s_cursorY);
}

static void HideCursor() {
    CGInputControl* input = CGInputControl::GetActive();

    if (!input) {
        return;
    }

    if (!s_hidCursor && CVars::CursorCentering() && s_appActive) {
        CenterCursor();
    }

    if (!(input->m_mouseModeFlags & CURSOR_HIDDEN)) {
        input->m_mouseModeFlags |= CURSOR_HIDDEN;
        input->UpdateMouseMode(0);
    }

    s_hidCursor = true;
}

static void ReleaseButtons() {
    for (int32_t button = 0; button < BUTTON_COUNT; button++) {
        if (s_posted[button]) {
            s_posted[button] = false;
            PostKey(s_postedKey[button], false);
        }
    }
}

void Release() {
    ReleaseButtons();
    StopMovement();
    StopLook();
    ShowCursor();
    Devices::StopVibration();
}

static void SetPadActive(bool active) {
    if (active == s_padActive) {
        return;
    }

    s_padActive = active;

    if (!active) {
        StopMovement();
        StopLook();
        ShowCursor();
    }

    Lua::Signal(Lua::EVENT_ACTIVE_CHANGED, "%b", active ? 1 : 0);
}

bool IsActive() {
    return s_padActive;
}

bool IsCursorControlEnabled() {
    return CursorModeActive();
}

void SetCursorControl(bool enable) {
    if (enable) {
        s_cursorManual = true;
        return;
    }

    s_cursorManual = false;
    s_autoSuppressed = s_cursorAuto;
}

void SetAutoCursor(bool open) {
    s_cursorAuto = open;

    if (!open) {
        s_autoSuppressed = false;
    }
}

bool IsFreeLookEnabled() {
    return s_padActive && InGame() && !CursorModeActive();
}

bool GetAllowHoverEventsWithFreeLook() {
    return s_allowHover;
}

void SetAllowHoverEventsWithFreeLook(bool enable) {
    if (enable == s_allowHover) {
        return;
    }

    s_allowHover = enable;
    Lua::Signal(Lua::EVENT_HOVER_CHANGED, "%b", enable ? 1 : 0);
}

static KEY EmulatedModifier(int32_t button) {
    if (button == CVars::EmulateShift()) {
        return KEY_LSHIFT;
    }

    if (button == CVars::EmulateCtrl()) {
        return KEY_LCONTROL;
    }

    if (button == CVars::EmulateAlt()) {
        return KEY_LALT;
    }

    return KEY_NONE;
}

void OnPoll(bool appActive, bool activity) {
    s_appActive = appActive;

    if (activity && appActive && CVars::Enabled()) {
        SetPadActive(true);
    }

    const MappedState& state = Devices::Combined();
    bool accept = appActive && CVars::Enabled();

    for (int32_t button = 0; button < BUTTON_COUNT; button++) {
        bool down = accept && state.m_buttons[button];

        if (down == s_posted[button]) {
            continue;
        }

        if (down) {
            KEY modifier = EmulatedModifier(button);
            s_postedKey[button] = modifier != KEY_NONE ? modifier : ButtonKey(button);
        }

        s_posted[button] = down;
        PostKey(s_postedKey[button], down);
    }
}

static bool ModifierHeld() {
    return EventIsKeyDown(KEY_LSHIFT) || EventIsKeyDown(KEY_RSHIFT) || EventIsKeyDown(KEY_LCONTROL) || EventIsKeyDown(KEY_RCONTROL) || EventIsKeyDown(KEY_LALT) || EventIsKeyDown(KEY_RALT);
}

static bool IsBound(const char* key) {
    CGUIBindings* bindings = CGUIBindings::s_bindings;
    return bindings && bindings->GetCommandAction(BINDING_MODE_0, key, 1);
}

// Start behaves like Escape in game unless the player bound it to something else.
static KEY TranslatedKey(int32_t button) {
    if (!InGame()) {
        switch (button) {
            case BUTTON_FORWARD:
                return KEY_ENTER;

            case BUTTON_BACK:
                return KEY_ESCAPE;

            default:
                return KEY_NONE;
        }
    }

    if (button == BUTTON_FORWARD && !ModifierHeld() && !IsBound(ButtonBinding(button))) {
        return KEY_ESCAPE;
    }

    return KEY_NONE;
}

static int32_t OnKeyDown(const void* data, void* param) {
    const EVENT_DATA_KEY* key = static_cast<const EVENT_DATA_KEY*>(data);
    bool synthetic = ConsumeSyntheticKey(key->m_key);
    int32_t button = KeyButton(key->m_key);

    if (!synthetic) {
        if (button < 0) {
            SetPadActive(false);
        }

        return 1;
    }

    if (button < 0) {
        return 1;
    }

    Capture& capture = s_capture[button];
    capture = Capture();

    if (Frames::ButtonDown(button)) {
        capture.m_type = CAPTURE_FRAME;
        return 0;
    }

    if (CursorModeActive()) {
        if (button == CVars::CursorLeftClick()) {
            capture.m_type = CAPTURE_LEFT_CLICK;
            PostClick(MOUSE_BUTTON_LEFT, true);
            return 0;
        }

        if (button == CVars::CursorRightClick()) {
            capture.m_type = CAPTURE_RIGHT_CLICK;
            PostClick(MOUSE_BUTTON_RIGHT, true);
            return 0;
        }
    }

    KEY translated = TranslatedKey(button);

    if (translated != KEY_NONE) {
        capture.m_type = CAPTURE_KEY;
        capture.m_key = translated;
        PostKey(translated, true);
        return 0;
    }

    return 1;
}

static int32_t OnKeyUp(const void* data, void* param) {
    const EVENT_DATA_KEY* key = static_cast<const EVENT_DATA_KEY*>(data);

    if (!ConsumeSyntheticKey(key->m_key)) {
        return 1;
    }

    int32_t button = KeyButton(key->m_key);

    if (button < 0) {
        return 1;
    }

    Capture capture = s_capture[button];
    s_capture[button] = Capture();

    switch (capture.m_type) {
        case CAPTURE_FRAME:
            Frames::ButtonUp(button);
            return 0;

        case CAPTURE_LEFT_CLICK:
            PostClick(MOUSE_BUTTON_LEFT, false);
            return 0;

        case CAPTURE_RIGHT_CLICK:
            PostClick(MOUSE_BUTTON_RIGHT, false);
            return 0;

        case CAPTURE_KEY:
            PostKey(capture.m_key, false);
            return 0;

        default:
            return 1;
    }
}

static int32_t OnMouseDown(const void* data, void* param) {
    if (s_syntheticMouseDowns > 0) {
        s_syntheticMouseDowns--;
    } else {
        SetPadActive(false);
    }

    return 1;
}

static void UpdateLook(const StickState& stick, float elapsed) {
    CGInputControl* input = CGInputControl::GetActive();
    CGCamera* camera = CGWorldFrame::GetActiveCamera();

    if (!input || !camera || (input->m_flags & MOUSE_LOOK) || stick.m_len <= 0.0f) {
        StopLook();
        return;
    }

    uint32_t time = OsGetAsyncTimeMs();

    if (!(input->m_flags & GAMEPAD_LOOK) && input->SetControlBit(GAMEPAD_LOOK, time)) {
        input->UpdatePlayer(time, 1);
    }

    s_looking = true;

    float x = stick.m_x * std::fabs(stick.m_x);
    float y = stick.m_y * std::fabs(stick.m_y);
    camera->UpdateFreeLookFacing(x * LOOK_SCALE_X * CVars::CameraYawSpeed() * elapsed, -y * LOOK_SCALE_Y * CVars::CameraPitchSpeed() * elapsed, nullptr);

    CGUnit_C* mover = Mover();

    if (mover && input->CanSyncFreeLookFacing(mover)) {
        camera->SyncFreeLookFacing();
    }
}

static void UpdateMovement(const StickState& stick) {
    uint32_t bits = 0;

    if (stick.m_len > MOVE_START) {
        float x = stick.m_x / stick.m_len;
        float y = stick.m_y / stick.m_len;

        if (y > MOVE_SECTOR) {
            bits |= MOVE_FORWARD;
        } else if (y < -MOVE_SECTOR) {
            bits |= MOVE_BACKWARD;
        }

        if (x > MOVE_SECTOR) {
            bits |= STRAFE_RIGHT;
        } else if (x < -MOVE_SECTOR) {
            bits |= STRAFE_LEFT;
        }
    }

    SetMoveBits(bits);

    float threshold = CVars::RunThreshold();

    if (!bits || threshold <= 0.0f) {
        SetWalking(false);
    } else if (stick.m_len < threshold - WALK_HYSTERESIS) {
        SetWalking(true);
    } else if (stick.m_len > threshold + WALK_HYSTERESIS) {
        SetWalking(false);
    }
}

static void MoveCursor(const StickState& stick, float elapsed) {
    if (stick.m_len <= 0.0f || !s_appActive) {
        s_cursorHeld = 0.0f;
        s_cursorAccumX = 0.0f;
        s_cursorAccumY = 0.0f;
        return;
    }

    RECT rect;

    if (!OsGetDefaultWindowRect(&rect)) {
        return;
    }

    int32_t width = rect.right - rect.left;
    int32_t height = rect.bottom - rect.top;

    s_cursorHeld += elapsed;

    float speed = CVars::CursorSpeedStart() + CVars::CursorSpeedAccel() * s_cursorHeld;
    speed = speed < CVars::CursorSpeedMax() ? speed : CVars::CursorSpeedMax();
    speed *= height * stick.m_len * stick.m_len;

    s_cursorAccumX += stick.m_x / stick.m_len * speed * elapsed;
    s_cursorAccumY -= stick.m_y / stick.m_len * speed * elapsed;

    int32_t dx = static_cast<int32_t>(s_cursorAccumX);
    int32_t dy = static_cast<int32_t>(s_cursorAccumY);

    if (!dx && !dy) {
        return;
    }

    s_cursorAccumX -= dx;
    s_cursorAccumY -= dy;

    int32_t x;
    int32_t y;
    OsInputGetMousePosition(&x, &y);

    x += dx;
    y += dy;
    x = x < 0 ? 0 : x >= width ? width - 1 : x;
    y = y < 0 ? 0 : y >= height ? height - 1 : y;

    OsInputSetMousePosition(x, y);
    s_cursorX = x;
    s_cursorY = y;
    s_haveCursor = true;
}

static bool CursorOverInterface() {
    CSimpleTop* top = CSimpleTop::s_instance;

    if (!top || !top->m_mouseFocus) {
        return false;
    }

    return !InGame() || top->m_mouseFocus != CGWorldFrame::s_currentWorldFrame;
}

static void Scroll(const StickState& stick, float elapsed) {
    if (stick.m_len <= 0.0f) {
        s_wheelAccum = 0.0f;
        return;
    }

    s_wheelAccum += stick.m_y * WHEEL_NOTCHES_PER_SECOND * elapsed;

    while (std::fabs(s_wheelAccum) >= 1.0f) {
        int32_t direction = s_wheelAccum > 0.0f ? 1 : -1;
        s_wheelAccum -= direction;

        int32_t x;
        int32_t y;
        OsInputGetMousePosition(&x, &y);
        OsQueuePut(OS_INPUT_MOUSE_WHEEL, direction * WHEEL_DELTA_NOTCH, x, y, 0);
    }
}

static void DetectMouseMotion() {
    CGInputControl* input = CGInputControl::GetActive();

    if (!s_appActive || (input && (input->m_flags & MOUSE_LOOK))) {
        s_haveCursor = false;
        return;
    }

    int32_t x;
    int32_t y;
    OsInputGetMousePosition(&x, &y);

    if (s_haveCursor && !AnyClickHeld() && (std::abs(x - s_cursorX) > MOUSE_MOVE_TOLERANCE || std::abs(y - s_cursorY) > MOUSE_MOVE_TOLERANCE)) {
        SetPadActive(false);
    }

    s_cursorX = x;
    s_cursorY = y;
    s_haveCursor = true;
}

void OnIdle(float elapsed) {
    s_syntheticKeys.clear();
    s_syntheticMouseDowns = 0;

    if (!CVars::Enabled()) {
        return;
    }

    DetectMouseMotion();

    bool inGame = InGame();
    bool cursorMode = CursorModeActive();

    if (inGame && s_padActive && !cursorMode) {
        HideCursor();
    } else {
        ShowCursor();
    }

    const MappedState& state = Devices::Combined();

    if (Frames::DispatchSticks(state)) {
        StopMovement();
        StopLook();
        return;
    }

    const StickState& left = state.m_sticks[STICK_LEFT];
    const StickState& right = state.m_sticks[STICK_RIGHT];

    if (cursorMode) {
        StopMovement();
        MoveCursor(left, elapsed);

        if (!inGame || CursorOverInterface()) {
            StopLook();
            Scroll(right, elapsed);
        } else {
            UpdateLook(right, elapsed);
        }

        return;
    }

    if (!inGame) {
        return;
    }

    UpdateMovement(left);
    UpdateLook(right, elapsed);
}

static const OBJECT_TYPE INTERACT_TYPES = static_cast<OBJECT_TYPE>(TYPE_UNIT | TYPE_GAMEOBJECT);

struct InteractCandidate {
    WOWGUID m_guid;
    float m_score;
};

struct InteractSearch {
    CGPlayer_C* m_player;
    C3Vector m_position;
    float m_facing;
    float m_range;
    std::vector<InteractCandidate> m_candidates;
};

// The game checks here can read spell records per lock, so they only run on the few candidates in front of the player.
static bool CanInteractWith(CGPlayer_C* player, CGObject_C* object, uint32_t time) {
    uint32_t type = object->m_obj->m_type;

    if (type & TYPE_UNIT) {
        CGUnit_C* unit = static_cast<CGUnit_C*>(object);

        if (unit->m_unit->m_unitFieldHealth <= 0) {
            return unit->CanBeLooted(time) && player->CanLoot(unit);
        }

        return unit->m_unit->m_unitNpcFlags && player->CanInteractNow(unit);
    }

    if (type & TYPE_GAMEOBJECT) {
        CGGameObject_C* gameObject = static_cast<CGGameObject_C*>(object);
        return gameObject->CanUse() && gameObject->CanUseNow();
    }

    return false;
}

static int32_t InteractProc(WOWGUID guid, void* param) {
    InteractSearch& search = *static_cast<InteractSearch*>(param);
    CGObject_C* object = ClntObjMgrObjectPtr(guid, INTERACT_TYPES, __FILE__, __LINE__);

    if (!object || object == search.m_player) {
        return 1;
    }

    float reach = 0.0f;

    if (object->m_obj->m_type & TYPE_UNIT) {
        CGUnit_C* unit = static_cast<CGUnit_C*>(object);
        reach = unit->m_unit->m_unitFieldCombatreach;

        if (unit->m_unit->m_unitFieldHealth > 0 && !unit->m_unit->m_unitNpcFlags) {
            return 1;
        }
    }

    C3Vector position = object->GetPosition();
    float dx = position.x - search.m_position.x;
    float dy = position.y - search.m_position.y;
    float dz = position.z - search.m_position.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (distance > search.m_range + reach) {
        return 1;
    }

    float angle = std::fabs(std::remainder(std::atan2(dy, dx) - search.m_facing, 6.2831855f));

    if (distance > 1.0f && angle > 1.75f) {
        return 1;
    }

    search.m_candidates.push_back({ guid, distance * (1.0f + angle) });
    return 1;
}

bool Interact() {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return false;
    }

    if (s_hidCursor && CGGameUI::m_currentObjectTrack) {
        return CGGameUI::OnSpriteRightClick(CGGameUI::m_currentObjectTrack) != 0;
    }

    InteractSearch search;
    search.m_player = player;
    search.m_position = player->GetPosition();
    search.m_facing = player->GetFacing();
    search.m_range = CVars::InteractRange();

    ClntObjMgrEnumVisibleObjects(InteractProc, &search);

    std::sort(search.m_candidates.begin(), search.m_candidates.end(), [](const InteractCandidate& a, const InteractCandidate& b) {
        return a.m_score < b.m_score;
    });

    uint32_t time = OsGetAsyncTimeMs();

    for (const InteractCandidate& candidate : search.m_candidates) {
        CGObject_C* object = ClntObjMgrObjectPtr(candidate.m_guid, INTERACT_TYPES, __FILE__, __LINE__);

        if (object && CanInteractWith(player, object, time)) {
            return CGGameUI::OnSpriteRightClick(candidate.m_guid) != 0;
        }
    }

    return false;
}

void Initialize() {
    if (s_initialized) {
        return;
    }

    s_initialized = true;

    for (KEY& key : s_postedKey) {
        key = KEY_NONE;
    }

    EventRegisterEx(EVENT_ID_KEYDOWN, OnKeyDown, nullptr, 5.0f);
    EventRegisterEx(EVENT_ID_KEYUP, OnKeyUp, nullptr, 5.0f);
    EventRegisterEx(EVENT_ID_MOUSEDOWN, OnMouseDown, nullptr, 5.0f);
}

void Shutdown() {
    if (!s_initialized) {
        return;
    }

    Release();
    EventUnregisterEx(EVENT_ID_KEYDOWN, OnKeyDown, nullptr, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_KEYUP, OnKeyUp, nullptr, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_MOUSEDOWN, OnMouseDown, nullptr, 0xFFFFFFFF);
    s_initialized = false;
}

}
