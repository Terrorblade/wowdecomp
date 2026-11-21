#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::Talent {

struct LoadoutInfo {
    uint32_t m_id = 0;
    std::string m_name;
    uint32_t m_nodeCount = 0;
};

struct NodeInfo {
    uint32_t m_index = 0;
    uint32_t m_spellId = 0;
    double m_xOffset = 0.0;
    double m_yOffset = 0.0;
    uint32_t m_mutex = 0;
    uint32_t m_buttonType = 0;
    uint32_t m_flagMask = 0;
    std::vector<uint32_t> m_childLinks;
};

static std::unordered_map<uint32_t, NodeInfo> s_nodes;
static uint32_t s_version;
static uint32_t s_freePoints;
static uint32_t s_talentLevel;
static uint32_t s_xpTalentLevel;
static uint32_t s_maxTalentLevel;
static std::vector<uint32_t> s_learntTalents;
static std::vector<uint32_t> s_itemGrantedTalents;
static std::vector<uint32_t> s_inspectLearntTalents;
static uint32_t s_inspectFreePoints;
static uint32_t s_inspectTalentLevel;
static std::vector<LoadoutInfo> s_loadouts;
static uint32_t s_activeLoadout;
static uint32_t s_maxLoadouts;

static void ReadLearntTalents(Net::Packet& packet, std::vector<uint32_t>& out) {
    uint32_t count = packet.GetUInt32();
    out.clear();

    for (uint32_t i = 0; i < count && packet.CanRead(4); i++) {
        out.push_back(packet.GetUInt32());
    }
}

static void SendU32(uint32_t opcode, uint32_t value) {
    Net::Packet(opcode).PutUInt32(value).Send();
}

