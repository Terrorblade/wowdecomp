#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: cb237dcc auto
// OFFSET: 0x0048E150
void CSimpleFontString::CreateScriptMetaTable() {
    CSimpleFontString::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleFontString::RegisterScriptMethods);
}

// VERIFY: ac50861d auto
// OFFSET: 0x0048BCC0
void CSimpleFontString::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleFontString::s_metatable);
    CSimpleFontString::s_metatable = LUA_NOREF;
}

// VERIFY: a0aea1f8 manual
// OFFSET: 0x0048CD20
static int32_t Script_GetObjectType(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    lua_pushstring(L, string->GetObjectTypeName());

    return 1;
}

// VERIFY: 83bf50ad auto
// OFFSET: 0x0048CD70
static int32_t Script_IsObjectType(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (!lua_isstring(L, 2)) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:IsObjectType(\"TYPE\")", name ? name : "<unnamed>");
        return 0;
    }

    if (string->IsA(lua_tolstring(L, 2, nullptr))) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 7e88863a manual
// OFFSET: 0x0048CE20
static int32_t Script_GetDrawLayer(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    lua_pushstring(L, DrawLayerToString(string->m_drawLayer));

    return 1;
}

// VERIFY: 08943b3d auto
// OFFSET: 0x0048CE70
static int32_t Script_SetDrawLayer(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    uint32_t layer;

    if (!lua_isstring(L, 2) || (layer = string->m_drawLayer, !StringToDrawLayer(lua_tolstring(L, 2, nullptr), layer))) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:SetDrawLayer(\"layer\")", name ? name : "<unnamed>");
        return 0;
    }

    string->SetFrame(string->m_parent, layer, string->m_shown);

    return 0;
}

// VERIFY: 2116a458 auto
// OFFSET: 0x0048CF30
static int32_t Script_SetVertexColor(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    CImVector current;
    string->GetVertexColor(current);

    CImVector color;
    FrameScript_GetColor(L, 2, color);

    if (!lua_isnumber(L, 5)) {
        color.a = current.a;
    }

    string->SetVertexColor(color);
    string->m_fontableFlags &= ~0x4u;

    return 0;
}

// VERIFY: 2f47cd3c auto
// OFFSET: 0x0048CFC0
static int32_t Script_GetAlpha(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    CImVector color;
    string->GetVertexColor(color);
    lua_pushnumber(L, color.a * 0.0039215689f);

    return 1;
}

// VERIFY: 821a5977 manual
// OFFSET: 0x0048D030
static int32_t Script_SetAlpha(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (!lua_isnumber(L, 2)) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:SetAlpha(alpha)", name ? name : "<unnamed>");
        return 0;
    }

    uint8_t alpha = static_cast<uint8_t>(static_cast<int64_t>(lua_tonumber(L, 2) * 255.0));

    CImVector color;
    string->GetVertexColor(color);
    color.a = alpha;
    string->SetVertexColor(color);

    return 0;
}

// VERIFY: de94c4d2 auto
// OFFSET: 0x0048D0F0
static int32_t Script_SetAlphaGradient(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:SetAlphaGradient(start, length)", name ? name : "<unnamed>");
        return 0;
    }

    int32_t start = static_cast<int32_t>(lua_tonumber(L, 2));
    int32_t length = static_cast<int32_t>(lua_tonumber(L, 3));

    if (string->SetAlphaGradient(start, length)) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: dab16c48 manual
// OFFSET: 0x0048D1C0
static int32_t Script_Show(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    string->m_shown = 1;
    string->ShowThis();

    return 0;
}

// VERIFY: 9330f08b manual
// OFFSET: 0x0048D200
static int32_t Script_Hide(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    string->m_shown = 0;
    string->HideThis();

    return 0;
}

