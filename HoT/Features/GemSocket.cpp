#include "HoT/Features/GemSocket.h"
#include <string>
#include <vector>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/Object/ObjectClient/Bag_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ContainerFrame.h"
#include "WoW/Source/Ui/TradeSkillFrame.h"

namespace HoT::GemSocket {

static const uint8_t GEM_SOCKET_OPEN_PANEL = 1;
static const int32_t JEWELCRAFTING_SPELL = 25229;
static const uint32_t ITEM_SPELLTRIGGER_ON_SOCKET = 3;
static const uint8_t NO_PENDING_LOADOUT = 0xFF;

struct SocketEntry {
    uint8_t m_slot;
    uint32_t m_itemId;
    uint8_t m_state;
};

struct LoadoutEntry {
    uint8_t m_id;
    std::string m_name;
    uint8_t m_sockets;
    uint8_t m_filled;
    uint8_t m_purchased;
};

static uint8_t s_totalSockets;
static uint8_t s_socketCount;
static std::vector<SocketEntry> s_sockets;
static bool s_hasData;

static uint32_t s_costItemId;
static uint32_t s_cost;
static uint8_t s_purchased;
static uint8_t s_canPurchase;
static bool s_hasCost;

static std::vector<LoadoutEntry> s_loadouts;
static uint8_t s_activeLoadout;
static uint8_t s_pendingLoadout = NO_PENDING_LOADOUT;
static uint8_t s_maxLoadouts;
static uint32_t s_unlockItemId;
static uint32_t s_unlockCost;
static uint8_t s_canUnlock;
static bool s_hasLoadouts;

static bool IsSpellGem(uint32_t entry) {
    ItemStats_C* stats = g_itemDBCache.GetRecord(entry, WOWGUID(), nullptr, nullptr, false);

    if (!stats) {
        return false;
    }

    for (int32_t i = 0; i < 5; i++) {
        if (stats->m_spellTrigger[i] == ITEM_SPELLTRIGGER_ON_SOCKET && static_cast<int32_t>(stats->m_spellId[i]) > 0) {
            return true;
        }
    }

    return false;
}

static int32_t Packet_SMSG_GEM_SOCKET_LIST(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t total = packet.GetUInt8();
    uint8_t count = packet.GetUInt8();

    s_totalSockets = total;
    s_socketCount = count;
    s_sockets.clear();
    s_sockets.reserve(count);

    for (uint8_t i = 0; i < count && packet.CanRead(6); i++) {
        SocketEntry entry;
        entry.m_slot = packet.GetUInt8();
        entry.m_itemId = packet.GetUInt32();
        entry.m_state = packet.GetUInt8();
        s_sockets.push_back(entry);
    }

    s_hasData = true;
    Lua::SignalEvent("HOT_GEM_SOCKET_UPDATE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_GEM_SOCKET_ERROR(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    int8_t code = Net::Packet(msg).GetInt8();
    Lua::SignalEvent("HOT_GEM_SOCKET_ERROR", "%d", static_cast<int32_t>(code));
    return 1;
}

static int32_t Packet_SMSG_GEM_SOCKET_COST(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_costItemId = packet.GetUInt32();
    s_cost = packet.GetUInt32();
    s_purchased = packet.GetUInt8();
    s_canPurchase = packet.GetUInt8();
    s_hasCost = true;
    Lua::SignalEvent("HOT_GEM_SOCKET_COST_UPDATE", nullptr);
    return 1;
}

// An old server sends no mode byte, the short read gives 0 and opens Jewelcrafting.
static int32_t Packet_SMSG_GEM_SOCKET_OPEN(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    uint8_t mode = Net::Packet(msg).GetUInt8();

    if (mode == GEM_SOCKET_OPEN_PANEL) {
        Lua::SignalEvent("HOT_GEM_SOCKET_OPEN", nullptr);
        return 1;
    }

    WOWGUID guid = ClntObjMgrGetActivePlayer();

    if (!guid) {
        return 1;
    }

    CGTradeSkillInfo::SetTradeSkill(JEWELCRAFTING_SPELL, guid, nullptr, 0, 0);
    return 1;
}

static int32_t Packet_SMSG_GEM_LOADOUTS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_activeLoadout = packet.GetUInt8();
    s_pendingLoadout = packet.GetUInt8();
    s_maxLoadouts = packet.GetUInt8();
    s_unlockItemId = packet.GetUInt32();
    s_unlockCost = packet.GetUInt32();
    s_canUnlock = packet.GetUInt8();

    uint8_t count = packet.GetUInt8();
    s_loadouts.clear();
    s_loadouts.reserve(count);

    for (uint8_t i = 0; i < count && packet.CanRead(1); i++) {
        LoadoutEntry entry;
        entry.m_id = packet.GetUInt8();
        char name[128] = {};
        packet.GetString(name, sizeof(name));
        entry.m_name = name;
        entry.m_sockets = packet.GetUInt8();
        entry.m_filled = packet.GetUInt8();
        entry.m_purchased = packet.GetUInt8();
        s_loadouts.push_back(std::move(entry));
    }

    s_hasLoadouts = true;
    Lua::SignalEvent("HOT_GEM_LOADOUTS", "%u", static_cast<uint32_t>(s_activeLoadout));
    return 1;
}

static int32_t Packet_SMSG_GEM_LOADOUT_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t op = packet.GetUInt8();
    uint8_t result = packet.GetUInt8();
    uint8_t loadoutId = packet.GetUInt8();
    Lua::SignalEvent("HOT_GEM_LOADOUT_RESULT", "%u%u%u", static_cast<uint32_t>(op), static_cast<uint32_t>(result), static_cast<uint32_t>(loadoutId));
    return 1;
}

