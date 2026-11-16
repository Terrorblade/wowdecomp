#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: db58dcf7 auto
// OFFSET: 0x00976FD0
static int32_t StringToButtonState(const char* string, CSimpleButtonState& state) {
    XMLENUMVALUE states[] = {
        { BUTTONSTATE_DISABLED, "DISABLED" },
        { BUTTONSTATE_NORMAL, "NORMAL" },
        { BUTTONSTATE_PUSHED, "PUSHED" },
    };

    for (uint32_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        if (!SStrCmpI(states[i].m_name, string, STORM_MAX_STR)) {
            state = static_cast<CSimpleButtonState>(states[i].m_value);
            return 1;
        }
    }

    return 0;
}

// VERIFY: edd3a0be auto
// OFFSET: 0x00977A70
static int32_t SetStateTexture(lua_State* L, CSimpleButtonState state, const char* method) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!texture) {
            const char* name = button->GetName();
            luaL_error(L, "%s:%s(): Couldn't find 'this' in texture", name ? name : "<unnamed>", method);
            return 0;
        }

        if (!texture->IsA(CSimpleTexture::GetObjectType()) || texture->m_internal) {
            const char* name = button->GetName();
            luaL_error(L, "%s:%s(): Wrong object type, expected texture", name ? name : "<unnamed>", method);
            return 0;
        }

        button->SetStateTexture(state, texture);
        return 0;
    }

    if (lua_isstring(L, 2)) {
        button->SetStateTexture(state, lua_tolstring(L, 2, nullptr));
        return 0;
    }

    if (lua_type(L, 2) != LUA_TNIL) {
        const char* name = button->GetName();
        luaL_error(L, "Usage: %s:%s(texture or \"texture\" or nil)", name ? name : "<unnamed>", method);
        return 0;
    }

    button->SetStateTexture(state, static_cast<CSimpleTexture*>(nullptr));
    return 0;
}

// VERIFY: 45fef3e2 manual
// OFFSET: 0x00977BF0
static int32_t GetStateTexture(lua_State* L, CSimpleButtonState state) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    CSimpleTexture* texture = button->m_stateTextures[state];

    if (!texture) {
        lua_pushnil(L);
        return 1;
    }

    if (!texture->m_luaRegistered) {
        texture->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, texture->m_luaObjectRef);
    return 1;
}

// VERIFY: 2a497fd9 auto
// OFFSET: 0x00977090
static int32_t Script_Enable(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    if (!button->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(button);
        }

        return 0;
    }

    button->Enable(1);
    return 0;
}

// VERIFY: b0dd1933 auto
// OFFSET: 0x00977100
static int32_t Script_Disable(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    if (!button->ProtectedFunctionsAllowed()) {
        if (CSimpleTop::s_instance->m_actionBlockedCallback) {
            CSimpleTop::s_instance->m_actionBlockedCallback(button);
        }

        return 0;
    }

    button->Enable(0);
    return 0;
}

// VERIFY: 376440d8 manual
// OFFSET: 0x00977170
static int32_t Script_IsEnabled(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    lua_pushnumber(L, button->m_state != BUTTONSTATE_DISABLED);
    return 1;
}

// VERIFY: 5acf429b auto
// OFFSET: 0x009771D0
static int32_t Script_GetButtonState(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    switch (button->m_state) {
        case BUTTONSTATE_DISABLED:
            lua_pushstring(L, "DISABLED");
            return 1;
        case BUTTONSTATE_NORMAL:
            lua_pushstring(L, "NORMAL");
            return 1;
        case BUTTONSTATE_PUSHED:
            lua_pushstring(L, "PUSHED");
            return 1;
        default:
            lua_pushstring(L, "UNKNOWN");
            return 1;
    }
}

// VERIFY: f22d21ec auto
// OFFSET: 0x00977280
static int32_t Script_SetButtonState(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    CSimpleButtonState state = BUTTONSTATE_DISABLED;

    if (!lua_isstring(L, 2) || !StringToButtonState(lua_tolstring(L, 2, nullptr), state)) {
        const char* name = button->GetName();
        luaL_error(L, "Usage: %s:SetButtonState(\"state\", lock)", name ? name : "<unnamed>");
        return 0;
    }

    button->SetButtonState(state, StringToBOOL(L, 3, 0));
    return 0;
}

// VERIFY: 5c9511a8 auto
// OFFSET: 0x00977340
static int32_t Script_SetNormalFontObject(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    CSimpleFont* font = nullptr;

    if (lua_type(L, 2) == LUA_TSTRING) {
        font = CSimpleFont::GetFont(lua_tolstring(L, 2, nullptr), 0);
    } else if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        font = static_cast<CSimpleFont*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
    }

    if (!button || !font || !font->IsA(CSimpleFont::GetObjectType())) {
        const char* name = button->GetName();
        luaL_error(L, "Usage: %s:SetNormalFontObject(\"fontname\" or fontObject)", name ? name : "<unnamed>");
        return 0;
    }

    button->m_normalFont = font;
    button->UpdateTextState(button->m_state);
    return 0;
}

