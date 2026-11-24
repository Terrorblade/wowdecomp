#include "CharacterModelBase.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/DB/DBClient/CreatureStats_C.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"

// VERIFY: 6ff99da1 auto
// OFFSET: 0x00597250
void CGCharacterModelBase::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CGCharacterModelBase::s_metatable);
    CGCharacterModelBase::s_metatable = LUA_NOREF;
}

// VERIFY: 71945220 auto
// OFFSET: 0x00597A80
void CGCharacterModelBase::CreateScriptMetaTable() {
    CGCharacterModelBase::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CGCharacterModelBase::RegisterScriptMethods);
}

// VERIFY: 8f11348e manual
// OFFSET: 0x00597960
static int32_t Script_SetCreature(lua_State* L) {
    CGCharacterModelBase* model = static_cast<CGCharacterModelBase*>(FrameScript_GetObjectThis(L, CGCharacterModelBase::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: SetCreature(creatureID)");
        return 0;
    }

    WOWGUID guid = WOWGUID();
    CreatureStats_C* stats = g_creatureDBCache.GetRecord(static_cast<int32_t>(lua_tointeger(L, 2)), guid, nullptr, nullptr, false);

    if (stats) {
        model->m_unitGuid = WOWGUID();
        model->m_creatureStats = stats;

        if (model->m_isVisible) {
            model->SetUpModel(stats);
        }
    }

    return 0;
}

// VERIFY: 16330539 manual
// OFFSET: 0x00597A10
static int32_t Script_SetRotation(lua_State* L) {
    CGCharacterModelBase* model = static_cast<CGCharacterModelBase*>(FrameScript_GetObjectThis(L, CGCharacterModelBase::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: SetRotation(rotation (in radians))");
        return 0;
    }

    model->RotateModel(static_cast<float>(lua_tonumber(L, 2)));
    return 0;
}

// VERIFY: a7b1e08a auto
// OFFSET: 0x005978E0
static int32_t Script_ModelSetUnit(lua_State* L) {
    CGCharacterModelBase* model = static_cast<CGCharacterModelBase*>(FrameScript_GetObjectThis(L, CGCharacterModelBase::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        luaL_error(L, "Usage: SetUnit(\"unit\")");
        return 0;
    }

    WOWGUID guid = GetGuidByKeyword(lua_tolstring(L, 2, nullptr));

    if (guid != WOWGUID()) {
        model->RefreshUnit(guid);
    }

    return 0;
}

// VERIFY: bcc692dc manual
// OFFSET: 0x00597B00
static int32_t Script_RefreshUnit(lua_State* L) {
    CGCharacterModelBase* model = static_cast<CGCharacterModelBase*>(FrameScript_GetObjectThis(L, CGCharacterModelBase::GetObjectType()));
    model->RefreshUnit(model->m_unitGuid);
    return 0;
}

// STATIC: 0x00ACF514
static FrameScript_Method s_ScriptMethods[] = {
    { "SetUnit", &Script_ModelSetUnit },
    { "SetCreature", &Script_SetCreature },
    { "RefreshUnit", &Script_RefreshUnit },
    { "SetRotation", &Script_SetRotation },
};

// VERIFY: 477f0f97 auto
// OFFSET: 0x00597230
void CGCharacterModelBase::RegisterScriptMethods(lua_State* L) {
    CSimpleModel::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
