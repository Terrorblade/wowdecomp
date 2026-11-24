#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CFramePoint.h"
#include "Engine/Source/Frame/CSimpleAnimGroup.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CRect.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 87c35ea2 auto
// OFFSET: 0x0049CAD0
static int32_t Script_IsProtected(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (region->IsProtected()) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    if (region->m_layoutFlags & CLayoutFrame::LAYOUT_EXPLICITLY_PROTECTED) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 2;
}

// VERIFY: 8bcf8745 auto
// OFFSET: 0x0049CB60
static int32_t Script_CanChangeProtectedState(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (!region->IsProtected() || CSimpleTop::s_instance->m_allowProtectedActions) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 2ca75d21 manual
// OFFSET: 0x0049CBD0
static int32_t Script_SetParent(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (!region->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(region);
            return 0;
        }

        return 0;
    }

    if ((region->IsA(CSimpleFontString::GetObjectType()) || region->IsA(CSimpleTexture::GetObjectType())) && static_cast<CSimpleRegion*>(region)->m_internal) {
        luaL_error(L, "%s:SetParent(): Cannot change the parent of this region", region->GetDisplayName());
        return 0;
    }

    if (lua_type(L, 2) != LUA_TNIL) {
        CSimpleFrame* parent = nullptr;

        if (lua_isstring(L, 2)) {
            parent = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName(lua_tolstring(L, 2, nullptr), CSimpleFrame::GetObjectType()));
        } else if (lua_type(L, 2) == LUA_TTABLE) {
            lua_rawgeti(L, 2, 0);
            parent = static_cast<CSimpleFrame*>(lua_touserdata(L, -1));
            lua_settop(L, -2);

            if (!parent) {
                luaL_error(L, "%s:SetParent(): Couldn't find 'this' in parent object", region->GetDisplayName());
                return 0;
            }

            if (!parent->IsA(CSimpleFrame::GetObjectType())) {
                luaL_error(L, "%s:SetParent(): Wrong parent object type, expected Frame", region->GetDisplayName());
                return 0;
            }
        }

        if (parent) {
            int32_t depth = 0;

            for (CSimpleFrame* ancestor = parent; ancestor; ancestor = ancestor->m_parent) {
                if (ancestor == region) {
                    const char* parentName = parent->GetName();
                    const char* regionName = region->GetName();
                    luaL_error(L, "%s:SetParent(): Would create a loop parenting to %s", regionName ? regionName : "<unnamed>", parentName ? parentName : "<unnamed>");
                    return 0;
                }

                depth++;
            }

            if (region->IsA(CSimpleFrame::GetObjectType())) {
                depth += CSimpleFrame_GetTreeHeight(static_cast<CSimpleFrame*>(region));
            }

            if (depth > SIMPLEFRAME_MAX_DEPTH) {
                luaL_error(L, "%s:SetParent(): Frame hierarchy would be too deep", region->GetDisplayName());
                return 0;
            }

            region->SetParent(parent);
            return 0;
        }

        const char* name = region->GetName();
        luaL_error(L, "%s:SetParent(): Couldn't find region named '%s'", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (region->IsA(CSimpleFontString::GetObjectType()) || region->IsA(CSimpleTexture::GetObjectType())) {
        const char* name = region->GetName();
        luaL_error(L, "%s:SetParent(): Cannot set a 'nil' parent for fonts or textures", name ? name : "<unnamed>");
        return 0;
    }

    region->SetParent(nullptr);
    return 0;
}

// VERIFY: 693187b6 auto
// OFFSET: 0x0049CE50
static int32_t Script_GetRect(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (region->IsResizePending()) {
        region->Resize(1);
    }

    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!region->GetRect(&rect)) {
        return 0;
    }

    float scale = 1.0f / region->m_layoutScale;
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * scale * rect.minX));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * scale * rect.minY));
    float width = rect.maxX - rect.minX;
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * scale * width));
    float height = rect.maxY - rect.minY;
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * scale * height));

    return 4;
}

// VERIFY: dfe86282 auto
// OFFSET: 0x0049CF90
static int32_t Script_GetCenter(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (region->IsResizePending()) {
        region->Resize(1);
    }

    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (region->GetRect(&rect)) {
        float width = rect.maxX - rect.minX;
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * ((width * 0.5f + rect.minX) / region->m_layoutScale)));
        float height = rect.maxY - rect.minY;
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * ((height * 0.5f + rect.minY) / region->m_layoutScale)));
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
    }

    return 2;
}