static int32_t Script_RequestGemSocketList(lua_State* L) {
    Net::Packet(Net::CMSG_GEM_SOCKET_LIST_REQUEST).Send();
    return 0;
}

static int32_t Script_GetGemSocketCount(lua_State* L) {
    lua_pushnumber(L, s_hasData ? s_totalSockets : 0);
    return 1;
}

static int32_t Script_GetGemSocketEntryCount(lua_State* L) {
    lua_pushnumber(L, s_hasData ? s_socketCount : 0);
    return 1;
}

static int32_t Script_GetActiveFilledCount(lua_State* L) {
    uint8_t count = 0;

    for (const SocketEntry& entry : s_sockets) {
        if (entry.m_state == 1) {
            count++;
        }
    }

    lua_pushnumber(L, count);
    return 1;
}

static int32_t Script_GetGemSocketInfo(lua_State* L) {
    if (!s_hasData) {
        lua_pushnil(L);
        return 1;
    }

    uint32_t slot = static_cast<uint32_t>(lua_tonumber(L, 1));

    for (const SocketEntry& entry : s_sockets) {
        if (entry.m_slot == slot) {
            lua_pushnumber(L, entry.m_itemId);
            lua_pushnumber(L, entry.m_state);
            return 2;
        }
    }

    lua_pushnil(L);
    return 1;
}

static int32_t Script_InsertGem(lua_State* L) {
    uint8_t slot = static_cast<uint8_t>(lua_tonumber(L, 1));
    uint32_t itemId = static_cast<uint32_t>(lua_tonumber(L, 2));
    Net::Packet(Net::CMSG_GEM_SOCKET_INSERT).PutUInt8(slot).PutUInt32(itemId).Send();
    return 0;
}

static int32_t Script_RemoveGem(lua_State* L) {
    int8_t slot = static_cast<int8_t>(lua_tonumber(L, 1));
    uint8_t shatter = lua_toboolean(L, 2) ? 1 : 0;
    Net::Packet(Net::CMSG_GEM_SOCKET_REMOVE).PutInt8(slot).PutUInt8(shatter).Send();
    return 0;
}

static int32_t Script_PurchaseGemSocket(lua_State* L) {
    Net::Packet(Net::CMSG_GEM_SOCKET_PURCHASE).Send();
    return 0;
}

static int32_t Script_RequestGemSocketCost(lua_State* L) {
    Net::Packet(Net::CMSG_GEM_SOCKET_COST_REQUEST).Send();
    return 0;
}

static int32_t Script_GetGemSocketCost(lua_State* L) {
    if (!s_hasCost) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, s_costItemId);
    lua_pushnumber(L, s_cost);
    lua_pushnumber(L, s_purchased);
    lua_pushboolean(L, s_canPurchase != 0);
    return 4;
}

static int32_t Script_RequestGemLoadouts(lua_State* L) {
    Net::Packet(Net::CMSG_GEM_LOADOUTS_REQUEST).Send();
    return 0;
}

static int32_t Script_GetGemLoadouts(lua_State* L) {
    lua_createtable(L, static_cast<int32_t>(s_loadouts.size()), 0);
    int32_t table = lua_gettop(L);

    for (int32_t i = 0; i < static_cast<int32_t>(s_loadouts.size()); i++) {
        const LoadoutEntry& entry = s_loadouts[i];

        lua_createtable(L, 0, 5);
        int32_t row = lua_gettop(L);

        lua_pushnumber(L, entry.m_id);
        lua_setfield(L, row, "id");

        lua_pushstring(L, entry.m_name.c_str());
        lua_setfield(L, row, "name");

        lua_pushnumber(L, entry.m_sockets);
        lua_setfield(L, row, "sockets");

        lua_pushnumber(L, entry.m_filled);
        lua_setfield(L, row, "filled");

        lua_pushnumber(L, entry.m_purchased);
        lua_setfield(L, row, "purchased");

        lua_rawseti(L, table, i + 1);
    }

    lua_settop(L, table);
    return 1;
}

static int32_t Script_GetActiveGemLoadout(lua_State* L) {
    lua_pushnumber(L, s_activeLoadout);

    if (s_pendingLoadout == NO_PENDING_LOADOUT) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, s_pendingLoadout);
    }

    return 2;
}