static int32_t PushTalentList(lua_State* L, const std::vector<uint32_t>& talents) {
    lua_createtable(L, static_cast<int>(talents.size()), 0);
    int tbl = lua_gettop(L);

    for (int i = 0; i < static_cast<int>(talents.size()); i++) {
        lua_pushnumber(L, talents[i]);
        lua_rawseti(L, tbl, i + 1);
    }

    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_CACHE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t count = packet.GetUInt32();
    s_nodes.clear();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        NodeInfo info;
        info.m_index = packet.GetUInt32();
        info.m_spellId = packet.GetUInt32();
        info.m_xOffset = packet.GetDouble();
        info.m_yOffset = packet.GetDouble();

        uint32_t linkCount = packet.GetUInt32();

        for (uint32_t j = 0; j < linkCount && packet.CanRead(4); j++) {
            info.m_childLinks.push_back(packet.GetUInt32());
        }

        info.m_mutex = packet.GetUInt32();
        info.m_buttonType = packet.GetUInt32();
        info.m_flagMask = packet.GetUInt32();

        uint32_t index = info.m_index;
        s_nodes[index] = std::move(info);
    }

    Lua::SignalEvent("HOT_TALENT_CACHE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_SMALL_CACHE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_version = packet.GetUInt32();
    s_freePoints = packet.GetUInt32();
    s_talentLevel = packet.GetUInt32();
    ReadLearntTalents(packet, s_learntTalents);
    ReadLearntTalents(packet, s_itemGrantedTalents);
    s_xpTalentLevel = packet.GetUInt32();
    s_maxTalentLevel = packet.GetUInt32();
    Lua::SignalEvent("HOT_TALENT_SMALL_CACHE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_ITEM_GRANTED_UPDATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    ReadLearntTalents(packet, s_itemGrantedTalents);
    Lua::SignalEvent("HOT_TALENT_ITEM_GRANTED_UPDATE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_POINTS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t freePoints = packet.GetUInt32();
    s_freePoints = freePoints;
    Lua::SignalEvent("HOT_TALENT_POINTS", "%u", freePoints);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_LEVEL(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t talentLevel = packet.GetUInt32();
    s_talentLevel = talentLevel;
    s_xpTalentLevel = packet.GetUInt32();
    s_maxTalentLevel = packet.GetUInt32();
    Lua::SignalEvent("HOT_TALENT_LEVEL", "%u", talentLevel);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_LEARN_RESPONSE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t nodeId = packet.GetUInt32();
    uint8_t canLearn = packet.GetUInt8();

    if (canLearn == 0) {
        if (std::find(s_learntTalents.begin(), s_learntTalents.end(), nodeId) == s_learntTalents.end()) {
            s_learntTalents.push_back(nodeId);
        }

        if (s_freePoints > 0) {
            s_freePoints--;
        }
    }

    Lua::SignalEvent("HOT_TALENT_LEARN_RESPONSE", "%u%u", nodeId, static_cast<uint32_t>(canLearn));
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_UNLEARN_RESPONSE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t nodeId = packet.GetUInt32();
    uint8_t result = packet.GetUInt8();

    if (result != 0) {
        auto it = std::find(s_learntTalents.begin(), s_learntTalents.end(), nodeId);

        if (it != s_learntTalents.end()) {
            s_learntTalents.erase(it);
            s_freePoints++;
        }
    }

    Lua::SignalEvent("HOT_TALENT_UNLEARN_RESPONSE", "%u%u", nodeId, static_cast<uint32_t>(result));
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_RESET(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t freePoints = packet.GetUInt32();
    s_freePoints = freePoints;
    s_learntTalents.clear();
    Lua::SignalEvent("HOT_TALENT_RESET", "%u", freePoints);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_INSPECT_RESPONSE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_inspectFreePoints = packet.GetUInt32();
    s_inspectTalentLevel = packet.GetUInt32();
    ReadLearntTalents(packet, s_inspectLearntTalents);
    Lua::SignalEvent("HOT_TALENT_INSPECT_RESPONSE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_NEW(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t nodeId = packet.GetUInt32();
    Lua::SignalEvent("HOT_TALENT_NEW", "%u", nodeId);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_LEARNT_UPDATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t freePoints = packet.GetUInt32();
    ReadLearntTalents(packet, s_learntTalents);
    s_freePoints = freePoints;
    Lua::SignalEvent("HOT_TALENT_LEARNT_UPDATE", "%u", freePoints);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_LOADOUTS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_activeLoadout = packet.GetUInt32();
    s_maxLoadouts = packet.GetUInt32();

    uint32_t count = packet.GetUInt32();

    if (count > 256) {
        count = 256;
    }

    s_loadouts.clear();
    s_loadouts.reserve(count);

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        LoadoutInfo info;
        info.m_id = packet.GetUInt32();
        char name[128] = {};
        packet.GetString(name, sizeof(name));
        info.m_name = name;
        info.m_nodeCount = packet.GetUInt32();
        s_loadouts.push_back(std::move(info));
    }

    Lua::SignalEvent("HOT_TALENT_LOADOUTS", "%u", s_activeLoadout);
    return 1;
}

static int32_t Packet_SMSG_CUSTOM_TALENT_LOADOUT_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t op = packet.GetUInt8();
    uint8_t result = packet.GetUInt8();
    uint32_t loadoutId = packet.GetUInt32();
    Lua::SignalEvent("HOT_TALENT_LOADOUT_RESULT", "%u%u%u", static_cast<uint32_t>(op), static_cast<uint32_t>(result), loadoutId);
    return 1;
}

static int32_t Script_GetCustomTalentStorage(lua_State* L) {
    lua_createtable(L, static_cast<int>(s_nodes.size()), 0);
    int mainTable = lua_gettop(L);

    for (const auto& it : s_nodes) {
        const NodeInfo& node = it.second;

        lua_createtable(L, 7, 0);
        int subTable = lua_gettop(L);

        lua_pushnumber(L, node.m_spellId);
        lua_rawseti(L, subTable, 1);

        lua_pushnumber(L, node.m_xOffset);
        lua_rawseti(L, subTable, 2);

        lua_pushnumber(L, node.m_yOffset);
        lua_rawseti(L, subTable, 3);

        lua_createtable(L, static_cast<int>(node.m_childLinks.size()), 0);
        int linkTable = lua_gettop(L);

        for (int i = 0; i < static_cast<int>(node.m_childLinks.size()); i++) {
            lua_pushnumber(L, node.m_childLinks[i]);
            lua_rawseti(L, linkTable, i + 1);
        }

        lua_rawseti(L, subTable, 4);

        lua_pushnumber(L, node.m_mutex);
        lua_rawseti(L, subTable, 5);

        lua_pushnumber(L, node.m_buttonType);
        lua_rawseti(L, subTable, 6);

        lua_pushnumber(L, node.m_flagMask);
        lua_rawseti(L, subTable, 7);

        lua_rawseti(L, mainTable, static_cast<int>(node.m_index));
    }

    lua_settop(L, mainTable);
    return 1;
}

static int32_t Script_GetTalentTreeVersion(lua_State* L) {
    lua_pushnumber(L, s_version);
    return 1;
}

static int32_t Script_GetCachedTalentFreePoints(lua_State* L) {
    lua_pushnumber(L, s_freePoints);
    return 1;
}

static int32_t Script_GetCachedTalentLevel(lua_State* L) {
    lua_pushnumber(L, s_talentLevel);
    return 1;
}

static int32_t Script_GetCachedXPTalentLevel(lua_State* L) {
    lua_pushnumber(L, s_xpTalentLevel);
    return 1;
}

static int32_t Script_GetCachedMaxTalentLevel(lua_State* L) {
    lua_pushnumber(L, s_maxTalentLevel);
    return 1;
}

static int32_t Script_GetCachedLearntTalents(lua_State* L) {
    return PushTalentList(L, s_learntTalents);
}

static int32_t Script_GetCachedItemGrantedTalents(lua_State* L) {
    return PushTalentList(L, s_itemGrantedTalents);
}

static int32_t Script_GetCachedInspectLearntTalents(lua_State* L) {
    return PushTalentList(L, s_inspectLearntTalents);
}

static int32_t Script_GetCachedInspectFreePoints(lua_State* L) {
    lua_pushnumber(L, s_inspectFreePoints);
    return 1;
}

static int32_t Script_GetCachedInspectTalentLevel(lua_State* L) {
    lua_pushnumber(L, s_inspectTalentLevel);
    return 1;
}

static int32_t Script_GetCachedTalentLoadouts(lua_State* L) {
    lua_createtable(L, static_cast<int>(s_loadouts.size()), 0);
    int tbl = lua_gettop(L);

    for (int i = 0; i < static_cast<int>(s_loadouts.size()); i++) {
        const LoadoutInfo& info = s_loadouts[i];

        lua_createtable(L, 0, 3);
        int entry = lua_gettop(L);

        lua_pushnumber(L, info.m_id);
        lua_setfield(L, entry, "id");

        lua_pushstring(L, info.m_name.c_str());
        lua_setfield(L, entry, "name");

        lua_pushnumber(L, info.m_nodeCount);
        lua_setfield(L, entry, "count");

        lua_rawseti(L, tbl, i + 1);
    }

    lua_settop(L, tbl);
    return 1;
}

static int32_t Script_GetActiveTalentLoadout(lua_State* L) {
    lua_pushnumber(L, s_activeLoadout);
    return 1;
}

static int32_t Script_GetMaxTalentLoadouts(lua_State* L) {
    lua_pushnumber(L, s_maxLoadouts);
    return 1;
}

static int32_t Script_RequestTalentCache(lua_State* L) {
    Net::Packet(Net::CMSG_CUSTOM_TALENT_CACHE_REQUEST).Send();
    return 0;
}

static int32_t Script_RequestTalentSmallCache(lua_State* L) {
    Net::Packet(Net::CMSG_CUSTOM_TALENT_SMALL_CACHE_REQ).Send();
    return 0;
}

static int32_t Script_RequestTalentPoints(lua_State* L) {
    Net::Packet(Net::CMSG_CUSTOM_TALENT_POINTS_REQUEST).Send();
    return 0;
}

static int32_t Script_LearnTalent(lua_State* L) {
    SendU32(Net::CMSG_CUSTOM_TALENT_LEARN, static_cast<uint32_t>(lua_tonumber(L, 1)));
    return 0;
}

static int32_t Script_UnlearnTalent(lua_State* L) {
    SendU32(Net::CMSG_CUSTOM_TALENT_UNLEARN, static_cast<uint32_t>(lua_tonumber(L, 1)));
    return 0;
}

static int32_t Script_ResetTalents(lua_State* L) {
    Net::Packet(Net::CMSG_CUSTOM_TALENT_RESET).Send();
    return 0;
}

static int32_t Script_InspectTalents(lua_State* L) {
    const char* playerName = lua_tolstring(L, 1, nullptr);

    if (!playerName || playerName[0] == '\0') {
        return 0;
    }

    Net::Packet(Net::CMSG_CUSTOM_TALENT_INSPECT).PutString(playerName).Send();
    return 0;
}

static int32_t Script_SwitchTalentLoadout(lua_State* L) {
    SendU32(Net::CMSG_CUSTOM_TALENT_LOADOUT_SWITCH, static_cast<uint32_t>(lua_tonumber(L, 1)));
    return 0;
}

static int32_t Script_CreateTalentLoadout(lua_State* L) {
    const char* name = lua_tolstring(L, 1, nullptr);

    if (!name || name[0] == '\0') {
        return 0;
    }

    Net::Packet(Net::CMSG_CUSTOM_TALENT_LOADOUT_CREATE).PutString(name).Send();
    return 0;
}

static int32_t Script_DeleteTalentLoadout(lua_State* L) {
    SendU32(Net::CMSG_CUSTOM_TALENT_LOADOUT_DELETE, static_cast<uint32_t>(lua_tonumber(L, 1)));
    return 0;
}

static int32_t Script_RenameTalentLoadout(lua_State* L) {
    uint32_t loadoutId = static_cast<uint32_t>(lua_tonumber(L, 1));
    const char* name = lua_tolstring(L, 2, nullptr);

    if (!name || name[0] == '\0') {
        return 0;
    }

    Net::Packet(Net::CMSG_CUSTOM_TALENT_LOADOUT_RENAME).PutUInt32(loadoutId).PutString(name).Send();
    return 0;
}

static int32_t Script_RequestTalentLoadouts(lua_State* L) {
    Net::Packet(Net::CMSG_CUSTOM_TALENT_LOADOUTS_REQUEST).Send();
    return 0;
}

static int32_t Script_ImportTalentBuild(lua_State* L) {
    // Raw reads on a string argument would read past the string, so only tables are accepted.
    if (lua_type(L, 1) != LUA_TTABLE) {
        return 0;
    }

    int count = static_cast<int>(lua_objlen(L, 1));

    if (count <= 0) {
        return 0;
    }

    if (count > 1000) {
        count = 1000;
    }

    Net::Packet packet(Net::CMSG_CUSTOM_TALENT_IMPORT);
    packet.PutUInt32(static_cast<uint32_t>(count));

    int top = lua_gettop(L);

    for (int i = 1; i <= count; i++) {
        lua_rawgeti(L, 1, i);
        uint32_t nodeId = static_cast<uint32_t>(lua_tonumber(L, top + 1));
        lua_settop(L, top);
        packet.PutUInt32(nodeId);
    }

    packet.Send();
    return 0;
}

static int32_t Script_TalentEditorSetLink(lua_State* L) {
    uint32_t nodeId = static_cast<uint32_t>(lua_tonumber(L, 1));
    uint32_t linkTo = static_cast<uint32_t>(lua_tonumber(L, 2));

    Net::Packet(Net::CMSG_TALENT_EDITOR_SET_LINK).PutUInt32(nodeId).PutUInt32(linkTo).Send();
    return 0;
}

static int32_t Script_TalentEditorDeleteLinks(lua_State* L) {
    SendU32(Net::CMSG_TALENT_EDITOR_DELETE_LINKS, static_cast<uint32_t>(lua_tonumber(L, 1)));
    return 0;
}

static int32_t Script_TalentEditorUpdateNode(lua_State* L) {
    uint32_t nodeId = static_cast<uint32_t>(lua_tonumber(L, 1));
    uint32_t spellId = static_cast<uint32_t>(lua_tonumber(L, 2));
    double x = lua_tonumber(L, 3);
    double y = lua_tonumber(L, 4);
    uint32_t buttonType = static_cast<uint32_t>(lua_tonumber(L, 5));
    uint32_t flagMask = static_cast<uint32_t>(lua_tonumber(L, 6));

    Net::Packet(Net::CMSG_TALENT_EDITOR_UPDATE_NODE)
        .PutUInt32(nodeId)
        .PutUInt32(spellId)
        .PutDouble(x)
        .PutDouble(y)
        .PutUInt32(buttonType)
        .PutUInt32(flagMask)
        .Send();
    return 0;
}

static int32_t Script_TalentEditorDeleteNode(lua_State* L) {
    SendU32(Net::CMSG_TALENT_EDITOR_DELETE_NODE, static_cast<uint32_t>(lua_tonumber(L, 1)));
    return 0;
}

static int32_t Script_TalentEditorNewNode(lua_State* L) {
    Net::Packet(Net::CMSG_TALENT_EDITOR_NEW_NODE).Send();
    return 0;
}

class TalentModule : public Module {
    public:
    const char* Name() const override {
        return "Talent";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_CACHE, Packet_SMSG_CUSTOM_TALENT_CACHE);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_SMALL_CACHE, Packet_SMSG_CUSTOM_TALENT_SMALL_CACHE);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_POINTS, Packet_SMSG_CUSTOM_TALENT_POINTS);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_LEVEL, Packet_SMSG_CUSTOM_TALENT_LEVEL);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_LEARN_RESPONSE, Packet_SMSG_CUSTOM_TALENT_LEARN_RESPONSE);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_UNLEARN_RESPONSE, Packet_SMSG_CUSTOM_TALENT_UNLEARN_RESPONSE);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_RESET, Packet_SMSG_CUSTOM_TALENT_RESET);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_INSPECT_RESPONSE, Packet_SMSG_CUSTOM_TALENT_INSPECT_RESPONSE);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_NEW, Packet_SMSG_CUSTOM_TALENT_NEW);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_LEARNT_UPDATE, Packet_SMSG_CUSTOM_TALENT_LEARNT_UPDATE);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_ITEM_GRANTED_UPDATE, Packet_SMSG_CUSTOM_TALENT_ITEM_GRANTED_UPDATE);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_LOADOUTS, Packet_SMSG_CUSTOM_TALENT_LOADOUTS);
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TALENT_LOADOUT_RESULT, Packet_SMSG_CUSTOM_TALENT_LOADOUT_RESULT);

        Lua::RegisterFunction("GetCustomTalentStorage", Script_GetCustomTalentStorage);
        Lua::RegisterFunction("GetTalentTreeVersion", Script_GetTalentTreeVersion);
        Lua::RegisterFunction("GetCachedTalentFreePoints", Script_GetCachedTalentFreePoints);
        Lua::RegisterFunction("GetCachedTalentLevel", Script_GetCachedTalentLevel);
        Lua::RegisterFunction("GetCachedXPTalentLevel", Script_GetCachedXPTalentLevel);
        Lua::RegisterFunction("GetCachedMaxTalentLevel", Script_GetCachedMaxTalentLevel);
        Lua::RegisterFunction("GetCachedLearntTalents", Script_GetCachedLearntTalents);
        Lua::RegisterFunction("GetCachedItemGrantedTalents", Script_GetCachedItemGrantedTalents);
        Lua::RegisterFunction("GetCachedInspectLearntTalents", Script_GetCachedInspectLearntTalents);
        Lua::RegisterFunction("GetCachedInspectFreePoints", Script_GetCachedInspectFreePoints);
        Lua::RegisterFunction("GetCachedInspectTalentLevel", Script_GetCachedInspectTalentLevel);
        Lua::RegisterFunction("GetCachedTalentLoadouts", Script_GetCachedTalentLoadouts);
        Lua::RegisterFunction("GetActiveTalentLoadout", Script_GetActiveTalentLoadout);
        Lua::RegisterFunction("GetMaxTalentLoadouts", Script_GetMaxTalentLoadouts);

        Lua::RegisterFunction("CustomRequestTalentCache", Script_RequestTalentCache);
        Lua::RegisterFunction("CustomRequestTalentSmallCache", Script_RequestTalentSmallCache);
        Lua::RegisterFunction("CustomRequestTalentPoints", Script_RequestTalentPoints);
        Lua::RegisterFunction("CustomLearnTalent", Script_LearnTalent);
        Lua::RegisterFunction("CustomUnlearnTalent", Script_UnlearnTalent);
        Lua::RegisterFunction("CustomResetTalents", Script_ResetTalents);
        Lua::RegisterFunction("CustomInspectTalents", Script_InspectTalents);
        Lua::RegisterFunction("CustomSwitchTalentLoadout", Script_SwitchTalentLoadout);
        Lua::RegisterFunction("CustomCreateTalentLoadout", Script_CreateTalentLoadout);
        Lua::RegisterFunction("CustomDeleteTalentLoadout", Script_DeleteTalentLoadout);
        Lua::RegisterFunction("CustomRenameTalentLoadout", Script_RenameTalentLoadout);
        Lua::RegisterFunction("CustomRequestTalentLoadouts", Script_RequestTalentLoadouts);
        Lua::RegisterFunction("CustomImportTalentBuild", Script_ImportTalentBuild);

        Lua::RegisterFunction("TalentEditorSetLink", Script_TalentEditorSetLink);
        Lua::RegisterFunction("TalentEditorDeleteLinks", Script_TalentEditorDeleteLinks);
        Lua::RegisterFunction("TalentEditorUpdateNode", Script_TalentEditorUpdateNode);
        Lua::RegisterFunction("TalentEditorDeleteNode", Script_TalentEditorDeleteNode);
        Lua::RegisterFunction("TalentEditorNewNode", Script_TalentEditorNewNode);
    }
};

Module* GetModule() {
    static TalentModule module;
    return &module;
}

}
