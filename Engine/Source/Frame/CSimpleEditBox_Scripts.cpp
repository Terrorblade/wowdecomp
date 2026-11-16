#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleEditBox.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// VERIFY: 62f47f0e auto
// OFFSET: 0x00975310
static int32_t Script_SetFontObject(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetFontObject(name ? name : "<unnamed>", font, L);
}

// VERIFY: 877ff481 auto
// OFFSET: 0x00975370
static int32_t Script_GetFontObject(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetFontObject(name ? name : "<unnamed>", font, L);
}

// VERIFY: 9d4f7842 auto
// OFFSET: 0x009753D0
static int32_t Script_SetFont(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetFont(name ? name : "<unnamed>", font, L);
}

// VERIFY: e0862b2d auto
// OFFSET: 0x00975430
static int32_t Script_GetFont(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetFont(name ? name : "<unnamed>", font, L);
}

// VERIFY: 160ad098 auto
// OFFSET: 0x00975490
static int32_t Script_SetTextColor(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetTextColor(name ? name : "<unnamed>", font, L);
}

// VERIFY: 25b12583 auto
// OFFSET: 0x009754F0
static int32_t Script_GetTextColor(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetTextColor(name ? name : "<unnamed>", font, L);
}

// VERIFY: b3e9dba3 auto
// OFFSET: 0x00975550
static int32_t Script_SetShadowColor(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetShadowColor(name ? name : "<unnamed>", font, L);
}

// VERIFY: dbe7bcc0 auto
// OFFSET: 0x009755B0
static int32_t Script_GetShadowColor(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetShadowColor(name ? name : "<unnamed>", font, L);
}

// VERIFY: 037469e1 auto
// OFFSET: 0x00975610
static int32_t Script_SetShadowOffset(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetShadowOffset(name ? name : "<unnamed>", font, L);
}

// VERIFY: 760f81e0 auto
// OFFSET: 0x00975670
static int32_t Script_GetShadowOffset(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetShadowOffset(name ? name : "<unnamed>", font, L);
}

// VERIFY: 2fb4adf0 auto
// OFFSET: 0x009756D0
static int32_t Script_SetSpacing(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetSpacing(name ? name : "<unnamed>", font, L);
}

// VERIFY: f0af7078 auto
// OFFSET: 0x00975730
static int32_t Script_GetSpacing(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetSpacing(name ? name : "<unnamed>", font, L);
}

// VERIFY: 62f0d354 auto
// OFFSET: 0x00975790
static int32_t Script_SetJustifyH(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetJustifyH(name ? name : "<unnamed>", font, L);
}

// VERIFY: 907e3958 auto
// OFFSET: 0x009757F0
static int32_t Script_GetJustifyH(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetJustifyH(name ? name : "<unnamed>", font, L);
}

// VERIFY: 4fa50b33 auto
// OFFSET: 0x00975850
static int32_t Script_SetJustifyV(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetJustifyV(name ? name : "<unnamed>", font, L);
}

// VERIFY: 0bb7d449 auto
// OFFSET: 0x009758B0
static int32_t Script_GetJustifyV(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetJustifyV(name ? name : "<unnamed>", font, L);
}

// VERIFY: bf7f1989 auto
// OFFSET: 0x00975910
static int32_t Script_SetIndentedWordWrap(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_SetIndentedWordWrap(name ? name : "<unnamed>", font, L);
}

// VERIFY: 7c5aeb04 auto
// OFFSET: 0x00975970
static int32_t Script_GetIndentedWordWrap(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleFont* font = editBox->m_font;
    const char* name = editBox->GetName();
    return CSimpleFont_GetIndentedWordWrap(name ? name : "<unnamed>", font, L);
}

// VERIFY: 2d9a5e90 manual
// OFFSET: 0x009759D0
static int32_t Script_SetAutoFocus(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    editBox->SetAutoFocus(StringToBOOL(L, 2, 1));
    return 0;
}

// VERIFY: ddec11f4 auto
// OFFSET: 0x00975A20
static int32_t Script_IsAutoFocus(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_editBoxFlags & 0x1) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 04ecb7c3 manual
// OFFSET: 0x00975A80
static int32_t Script_SetCountInvisibleLetters(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    editBox->SetCountInvisibleLetters(StringToBOOL(L, 2, 1));
    return 0;
}

// VERIFY: e9a229bf auto
// OFFSET: 0x00975AD0
static int32_t Script_IsCountInvisibleLetters(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_editBoxFlags & 0x20) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 99a14048 manual
// OFFSET: 0x00975B30
static int32_t Script_SetMultiLine(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    editBox->SetMultiLine(StringToBOOL(L, 2, 1));
    return 0;
}

