#include <cstddef>
#include <cstdint>
#include <new>
#include "Engine/Source/Frame/CSimpleAnimGroup.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CSimpleAlphaAnim.h"
#include "Engine/Source/Frame/CSimpleAnim.h"
#include "Engine/Source/Frame/CSimplePathAnim.h"
#include "Engine/Source/Frame/CSimpleRotationAnim.h"
#include "Engine/Source/Frame/CSimpleScaleAnim.h"
#include "Engine/Source/Frame/CSimpleTranslationAnim.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 95333f8a manual
// OFFSET: 0x004A6EA0
static int32_t Script_Play(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    group->Play();
    return 0;
}

// VERIFY: 25f9edb3 manual
// OFFSET: 0x004A6EE0
static int32_t Script_Pause(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    group->Pause();
    return 0;
}

// VERIFY: 6d4002c3 manual
// OFFSET: 0x004A6F20
static int32_t Script_Stop(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    group->Stop(true);
    return 0;
}

// VERIFY: ad2f4202 manual
// OFFSET: 0x004A6F60
static int32_t Script_Finish(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    group->Finish();
    return 0;
}

// VERIFY: 3f6c5bdc manual
// OFFSET: 0x004A6FA0
static int32_t Script_GetProgress(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    lua_pushnumber(L, group->m_elapsed / group->m_duration);
    return 1;
}

// VERIFY: 1a1fa5ce manual
// OFFSET: 0x004A6FF0
static int32_t Script_IsDone(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    lua_pushboolean(L, group->IsDone());
    return 1;
}

// VERIFY: c1e4dcb4 manual
// OFFSET: 0x004A7040
static int32_t Script_IsPaused(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    lua_pushboolean(L, (group->m_flags & 0x4) != 0);
    return 1;
}

// VERIFY: abd9dfba manual
// OFFSET: 0x004A7090
static int32_t Script_IsPendingFinish(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    lua_pushboolean(L, (group->m_flags & 0x2) != 0);
    return 1;
}

// VERIFY: 61a8b07d manual
// OFFSET: 0x004A70E0
static int32_t Script_GetDuration(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    lua_pushnumber(L, group->m_duration);
    return 1;
}

// VERIFY: 723f9d56 auto
// OFFSET: 0x004A7130
static int32_t Script_SetLooping(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        const char* name = group->GetName();
        luaL_error(L, "Usage: %s:SetLooping(\"loopType\")", name ? name : "<unnamed>");
        return 0;
    }

    SimpleAnimLoopType loopType;

    if (StringToLoopType(lua_tolstring(L, 2, nullptr), loopType)) {
        group->m_loopType = loopType;
        return 0;
    }

    const char* name = group->GetName();
    luaL_error(L, "%s:SetLooping(): Unknown loop type specified", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 3ab31767 manual
// OFFSET: 0x004A71F0
static int32_t Script_GetLooping(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    lua_pushstring(L, LoopTypeToString(static_cast<SimpleAnimLoopType>(group->m_loopType)));
    return 1;
}

// VERIFY: 252a4ecf manual
// OFFSET: 0x004A7240
static int32_t Script_GetLoopState(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    lua_pushstring(L, LoopStateToString(static_cast<SimpleAnimLoopState>(group->m_loopState)));
    return 1;
}

// VERIFY: 20ea4d5f manual
// OFFSET: 0x004A7290
static int32_t Script_GetMaxOrder(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    int32_t order = group->GetMaxAnimOrder() + 1;
    lua_pushnumber(L, order);
    return 1;
}

// VERIFY: ec7b6810 auto
// OFFSET: 0x004A72F0
static int32_t Script_SetInitialOffset(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = group->GetName();
        luaL_error(L, "Usage: %s:SetInitialOffset(x, y)", name ? name : "<unnamed>");
        return 0;
    }

    C2Vector offset;
    float x = static_cast<float>(lua_tonumber(L, 2));
    offset.x = NDCToDDCWidth(x / (CoordinateGetAspectCompensation() * 1024.0f));
    float y = static_cast<float>(lua_tonumber(L, 3));
    offset.y = NDCToDDCWidth(y / (CoordinateGetAspectCompensation() * 1024.0f));
    group->SetInitialOffset(offset);

    return 0;
}

// VERIFY: 9c974d46 manual
// OFFSET: 0x004A73E0
static int32_t Script_GetInitialOffset(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));

    float x = group->m_initialOffset.x;
    float y = group->m_initialOffset.y;
    x = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * x);
    y = DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * y);

    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// VERIFY: b6d7b6d7 auto
// OFFSET: 0x004A7480
static int32_t Script_HasScript(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        const char* name = group->GetName();
        luaL_error(L, "Usage: %s:HasScript(\"type\")", name ? name : "<unnamed>");
        return 0;
    }

    FrameScript_Object::ScriptData data;

    if (group->GetScriptByName(lua_tolstring(L, 2, nullptr), data)) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 78cae4b1 auto
