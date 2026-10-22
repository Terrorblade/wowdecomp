#include "TaxiMapFrame.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "WoW/Source/DB/Rec/TaxiNodesRec.h"

// VERIFY: 70a6c64a manual
// OFFSET: 0x00590D10
static int32_t Script_SetTaxiMap(lua_State* L) {
    if (CGTaxiMap::m_mapTexture) {
        CTexture* texture = CGTaxiMap::m_mapTexture;
        static_cast<CSimpleTexture*>(FrameScript_GetObjectThis(L, CSimpleTexture::GetObjectType()))->SetTexture(texture);
    }

    return 0;
}

// VERIFY: 341f53fa manual
// OFFSET: 0x00590710
static int32_t Script_NumTaxiNodes(lua_State* L) {
    lua_pushnumber(L, CGTaxiMap::m_numNodes);
    return 1;
}

// VERIFY: f86dd911 manual
// OFFSET: 0x00590D60
static int32_t Script_TaxiNodeName(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: TaxiNodeName(slot)");
        return 0;
    }

    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;

    if (index >= CGTaxiMap::m_numNodes) {
        lua_pushstring(L, "INVALID");
    } else {
        lua_pushstring(L, CGTaxiMap::m_nodes.m_data[index].m_rec->m_name);
    }

    return 1;
}

// VERIFY: 39b322e3 auto
// OFFSET: 0x00590E00
static int32_t Script_TaxiNodePosition(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: TaxiNodePosition(slot)");
        return 0;
    }

    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;

    if (index >= CGTaxiMap::m_numNodes) {
        luaL_error(L, "Invalid taxi node slot");
        return 0;
    }

    TaxiNodeInfo* node = &CGTaxiMap::m_nodes.m_data[index];
    float y = node->m_y;
    lua_pushnumber(L, node->m_x);
    lua_pushnumber(L, y);
    return 2;
}

// VERIFY: ce65bfe9 manual
// OFFSET: 0x005911C0
static int32_t Script_GetNumRoutes(lua_State* L) {
    uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;
    uint32_t routes;

    if (index < CGTaxiMap::m_numNodes) {
        TaxiNodeInfo* node = &CGTaxiMap::m_nodes.m_data[index];
        routes = node->m_noDirectPath ? node->m_numHops - 1 : 1;
    } else {
        routes = 0;
    }

    lua_pushnumber(L, routes);
    return 1;
}

// VERIFY: d1dc00e8 manual
// OFFSET: 0x00590F40
static int32_t Script_TaxiGetSrcX(lua_State* L) {
    int32_t line = static_cast<int32_t>(lua_tonumber(L, 2)) - 1;
    int32_t node = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;
    float value;

    if (node < 0 || node >= CGTaxiMap::m_numNodes || line < 0 || line >= static_cast<int32_t>(CGTaxiMap::m_routeLines.m_data[node].Count())) {
        value = 0.0f;
    } else {
        value = CGTaxiMap::m_routeLines.m_data[node].m_data[line].m_src.x;
    }

    lua_pushnumber(L, value);
    return 1;
}

// VERIFY: 7476c291 manual
// OFFSET: 0x00590FE0
static int32_t Script_TaxiGetSrcY(lua_State* L) {
    int32_t line = static_cast<int32_t>(lua_tonumber(L, 2)) - 1;
    int32_t node = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;
    float value;

    if (node < 0 || node >= CGTaxiMap::m_numNodes || line < 0 || line >= static_cast<int32_t>(CGTaxiMap::m_routeLines.m_data[node].Count())) {
        value = 0.0f;
    } else {
        value = CGTaxiMap::m_routeLines.m_data[node].m_data[line].m_src.y;
    }

    lua_pushnumber(L, value);
    return 1;
}

// VERIFY: 8a72d78b manual
// OFFSET: 0x00591080
static int32_t Script_TaxiGetDestX(lua_State* L) {
    int32_t line = static_cast<int32_t>(lua_tonumber(L, 2)) - 1;
    int32_t node = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;
    float value;

    if (node < 0 || node >= CGTaxiMap::m_numNodes || line < 0 || line >= static_cast<int32_t>(CGTaxiMap::m_routeLines.m_data[node].Count())) {
        value = 0.0f;
    } else {
        value = CGTaxiMap::m_routeLines.m_data[node].m_data[line].m_dest.x;
    }

    lua_pushnumber(L, value);
    return 1;
}

