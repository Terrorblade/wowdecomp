#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleCheckbox.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 36a58313 manual
// OFFSET: 0x00976B40
static int32_t Script_SetChecked(lua_State* L) {
    CSimpleCheckbox* checkbox = static_cast<CSimpleCheckbox*>(FrameScript_GetObjectThis(L, CSimpleCheckbox::GetObjectType()));

    checkbox->SetChecked(StringToBOOL(L, 2, 1), 0);
    return 0;
}

// VERIFY: 414d3494 auto
// OFFSET: 0x00976B90
static int32_t Script_GetChecked(lua_State* L) {
    CSimpleCheckbox* checkbox = static_cast<CSimpleCheckbox*>(FrameScript_GetObjectThis(L, CSimpleCheckbox::GetObjectType()));

    if (checkbox->m_checked) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 185c2417 manual
// OFFSET: 0x00976BF0
static int32_t Script_GetCheckedTexture(lua_State* L) {
    CSimpleCheckbox* checkbox = static_cast<CSimpleCheckbox*>(FrameScript_GetObjectThis(L, CSimpleCheckbox::GetObjectType()));
    CSimpleTexture* texture = checkbox->m_checkedTexture;

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

// VERIFY: 57dbdbc9 auto
// OFFSET: 0x00976C70
static int32_t Script_SetCheckedTexture(lua_State* L) {
    CSimpleCheckbox* checkbox = static_cast<CSimpleCheckbox*>(FrameScript_GetObjectThis(L, CSimpleCheckbox::GetObjectType()));

    if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!texture) {
            const char* name = checkbox->GetName();
            luaL_error(L, "%s:SetCheckedTexture(): Couldn't find 'this' in texture", name ? name : "<unnamed>");
            return 0;
        }

        if (!texture->IsA(CSimpleTexture::GetObjectType()) || texture->m_internal) {
            const char* name = checkbox->GetName();
            luaL_error(L, "%s:SetCheckedTexture(): Wrong object type, expected texture", name ? name : "<unnamed>");
            return 0;
        }

        checkbox->SetCheckedTexture(texture);
        return 0;
    }

    if (lua_isstring(L, 2)) {
        checkbox->SetCheckedTexture(lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (lua_type(L, 2) != LUA_TNIL) {
        const char* name = checkbox->GetName();
        luaL_error(L, "Usage: %s:SetCheckedTexture(texture or \"texture\" or nil)", name ? name : "<unnamed>");
        return 0;
    }

    checkbox->SetCheckedTexture(static_cast<CSimpleTexture*>(nullptr));
    return 0;
}

// VERIFY: 402b6d3b manual
// OFFSET: 0x00976DD0
static int32_t Script_GetDisabledCheckedTexture(lua_State* L) {
    CSimpleCheckbox* checkbox = static_cast<CSimpleCheckbox*>(FrameScript_GetObjectThis(L, CSimpleCheckbox::GetObjectType()));
    CSimpleTexture* texture = checkbox->m_disabledCheckedTexture;

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

// VERIFY: b4744905 auto
// OFFSET: 0x00976E50
static int32_t Script_SetDisabledCheckedTexture(lua_State* L) {
    CSimpleCheckbox* checkbox = static_cast<CSimpleCheckbox*>(FrameScript_GetObjectThis(L, CSimpleCheckbox::GetObjectType()));

    if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!texture) {
            const char* name = checkbox->GetName();
            luaL_error(L, "%s:SetDisabledCheckedTexture(): Couldn't find 'this' in texture", name ? name : "<unnamed>");
            return 0;
        }

        if (!texture->IsA(CSimpleTexture::GetObjectType()) || texture->m_internal) {
            const char* name = checkbox->GetName();
            luaL_error(L, "%s:SetDisabledCheckedTexture(): Wrong object type, expected texture", name ? name : "<unnamed>");
            return 0;
        }

        checkbox->SetDisabledCheckedTexture(texture);
        return 0;
    }

    if (lua_isstring(L, 2)) {
        checkbox->SetDisabledCheckedTexture(lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (lua_type(L, 2) != LUA_TNIL) {
        const char* name = checkbox->GetName();
        luaL_error(L, "Usage: %s:SetDisabledCheckedTexture(texture or \"texture\" or nil)", name ? name : "<unnamed>");
        return 0;
    }

    checkbox->SetDisabledCheckedTexture(static_cast<CSimpleTexture*>(nullptr));
    return 0;
}

// VERIFY: dd86ea1c auto
// OFFSET: 0x00976B10
void CSimpleCheckbox::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleCheckbox::s_metatable);
    CSimpleCheckbox::s_metatable = LUA_NOREF;
}

// VERIFY: 19366438 auto
// OFFSET: 0x00976FB0
void CSimpleCheckbox::CreateScriptMetaTable() {
    CSimpleCheckbox::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleCheckbox::RegisterScriptMethods);
}

// STATIC: 0x00B2D3E4
static FrameScript_Method s_ScriptMethods[] = {
    { "SetChecked", &Script_SetChecked },
    { "GetChecked", &Script_GetChecked },
    { "GetCheckedTexture", &Script_GetCheckedTexture },
    { "SetCheckedTexture", &Script_SetCheckedTexture },
    { "GetDisabledCheckedTexture", &Script_GetDisabledCheckedTexture },
    { "SetDisabledCheckedTexture", &Script_SetDisabledCheckedTexture },
};

// VERIFY: 3cfa1676 auto
// OFFSET: 0x00976AF0
void CSimpleCheckbox::RegisterScriptMethods(lua_State* L) {
    CSimpleButton::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
