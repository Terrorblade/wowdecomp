#include <cstddef>
#include <cstdint>
#include <new>
#include "Engine/Source/Frame/CSimplePathAnim.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CSimpleControlPoint.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 572f3afe auto
// OFFSET: 0x004A6C10
static int32_t Script_SetCurve(lua_State* L) {
    CSimplePathAnim* path = static_cast<CSimplePathAnim*>(FrameScript_GetObjectThis(L, CSimplePathAnim::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        const char* name = path->GetName();
        luaL_error(L, "Usage: %s:SetCurve(\"curveType\")", name ? name : "<unnamed>");
        return 0;
    }

    SimpleAnimCurveType curve;

    if (StringToCurveType(lua_tolstring(L, 2, nullptr), curve)) {
        path->m_curve = curve;
        return 0;
    }

    const char* name = path->GetName();
    luaL_error(L, "%s:SetCurve(): Unknown curve type specified", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 5b70adcb manual
// OFFSET: 0x004A6CD0
static int32_t Script_GetCurve(lua_State* L) {
    CSimplePathAnim* path = static_cast<CSimplePathAnim*>(FrameScript_GetObjectThis(L, CSimplePathAnim::GetObjectType()));
    lua_pushstring(L, CurveTypeToString(static_cast<SimpleAnimCurveType>(path->m_curve)));
    return 1;
}

// VERIFY: 5ed6df17 manual
// OFFSET: 0x004A6D20
static int32_t Script_GetMaxOrder(lua_State* L) {
    CSimplePathAnim* path = static_cast<CSimplePathAnim*>(FrameScript_GetObjectThis(L, CSimplePathAnim::GetObjectType()));
    int32_t order = path->GetMaxControlPointOrder() + 1;
    lua_pushnumber(L, order);
    return 1;
}

// VERIFY: 164f44e2 auto
// OFFSET: 0x004A79D0
static int32_t Script_GetControlPoints(lua_State* L) {
    CSimplePathAnim* path = static_cast<CSimplePathAnim*>(FrameScript_GetObjectThis(L, CSimplePathAnim::GetObjectType()));
    uint32_t count = path->m_sortedPoints.Count();

    if (!count) {
        return 0;
    }

    if (!lua_checkstack(L, count)) {
        const char* name = path->GetName();
        luaL_error(L, "%s:GetControlPoints(): Stack overflow", name ? name : "<unnamed>");
        return 0;
    }

    for (uint32_t i = 0; i < count; i++) {
        CSimpleControlPoint* point = path->GetControlPoint(i);

        if (!point->m_luaRegistered) {
            point->RegisterScriptObject(nullptr);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, point->m_luaObjectRef);
    }

    return count;
}

// VERIFY: 51d8de97 manual
// OFFSET: 0x004A7BF0
static int32_t Script_CreateControlPoint(lua_State* L) {
    XMLNode* inheritNode = nullptr;
    CSimplePathAnim* path = static_cast<CSimplePathAnim*>(FrameScript_GetObjectThis(L, CSimplePathAnim::GetObjectType()));
    const char* name = nullptr;

    if (lua_isstring(L, 2)) {
        name = lua_tolstring(L, 2, nullptr);
    }

    if (lua_type(L, 3) == LUA_TSTRING) {
        const char* inherits = lua_tolstring(L, 3, nullptr);
        const char* taint;
        bool locked;
        inheritNode = FrameXML_AcquireHashNode(inherits, taint, locked);

        if (!inheritNode) {
            const char* pathName = path->GetName();
            luaL_error(L, "%s:CreateControlPoint(): Couldn't find inherited node \"%s\"", pathName ? pathName : "<unnamed>", inherits);
            return 0;
        }

        if (locked) {
            const char* pathName = path->GetName();
            luaL_error(L, "%s:CreateControlPoint(): Recursively inherited node \"%s\"", pathName ? pathName : "<unnamed>", inherits);
            return 0;
        }
    }

    void* mem = SMemAlloc(sizeof(CSimpleControlPoint), __FILE__, __LINE__, 0);
    CSimpleControlPoint* point = mem ? new (mem) CSimpleControlPoint(path) : nullptr;

    if (!point) {
        lua_pushnil(L);
        return 1;
    }

    if (name && *name) {
        point->SetName(name);
    }

    if (inheritNode) {
        CStatus status;
        point->LoadXML(inheritNode, &status);
        FrameXML_ReleaseHashNode(lua_tolstring(L, 3, nullptr));
    }

    if (lua_isnumber(L, 4)) {
        point->SetOrder(static_cast<int32_t>(lua_tointeger(L, 4) - 1), false, false);
    }

    if (point->m_order == -1) {
        point->SetOrder(path->GetMaxControlPointOrder(), false, true);
    }

    path->NotifyControlPointAdded(point);

    if (!point->m_luaRegistered) {
        point->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, point->m_luaObjectRef);
    return 1;
}

// VERIFY: fe85cabc auto
// OFFSET: 0x004A6D80
void CSimplePathAnim::CreateScriptMetaTable() {
    CSimplePathAnim::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimplePathAnim::RegisterScriptMethods);
}

// VERIFY: edb69ae2 auto
// OFFSET: 0x004A4E20
void CSimplePathAnim::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimplePathAnim::s_metatable);
    CSimplePathAnim::s_metatable = LUA_NOREF;
}

// STATIC: 0x00AC1A74
static FrameScript_Method s_ScriptMethods[] = {
    { "SetCurve", &Script_SetCurve },
    { "GetCurve", &Script_GetCurve },
    { "GetControlPoints", &Script_GetControlPoints },
    { "CreateControlPoint", &Script_CreateControlPoint },
    { "GetMaxOrder", &Script_GetMaxOrder },
};

// VERIFY: 71140725 manual
// OFFSET: 0x004A4DF0
void CSimplePathAnim::RegisterScriptMethods(lua_State* L) {
    CSimpleAnim::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