// VERIFY: be585441 auto
// OFFSET: 0x0048D240
static int32_t Script_IsVisible(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (string->m_visible) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 9bbfa354 auto
// OFFSET: 0x0048D2A0
static int32_t Script_IsShown(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (string->m_shown) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 3e9bf3c5 manual
// OFFSET: 0x0048D300
static int32_t Script_GetFontObject(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    CSimpleFont* font = string->m_fontObject;

    if (!font) {
        lua_pushnil(L);
        return 1;
    }

    if (!font->m_luaRegistered) {
        font->RegisterScriptObject(nullptr);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, font->m_luaObjectRef);

    return 1;
}

// VERIFY: 4b2c5f5c auto
// OFFSET: 0x0048D380
static int32_t Script_SetFontObject(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    CSimpleFont* font = nullptr;

    if (lua_type(L, 2) == LUA_TTABLE) {
        lua_rawgeti(L, 2, 0);
        font = static_cast<CSimpleFont*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!font) {
            const char* name = string->GetName();
            luaL_error(L, "%s:SetFontObject(): Couldn't find 'this' in font object", name ? name : "<unnamed>");
            return 0;
        }

        if (!font->IsA(CSimpleFont::GetObjectType())) {
            const char* name = string->GetName();
            luaL_error(L, "%s:SetFontObject(): Wrong object type, expected font", name ? name : "<unnamed>");
            return 0;
        }
    } else if (lua_type(L, 2) == LUA_TSTRING) {
        font = CSimpleFont::GetFont(lua_tolstring(L, 2, nullptr), 0);

        if (!font) {
            const char* name = string->GetName();
            luaL_error(L, "%s:SetFontObject(): Couldn't find font named %s", name ? name : "<unnamed>", lua_tolstring(L, 2, nullptr));
            return 0;
        }
    } else if (lua_type(L, 2) != LUA_TNIL) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:SetFontObject(font or \"font\" or nil)", name ? name : "<unnamed>");
        return 0;
    }

    string->SetFontObject(font);

    return 0;
}

// VERIFY: d1a6d829 auto
// OFFSET: 0x0048D510
static int32_t Script_GetFont(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    lua_pushstring(L, string->GetFontName());
    float fontHeight = string->GetFontHeight(false);
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * fontHeight));
    lua_pushstring(L, FontFlagsToString(string->GetFontFlags()));

    return 3;
}

