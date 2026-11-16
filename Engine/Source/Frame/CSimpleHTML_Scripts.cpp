#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleHTML.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 17d3ca2b auto
// OFFSET: 0x009748F0
static HTML_TEXT_TYPE ReadHtmlTextType(lua_State* L) {
    if (lua_type(L, 2) != LUA_TSTRING) {
        return HTML_TEXT_NORMAL;
    }

    const char* style = lua_tolstring(L, 2, nullptr);

    if (!SStrCmpI(style, "P", STORM_MAX_STR)) {
        lua_remove(L, 2);
        return HTML_TEXT_NORMAL;
    }

    if (!SStrCmpI(style, "H1", STORM_MAX_STR)) {
        lua_remove(L, 2);
        return HTML_TEXT_HEADER1;
    }

    if (!SStrCmpI(style, "H2", STORM_MAX_STR)) {
        lua_remove(L, 2);
        return HTML_TEXT_HEADER2;
    }

    if (!SStrCmpI(style, "H3", STORM_MAX_STR)) {
        lua_remove(L, 2);
        return HTML_TEXT_HEADER3;
    }

    return HTML_TEXT_NORMAL;
}

// VERIFY: 7f55f6d4 auto
// OFFSET: 0x00974A10
static int32_t Script_SetFontObject(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetFontObject(name ? name : "<unnamed>", font, L);
}

// VERIFY: 85236b1a auto
// OFFSET: 0x00974A70
static int32_t Script_GetFontObject(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetFontObject(name ? name : "<unnamed>", font, L);
}

// VERIFY: 78f76a14 auto
// OFFSET: 0x00974AD0
static int32_t Script_SetFont(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetFont(name ? name : "<unnamed>", font, L);
}

// VERIFY: 517702ef auto
// OFFSET: 0x00974B30
static int32_t Script_GetFont(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetFont(name ? name : "<unnamed>", font, L);
}

// VERIFY: 43a234d7 auto
// OFFSET: 0x00974B90
static int32_t Script_SetTextColor(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetTextColor(name ? name : "<unnamed>", font, L);
}

// VERIFY: 72b9fb60 auto
// OFFSET: 0x00974BF0
static int32_t Script_GetTextColor(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetTextColor(name ? name : "<unnamed>", font, L);
}

// VERIFY: e2c7782e auto
// OFFSET: 0x00974C50
static int32_t Script_SetShadowColor(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetShadowColor(name ? name : "<unnamed>", font, L);
}

// VERIFY: acc9198b auto
// OFFSET: 0x00974CB0
static int32_t Script_GetShadowColor(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetShadowColor(name ? name : "<unnamed>", font, L);
}

// VERIFY: 66e893f4 auto
// OFFSET: 0x00974D10
static int32_t Script_SetShadowOffset(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetShadowOffset(name ? name : "<unnamed>", font, L);
}

// VERIFY: 6813eef3 auto
// OFFSET: 0x00974D70
static int32_t Script_GetShadowOffset(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetShadowOffset(name ? name : "<unnamed>", font, L);
}

// VERIFY: f3ee6d28 auto
// OFFSET: 0x00974DD0
static int32_t Script_SetSpacing(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetSpacing(name ? name : "<unnamed>", font, L);
}

// VERIFY: 1623f9a2 auto
// OFFSET: 0x00974E30
static int32_t Script_GetSpacing(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetSpacing(name ? name : "<unnamed>", font, L);
}

// VERIFY: 86810935 auto
// OFFSET: 0x00974E90
static int32_t Script_SetJustifyH(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetJustifyH(name ? name : "<unnamed>", font, L);
}

// VERIFY: f631c956 auto
// OFFSET: 0x00974EF0
static int32_t Script_GetJustifyH(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetJustifyH(name ? name : "<unnamed>", font, L);
}

// VERIFY: d74d5372 auto
// OFFSET: 0x00974F50
static int32_t Script_SetJustifyV(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetJustifyV(name ? name : "<unnamed>", font, L);
}

// VERIFY: f6a5f9ad auto
// OFFSET: 0x00974FB0
static int32_t Script_GetJustifyV(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetJustifyV(name ? name : "<unnamed>", font, L);
}

