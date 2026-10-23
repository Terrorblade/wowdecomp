#include "Controller/Frames.h"
#include <unordered_map>
#include <vector>
#include "Controller/Buttons.h"
#include "Controller/Devices.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace Controller::Frames {

struct FrameData {
    bool m_buttonEnabled = false;
    bool m_stickEnabled = false;
    FrameScript_Object::ScriptIx m_onButtonDown;
    FrameScript_Object::ScriptIx m_onButtonUp;
    FrameScript_Object::ScriptIx m_onStick;
};

static std::unordered_map<CSimpleFrame*, FrameData> s_frames;
static CSimpleFrame* s_capture[BUTTON_COUNT];
static StickState s_lastSticks[STICK_PHYSICAL_COUNT];

static FrameData* Data(CSimpleFrame* frame) {
    auto it = s_frames.find(frame);
    return it == s_frames.end() ? nullptr : &it->second;
}

FrameScript_Object::ScriptIx* GetScript(CSimpleFrame* frame, const char* name, FrameScript_Object::ScriptData& data) {
    if (!SStrCmpI(name, "OnGamePadButtonDown", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,button) %s end";
        return &s_frames[frame].m_onButtonDown;
    }

    if (!SStrCmpI(name, "OnGamePadButtonUp", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,button) %s end";
        return &s_frames[frame].m_onButtonUp;
    }

    if (!SStrCmpI(name, "OnGamePadStick", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,stick,x,y,len) %s end";
        return &s_frames[frame].m_onStick;
    }

    return nullptr;
}

void OnDestroyed(CSimpleFrame* frame) {
    s_frames.erase(frame);

    for (CSimpleFrame*& capture : s_capture) {
        if (capture == frame) {
            capture = nullptr;
        }
    }
}

static void UpdateRegistration(CSimpleFrame* frame, const FrameData& data) {
    if (data.m_buttonEnabled || data.m_stickEnabled) {
        frame->EnableEvent(SIMPLE_EVENT_JOYSTICK, -1);
    } else {
        frame->DisableEvent(SIMPLE_EVENT_JOYSTICK);
    }
}

static CSimpleFrame* GetFrame(lua_State* L) {
    return static_cast<CSimpleFrame*>(FrameScript_GetObjectThis(L, CSimpleFrame::GetObjectType()));
}

static int32_t Script_EnableGamePadButton(lua_State* L) {
    CSimpleFrame* frame = GetFrame(L);
    FrameData& data = s_frames[frame];
    data.m_buttonEnabled = StringToBOOL(L, 2, 1) != 0;
    UpdateRegistration(frame, data);
    return 0;
}

static int32_t Script_IsGamePadButtonEnabled(lua_State* L) {
    FrameData* data = Data(GetFrame(L));
    lua_pushboolean(L, data && data->m_buttonEnabled);
    return 1;
}

static int32_t Script_EnableGamePadStick(lua_State* L) {
    CSimpleFrame* frame = GetFrame(L);
    FrameData& data = s_frames[frame];
    data.m_stickEnabled = StringToBOOL(L, 2, 1) != 0;
    UpdateRegistration(frame, data);
    return 0;
}

static int32_t Script_IsGamePadStickEnabled(lua_State* L) {
    FrameData* data = Data(GetFrame(L));
    lua_pushboolean(L, data && data->m_stickEnabled);
    return 1;
}

static FrameScript_Method s_methods[] = {
    { "EnableGamePadButton", &Script_EnableGamePadButton },
    { "IsGamePadButtonEnabled", &Script_IsGamePadButtonEnabled },
    { "EnableGamePadStick", &Script_EnableGamePadStick },
    { "IsGamePadStickEnabled", &Script_IsGamePadStickEnabled },
};

void RegisterMethods(lua_State* L) {
    FrameScript_Object::FillScriptMethodTable(L, s_methods, sizeof(s_methods) / sizeof(s_methods[0]));
}

// Visible frames in the gamepad event queue, top strata first, in the order CSimpleTop delivers keys.
template <typename Predicate>
static void Collect(std::vector<CSimpleFrame*>& frames, Predicate predicate) {
    CSimpleTop* top = CSimpleTop::s_instance;

    if (!top) {
        return;
    }

    for (int32_t strata = FRAME_STRATA_TOOLTIP; strata >= 0; strata--) {
        CSimpleSortedArray<FRAMEPRIORITY*>& queue = top->m_eventqueue[SIMPLE_EVENT_JOYSTICK][strata];

        for (uint32_t i = 0; i < queue.m_count; i++) {
            FRAMEPRIORITY* priority = queue.m_array[i];
            CSimpleFrame* frame = priority ? priority->m_frame : nullptr;
            FrameData* data = frame && frame->m_isVisible ? Data(frame) : nullptr;

            if (data && predicate(*data)) {
                frames.push_back(frame);
            }
        }
    }
}

bool ButtonDown(int32_t button) {
    if (button < 0 || button >= BUTTON_COUNT) {
        return false;
    }

    std::vector<CSimpleFrame*> frames;
    Collect(frames, [](const FrameData& data) { return data.m_buttonEnabled; });

    if (frames.empty()) {
        return false;
    }

    CSimpleFrame* frame = frames.front();
    s_capture[button] = frame;

    const FrameScript_Object::ScriptIx& script = s_frames[frame].m_onButtonDown;

    if (script.m_luaRef) {
        lua_pushstring(FrameScript_GetContext(), ButtonBinding(button));
        frame->RunScript(script, 1, nullptr);
    }

    return true;
}

bool ButtonUp(int32_t button) {
    if (button < 0 || button >= BUTTON_COUNT || !s_capture[button]) {
        return false;
    }

    CSimpleFrame* frame = s_capture[button];
    s_capture[button] = nullptr;

    FrameData* data = Data(frame);

    if (data && data->m_onButtonUp.m_luaRef && frame->m_isVisible) {
        lua_pushstring(FrameScript_GetContext(), ButtonBinding(button));
        frame->RunScript(data->m_onButtonUp, 1, nullptr);
    }

    return true;
}

bool DispatchSticks(const MappedState& state) {
    std::vector<CSimpleFrame*> frames;
    Collect(frames, [](const FrameData& data) { return data.m_stickEnabled; });

    if (frames.empty()) {
        for (StickState& last : s_lastSticks) {
            last = StickState();
        }

        return false;
    }

    for (int32_t stick = 0; stick < STICK_PHYSICAL_COUNT; stick++) {
        const StickState& current = state.m_sticks[stick];
        StickState& last = s_lastSticks[stick];

        if (current.m_x == last.m_x && current.m_y == last.m_y) {
            continue;
        }

        last = current;

        for (CSimpleFrame* frame : frames) {
            FrameData* data = Data(frame);

            if (!data || !data->m_onStick.m_luaRef || !frame->m_isVisible) {
                continue;
            }

            lua_State* L = FrameScript_GetContext();
            lua_pushstring(L, StickConfigName(stick));
            lua_pushnumber(L, current.m_x);
            lua_pushnumber(L, current.m_y);
            lua_pushnumber(L, current.m_len);
            frame->RunScript(data->m_onStick, 4, nullptr);
        }
    }

    return true;
}

}
