#include <string>
#include <vector>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::ItemGen {

struct StatGroupEntry {
    int32_t m_id;
    std::string m_name;
    uint32_t m_mask;
    uint32_t m_flags;
    uint32_t m_weight;
    int32_t m_trinketGroup;
};

struct StatGroupMaskEntry {
    uint32_t m_bit;
    std::string m_name;
};

struct LegendaryEntry {
    uint32_t m_id;
    std::string m_name;
    std::string m_nameOverride;
    uint32_t m_flags;
    int32_t m_minItemLevel;
    int32_t m_maxItemLevel;
    int32_t m_itemClass;
    int32_t m_itemSubClass;
    int32_t m_inventoryType;
    int32_t m_statGroup;
    uint32_t m_statGroupMask;
    int32_t m_statGroupOverride;
    uint32_t m_uniqueId;
};

struct UniqueEntry {
    uint32_t m_id;
    std::string m_name;
    std::string m_description;
    uint32_t m_quality;
    uint32_t m_itemClass;
    uint32_t m_subClass;
    uint32_t m_inventoryType;
    int32_t m_statGroup;
    uint32_t m_flags;
    uint32_t m_itemLevelMin;
    uint32_t m_itemLevelMax;
    bool m_spawnable;
};

static std::vector<StatGroupEntry> s_groups;
static std::vector<StatGroupMaskEntry> s_maskBits;
static std::vector<LegendaryEntry> s_legendaries;
static std::vector<UniqueEntry> s_uniques;
static bool s_hasGroups;
static bool s_hasLegendaries;
static bool s_hasUniques;

static std::string ReadString(Net::Packet& packet, uint32_t size) {
    char buffer[1024];
    packet.GetString(buffer, size < sizeof(buffer) ? size : sizeof(buffer));
    return buffer;
}

// Lua indices are 1 based, returns null when out of range or not received yet.
template <typename T>
static const T* GetEntry(lua_State* L, const std::vector<T>& entries, bool hasData) {
    uint32_t index = static_cast<uint32_t>(lua_tonumber(L, 1));

    if (!hasData || index < 1 || index > entries.size()) {
        lua_pushnil(L);
        return nullptr;
    }

    return &entries[index - 1];
}

static int32_t Packet_SMSG_ITEMGEN_STAT_GROUPS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_groups.clear();
    s_maskBits.clear();

    uint32_t count = packet.GetUInt32();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        StatGroupEntry entry;
        entry.m_id = packet.GetInt32();
        entry.m_name = ReadString(packet, 128);
        entry.m_mask = packet.GetUInt32();
        entry.m_flags = packet.GetUInt32();
        entry.m_weight = packet.GetUInt32();
        entry.m_trinketGroup = packet.GetInt32();
        s_groups.push_back(std::move(entry));
    }

    uint32_t maskCount = packet.GetUInt32();

    for (uint32_t i = 0; i < maskCount && packet.CanRead(1); i++) {
        StatGroupMaskEntry entry;
        entry.m_bit = packet.GetUInt32();
        entry.m_name = ReadString(packet, 128);
        s_maskBits.push_back(std::move(entry));
    }

    s_hasGroups = true;
    Lua::SignalEvent("HOT_STAT_GROUPS", nullptr);
    return 1;
}

static int32_t Packet_SMSG_ITEMGEN_LEGENDARIES(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_legendaries.clear();

    uint32_t count = packet.GetUInt32();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        LegendaryEntry entry;
        entry.m_id = packet.GetUInt32();
        entry.m_name = ReadString(packet, 256);
        entry.m_nameOverride = ReadString(packet, 256);
        entry.m_flags = packet.GetUInt32();
        entry.m_minItemLevel = packet.GetInt32();
        entry.m_maxItemLevel = packet.GetInt32();
        entry.m_itemClass = packet.GetInt32();
        entry.m_itemSubClass = packet.GetInt32();
        entry.m_inventoryType = packet.GetInt32();
        entry.m_statGroup = packet.GetInt32();
        entry.m_statGroupMask = packet.GetUInt32();
        entry.m_statGroupOverride = packet.GetInt32();
        entry.m_uniqueId = packet.GetUInt32();
        s_legendaries.push_back(std::move(entry));
    }

    s_hasLegendaries = true;
    Lua::SignalEvent("HOT_LEGENDARIES", nullptr);
    return 1;
}

static int32_t Packet_SMSG_ITEMGEN_UNIQUES(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_uniques.clear();

    uint32_t count = packet.GetUInt32();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        UniqueEntry entry;
        entry.m_id = packet.GetUInt32();
        entry.m_name = ReadString(packet, 1024);
        entry.m_description = ReadString(packet, 1024);
        entry.m_quality = packet.GetUInt32();
        entry.m_itemClass = packet.GetUInt32();
        entry.m_subClass = packet.GetUInt32();
        entry.m_inventoryType = packet.GetUInt32();
        entry.m_statGroup = packet.GetInt32();
        entry.m_flags = packet.GetUInt32();
        entry.m_itemLevelMin = packet.GetUInt32();
        entry.m_itemLevelMax = packet.GetUInt32();
        entry.m_spawnable = packet.GetUInt8() != 0;
        s_uniques.push_back(std::move(entry));
    }

    s_hasUniques = true;
    Lua::SignalEvent("HOT_UNIQUE_ITEMS", nullptr);
    return 1;
}

static int32_t Script_RequestStatGroups(lua_State* L) {
    Net::Packet(Net::CMSG_ITEMGEN_STAT_GROUPS_REQUEST).Send();
    return 0;
}

