#include "HoT/Editor/TemplateSearch.h"
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::Editor::TemplateSearch {

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr uint8_t kKinds = 6;
static const char* const kKindNames[kKinds] = { "creature", "gameobject", "quest", "item", "loot", "lootuse" };

struct Hit {
    uint32_t m_entry = 0;
    std::string m_name;
    std::string m_subName;
    uint32_t m_type = 0;
    uint32_t m_displayId = 0;
    uint8_t m_minLevel = 0;
    uint8_t m_maxLevel = 0;
};

struct Results {
    uint32_t m_requestId = 0;
    std::string m_query;
    bool m_received = false;
    bool m_truncated = false;
    std::vector<Hit> m_hits;
};

static constexpr uint8_t kFlagTruncated = 1;
static constexpr uint8_t kFlagMore = 2;
static constexpr size_t kMaxPending = 8;

static Results s_results[kKinds];
static std::unordered_map<uint32_t, Results> s_pending[kKinds];
static uint32_t s_nextRequestId = 1;

static const char* KindName(uint8_t kind) {
    return kind < kKinds ? kKindNames[kind] : "";
}

static int KindFromLua(lua_State* L, int index) {
    const char* name = lua_tostring(L, index);

    if (!name) {
        return -1;
    }

    for (uint8_t kind = 0; kind < kKinds; kind++) {
        if (!std::strcmp(name, kKindNames[kind])) {
            return kind;
        }
    }

    return -1;
}

static int32_t Packet_SMSG_EDITOR_TEMPLATE_RESULTS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    char buffer[512];
    Net::Packet packet(msg);
    uint8_t kind = packet.GetUInt8();
    uint32_t requestId = packet.GetUInt32();

    if (kind >= kKinds) {
        return 1;
    }

    auto pending = s_pending[kind].find(requestId);

    if (pending == s_pending[kind].end()) {
        return 1;
    }

    Results& results = pending->second;
    uint8_t flags = packet.GetUInt8();
    results.m_truncated = results.m_truncated || (flags & kFlagTruncated) != 0;

    uint16_t count = packet.GetUInt16();
    results.m_hits.reserve(results.m_hits.size() + count);

    for (uint16_t i = 0; i < count && packet.CanRead(4); i++) {
        Hit hit;
        hit.m_entry = packet.GetUInt32();
        packet.GetString(buffer, sizeof(buffer));
        hit.m_name = buffer;
        packet.GetString(buffer, sizeof(buffer));
        hit.m_subName = buffer;
        hit.m_type = packet.GetUInt32();
        hit.m_displayId = packet.GetUInt32();
        hit.m_minLevel = packet.GetUInt8();
        hit.m_maxLevel = packet.GetUInt8();
        results.m_hits.push_back(std::move(hit));
    }

    if (flags & kFlagMore) {
        return 1;
    }

    results.m_received = true;
    s_results[kind] = std::move(results);
    s_pending[kind].erase(pending);

    Lua::SignalEvent("EDITOR_TEMPLATE_RESULTS", "%s%d%d%b", KindName(kind), static_cast<int32_t>(requestId),
        static_cast<int32_t>(s_results[kind].m_hits.size()), s_results[kind].m_truncated ? 1 : 0);
    return 1;
}

// TemplateSearch_Query("creature" | "gameobject" | "quest" | "item" | "loot" | "lootuse", query [, limit]) returns the request id, an entry number matches exactly, an empty query with limit 65535 lists everything.
static int32_t Script_Query(lua_State* L) {
    int kind = KindFromLua(L, 1);
    const char* query = lua_tostring(L, 2);

    if (kind < 0 || !query) {
        lua_pushnil(L);
        return 1;
    }

    uint16_t limit = lua_isnumber(L, 3) ? static_cast<uint16_t>(lua_tonumber(L, 3)) : 200;
    uint32_t requestId = s_nextRequestId++;

    // Requests the server never answers would pile up, the oldest goes first.
    if (s_pending[kind].size() >= kMaxPending) {
        auto oldest = s_pending[kind].begin();

        for (auto it = s_pending[kind].begin(); it != s_pending[kind].end(); ++it) {
            if (it->first < oldest->first) {
                oldest = it;
            }
        }

        s_pending[kind].erase(oldest);
    }

    Results& pending = s_pending[kind][requestId];
    pending.m_requestId = requestId;
    pending.m_query = query;

    Net::Packet(Net::CMSG_EDITOR_TEMPLATE_SEARCH)
        .PutUInt8(static_cast<uint8_t>(kind))
        .PutUInt32(requestId)
        .PutString(query)
        .PutUInt16(limit)
        .Send();

    lua_pushnumber(L, requestId);
    return 1;
}

static void SetNumber(lua_State* L, int tableIdx, const char* key, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, tableIdx, key);
}

static void SetString(lua_State* L, int tableIdx, const char* key, const std::string& value) {
    lua_pushstring(L, value.c_str());
    lua_setfield(L, tableIdx, key);
}

// TemplateSearch_GetResults(kind) returns { requestId, query, truncated, rows = { { entry, name, subName, type, displayId, minLevel, maxLevel } } }.
static int32_t Script_GetResults(lua_State* L) {
    int kind = KindFromLua(L, 1);

    if (kind < 0 || !s_results[kind].m_received) {
        lua_pushnil(L);
        return 1;
    }

    const Results& results = s_results[kind];

    lua_createtable(L, 0, 4);
    int tableIdx = lua_gettop(L);
    SetNumber(L, tableIdx, "requestId", results.m_requestId);
    SetString(L, tableIdx, "query", results.m_query);
    lua_pushboolean(L, results.m_truncated ? 1 : 0);
    lua_setfield(L, tableIdx, "truncated");

    lua_createtable(L, static_cast<int>(results.m_hits.size()), 0);
    int rowsIdx = lua_gettop(L);

    for (size_t i = 0; i < results.m_hits.size(); i++) {
        const Hit& hit = results.m_hits[i];

        lua_createtable(L, 0, 7);
        int rowIdx = lua_gettop(L);
        SetNumber(L, rowIdx, "entry", hit.m_entry);
        SetString(L, rowIdx, "name", hit.m_name);
        SetString(L, rowIdx, "subName", hit.m_subName);
        SetNumber(L, rowIdx, "type", hit.m_type);
        SetNumber(L, rowIdx, "displayId", hit.m_displayId);
        SetNumber(L, rowIdx, "minLevel", hit.m_minLevel);
        SetNumber(L, rowIdx, "maxLevel", hit.m_maxLevel);
        lua_rawseti(L, rowsIdx, static_cast<int>(i + 1));
    }

    lua_setfield(L, tableIdx, "rows");
    return 1;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!Session::HasClearance(kRequiredSecurityLevel)) {
        return 0;
    }

    return Function(L);
}

void Apply() {
    Net::RegisterWorldHandler(Net::SMSG_EDITOR_TEMPLATE_RESULTS, Packet_SMSG_EDITOR_TEMPLATE_RESULTS);

    Lua::RegisterFunction("TemplateSearch_Query", Gated<Script_Query>);
    Lua::RegisterFunction("TemplateSearch_GetResults", Gated<Script_GetResults>);
}

void OnGameDestroy() {
    for (uint8_t kind = 0; kind < kKinds; kind++) {
        s_results[kind] = Results();
        s_pending[kind].clear();
    }
}

}
