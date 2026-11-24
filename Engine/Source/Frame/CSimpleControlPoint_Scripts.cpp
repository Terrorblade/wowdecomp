#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleControlPoint.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CSimplePathAnim.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 0a8264fc auto
// OFFSET: 0x004A6790
static int32_t Script_SetParent(lua_State* L) {
    CSimpleControlPoint* point = static_cast<CSimpleControlPoint*>(FrameScript_GetObjectThis(L, CSimpleControlPoint::GetObjectType()));

    if (lua_type(L, 2) == LUA_TNIL) {
        const char* name = point->GetName();
        luaL_error(L, "%s:SetParent(): Cannot set a 'nil' parent for control points", name ? name : "<unnamed>");
        return 0;
    }

    CSimplePathAnim* path = nullptr;

    if (lua_isstring(L, 2)) {
        path = static_cast<CSimplePathAnim*>(CScriptObject::GetScriptObjectByName(lua_tolstring(L, 2, nullptr), CSimplePathAnim::GetObjectType()));
    } else if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        path = static_cast<CSimplePathAnim*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!path) {
            const char* name = point->GetName();
            luaL_error(L, "%s:SetParent(): Couldn't find 'this' in parent object", name ? name : "<unnamed>");
            return 0;
        }

        if (!path->IsA(CSimplePathAnim::GetObjectType())) {
            luaL_error(L, "%s:SetParent(): Wrong parent object type, expected Path", point->GetDisplayName());
            return 0;
        }
    }

    if (!path) {
        const char* name = point->GetName();
        luaL_error(L, "%s:SetParent(): Couldn't find Path named '%s'", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    int32_t oldOrder = point->m_order;

    if (lua_isnumber(L, 3)) {
        point->SetOrder(static_cast<int32_t>(lua_tointeger(L, 3) - 1), false, false);
    } else {
        int32_t order = point->m_order;

        if (order == -1) {
            order = path->GetMaxControlPointOrder() + 1;
        }

        point->SetOrder(order, false, true);
    }

    if (!point->ChangeParent(path) && oldOrder != point->m_order) {
        path->NotifyOrderChanged(point);
    }

    return 0;
}

// VERIFY: a0fd78bf auto
// OFFSET: 0x004A6980
static int32_t Script_SetOffset(lua_State* L) {
    CSimpleControlPoint* point = static_cast<CSimpleControlPoint*>(FrameScript_GetObjectThis(L, CSimpleControlPoint::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = point->GetName();
        luaL_error(L, "Usage: %s:SetOffset(x, y)", name ? name : "<unnamed>");
        return 0;
    }

    C2Vector offset;
    float x = static_cast<float>(lua_tonumber(L, 2));
    offset.x = NDCToDDCWidth(x / (CoordinateGetAspectCompensation() * 1024.0f));
    float y = static_cast<float>(lua_tonumber(L, 3));
    offset.y = NDCToDDCWidth(y / (CoordinateGetAspectCompensation() * 1024.0f));
    point->SetOffset(offset);

    return 0;
}

// VERIFY: 66dd590a manual
// OFFSET: 0x004A6A70
static int32_t Script_GetOffset(lua_State* L) {
    CSimpleControlPoint* point = static_cast<CSimpleControlPoint*>(FrameScript_GetObjectThis(L, CSimpleControlPoint::GetObjectType()));

    float x = point->m_offset.x;
    float y = point->m_offset.y;
    x = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * x);
    y = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * y);

    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// VERIFY: 6c29894d auto
// OFFSET: 0x004A6B10
static int32_t Script_SetOrder(lua_State* L) {
    CSimpleControlPoint* point = static_cast<CSimpleControlPoint*>(FrameScript_GetObjectThis(L, CSimpleControlPoint::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = point->GetName();
        luaL_error(L, "Usage: %s:SetOrder(order)", name ? name : "<unnamed>");
        return 0;
    }

    point->SetOrder(static_cast<int32_t>(lua_tointeger(L, 2) - 1), true, false);
    return 0;
}

// VERIFY: 0848e1cc manual
// OFFSET: 0x004A6BA0
static int32_t Script_GetOrder(lua_State* L) {
    CSimpleControlPoint* point = static_cast<CSimpleControlPoint*>(FrameScript_GetObjectThis(L, CSimpleControlPoint::GetObjectType()));
    lua_pushinteger(L, point->m_order + 1);
    return 1;
}

// VERIFY: 02dfb2f9 auto
// OFFSET: 0x004A6BF0
void CSimpleControlPoint::CreateScriptMetaTable() {
    CSimpleControlPoint::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleControlPoint::RegisterScriptMethods);
}

// VERIFY: 1011bc7c auto
// OFFSET: 0x004A4DC0
void CSimpleControlPoint::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleControlPoint::s_metatable);
    CSimpleControlPoint::s_metatable = LUA_NOREF;
}

// STATIC: 0x00AC1A48
static FrameScript_Method s_ScriptMethods[] = {
    { "SetParent", &Script_SetParent },
    { "SetOffset", &Script_SetOffset },
    { "GetOffset", &Script_GetOffset },
    { "SetOrder", &Script_SetOrder },
    { "GetOrder", &Script_GetOrder },
};

// VERIFY: c332bef1 auto
// OFFSET: 0x004A4DA0
void CSimpleControlPoint::RegisterScriptMethods(lua_State* L) {
    CScriptObject::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
