#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/SimpleFrameScript.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CSimpleAlphaAnim.h"
#include "Engine/Source/Frame/CSimpleAnim.h"
#include "Engine/Source/Frame/CSimpleAnimGroup.h"
#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Frame/CSimpleCheckbox.h"
#include "Engine/Source/Frame/CSimpleColorSelect.h"
#include "Engine/Source/Frame/CSimpleControlPoint.h"
#include "Engine/Source/Frame/CSimpleEditBox.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleHTML.h"
#include "Engine/Source/Frame/CSimpleMessageFrame.h"
#include "Engine/Source/Frame/CSimpleMessageScrollFrame.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "Engine/Source/Frame/CSimpleMovieFrame.h"
#include "Engine/Source/Frame/CSimplePathAnim.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleRotationAnim.h"
#include "Engine/Source/Frame/CSimpleScaleAnim.h"
#include "Engine/Source/Frame/CSimpleScrollFrame.h"
#include "Engine/Source/Frame/CSimpleSlider.h"
#include "Engine/Source/Frame/CSimpleStatusBar.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CSimpleTitleRegion.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CSimpleTranslationAnim.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"

// VERIFY: 98c4dac9 auto
// OFFSET: 0x0081B720
static int32_t Script_GetText(lua_State* L) {
    const char* token = lua_tolstring(L, 1, nullptr);
    int32_t ordinal = -1;
    FRAMESCRIPT_GENDER gender = GENDER_NOT_APPLICABLE;

    if (!token) {
        luaL_error(L, "Usage: GetText(\"token\" [,gender] [,ordinal])");
    }

    if (lua_isnumber(L, 2)) {
        gender = static_cast<FRAMESCRIPT_GENDER>(lua_tointeger(L, 2));
    }

    if (lua_isnumber(L, 3)) {
        ordinal = lua_tointeger(L, 3);
    }

    lua_pushstring(L, FrameScript_GetText(token, ordinal, gender));
    return 1;
}

// VERIFY: 5d8fa6da auto
// OFFSET: 0x0081B7B0
static int32_t Script_CreateFont(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: CreateFont(\"name\")");
        return 0;
    }

    CSimpleFont* font = CSimpleFont::GetFont(lua_tolstring(L, 1, nullptr), 1);

    if (!font->m_luaRegistered) {
        font->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, font->m_luaObjectRef);
    return 1;
}

// VERIFY: 9415d77e auto
// OFFSET: 0x0081B820
static int32_t Script_GetCurrentKeyBoardFocus(lua_State* L) {
    CSimpleEditBox* focus = CSimpleEditBox::s_currentFocus;

    if (!focus) {
        lua_pushnil(L);
        return 1;
    }

    if (!focus->m_luaRegistered) {
        focus->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, focus->m_luaObjectRef);
    return 1;
}

// VERIFY: e9ec2bb1 manual
// OFFSET: 0x0081B9C0
static int32_t Script_EnumerateFrames(lua_State* L) {
    CSimpleFrame* frame;

    if (lua_type(L, 1) == LUA_TTABLE) {
        lua_rawgeti(L, 1, 0);
        CSimpleFrame* current = static_cast<CSimpleFrame*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!current) {
            luaL_error(L, "EnumerateFrames: Couldn't find 'this' in current object");
            return 0;
        }

        if (!current->IsA(CSimpleFrame::GetObjectType())) {
            luaL_error(L, "EnumerateFrames: Wrong current object type, expected frame");
            return 0;
        }

        frame = current->m_framesLink.Next();
    } else {
        frame = CSimpleTop::s_instance->m_frames.Head();
    }

    if (!frame) {
        lua_pushnil(L);
        return 1;
    }

    if (!frame->m_luaRegistered) {
        frame->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, frame->m_luaObjectRef);
    return 1;
}

// VERIFY: f2be837e manual
// OFFSET: 0x0081BAB0
static int32_t Script_GetNumFrames(lua_State* L) {
    uint32_t count = 0;

    for (CSimpleFrame* frame = CSimpleTop::s_instance->m_frames.Head(); frame; frame = CSimpleTop::s_instance->m_frames.Next(frame)) {
        count++;
    }

    lua_pushnumber(L, count);
    return 1;
}

