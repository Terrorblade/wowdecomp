#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleScrollFrame.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 37637b0d manual
// OFFSET: 0x00972210
static int32_t Script_SetScrollChild(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));

    if (lua_type(L, 2) == LUA_TNIL) {
        scrollFrame->SetScrollChild(nullptr);
        return 0;
    }

    CSimpleFrame* child = nullptr;

    if (lua_isstring(L, 2)) {
        child = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName(lua_tolstring(L, 2, nullptr), CSimpleFrame::GetObjectType()));
    } else if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        child = static_cast<CSimpleFrame*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!child) {
            const char* name = scrollFrame->GetName();
            luaL_error(L, "%s:SetScrollChild(): Couldn't find 'this' in child object", name ? name : "<unnamed>");
            return 0;
        }

        if (!child->IsA(CSimpleFrame::GetObjectType())) {
            luaL_error(L, "%s:SetScrollChild(): Wrong child object type, expected frame", scrollFrame->GetDisplayName());
            return 0;
        }
    }

    if (!child) {
        const char* name = scrollFrame->GetName();
        luaL_error(L, "%s:SetScrollChild(): Couldn't find frame named '%s'", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (!child->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(scrollFrame);
        }

        return 0;
    }

    int32_t depth = 0;

    for (CSimpleFrame* frame = scrollFrame; frame; frame = frame->m_parent) {
        if (frame == child) {
            const char* childName = child->GetName();
            const char* name = scrollFrame->GetName();
            luaL_error(L, "%s:SetScrollChild(): Would create a loop adding child %s", name ? name : "<unnamed>", childName ? childName : "<unnamed>");
            return 0;
        }

        depth++;
    }

    if (depth + CSimpleFrame_GetTreeHeight(child) > SIMPLEFRAME_MAX_DEPTH) {
        luaL_error(L, "%s:SetScrollChild(): Frame hierarchy would be too deep", scrollFrame->GetDisplayName());
        return 0;
    }

    scrollFrame->SetScrollChild(child);
    return 0;
}

// VERIFY: 75857c97 manual
// OFFSET: 0x00972410
static int32_t Script_GetScrollChild(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));
    CSimpleFrame* child = scrollFrame->m_scrollChild;

    if (!child) {
        lua_pushnil(L);
        return 1;
    }

    if (!child->m_luaRegistered) {
        child->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, child->m_luaObjectRef);
    return 1;
}

// VERIFY: b554b7b2 auto
// OFFSET: 0x00972490
static int32_t Script_SetHorizontalScroll(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));

    if (!scrollFrame->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(scrollFrame);
        }

        return 0;
    }

    if (lua_isnumber(L, 2)) {
        float offset = static_cast<float>(lua_tonumber(L, 2));
        scrollFrame->SetHorizontalScroll(NDCToDDCWidth(offset / (CoordinateGetAspectCompensation() * 1024.0f)));
        return 0;
    }

    const char* name = scrollFrame->GetName();
    luaL_error(L, "Usage: %s:SetHorizontalScroll(offset)", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 3469e010 auto
// OFFSET: 0x00972560
static int32_t Script_SetVerticalScroll(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));

    if (!scrollFrame->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(scrollFrame);
        }

        return 0;
    }

    if (lua_isnumber(L, 2)) {
        float offset = static_cast<float>(lua_tonumber(L, 2));
        scrollFrame->SetVerticalScroll(NDCToDDCWidth(offset / (CoordinateGetAspectCompensation() * 1024.0f)));
        return 0;
    }

    const char* name = scrollFrame->GetName();
    luaL_error(L, "Usage: %s:SetVerticalScroll(offset)", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 30b3f47c manual
// OFFSET: 0x00972630
static int32_t Script_GetHorizontalScroll(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * scrollFrame->m_scrollOffset.x));
    return 1;
}

// VERIFY: e13915c9 manual
// OFFSET: 0x00972690
static int32_t Script_GetVerticalScroll(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * scrollFrame->m_scrollOffset.y));
    return 1;
}

// VERIFY: c56f5f68 manual
// OFFSET: 0x009726F0
static int32_t Script_GetHorizontalScrollRange(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * scrollFrame->m_scrollRange.x));
    return 1;
}

// VERIFY: 3492a4da manual
// OFFSET: 0x00972750
static int32_t Script_GetVerticalScrollRange(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * scrollFrame->m_scrollRange.y));
    return 1;
}

// VERIFY: 02c2e551 manual
// OFFSET: 0x009727B0
static int32_t Script_UpdateScrollChildRect(lua_State* L) {
    CSimpleScrollFrame* scrollFrame = static_cast<CSimpleScrollFrame*>(FrameScript_GetObjectThis(L, CSimpleScrollFrame::GetObjectType()));

    scrollFrame->m_scrollChildUpdate = 1;
    return 0;
}

// VERIFY: bef254f0 auto
// OFFSET: 0x009721E0
void CSimpleScrollFrame::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleScrollFrame::s_metatable);
    CSimpleScrollFrame::s_metatable = LUA_NOREF;
}

// VERIFY: a912e40f auto
// OFFSET: 0x009727F0
void CSimpleScrollFrame::CreateScriptMetaTable() {
    CSimpleScrollFrame::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleScrollFrame::RegisterScriptMethods);
}

// STATIC: 0x00B2CE90
static FrameScript_Method s_ScriptMethods[] = {
    { "SetScrollChild", &Script_SetScrollChild },
    { "GetScrollChild", &Script_GetScrollChild },
    { "SetHorizontalScroll", &Script_SetHorizontalScroll },
    { "SetVerticalScroll", &Script_SetVerticalScroll },
    { "GetHorizontalScroll", &Script_GetHorizontalScroll },
    { "GetVerticalScroll", &Script_GetVerticalScroll },
    { "GetHorizontalScrollRange", &Script_GetHorizontalScrollRange },
    { "GetVerticalScrollRange", &Script_GetVerticalScrollRange },
    { "UpdateScrollChildRect", &Script_UpdateScrollChildRect },
};

// VERIFY: 144bac6f auto
// OFFSET: 0x009721C0
void CSimpleScrollFrame::RegisterScriptMethods(lua_State* L) {
    CSimpleFrame::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
