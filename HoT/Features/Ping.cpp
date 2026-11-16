#include <cmath>
#include <iterator>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Ping {

enum HitFilter : int32_t {
    HIT_FILTER_ALL = 0,
    HIT_FILTER_ENVIRONMENT = 1,
    HIT_FILTER_UNITS = 2,
};

static constexpr uint32_t HIT_FLAGS_ALL = 0x5D;
static constexpr uint32_t HIT_FLAGS_UNITS = 0x18;
static constexpr uint8_t RESTRICTION_QUERY = 0xFF;
static constexpr float NAME_LIFT = 0.6666667f;

static void NDCToUI(float ndcX, float ndcY, float& x, float& y) {
    x = ndcX * CoordinateGetAspectCompensation() * 1024.0f;
    y = ndcY * 768.0f;
}

static void DDCToUI(float ddcX, float ddcY, float& x, float& y) {
    float ndcX = 0.0f;
    float ndcY = 0.0f;
    DDCToNDC(ddcX, ddcY, &ndcX, &ndcY);
    NDCToUI(ndcX, ndcY, x, y);
}

// A point behind the camera is mirrored through it so the direction to it stays right.
static bool ProjectToUI(const C3Vector& world, float& x, float& y, bool& onScreen, bool& behind) {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;
    if (!frame || !frame->m_camera) {
        return false;
    }

    C3Vector screen = { 0.0f, 0.0f, 0.0f };
    uint32_t edges = 0;
    bool visible = frame->GetScreenCoordinates(world, &screen, &edges);

    if (edges) {
        DDCToUI(screen.x, screen.y, x, y);
        onScreen = visible;
        behind = false;
        return true;
    }

    const C3Vector& camera = frame->m_camera->m_position;
    C3Vector mirrored = { 2.0f * camera.x - world.x, 2.0f * camera.y - world.y, 2.0f * camera.z - world.z };
    edges = 0;
    frame->GetScreenCoordinates(mirrored, &screen, &edges);

    if (!edges) {
        return false;
    }

    float mirroredX = 0.0f;
    float mirroredY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    DDCToUI(screen.x, screen.y, mirroredX, mirroredY);
    NDCToUI(1.0f, 1.0f, width, height);
    x = width - mirroredX;
    y = height - mirroredY;
    onScreen = false;
    behind = true;
    return true;
}

static int32_t PushScreenPosition(lua_State* L, const C3Vector& world) {
    float x = 0.0f;
    float y = 0.0f;
    bool onScreen = false;
    bool behind = false;

    if (!ProjectToUI(world, x, y, onScreen, behind)) {
        return 0;
    }

    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushboolean(L, onScreen ? 1 : 0);
    lua_pushboolean(L, behind ? 1 : 0);
    return 4;
}

static void PushGuid(lua_State* L, WOWGUID guid) {
    char buffer[32] = {};
    lua_pushstring(L, GUIDToHexString(guid, buffer));
}

// Takes a guid string from UnitGUID or a unit token.
static WOWGUID GuidArg(lua_State* L, int32_t index) {
    if (lua_type(L, index) != LUA_TSTRING) {
        return WOWGUID();
    }

    const char* text = lua_tolstring(L, index, nullptr);
    if (!text || !*text) {
        return WOWGUID();
    }

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        return StringToGuid(text);
    }

    WOWGUID guid;
    if (!Script_GetGUIDFromToken(text, guid, false)) {
        return WOWGUID();
    }

    return guid;
}

static CGObject_C* UnitArg(lua_State* L, int32_t index) {
    WOWGUID guid = GuidArg(L, index);
    return guid ? ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__) : nullptr;
}

static void SendRestriction(uint8_t value) {
    Net::Packet(Net::CMSG_SET_PING_RESTRICTION).PutUInt8(value).Send();
}

static int32_t Script_HitTest(lua_State* L) {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;
    CSimpleTop* top = CSimpleTop::s_instance;
    if (!frame || !top) {
        return 0;
    }

    float ddcX = 0.0f;
    float ddcY = 0.0f;
    NDCToDDC(top->m_mousePosition.m_x, top->m_mousePosition.m_y, &ddcX, &ddcY);

    int32_t filter = lua_isnumber(L, 1) ? static_cast<int32_t>(lua_tonumber(L, 1)) : HIT_FILTER_ALL;
    uint32_t flags = HIT_FLAGS_ALL;

    if (filter == HIT_FILTER_ENVIRONMENT) {
        flags = HIT_FLAGS_ALL & ~HIT_FLAGS_UNITS;
    } else if (filter == HIT_FILTER_UNITS) {
        flags = HIT_FLAGS_UNITS;
    }

    HITTESTRESULT result = {};
    int32_t kind = frame->HitTestPointFlags(ddcX, ddcY, flags, &result);

    if (kind == CGWorldFrame::HIT_OBJECT && result.m_guid && ClntObjMgrObjectPtr(result.m_guid, TYPE_UNIT, __FILE__, __LINE__)) {
        lua_pushstring(L, "unit");
        PushGuid(L, result.m_guid);
        return 2;
    }

    if (kind == CGWorldFrame::HIT_NONE || filter == HIT_FILTER_UNITS) {
        return 0;
    }

    C3Vector point = result.m_point;

    // A transport hit comes back in the transport's local space, so rebuild it from the segment.
    if (kind == CGWorldFrame::HIT_WORLD_OBJECT && result.m_guid) {
        float dx = result.m_segEnd.x - result.m_segStart.x;
        float dy = result.m_segEnd.y - result.m_segStart.y;
        float dz = result.m_segEnd.z - result.m_segStart.z;
        float length = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (length > 0.0f) {
            float scale = result.m_distance / length;
            point = { result.m_segStart.x + dx * scale, result.m_segStart.y + dy * scale, result.m_segStart.z + dz * scale };
        }
    }

    lua_pushstring(L, "point");
    lua_pushnil(L);
    lua_pushnumber(L, point.x);
    lua_pushnumber(L, point.y);
    lua_pushnumber(L, point.z);
    return 5;
}