// VERIFY: 7091d7ac auto
// OFFSET: 0x00975B80
static int32_t Script_IsMultiLine(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_editBoxFlags & 0x2) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 542d3cb7 manual
// OFFSET: 0x00975BE0
static int32_t Script_SetNumeric(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    editBox->SetNumeric(StringToBOOL(L, 2, 1));
    return 0;
}

// VERIFY: f75afea2 auto
// OFFSET: 0x00975C30
static int32_t Script_IsNumeric(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_editBoxFlags & 0x4) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: deb15ec6 manual
// OFFSET: 0x00975C90
static int32_t Script_SetPassword(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    editBox->SetPassword(StringToBOOL(L, 2, 1));
    return 0;
}

// VERIFY: 404c15c7 auto
// OFFSET: 0x00975CE0
static int32_t Script_IsPassword(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_editBoxFlags & 0x8) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: c4e7c13c auto
// OFFSET: 0x00975D40
static int32_t Script_SetBlinkSpeed(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (!lua_isnumber(L, 2)) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:SetBlinkSpeed(speed)", name ? name : "<unnamed>");
        return 0;
    }

    editBox->m_cursorBlinkSpeed = lua_tonumber(L, 2);
    return 0;
}

// VERIFY: 9fc2505f manual
// OFFSET: 0x00975DC0
static int32_t Script_GetBlinkSpeed(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    lua_pushnumber(L, editBox->m_cursorBlinkSpeed);
    return 1;
}

// VERIFY: d2fb0772 auto
// OFFSET: 0x00975E10
static int32_t Script_Insert(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (lua_isstring(L, 2)) {
        const char* taint = lua_tainted;
        editBox->Insert(lua_tolstring(L, 2, nullptr), taint, 1, 0, 0);
    }

    return 0;
}

// VERIFY: 02668637 auto
// OFFSET: 0x00975E80
static int32_t Script_SetText(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (!lua_isstring(L, 2)) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:SetText(\"text\")", name ? name : "<unnamed>");
        return 0;
    }

    const char* taint = lua_tainted;
    editBox->SetText(lua_tolstring(L, 2, nullptr), taint);
    return 0;
}

// VERIFY: f9bbb713 auto
// OFFSET: 0x00975F10
static int32_t Script_GetText(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_textTaint && lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = editBox->m_textTaint;
    }

    lua_pushstring(L, editBox->m_text);
    return 1;
}

// VERIFY: 700cda06 auto
// OFFSET: 0x00975F80
static int32_t Script_SetNumber(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (!lua_isstring(L, 2)) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:SetNumber(number)", name ? name : "<unnamed>");
        return 0;
    }

    const char* taint = lua_tainted;
    editBox->SetText(lua_tolstring(L, 2, nullptr), taint);
    return 0;
}

// VERIFY: 2fc78b84 auto
// OFFSET: 0x00976010
static int32_t Script_GetNumber(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_textTaint && lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = editBox->m_textTaint;
    }

    lua_pushnumber(L, SStrToFloat(editBox->m_text));
    return 1;
}

// VERIFY: 99221d91 auto
// OFFSET: 0x00976080
static int32_t Script_HighlightText(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    int32_t start = 0;

    if (lua_isnumber(L, 2)) {
        start = static_cast<int32_t>(lua_tonumber(L, 2));
    }

    int32_t end = -1;

    if (lua_isnumber(L, 3)) {
        end = static_cast<int32_t>(lua_tonumber(L, 3));
    }

    editBox->HighlightText(start, end);
    return 0;
}

// VERIFY: c0d84f79 auto
// OFFSET: 0x00976110
static int32_t Script_AddHistoryLine(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (lua_gettop(L) != 2) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:AddHistoryLine(\"text\")", name ? name : "<unnamed>");
        return 0;
    }

    const char* taint = lua_tainted;
    editBox->AddHistoryLine(lua_tolstring(L, 2, nullptr), taint);
    return 0;
}

// VERIFY: f39a94bc manual
// OFFSET: 0x009761A0
static int32_t Script_ClearHistory(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    editBox->ClearHistory();
    return 0;
}