// VERIFY: afd92dee manual
// OFFSET: 0x0081BB20
static int32_t Script_CreateFrame(lua_State* L) {
    if (!lua_isstring(L, 1) || (lua_type(L, 3) != LUA_TNONE && lua_type(L, 3) != LUA_TNIL && lua_type(L, 3) != LUA_TTABLE)) {
        luaL_error(L, "Usage: CreateFrame(\"frameType\" [, \"name\"] [, parent] [, \"template\"] [, id])");
        return 0;
    }

    CSimpleFrame* parent = nullptr;
    const char* frameType = lua_tolstring(L, 1, nullptr);
    const char* name = lua_tolstring(L, 2, nullptr);
    const char* inherits = lua_tolstring(L, 4, nullptr);

    if (lua_type(L, 3) == LUA_TTABLE) {
        lua_rawgeti(L, 3, 0);
        parent = static_cast<CSimpleFrame*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!parent) {
            luaL_error(L, "CreateFrame: Couldn't find 'this' in parent object");
            return 0;
        }

        if (!parent->IsA(CSimpleFrame::GetObjectType())) {
            luaL_error(L, "CreateFrame: Wrong parent object type, expected frame");
            return 0;
        }

        int32_t depth = 1;

        for (CSimpleFrame* ancestor = parent; ancestor; ancestor = ancestor->m_parent) {
            depth++;
        }

        if (depth > SIMPLEFRAME_MAX_DEPTH) {
            luaL_error(L, "CreateFrame: Frame hierarchy would be too deep");
            return 0;
        }
    }

    if (lua_type(L, 4) == LUA_TSTRING) {
        inherits = lua_tolstring(L, 4, nullptr);

        char inheritName[1024];

        do {
            SStrTokenize(&inherits, inheritName, sizeof(inheritName), " ,", nullptr);

            if (!inheritName[0]) {
                break;
            }

            const char* taint;
            bool locked;

            if (!FrameXML_AcquireHashNode(inheritName, taint, locked)) {
                luaL_error(L, "CreateFrame(): Couldn't find inherited node \"%s\"", inheritName);
                return 0;
            }

            if (locked) {
                luaL_error(L, "CreateFrame(): Recursively inherited node \"%s\"", inheritName);
                return 0;
            }

            FrameXML_ReleaseHashNode(inheritName);
        } while (inheritName[0]);

        inherits = lua_tolstring(L, 4, nullptr);
    }

    XMLNode node(nullptr, frameType);

    if (name) {
        node.SetAttribute("name", name);
    }

    if (parent) {
        node.SetAttribute("parent", "");
    }

    if (inherits) {
        node.SetAttribute("inherits", inherits);
    }

    if (lua_isstring(L, 5)) {
        node.SetAttribute("id", lua_tolstring(L, 5, nullptr));
    } else if (lua_isnumber(L, 5)) {
        char id[256];
        SStrPrintf(id, sizeof(id), "%d", lua_tointeger(L, 5));
        node.SetAttribute("id", id);
    }

    CStatus status;
    CSimpleFrame* frame = FrameXML_CreateFrame(&node, parent, &status);

    if (!frame) {
        luaL_error(L, "CreateFrame: Unknown frame type '%s'", frameType);
        return 0;
    }

    if (!frame->m_luaRegistered) {
        frame->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, frame->m_luaObjectRef);
    return 1;
}

// Listeners waiting on a deferred unregister may already be destroyed.
static bool IsPendingUnregister(FrameScript_EventObject* event, FrameScript_Object* listener) {
    for (EVENTLISTENERNODE* node = event->m_unregisterListeners.Head(); !event->m_unregisterListeners.IsEnd(node); node = event->m_unregisterListeners.RawNext(node)) {
        if (node->m_listener == listener) {
            return true;
        }
    }

    return false;
}

// VERIFY: 6e83215b manual
// OFFSET: 0x0081BE70
static int32_t Script_GetFramesRegisteredForEvent(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: GetFramesRegisteredForEvent(\"event\")");
        return 0;
    }

    FrameScript_EventObject* event = s_scriptEventsHash.Ptr(lua_tolstring(L, 1, nullptr));

    if (!event) {
        return 0;
    }

    TSList<EVENTLISTENERNODE>* listeners = &event->m_listeners;
    int32_t count = 0;

    for (EVENTLISTENERNODE* node = listeners->Head(); node; node = listeners->Next(node)) {
        if (!IsPendingUnregister(event, node->m_listener)) {
            count++;
        }
    }

    if (!lua_checkstack(L, count)) {
        luaL_error(L, "GetFramesRegisteredForEvent(%s): Stack overflow", lua_tolstring(L, 1, nullptr));
        return 0;
    }

    for (EVENTLISTENERNODE* node = listeners->Head(); node; node = listeners->Next(node)) {
        FrameScript_Object* listener = node->m_listener;

        if (IsPendingUnregister(event, listener)) {
            continue;
        }

        if (!listener->m_luaRegistered) {
            listener->RegisterScriptObject(nullptr);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, listener->m_luaObjectRef);
    }

    return count;
}