static int32_t Script_SendPing(lua_State* L) {
    Net::Packet(Net::CMSG_SEND_PING)
        .PutUInt8(static_cast<uint8_t>(lua_tonumber(L, 1)))
        .PutUInt64(GuidArg(L, 2).m_guid)
        .PutFloat(static_cast<float>(lua_tonumber(L, 3)))
        .PutFloat(static_cast<float>(lua_tonumber(L, 4)))
        .PutFloat(static_cast<float>(lua_tonumber(L, 5)))
        .Send();
    return 0;
}

static int32_t Script_GetScreenPosition(lua_State* L) {
    C3Vector world = {
        static_cast<float>(lua_tonumber(L, 1)),
        static_cast<float>(lua_tonumber(L, 2)),
        static_cast<float>(lua_tonumber(L, 3)),
    };
    return PushScreenPosition(L, world);
}

static int32_t Script_GetUnitScreenPosition(lua_State* L) {
    CGObject_C* unit = UnitArg(L, 1);
    if (!unit) {
        return 0;
    }

    C3Vector world = unit->GetNamePosition();
    world.z += NAME_LIFT;
    return PushScreenPosition(L, world);
}

static int32_t Script_GetUnitPosition(lua_State* L) {
    CGObject_C* unit = UnitArg(L, 1);
    if (!unit) {
        return 0;
    }

    C3Vector position = unit->GetPosition();
    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    return 3;
}

static int32_t Script_SetRestriction(lua_State* L) {
    SendRestriction(static_cast<uint8_t>(lua_tonumber(L, 1)));
    return 0;
}

static int32_t Script_RequestRestriction(lua_State* L) {
    SendRestriction(RESTRICTION_QUERY);
    return 0;
}

static int32_t Packet_SMSG_RECEIVE_PING(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    WOWGUID sender = { packet.GetUInt64() };
    uint32_t type = packet.GetUInt8();
    WOWGUID target = { packet.GetUInt64() };
    float x = packet.GetFloat();
    float y = packet.GetFloat();
    float z = packet.GetFloat();
    int32_t sameMap = packet.GetUInt8() ? 1 : 0;
    char senderName[64];
    char targetName[128];
    packet.GetString(senderName, sizeof(senderName));
    packet.GetString(targetName, sizeof(targetName));

    if (msg->m_read > msg->m_size || !sender) {
        return 1;
    }

    char senderGuid[32] = {};
    char targetGuid[32] = {};
    GUIDToHexString(sender, senderGuid);

    if (target) {
        GUIDToHexString(target, targetGuid);
    }

    Lua::SignalEvent("HOT_PING_RECEIVED", "%s%u%s%f%f%f%b%s%s", senderGuid, type, target ? targetGuid : nullptr, static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), sameMap, senderName, targetName);
    return 1;
}

static int32_t Packet_SMSG_PING_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t result = packet.GetUInt8();
    uint32_t cooldownMs = packet.GetUInt32();

    Lua::SignalEvent("HOT_PING_RESULT", "%u%u", result, cooldownMs);
    return 1;
}

static int32_t Packet_SMSG_PING_RESTRICTION(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t restriction = packet.GetUInt8();

    Lua::SignalEvent("HOT_PING_RESTRICTION", "%u", restriction);
    return 1;
}

static void Install(lua_State* L) {
    if (!L) {
        return;
    }

    static const luaL_Reg functions[] = {
        { "HitTest", Script_HitTest },
        { "SendPing", Script_SendPing },
        { "GetScreenPosition", Script_GetScreenPosition },
        { "GetUnitScreenPosition", Script_GetUnitScreenPosition },
        { "GetUnitPosition", Script_GetUnitPosition },
        { "SetRestriction", Script_SetRestriction },
        { "RequestRestriction", Script_RequestRestriction },
    };

    lua_createtable(L, 0, static_cast<int32_t>(std::size(functions)));

    for (const luaL_Reg& entry : functions) {
        lua_pushcclosure(L, entry.func, 0);
        lua_setfield(L, -2, entry.name);
    }

    lua_setfield(L, LUA_GLOBALSINDEX, "C_Ping");
}

class PingModule : public Module {
    public:
    const char* Name() const override {
        return "Ping";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_RECEIVE_PING, Packet_SMSG_RECEIVE_PING);
        Net::RegisterWorldHandler(Net::SMSG_PING_RESULT, Packet_SMSG_PING_RESULT);
        Net::RegisterWorldHandler(Net::SMSG_PING_RESTRICTION, Packet_SMSG_PING_RESTRICTION);
    }

    void OnScriptsLoaded(Lua::State state) override {
        if (state == Lua::State::Game) {
            Install(FrameScript_GetContext());
        }
    }
};

Module* GetModule() {
    static PingModule module;
    return &module;
}

}
