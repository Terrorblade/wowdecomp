#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleStatusBar.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: bbf92432 auto
// OFFSET: 0x00971A20
void CSimpleStatusBar::CreateScriptMetaTable() {
    CSimpleStatusBar::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleStatusBar::RegisterScriptMethods);
}

// VERIFY: d47cea3b auto
// OFFSET: 0x009711D0
void CSimpleStatusBar::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleStatusBar::s_metatable);
    CSimpleStatusBar::s_metatable = LUA_NOREF;
}

// VERIFY: e80dba42 auto
// OFFSET: 0x00971200
void CSimpleStatusBar::GetStatusBarColor(CImVector& color) {
    if (this->m_barTexture) {
        this->m_barTexture->GetVertexColor(color);
    } else {
        color.Set(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

// VERIFY: 568374ea manual
// OFFSET: 0x00971240
static int32_t Script_GetOrientation(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));

    lua_pushstring(L, OrientationToString(statusBar->m_orientation));
    return 1;
}

// VERIFY: 11d8df2f auto
// OFFSET: 0x00971290
static int32_t Script_SetOrientation(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));

    if (lua_isstring(L, 2)) {
        ORIENTATION orientation;

        if (StringToOrientation(lua_tolstring(L, 2, nullptr), orientation)) {
            statusBar->SetOrientation(orientation);
            return 0;
        }

        const char* name = statusBar->GetName();
        luaL_error(L, "%s:SetOrientation(): Unknown orientation: %s", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    const char* name = statusBar->GetName();
    luaL_error(L, "Usage: %s:SetOrientation(\"orientation\")", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 4c6a8df5 manual
// OFFSET: 0x00971360
static int32_t Script_GetMinMaxValues(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));

    lua_pushnumber(L, statusBar->m_minValue);
    lua_pushnumber(L, statusBar->m_maxValue);
    return 2;
}

// VERIFY: b0ef1fa1 auto
// OFFSET: 0x009713C0
static int32_t Script_SetMinMaxValues(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = statusBar->GetName();
        luaL_error(L, "Usage: %s:SetMinMaxValues(min, max)", name ? name : "<unnamed>");
        return 0;
    }

    float minValue = lua_tonumber(L, 2);
    float maxValue = lua_tonumber(L, 3);

    if (minValue < -1.0e12 || minValue > 1.0e12 || maxValue < -1.0e12 || maxValue > 1.0e12) {
        luaL_error(L, "Min or Max out of range");
        return 0;
    }

    if (maxValue - minValue > 1.0e12) {
        luaL_error(L, "Min and Max too far apart");
        return 0;
    }

    statusBar->SetMinMaxValues(minValue, maxValue);
    return 0;
}

// VERIFY: b2934178 manual
// OFFSET: 0x009714F0
static int32_t Script_GetValue(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));

    lua_pushnumber(L, statusBar->m_value);
    return 1;
}

// VERIFY: 1d4b150d auto
// OFFSET: 0x00971540
static int32_t Script_SetValue(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));

    if (lua_isnumber(L, 2)) {
        statusBar->SetValue(lua_tonumber(L, 2));
        return 0;
    }

    const char* name = statusBar->GetName();
    luaL_error(L, "Usage: %s:SetValue(value)", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 707dc1f0 manual
// OFFSET: 0x009715D0
static int32_t Script_GetStatusBarTexture(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));
    CSimpleTexture* texture = statusBar->m_barTexture;

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

// VERIFY: b8a2b959 auto
// OFFSET: 0x00971650
static int32_t Script_SetStatusBarTexture(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));
    uint32_t drawLayer = 2;

    if (lua_isstring(L, 3)) {
        StringToDrawLayer(lua_tolstring(L, 3, nullptr), drawLayer);
    }

    if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!texture) {
            const char* name = statusBar->GetName();
            luaL_error(L, "%s:SetCheckedTexture(): Couldn't find 'this' in texture", name ? name : "<unnamed>");
            return 0;
        }

        if (!texture->IsA(CSimpleTexture::GetObjectType()) || texture->m_internal) {
            const char* name = statusBar->GetName();
            luaL_error(L, "%s:SetCheckedTexture(): Wrong object type, expected texture", name ? name : "<unnamed>");
            return 0;
        }

        statusBar->SetBarTexture(texture, drawLayer);
        return 0;
    }

    if (lua_isstring(L, 2)) {
        statusBar->SetBarTexture(lua_tolstring(L, 2, nullptr), drawLayer);
        return 0;
    }

    if (lua_type(L, 2) != LUA_TNIL) {
        const char* name = statusBar->GetName();
        luaL_error(L, "Usage: %s:SetStatusBarTexture(texture or \"texture\" or nil [, \"layer\"])", name ? name : "<unnamed>");
        return 0;
    }

    statusBar->SetBarTexture(static_cast<CSimpleTexture*>(nullptr), 2);
    return 0;
}

// VERIFY: 868d0054 manual
// OFFSET: 0x00971800
static int32_t Script_GetStatusBarColor(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));
    CImVector color;

    statusBar->GetStatusBarColor(color);
    lua_pushnumber(L, color.r * 0.00392156f);
    lua_pushnumber(L, color.g * 0.00392156f);
    lua_pushnumber(L, color.b * 0.00392156f);
    lua_pushnumber(L, color.a * 0.00392156f);
    return 4;
}

// VERIFY: 1075e690 manual
// OFFSET: 0x00971910
static int32_t Script_SetStatusBarColor(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));
    CImVector color;

    FrameScript_GetColor(L, 2, color);
    statusBar->SetStatusBarColor(color);
    return 0;
}

// VERIFY: 46f71f60 auto
// OFFSET: 0x00971970
static int32_t Script_GetRotatesTexture(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));

    if (statusBar->m_rotatesTexture) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: d044810d manual
// OFFSET: 0x009719D0
static int32_t Script_SetRotatesTexture(lua_State* L) {
    CSimpleStatusBar* statusBar = static_cast<CSimpleStatusBar*>(FrameScript_GetObjectThis(L, CSimpleStatusBar::GetObjectType()));

    statusBar->SetRotatesTexture(StringToBOOL(L, 2, 0));
    return 0;
}

// STATIC: 0x00B2CDB8
static FrameScript_Method s_ScriptMethods[] = {
    { "GetOrientation", &Script_GetOrientation },
    { "SetOrientation", &Script_SetOrientation },
    { "GetMinMaxValues", &Script_GetMinMaxValues },
    { "SetMinMaxValues", &Script_SetMinMaxValues },
    { "GetValue", &Script_GetValue },
    { "SetValue", &Script_SetValue },
    { "GetStatusBarTexture", &Script_GetStatusBarTexture },
    { "SetStatusBarTexture", &Script_SetStatusBarTexture },
    { "GetStatusBarColor", &Script_GetStatusBarColor },
    { "SetStatusBarColor", &Script_SetStatusBarColor },
    { "GetRotatesTexture", &Script_GetRotatesTexture },
    { "SetRotatesTexture", &Script_SetRotatesTexture },
};

// VERIFY: 405c2e64 auto
// OFFSET: 0x009711B0
void CSimpleStatusBar::RegisterScriptMethods(lua_State* L) {
    CSimpleFrame::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