// VERIFY: 8f419ab4 auto
// OFFSET: 0x00977450
static int32_t Script_GetNormalFontObject(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    if (!button || !button->m_normalFont) {
        return 0;
    }

    const char* name = button->GetName();
    return CSimpleFont_GetFontObject(name ? name : "<unnamed>", button->m_normalFont, L);
}

// VERIFY: 328d0f84 auto
// OFFSET: 0x009774C0
static int32_t Script_SetDisabledFontObject(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    CSimpleFont* font = nullptr;

    if (lua_type(L, 2) == LUA_TSTRING) {
        font = CSimpleFont::GetFont(lua_tolstring(L, 2, nullptr), 0);
    } else if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        font = static_cast<CSimpleFont*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
    }

    if (!button || !font || !font->IsA(CSimpleFont::GetObjectType())) {
        const char* name = button->GetName();
        luaL_error(L, "Usage: %s:SetDisabledFontObject(\"fontname\")", name ? name : "<unnamed>");
        return 0;
    }

    button->m_disabledFont = font;
    button->UpdateTextState(button->m_state);
    return 0;
}

// VERIFY: c7f51331 auto
// OFFSET: 0x009775D0
static int32_t Script_GetDisabledFontObject(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    if (!button || !button->m_disabledFont) {
        return 0;
    }

    const char* name = button->GetName();
    return CSimpleFont_GetFontObject(name ? name : "<unnamed>", button->m_disabledFont, L);
}

// VERIFY: 3c257552 auto
// OFFSET: 0x00977640
static int32_t Script_SetHighlightFontObject(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    CSimpleFont* font = nullptr;

    if (lua_type(L, 2) == LUA_TSTRING) {
        font = CSimpleFont::GetFont(lua_tolstring(L, 2, nullptr), 0);
    } else if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        font = static_cast<CSimpleFont*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
    }

    if (!button || !font || !font->IsA(CSimpleFont::GetObjectType())) {
        const char* name = button->GetName();
        luaL_error(L, "Usage: %s:SetHighlightFontObject(\"fontname\")", name ? name : "<unnamed>");
        return 0;
    }

    button->m_highlightFont = font;
    button->UpdateTextState(button->m_state);
    return 0;
}

// VERIFY: ba8f163c auto
// OFFSET: 0x00977750
static int32_t Script_GetHighlightFontObject(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    if (!button || !button->m_highlightFont) {
        return 0;
    }

    const char* name = button->GetName();
    return CSimpleFont_GetFontObject(name ? name : "<unnamed>", button->m_highlightFont, L);
}