static int32_t Script_GetStatGroupCount(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_hasGroups ? s_groups.size() : 0));
    return 1;
}

static int32_t Script_GetStatGroupInfo(lua_State* L) {
    const StatGroupEntry* entry = GetEntry(L, s_groups, s_hasGroups);

    if (!entry) {
        return 1;
    }

    lua_pushnumber(L, entry->m_id);
    lua_pushstring(L, entry->m_name.c_str());
    lua_pushnumber(L, entry->m_mask);
    lua_pushnumber(L, entry->m_flags);
    lua_pushnumber(L, entry->m_weight);
    lua_pushnumber(L, entry->m_trinketGroup);
    return 6;
}

static int32_t Script_GetStatGroupMaskCount(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_hasGroups ? s_maskBits.size() : 0));
    return 1;
}

static int32_t Script_GetStatGroupMaskInfo(lua_State* L) {
    const StatGroupMaskEntry* entry = GetEntry(L, s_maskBits, s_hasGroups);

    if (!entry) {
        return 1;
    }

    lua_pushnumber(L, entry->m_bit);
    lua_pushstring(L, entry->m_name.c_str());
    return 2;
}

static int32_t Script_RequestLegendaries(lua_State* L) {
    Net::Packet(Net::CMSG_ITEMGEN_LEGENDARIES_REQUEST).Send();
    return 0;
}

static int32_t Script_GetLegendaryCount(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_hasLegendaries ? s_legendaries.size() : 0));
    return 1;
}

static int32_t Script_GetLegendaryInfo(lua_State* L) {
    const LegendaryEntry* entry = GetEntry(L, s_legendaries, s_hasLegendaries);

    if (!entry) {
        return 1;
    }

    lua_pushnumber(L, entry->m_id);
    lua_pushstring(L, entry->m_name.c_str());
    lua_pushstring(L, entry->m_nameOverride.c_str());
    lua_pushnumber(L, entry->m_flags);
    lua_pushnumber(L, entry->m_minItemLevel);
    lua_pushnumber(L, entry->m_maxItemLevel);
    lua_pushnumber(L, entry->m_itemClass);
    lua_pushnumber(L, entry->m_itemSubClass);
    lua_pushnumber(L, entry->m_inventoryType);
    lua_pushnumber(L, entry->m_statGroup);
    lua_pushnumber(L, entry->m_statGroupMask);
    lua_pushnumber(L, entry->m_statGroupOverride);
    lua_pushnumber(L, entry->m_uniqueId);
    return 13;
}

static int32_t Script_RequestUniqueItems(lua_State* L) {
    Net::Packet(Net::CMSG_ITEMGEN_UNIQUES_REQUEST).Send();
    return 0;
}

static int32_t Script_GetUniqueItemCount(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_hasUniques ? s_uniques.size() : 0));
    return 1;
}

static int32_t Script_GetUniqueItemInfo(lua_State* L) {
    const UniqueEntry* entry = GetEntry(L, s_uniques, s_hasUniques);

    if (!entry) {
        return 1;
    }

    lua_pushnumber(L, entry->m_id);
    lua_pushstring(L, entry->m_name.c_str());
    lua_pushstring(L, entry->m_description.c_str());
    lua_pushnumber(L, entry->m_quality);
    lua_pushnumber(L, entry->m_itemClass);
    lua_pushnumber(L, entry->m_subClass);
    lua_pushnumber(L, entry->m_inventoryType);
    lua_pushnumber(L, entry->m_statGroup);
    lua_pushnumber(L, entry->m_flags);
    lua_pushnumber(L, entry->m_itemLevelMin);
    lua_pushnumber(L, entry->m_itemLevelMax);
    lua_pushboolean(L, entry->m_spawnable);
    return 12;
}

class ItemGenModule : public Module {
    public:
    const char* Name() const override {
        return "ItemGen";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_ITEMGEN_STAT_GROUPS, Packet_SMSG_ITEMGEN_STAT_GROUPS);
        Net::RegisterWorldHandler(Net::SMSG_ITEMGEN_LEGENDARIES, Packet_SMSG_ITEMGEN_LEGENDARIES);
        Net::RegisterWorldHandler(Net::SMSG_ITEMGEN_UNIQUES, Packet_SMSG_ITEMGEN_UNIQUES);

        Lua::RegisterFunction("RequestStatGroups", Script_RequestStatGroups);
        Lua::RegisterFunction("GetStatGroupCount", Script_GetStatGroupCount);
        Lua::RegisterFunction("GetStatGroupInfo", Script_GetStatGroupInfo);
        Lua::RegisterFunction("GetStatGroupMaskCount", Script_GetStatGroupMaskCount);
        Lua::RegisterFunction("GetStatGroupMaskInfo", Script_GetStatGroupMaskInfo);
        Lua::RegisterFunction("RequestLegendaries", Script_RequestLegendaries);
        Lua::RegisterFunction("GetLegendaryCount", Script_GetLegendaryCount);
        Lua::RegisterFunction("GetLegendaryInfo", Script_GetLegendaryInfo);
        Lua::RegisterFunction("RequestUniqueItems", Script_RequestUniqueItems);
        Lua::RegisterFunction("GetUniqueItemCount", Script_GetUniqueItemCount);
        Lua::RegisterFunction("GetUniqueItemInfo", Script_GetUniqueItemInfo);
    }
};

Module* GetModule() {
    static ItemGenModule module;
    return &module;
}

}
