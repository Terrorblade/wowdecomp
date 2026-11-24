#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleRotationAnim.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Tempest/CMath.h"

// VERIFY: 7ff99a36 auto
// OFFSET: 0x004A61F0
static int32_t Script_SetOrigin(lua_State* L) {
    CSimpleRotationAnim* anim = static_cast<CSimpleRotationAnim*>(FrameScript_GetObjectThis(L, CSimpleRotationAnim::GetObjectType()));

    if (!lua_isstring(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetOrigin(point, offsetX, offsetY)", name ? name : "<unnamed>");
        return 0;
    }

    FRAMEPOINT point;
    C2Vector offset;
    ReadOrigin(L, 2, point, offset);

    anim->m_origin = point;
    anim->m_originOffset = offset;

    return 0;
}

// VERIFY: a6304d69 manual
// OFFSET: 0x004A62C0
static int32_t Script_GetOrigin(lua_State* L) {
    CSimpleRotationAnim* anim = static_cast<CSimpleRotationAnim*>(FrameScript_GetObjectThis(L, CSimpleRotationAnim::GetObjectType()));

    float x = anim->m_originOffset.x;
    float y = anim->m_originOffset.y;

    lua_pushstring(L, FramePointToString(anim->m_origin));
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 3;
}

// VERIFY: cc4b1cd0 auto
// OFFSET: 0x004A6340
static int32_t Script_SetDegrees(lua_State* L) {
    CSimpleRotationAnim* anim = static_cast<CSimpleRotationAnim*>(FrameScript_GetObjectThis(L, CSimpleRotationAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetDegrees(degrees)", name ? name : "<unnamed>");
        return 0;
    }

    anim->m_radians = static_cast<float>(lua_tonumber(L, 2) * 0.017453292f);
    return 0;
}

// VERIFY: 191fe43b manual
// OFFSET: 0x004A63C0
static int32_t Script_GetDegrees(lua_State* L) {
    CSimpleRotationAnim* anim = static_cast<CSimpleRotationAnim*>(FrameScript_GetObjectThis(L, CSimpleRotationAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_radians * 57.29578f);
    return 1;
}

// VERIFY: 22ae1dec auto
// OFFSET: 0x004A6410
static int32_t Script_SetRadians(lua_State* L) {
    CSimpleRotationAnim* anim = static_cast<CSimpleRotationAnim*>(FrameScript_GetObjectThis(L, CSimpleRotationAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetRadians(radians)", name ? name : "<unnamed>");
        return 0;
    }

    anim->m_radians = static_cast<float>(lua_tonumber(L, 2));
    return 0;
}

// VERIFY: c7a7e355 manual
// OFFSET: 0x004A6490
static int32_t Script_GetRadians(lua_State* L) {
    CSimpleRotationAnim* anim = static_cast<CSimpleRotationAnim*>(FrameScript_GetObjectThis(L, CSimpleRotationAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_radians);
    return 1;
}

// VERIFY: 3538788e auto
// OFFSET: 0x004A64E0
void CSimpleRotationAnim::CreateScriptMetaTable() {
    CSimpleRotationAnim::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleRotationAnim::RegisterScriptMethods);
}

// VERIFY: ed18cc33 auto
// OFFSET: 0x004A4D10
void CSimpleRotationAnim::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleRotationAnim::s_metatable);
    CSimpleRotationAnim::s_metatable = LUA_NOREF;
}

// STATIC: 0x00AC19F0
static FrameScript_Method s_ScriptMethods[] = {
    { "SetOrigin", &Script_SetOrigin },
    { "GetOrigin", &Script_GetOrigin },
    { "SetDegrees", &Script_SetDegrees },
    { "GetDegrees", &Script_GetDegrees },
    { "SetRadians", &Script_SetRadians },
    { "GetRadians", &Script_GetRadians },
};

// VERIFY: 3026a0e3 manual
// OFFSET: 0x004A4CE0
void CSimpleRotationAnim::RegisterScriptMethods(lua_State* L) {
    CSimpleAnim::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
