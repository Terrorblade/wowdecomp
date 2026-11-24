#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleSlider.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: d9708ab2 manual
// OFFSET: 0x00971A90
static int32_t Script_GetThumbTexture(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));
    CSimpleTexture* texture = slider->m_thumbTexture;

    if (!texture) {
        lua_pushnil(L);
        return 1;
    }

    if (!texture->m_luaRegistered) {
        texture->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, texture->m_luaObjectRef);
    return 1;
}

// VERIFY: 95befe92 auto
// OFFSET: 0x00971B10
static int32_t Script_SetThumbTexture(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!texture) {
            const char* name = slider->GetName();
            luaL_error(L, "%s:SetThumbTexture(): Couldn't find 'this' in texture", name ? name : "<unnamed>");
            return 0;
        }

        if (!texture->IsA(CSimpleTexture::GetObjectType()) || texture->m_internal) {
            const char* name = slider->GetName();
            luaL_error(L, "%s:SetThumbTexture(): Wrong object type, expected texture", name ? name : "<unnamed>");
            return 0;
        }

        slider->SetThumbTexture(texture, 3);
        return 0;
    }

    if (lua_isstring(L, 2)) {
        slider->SetThumbTexture(lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (lua_type(L, 2) != LUA_TNIL) {
        const char* name = slider->GetName();
        luaL_error(L, "Usage: %s:SetThumbTexture(texture or \"texture\" or nil)", name ? name : "<unnamed>");
        return 0;
    }

    slider->SetThumbTexture(nullptr, 3);
    return 0;
}

// VERIFY: 8247eab8 manual
// OFFSET: 0x00971C70
static int32_t Script_GetOrientation(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    lua_pushstring(L, OrientationToString(slider->m_orientation));
    return 1;
}

// VERIFY: 09ce52e9 auto
// OFFSET: 0x00971CC0
static int32_t Script_SetOrientation(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    if (lua_isstring(L, 2)) {
        ORIENTATION orientation;

        if (StringToOrientation(lua_tolstring(L, 2, nullptr), orientation)) {
            slider->SetOrientation(orientation);
            return 0;
        }

        const char* name = slider->GetName();
        luaL_error(L, "%s:SetOrientation(): Unknown orientation: %s", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    const char* name = slider->GetName();
    luaL_error(L, "Usage: %s:SetOrientation(\"orientation\")", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 5045ea87 manual
// OFFSET: 0x00971D90
static int32_t Script_GetMinMaxValues(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    lua_pushnumber(L, slider->m_baseValue);
    lua_pushnumber(L, slider->m_range + slider->m_baseValue);
    return 2;
}

// VERIFY: 729a0b22 auto
// OFFSET: 0x00971DF0
static int32_t Script_SetMinMaxValues(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));
    float minValue;
    double maxValue;

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3) || (minValue = static_cast<float>(lua_tonumber(L, 2)), maxValue = lua_tonumber(L, 3), minValue > maxValue)) {
        const char* name = slider->GetName();
        luaL_error(L, "Usage: %s:SetMinMaxValues(min, max)", name ? name : "<unnamed>");
        return 0;
    }

    slider->SetMinMaxValues(minValue, static_cast<float>(maxValue));
    return 0;
}

// VERIFY: 04a380cd manual
// OFFSET: 0x00971EB0
static int32_t Script_GetValue(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    lua_pushnumber(L, slider->m_value);
    return 1;
}

// VERIFY: 4675b583 auto
// OFFSET: 0x00971F00
static int32_t Script_SetValue(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    if (lua_isnumber(L, 2)) {
        slider->SetValue(static_cast<float>(lua_tonumber(L, 2)));
        return 0;
    }

    const char* name = slider->GetName();
    luaL_error(L, "Usage: %s:SetValue(value)", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 32981250 manual
// OFFSET: 0x00971F80
static int32_t Script_GetValueStep(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    lua_pushnumber(L, slider->m_valueStep);
    return 1;
}

// VERIFY: c852be13 auto
// OFFSET: 0x00971FD0
static int32_t Script_SetValueStep(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    if (lua_isnumber(L, 2)) {
        slider->SetValueStep(static_cast<float>(lua_tonumber(L, 2)));
        return 0;
    }

    const char* name = slider->GetName();
    luaL_error(L, "Usage: %s:SetValueStep(value)", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 94648508 manual
// OFFSET: 0x00972050
static int32_t Script_IsEnabled(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    if (slider->m_frameFlags & CSimpleFrame::FRAME_DISABLED) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, 1.0);
    }

    return 1;
}

// VERIFY: 486a0986 auto
// OFFSET: 0x009720E0
static int32_t Script_Enable(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    slider->SetFrameFlag(CSimpleFrame::FRAME_DISABLED, 0);

    if (slider->m_onEnable.m_luaRef && !slider->m_isLoading) {
        slider->RunScript(slider->m_onEnable, 0, nullptr);
    }

    return 0;
}

// VERIFY: 3661396e auto
// OFFSET: 0x00972150
static int32_t Script_Disable(lua_State* L) {
    CSimpleSlider* slider = static_cast<CSimpleSlider*>(FrameScript_GetObjectThis(L, CSimpleSlider::GetObjectType()));

    slider->SetFrameFlag(CSimpleFrame::FRAME_DISABLED, 1);

    if (slider->m_onDisable.m_luaRef && !slider->m_isLoading) {
        slider->RunScript(slider->m_onDisable, 0, nullptr);
    }

    return 0;
}

// VERIFY: e4dec925 auto
// OFFSET: 0x00971A60
void CSimpleSlider::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleSlider::s_metatable);
    CSimpleSlider::s_metatable = LUA_NOREF;
}

// VERIFY: b274e4b5 auto
// OFFSET: 0x009720C0
void CSimpleSlider::CreateScriptMetaTable() {
    CSimpleSlider::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleSlider::RegisterScriptMethods);
}

// STATIC: 0x00B2CE20
static FrameScript_Method s_ScriptMethods[] = {
    { "GetThumbTexture", &Script_GetThumbTexture },
    { "SetThumbTexture", &Script_SetThumbTexture },
    { "GetOrientation", &Script_GetOrientation },
    { "SetOrientation", &Script_SetOrientation },
    { "GetMinMaxValues", &Script_GetMinMaxValues },
    { "SetMinMaxValues", &Script_SetMinMaxValues },
    { "GetValue", &Script_GetValue },
    { "SetValue", &Script_SetValue },
    { "GetValueStep", &Script_GetValueStep },
    { "SetValueStep", &Script_SetValueStep },
    { "Enable", &Script_Enable },
    { "Disable", &Script_Disable },
    { "IsEnabled", &Script_IsEnabled },
};

// VERIFY: fb907021 auto
// OFFSET: 0x00971A40
void CSimpleSlider::RegisterScriptMethods(lua_State* L) {
    CSimpleFrame::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
