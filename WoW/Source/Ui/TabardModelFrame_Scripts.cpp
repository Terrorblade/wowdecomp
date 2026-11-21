#include "TabardModelFrame.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/BLPFile/blp.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Services/Texture.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Common/ComponentCore/Types.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"


// VERIFY: 5cd46383 auto
// OFFSET: 0x00599000
void CGTabardModelFrame::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CGTabardModelFrame::s_metatable);
    CGTabardModelFrame::s_metatable = LUA_NOREF;
}

// VERIFY: 07bd4eda auto
// OFFSET: 0x00599390
void CGTabardModelFrame::CreateScriptMetaTable() {
    CGTabardModelFrame::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CGTabardModelFrame::RegisterScriptMethods);
}

// VERIFY: cb34ed66 auto
// OFFSET: 0x00598FA0
static int32_t Script_CanSaveTabardNow(lua_State* L) {
    if (CGPlayer_C::CanSaveTabardNow()) {
        lua_pushnumber(L, 1.0);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: a0c4da74 manual
// OFFSET: 0x005991D0
static int32_t Script_GetUpperBackgroundFileName(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));
    char fileName[260];
    CCharacterComponent::GetTabardBackgroundFileName(SECTION_TORSO_UPPER, frame->m_variations[4], fileName, sizeof(fileName));
    lua_pushstring(L, fileName);
    return 1;
}

// VERIFY: 30cff265 manual
// OFFSET: 0x00599240
static int32_t Script_GetLowerBackgroundFileName(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));
    char fileName[260];
    CCharacterComponent::GetTabardBackgroundFileName(SECTION_TORSO_LOWER, frame->m_variations[4], fileName, sizeof(fileName));
    lua_pushstring(L, fileName);
    return 1;
}

// VERIFY: 1ec781b2 manual
// OFFSET: 0x005992B0
static int32_t Script_GetUpperEmblemFileName(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));
    char fileName[260];
    CCharacterComponent::GetTabardEmblemFileName(SECTION_TORSO_UPPER, frame->m_variations[0], frame->m_variations[1], fileName, sizeof(fileName));
    lua_pushstring(L, fileName);
    return 1;
}

// VERIFY: f52d0233 manual
// OFFSET: 0x00599320
static int32_t Script_GetLowerEmblemFileName(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));
    char fileName[260];
    CCharacterComponent::GetTabardEmblemFileName(SECTION_TORSO_LOWER, frame->m_variations[0], frame->m_variations[1], fileName, sizeof(fileName));
    lua_pushstring(L, fileName);
    return 1;
}

// VERIFY: 3031c972 manual
// OFFSET: 0x005994C0
static int32_t Script_Save(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));
    frame->Save();
    return 0;
}

// VERIFY: eddd0be9 manual
// OFFSET: 0x00599500
static int32_t Script_CycleVariation(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));

    if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        luaL_error(L, "Usage: CycleVariation(variationIndex, delta)");
        return 0;
    }

    uint32_t variation = static_cast<int32_t>(lua_tonumber(L, 2)) - 1;

    if (variation >= 5) {
        luaL_error(L, "Invalid variationIndex in CycleVariation");
        return 0;
    }

    frame->CycleVariation(variation, static_cast<int32_t>(lua_tonumber(L, 3)));
    return 0;
}

// VERIFY: cb1e71ba manual
// OFFSET: 0x005997C0
static int32_t Script_InitializeTabardColors(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (player) {
        frame->InitializeTabardColors(player);
    }

    return 0;
}

// VERIFY: 834caa4a manual
// OFFSET: 0x00599890
static int32_t Script_GetUpperEmblemTexture(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));

    if (lua_type(L, 2) != LUA_TTABLE) {
        const char* name = frame->GetName();
        luaL_error(L, "Usage: %s:GetUpperEmblemTexture(texture)", name ? name : "<unnamed>");
    }

    lua_rawgeti(L, 2, 0);
    CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
    lua_settop(L, -2);

    if (!texture) {
        const char* name = frame->GetName();
        luaL_error(L, "%s:GetUpperEmblemTexture(): Couldn't find 'this' in texture object", name ? name : "<unnamed>");
    }

    if (!texture->IsA(CSimpleTexture::GetObjectType())) {
        const char* name = frame->GetName();
        luaL_error(L, "%s:GetUpperEmblemTexture(): Wrong object type, expected texture", name ? name : "<unnamed>");
    }

    // STATIC: 0x00C0E538
    static TSGrowableArray<CImVector> s_texels;

    if (!s_texels.m_count) {
        s_texels.SetCount(0x2000);
    }

    char fileName[260];
    CCharacterComponent::GetTabardEmblemFileName(SECTION_TORSO_UPPER, frame->m_variations[0], frame->m_variations[1], fileName, sizeof(fileName));
    SStrPack(fileName, ".BLP", sizeof(fileName));
    CBLPFile blp;

    if (blp.Open(fileName, 0)) {
        uint8_t* data;
        uint32_t stride;

        if (blp.Lock(PIXEL_ARGB8888, 0, data, stride)) {
            uint32_t count = blp.m_header.m_width * blp.m_header.m_height;

            if (count > s_texels.m_count) {
                count = s_texels.m_count;
            }

            for (uint32_t i = 0; i < count; i++) {
                CImVector& texel = s_texels.m_data[i];
                texel.a = data[4 * i + 3];
                texel.r = 0xFF;
                texel.g = 0xFF;
                texel.b = 0xFF;
            }
        }

        blp.Unlock(0);
        blp.Close();
    }

    CTexture* emblem = TextureCreate(0x80, 0x40, GxTex_Argb8888, GxTex_Argb8888, CGxTexFlags(GxTex_Linear, 0, 0, 0, 0, 0, 1), &s_texels, EmblemTextureUpdate, "TabardModelFrameUpper", 0);
    texture->SetTexture(emblem);
    HandleClose(reinterpret_cast<HOBJECT>(emblem));
    blp.Close();
    return 0;
}