// VERIFY: cb7d4ea4 auto
// OFFSET: 0x009777C0
static int32_t Script_SetFontString(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        CSimpleFontString* fontString = static_cast<CSimpleFontString*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!fontString) {
            const char* name = button->GetName();
            luaL_error(L, "%s:SetFontString(): Couldn't find 'this' in fontstring", name ? name : "<unnamed>");
            return 0;
        }

        if (!fontString->IsA(CSimpleFontString::GetObjectType()) || fontString->m_internal) {
            const char* name = button->GetName();
            luaL_error(L, "%s:SetFontString(): Wrong object type, expected fontstring", name ? name : "<unnamed>");
            return 0;
        }

        button->SetFontString(fontString);
        return 0;
    }

    const char* name = button->GetName();
    luaL_error(L, "Usage: %s:SetFontString(fontstring)", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 1a8abbce manual
// OFFSET: 0x009778E0
static int32_t Script_GetFontString(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    CSimpleFontString* fontString = button->m_fontString;

    if (!fontString) {
        lua_pushnil(L);
        return 1;
    }

    if (!fontString->m_luaRegistered) {
        fontString->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, fontString->m_luaObjectRef);
    return 1;
}

// VERIFY: 9d7477fa manual
// OFFSET: 0x00977960
static int32_t Script_SetText(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    button->SetText(lua_tolstring(L, 2, nullptr));
    return 0;
}

// VERIFY: c802d6cc manual
// OFFSET: 0x009779B0
static int32_t Script_SetFormattedText(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    char buffer[4096];

    button->SetText(FrameScript_Sprintf(L, 2, buffer, sizeof(buffer)));
    return 0;
}

// VERIFY: 9626b0d6 manual
// OFFSET: 0x00977A10
static int32_t Script_GetText(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    const char* text = nullptr;

    if (button->m_fontString && button->m_fontString->m_text && *button->m_fontString->m_text) {
        text = button->m_fontString->m_text;
    }

    lua_pushstring(L, text);
    return 1;
}

// VERIFY: 289ba4c4 auto
// OFFSET: 0x00977C70
static int32_t Script_SetNormalTexture(lua_State* L) {
    return SetStateTexture(L, BUTTONSTATE_NORMAL, "SetNormalTexture");
}

// VERIFY: c97789f1 auto
// OFFSET: 0x00977C90
static int32_t Script_GetNormalTexture(lua_State* L) {
    return GetStateTexture(L, BUTTONSTATE_NORMAL);
}

// VERIFY: e1a39d21 auto
// OFFSET: 0x00977CB0
static int32_t Script_SetPushedTexture(lua_State* L) {
    return SetStateTexture(L, BUTTONSTATE_PUSHED, "SetPushedTexture");
}

// VERIFY: f1561d07 auto
// OFFSET: 0x00977CD0
static int32_t Script_GetPushedTexture(lua_State* L) {
    return GetStateTexture(L, BUTTONSTATE_PUSHED);
}

// VERIFY: 02ff1bbf auto
// OFFSET: 0x00977CF0
static int32_t Script_SetDisabledTexture(lua_State* L) {
    return SetStateTexture(L, BUTTONSTATE_DISABLED, "SetDisabledTexture");
}

// VERIFY: 277625c6 auto
// OFFSET: 0x00977D10
static int32_t Script_GetDisabledTexture(lua_State* L) {
    return GetStateTexture(L, BUTTONSTATE_DISABLED);
}

// VERIFY: ef17433b auto
// OFFSET: 0x00977D30
static int32_t Script_SetHighlightTexture(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    EGxBlend blend = GxBlend_Add;

    if (lua_isstring(L, 3)) {
        StringToBlendMode(lua_tolstring(L, 3, nullptr), blend);
    }

    if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!texture) {
            const char* name = button->GetName();
            luaL_error(L, "%s:SetHighlightTexture(): Couldn't find 'this' in texture", name ? name : "<unnamed>");
            return 0;
        }

        if (!texture->IsA(CSimpleTexture::GetObjectType()) || texture->m_internal) {
            const char* name = button->GetName();
            luaL_error(L, "%s:SetHighlightTexture(): Wrong object type, expected texture", name ? name : "<unnamed>");
            return 0;
        }

        button->SetHighlight(texture, blend);
        return 0;
    }

    if (lua_isstring(L, 2)) {
        button->SetHighlight(lua_tolstring(L, 2, nullptr), blend);
        return 0;
    }

    if (lua_type(L, 2) != LUA_TNIL) {
        const char* name = button->GetName();
        luaL_error(L, "Usage: %s:SetHighlightTexture(texture or \"texture\" or nil [, \"blendmode\")", name ? name : "<unnamed>");
        return 0;
    }

    button->SetHighlight(static_cast<CSimpleTexture*>(nullptr), GxBlend_Add);
    return 0;
}

// VERIFY: ea14518e manual
// OFFSET: 0x00977EE0
static int32_t Script_GetHighlightTexture(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    CSimpleTexture* texture = button->m_highlightTexture;

    if (!texture) {
        lua_pushnil(L);
        return 1;
    }

    if (!texture->m_luaRegistered) {
        texture->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, texture->m_luaObjectRef);
    return 1;
}

// VERIFY: 2ae93d01 auto
// OFFSET: 0x00977F60
static int32_t Script_SetPushedTextOffset(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = button->GetName();
        luaL_error(L, "Usage: %s:SetPushedTextOffset(x, y)", name ? name : "<unnamed>");
        return 0;
    }

    float x = lua_tonumber(L, 2);
    x = NDCToDDCWidth(x / (CoordinateGetAspectCompensation() * 1024.0f));
    float y = lua_tonumber(L, 3);
    y = NDCToDDCWidth(y / (CoordinateGetAspectCompensation() * 1024.0f));

    button->m_pushedTextOffset.x = x;
    button->m_pushedTextOffset.y = y;
    return 0;
}

// VERIFY: ff45eb03 manual
// OFFSET: 0x00978040
static int32_t Script_GetPushedTextOffset(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    float x = button->m_pushedTextOffset.x;
    float y = button->m_pushedTextOffset.y;

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * x));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * y));
    return 2;
}

// VERIFY: 170ecd6c manual
// OFFSET: 0x009780D0
static int32_t Script_GetTextWidth(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    float width = button->m_fontString ? button->m_fontString->GetWidth() : 0.0f;

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * width));
    return 1;
}

// VERIFY: dc7eeb78 manual
// OFFSET: 0x00978150
static int32_t Script_GetTextHeight(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    float height = button->m_fontString ? button->m_fontString->GetHeight() : 0.0f;

    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * height));
    return 1;
}

