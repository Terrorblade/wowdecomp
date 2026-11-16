#include "HoT/Editor/Spell/WorldTables.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::Editor::WorldTables {

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr uint8_t kScriptKinds = 3;

enum ValueType : uint8_t {
    VALUE_UINT32 = 0,
    VALUE_INT32 = 1,
    VALUE_FLOAT = 2,
    VALUE_STRING = 3,
    VALUE_UINT64 = 4,
    VALUE_INT64 = 5,
};

struct Value {
    uint64_t u = 0;
    int64_t i = 0;
    double f = 0.0;
    std::string text;
};

struct ColumnInfo {
    std::string name;
    uint8_t type = VALUE_UINT32;
};

struct RowSet {
    std::string table;
    int64_t key = 0;
    std::string keyColumn;
    std::vector<ColumnInfo> columns;
    std::vector<std::vector<Value>> rows;
};

static std::map<std::pair<std::string, int64_t>, RowSet> s_rows;
static std::unordered_map<std::string, std::vector<ColumnInfo>> s_columns;
static std::vector<std::string> s_scriptNames[kScriptKinds];
static bool s_scriptReset[kScriptKinds] = { true, true, true };

static std::string Lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    return text;
}

static const char* TypeName(uint8_t type) {
    switch (type) {
        case VALUE_UINT32:
            return "uint";
        case VALUE_INT32:
            return "int";
        case VALUE_FLOAT:
            return "float";
        case VALUE_UINT64:
            return "uint64";
        case VALUE_INT64:
            return "int64";
        default:
            return "string";
    }
}

static uint8_t TypeFromName(const char* name, uint8_t fallback) {
    if (!name) {
        return fallback;
    }

    for (uint8_t type = VALUE_UINT32; type <= VALUE_INT64; type++) {
        if (_stricmp(name, TypeName(type)) == 0) {
            return type;
        }
    }

    return fallback;
}

static std::string ReadCString(Net::Packet& packet) {
    std::string text;

    while (packet.CanRead(1)) {
        char c = static_cast<char>(packet.GetUInt8());

        if (!c) {
            break;
        }

        text.push_back(c);
    }

    return text;
}

static bool ReadValue(Net::Packet& packet, uint8_t type, Value& value) {
    switch (type) {
        case VALUE_UINT32:
            if (!packet.CanRead(4)) {
                return false;
            }

            value.u = packet.GetUInt32();
            return true;
        case VALUE_INT32:
            if (!packet.CanRead(4)) {
                return false;
            }

            value.i = packet.GetInt32();
            return true;
        case VALUE_FLOAT:
            if (!packet.CanRead(4)) {
                return false;
            }

            value.f = packet.GetFloat();
            return true;
        case VALUE_STRING:
            if (!packet.CanRead(1)) {
                return false;
            }

            value.text = ReadCString(packet);
            return true;
        case VALUE_UINT64:
            if (!packet.CanRead(8)) {
                return false;
            }

            value.u = packet.GetUInt64();
            return true;
        case VALUE_INT64:
            if (!packet.CanRead(8)) {
                return false;
            }

            value.i = packet.GetInt64();
            return true;
        default:
            return false;
    }
}

static void PushValue(lua_State* L, uint8_t type, const Value& value) {
    switch (type) {
        case VALUE_UINT32:
        case VALUE_UINT64:
            lua_pushnumber(L, static_cast<double>(value.u));
            break;
        case VALUE_INT32:
        case VALUE_INT64:
            lua_pushnumber(L, static_cast<double>(value.i));
            break;
        case VALUE_FLOAT:
            lua_pushnumber(L, value.f);
            break;
        default:
            lua_pushstring(L, value.text.c_str());
            break;
    }
}

static bool KeyArgument(lua_State* L, int index, int64_t& key) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        key = static_cast<int64_t>(std::llround(lua_tonumber(L, index)));
        return true;
    }

    if (lua_type(L, index) == LUA_TSTRING) {
        const char* text = lua_tostring(L, index);
        char* end = nullptr;
        key = std::strtoll(text, &end, 0);
        return end != text;
    }

    return false;
}

