#include "HoT/Editor/FreeCam.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <ctime>
#include "Engine/Source/Os/Gui.h"

// QueryPerformance* on CLOCK_MONOTONIC in nanoseconds, as the Linux TimeManager does.
static int32_t QueryPerformanceFrequency(LARGE_INTEGER* frequency) {
    frequency->QuadPart = 1000000000;
    return 1;
}

static int32_t QueryPerformanceCounter(LARGE_INTEGER* count) {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    count->QuadPart = static_cast<int64_t>(now.tv_sec) * 1000000000 + now.tv_nsec;
    return 1;
}
#endif
#include <cmath>
#include <cstdint>
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Frame/CSimpleEditBox.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Os/TimeManager.h"
#include "HoT/Core/Chat.h"
#include "HoT/Editor/VectorMath.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/Movement.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/Types.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Editor::FreeCam {

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr uint32_t kHeartbeatIntervalMs = 250;

static constexpr float kMinSpeed = 1.0f;
static constexpr float kMaxSpeed = 4000.0f;
static constexpr float kSpeedStep = 1.25f;

struct FreeCamState {
    bool m_active = false;
    bool m_hideCharacter = true;
    bool m_syncCharacter = true;

    float m_speed = 40.0f;
    float m_maxLag = 60.0f;

    C3Vector m_position{};

    bool m_forward = false;
    bool m_back = false;
    bool m_left = false;
    bool m_right = false;
    bool m_up = false;
    bool m_down = false;

    bool m_haveLastTick = false;
    LARGE_INTEGER m_lastTick{};

    bool m_characterHidden = false;
    bool m_gravityForced = false;
    bool m_gravityWasSet = false;
    uint32_t m_lastHeartbeatMs = 0;

    bool m_eventsRegistered = false;
};

static FreeCamState s_state;

static bool KeyboardIsCaptured() {
    return CSimpleEditBox::s_currentFocus != nullptr;
}

static bool* SlotForKey(KEY key) {
    switch (key) {
        case KEY_W:
            return &s_state.m_forward;
        case KEY_S:
            return &s_state.m_back;
        case KEY_A:
            return &s_state.m_left;
        case KEY_D:
            return &s_state.m_right;
        case KEY_SPACE:
        case KEY_E:
            return &s_state.m_up;
        case KEY_X:
        case KEY_Q:
            return &s_state.m_down;
        default:
            return nullptr;
    }
}

static void ClearKeys() {
    s_state.m_forward = false;
    s_state.m_back = false;
    s_state.m_left = false;
    s_state.m_right = false;
    s_state.m_up = false;
    s_state.m_down = false;
}

static void SetSpeed(float speed, bool announce) {
    if (speed < kMinSpeed) {
        speed = kMinSpeed;
    }

    if (speed > kMaxSpeed) {
        speed = kMaxSpeed;
    }

    s_state.m_speed = speed;

    if (announce) {
        Chat::Print("FreeCam: %.0f yards a second", speed);
    }
}

static bool HandleSpeedKey(KEY key, bool announce) {
    if (key == KEY_PLUS || key == KEY_NUMPAD_PLUS) {
        SetSpeed(s_state.m_speed * kSpeedStep, announce);
        return true;
    }

    if (key == KEY_MINUS || key == KEY_NUMPAD_MINUS) {
        SetSpeed(s_state.m_speed / kSpeedStep, announce);
        return true;
    }

    return false;
}

static int32_t OnKeyDown(const void* data, void* param) {
    if (!s_state.m_active || !data || KeyboardIsCaptured()) {
        return 1;
    }

    const EVENT_DATA_KEY* key = static_cast<const EVENT_DATA_KEY*>(data);

    if (HandleSpeedKey(key->m_key, true)) {
        return 0;
    }

    bool* slot = SlotForKey(key->m_key);

    if (!slot) {
        return 1;
    }

    *slot = true;

    return 0;
}

static int32_t OnKeyUp(const void* data, void* param) {
    if (!s_state.m_active || !data) {
        return 1;
    }

    const EVENT_DATA_KEY* key = static_cast<const EVENT_DATA_KEY*>(data);
    bool* slot = SlotForKey(key->m_key);

    if (!slot || !*slot) {
        return 1;
    }

    *slot = false;

    return 0;
}

