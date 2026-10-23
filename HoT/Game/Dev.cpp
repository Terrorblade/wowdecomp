#include "HoT/Game/Dev.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "HoT/Core/Chat.h"
#include "HoT/Core/Modules.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/WorldClient/World.h"

namespace HoT::Dev {

struct RenderToggle {
    uint32_t m_flags;
    const char* m_onText;
    const char* m_offText;
};

static const RenderToggle s_wireframe = { Enable_ShowTris, "Wireframe mode on.", "Wireframe mode off." };
static const RenderToggle s_normals = { Enable_ShowNormals, "Normal display turned on.", "Normal display turned off." };
static const RenderToggle s_groundEffects = { Enable_DetailDoodads, "Ground clutter shown.", "Ground clutter hidden." };
static const RenderToggle s_liquids = { Enable_Water | Enable_Particulates, "Liquids shown.", "Liquids hidden." };
static const RenderToggle s_terrain = { Enable_Chunks, "Terrain shown.", "Terrain hidden." };
static const RenderToggle s_terrainCulling = { Enable_Culling, "Terrain culling enabled.", "Terrain culling disabled." };
static const RenderToggle s_wmo = { Enable_MapObjs, "WMOs shown.", "WMOs hidden." };
static const RenderToggle s_m2 = { Enable_Doodads, "Client-side M2s shown.", "Client-side M2s hidden." };

static void Toggle(const RenderToggle& toggle) {
    CWorld::enables ^= toggle.m_flags;
    Chat::Print("%s", (CWorld::enables & toggle.m_flags) ? toggle.m_onText : toggle.m_offText);
}

static bool CheckClearance(lua_State* L) {
    if (Session::HasClearance(2)) {
        return true;
    }

    luaL_error(L, "You don't have the clearance for this function.");
    return false;
}

static int32_t Packet_SMSG_TB_BULLSHIT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    static const RenderToggle* const s_toggles[] = { &s_wireframe, &s_normals, &s_groundEffects, &s_liquids, &s_terrain, &s_terrainCulling, &s_wmo };

    int32_t type = Net::Packet(msg).GetInt32();

    if (type >= 1 && type <= static_cast<int32_t>(sizeof(s_toggles) / sizeof(s_toggles[0]))) {
        Toggle(*s_toggles[type - 1]);
    }

    return 1;
}

template <const RenderToggle& T>
static int32_t Script_Toggle(lua_State* L) {
    if (Session::HasClearance(2)) {
        Toggle(T);
    }

    return 0;
}

static int32_t Script_ReloadMap(lua_State* L) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    int32_t mapId = CGGameUI::m_iCurrentMapID;

    if (!Session::HasClearance(2) || !player || mapId < 0) {
        return 0;
    }

    MapRec* map = g_mapDB.GetRecord(mapId);

    if (!map) {
        return 0;
    }

    C3Vector position = player->GetRawPosition();
    World::UnloadMap();
    CWorld::LoadMap(map->m_directory, &position, mapId, 1);
    Chat::Print("Map ID: %d (Directory: \"%s\", x: %f, y: %f, z: %f) reloaded.", mapId, map->m_directory, position.x, position.y, position.z);
    return 0;
}

static int32_t Script_PlayAnimation(lua_State* L) {
    if (!CheckClearance(L)) {
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: PlayAnimation(animationId[, \"unit\"])");
        return 0;
    }

    uint32_t animationId = static_cast<uint32_t>(lua_tonumber(L, 1));
    const char* token = "player";
    WOWGUID guid = ClntObjMgrGetActivePlayer();

    if (lua_type(L, 2) == LUA_TSTRING) {
        token = lua_tolstring(L, 2, nullptr);
        guid = WOWGUID();
        Script_GetGUIDFromToken(token, guid, false);
    }

    if (!guid) {
        luaL_error(L, "PlayAnimation: \"%s\" is not a valid unit.", token);
        return 0;
    }

    CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__));

    if (!unit) {
        luaL_error(L, "PlayAnimation: \"%s\" is not in range / not loaded.", token);
        return 0;
    }

    unit->PlayBaseAnimation(animationId, 0);
    Chat::Print("Playing animation %u on %s.", animationId, token);
    return 0;
}

static int32_t Script_GetEventId(lua_State* L) {
    if (!CheckClearance(L)) {
        return 0;
    }

    lua_pushnumber(L, Lua::EventIndex(lua_tolstring(L, 1, nullptr)));
    return 1;
}

template <int32_t Axis>
static int32_t Script_GetPlayerPosition(lua_State* L) {
    if (!CheckClearance(L)) {
        return 0;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return 0;
    }

    C3Vector position = player->GetRawPosition();
    lua_pushnumber(L, Axis == 0 ? position.x : Axis == 1 ? position.y : position.z);
    return 1;
}

class DevModule : public Module {
    public:
    const char* Name() const override {
        return "Dev";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_TB_BULLSHIT, Packet_SMSG_TB_BULLSHIT);

        Lua::RegisterFunction("ReloadMap", Script_ReloadMap);
        Lua::RegisterFunction("ToggleDisplayNormals", Script_Toggle<s_normals>);
        Lua::RegisterFunction("ToggleGroundEffects", Script_Toggle<s_groundEffects>);
        Lua::RegisterFunction("ToggleM2", Script_Toggle<s_m2>);
        Lua::RegisterFunction("ToggleLiquids", Script_Toggle<s_liquids>);
        Lua::RegisterFunction("ToggleTerrain", Script_Toggle<s_terrain>);
        Lua::RegisterFunction("ToggleTerrainCulling", Script_Toggle<s_terrainCulling>);
        Lua::RegisterFunction("ToggleWireframeMode", Script_Toggle<s_wireframe>);
        Lua::RegisterFunction("ToggleWMO", Script_Toggle<s_wmo>);
        Lua::RegisterFunction("PlayAnimation", Script_PlayAnimation);
        Lua::RegisterFunction("GetEventId", Script_GetEventId);
        Lua::RegisterFunction("GetPlayerX", Script_GetPlayerPosition<0>);
        Lua::RegisterFunction("GetPlayerY", Script_GetPlayerPosition<1>);
        Lua::RegisterFunction("GetPlayerZ", Script_GetPlayerPosition<2>);
    }
};

Module* GetModule() {
    static DevModule module;
    return &module;
}

}
