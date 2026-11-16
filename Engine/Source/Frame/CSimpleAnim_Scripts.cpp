#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleAnim.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Frame/CSimpleAnimGroup.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 6240f242 manual
// OFFSET: 0x004A4BA0
void ReadOrigin(lua_State* L, int32_t index, FRAMEPOINT& point, C2Vector& offset) {
    if (!StringToFramePoint(lua_tolstring(L, index, nullptr), point)) {
        point = FRAMEPOINT_CENTER;
    }

    float x = lua_tonumber(L, index + 1);
    offset.x = NDCToDDCWidth(x / (CoordinateGetAspectCompensation() * 1024.0f));

    float y = lua_tonumber(L, index + 2);
    offset.y = NDCToDDCWidth(y / (CoordinateGetAspectCompensation() * 1024.0f));
}

// VERIFY: 8b4c1c68 manual
// OFFSET: 0x004A4F40
static int32_t Script_Play(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    anim->Play();
    return 0;
}

// VERIFY: 2d856ffb manual
// OFFSET: 0x004A4F80
static int32_t Script_Pause(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    anim->Pause();
    return 0;
}

// VERIFY: 8d9c25b8 manual
// OFFSET: 0x004A4FC0
static int32_t Script_Stop(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    anim->Stop(true);
    return 0;
}

// VERIFY: 3571d7a9 manual
// OFFSET: 0x004A5000
static int32_t Script_IsDone(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushboolean(L, anim->m_progressWithDelay >= 1.0f);
    return 1;
}

// VERIFY: 10cb206e manual
// OFFSET: 0x004A5060
static int32_t Script_IsPlaying(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushboolean(L, anim->m_playState == 1);
    return 1;
}

// VERIFY: 1f6afac5 manual
// OFFSET: 0x004A50B0
static int32_t Script_IsPaused(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushboolean(L, anim->m_playState == 2);
    return 1;
}

// VERIFY: d9cbdaea manual
// OFFSET: 0x004A5100
static int32_t Script_IsStopped(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushboolean(L, anim->m_playState == 0);
    return 1;
}

// VERIFY: 9be75a62 manual
// OFFSET: 0x004A5150
static int32_t Script_IsDelaying(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushboolean(L, anim->IsDelaying());
    return 1;
}

// VERIFY: c181e1fb auto
// OFFSET: 0x004A51A0
static int32_t Script_SetStartDelay(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetStartDelay(delaySec)", name ? name : "<unnamed>");
        return 0;
    }

    anim->SetStartDelay(lua_tonumber(L, 2), true);
    return 0;
}

// VERIFY: 5652db04 manual
// OFFSET: 0x004A5220
static int32_t Script_GetStartDelay(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_startDelay);
    return 1;
}

// VERIFY: 6c5823ac auto
// OFFSET: 0x004A5260
static int32_t Script_SetEndDelay(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetEndDelay(delaySec)", name ? name : "<unnamed>");
        return 0;
    }

    anim->SetEndDelay(lua_tonumber(L, 2), true);
    return 0;
}

// VERIFY: d6ceabd9 manual
// OFFSET: 0x004A52E0
static int32_t Script_GetEndDelay(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_endDelay);
    return 1;
}

// VERIFY: 9bef9c68 auto
// OFFSET: 0x004A5320
static int32_t Script_SetDuration(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetDuration(durationSec)", name ? name : "<unnamed>");
        return 0;
    }

    anim->SetDuration(lua_tonumber(L, 2), true);
    return 0;
}

// VERIFY: f7db29c2 manual
// OFFSET: 0x004A53A0
static int32_t Script_GetDuration(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_duration);
    return 1;
}

// VERIFY: bc84255c manual
// OFFSET: 0x004A53E0
static int32_t Script_GetSmoothProgress(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_smoothProgress);
    return 1;
}

// VERIFY: 9bb2299b auto
// OFFSET: 0x004A5430
static int32_t Script_SetSmoothProgress(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetSmoothProgress(smoothProgress)", name ? name : "<unnamed>");
        return 0;
    }

    anim->SetSmoothProgress(lua_tonumber(L, 2));
    return 0;
}

// VERIFY: 3893297d manual
// OFFSET: 0x004A54B0
static int32_t Script_GetProgress(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_progress);
    return 1;
}

// VERIFY: f85aec55 manual
// OFFSET: 0x004A5500
static int32_t Script_GetProgressWithDelay(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_progressWithDelay);
    return 1;
}