// VERIFY: f1e7b646 manual
// OFFSET: 0x00599B20
static int32_t Script_GetLowerEmblemTexture(lua_State* L) {
    CGTabardModelFrame* frame = static_cast<CGTabardModelFrame*>(FrameScript_GetObjectThis(L, CGTabardModelFrame::GetObjectType()));

    if (lua_type(L, 2) != LUA_TTABLE) {
        const char* name = frame->GetName();
        luaL_error(L, "Usage: %s:GetLowerEmblemTexture(texture)", name ? name : "<unnamed>");
    }

    lua_rawgeti(L, 2, 0);
    CSimpleTexture* texture = static_cast<CSimpleTexture*>(lua_touserdata(L, -1));
    lua_settop(L, -2);

    if (!texture) {
        const char* name = frame->GetName();
        luaL_error(L, "%s:GetLowerEmblemTexture(): Couldn't find 'this' in texture object", name ? name : "<unnamed>");
    }

    if (!texture->IsA(CSimpleTexture::GetObjectType())) {
        const char* name = frame->GetName();
        luaL_error(L, "%s:GetLowerEmblemTexture(): Wrong object type, expected texture", name ? name : "<unnamed>");
    }

    // STATIC: 0x00C0E548
    static TSGrowableArray<CImVector> s_texels;

    if (!s_texels.m_count) {
        s_texels.SetCount(0x1000);
    }

    char fileName[260];
    CCharacterComponent::GetTabardEmblemFileName(SECTION_TORSO_LOWER, frame->m_variations[0], frame->m_variations[1], fileName, sizeof(fileName));
    SStrPack(fileName, ".BLP", sizeof(fileName));
    CBLPFile blp;

    if (blp.Open(fileName, 0)) {
        uint8_t* data;
        uint32_t stride;

        if (blp.Lock(PIXEL_ARGB8888, 0, data, stride)) {
            uint32_t count = blp.m_header.m_width * blp.m_header.m_height;

            if (count > s_texels.m_count) {
                count = s_texels.m_count;
            }

            for (uint32_t i = 0; i < count; i++) {
                CImVector& texel = s_texels.m_data[i];
                texel.a = data[4 * i + 3];
                texel.r = 0xFF;
                texel.g = 0xFF;
                texel.b = 0xFF;
            }
        }

        blp.Unlock(0);
        blp.Close();
    }

    CTexture* emblem = TextureCreate(0x80, 0x20, GxTex_Argb8888, GxTex_Argb8888, CGxTexFlags(GxTex_Linear, 0, 0, 0, 0, 0, 1), &s_texels, EmblemTextureUpdate, "TabardModelFrameLower", 0);
    texture->SetTexture(emblem);
    HandleClose(reinterpret_cast<HOBJECT>(emblem));
    blp.Close();
    return 0;
}

// STATIC: 0x00ACF588
static FrameScript_Method s_ScriptMethods[] = {
    { "InitializeTabardColors", &Script_InitializeTabardColors },
    { "Save", &Script_Save },
    { "CycleVariation", &Script_CycleVariation },
    { "GetUpperBackgroundFileName", &Script_GetUpperBackgroundFileName },
    { "GetLowerBackgroundFileName", &Script_GetLowerBackgroundFileName },
    { "GetUpperEmblemFileName", &Script_GetUpperEmblemFileName },
    { "GetLowerEmblemFileName", &Script_GetLowerEmblemFileName },
    { "GetUpperEmblemTexture", &Script_GetUpperEmblemTexture },
    { "GetLowerEmblemTexture", &Script_GetLowerEmblemTexture },
    { "CanSaveTabardNow", &Script_CanSaveTabardNow },
};

// VERIFY: ff1f59bb auto
// OFFSET: 0x00598FE0
void CGTabardModelFrame::RegisterScriptMethods(lua_State* L) {
    CGCharacterModelBase::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