static void SignalRows(const std::string& table, int64_t key, bool ok, const std::string& error) {
    Lua::SignalEvent("WORLD_TABLE_ROWS", "%s%f%b%s", table.c_str(), static_cast<double>(key), ok ? 1 : 0, error.c_str());
}

static int32_t Packet_SMSG_EDITOR_WORLD_ROWS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    bool ok = packet.GetUInt8() != 0;
    RowSet set;
    set.table = ReadCString(packet);
    set.key = packet.GetInt64();

    auto id = std::make_pair(Lower(set.table), set.key);

    if (!ok) {
        std::string error = ReadCString(packet);
        s_rows.erase(id);
        SignalRows(set.table, set.key, false, error);
        return 1;
    }

    set.keyColumn = ReadCString(packet);
    uint16_t columnCount = packet.GetUInt16();
    set.columns.resize(columnCount);

    for (ColumnInfo& column : set.columns) {
        column.name = ReadCString(packet);
        column.type = packet.GetUInt8();
    }

    uint16_t rowCount = packet.GetUInt16();
    set.rows.reserve(rowCount < packet.Remaining() ? rowCount : packet.Remaining());

    for (uint16_t r = 0; r < rowCount; r++) {
        std::vector<Value> row(columnCount);

        for (uint16_t c = 0; c < columnCount; c++) {
            if (!ReadValue(packet, set.columns[c].type, row[c])) {
                s_rows.erase(id);
                SignalRows(set.table, set.key, false, "Malformed row data");
                return 1;
            }
        }

        set.rows.push_back(std::move(row));
    }

    s_columns[id.first] = set.columns;
    std::string table = set.table;
    int64_t key = set.key;
    s_rows[id] = std::move(set);

    SignalRows(table, key, true, "");
    return 1;
}

static int32_t Packet_SMSG_EDITOR_WORLD_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    int32_t ok = packet.GetUInt8() != 0;
    std::string table = ReadCString(packet);
    int64_t key = packet.GetInt64();
    std::string message = ReadCString(packet);

    Lua::SignalEvent("WORLD_TABLE_RESULT", "%b%s%f%s", ok, table.c_str(), static_cast<double>(key), message.c_str());
    return 1;
}

static int32_t Packet_SMSG_EDITOR_SCRIPT_NAMES(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t kind = packet.GetUInt8();
    bool final = packet.GetUInt8() != 0;
    uint16_t count = packet.GetUInt16();

    if (kind >= kScriptKinds) {
        return 1;
    }

    if (s_scriptReset[kind]) {
        s_scriptNames[kind].clear();
        s_scriptReset[kind] = false;
    }

    for (uint16_t i = 0; i < count && packet.CanRead(1); i++) {
        s_scriptNames[kind].push_back(ReadCString(packet));
    }

    if (final) {
        s_scriptReset[kind] = true;
        Lua::SignalEvent("WORLD_SCRIPT_NAMES", "%d", static_cast<int32_t>(kind));
    }

    return 1;
}

