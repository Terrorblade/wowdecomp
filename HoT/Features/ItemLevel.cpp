#include <unordered_map>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::ItemLevel {

static std::unordered_map<uint8_t, uint32_t> s_slotLevels;
static uint32_t s_averageItemLevel;
static uint8_t s_subClass;
static bool s_hasData;

static void Clear() {
    s_slotLevels.clear();
    s_averageItemLevel = 0;
    s_subClass = 0;
    s_hasData = false;
}

static int32_t Packet_SMSG_ITEM_LEVEL_BREAKDOWN(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_subClass = packet.GetUInt8();
    uint8_t count = packet.GetUInt8();
    s_slotLevels.clear();

    for (uint8_t i = 0; i < count && packet.CanRead(5); i++) {
        uint8_t slot = packet.GetUInt8();
        s_slotLevels[slot] = packet.GetUInt32();
    }

    s_averageItemLevel = packet.GetUInt32();
    s_hasData = true;
    Lua::SignalEvent("HOT_ITEM_LEVEL_UPDATE", nullptr);
    return 1;
}

static int32_t Script_RequestItemLevelBreakdown(lua_State* L) {
    Net::Packet(Net::CMSG_ITEM_LEVEL_BREAKDOWN_REQUEST).Send();
    return 0;
}

static int32_t Script_HasItemLevelBreakdown(lua_State* L) {
    lua_pushboolean(L, s_hasData);
    return 1;
}

static int32_t Script_GetItemLevelSubClass(lua_State* L) {
    lua_pushnumber(L, s_hasData ? s_subClass : 0);
    return 1;
}

static int32_t Script_GetItemLevelBreakdownSlot(lua_State* L) {
    auto it = s_slotLevels.find(static_cast<uint8_t>(lua_tonumber(L, 1)));

    if (!s_hasData || it == s_slotLevels.end()) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, it->second);
    }

    return 1;
}

static int32_t Script_GetItemLevelBreakdownAverage(lua_State* L) {
    if (s_hasData) {
        lua_pushnumber(L, s_averageItemLevel);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

class ItemLevelModule : public Module {
    public:
    const char* Name() const override {
        return "ItemLevel";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_ITEM_LEVEL_BREAKDOWN, Packet_SMSG_ITEM_LEVEL_BREAKDOWN);

        Lua::RegisterFunction("RequestItemLevelBreakdown", Script_RequestItemLevelBreakdown);
        Lua::RegisterFunction("HasItemLevelBreakdown", Script_HasItemLevelBreakdown);
        Lua::RegisterFunction("GetItemLevelSubClass", Script_GetItemLevelSubClass);
        Lua::RegisterFunction("GetItemLevelBreakdownSlot", Script_GetItemLevelBreakdownSlot);
        Lua::RegisterFunction("GetItemLevelBreakdownAverage", Script_GetItemLevelBreakdownAverage);
    }

    void OnEnterWorld() override {
        Clear();
    }
};

Module* GetModule() {
    static ItemLevelModule module;
    return &module;
}

}