// VERIFY: 199d6222 auto
// OFFSET: 0x0049D0B0
static int32_t Script_GetLeft(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (region->IsResizePending()) {
        region->Resize(1);
    }

    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (region->GetRect(&rect)) {
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f / region->m_layoutScale * rect.minX));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 3bdcea8b auto
// OFFSET: 0x0049D170
static int32_t Script_GetRight(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (region->IsResizePending()) {
        region->Resize(1);
    }

    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (region->GetRect(&rect)) {
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f / region->m_layoutScale * rect.maxX));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: ce1e534a auto
// OFFSET: 0x0049D230
static int32_t Script_GetTop(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (region->IsResizePending()) {
        region->Resize(1);
    }

    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (region->GetRect(&rect)) {
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f / region->m_layoutScale * rect.maxY));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: e2374169 auto
// OFFSET: 0x0049D2F0
static int32_t Script_GetBottom(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (region->IsResizePending()) {
        region->Resize(1);
    }

    CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (region->GetRect(&rect)) {
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f / region->m_layoutScale * rect.minY));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: d8d4d2e0 auto
// OFFSET: 0x0049D3B0
static int32_t Script_GetWidth(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    float width = region->GetWidth();

    if (0.0f == width && !StringToBOOL(L, 2, 0)) {
        if (region->IsResizePending()) {
            region->Resize(1);
        }

        CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (region->GetRect(&rect)) {
            width = (rect.maxX - rect.minX) / region->m_layoutScale;
        }
    }

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * width));
    return 1;
}

// VERIFY: 0f79f03a manual
// OFFSET: 0x0049D480
static int32_t Script_SetWidth(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (!region->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(region);
        }

        return 0;
    }

    if (!lua_isnumber(L, 2)) {
        const char* name = region->GetName();
        luaL_error(L, "Usage: %s:SetWidth(width)", name ? name : "<unnamed>");
        return 0;
    }

    float width = static_cast<float>(lua_tonumber(L, 2));
    region->SetWidth(NDCToDDCWidth(width / (CoordinateGetAspectCompensation() * 1024.0f)));
    return 0;
}

// VERIFY: a27bf181 auto
// OFFSET: 0x0049D550
static int32_t Script_GetHeight(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    float height = region->GetHeight();

    if (0.0f == height && !StringToBOOL(L, 2, 0)) {
        if (region->IsResizePending()) {
            region->Resize(1);
        }

        CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (region->GetRect(&rect)) {
            height = (rect.maxY - rect.minY) / region->m_layoutScale;
        }
    }

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * height));
    return 1;
}

// VERIFY: 9529267e manual
// OFFSET: 0x0049D620
static int32_t Script_SetHeight(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (!region->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(region);
        }

        return 0;
    }

    if (!lua_isnumber(L, 2)) {
        const char* name = region->GetName();
        luaL_error(L, "Usage: %s:SetHeight(height)", name ? name : "<unnamed>");
        return 0;
    }

    float height = static_cast<float>(lua_tonumber(L, 2));
    region->SetHeight(NDCToDDCWidth(height / (CoordinateGetAspectCompensation() * 1024.0f)));
    return 0;
}

// VERIFY: 58fe04bd manual
// OFFSET: 0x0049D6F0
static int32_t Script_GetSize(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    int32_t ignoreRect = StringToBOOL(L, 2, 0);

    if (!region) {
        return 0;
    }

    float width;
    float height;
    region->GetSize(width, height, ignoreRect);

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * width));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * height));
    return 2;
}

// VERIFY: b1bd0e85 manual
// OFFSET: 0x0049D7A0
static int32_t Script_SetSize(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (!region->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(region);
        }

        return 0;
    }

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = region->GetName();
        luaL_error(L, "Usage: %s:SetSize(width, height)", name ? name : "<unnamed>");
        return 0;
    }

    float height = static_cast<float>(lua_tonumber(L, 3));
    float width = static_cast<float>(lua_tonumber(L, 2));
    float ddcHeight = NDCToDDCWidth(height / (CoordinateGetAspectCompensation() * 1024.0f));
    float ddcWidth = NDCToDDCWidth(width / (CoordinateGetAspectCompensation() * 1024.0f));
    region->SetSize(ddcWidth, ddcHeight);
    return 0;
}