// VERIFY: b7efa0ad auto
// OFFSET: 0x0048D5B0
static int32_t Script_SetFont(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (!lua_isstring(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:SetFont(\"font\", fontHeight [, flags])", name ? name : "<unnamed>");
        return 0;
    }

    const char* fontName = lua_tolstring(L, 2, nullptr);
    float height = static_cast<float>(lua_tonumber(L, 3));
    float fontHeight = NDCToDDCWidth(height / (CoordinateGetAspectCompensation() * 1024.0f));

    if (fontHeight <= 0.00000011920929f) {
        const char* name = string->GetName();
        luaL_error(L, "ERROR: %s:SetFont(): invalid fontHeight: %f, height must be > 0", name ? name : "<unnamed>", fontHeight);
        return 0;
    }

    if (!*fontName) {
        return 0;
    }

    uint32_t fontFlags = 0;

    if (lua_isstring(L, 4)) {
        fontFlags = StringToFontFlags(lua_tolstring(L, 4, nullptr));
    }

    if (string->SetFont(fontName, fontHeight, fontFlags, false)) {
        string->m_fontableFlags &= ~0x1u;
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: c6336ff6 auto
// OFFSET: 0x0048D730
static int32_t Script_GetText(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    const char* text = string->m_text && *string->m_text ? string->m_text : nullptr;
    lua_pushstring(L, text);

    return 1;
}

// VERIFY: 79b503aa auto
// OFFSET: 0x0048BC70
static int32_t Script_GetFieldSize(lua_State* L) {
    lua_pushnumber(L, 8191.0);

    return 1;
}

// VERIFY: 8e283226 auto
// OFFSET: 0x0048D780
static int32_t Script_SetText(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (!string->m_fontHandle) {
        const char* name = string->GetName();
        luaL_error(L, "%s:SetText(): Font not set", name ? name : "<unnamed>");
    }

    string->SetText(lua_tolstring(L, 2, nullptr), 1);

    return 0;
}

// VERIFY: ba475506 auto
// OFFSET: 0x0048D800
static int32_t Script_SetFormattedText(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (!string->m_fontHandle) {
        const char* name = string->GetName();
        luaL_error(L, "%s:SetFormattedText(): Font not set", name ? name : "<unnamed>");
    }

    char buffer[4096];
    string->SetText(FrameScript_Sprintf(L, 2, buffer, sizeof(buffer)), 1);

    return 0;
}

// VERIFY: 5c9814f7 manual
// OFFSET: 0x0048D890
static int32_t Script_GetTextColor(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    CImVector color;
    string->GetVertexColor(color);
    lua_pushnumber(L, color.r * 0.00392156f);
    lua_pushnumber(L, color.g * 0.00392156f);
    lua_pushnumber(L, color.b * 0.00392156f);
    lua_pushnumber(L, color.a * 0.00392156f);

    return 4;
}

// VERIFY: d25455b2 manual
// OFFSET: 0x0048D970
static int32_t Script_SetTextColor(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    CImVector color;
    FrameScript_GetColor(L, 2, color);
    string->SetVertexColor(color);
    string->m_fontableFlags &= ~0x4u;

    return 0;
}

// VERIFY: b0954a68 manual
// OFFSET: 0x0048D9E0
static int32_t Script_GetShadowColor(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    CImVector color = string->m_shadowColor;
    lua_pushnumber(L, color.r * 0.00392156f);
    lua_pushnumber(L, color.g * 0.00392156f);
    lua_pushnumber(L, color.b * 0.00392156f);
    lua_pushnumber(L, color.a * 0.00392156f);

    return 4;
}

// VERIFY: 853d189e manual
// OFFSET: 0x0048DAB0
static int32_t Script_SetShadowColor(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    CImVector color;
    FrameScript_GetColor(L, 2, color);
    string->SetShadowColor(color);
    string->m_fontableFlags &= ~0x8u;

    return 0;
}

// VERIFY: 97fb2ef8 auto
// OFFSET: 0x0048DB20
static int32_t Script_GetShadowOffset(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    C2Vector offset = string->m_shadowOffset;
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * offset.x));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * offset.y));

    return 2;
}

// VERIFY: 53c304b1 auto
// OFFSET: 0x0048DBB0
static int32_t Script_SetShadowOffset(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:SetShadowOffset(x, y)", name ? name : "<unnamed>");
        return 0;
    }

    float x = static_cast<float>(lua_tonumber(L, 2));
    float y = static_cast<float>(lua_tonumber(L, 3));

    C2Vector offset;
    offset.x = NDCToDDCWidth(x / (CoordinateGetAspectCompensation() * 1024.0f));
    offset.y = NDCToDDCWidth(y / (CoordinateGetAspectCompensation() * 1024.0f));
    string->SetShadowOffset(offset);
    string->m_fontableFlags &= ~0x8u;

    return 0;
}

// VERIFY: 723c2755 manual
// OFFSET: 0x0048DCA0
static int32_t Script_GetSpacing(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    float spacing = string->m_spacing;
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * spacing));

    return 1;
}

// VERIFY: 18b98b13 auto
// OFFSET: 0x0048DD00
static int32_t Script_SetSpacing(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (lua_isnumber(L, 2)) {
        float spacing = static_cast<float>(lua_tonumber(L, 2));
        string->SetSpacing(NDCToDDCWidth(spacing / (CoordinateGetAspectCompensation() * 1024.0f)));
        string->m_fontableFlags &= ~0x10u;
        return 0;
    }

    const char* name = string->GetName();
    luaL_error(L, "Usage: %s:SetSpacing(spacing)", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: 054b2881 auto
// OFFSET: 0x0048DDB0
static int32_t Script_SetTextHeight(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (lua_isnumber(L, 2)) {
        float height = static_cast<float>(lua_tonumber(L, 2));

        if (!(height > 0.00000011920929f)) {
            const char* name = string->GetName();
            luaL_error(L, "%s:SetTextHeight(): invalid texHeight: %f, height must be > 0", name ? name : "<unnamed>", height);
            return 0;
        }

        string->SetTextHeight(NDCToDDCWidth(height / (CoordinateGetAspectCompensation() * 1024.0f)));
        return 0;
    }

    const char* name = string->GetName();
    luaL_error(L, "Usage: %s:SetTextHeight(pixelHeight)", name ? name : "<unnamed>");
    return 0;
}

// VERIFY: b74114b2 auto
// OFFSET: 0x0048DE90
static int32_t Script_GetStringWidth(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    float width = string->GetStringWidth();
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * width));

    return 1;
}