// VERIFY: 0efa1962 auto
// OFFSET: 0x009761E0
static int32_t Script_SetTextInsets(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5)) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:SetTextInsets(l, r, t, b)", name ? name : "<unnamed>");
        return 0;
    }

    float bottom = lua_tonumber(L, 5);
    float top = lua_tonumber(L, 4);
    float right = lua_tonumber(L, 3);
    float left = lua_tonumber(L, 2);
    float ddcBottom = NDCToDDCWidth(bottom / (CoordinateGetAspectCompensation() * 1024.0f));
    float ddcTop = NDCToDDCWidth(top / (CoordinateGetAspectCompensation() * 1024.0f));
    float ddcRight = NDCToDDCWidth(right / (CoordinateGetAspectCompensation() * 1024.0f));
    float ddcLeft = NDCToDDCWidth(left / (CoordinateGetAspectCompensation() * 1024.0f));
    editBox->SetEditTextInsets(ddcLeft, ddcRight, ddcTop, ddcBottom);
    return 0;
}

// VERIFY: cde94e81 auto
// OFFSET: 0x00976330
static int32_t Script_GetTextInsets(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    float left;
    float right;
    float top;
    float bottom;
    editBox->GetEditTextInsets(left, right, top, bottom);
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * left));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * right));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * top));
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * bottom));
    return 4;
}

// VERIFY: e7d01170 manual
// OFFSET: 0x00976410
static int32_t Script_SetFocus(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleEditBox::SetKeyboardFocus(editBox);
    return 0;
}

// VERIFY: c6a7064c manual
// OFFSET: 0x00976450
static int32_t Script_ClearFocus(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    CSimpleEditBox::ClearKeyboardFocus(editBox, true);
    return 0;
}

// VERIFY: 83c302bb manual
// OFFSET: 0x00976490
static int32_t Script_HasFocus(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    lua_pushboolean(L, editBox == CSimpleEditBox::s_currentFocus);
    return 1;
}

// VERIFY: 6736d24f auto
// OFFSET: 0x009764E0
static int32_t Script_SetMaxBytes(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (lua_gettop(L) != 2) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:SetMaxBytes(max)", name ? name : "<unnamed>");
        return 0;
    }

    int32_t maxBytes = static_cast<int32_t>(lua_tonumber(L, 2));
    editBox->m_textMaxLength = maxBytes <= 0 ? -1 : maxBytes - 1;
    return 0;
}

// VERIFY: 324af91b manual
// OFFSET: 0x00976580
static int32_t Script_GetMaxBytes(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    lua_pushnumber(L, editBox->m_textMaxLength + 1);
    return 1;
}

// VERIFY: 775d5f8d auto
// OFFSET: 0x009765D0
static int32_t Script_SetMaxLetters(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (lua_gettop(L) != 2) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:SetMaxLetters(max)", name ? name : "<unnamed>");
        return 0;
    }

    int32_t maxLetters = static_cast<int32_t>(lua_tonumber(L, 2));
    editBox->m_textMaxLetters = maxLetters < 0 ? 0 : maxLetters;
    return 0;
}

// VERIFY: 4db61217 manual
// OFFSET: 0x00976650
static int32_t Script_GetMaxLetters(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    lua_pushnumber(L, editBox->m_textMaxLetters);
    return 1;
}

// VERIFY: 3949c602 auto
// OFFSET: 0x009766A0
static int32_t Script_GetNumLetters(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    int32_t letters = editBox->m_editBoxFlags & 0x20 ? SStrLenUTF8(editBox->m_text) : editBox->GetLenToNum(0, editBox->m_textLength);
    lua_pushnumber(L, letters);
    return 1;
}

// VERIFY: f39b5d2d manual
// OFFSET: 0x00976720
static int32_t Script_GetHistoryLines(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    lua_pushnumber(L, editBox->m_historyLines);
    return 1;
}

// VERIFY: 8713f97d auto
// OFFSET: 0x00976770
static int32_t Script_SetHistoryLines(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    int32_t lines;

    if (!lua_isnumber(L, 2) || (lines = static_cast<int32_t>(lua_tonumber(L, 2))) <= 0) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:SetHistoryLines(numLines)", name ? name : "<unnamed>");
        return 0;
    }

    editBox->SetHistoryLines(lines);
    return 0;
}

// VERIFY: 7d9fcc3c manual
// OFFSET: 0x00976800
static int32_t Script_GetInputLanguage(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    lua_pushstring(L, editBox->GetInputLanguageString());
    return 1;
}

// VERIFY: 700fc26c manual
// OFFSET: 0x00976850
static int32_t Script_ToggleInputLanguage(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    editBox->ToggleInputLanguage();
    return 0;
}

// VERIFY: 99b3501e manual
// OFFSET: 0x00976890
static int32_t Script_SetAltArrowKeyMode(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    editBox->SetAltArrowKeyMode(StringToBOOL(L, 2, 1));
    return 0;
}