// WorldTable_Query(table, key) asks the server for every row of the table where its key column equals key.
static int32_t Script_Query(lua_State* L) {
    int64_t key;

    if (lua_type(L, 1) != LUA_TSTRING || !KeyArgument(L, 2, key)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Net::Packet(Net::CMSG_EDITOR_WORLD_QUERY).PutString(lua_tostring(L, 1)).PutInt64(key).Send();
    lua_pushboolean(L, 1);
    return 1;
}

// WorldTable_GetRows(table, key) -> { table, key, keyColumn, columns = { { name, type }, ... }, rows = { { column = value }, ... } } or nil.
static int32_t Script_GetRows(lua_State* L) {
    int64_t key;

    if (lua_type(L, 1) != LUA_TSTRING || !KeyArgument(L, 2, key)) {
        lua_pushnil(L);
        return 1;
    }

    auto it = s_rows.find({ Lower(lua_tostring(L, 1)), key });

    if (it == s_rows.end()) {
        lua_pushnil(L);
        return 1;
    }

    const RowSet& set = it->second;
    lua_createtable(L, 0, 5);
    int result = lua_gettop(L);

    lua_pushstring(L, set.table.c_str());
    lua_setfield(L, result, "table");
    lua_pushnumber(L, static_cast<double>(set.key));
    lua_setfield(L, result, "key");
    lua_pushstring(L, set.keyColumn.c_str());
    lua_setfield(L, result, "keyColumn");

    lua_createtable(L, static_cast<int>(set.columns.size()), 0);

    for (size_t c = 0; c < set.columns.size(); c++) {
        lua_createtable(L, 2, 3);
        lua_pushstring(L, set.columns[c].name.c_str());
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "name");
        lua_rawseti(L, -2, 1);
        lua_pushstring(L, TypeName(set.columns[c].type));
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "type");
        lua_rawseti(L, -2, 2);
        lua_pushnumber(L, set.columns[c].type);
        lua_setfield(L, -2, "typeId");
        lua_rawseti(L, -2, static_cast<int>(c + 1));
    }

    lua_setfield(L, result, "columns");

    lua_createtable(L, static_cast<int>(set.rows.size()), 0);

    for (size_t r = 0; r < set.rows.size(); r++) {
        lua_createtable(L, 0, static_cast<int>(set.columns.size()));

        for (size_t c = 0; c < set.columns.size(); c++) {
            PushValue(L, set.columns[c].type, set.rows[r][c]);
            lua_setfield(L, -2, set.columns[c].name.c_str());
        }

        lua_rawseti(L, -2, static_cast<int>(r + 1));
    }

    lua_setfield(L, result, "rows");
    return 1;
}

static const ColumnInfo* FindKnown(const std::vector<ColumnInfo>* known, const char* name) {
    if (known) {
        for (const ColumnInfo& column : *known) {
            if (_stricmp(column.name.c_str(), name) == 0) {
                return &column;
            }
        }
    }

    return nullptr;
}

static uint8_t InferType(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        double number = lua_tonumber(L, index);
        return number == std::floor(number) ? VALUE_INT64 : VALUE_FLOAT;
    }

    return lua_type(L, index) == LUA_TBOOLEAN ? VALUE_INT32 : VALUE_STRING;
}

// Columns given as names or { name, type } entries, types fall back to the last query of the table.
static void ReadColumnList(lua_State* L, int index, const std::vector<ColumnInfo>* known, std::vector<ColumnInfo>& columns) {
    int count = static_cast<int>(lua_objlen(L, index));

    for (int i = 1; i <= count; i++) {
        lua_rawgeti(L, index, i);
        ColumnInfo column;
        const char* typeName = nullptr;

        if (lua_type(L, -1) == LUA_TSTRING) {
            column.name = lua_tostring(L, -1);
        } else if (lua_type(L, -1) == LUA_TTABLE) {
            lua_getfield(L, -1, "name");

            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 1);
            }

            column.name = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "";
            lua_pop(L, 1);

            lua_getfield(L, -1, "type");

            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 2);
            }

            if (lua_type(L, -1) == LUA_TNUMBER) {
                column.type = static_cast<uint8_t>(lua_tonumber(L, -1));
                typeName = TypeName(column.type);
            } else if (lua_type(L, -1) == LUA_TSTRING) {
                typeName = lua_tostring(L, -1);
            }

            lua_pop(L, 1);
        }

        lua_pop(L, 1);

        if (column.name.empty()) {
            continue;
        }

        const ColumnInfo* match = FindKnown(known, column.name.c_str());
        column.type = TypeFromName(typeName, match ? match->type : VALUE_STRING);

        if (match) {
            column.name = match->name;
        }

        columns.push_back(std::move(column));
    }
}