// VERIFY: 8812eaa0 auto
// OFFSET: 0x0048DF00
static int32_t Script_GetStringHeight(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    float height = string->GetStringHeight();
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * height));

    return 1;
}

// VERIFY: 03e2b414 manual
// OFFSET: 0x0048DF70
static int32_t Script_GetJustifyH(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    lua_pushstring(L, JustifyToString(string->m_styleFlags & 0x7));

    return 1;
}

// VERIFY: 51b186b6 auto
// OFFSET: 0x0048E360
static int32_t Script_SetJustifyH(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    uint32_t justify;

    if (!lua_isstring(L, 2) || !StringToJustify(lua_tolstring(L, 2, nullptr), justify)) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:SetJustifyH(\"justify\")", name ? name : "<unnamed>");
        return 0;
    }

    string->m_settableStyleFlags &= ~0x7u;
    uint32_t styleFlags = (string->m_styleFlags & ~0x7u) | (justify & 0x7);

    if (styleFlags != string->m_styleFlags) {
        string->m_styleFlags = styleFlags;

        if (string->m_string) {
            string->UpdateString();
        }
    }

    string->m_fontableFlags &= ~0x2u;

    return 0;
}

// VERIFY: 31230f46 manual
// OFFSET: 0x0048DFC0
static int32_t Script_GetJustifyV(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    lua_pushstring(L, JustifyToString(string->m_styleFlags & 0x38));

    return 1;
}

// VERIFY: dd3890a2 auto
// OFFSET: 0x0048E430
static int32_t Script_SetJustifyV(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    uint32_t justify;

    if (!lua_isstring(L, 2) || !StringToJustify(lua_tolstring(L, 2, nullptr), justify)) {
        const char* name = string->GetName();
        luaL_error(L, "Usage: %s:SetJustifyV(\"justify\")", name ? name : "<unnamed>");
        return 0;
    }

    string->m_settableStyleFlags &= ~0x38u;
    uint32_t styleFlags = (string->m_styleFlags & ~0x38u) | (justify & 0x38);

    if (styleFlags != string->m_styleFlags) {
        string->m_styleFlags = styleFlags;

        if (string->m_string) {
            string->UpdateString();
        }
    }

    string->m_fontableFlags &= ~0x2u;

    return 0;
}

// VERIFY: 679d5c2f auto
// OFFSET: 0x0048E010
static int32_t Script_CanNonSpaceWrap(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (string->m_styleFlags & 0x1000) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 924f8b7b auto
// OFFSET: 0x0048E500
static int32_t Script_SetNonSpaceWrap(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    int32_t wrap = StringToBOOL(L, 2, 1);

    string->m_settableStyleFlags &= ~0x1000u;
    uint32_t styleFlags = wrap ? string->m_styleFlags | 0x1000 : string->m_styleFlags & ~0x1000u;

    if (styleFlags != string->m_styleFlags) {
        string->m_styleFlags = styleFlags;

        if (string->m_string) {
            string->UpdateString();
        }
    }

    return 0;
}

// VERIFY: f0c68146 manual
// OFFSET: 0x0048E080
static int32_t Script_CanWordWrap(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (string->m_styleFlags & 0x40) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, 1.0);
    }

    return 1;
}