static int32_t OnKeyRepeat(const void* data, void* param) {
    if (!s_state.m_active || !data || KeyboardIsCaptured()) {
        return 1;
    }

    const EVENT_DATA_KEY* key = static_cast<const EVENT_DATA_KEY*>(data);

    if (HandleSpeedKey(key->m_key, false)) {
        return 0;
    }

    bool* slot = SlotForKey(key->m_key);

    return slot && *slot ? 0 : 1;
}

static float StepSeconds() {
    static LARGE_INTEGER s_frequency = {};

    if (s_frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&s_frequency);
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    if (!s_state.m_haveLastTick) {
        s_state.m_lastTick = now;
        s_state.m_haveLastTick = true;

        return 0.0f;
    }

    float seconds = static_cast<float>(now.QuadPart - s_state.m_lastTick.QuadPart) / static_cast<float>(s_frequency.QuadPart);
    s_state.m_lastTick = now;

    if (seconds < 0.0f) {
        return 0.0f;
    }

    if (seconds > 0.1f) {
        seconds = 0.1f;
    }

    return seconds;
}

static void ApplyCharacterVisibility() {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return;
    }

    if (!s_state.m_active || !s_state.m_hideCharacter) {
        if (!s_state.m_characterHidden) {
            return;
        }

        s_state.m_characterHidden = false;
        player->UpdateDisplayInfo(1);

        return;
    }

    CM2Model* model = player->GetObjectModel();

    // Geometry calls on a model that has not loaded queue a heap record instead of applying.
    if (!model || !(model->m_fFlags & 0x1)) {
        return;
    }

    model->SetGeometryVisible(0, 0xFFFFFFFF, 0);
    s_state.m_characterHidden = true;
}

static void SyncCharacter(const C3Vector& target) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player || !player->m_movement) {
        return;
    }

    CMovement_C* movement = player->m_movement;

    if (!s_state.m_gravityForced) {
        s_state.m_gravityWasSet = (movement->m_flags & MOVEMENTFLAG_DISABLE_GRAVITY) != 0;
        s_state.m_gravityForced = true;
    }

    movement->m_flags |= MOVEMENTFLAG_DISABLE_GRAVITY;

    C3Vector& position = movement->m_position;
    C3Vector& anchor = movement->m_anchorPos;
    float dx = target.x - position.x;
    float dy = target.y - position.y;
    float dz = target.z - position.z;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);

    if (distance > s_state.m_maxLag) {
        float keep = s_state.m_maxLag * 0.5f;
        float amount = (distance - keep) / distance;

        float mx = dx * amount;
        float my = dy * amount;
        float mz = dz * amount;

        position.x += mx;
        position.y += my;
        position.z += mz;
        anchor.x += mx;
        anchor.y += my;
        anchor.z += mz;
    }

    uint32_t nowMs = OsGetAsyncTimeMs();

    if (nowMs - s_state.m_lastHeartbeatMs >= kHeartbeatIntervalMs) {
        s_state.m_lastHeartbeatMs = nowMs;
        player->SendMovementUpdate(nowMs, MSG_MOVE_HEARTBEAT, 0.0f, 0, 0, 0xFF);
    }
}

static void ReleaseCharacter() {
    if (!s_state.m_gravityForced) {
        return;
    }

    s_state.m_gravityForced = false;

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player || !player->m_movement || s_state.m_gravityWasSet) {
        return;
    }

    player->m_movement->m_flags &= ~MOVEMENTFLAG_DISABLE_GRAVITY;
}

static void Fly(CGCamera* camera, float seconds) {
    C3Vector forward = VectorMath::Normalize(camera->Forward());

    const C3Vector worldUp{ 0.0f, 0.0f, 1.0f };
    C3Vector right = VectorMath::Cross(forward, worldUp);

    if (VectorMath::Length(right) < 0.001f) {
        right = camera->Right();
    }

    right = VectorMath::Normalize(right);

    C3Vector move{};

    if (s_state.m_forward) {
        move = VectorMath::Add(move, forward);
    }

    if (s_state.m_back) {
        move = VectorMath::Subtract(move, forward);
    }

    if (s_state.m_right) {
        move = VectorMath::Add(move, right);
    }

    if (s_state.m_left) {
        move = VectorMath::Subtract(move, right);
    }

    if (s_state.m_up) {
        move.z += 1.0f;
    }

    if (s_state.m_down) {
        move.z -= 1.0f;
    }

    float length = VectorMath::Length(move);

    if (length > 0.0f && seconds > 0.0f) {
        float speed = s_state.m_speed;

#if defined(_WIN32)
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
            speed *= 4.0f;
        }

        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
            speed *= 0.25f;
        }
