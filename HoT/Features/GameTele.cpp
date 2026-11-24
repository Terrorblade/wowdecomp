#include <string>
#include <vector>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::GameTele {

struct TeleEntry {
    uint32_t m_id;
    std::string m_name;
    uint32_t m_mapId;
};

static std::vector<TeleEntry> s_teles;
static bool s_hasData;

static int32_t Packet_SMSG_GAME_TELE_LIST(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t count = packet.GetUInt32();
    s_teles.clear();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        char name[512];
        TeleEntry entry;
        entry.m_id = packet.GetUInt32();
        packet.GetString(name, sizeof(name));
        entry.m_name = name;
        entry.m_mapId = packet.GetUInt32();
        s_teles.push_back(std::move(entry));
    }

    s_hasData = true;
    Lua::SignalEvent("HOT_GAME_TELES", nullptr);
    return 1;
}

static int32_t Script_RequestGameTeles(lua_State* L) {
    Net::Packet(Net::CMSG_GAME_TELE_LIST_REQUEST).Send();
    return 0;
}

static int32_t Script_GetGameTeleCount(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_hasData ? s_teles.size() : 0));
    return 1;
}

static int32_t Script_GetGameTeleInfo(lua_State* L) {
    uint32_t index = static_cast<uint32_t>(lua_tonumber(L, 1));

    if (!s_hasData || index < 1 || index > s_teles.size()) {
        lua_pushnil(L);
        return 1;
    }

    const TeleEntry& entry = s_teles[index - 1];
    lua_pushnumber(L, entry.m_id);
    lua_pushstring(L, entry.m_name.c_str());
    lua_pushnumber(L, entry.m_mapId);
    return 3;
}

class GameTeleModule : public Module {
    public:
    const char* Name() const override {
        return "GameTele";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_GAME_TELE_LIST, Packet_SMSG_GAME_TELE_LIST);

        Lua::RegisterFunction("RequestGameTeles", Script_RequestGameTeles);
        Lua::RegisterFunction("GetGameTeleCount", Script_GetGameTeleCount);
        Lua::RegisterFunction("GetGameTeleInfo", Script_GetGameTeleInfo);
    }
};

Module* GetModule() {
    static GameTeleModule module;
    return &module;
}

}