// STATIC: 0x00AF5848
static FrameScript_Method s_ScriptFunctions[] = {
    { "GetText", &Script_GetText },
    { "GetNumFrames", &Script_GetNumFrames },
    { "EnumerateFrames", &Script_EnumerateFrames },
    { "CreateFont", &Script_CreateFont },
    { "CreateFrame", &Script_CreateFrame },
    { "GetFramesRegisteredForEvent", &Script_GetFramesRegisteredForEvent },
    { "GetCurrentKeyBoardFocus", &Script_GetCurrentKeyBoardFocus },
};

// VERIFY: 95d66d5a auto
// OFFSET: 0x0081B870
void RegisterSimpleFrameScriptMethods() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_ScriptFunctions[i].m_name, s_ScriptFunctions[i].m_method);
    }

    CSimpleAnim::CreateScriptMetaTable();
    CSimpleTranslationAnim::CreateScriptMetaTable();
    CSimpleRotationAnim::CreateScriptMetaTable();
    CSimpleScaleAnim::CreateScriptMetaTable();
    CSimpleControlPoint::CreateScriptMetaTable();
    CSimplePathAnim::CreateScriptMetaTable();
    CSimpleAlphaAnim::CreateScriptMetaTable();
    CSimpleAnimGroup::CreateScriptMetaTable();
    CSimpleFont::CreateScriptMetaTable();
    CSimpleTexture::CreateScriptMetaTable();
    CSimpleFontString::CreateScriptMetaTable();
    CSimpleTitleRegion::CreateScriptMetaTable();
    CSimpleFrame::CreateScriptMetaTable();
    CSimpleButton::CreateScriptMetaTable();
    CSimpleCheckbox::CreateScriptMetaTable();
    CSimpleEditBox::CreateScriptMetaTable();
    CSimpleHTML::CreateScriptMetaTable();
    CSimpleMessageFrame::CreateScriptMetaTable();
    CSimpleMessageScrollFrame::CreateScriptMetaTable();
    CSimpleModel::CreateScriptMetaTable();
    CSimpleModelFFX::CreateScriptMetaTable();
    CSimpleScrollFrame::CreateScriptMetaTable();
    CSimpleSlider::CreateScriptMetaTable();
    CSimpleStatusBar::CreateScriptMetaTable();
    CSimpleColorSelect::CreateScriptMetaTable();
    CSimpleMovieFrame::CreateScriptMetaTable();
}

// VERIFY: 10a8f5da manual
// OFFSET: 0x0081B920
void UnregisterSimpleFrameScriptMethods() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_ScriptFunctions[i].m_name);
    }

    CSimpleAnim::DestroyScriptMetaTable();
    CSimpleTranslationAnim::DestroyScriptMetaTable();
    CSimpleRotationAnim::DestroyScriptMetaTable();
    CSimpleScaleAnim::DestroyScriptMetaTable();
    CSimpleControlPoint::DestroyScriptMetaTable();
    CSimplePathAnim::DestroyScriptMetaTable();
    CSimpleAlphaAnim::DestroyScriptMetaTable();
    CSimpleAnimGroup::DestroyScriptMetaTable();
    CSimpleFont::DestroyScriptMetaTable();
    CSimpleTexture::DestroyScriptMetaTable();
    CSimpleFontString::DestroyScriptMetaTable();
    CSimpleTitleRegion::DestroyScriptMetaTable();
    CSimpleFrame::DestroyScriptMetaTable();
    CSimpleButton::DestroyScriptMetaTable();
    CSimpleCheckbox::DestroyScriptMetaTable();
    CSimpleEditBox::DestroyScriptMetaTable();
    CSimpleHTML::DestroyScriptMetaTable();
    CSimpleMessageFrame::DestroyScriptMetaTable();
    CSimpleMessageScrollFrame::DestroyScriptMetaTable();
    CSimpleModel::DestroyScriptMetaTable();
    CSimpleScrollFrame::DestroyScriptMetaTable();
    CSimpleSlider::DestroyScriptMetaTable();
    CSimpleStatusBar::DestroyScriptMetaTable();
    CSimpleColorSelect::DestroyScriptMetaTable();
    CSimpleMovieFrame::DestroyScriptMetaTable();
}