// VERIFY: 8320173e auto
// OFFSET: 0x004A5550
static int32_t Script_SetMaxFramerate(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetMaxFramerate(framesPerSec)", name ? name : "<unnamed>");
        return 0;
    }

    anim->SetMaxFramerate(lua_tonumber(L, 2));
    return 0;
}

// VERIFY: eaba25ad manual
// OFFSET: 0x004A55D0
static int32_t Script_GetMaxFramerate(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_maxFramerate);
    return 1;
}

// VERIFY: 073286dd manual
// OFFSET: 0x004A5610
static int32_t Script_GetElapsed(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushnumber(L, anim->m_elapsed);
    return 1;
}

// VERIFY: 3c6b7087 auto
// OFFSET: 0x004A5660
static int32_t Script_SetOrder(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetOrder(order)", name ? name : "<unnamed>");
        return 0;
    }

    // A playing group walks its order lists while running scripts, so they must not change under it.
    if (anim->m_group && anim->m_group->m_loopState) {
        luaL_error(L, "%s:SetOrder(): Cannot change the order while the animation group is playing", anim->GetDisplayName());
        return 0;
    }

    anim->SetOrder(lua_tointeger(L, 2) - 1, true, false);
    return 0;
}

// VERIFY: 715d325b manual
// OFFSET: 0x004A56F0
static int32_t Script_GetOrder(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    lua_pushinteger(L, anim->m_order + 1);
    return 1;
}