// VERIFY: 9f2e4158 manual
// OFFSET: 0x00591120
static int32_t Script_TaxiGetDestY(lua_State* L) {
    int32_t line = static_cast<int32_t>(lua_tonumber(L, 2)) - 1;
    int32_t node = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;
    float value;

    if (node < 0 || node >= CGTaxiMap::m_numNodes || line < 0 || line >= static_cast<int32_t>(CGTaxiMap::m_routeLines.m_data[node].Count())) {
        value = 0.0f;
    } else {
        value = CGTaxiMap::m_routeLines.m_data[node].m_data[line].m_dest.y;
    }

    lua_pushnumber(L, value);
    return 1;
}

// VERIFY: 80d905ad auto
// OFFSET: 0x00590EC0
static int32_t Script_CloseTaxiMap(lua_State* L) {
    CGTaxiMap::CloseMap();
    return 0;
}

// VERIFY: a6b4fd5b auto
// OFFSET: 0x00590ED0
static int32_t Script_TaxiNodeGetType(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: TaxiNodeGetType(slot)");
        return 0;
    }

    int64_t slot = static_cast<int64_t>(lua_tonumber(L, 1));
    lua_pushstring(L, CGTaxiMap::TaxiNodeType(static_cast<uint32_t>(slot - 1)));
    return 1;
}

// VERIFY: 9565b7c3 auto
// OFFSET: 0x005915E0
static int32_t Script_TaxiNodeCost(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: TaxiNodeCost(slot)");
        return 0;
    }

    uint32_t slot = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1))) - 1;

    if (slot >= static_cast<uint32_t>(CGTaxiMap::m_numNodes)) {
        luaL_error(L, "Invalid taxi node slot");
        return 0;
    }

    lua_pushnumber(L, TaxiNodeCost(slot));
    return 1;
}

// VERIFY: 113b9a85 auto
// OFFSET: 0x00591680
static int32_t Script_TakeTaxiNode(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: TakeTaxiNode(slot)");
        return 0;
    }

    int64_t slot = static_cast<int64_t>(lua_tonumber(L, 1));
    CGTaxiMap::TakeTaxiNode(static_cast<uint32_t>(slot - 1));
    return 0;
}

// VERIFY: 81d3cf0d auto
// OFFSET: 0x00591E60
static int32_t Script_TaxiNodeSetCurrent(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: TaxiNodeSetCurrent(slot)");
        return 0;
    }

    int64_t slot = static_cast<int64_t>(lua_tonumber(L, 1));
    CGTaxiMap::TaxiNodeSetCurrent(static_cast<uint32_t>(slot - 1));
    return 0;
}

// STATIC: 0x00ACF258
static FrameScript_Method s_ScriptFunctions[] = {
    { "SetTaxiMap", &Script_SetTaxiMap },
    { "NumTaxiNodes", &Script_NumTaxiNodes },
    { "TaxiNodeName", &Script_TaxiNodeName },
    { "TaxiNodePosition", &Script_TaxiNodePosition },
    { "TaxiNodeCost", &Script_TaxiNodeCost },
    { "TakeTaxiNode", &Script_TakeTaxiNode },
    { "CloseTaxiMap", &Script_CloseTaxiMap },
    { "TaxiNodeGetType", &Script_TaxiNodeGetType },
    { "TaxiNodeSetCurrent", &Script_TaxiNodeSetCurrent },
    { "TaxiGetSrcX", &Script_TaxiGetSrcX },
    { "TaxiGetSrcY", &Script_TaxiGetSrcY },
    { "TaxiGetDestX", &Script_TaxiGetDestX },
    { "TaxiGetDestY", &Script_TaxiGetDestY },
    { "GetNumRoutes", &Script_GetNumRoutes },
};

// VERIFY: 8e9fe376 auto
// OFFSET: 0x00590750
void CGTaxiMap::RegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_ScriptFunctions[i].m_name, s_ScriptFunctions[i].m_method);
    }
}

// VERIFY: a19c6364 auto
// OFFSET: 0x00590780
void CGTaxiMap::UnregisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_ScriptFunctions) / sizeof(s_ScriptFunctions[0]); i++) {
        FrameScript_UnregisterFunction(s_ScriptFunctions[i].m_name);
    }
}