static int32_t Script_GetMaxGemLoadouts(lua_State* L) {
    lua_pushnumber(L, s_maxLoadouts);
    return 1;
}

static int32_t Script_GetGemLoadoutUnlockCost(lua_State* L) {
    if (!s_hasLoadouts) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, s_unlockItemId);
    lua_pushnumber(L, s_unlockCost);
    lua_pushboolean(L, s_canUnlock != 0);
    return 3;
}

static int32_t Script_SwitchGemLoadout(lua_State* L) {
    uint8_t id = static_cast<uint8_t>(lua_tonumber(L, 1));
    Net::Packet(Net::CMSG_GEM_LOADOUT_SWITCH).PutUInt8(id).Send();
    return 0;
}

// An empty name is allowed, the server names the loadout after its position.
static int32_t Script_UnlockGemLoadout(lua_State* L) {
    const char* name = lua_tolstring(L, 1, nullptr);
    Net::Packet(Net::CMSG_GEM_LOADOUT_UNLOCK).PutString(name ? name : "").Send();
    return 0;
}

static int32_t Script_RenameGemLoadout(lua_State* L) {
    uint8_t id = static_cast<uint8_t>(lua_tonumber(L, 1));
    const char* name = lua_tolstring(L, 2, nullptr);

    if (!name || name[0] == '\0') {
        return 0;
    }

    Net::Packet(Net::CMSG_GEM_LOADOUT_RENAME).PutUInt8(id).PutString(name).Send();
    return 0;
}

bool HandleUseContainerItem(lua_State* L) {
    if (!Feature::IsEnabled(Feature::FEATURE_GEM_SYSTEM) || Crafting::IsOpen()) {
        return false;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return false;
    }

    CGBag_C* bag;
    int32_t slot;
    int32_t bank;

    if (!CGContainerInfo::GetLuaBagAndSlot(L, &bag, &slot, &bank)) {
        return false;
    }

    CGItem_C* item = bag->GetItemPointer(slot);

    if (!item || (item->m_itemFlags & 1)) {
        return false;
    }

    uint32_t entry = item->m_obj->m_entryID;

    if (!IsSpellGem(entry)) {
        return false;
    }

    Lua::SignalEvent("HOT_GEM_SOCKET_USE", "%u", entry);
    return true;
}

class GemSocketModule : public Module {
    public:
    const char* Name() const override {
        return "GemSocket";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_GEM_SOCKET_LIST, Packet_SMSG_GEM_SOCKET_LIST);
        Net::RegisterWorldHandler(Net::SMSG_GEM_SOCKET_ERROR, Packet_SMSG_GEM_SOCKET_ERROR);
        Net::RegisterWorldHandler(Net::SMSG_GEM_SOCKET_COST, Packet_SMSG_GEM_SOCKET_COST);
        Net::RegisterWorldHandler(Net::SMSG_GEM_SOCKET_OPEN, Packet_SMSG_GEM_SOCKET_OPEN);
        Net::RegisterWorldHandler(Net::SMSG_GEM_LOADOUTS, Packet_SMSG_GEM_LOADOUTS);
        Net::RegisterWorldHandler(Net::SMSG_GEM_LOADOUT_RESULT, Packet_SMSG_GEM_LOADOUT_RESULT);

        Lua::RegisterFunction("RequestGemSocketList", Script_RequestGemSocketList);
        Lua::RegisterFunction("GetGemSocketCount", Script_GetGemSocketCount);
        Lua::RegisterFunction("GetGemSocketEntryCount", Script_GetGemSocketEntryCount);
        Lua::RegisterFunction("GetGemSocketInfo", Script_GetGemSocketInfo);
        Lua::RegisterFunction("InsertGem", Script_InsertGem);
        Lua::RegisterFunction("RemoveGem", Script_RemoveGem);
        Lua::RegisterFunction("GetActiveFilledCount", Script_GetActiveFilledCount);
        Lua::RegisterFunction("PurchaseGemSocket", Script_PurchaseGemSocket);
        Lua::RegisterFunction("RequestGemSocketCost", Script_RequestGemSocketCost);
        Lua::RegisterFunction("GetGemSocketCost", Script_GetGemSocketCost);
        Lua::RegisterFunction("RequestGemLoadouts", Script_RequestGemLoadouts);
        Lua::RegisterFunction("GetGemLoadouts", Script_GetGemLoadouts);
        Lua::RegisterFunction("GetActiveGemLoadout", Script_GetActiveGemLoadout);
        Lua::RegisterFunction("GetMaxGemLoadouts", Script_GetMaxGemLoadouts);
        Lua::RegisterFunction("GetGemLoadoutUnlockCost", Script_GetGemLoadoutUnlockCost);
        Lua::RegisterFunction("SwitchGemLoadout", Script_SwitchGemLoadout);
        Lua::RegisterFunction("UnlockGemLoadout", Script_UnlockGemLoadout);
        Lua::RegisterFunction("RenameGemLoadout", Script_RenameGemLoadout);
    }
};

Module* GetModule() {
    static GemSocketModule module;
    return &module;
}

}