// OFFSET: 0x004A7540
static int32_t Script_GetScript(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        const char* name = group->GetName();
        luaL_error(L, "Usage: %s:GetScript(\"type\")", name ? name : "<unnamed>");
        return 0;
    }

    FrameScript_Object::ScriptData data;
    FrameScript_Object::ScriptIx* script = group->GetScriptByName(lua_tolstring(L, 2, nullptr), data);

    if (!script) {
        const char* name = group->GetName();
        luaL_error(L, "%s doesn't have a \"%s\" script", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (script->m_tainted && lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = script->m_tainted;
    }

    if (script->m_luaRef > 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_luaRef);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 7ee7ae6c auto
// OFFSET: 0x004A7650
static int32_t Script_SetScript(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));

    if (!lua_isstring(L, 2) || (lua_type(L, 3) != LUA_TFUNCTION && lua_type(L, 3) != LUA_TNIL)) {
        const char* name = group->GetName();
        luaL_error(L, "Usage: %s:SetScript(\"type\", function)", name ? name : "<unnamed>");
        return 0;
    }

    FrameScript_Object::ScriptData data;
    FrameScript_Object::ScriptIx* script = group->GetScriptByName(lua_tolstring(L, 2, nullptr), data);

    if (!script) {
        const char* name = group->GetName();
        luaL_error(L, "%s doesn't have a \"%s\" script", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (script->m_luaRef) {
        luaL_unref(L, LUA_REGISTRYINDEX, script->m_luaRef);
    }

    int32_t ref = luaL_ref(L, LUA_REGISTRYINDEX);
    script->m_luaRef = ref > 0 ? ref : 0;
    script->m_tainted = lua_tainted;

    return 0;
}

// VERIFY: a3fa046a auto
// OFFSET: 0x004A7780
static int32_t Script_HookScript(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));

    if (!lua_isstring(L, 2) || lua_type(L, 3) != LUA_TFUNCTION) {
        const char* name = group->GetName();
        luaL_error(L, "Usage: %s:HookScript(\"type\", function)", name ? name : "<unnamed>");
        return 0;
    }

    FrameScript_Object::ScriptData data;
    FrameScript_Object::ScriptIx* script = group->GetScriptByName(lua_tolstring(L, 2, nullptr), data);

    if (!script) {
        const char* name = group->GetName();
        luaL_error(L, "%s doesn't have a \"%s\" script", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (!script->m_luaRef) {
        int32_t ref = luaL_ref(L, LUA_REGISTRYINDEX);
        script->m_luaRef = ref > 0 ? ref : 0;
        script->m_tainted = lua_tainted;
        return 0;
    }

    lua_remove(L, 2);

    const char* hookTaint = lua_tainted;

    if (++lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = nullptr;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_luaRef);
    luaL_unref(L, LUA_REGISTRYINDEX, script->m_luaRef);

    const char* scriptTaint = script->m_tainted;
    const char* taint = lua_tainted;

    if (++lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = scriptTaint;
    }

    lua_insert(L, 2);
    lua_settaint(L, 2, scriptTaint);
    lua_settaint(L, 3, hookTaint);
    lua_pushcclosure(L, &FrameScript_HookWrapper, 2);
    lua_settaint(L, 2, scriptTaint);

    if (lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = taint;
    }

    if (--lua_taintexpected <= 0) {
        lua_taintexpected = 0;
    }

    script->m_luaRef = luaL_ref(L, LUA_REGISTRYINDEX);

    if (lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = hookTaint;
    }

    if (--lua_taintexpected <= 0) {
        lua_taintexpected = 0;
    }

    return 0;
}

// VERIFY: 582b2e24 manual
// OFFSET: 0x004A7AA0
static int32_t Script_IsPlaying(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    lua_pushboolean(L, group->m_loopState && !(group->m_flags & 0x4));
    return 1;
}

// VERIFY: 994740f8 manual
// OFFSET: 0x004A7B00
static int32_t Script_GetAnimations(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    TSExplicitList<CSimpleAnim, offsetof(CSimpleAnim, m_link)>* animations = &group->m_animations;

    if (!animations) {
        return 0;
    }

    int32_t count = 0;

    for (CSimpleAnim* anim = animations->Head(); anim; anim = animations->Next(anim)) {
        count++;
    }

    if (!lua_checkstack(L, count)) {
        const char* name = group->GetName();
        luaL_error(L, "%s:GetAnimations(): Stack overflow", name ? name : "<unnamed>");
        return 0;
    }

    for (CSimpleAnim* anim = animations->Head(); anim; anim = animations->Next(anim)) {
        if (!anim->m_luaRegistered) {
            anim->RegisterScriptObject(nullptr);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, anim->m_luaObjectRef);
    }

    return count;
}

// VERIFY: d865b0ab manual
// OFFSET: 0x004A7E00
static int32_t Script_CreateAnimation(lua_State* L) {
    CSimpleAnimGroup* group = static_cast<CSimpleAnimGroup*>(FrameScript_GetObjectThis(L, CSimpleAnimGroup::GetObjectType()));
    const char* type = "Animation";

    if (lua_isstring(L, 2)) {
        type = lua_tolstring(L, 2, nullptr);
    }

    const char* name = nullptr;

    if (lua_isstring(L, 3)) {
        name = lua_tolstring(L, 3, nullptr);
    }

    XMLNode* inheritNode = nullptr;

    if (lua_type(L, 4) == LUA_TSTRING) {
        const char* inherits = lua_tolstring(L, 4, nullptr);
        const char* taint;
        bool locked;
        inheritNode = FrameXML_AcquireHashNode(inherits, taint, locked);

        if (!inheritNode) {
            const char* groupName = group->GetName();
            luaL_error(L, "%s:CreateAnimation(): Couldn't find inherited node \"%s\"", groupName ? groupName : "<unnamed>", inherits);
            return 0;
        }

        if (locked) {
            const char* groupName = group->GetName();
            luaL_error(L, "%s:CreateAnimation(): Recursively inherited node \"%s\"", groupName ? groupName : "<unnamed>", inherits);
            return 0;
        }
    }

    CSimpleAnim* anim;

    if (type && *type) {
        if (!SStrCmpI(type, "Animation", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleAnim), __FILE__, __LINE__, 0);
            anim = mem ? new (mem) CSimpleAnim(group) : nullptr;
        } else if (!SStrCmpI(type, "Translation", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleTranslationAnim), __FILE__, __LINE__, 0);
            anim = mem ? new (mem) CSimpleTranslationAnim(group) : nullptr;
        } else if (!SStrCmpI(type, "Rotation", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleRotationAnim), __FILE__, __LINE__, 0);
            anim = mem ? new (mem) CSimpleRotationAnim(group) : nullptr;
        } else if (!SStrCmpI(type, "Scale", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleScaleAnim), __FILE__, __LINE__, 0);
            anim = mem ? new (mem) CSimpleScaleAnim(group) : nullptr;
        } else if (!SStrCmpI(type, "Path", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimplePathAnim), __FILE__, __LINE__, 0);
            anim = mem ? new (mem) CSimplePathAnim(group) : nullptr;
        } else if (!SStrCmpI(type, "Alpha", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleAlphaAnim), __FILE__, __LINE__, 0);
            anim = mem ? new (mem) CSimpleAlphaAnim(group) : nullptr;
        } else {
            void* mem = SMemAlloc(sizeof(CSimpleAnim), __FILE__, __LINE__, 0);
            anim = mem ? new (mem) CSimpleAnim(group) : nullptr;
        }
    } else {
        void* mem = SMemAlloc(sizeof(CSimpleAnim), __FILE__, __LINE__, 0);
        anim = mem ? new (mem) CSimpleAnim(group) : nullptr;
    }

    if (!anim) {
        lua_pushnil(L);
        return 1;
    }

    if (name && *name) {
        anim->SetName(name);
    }

    if (inheritNode) {
        CStatus status;
        anim->LoadXML(inheritNode, &status);
        anim->PostLoadXML(inheritNode, &status);
        FrameXML_ReleaseHashNode(lua_tolstring(L, 4, nullptr));
    }

    if (anim->m_order == -1) {
        anim->SetOrder(group->GetMaxAnimOrder(), false, true);
    }

    group->NotifyAnimAdded(anim);

    if (!anim->m_luaRegistered) {
        anim->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, anim->m_luaObjectRef);
    return 1;
}