// VERIFY: 5feaef2b auto
// OFFSET: 0x00975010
static int32_t Script_SetIndentedWordWrap(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_SetIndentedWordWrap(name ? name : "<unnamed>", font, L);
}

// VERIFY: bfb865ba auto
// OFFSET: 0x00975070
static int32_t Script_GetIndentedWordWrap(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    CSimpleFont* font = html->m_fonts[ReadHtmlTextType(L)];
    const char* name = html->GetName();
    return CSimpleFont_GetIndentedWordWrap(name ? name : "<unnamed>", font, L);
}

// VERIFY: dbc9484d manual
// OFFSET: 0x009750D0
static int32_t Script_SetText(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));

    html->SetText(lua_tolstring(L, 2, nullptr), nullptr);
    return 0;
}

// VERIFY: 5196d1a8 auto
// OFFSET: 0x00975120
static int32_t Script_SetHyperlinkFormat(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));

    if (lua_isstring(L, 2)) {
        html->SetHyperlinkFormat(lua_tolstring(L, 2, nullptr));
        return 0;
    }

    const char* name = html->GetName();
    luaL_error(L, "Usage: %s:SetHyperlinkFormat(\"format\")", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 07785af6 manual
// OFFSET: 0x009751A0
static int32_t Script_GetHyperlinkFormat(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));

    lua_pushstring(L, html->m_hyperlinkFormat);
    return 1;
}

// VERIFY: 377f4561 auto
// OFFSET: 0x009751E0
static int32_t Script_SetHyperlinksEnabled(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));
    bool enabled = StringToBOOL(L, 2, 1) != 0;

    if (enabled != html->m_hyperlinksEnabled) {
        html->m_hyperlinksEnabled = enabled;
        html->m_hyperlinksDirty = 1;
    }

    return 0;
}

// VERIFY: 72b1db45 auto
// OFFSET: 0x00975240
static int32_t Script_GetHyperlinksEnabled(lua_State* L) {
    CSimpleHTML* html = static_cast<CSimpleHTML*>(FrameScript_GetObjectThis(L, CSimpleHTML::GetObjectType()));

    if (html->m_hyperlinksEnabled) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 2f8ea65e auto
// OFFSET: 0x009749E0
void CSimpleHTML::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleHTML::s_metatable);
    CSimpleHTML::s_metatable = LUA_NOREF;
}

// VERIFY: 31c4b9cf auto
// OFFSET: 0x009752A0
void CSimpleHTML::CreateScriptMetaTable() {
    CSimpleHTML::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleHTML::RegisterScriptMethods);
}

// STATIC: 0x00B2D150
static FrameScript_Method s_ScriptMethods[] = {
    { "SetFontObject", &Script_SetFontObject },
    { "GetFontObject", &Script_GetFontObject },
    { "SetFont", &Script_SetFont },
    { "GetFont", &Script_GetFont },
    { "SetTextColor", &Script_SetTextColor },
    { "GetTextColor", &Script_GetTextColor },
    { "SetShadowColor", &Script_SetShadowColor },
    { "GetShadowColor", &Script_GetShadowColor },
    { "SetShadowOffset", &Script_SetShadowOffset },
    { "GetShadowOffset", &Script_GetShadowOffset },
    { "SetSpacing", &Script_SetSpacing },
    { "GetSpacing", &Script_GetSpacing },
    { "SetJustifyH", &Script_SetJustifyH },
    { "GetJustifyH", &Script_GetJustifyH },
    { "SetJustifyV", &Script_SetJustifyV },
    { "GetJustifyV", &Script_GetJustifyV },
    { "SetIndentedWordWrap", &Script_SetIndentedWordWrap },
    { "GetIndentedWordWrap", &Script_GetIndentedWordWrap },
    { "SetText", &Script_SetText },
    { "SetHyperlinkFormat", &Script_SetHyperlinkFormat },
    { "GetHyperlinkFormat", &Script_GetHyperlinkFormat },
    { "SetHyperlinksEnabled", &Script_SetHyperlinksEnabled },
    { "GetHyperlinksEnabled", &Script_GetHyperlinksEnabled },
};

// VERIFY: bf499fdd auto
// OFFSET: 0x009749C0
void CSimpleHTML::RegisterScriptMethods(lua_State* L) {
    CSimpleFrame::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