// VERIFY: 595625de manual
// OFFSET: 0x0049D8B0
static int32_t Script_GetNumPoints(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    int32_t count = 0;

    for (int32_t i = 0; i < FRAMEPOINT_NUMPOINTS; i++) {
        CFramePoint* framePoint = region->m_framePoints[i];

        if (framePoint && !(framePoint->m_flags & CFramePoint::FLAG_UNUSED)) {
            count++;
        }
    }

    lua_pushnumber(L, count);
    return 1;
}

// VERIFY: f0ad925e manual
// OFFSET: 0x0049D950
static int32_t Script_GetPoint(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    int32_t index = 0;

    if (lua_isnumber(L, 2)) {
        index = static_cast<int32_t>(lua_tonumber(L, 2)) - 1;
    }

    int32_t found = 0;
    int32_t point;
    CFramePoint* framePoint = nullptr;

    for (point = 0; point < FRAMEPOINT_NUMPOINTS; point++) {
        framePoint = region->m_framePoints[point];

        if (framePoint && !(framePoint->m_flags & CFramePoint::FLAG_UNUSED)) {
            if (found >= index) {
                break;
            }

            found++;
        }
    }

    if (point >= FRAMEPOINT_NUMPOINTS) {
        return 0;
    }

    if (framePoint->m_flags & CFramePoint::FLAG_ABSOLUTE_COORDINATES) {
        lua_pushstring(L, FramePointToString(static_cast<FRAMEPOINT>(point)));
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * framePoint->m_x));
        lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * framePoint->m_y));
        return 3;
    }

    CLayoutFrame* relative = framePoint->m_relative;
    lua_pushstring(L, FramePointToString(static_cast<FRAMEPOINT>(point)));

    if (relative == CSimpleTop::s_instance) {
        lua_pushnil(L);
    } else {
        CScriptRegion* relativeRegion = static_cast<CScriptRegion*>(relative);

        if (!relativeRegion->m_luaRegistered) {
            relativeRegion->RegisterScriptObject(nullptr);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, relativeRegion->m_luaObjectRef);
    }

    lua_pushstring(L, FramePointToString(framePoint->m_framePoint));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * framePoint->m_x));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * framePoint->m_y));
    return 5;
}

// VERIFY: bfe26544 auto
// OFFSET: 0x0049DB10
static int32_t Script_SetPoint(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (!region->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(region);
            return 0;
        }

        return 0;
    }

    if (!lua_isstring(L, 2)) {
        const char* name = region->GetName();
        luaL_error(L, "Usage: %s:SetPoint(\"point\" [, region or nil] [, \"relativePoint\"] [, offsetX, offsetY])", name ? name : "<unnamed>");
        return 0;
    }

    CLayoutFrame* relative = region->GetLayoutParent();
    float offsetX = 0.0f;
    FRAMEPOINT point;

    if (!StringToFramePoint(lua_tolstring(L, 2, nullptr), point)) {
        const char* name = region->GetName();
        luaL_error(L, "%s:SetPoint(): Unknown region point", name ? name : "<unnamed>");
        return 0;
    }

    int32_t argIndex = 3;
    FRAMEPOINT relativePoint = point;

    if (lua_type(L, 3) == LUA_TSTRING) {
        relative = region->GetLayoutFrameByName(lua_tolstring(L, 3, nullptr));
        argIndex = 4;
    } else if (lua_type(L, 3) == LUA_TTABLE) {
        lua_rawgeti(L, 3, 0);
        FrameScript_Object* object = static_cast<FrameScript_Object*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
        relative = object && object->IsA(CScriptRegion::GetObjectType()) ? static_cast<CScriptRegion*>(object) : nullptr;
        argIndex = 4;
    } else if (lua_type(L, 3) == LUA_TNIL) {
        relative = CSimpleTop::s_instance;
        argIndex = 4;
    }

    if (!relative) {
        const char* name = region->GetName();
        luaL_error(L, "%s:SetPoint(): Couldn't find region named '%s'", name ? name : "<unnamed>", lua_tolstring(L, 3, nullptr));
        return 0;
    }

    if (relative == region) {
        luaL_error(L, "%s:SetPoint(): trying to anchor to itself", region->GetDisplayName());
        return 0;
    }

    if (relative->IsResizeDependency(region)) {
        luaL_error(L, "%s:SetPoint(): %s is dependent on this", region->GetDisplayName(), static_cast<CScriptRegion*>(relative)->GetDisplayName());
        return 0;
    }

    if (lua_type(L, argIndex) == LUA_TSTRING) {
        if (!StringToFramePoint(lua_tolstring(L, argIndex, nullptr), relativePoint)) {
            luaL_error(L, "%s:SetPoint(): Unknown region point", region->GetDisplayName());
            return 0;
        }

        argIndex++;
    }

    float offsetY;

    if (lua_isnumber(L, argIndex) && lua_isnumber(L, argIndex + 1)) {
        offsetX = static_cast<float>(lua_tonumber(L, argIndex));
        offsetX = NDCToDDCWidth(offsetX / (CoordinateGetAspectCompensation() * 1024.0f));
        offsetY = static_cast<float>(lua_tonumber(L, argIndex + 1));
        offsetY = NDCToDDCWidth(offsetY / (CoordinateGetAspectCompensation() * 1024.0f));
    } else {
        offsetY = 0.0f;
    }

    region->SetPoint(point, relative, relativePoint, offsetX, offsetY, 1);
    return 0;
}

