#include "HoT/Editor/Mode.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <ctime>

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
#include <cstring>
#include <string>
#include <vector>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleEditBox.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Chat.h"
#include "HoT/Core/Log.h"
#include "HoT/Editor/Editor.h"
#include "HoT/Editor/FreeCam.h"
#include "HoT/Editor/Selection.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/Glue/CWOWClientStatus.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/UIBindings.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Editor::Mode {

static const char* const kToc = "Interface\\GameEditor\\GameEditor.toc";

// Game bindings that keep working while the editor owns the keyboard.
static const char* const s_passThroughCommands[] = {
    "OPENCHAT",
    "OPENCHATSLASH",
    "REPLY",
    "REPLY2",
    "SCREENSHOT",
    "TOGGLESOUND",
    "MASTERVOLUMEUP",
    "MASTERVOLUMEDOWN",
};

struct ModeState {
    bool m_active = false;
    bool m_uiLoaded = false;
    bool m_startedFreeCam = false;
    bool m_useFreeCam = true;
    bool m_hidUIParent = false;
    bool m_eventsRegistered = false;

    std::string m_tool = "select";
    std::string m_toggleKey = "F12";

    std::vector<Handler*> m_handlers;

    uint32_t m_buttonsDown = 0;
    uint32_t m_buttonsHeld = 0;
    Handler* m_buttonOwner[16] = {};

    bool m_haveLastTick = false;
    LARGE_INTEGER m_lastTick{};
};

static ModeState s_state;

static bool SameKey(const char* a, const char* b) {
    return a && b && SStrCmpI(a, b, STORM_MAX_STR) == 0;
}

static uint32_t ButtonIndex(MOUSEBUTTON button) {
    for (uint32_t i = 0; i < 16; i++) {
        if (static_cast<uint32_t>(button) == (1u << i)) {
            return i;
        }
    }

    return 15;
}

static Handler* ToolOwner() {
    for (Handler* handler : s_state.m_handlers) {
        if (handler->OwnsTool(s_state.m_tool.c_str())) {
            return handler;
        }
    }

    return nullptr;
}

