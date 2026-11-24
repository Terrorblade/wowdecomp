#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleScaleAnim.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 9d6eda69 auto
// OFFSET: 0x004A6500
static int32_t Script_SetOrigin(lua_State* L) {
    CSimpleScaleAnim* anim = static_cast<CSimpleScaleAnim*>(FrameScript_GetObjectThis(L, CSimpleScaleAnim::GetObjectType()));

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

// VERIFY: f0a3017f manual
// OFFSET: 0x004A65D0
static int32_t Script_GetOrigin(lua_State* L) {
    CSimpleScaleAnim* anim = static_cast<CSimpleScaleAnim*>(FrameScript_GetObjectThis(L, CSimpleScaleAnim::GetObjectType()));

    float x = anim->m_originOffset.x;
    float y = anim->m_originOffset.y;

    lua_pushstring(L, FramePointToString(anim->m_origin));
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 3;
}

// VERIFY: dbbc51ea auto
// OFFSET: 0x004A6650
static int32_t Script_SetScale(lua_State* L) {
    CSimpleScaleAnim* anim = static_cast<CSimpleScaleAnim*>(FrameScript_GetObjectThis(L, CSimpleScaleAnim::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetScale(x, y)", name ? name : "<unnamed>");
        return 0;
    }

    C2Vector scale;
    scale.x = static_cast<float>(lua_tonumber(L, 2));
    scale.y = static_cast<float>(lua_tonumber(L, 3));
    anim->SetScale(scale);

    return 0;
}

// VERIFY: e7070fbc manual
// OFFSET: 0x004A6700
static int32_t Script_GetScale(lua_State* L) {
    CSimpleScaleAnim* anim = static_cast<CSimpleScaleAnim*>(FrameScript_GetObjectThis(L, CSimpleScaleAnim::GetObjectType()));

    C2Vector scale;
    anim->GetScale(scale);

    lua_pushnumber(L, scale.x);
    lua_pushnumber(L, scale.y);
    return 2;
}

// VERIFY: 044ce984 auto
// OFFSET: 0x004A6770
void CSimpleScaleAnim::CreateScriptMetaTable() {
    CSimpleScaleAnim::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleScaleAnim::RegisterScriptMethods);
}

// VERIFY: 1acf695f auto
// OFFSET: 0x004A4D70
void CSimpleScaleAnim::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleScaleAnim::s_metatable);
    CSimpleScaleAnim::s_metatable = LUA_NOREF;
}

// STATIC: 0x00AC1A24
static FrameScript_Method s_ScriptMethods[] = {
    { "SetOrigin", &Script_SetOrigin },
    { "GetOrigin", &Script_GetOrigin },
    { "SetScale", &Script_SetScale },
    { "GetScale", &Script_GetScale },
};

// VERIFY: 16733ad2 manual
// OFFSET: 0x004A4D40
void CSimpleScaleAnim::RegisterScriptMethods(lua_State* L) {
    CSimpleAnim::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