// VERIFY: a2872688 auto
// OFFSET: 0x0049DE40
static int32_t Script_SetAllPoints(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (!region->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(region);
            return 0;
        }

        return 0;
    }

    CLayoutFrame* relative = region->GetLayoutParent();

    if (lua_isstring(L, 2)) {
        relative = region->GetLayoutFrameByName(lua_tolstring(L, 2, nullptr));
    } else if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        FrameScript_Object* object = static_cast<FrameScript_Object*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
        relative = object && object->IsA(CScriptRegion::GetObjectType()) ? static_cast<CScriptRegion*>(object) : nullptr;
    } else if (lua_type(L, 2) == LUA_TNIL) {
        relative = CSimpleTop::s_instance;
    }

    if (!relative) {
        const char* name = region->GetName();
        luaL_error(L, "%s:SetAllPoints(): Couldn't find region named '%s'", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (relative == region) {
        const char* name = region->GetName();
        luaL_error(L, "%s:SetAllPoints(): trying to anchor to itself", name ? name : "<unnamed>");
        return 0;
    }

    if (relative->IsResizeDependency(region)) {
        const char* relativeName = static_cast<CScriptRegion*>(relative)->GetName();
        const char* name = region->GetName();
        luaL_error(L, "%s:SetAllPoints(): %s is dependent on this", name ? name : "<unnamed>", relativeName ? relativeName : "<unnamed>");
        return 0;
    }

    region->SetAllPoints(relative, 1);
    return 0;
}

// VERIFY: a010b1bc auto
// OFFSET: 0x0049E010
static int32_t Script_ClearAllPoints(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));

    if (!region->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(region);
            return 0;
        }

        return 0;
    }

    region->MarkFramePointsUnused();
    return 0;
}

// VERIFY: 830a3e47 manual
// OFFSET: 0x0049E070
static int32_t Script_StopAnimating(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    region->StopAnimating();
    return 0;
}

// VERIFY: afda47e5 manual
// OFFSET: 0x0049E0B0
static int32_t Script_IsDragging(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    lua_pushboolean(L, region->IsDragging());
    return 1;
}

// VERIFY: cad49304 auto
// OFFSET: 0x0049E100
static int32_t Script_IsMouseOver(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    float top = lua_isnumber(L, 2) ? static_cast<float>(lua_tonumber(L, 2)) : 0.0f;
    float bottom = lua_isnumber(L, 3) ? static_cast<float>(lua_tonumber(L, 3)) : 0.0f;
    float left = lua_isnumber(L, 4) ? static_cast<float>(lua_tonumber(L, 4)) : 0.0f;
    float right = lua_isnumber(L, 5) ? static_cast<float>(lua_tonumber(L, 5)) : 0.0f;

    top = NDCToDDCWidth(top / (CoordinateGetAspectCompensation() * 1024.0f));
    bottom = NDCToDDCWidth(bottom / (CoordinateGetAspectCompensation() * 1024.0f));
    left = NDCToDDCWidth(left / (CoordinateGetAspectCompensation() * 1024.0f));
    right = NDCToDDCWidth(right / (CoordinateGetAspectCompensation() * 1024.0f));

    lua_pushboolean(L, region->IsMouseOver(top, bottom, left, right));
    return 1;
}

