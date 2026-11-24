#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleTranslationAnim.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: e4491c8e auto
// OFFSET: 0x004A6040
static int32_t Script_SetOffset(lua_State* L) {
    CSimpleTranslationAnim* anim = static_cast<CSimpleTranslationAnim*>(FrameScript_GetObjectThis(L, CSimpleTranslationAnim::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetOffset(x, y)", name ? name : "<unnamed>");
        return 0;
    }

    C2Vector offset;
    float x = static_cast<float>(lua_tonumber(L, 2));
    offset.x = NDCToDDCWidth(x / (CoordinateGetAspectCompensation() * 1024.0f));
    float y = static_cast<float>(lua_tonumber(L, 3));
    offset.y = NDCToDDCWidth(y / (CoordinateGetAspectCompensation() * 1024.0f));
    anim->SetOffset(offset);

    return 0;
}

// VERIFY: 23301ca9 manual
// OFFSET: 0x004A6130
static int32_t Script_GetOffset(lua_State* L) {
    CSimpleTranslationAnim* anim = static_cast<CSimpleTranslationAnim*>(FrameScript_GetObjectThis(L, CSimpleTranslationAnim::GetObjectType()));

    float x = anim->m_offset.x;
    float y = anim->m_offset.y;
    x = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * x);
    y = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * y);

    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}


// VERIFY: 5b9c53b9 auto
// OFFSET: 0x004A61D0
void CSimpleTranslationAnim::CreateScriptMetaTable() {
    CSimpleTranslationAnim::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleTranslationAnim::RegisterScriptMethods);
}

// VERIFY: 966e5a5a auto
// OFFSET: 0x004A4CB0
void CSimpleTranslationAnim::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleTranslationAnim::s_metatable);
    CSimpleTranslationAnim::s_metatable = LUA_NOREF;
}

// STATIC: 0x00AC19DC
static FrameScript_Method s_ScriptMethods[] = {
    { "SetOffset", &Script_SetOffset },
    { "GetOffset", &Script_GetOffset },
};

// VERIFY: 2a00b654 manual
// OFFSET: 0x004A4C80
void CSimpleTranslationAnim::RegisterScriptMethods(lua_State* L) {
    CSimpleAnim::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
