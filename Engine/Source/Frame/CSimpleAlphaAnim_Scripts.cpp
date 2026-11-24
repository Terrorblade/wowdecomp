#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleAlphaAnim.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: d8ba0810 auto
// OFFSET: 0x004A6DA0
static int32_t Script_SetChange(lua_State* L) {
    CSimpleAlphaAnim* anim = static_cast<CSimpleAlphaAnim*>(FrameScript_GetObjectThis(L, CSimpleAlphaAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetChange(change)", name ? name : "<unnamed>");
        return 0;
    }

    anim->SetChange(static_cast<float>(lua_tonumber(L, 2)));
    return 0;
}

// VERIFY: 110769d8 manual
// OFFSET: 0x004A6E20
static int32_t Script_GetChange(lua_State* L) {
    CSimpleAlphaAnim* anim = static_cast<CSimpleAlphaAnim*>(FrameScript_GetObjectThis(L, CSimpleAlphaAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_change * (1.0f / 255.0f));
    return 1;
}

// VERIFY: cd7dd594 auto
// OFFSET: 0x004A6E80
void CSimpleAlphaAnim::CreateScriptMetaTable() {
    CSimpleAlphaAnim::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleAlphaAnim::RegisterScriptMethods);
}

// VERIFY: 608d8f16 auto
// OFFSET: 0x004A4E80
void CSimpleAlphaAnim::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleAlphaAnim::s_metatable);
    CSimpleAlphaAnim::s_metatable = LUA_NOREF;
}

// STATIC: 0x00AC1AA0
static FrameScript_Method s_ScriptMethods[] = {
    { "SetChange", &Script_SetChange },
    { "GetChange", &Script_GetChange },
};

// VERIFY: 523f1ec1 manual
// OFFSET: 0x004A4E50
void CSimpleAlphaAnim::RegisterScriptMethods(lua_State* L) {
    CSimpleAnim::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
