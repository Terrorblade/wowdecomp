#include "CGQuestPOIFrame.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Ui/QuestLog.h"

// VERIFY: 59c6acd1 manual
// OFFSET: 0x0058E800
static int32_t Script_EnableSmoothing(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    frame->m_smoothing = lua_toboolean(L, 2);
    return 0;
}

// VERIFY: d51ff712 manual
// OFFSET: 0x0058E850
static int32_t Script_EnableMerging(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    frame->m_merging = lua_toboolean(L, 2);
    return 0;
}

// VERIFY: 12799c8d manual
// OFFSET: 0x0058E8A0
static int32_t Script_SetMergeThreshold(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    double threshold = lua_tonumber(L, 2);

    if (threshold < 0.1f) {
        threshold = 0.1f;
    } else if (threshold > 0.5f) {
        threshold = 0.5f;
    }

    frame->m_mergeThreshold = threshold;
    return 0;
}

// VERIFY: 8c983187 manual
// OFFSET: 0x0058E920
static int32_t Script_SetNumSplinePoints(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    int32_t points = lua_tointeger(L, 2);

    if (points < 8) {
        points = 8;
    } else if (points > 30) {
        points = 30;
    }

    frame->m_numSplinePoints = points;
    return 0;
}

// VERIFY: b26f7e69 manual
// OFFSET: 0x0058E990
static int32_t Script_UpdateQuestPOI(lua_State* L) {
    FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType());
    return 0;
}

// VERIFY: ba309179 manual
// OFFSET: 0x0058FE50
static int32_t Script_SetFillTexture(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    frame->SetFillTexture(lua_tolstring(L, 2, nullptr));
    return 0;
}

// VERIFY: aa010914 manual
// OFFSET: 0x0058FEA0
static int32_t Script_SetBorderTexture(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    frame->SetBorderTexture(lua_tolstring(L, 2, nullptr));
    return 0;
}

// VERIFY: 5609408a auto
// OFFSET: 0x0058E000
void CGQuestPOIFrame::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CGQuestPOIFrame::s_metatable);
    CGQuestPOIFrame::s_metatable = LUA_NOREF;
}

// VERIFY: 2b3e96b0 manual
// OFFSET: 0x0058EA50
static int32_t Script_GetTooltipIndex(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    uint32_t index = lua_tointeger(L, 2) - 1;

    if (index > 3) {
        lua_pushinteger(L, 0);
    } else {
        lua_pushinteger(L, frame->m_mouseOverObjectives[index]);
    }

    return 1;
}

// VERIFY: bd59236e manual
// OFFSET: 0x0058EAC0
static int32_t Script_GetNumTooltips(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    lua_pushinteger(L, frame->m_mouseOverCount);
    return 1;
}

// VERIFY: 4ad94ed6 auto
// OFFSET: 0x0058EB00
void CGQuestPOIFrame::CreateScriptMetaTable() {
    CGQuestPOIFrame::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CGQuestPOIFrame::RegisterScriptMethods);
}

// VERIFY: 76c68ec0 manual
// OFFSET: 0x0058E660
static int32_t Script_SetFillAlpha(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    double value = lua_tonumber(L, 2);
    float alpha = value;

    if (value > 255.0) {
        alpha = 255.0f;
    } else if (value < 0.0) {
        alpha = 0.0f;
    }

    frame->m_fillAlpha = static_cast<uint8_t>(static_cast<int64_t>(alpha));
    return 0;
}

// VERIFY: fb909670 manual
// OFFSET: 0x0058E6F0
static int32_t Script_SetBorderAlpha(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    double value = lua_tonumber(L, 2);
    float alpha = value;

    if (value > 255.0) {
        alpha = 255.0f;
    } else if (value < 0.0) {
        alpha = 0.0f;
    }

    frame->m_borderAlpha = static_cast<uint8_t>(static_cast<int64_t>(alpha));
    return 0;
}

// VERIFY: 8b6a60cf manual
// OFFSET: 0x0058E780
static int32_t Script_SetBorderScalar(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    double scalar = lua_tonumber(L, 2);

    if (scalar > 10.0) {
        scalar = 10.0;
    } else if (scalar < 0.0) {
        scalar = 0.0;
    }

    frame->m_borderScalar = scalar;
    return 0;
}

// VERIFY: 87ceb563 manual
// OFFSET: 0x005905F0
static int32_t Script_DrawQuestBlob(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    int32_t questId = static_cast<int32_t>(lua_tointeger(L, 2));

    if (lua_toboolean(L, 3)) {
        QuestPOIInfo* info = CGQuestLog::GetQuestPOIInfo(questId);

        if (info) {
            frame->AddQuest(info);
        }
    } else {
        frame->RemoveQuest(questId);
    }

    return 0;
}

// VERIFY: aaa0eec9 manual
// OFFSET: 0x0058E9C0
static int32_t Script_UpdateMouseOverTooltip(lua_State* L) {
    CGQuestPOIFrame* frame = static_cast<CGQuestPOIFrame*>(FrameScript_GetObjectThis(L, CGQuestPOIFrame::GetObjectType()));
    float x = static_cast<float>(lua_tonumber(L, 2));
    float y = static_cast<float>(lua_tonumber(L, 3));
    frame->UpdateMouseOverPOI(x, y);

    if (!frame->m_mouseOverQuestId) {
        return 0;
    }

    lua_pushinteger(L, CGQuestLog::GetQuestIndex(frame->m_mouseOverQuestId));
    lua_pushinteger(L, frame->m_mouseOverCount);
    return 2;
}

// STATIC: 0x00ACF180
static FrameScript_Method s_ScriptMethods[] = {
    { "SetFillTexture", &Script_SetFillTexture },
    { "SetFillAlpha", &Script_SetFillAlpha },
    { "SetBorderTexture", &Script_SetBorderTexture },
    { "SetBorderAlpha", &Script_SetBorderAlpha },
    { "SetBorderScalar", &Script_SetBorderScalar },
    { "DrawQuestBlob", &Script_DrawQuestBlob },
    { "EnableSmoothing", &Script_EnableSmoothing },
    { "EnableMerging", &Script_EnableMerging },
    { "SetMergeThreshold", &Script_SetMergeThreshold },
    { "SetNumSplinePoints", &Script_SetNumSplinePoints },
    { "UpdateQuestPOI", &Script_UpdateQuestPOI },
    { "UpdateMouseOverTooltip", &Script_UpdateMouseOverTooltip },
    { "GetTooltipIndex", &Script_GetTooltipIndex },
    { "GetNumTooltips", &Script_GetNumTooltips },
};

// VERIFY: ae3af700 auto
// OFFSET: 0x0058DFE0
void CGQuestPOIFrame::RegisterScriptMethods(lua_State* L) {
    CSimpleFrame::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
