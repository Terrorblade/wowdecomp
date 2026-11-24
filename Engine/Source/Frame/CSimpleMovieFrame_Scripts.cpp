#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleMovieFrame.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// INLINED
void CSimpleMovieFrame::CreateScriptMetaTable() {
    CSimpleMovieFrame::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleMovieFrame::RegisterScriptMethods);
}

// INLINED
void CSimpleMovieFrame::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CSimpleMovieFrame::s_metatable);
    CSimpleMovieFrame::s_metatable = LUA_NOREF;
}

// VERIFY: e4c13c27 auto
// OFFSET: 0x00970660
static int32_t Script_StartMovie(lua_State* L) {
    CSimpleMovieFrame* movieFrame = static_cast<CSimpleMovieFrame*>(FrameScript_GetObjectThis(L, CSimpleMovieFrame::GetObjectType()));

    if (!lua_isstring(L, 2) || !lua_isnumber(L, 3)) {
        const char* name = movieFrame->GetName();
        luaL_error(L, "Usage: %s:StartMovie(\"filename\", volume_0_to_255)", name ? name : "<unnamed>");
        return 0;
    }

    if (movieFrame->StartMovie(lua_tolstring(L, 2, nullptr), static_cast<int32_t>(lua_tointeger(L, 3)))) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: 20721d4b manual
// OFFSET: 0x00970730
static int32_t Script_StopMovie(lua_State* L) {
    CSimpleMovieFrame* movieFrame = static_cast<CSimpleMovieFrame*>(FrameScript_GetObjectThis(L, CSimpleMovieFrame::GetObjectType()));

    movieFrame->StopMovie();
    return 0;
}

// VERIFY: 468643b1 manual
// OFFSET: 0x00970770
static int32_t Script_EnableSubtitles(lua_State* L) {
    CSimpleMovieFrame* movieFrame = static_cast<CSimpleMovieFrame*>(FrameScript_GetObjectThis(L, CSimpleMovieFrame::GetObjectType()));

    movieFrame->m_subtitlesEnabled = StringToBOOL(L, 2, 1);
    return 0;
}

// STATIC: 0x00B2CD30
static FrameScript_Method s_ScriptMethods[] = {
    { "StartMovie", &Script_StartMovie },
    { "StopMovie", &Script_StopMovie },
    { "EnableSubtitles", &Script_EnableSubtitles },
};

// VERIFY: eda0e55c auto
// OFFSET: 0x00970610
void CSimpleMovieFrame::RegisterScriptMethods(lua_State* L) {
    CSimpleFrame::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