// VERIFY: f03d5c1d auto
// OFFSET: 0x009768E0
static int32_t Script_GetAltArrowKeyMode(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_editBoxFlags & 0x10) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 7ce6163c auto
// OFFSET: 0x00976940
static int32_t Script_IsInIMECompositionMode(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (editBox->m_imeComposition) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 513ec1db manual
// OFFSET: 0x009769A0
static int32_t Script_SetCursorPosition(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    if (!lua_isnumber(L, 2)) {
        const char* name = editBox->GetName();
        luaL_error(L, "Usage: %s:SetCursorPosition(position)", name ? name : "<unnamed>");
        return 0;
    }

    editBox->SetCursorPosition(static_cast<int32_t>(lua_tonumber(L, 2)));
    return 0;
}

// VERIFY: e2b91fb8 manual
// OFFSET: 0x00976A20
static int32_t Script_GetCursorPosition(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    lua_pushnumber(L, editBox->m_cursorPos);
    return 1;
}

// VERIFY: d07fe843 manual
// OFFSET: 0x00976A70
static int32_t Script_GetUTF8CursorPosition(lua_State* L) {
    CSimpleEditBox* editBox = static_cast<CSimpleEditBox*>(FrameScript_GetObjectThis(L, CSimpleEditBox::GetObjectType()));
    lua_pushnumber(L, SStrLenUTF8(editBox->m_text, editBox->m_cursorPos));
    return 1;
}

// VERIFY: 10d61e61 auto
// OFFSET: 0x009752E0
void CSimpleEditBox::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleEditBox::s_metatable);
    CSimpleEditBox::s_metatable = LUA_NOREF;
}

// VERIFY: 0ebf05e3 auto
// OFFSET: 0x00976AD0
void CSimpleEditBox::CreateScriptMetaTable() {
    CSimpleEditBox::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleEditBox::RegisterScriptMethods);
}

// STATIC: 0x00B2D210
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
    { "SetAutoFocus", &Script_SetAutoFocus },
    { "IsAutoFocus", &Script_IsAutoFocus },
    { "SetCountInvisibleLetters", &Script_SetCountInvisibleLetters },
    { "IsCountInvisibleLetters", &Script_IsCountInvisibleLetters },
    { "SetMultiLine", &Script_SetMultiLine },
    { "IsMultiLine", &Script_IsMultiLine },
    { "SetNumeric", &Script_SetNumeric },
    { "IsNumeric", &Script_IsNumeric },
    { "SetPassword", &Script_SetPassword },
    { "IsPassword", &Script_IsPassword },
    { "SetBlinkSpeed", &Script_SetBlinkSpeed },
    { "GetBlinkSpeed", &Script_GetBlinkSpeed },
    { "Insert", &Script_Insert },
    { "SetText", &Script_SetText },
    { "GetText", &Script_GetText },
    { "SetNumber", &Script_SetNumber },
    { "GetNumber", &Script_GetNumber },
    { "HighlightText", &Script_HighlightText },
    { "AddHistoryLine", &Script_AddHistoryLine },
    { "ClearHistory", &Script_ClearHistory },
    { "SetTextInsets", &Script_SetTextInsets },
    { "GetTextInsets", &Script_GetTextInsets },
    { "SetFocus", &Script_SetFocus },
    { "ClearFocus", &Script_ClearFocus },
    { "HasFocus", &Script_HasFocus },
    { "SetMaxBytes", &Script_SetMaxBytes },
    { "GetMaxBytes", &Script_GetMaxBytes },
    { "SetMaxLetters", &Script_SetMaxLetters },
    { "GetMaxLetters", &Script_GetMaxLetters },
    { "GetNumLetters", &Script_GetNumLetters },
    { "GetHistoryLines", &Script_GetHistoryLines },
    { "SetHistoryLines", &Script_SetHistoryLines },
    { "GetInputLanguage", &Script_GetInputLanguage },
    { "ToggleInputLanguage", &Script_ToggleInputLanguage },
    { "SetAltArrowKeyMode", &Script_SetAltArrowKeyMode },
    { "GetAltArrowKeyMode", &Script_GetAltArrowKeyMode },
    { "IsInIMECompositionMode", &Script_IsInIMECompositionMode },
    { "SetCursorPosition", &Script_SetCursorPosition },
    { "GetCursorPosition", &Script_GetCursorPosition },
    { "GetUTF8CursorPosition", &Script_GetUTF8CursorPosition },
};

// VERIFY: 068658a4 auto
// OFFSET: 0x009752C0
void CSimpleEditBox::RegisterScriptMethods(lua_State* L) {
    CSimpleFrame::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