// VERIFY: fc086b58 auto
// OFFSET: 0x009781D0
static int32_t Script_RegisterForClicks(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    uint64_t action = 0;

    for (int32_t i = 2; lua_isstring(L, i); i++) {
        action |= MouseButtonStringToBitmask(lua_tolstring(L, i, nullptr));
    }

    button->SetClickAction(action);
    return 0;
}

// VERIFY: 5bbdeaed manual
// OFFSET: 0x00978260
static int32_t Script_Click(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));
    const char* btn = "LeftButton";

    if (lua_isstring(L, 2)) {
        btn = lua_tolstring(L, 2, nullptr);
    }

    button->OnClick(btn, StringToBOOL(L, 3, 0));
    return 0;
}

// VERIFY: 427f6c52 manual
// OFFSET: 0x009782E0
static int32_t Script_LockHighlight(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    button->LockHighlight(1);
    return 0;
}

// VERIFY: 7d3154f0 manual
// OFFSET: 0x00978320
static int32_t Script_UnlockHighlight(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    button->LockHighlight(0);
    return 0;
}

// VERIFY: ff73dd2e manual
// OFFSET: 0x00978360
static int32_t Script_GetMotionScriptsWhileDisabled(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    lua_pushboolean(L, button->GetMotionScriptsWhileDisabled());
    return 1;
}

// VERIFY: c2fb9637 manual
// OFFSET: 0x009783B0
static int32_t Script_SetMotionScriptsWhileDisabled(lua_State* L) {
    CSimpleButton* button = static_cast<CSimpleButton*>(FrameScript_GetObjectThis(L, CSimpleButton::GetObjectType()));

    button->SetMotionScriptsWhileDisabled(StringToBOOL(L, 2, 0));
    return 0;
}

// VERIFY: a9654e41 auto
// OFFSET: 0x00977060
void CSimpleButton::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleButton::s_metatable);
    CSimpleButton::s_metatable = LUA_NOREF;
}

// VERIFY: 88a11534 auto
// OFFSET: 0x00978400
void CSimpleButton::CreateScriptMetaTable() {
    CSimpleButton::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleButton::RegisterScriptMethods);
}

// STATIC: 0x00B2D418
static FrameScript_Method s_ScriptMethods[] = {
    { "Enable", &Script_Enable },
    { "Disable", &Script_Disable },
    { "IsEnabled", &Script_IsEnabled },
    { "GetButtonState", &Script_GetButtonState },
    { "SetButtonState", &Script_SetButtonState },
    { "SetNormalFontObject", &Script_SetNormalFontObject },
    { "GetNormalFontObject", &Script_GetNormalFontObject },
    { "SetDisabledFontObject", &Script_SetDisabledFontObject },
    { "GetDisabledFontObject", &Script_GetDisabledFontObject },
    { "SetHighlightFontObject", &Script_SetHighlightFontObject },
    { "GetHighlightFontObject", &Script_GetHighlightFontObject },
    { "SetFontString", &Script_SetFontString },
    { "GetFontString", &Script_GetFontString },
    { "SetText", &Script_SetText },
    { "SetFormattedText", &Script_SetFormattedText },
    { "GetText", &Script_GetText },
    { "SetNormalTexture", &Script_SetNormalTexture },
    { "GetNormalTexture", &Script_GetNormalTexture },
    { "SetPushedTexture", &Script_SetPushedTexture },
    { "GetPushedTexture", &Script_GetPushedTexture },
    { "SetDisabledTexture", &Script_SetDisabledTexture },
    { "GetDisabledTexture", &Script_GetDisabledTexture },
    { "SetHighlightTexture", &Script_SetHighlightTexture },
    { "GetHighlightTexture", &Script_GetHighlightTexture },
    { "SetPushedTextOffset", &Script_SetPushedTextOffset },
    { "GetPushedTextOffset", &Script_GetPushedTextOffset },
    { "GetTextWidth", &Script_GetTextWidth },
    { "GetTextHeight", &Script_GetTextHeight },
    { "RegisterForClicks", &Script_RegisterForClicks },
    { "Click", &Script_Click },
    { "LockHighlight", &Script_LockHighlight },
    { "UnlockHighlight", &Script_UnlockHighlight },
    { "GetMotionScriptsWhileDisabled", &Script_GetMotionScriptsWhileDisabled },
    { "SetMotionScriptsWhileDisabled", &Script_SetMotionScriptsWhileDisabled },
};

// VERIFY: 99ad8db9 auto
// OFFSET: 0x00977040
void CSimpleButton::RegisterScriptMethods(lua_State* L) {
    CSimpleFrame::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