// VERIFY: 6184ccaa auto
// OFFSET: 0x0048E580
static int32_t Script_SetWordWrap(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    int32_t wrap = StringToBOOL(L, 2, 1);

    string->m_settableStyleFlags &= ~0x40u;
    uint32_t styleFlags = wrap ? string->m_styleFlags & ~0x40u : string->m_styleFlags | 0x40;

    if (styleFlags != string->m_styleFlags) {
        string->m_styleFlags = styleFlags;

        if (string->m_string) {
            string->UpdateString();
        }
    }

    return 0;
}

// VERIFY: 9b3d0f99 auto
// OFFSET: 0x0048E0E0
static int32_t Script_GetIndentedWordWrap(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));

    if (string->m_styleFlags & 0x20000) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 3e88d9d6 auto
// OFFSET: 0x0048E600
static int32_t Script_SetIndentedWordWrap(lua_State* L) {
    CSimpleFontString* string = static_cast<CSimpleFontString*>(FrameScript_GetObjectThis(L, CSimpleFontString::GetObjectType()));
    int32_t indented = StringToBOOL(L, 2, 1);

    string->m_settableStyleFlags &= ~0x20000u;
    uint32_t styleFlags = indented ? string->m_styleFlags | 0x20000 : string->m_styleFlags & ~0x20000u;

    if (styleFlags != string->m_styleFlags) {
        string->m_styleFlags = styleFlags;

        if (string->m_string) {
            string->UpdateString();
        }
    }

    return 0;
}

// STATIC: 0x00AC1118
static FrameScript_Method s_ScriptMethods[] = {
    { "IsObjectType", &Script_IsObjectType },
    { "GetObjectType", &Script_GetObjectType },
    { "GetDrawLayer", &Script_GetDrawLayer },
    { "SetDrawLayer", &Script_SetDrawLayer },
    { "SetVertexColor", &Script_SetVertexColor },
    { "GetAlpha", &Script_GetAlpha },
    { "SetAlpha", &Script_SetAlpha },
    { "SetAlphaGradient", &Script_SetAlphaGradient },
    { "Show", &Script_Show },
    { "Hide", &Script_Hide },
    { "IsVisible", &Script_IsVisible },
    { "IsShown", &Script_IsShown },
    { "GetFontObject", &Script_GetFontObject },
    { "SetFontObject", &Script_SetFontObject },
    { "GetFont", &Script_GetFont },
    { "SetFont", &Script_SetFont },
    { "GetText", &Script_GetText },
    { "GetFieldSize", &Script_GetFieldSize },
    { "SetText", &Script_SetText },
    { "SetFormattedText", &Script_SetFormattedText },
    { "GetTextColor", &Script_GetTextColor },
    { "SetTextColor", &Script_SetTextColor },
    { "GetShadowColor", &Script_GetShadowColor },
    { "SetShadowColor", &Script_SetShadowColor },
    { "GetShadowOffset", &Script_GetShadowOffset },
    { "SetShadowOffset", &Script_SetShadowOffset },
    { "GetSpacing", &Script_GetSpacing },
    { "SetSpacing", &Script_SetSpacing },
    { "SetTextHeight", &Script_SetTextHeight },
    { "GetStringWidth", &Script_GetStringWidth },
    { "GetStringHeight", &Script_GetStringHeight },
    { "GetJustifyH", &Script_GetJustifyH },
    { "SetJustifyH", &Script_SetJustifyH },
    { "GetJustifyV", &Script_GetJustifyV },
    { "SetJustifyV", &Script_SetJustifyV },
    { "CanNonSpaceWrap", &Script_CanNonSpaceWrap },
    { "SetNonSpaceWrap", &Script_SetNonSpaceWrap },
    { "CanWordWrap", &Script_CanWordWrap },
    { "SetWordWrap", &Script_SetWordWrap },
    { "GetIndentedWordWrap", &Script_SetIndentedWordWrap },
    { "SetIndentedWordWrap", &Script_GetIndentedWordWrap },
};

// VERIFY: aeb77522 auto
// OFFSET: 0x0048BCA0
void CSimpleFontString::RegisterScriptMethods(lua_State* L) {
    CScriptRegion::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