// VERIFY: d2ab9274 auto
// OFFSET: 0x004A79B0
void CSimpleAnimGroup::CreateScriptMetaTable() {
    CSimpleAnimGroup::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleAnimGroup::RegisterScriptMethods);
}

// VERIFY: 0807309e auto
// OFFSET: 0x004A4ED0
void CSimpleAnimGroup::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleAnimGroup::s_metatable);
    CSimpleAnimGroup::s_metatable = LUA_NOREF;
}

// STATIC: 0x00AC1AB8
static FrameScript_Method s_ScriptMethods[] = {
    { "Play", &Script_Play },
    { "Pause", &Script_Pause },
    { "Stop", &Script_Stop },
    { "Finish", &Script_Finish },
    { "GetProgress", &Script_GetProgress },
    { "IsDone", &Script_IsDone },
    { "IsPlaying", &Script_IsPlaying },
    { "IsPaused", &Script_IsPaused },
    { "IsPendingFinish", &Script_IsPendingFinish },
    { "GetDuration", &Script_GetDuration },
    { "SetLooping", &Script_SetLooping },
    { "GetLooping", &Script_GetLooping },
    { "GetLoopState", &Script_GetLoopState },
    { "GetMaxOrder", &Script_GetMaxOrder },
    { "SetInitialOffset", &Script_SetInitialOffset },
    { "GetInitialOffset", &Script_GetInitialOffset },
    { "GetAnimations", &Script_GetAnimations },
    { "CreateAnimation", &Script_CreateAnimation },
    { "HasScript", &Script_HasScript },
    { "GetScript", &Script_GetScript },
    { "SetScript", &Script_SetScript },
    { "HookScript", &Script_HookScript },
};

// VERIFY: 33724c0a auto
// OFFSET: 0x004A4EB0
void CSimpleAnimGroup::RegisterScriptMethods(lua_State* L) {
    CScriptObject::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