// VERIFY: 3092723d manual
// OFFSET: 0x0049E260
static int32_t Script_GetAnimationGroups(lua_State* L) {
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    TSExplicitList<CSimpleAnimGroup, offsetof(CSimpleAnimGroup, m_regionLink)>* groups = region->m_animGroups;

    if (!groups) {
        return 0;
    }

    int32_t count = 0;

    for (CSimpleAnimGroup* group = groups->Head(); group; group = groups->Next(group)) {
        count++;
    }

    if (!lua_checkstack(L, count)) {
        const char* name = region->GetName();
        luaL_error(L, "%s:GetAnimationGroups(): Stack overflow", name ? name : "<unnamed>");
        return 0;
    }

    for (CSimpleAnimGroup* group = groups->Head(); group; group = groups->Next(group)) {
        if (!group->m_luaRegistered) {
            group->RegisterScriptObject(nullptr);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, group->m_luaObjectRef);
    }

    return count;
}

// VERIFY: 12c26586 manual
// OFFSET: 0x0049E350
static int32_t Script_CreateAnimationGroup(lua_State* L) {
    XMLNode* inheritNode = nullptr;
    CScriptRegion* region = static_cast<CScriptRegion*>(FrameScript_GetObjectThis(L, CScriptRegion::GetObjectType()));
    const char* name = nullptr;

    if ((region->IsA(CSimpleFontString::GetObjectType()) || region->IsA(CSimpleTexture::GetObjectType())) && static_cast<CSimpleRegion*>(region)->m_internal) {
        luaL_error(L, "%s:CreateAnimationGroup(): Cannot animate this region", region->GetDisplayName());
        return 0;
    }

    if (lua_isstring(L, 2)) {
        name = lua_tolstring(L, 2, nullptr);
    }

    if (lua_type(L, 3) == LUA_TSTRING) {
        const char* inherits = lua_tolstring(L, 3, nullptr);
        const char* taint;
        bool locked;
        inheritNode = FrameXML_AcquireHashNode(inherits, taint, locked);

        if (!inheritNode) {
            const char* regionName = region->GetName();
            luaL_error(L, "%s:CreateAnimationGroup(): Couldn't find inherited node \"%s\"", regionName ? regionName : "<unnamed>", inherits);
            return 0;
        }

        if (locked) {
            const char* regionName = region->GetName();
            luaL_error(L, "%s:CreateAnimationGroup(): Recursively inherited node \"%s\"", regionName ? regionName : "<unnamed>", inherits);
            return 0;
        }
    }

    CSimpleAnimGroup* group = new (SMemAlloc(sizeof(CSimpleAnimGroup), __FILE__, __LINE__, 0)) CSimpleAnimGroup(region);

    if (name && *name) {
        group->SetName(name);
    }

    if (inheritNode) {
        CStatus status;
        group->LoadXML(inheritNode, &status);
        group->PostLoadXML(inheritNode, &status);
        FrameXML_ReleaseHashNode(lua_tolstring(L, 3, nullptr));
    }

    if (!group->m_luaRegistered) {
        group->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, group->m_luaObjectRef);
    return 1;
}

// STATIC: 0x00AC1480
static FrameScript_Method s_ScriptMethods[] = {
    { "IsProtected", &Script_IsProtected },
    { "CanChangeProtectedState", &Script_CanChangeProtectedState },
    { "SetParent", &Script_SetParent },
    { "GetRect", &Script_GetRect },
    { "GetCenter", &Script_GetCenter },
    { "GetLeft", &Script_GetLeft },
    { "GetRight", &Script_GetRight },
    { "GetTop", &Script_GetTop },
    { "GetBottom", &Script_GetBottom },
    { "GetWidth", &Script_GetWidth },
    { "SetWidth", &Script_SetWidth },
    { "GetHeight", &Script_GetHeight },
    { "SetHeight", &Script_SetHeight },
    { "SetSize", &Script_SetSize },
    { "GetSize", &Script_GetSize },
    { "GetNumPoints", &Script_GetNumPoints },
    { "GetPoint", &Script_GetPoint },
    { "SetPoint", &Script_SetPoint },
    { "SetAllPoints", &Script_SetAllPoints },
    { "ClearAllPoints", &Script_ClearAllPoints },
    { "CreateAnimationGroup", &Script_CreateAnimationGroup },
    { "GetAnimationGroups", &Script_GetAnimationGroups },
    { "StopAnimating", &Script_StopAnimating },
    { "IsDragging", &Script_IsDragging },
    { "IsMouseOver", &Script_IsMouseOver },
};

// VERIFY: 7348b001 auto
// OFFSET: 0x0049CA90
void CScriptRegion::RegisterScriptMethods(lua_State* L) {
    CScriptObject::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