// Every column some row sets, in the order of the last query, unknown names last.
static void CollectColumns(lua_State* L, int rowsIndex, const std::vector<ColumnInfo>* known, std::vector<ColumnInfo>& columns) {
    std::vector<ColumnInfo> extra;
    std::vector<bool> used(known ? known->size() : 0, false);
    int count = static_cast<int>(lua_objlen(L, rowsIndex));

    for (int r = 1; r <= count; r++) {
        lua_rawgeti(L, rowsIndex, r);

        if (lua_type(L, -1) == LUA_TTABLE) {
            int row = lua_gettop(L);
            lua_pushnil(L);

            while (lua_next(L, row)) {
                if (lua_type(L, -2) == LUA_TSTRING) {
                    const char* name = lua_tostring(L, -2);
                    const ColumnInfo* match = FindKnown(known, name);

                    if (match) {
                        used[match - known->data()] = true;
                    } else if (!FindKnown(&extra, name)) {
                        extra.push_back({ name, InferType(L, lua_gettop(L)) });
                    }
                }

                lua_pop(L, 1);
            }
        }

        lua_pop(L, 1);
    }

    for (size_t i = 0; i < used.size(); i++) {
        if (used[i]) {
            columns.push_back((*known)[i]);
        }
    }

    columns.insert(columns.end(), extra.begin(), extra.end());
}

static void PutValue(Net::Packet& packet, lua_State* L, int index, uint8_t type) {
    int luaType = lua_type(L, index);
    double number = 0.0;
    const char* text = nullptr;

    if (luaType == LUA_TNUMBER) {
        number = lua_tonumber(L, index);
    } else if (luaType == LUA_TBOOLEAN) {
        number = lua_toboolean(L, index) ? 1.0 : 0.0;
    } else if (luaType == LUA_TSTRING) {
        text = lua_tostring(L, index);
    }

    switch (type) {
        case VALUE_UINT32:
            packet.PutUInt32(text ? static_cast<uint32_t>(std::strtoll(text, nullptr, 0)) : static_cast<uint32_t>(static_cast<int64_t>(number)));
            break;
        case VALUE_INT32:
            packet.PutInt32(text ? static_cast<int32_t>(std::strtoll(text, nullptr, 0)) : static_cast<int32_t>(static_cast<int64_t>(number)));
            break;
        case VALUE_FLOAT:
            packet.PutFloat(static_cast<float>(text ? std::strtod(text, nullptr) : number));
            break;
        case VALUE_UINT64:
            packet.PutUInt64(text ? std::strtoull(text, nullptr, 0) : (number < 0.0 ? static_cast<uint64_t>(static_cast<int64_t>(number)) : static_cast<uint64_t>(number)));
            break;
        case VALUE_INT64:
            packet.PutInt64(text ? std::strtoll(text, nullptr, 0) : static_cast<int64_t>(std::llround(number)));
            break;
        default:
            if (text) {
                packet.PutString(text);
            } else if (luaType == LUA_TNUMBER) {
                lua_pushvalue(L, index);
                packet.PutString(lua_tostring(L, -1));
                lua_pop(L, 1);
            } else if (luaType == LUA_TBOOLEAN) {
                packet.PutString(number != 0.0 ? "1" : "0");
            } else {
                packet.PutString("");
            }

            break;
    }
}