// VERIFY: c0edbd01 auto
// OFFSET: 0x004A5740
static int32_t Script_SetSmoothing(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetSmoothing(\"smoothingType\")", name ? name : "<unnamed>");
        return 0;
    }

    float in;
    float out;

    if (StringToSmoothWeights(lua_tolstring(L, 2, nullptr), in, out)) {
        anim->SetSmoothing(in, out);
        return 0;
    }

    const char* name = anim->GetName();
    luaL_error(L, "%s:SetSmoothing(): Unknown smoothing type specified", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 4153082c manual
// OFFSET: 0x004A5810
static int32_t Script_GetSmoothing(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    float in;
    float out;
    anim->GetSmoothing(in, out);

    lua_pushstring(L, SmoothWeightsToString(in, out));
    return 1;
}

// VERIFY: 2887ad52 auto
// OFFSET: 0x004A5880
static int32_t Script_SetParent(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (lua_type(L, 2) == LUA_TNIL) {
        const char* name = anim->GetName();
        luaL_error(L, "%s:SetParent(): Cannot set a 'nil' parent for animations", name ? name : "<unnamed>");
        return 0;
    }

    CSimpleAnimGroup* group = nullptr;

    if (lua_isstring(L, 2)) {
        group = static_cast<CSimpleAnimGroup*>(CScriptObject::GetScriptObjectByName(lua_tolstring(L, 2, nullptr), CSimpleAnimGroup::GetObjectType()));
    } else if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        group = static_cast<CSimpleAnimGroup*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!group) {
            const char* name = anim->GetName();
            luaL_error(L, "%s:SetParent(): Couldn't find 'this' in parent object", name ? name : "<unnamed>");
            return 0;
        }

        if (!group->IsA(CSimpleAnimGroup::GetObjectType())) {
            luaL_error(L, "%s:SetParent(): Wrong parent object type, expected AnimationGroup", anim->GetDisplayName());
            return 0;
        }
    }

    if (!group) {
        const char* name = anim->GetName();
        luaL_error(L, "%s:SetParent(): Couldn't find animation group named '%s'", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if ((anim->m_group && anim->m_group->m_loopState) || group->m_loopState) {
        luaL_error(L, "%s:SetParent(): Cannot move an animation while its animation group is playing", anim->GetDisplayName());
        return 0;
    }

    int32_t oldOrder = anim->m_order;

    if (lua_isnumber(L, 3)) {
        anim->SetOrder(lua_tointeger(L, 3) - 1, false, false);
    } else {
        int32_t order = anim->m_order;

        if (order == -1) {
            order = group->GetMaxAnimOrder() + 1;
        }

        anim->SetOrder(order, false, true);
    }

    if (!anim->ChangeParent(group) && oldOrder != anim->m_order) {
        group->NotifyOrderChanged(anim, oldOrder);
    }

    return 0;
}

// VERIFY: 8d6b6b0d auto
// OFFSET: 0x004A5A70
static int32_t Script_GetRegionParent(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));
    CScriptRegion* region = anim->GetRegionParent();

    if (region) {
        if (!region->m_luaRegistered) {
            region->RegisterScriptObject(nullptr);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, region->m_luaObjectRef);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 28abea4d auto
// OFFSET: 0x004A5AF0
static int32_t Script_HasScript(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:HasScript(\"type\")", name ? name : "<unnamed>");
        return 0;
    }

    FrameScript_Object::ScriptData data;

    if (anim->GetScriptByName(lua_tolstring(L, 2, nullptr), data)) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 8314e76f auto
// OFFSET: 0x004A5BB0
static int32_t Script_GetScript(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:GetScript(\"type\")", name ? name : "<unnamed>");
        return 0;
    }

    FrameScript_Object::ScriptData data;
    FrameScript_Object::ScriptIx* script = anim->GetScriptByName(lua_tolstring(L, 2, nullptr), data);

    if (!script) {
        const char* name = anim->GetName();
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

// VERIFY: 52cbe5bc auto
// OFFSET: 0x004A5CC0
static int32_t Script_SetScript(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isstring(L, 2) || (lua_type(L, 3) != LUA_TFUNCTION && lua_type(L, 3) != LUA_TNIL)) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:SetScript(\"type\", function)", name ? name : "<unnamed>");
        return 0;
    }

    FrameScript_Object::ScriptData data;
    FrameScript_Object::ScriptIx* script = anim->GetScriptByName(lua_tolstring(L, 2, nullptr), data);

    if (!script) {
        const char* name = anim->GetName();
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

// VERIFY: 52635e16 auto
// OFFSET: 0x004A5DF0
static int32_t Script_HookScript(lua_State* L) {
    CSimpleAnim* anim = static_cast<CSimpleAnim*>(FrameScript_GetObjectThis(L, CSimpleAnim::GetObjectType()));

    if (!lua_isstring(L, 2) || lua_type(L, 3) != LUA_TFUNCTION) {
        const char* name = anim->GetName();
        luaL_error(L, "Usage: %s:HookScript(\"type\", function)", name ? name : "<unnamed>");
        return 0;
    }

    FrameScript_Object::ScriptData data;
    FrameScript_Object::ScriptIx* script = anim->GetScriptByName(lua_tolstring(L, 2, nullptr), data);

    if (!script) {
        const char* name = anim->GetName();
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

// VERIFY: 0fae3dcc auto
// OFFSET: 0x004A6020
void CSimpleAnim::CreateScriptMetaTable() {
    CSimpleAnim::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleAnim::RegisterScriptMethods);
}

// VERIFY: ace702cb auto
// OFFSET: 0x004A4C50
void CSimpleAnim::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleAnim::s_metatable);
    CSimpleAnim::s_metatable = LUA_NOREF;
}

// STATIC: 0x00AC18E0
static FrameScript_Method s_ScriptMethods[] = {
    { "Play", &Script_Play },
    { "Pause", &Script_Pause },
    { "Stop", &Script_Stop },
    { "IsDone", &Script_IsDone },
    { "IsPlaying", &Script_IsPlaying },
    { "IsPaused", &Script_IsPaused },
    { "IsStopped", &Script_IsStopped },
    { "IsDelaying", &Script_IsDelaying },
    { "GetElapsed", &Script_GetElapsed },
    { "SetStartDelay", &Script_SetStartDelay },
    { "GetStartDelay", &Script_GetStartDelay },
    { "SetEndDelay", &Script_SetEndDelay },
    { "GetEndDelay", &Script_GetEndDelay },
    { "SetDuration", &Script_SetDuration },
    { "GetDuration", &Script_GetDuration },
    { "GetSmoothProgress", &Script_GetSmoothProgress },
    { "SetSmoothProgress", &Script_SetSmoothProgress },
    { "GetProgress", &Script_GetProgress },
    { "GetProgressWithDelay", &Script_GetProgressWithDelay },
    { "SetMaxFramerate", &Script_SetMaxFramerate },
    { "GetMaxFramerate", &Script_GetMaxFramerate },
    { "SetOrder", &Script_SetOrder },
    { "GetOrder", &Script_GetOrder },
    { "SetSmoothing", &Script_SetSmoothing },
    { "GetSmoothing", &Script_GetSmoothing },
    { "SetParent", &Script_SetParent },
    { "GetRegionParent", &Script_GetRegionParent },
    { "HasScript", &Script_HasScript },
    { "GetScript", &Script_GetScript },
    { "SetScript", &Script_SetScript },
    { "HookScript", &Script_HookScript },
};

// VERIFY: 14f4a5b3 auto
// OFFSET: 0x004A4C30
void CSimpleAnim::RegisterScriptMethods(lua_State* L) {
    CScriptObject::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