#else
        // SDL modifier state (1 = shift, 0 = control) stands in for GetAsyncKeyState.
        if (OsGuiIsModifierKeyDown(1)) {
            speed *= 4.0f;
        }

        if (OsGuiIsModifierKeyDown(0)) {
            speed *= 0.25f;
        }
#endif

        s_state.m_position = VectorMath::Add(s_state.m_position, VectorMath::Scale(move, speed * seconds / length));
    }
}

static void Deactivate(bool announce);

static void Tick(CGCamera* camera) {
    if (s_state.m_active && !Session::HasClearance(kRequiredSecurityLevel)) {
        Deactivate(false);
    }

    ApplyCharacterVisibility();

    if (!s_state.m_active) {
        s_state.m_haveLastTick = false;
        return;
    }

    Fly(camera, StepSeconds());

    camera->m_position = s_state.m_position;

    if (s_state.m_syncCharacter) {
        SyncCharacter(s_state.m_position);
    }
}

static bool PlayerPosition(C3Vector& out) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return false;
    }

    out = player->GetPosition();

    return true;
}

static void Activate(bool announce) {
    if (s_state.m_active || !Session::HasClearance(kRequiredSecurityLevel)) {
        return;
    }

    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    if (!frame || !frame->m_camera) {
        Chat::Print("FreeCam: no world camera to take over");
        return;
    }

    s_state.m_position = frame->m_camera->m_position;
    s_state.m_haveLastTick = false;
    ClearKeys();
    s_state.m_active = true;

    if (!announce) {
        return;
    }

    Chat::Print("FreeCam: on at %.0f yards a second. W A S D to fly, space and X for up and down, shift for fast, ctrl for slow, - and = for speed. Mouse look is unchanged.", s_state.m_speed);
}

static void Deactivate(bool announce) {
    if (!s_state.m_active) {
        return;
    }

    s_state.m_active = false;
    ClearKeys();
    s_state.m_haveLastTick = false;

    ReleaseCharacter();
    ApplyCharacterVisibility();

    if (announce) {
        Chat::Print("FreeCam: off");
    }
}

bool IsActive() {
    return s_state.m_active;
}

void SetActive(bool active, bool announce) {
    if (active) {
        Activate(announce);
    } else {
        Deactivate(announce);
    }
}

C3Vector GetPosition() {
    return s_state.m_position;
}

void SetPosition(const C3Vector& position) {
    s_state.m_position = position;
    s_state.m_haveLastTick = false;
}

float GetSpeed() {
    return s_state.m_speed;
}

void SetSpeed(float speed) {
    SetSpeed(speed, false);
}

static int32_t Script_SetEnabled(lua_State* L) {
    if (lua_toboolean(L, 1)) {
        Activate(true);
    } else {
        Deactivate(true);
    }

    lua_pushboolean(L, s_state.m_active ? 1 : 0);

    return 1;
}

static int32_t Script_Toggle(lua_State* L) {
    if (s_state.m_active) {
        Deactivate(true);
    } else {
        Activate(true);
    }

    lua_pushboolean(L, s_state.m_active ? 1 : 0);

    return 1;
}

static int32_t Script_IsEnabled(lua_State* L) {
    lua_pushboolean(L, s_state.m_active ? 1 : 0);

    return 1;
}

static int32_t Script_SetSpeed(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        SetSpeed(static_cast<float>(lua_tonumber(L, 1)), false);
    }

    lua_pushnumber(L, s_state.m_speed);

    return 1;
}

static int32_t Script_AdjustSpeed(lua_State* L) {
    float factor = lua_isnumber(L, 1) ? static_cast<float>(lua_tonumber(L, 1)) : kSpeedStep;

    if (factor > 0.0f) {
        SetSpeed(s_state.m_speed * factor, false);
    }

    lua_pushnumber(L, s_state.m_speed);

    return 1;
}