// WorldTable_Save(table, key, rows) or WorldTable_Save(table, key, columns, rows) replaces every row of the key.
static int32_t Script_Save(lua_State* L) {
    int64_t key;

    if (lua_type(L, 1) != LUA_TSTRING || !KeyArgument(L, 2, key) || lua_type(L, 3) != LUA_TTABLE) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char* table = lua_tostring(L, 1);
    auto knownIt = s_columns.find(Lower(table));
    const std::vector<ColumnInfo>* known = knownIt != s_columns.end() ? &knownIt->second : nullptr;

    std::vector<ColumnInfo> columns;
    int rowsIndex = 3;

    if (lua_type(L, 4) == LUA_TTABLE) {
        ReadColumnList(L, 3, known, columns);
        rowsIndex = 4;
    } else {
        CollectColumns(L, 3, known, columns);
    }

    int rowCount = static_cast<int>(lua_objlen(L, rowsIndex));

    if (columns.size() > 0xFFFF || rowCount > 0xFFFF || (rowCount && columns.empty()) || columns.size() * static_cast<size_t>(rowCount) > 0x100000) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Net::Packet packet(Net::CMSG_EDITOR_WORLD_SAVE);
    packet.PutString(table);
    packet.PutInt64(key);
    packet.PutUInt16(static_cast<uint16_t>(columns.size()));

    for (const ColumnInfo& column : columns) {
        packet.PutString(column.name.c_str());
        packet.PutUInt8(column.type);
    }

    packet.PutUInt16(static_cast<uint16_t>(rowCount));

    for (int r = 1; r <= rowCount; r++) {
        lua_rawgeti(L, rowsIndex, r);
        int row = lua_gettop(L);

        for (const ColumnInfo& column : columns) {
            if (lua_type(L, row) == LUA_TTABLE) {
                lua_getfield(L, row, column.name.c_str());
            } else {
                lua_pushnil(L);
            }

            PutValue(packet, L, lua_gettop(L), column.type);
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }

    packet.Send();
    lua_pushboolean(L, 1);
    return 1;
}

// WorldTable_RequestScriptNames(kind), kind 0 spell scripts, 1 creature AI and scripts, 2 gameobject scripts.
static int32_t Script_RequestScriptNames(lua_State* L) {
    int32_t kind = static_cast<int32_t>(lua_tonumber(L, 1));

    if (kind < 0 || kind >= kScriptKinds) {
        lua_pushboolean(L, 0);
        return 1;
    }

    s_scriptReset[kind] = true;
    Net::Packet(Net::CMSG_EDITOR_SCRIPT_NAMES).PutUInt8(static_cast<uint8_t>(kind)).Send();
    lua_pushboolean(L, 1);
    return 1;
}

// WorldTable_GetScriptNames(kind) -> { "name", ... } as last received.
static int32_t Script_GetScriptNames(lua_State* L) {
    int32_t kind = static_cast<int32_t>(lua_tonumber(L, 1));

    if (kind < 0 || kind >= kScriptKinds) {
        lua_pushnil(L);
        return 1;
    }

    const std::vector<std::string>& names = s_scriptNames[kind];
    lua_createtable(L, static_cast<int>(names.size()), 0);

    for (size_t i = 0; i < names.size(); i++) {
        lua_pushstring(L, names[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

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
    Net::RegisterWorldHandler(Net::SMSG_EDITOR_WORLD_ROWS, Packet_SMSG_EDITOR_WORLD_ROWS);
    Net::RegisterWorldHandler(Net::SMSG_EDITOR_WORLD_RESULT, Packet_SMSG_EDITOR_WORLD_RESULT);
    Net::RegisterWorldHandler(Net::SMSG_EDITOR_SCRIPT_NAMES, Packet_SMSG_EDITOR_SCRIPT_NAMES);

    Lua::RegisterFunction("WorldTable_Query", Gated<Script_Query>);
    Lua::RegisterFunction("WorldTable_GetRows", Gated<Script_GetRows>);
    Lua::RegisterFunction("WorldTable_Save", Gated<Script_Save>);
    Lua::RegisterFunction("WorldTable_RequestScriptNames", Gated<Script_RequestScriptNames>);
    Lua::RegisterFunction("WorldTable_GetScriptNames", Gated<Script_GetScriptNames>);
}

void OnGameDestroy() {
    s_rows.clear();
    s_columns.clear();

    for (uint8_t kind = 0; kind < kScriptKinds; kind++) {
        s_scriptNames[kind].clear();
        s_scriptReset[kind] = true;
    }
}

}