static bool CallLuaBool(const char* function, const char* argument) {
    lua_State* L = FrameScript_GetContext();

    if (!L) {
        return false;
    }

    int32_t top = lua_gettop(L);
    lua_getfield(L, LUA_GLOBALSINDEX, function);

    if (lua_type(L, -1) != LUA_TFUNCTION) {
        lua_settop(L, top);
        return false;
    }

    lua_pushstring(L, argument);

    bool result = false;

    if (lua_pcall(L, 1, 1, 0) == 0) {
        result = lua_toboolean(L, -1) != 0;
    } else {
        LOG_INFO << "game editor: " << function << " failed: " << (lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
    }

    lua_settop(L, top);

    return result;
}

static bool LoadUI() {
    if (s_state.m_uiLoaded) {
        return true;
    }

    if (!SFile::FileExistsEx(kToc, 1)) {
        Chat::Print("Game editor: %s is missing", kToc);
        return false;
    }

    OsCreateDirectory("Logs", 0);
    CWOWClientStatus status("Logs\\GameEditor.log");

    // Not part of the signed interface, nothing checks this digest.
    MD5_CTX md5;
    MD5Init(&md5);
    FrameXML_CreateFrames(kToc, nullptr, &md5, &status);

    if (!CScriptObject::GetScriptObjectByName("EditorParent", CSimpleFrame::GetObjectType())) {
        Chat::Print("Game editor: the UI failed to load, see Logs\\GameEditor.log");
        return false;
    }

    s_state.m_uiLoaded = true;

    return true;
}

static void ReleaseButtons(float x, float y) {
    for (uint32_t i = 0; i < 16; i++) {
        if (!(s_state.m_buttonsDown & (1u << i))) {
            continue;
        }

        if (Handler* owner = s_state.m_buttonOwner[i]) {
            owner->OnMouseUp(static_cast<MOUSEBUTTON>(1u << i), 0, x, y);
        }

        s_state.m_buttonOwner[i] = nullptr;
    }

    s_state.m_buttonsDown = 0;
}

static void SetUIParentShown(bool shown) {
    CSimpleFrame* parent = CGGameUI::m_UISimpleParent;

    if (!parent) {
        return;
    }

    if (!shown) {
        if (parent->m_isShown) {
            parent->Hide();
            s_state.m_hidUIParent = true;
        }

        return;
    }

    if (s_state.m_hidUIParent) {
        parent->Show();
        s_state.m_hidUIParent = false;
    }
}

void RegisterHandler(Handler* handler) {
    for (Handler* existing : s_state.m_handlers) {
        if (existing == handler) {
            return;
        }
    }

    s_state.m_handlers.push_back(handler);
}

bool IsActive() {
    return s_state.m_active;
}

void SetActive(bool active) {
    if (active == s_state.m_active) {
        return;
    }

    if (active) {
        if (!Session::HasClearance(3)) {
            return;
        }

        if (!ClntObjMgrGetActivePlayerObj() || !CGWorldFrame::s_currentWorldFrame) {
            Chat::Print("Game editor: enter the world first");
            return;
        }

        if (!LoadUI()) {
            return;
        }

        s_state.m_active = true;
        s_state.m_haveLastTick = false;

        SetUIParentShown(false);

        s_state.m_startedFreeCam = s_state.m_useFreeCam && !FreeCam::IsActive();

        if (s_state.m_startedFreeCam) {
            FreeCam::SetActive(true, false);
        }

        for (Handler* handler : s_state.m_handlers) {
            handler->OnActivate();
        }

        for (Handler* handler : s_state.m_handlers) {
            handler->OnToolChanged(s_state.m_tool.c_str());
        }

        Lua::SignalEvent("GAME_EDITOR_MODE_CHANGED", "%d", 1);

        return;
    }

    ReleaseButtons(0.0f, 0.0f);

    for (Handler* handler : s_state.m_handlers) {
        handler->OnDeactivate();
    }

    s_state.m_active = false;

    if (s_state.m_startedFreeCam) {
        FreeCam::SetActive(false, false);
        s_state.m_startedFreeCam = false;
    }

    SetUIParentShown(true);

    Lua::SignalEvent("GAME_EDITOR_MODE_CHANGED", "%d", 0);
}

const char* ActiveTool() {
    return s_state.m_tool.c_str();
}

bool ToolIs(const char* name) {
    return name && s_state.m_tool == name;
}

bool ToolHasPrefix(const char* prefix) {
    return prefix && s_state.m_tool.compare(0, strlen(prefix), prefix) == 0;
}

static void SetTool(const char* tool) {
    if (!tool || s_state.m_tool == tool) {
        return;
    }

    ReleaseButtons(0.0f, 0.0f);
    s_state.m_tool = tool;

    for (Handler* handler : s_state.m_handlers) {
        handler->OnToolChanged(tool);
    }
}

void GetMouse(float& x, float& y) {
    x = 0.0f;
    y = 0.0f;

    CSimpleTop* top = CGGameUI::m_simpleTop;

    if (top) {
        NDCToDDC(top->m_mousePosition.m_x, top->m_mousePosition.m_y, &x, &y);
    }
}

bool IsMouseOverWorld() {
    CSimpleTop* top = CGGameUI::m_simpleTop;
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    return top && frame && top->m_mouseFocus == frame;
}

bool IsMouseButtonDown(MOUSEBUTTON button) {
    return (s_state.m_buttonsDown & static_cast<uint32_t>(button)) != 0;
}

bool IsMouseButtonHeld(MOUSEBUTTON button) {
    return (s_state.m_buttonsHeld & static_cast<uint32_t>(button)) != 0;
}

bool GetMouseRay(C3Vector& start, C3Vector& end) {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    if (!frame || !frame->m_camera) {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    GetMouse(x, y);
    MouseToWorld(frame, x, y, &start, &end);

    return start.x != end.x || start.y != end.y || start.z != end.z;
}

static bool IsPassThrough(const char* key) {
    if (!CGUIBindings::s_bindings) {
        return false;
    }

    const char* command = CGUIBindings::s_bindings->GetCommandAction(static_cast<BINDING_MODE>(4), key, 1);

    if (!command) {
        return false;
    }

    for (const char* allowed : s_passThroughCommands) {
        if (SameKey(command, allowed)) {
            return true;
        }
    }

    return false;
}

bool OnWorldKey(const char* key, uint32_t metaKeys, bool down) {
    if (SameKey(key, s_state.m_toggleKey.c_str()) && (s_state.m_active || Session::HasClearance(3))) {
        if (down) {
            SetActive(!s_state.m_active);
        }

        return true;
    }

    if (!s_state.m_active) {
        return false;
    }

    if (Handler* owner = ToolOwner()) {
        if (owner->OnKey(key, down)) {
            return true;
        }
    }

    if (down && CallLuaBool("GameEditor_OnHotkey", key)) {
        return true;
    }

    // Without the free camera the character walks and the game bindings stay live, the game menu stays shut.
    if (!FreeCam::IsActive()) {
        return SameKey(key, "ESCAPE");
    }

    return !IsPassThrough(key);
}

// Tools that do not work on creatures or gameobjects still let a click select one, terrain brushes keep their clicks.
static bool ClickSelectsObjects() {
    if (s_state.m_tool.empty() || s_state.m_tool == "map.object") {
        return true;
    }

    if (ToolHasPrefix("map.")) {
        return false;
    }

    return !ToolIs("select") && !ToolIs("move") && !ToolIs("rotate") && !ToolIs("waypoint");
}

bool OnWorldMouse(MOUSEBUTTON button, uint32_t metaKeys, float ndcX, float ndcY, bool down) {
    if (!s_state.m_active) {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    NDCToDDC(ndcX, ndcY, &x, &y);

    if (down && CSimpleEditBox::s_currentFocus) {
        CSimpleEditBox::ClearKeyboardFocus(CSimpleEditBox::s_currentFocus, true);
    }

    // Right mouse looks around like the default left button, without interacting or attacking.
    if (button == MOUSE_BUTTON_RIGHT) {
        if (CGUIBindings::s_bindings) {
            CGUIBindings::s_bindings->ExecKey(metaKeys, "BUTTON1", down ? 1 : 0, 1, static_cast<BINDING_MODE>(4));
        }

        return true;
    }

    // Mouse-up is delivered from the raw event so releases over the UI still end drags.
    if (!down) {
        return true;
    }

    if (button == MOUSE_BUTTON_LEFT && ClickSelectsObjects() && Selection::ClickSelect(x, y)) {
        return true;
    }

    uint32_t index = ButtonIndex(button);
    Handler* owner = ToolOwner();

    s_state.m_buttonsDown |= static_cast<uint32_t>(button);
    s_state.m_buttonOwner[index] = owner;

    if (owner) {
        owner->OnMouseDown(button, metaKeys, x, y);
    }

    return true;
}

bool OnWorldMouseWheel(int32_t delta, uint32_t metaKeys) {
    if (!s_state.m_active) {
        return false;
    }

    if (Handler* owner = ToolOwner()) {
        if (owner->OnMouseWheel(delta, metaKeys)) {
            return true;
        }
    }

    if (!FreeCam::IsActive()) {
        return false;
    }

    float speed = FreeCam::GetSpeed();
    FreeCam::SetSpeed(delta > 0 ? speed * 1.25f : speed / 1.25f);

    return true;
}

static int32_t OnRawMouseDown(const void* data, void* param) {
    if (data) {
        s_state.m_buttonsHeld |= static_cast<uint32_t>(static_cast<const EVENT_DATA_MOUSE*>(data)->m_button);
    }

    return 1;
}

static int32_t OnRawMouseUp(const void* data, void* param) {
    if (!data) {
        return 1;
    }

    const EVENT_DATA_MOUSE* mouse = static_cast<const EVENT_DATA_MOUSE*>(data);
    uint32_t bit = static_cast<uint32_t>(mouse->m_button);
    s_state.m_buttonsHeld &= ~bit;

    if (!(s_state.m_buttonsDown & bit)) {
        return 1;
    }

    s_state.m_buttonsDown &= ~bit;

    uint32_t index = ButtonIndex(mouse->m_button);
    Handler* owner = s_state.m_buttonOwner[index];
    s_state.m_buttonOwner[index] = nullptr;

    if (owner && s_state.m_active) {
        float x = 0.0f;
        float y = 0.0f;
        NDCToDDC(mouse->m_x, mouse->m_y, &x, &y);
        owner->OnMouseUp(mouse->m_button, mouse->m_metaKeyState, x, y);
    }

    return 1;
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

    return seconds < 0.0f ? 0.0f : (seconds > 0.25f ? 0.25f : seconds);
}

void OnDecal(C3Vector& position, float& size, int32_t& style) {
    if (!s_state.m_active) {
        return;
    }

    for (Handler* handler : s_state.m_handlers) {
        handler->OnDecal(position, size, style);
    }
}

void OnWorldRender(CGWorldFrame* frame) {
    if (!s_state.m_active) {
        return;
    }

    float seconds = StepSeconds();

    for (Handler* handler : s_state.m_handlers) {
        handler->OnUpdate(seconds);
    }

    for (Handler* handler : s_state.m_handlers) {
        handler->OnWorldRender(frame);
    }
}

static void OnToggleKeyChanged(const char* value) {
    s_state.m_toggleKey = value && *value ? value : "F12";
}

static int32_t Script_IsActive(lua_State* L) {
    lua_pushboolean(L, s_state.m_active ? 1 : 0);
    return 1;
}

static int32_t Script_SetActive(lua_State* L) {
    SetActive(lua_toboolean(L, 1) != 0);
    lua_pushboolean(L, s_state.m_active ? 1 : 0);
    return 1;
}

static int32_t Script_Toggle(lua_State* L) {
    SetActive(!s_state.m_active);
    lua_pushboolean(L, s_state.m_active ? 1 : 0);
    return 1;
}

static int32_t Script_SetTool(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    if (lua_isstring(L, 1)) {
        SetTool(lua_tostring(L, 1));
    }

    return 0;
}

static int32_t Script_GetTool(lua_State* L) {
    lua_pushstring(L, s_state.m_tool.c_str());
    return 1;
}

static int32_t Script_GetToggleKey(lua_State* L) {
    lua_pushstring(L, s_state.m_toggleKey.c_str());
    return 1;
}

static int32_t Script_IsMouseOverWorld(lua_State* L) {
    lua_pushboolean(L, IsMouseOverWorld() ? 1 : 0);
    return 1;
}

static void OnFreeCamChanged(const char* value) {
    s_state.m_useFreeCam = HoT::CVars::Bool(value);

    if (!s_state.m_active) {
        return;
    }

    if (s_state.m_useFreeCam && !FreeCam::IsActive()) {
        FreeCam::SetActive(true, false);
        s_state.m_startedFreeCam = true;
    } else if (!s_state.m_useFreeCam && FreeCam::IsActive()) {
        FreeCam::SetActive(false, false);
        s_state.m_startedFreeCam = false;
    }
}

void RegisterScripts() {
    HoT::CVars::Register("editorToggleKey", "Key that opens and closes the game editor", "F12", OnToggleKeyChanged);
    HoT::CVars::Register("editorFreeCam", "Use the free camera while the game editor is open, 0 keeps the normal camera and movement", "1", OnFreeCamChanged);
    HoT::CVars::Register("editorUIScale", "Scale of the game editor UI, 1 is one pixel per unit", "1");

    Lua::RegisterFunction("GameEditor_IsActive", Script_IsActive);
    Lua::RegisterFunction("GameEditor_SetActive", Script_SetActive);
    Lua::RegisterFunction("GameEditor_Toggle", Script_Toggle);
    Lua::RegisterFunction("GameEditor_SetTool", Script_SetTool);
    Lua::RegisterFunction("GameEditor_GetTool", Script_GetTool);
    Lua::RegisterFunction("GameEditor_GetToggleKey", Script_GetToggleKey);
    Lua::RegisterFunction("GameEditor_IsMouseOverWorld", Script_IsMouseOverWorld);
}

void OnGameInitialize() {
    if (!s_state.m_eventsRegistered) {
        EventRegisterEx(EVENT_ID_MOUSEDOWN, OnRawMouseDown, nullptr, 10.0f);
        EventRegisterEx(EVENT_ID_MOUSEUP, OnRawMouseUp, nullptr, 10.0f);
        s_state.m_eventsRegistered = true;
    }
}

void OnWorldUnloaded() {
    SetActive(false);

    for (Handler* handler : s_state.m_handlers) {
        handler->OnWorldUnloaded();
    }
}

void OnScriptsReloaded() {
    bool wasActive = s_state.m_active;
    s_state.m_active = false;
    s_state.m_uiLoaded = false;
    s_state.m_hidUIParent = false;
    s_state.m_buttonsDown = 0;
    s_state.m_buttonsHeld = 0;

    for (Handler*& owner : s_state.m_buttonOwner) {
        owner = nullptr;
    }

    if (s_state.m_startedFreeCam) {
        FreeCam::SetActive(false, false);
        s_state.m_startedFreeCam = false;
    }

    if (!wasActive) {
        return;
    }

    for (Handler* handler : s_state.m_handlers) {
        handler->OnDeactivate();
    }
}

void OnGameDestroy() {
    OnWorldUnloaded();

    if (s_state.m_eventsRegistered) {
        EventUnregister(EVENT_ID_MOUSEDOWN, OnRawMouseDown);
        EventUnregister(EVENT_ID_MOUSEUP, OnRawMouseUp);
        s_state.m_eventsRegistered = false;
    }

    s_state.m_uiLoaded = false;
}

}