static int32_t Script_SetHideCharacter(lua_State* L) {
    s_state.m_hideCharacter = lua_toboolean(L, 1) != 0;
    ApplyCharacterVisibility();

    return 0;
}

static int32_t Script_SetSyncCharacter(lua_State* L) {
    s_state.m_syncCharacter = lua_toboolean(L, 1) != 0;

    if (!s_state.m_syncCharacter) {
        ReleaseCharacter();
        Chat::Print("FreeCam: character sync off. Tiles away from where you left it will not stream in.");
    }

    return 0;
}

static int32_t Script_SetMaxLag(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        float lag = static_cast<float>(lua_tonumber(L, 1));

        if (lag < 5.0f) {
            lag = 5.0f;
        }

        if (lag > 500.0f) {
            lag = 500.0f;
        }

        s_state.m_maxLag = lag;
    }

    lua_pushnumber(L, s_state.m_maxLag);

    return 1;
}

static int32_t Script_Recenter(lua_State* L) {
    C3Vector position{};

    if (!s_state.m_active || !PlayerPosition(position)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    s_state.m_position = position;
    s_state.m_position.z += 2.0f;
    s_state.m_haveLastTick = false;

    lua_pushboolean(L, 1);

    return 1;
}

static int32_t Script_GetState(lua_State* L) {
    lua_pushboolean(L, s_state.m_active ? 1 : 0);
    lua_pushnumber(L, s_state.m_speed);
    lua_pushboolean(L, s_state.m_hideCharacter ? 1 : 0);
    lua_pushboolean(L, s_state.m_syncCharacter ? 1 : 0);
    lua_pushnumber(L, s_state.m_maxLag);
    lua_pushnumber(L, s_state.m_position.x);
    lua_pushnumber(L, s_state.m_position.y);
    lua_pushnumber(L, s_state.m_position.z);

    return 8;
}

void RegisterScripts() {
    Lua::RegisterFunction("FreeCam_SetEnabled", Script_SetEnabled);
    Lua::RegisterFunction("FreeCam_Toggle", Script_Toggle);
    Lua::RegisterFunction("FreeCam_IsEnabled", Script_IsEnabled);
    Lua::RegisterFunction("FreeCam_SetSpeed", Script_SetSpeed);
    Lua::RegisterFunction("FreeCam_AdjustSpeed", Script_AdjustSpeed);
    Lua::RegisterFunction("FreeCam_SetHideCharacter", Script_SetHideCharacter);
    Lua::RegisterFunction("FreeCam_SetSyncCharacter", Script_SetSyncCharacter);
    Lua::RegisterFunction("FreeCam_SetMaxLag", Script_SetMaxLag);
    Lua::RegisterFunction("FreeCam_Recenter", Script_Recenter);
    Lua::RegisterFunction("FreeCam_GetState", Script_GetState);
}

void OnCameraUpdated(CGCamera* camera) {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    if (camera && frame && camera == frame->m_camera) {
        Tick(camera);
    }
}

void OnGameInitialize() {
    if (s_state.m_eventsRegistered) {
        return;
    }

    EventRegisterEx(EVENT_ID_KEYDOWN, OnKeyDown, nullptr, 10.0f);
    EventRegisterEx(EVENT_ID_KEYUP, OnKeyUp, nullptr, 10.0f);
    EventRegisterEx(EVENT_ID_KEYDOWN_REPEATING, OnKeyRepeat, nullptr, 10.0f);
    s_state.m_eventsRegistered = true;
}

void OnGameDestroy() {
    if (s_state.m_eventsRegistered) {
        EventUnregister(EVENT_ID_KEYDOWN, OnKeyDown);
        EventUnregister(EVENT_ID_KEYUP, OnKeyUp);
        EventUnregister(EVENT_ID_KEYDOWN_REPEATING, OnKeyRepeat);
        s_state.m_eventsRegistered = false;
    }

    s_state.m_active = false;
    s_state.m_characterHidden = false;
    s_state.m_gravityForced = false;
    s_state.m_haveLastTick = false;
    ClearKeys();
}

}
