#include "HoT/Editor/Dbc/DbcEditor.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "HoT/Core/Log.h"
#include "HoT/DB/Custom.h"
#include "HoT/DB/Refresh.h"
#include "HoT/Editor/Dbc/Schema.h"
#include "HoT/Editor/Dbc/Store.h"
#include "HoT/Editor/Light/LightEditor.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Magic/MagicClient/SpellVisuals.h"
#include "WoW/Source/Object/ObjectClient/Effect_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/QuestTextParser.h"
#include "WoW/Source/Ui/SpellBookFrame.h"

namespace HoT::Editor::Dbc {

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr int32_t kDefaultFindLimit = 200;
static constexpr int32_t kMaxFindLimit = 2000;
static constexpr uint32_t kDefaultLocaleFlags = 0x00FF01FE;
// A new id grows the by-id index to cover it, this keeps that index a sane size.
static constexpr int32_t kMaxServerRecordId = 0x00FFFFFF;

struct Value {
    uint32_t raw = 0;
    std::string text;
};

using Row = std::vector<Value>;

// Tables whose loaded record differs from the file row, rows for Lua and the server always hold the file values.
struct Codec {
    const char* stem;
    void (*toFile)(uint8_t* record);
    void (*toClient)(uint8_t* record);
};

static const Codec s_codecs[] = {
    { "Light", LightEditor::RecordToDbc, LightEditor::RecordToWorld },
};

struct Table {
    const Store* store;
    const Schema* schema;
    const Codec* codec = nullptr;
};

// Full rows as the server last sent them, they carry the columns the loaded record drops.
static std::unordered_map<std::string, std::unordered_map<int32_t, Row>> s_serverRows;

// The row before the first local preview edit, nullopt when the row did not exist.
static std::map<std::pair<std::string, int32_t>, std::optional<Row>> s_localOriginals;

static std::unordered_set<std::string> s_strings;

static bool s_spellsDirty = false;
static bool s_modelAttachDirty = false;
static bool s_effectNamesDirty = false;

struct ClassMaskBit {
    std::vector<std::string> names;
    int32_t spells = 0;
};

// Bits of the spell class mask by family, rebuilt after a Spell row changes.
static std::unordered_map<int32_t, std::vector<ClassMaskBit>> s_classMaskBits;

static const char* Intern(const std::string& text) {
    return s_strings.insert(text).first->c_str();
}

static std::string Lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    return text;
}

static bool Resolve(const char* name, Table& table) {
    const Store* store = FindStore(name);

    if (!store) {
        return false;
    }

    const Schema* schema = GetSchema(store->stem);

    if (!schema) {
        return false;
    }

    if (schema->recordSize != store->recordSize) {
        static std::unordered_set<std::string> s_reported;

        if (s_reported.insert(store->stem).second) {
            LOG_ERROR << "game editor schema of " << store->stem << " lays out " << schema->recordSize << " bytes, the client record has " << store->recordSize;
        }

        return false;
    }

    const Codec* codec = nullptr;

    for (const Codec& entry : s_codecs) {
        if (_stricmp(entry.stem, store->stem) == 0) {
            codec = &entry;
        }
    }

    table = { store, schema, codec };
    return true;
}

static bool ReadRecord(const Table& table, int32_t id, uint8_t* out) {
    if (!table.store->read(id, out)) {
        return false;
    }

    if (table.codec) {
        table.codec->toFile(out);
    }

    return true;
}

static bool ReadRecordIndex(const Table& table, int32_t index, uint8_t* out) {
    if (!table.store->readIndex(index, out)) {
        return false;
    }

    if (table.codec) {
        table.codec->toFile(out);
    }

    return true;
}

static bool ExpectsText(const Column& column) {
    return IsStringField(column) || (column.slot == Slot::Dropped && column.type == ColumnType::String);
}

static const char* ReadPointer(const uint8_t* record, uint32_t offset) {
    const char* text = nullptr;
    memcpy(&text, record + offset, sizeof(text));
    return text ? text : "";
}

static Value DefaultValue(const Column& column) {
    Value value;

    if (column.slot == Slot::Dropped && column.localeFlags) {
        value.raw = kDefaultLocaleFlags;
    }

    return value;
}

static void DefaultRow(const Schema& schema, int32_t id, Row& row) {
    row.clear();

    for (const Column& column : schema.columns) {
        row.push_back(DefaultValue(column));
    }

    if (!row.empty()) {
        row[0].raw = static_cast<uint32_t>(id);
    }
}

static void RecordToRow(const Table& table, int32_t id, const uint8_t* record, Row& row) {
    const Row* server = nullptr;
    auto rows = s_serverRows.find(table.store->stem);

    if (rows != s_serverRows.end()) {
        auto it = rows->second.find(id);
        server = it != rows->second.end() && it->second.size() == table.schema->columns.size() ? &it->second : nullptr;
    }

    row.clear();
    row.reserve(table.schema->columns.size());

    for (size_t i = 0; i < table.schema->columns.size(); i++) {
        const Column& column = table.schema->columns[i];
        Value value;

        if (column.slot == Slot::Dropped) {
            value = server ? (*server)[i] : DefaultValue(column);
        } else if (IsStringField(column)) {
            value.text = ReadPointer(record, column.offset);
        } else if (column.size == 1) {
            value.raw = record[column.offset];
        } else {
            memcpy(&value.raw, record + column.offset, sizeof(value.raw));
        }

        row.push_back(std::move(value));
    }
}

static bool ReadRow(const Table& table, int32_t id, Row& row) {
    std::vector<uint8_t> record(table.store->recordSize);

    if (!ReadRecord(table, id, record.data())) {
        return false;
    }

    RecordToRow(table, id, record.data(), row);
    return true;
}

static void OnRowWritten(const Store& store, int32_t id, bool inserted) {
    std::string stem = store.stem;

    if (stem == "Spell") {
        HoT::DB::GrowKnownSpellBits();
        s_spellsDirty = true;
        s_classMaskBits.clear();
    } else if (stem == "SpellClassMaskExtension") {
        s_classMaskBits.clear();
    } else if (stem == "SpellVisualKit" || stem == "SpellVisualKitModelAttach") {
        s_modelAttachDirty = true;
    } else if (stem == "SpellVisualEffectName") {
        s_effectNamesDirty = true;
    } else if (stem == "Light") {
        LightEditor::OnLightWritten();
    }
}

// The raw record a row stands for, columns the client drops keep the loaded value.
static bool EncodeRow(const Table& table, int32_t id, const Row& row, std::vector<uint8_t>& record) {
    const Schema& schema = *table.schema;

    if (row.size() != schema.columns.size()) {
        return false;
    }

    record.assign(table.store->recordSize, 0);

    if (!ReadRecord(table, id, record.data())) {
        const char* empty = Intern("");

        for (const Column& column : schema.columns) {
            if (IsStringField(column)) {
                memcpy(record.data() + column.offset, &empty, sizeof(empty));
            }
        }
    }

    for (size_t i = 0; i < schema.columns.size(); i++) {
        const Column& column = schema.columns[i];
        const Value& value = row[i];

        if (column.slot == Slot::Dropped) {
            continue;
        }

        if (IsStringField(column)) {
            const char* text = Intern(value.text);
            memcpy(record.data() + column.offset, &text, sizeof(text));
        } else if (column.size == 1) {
            record[column.offset] = static_cast<uint8_t>(value.raw);
        } else {
            memcpy(record.data() + column.offset, &value.raw, sizeof(value.raw));
        }
    }

    int32_t recordId = id;
    memcpy(record.data(), &recordId, sizeof(recordId));

    if (table.codec) {
        table.codec->toClient(record.data());
    }

    return true;
}

static bool WriteRow(const Table& table, int32_t id, const Row& row) {
    std::vector<uint8_t> record;

    if (!EncodeRow(table, id, row, record)) {
        return false;
    }

    bool inserted = false;

    if (!table.store->write(id, record.data(), &inserted)) {
        return false;
    }

    OnRowWritten(*table.store, id, inserted);
    return true;
}

static uint32_t ParseNumber(const char* text, ColumnType type) {
    if (!text) {
        return 0;
    }

    if (type == ColumnType::Float) {
        float f = static_cast<float>(std::strtod(text, nullptr));
        uint32_t raw;
        memcpy(&raw, &f, sizeof(raw));
        return raw;
    }

    return static_cast<uint32_t>(std::strtoll(text, nullptr, 0));
}

static uint32_t NumberBits(double number, ColumnType type) {
    switch (type) {
        case ColumnType::Float: {
            float f = static_cast<float>(number);
            uint32_t raw;
            memcpy(&raw, &f, sizeof(raw));
            return raw;
        }
        case ColumnType::UInt8:
        case ColumnType::Int8:
            return static_cast<uint32_t>(static_cast<int64_t>(number)) & 0xFF;
        default:
            return number < 0.0 ? static_cast<uint32_t>(static_cast<int64_t>(number)) : static_cast<uint32_t>(static_cast<uint64_t>(number));
    }
}

static bool LuaToValue(lua_State* L, int index, const Column& column, Value& value) {
    int type = lua_type(L, index);

    if (ExpectsText(column)) {
        if (type == LUA_TSTRING || type == LUA_TNUMBER) {
            const char* text = lua_tostring(L, index);
            value.text = text ? text : "";
            return true;
        }

        if (type == LUA_TBOOLEAN) {
            value.text = lua_toboolean(L, index) ? "1" : "0";
            return true;
        }

        return false;
    }

    switch (type) {
        case LUA_TNUMBER:
            value.raw = NumberBits(lua_tonumber(L, index), column.type);
            return true;
        case LUA_TBOOLEAN:
            value.raw = NumberBits(lua_toboolean(L, index) ? 1.0 : 0.0, column.type);
            return true;
        case LUA_TSTRING:
            value.raw = ParseNumber(lua_tostring(L, index), column.type);
            return true;
        default:
            return false;
    }
}

static void PushValue(lua_State* L, const Column& column, const Value& value) {
    if (ExpectsText(column)) {
        lua_pushstring(L, value.text.c_str());
        return;
    }

    switch (column.type) {
        case ColumnType::Int:
            lua_pushnumber(L, static_cast<int32_t>(value.raw));
            break;
        case ColumnType::Int8:
            lua_pushnumber(L, static_cast<int8_t>(value.raw));
            break;
        case ColumnType::UInt8:
            lua_pushnumber(L, value.raw & 0xFF);
            break;
        case ColumnType::Float: {
            float f;
            memcpy(&f, &value.raw, sizeof(f));
            lua_pushnumber(L, f);
            break;
        }
        default:
            lua_pushnumber(L, value.raw);
            break;
    }
}

// Keys are column names (case-insensitive) or 1-based column indexes.
static void MergeLuaValues(lua_State* L, int tableIndex, const Schema& schema, Row& row) {
    if (lua_type(L, tableIndex) != LUA_TTABLE) {
        return;
    }

    lua_pushnil(L);

    while (lua_next(L, tableIndex)) {
        int32_t column = -1;

        if (lua_type(L, -2) == LUA_TSTRING) {
            column = FindColumn(schema, lua_tostring(L, -2));
        } else if (lua_type(L, -2) == LUA_TNUMBER) {
            column = static_cast<int32_t>(lua_tonumber(L, -2)) - 1;
        }

        if (column >= 0 && static_cast<size_t>(column) < schema.columns.size()) {
            LuaToValue(L, lua_gettop(L), schema.columns[column], row[column]);
        }

        lua_pop(L, 1);
    }
}

static bool IdArgument(lua_State* L, int index, int32_t& id) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        id = static_cast<int32_t>(static_cast<int64_t>(lua_tonumber(L, index)));
        return true;
    }

    if (lua_type(L, index) == LUA_TSTRING) {
        const char* text = lua_tostring(L, index);
        char* end = nullptr;
        long long value = std::strtoll(text, &end, 0);

        if (end != text) {
            id = static_cast<int32_t>(value);
            return true;
        }
    }

    return false;
}

static bool TableArgument(lua_State* L, int index, Table& table) {
    return lua_type(L, index) == LUA_TSTRING && Resolve(lua_tostring(L, index), table);
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

static bool ReadWireValue(Net::Packet& packet, const Column& column, Value& value) {
    if (!packet.CanRead(1)) {
        return false;
    }

    uint8_t type = packet.GetUInt8();

    switch (type) {
        case 0:
        case 1:
        case 2:
            if (!packet.CanRead(4)) {
                return false;
            }

            value.raw = packet.GetUInt32();
            break;
        case 3:
            value.text = ReadCString(packet);
            break;
        case 4:
        case 5:
            if (!packet.CanRead(8)) {
                return false;
            }

            value.raw = static_cast<uint32_t>(packet.GetUInt64());
            break;
        default:
            return false;
    }

    if (ExpectsText(column) && type != 3) {
        value.text = type == 1 ? std::to_string(static_cast<int32_t>(value.raw)) : std::to_string(value.raw);
    } else if (!ExpectsText(column) && type == 3) {
        value.raw = ParseNumber(value.text.c_str(), column.type);
    }

    return true;
}

static void WriteWireValue(Net::Packet& packet, const Column& column, const Value& value) {
    if (ExpectsText(column)) {
        packet.PutUInt8(3);
        packet.PutString(value.text.c_str());
        return;
    }

    packet.PutUInt8(WireType(column.type));
    packet.PutUInt32(value.raw);
}

static int32_t Packet_SMSG_DBC_RECORD_UPDATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    std::string name = ReadCString(packet);
    int32_t id = static_cast<int32_t>(packet.GetUInt32());
    uint16_t count = packet.GetUInt16();

    Table table;

    if (!Resolve(name.c_str(), table)) {
        LOG_DEBUG << "DBC update for " << name << " " << id << " has no editable client table";
        return 1;
    }

    if (id < 0 || id > kMaxServerRecordId) {
        LOG_WARN << "DBC update for " << name << " has out of range id " << id;
        return 1;
    }

    const Schema& schema = *table.schema;

    if (count != schema.columns.size()) {
        LOG_WARN << "DBC update for " << name << " " << id << " has " << count << " columns, the binding has " << schema.columns.size();
        return 1;
    }

    Row row(count);

    for (uint16_t i = 0; i < count; i++) {
        if (!ReadWireValue(packet, schema.columns[i], row[i])) {
            LOG_WARN << "DBC update for " << name << " " << id << " is truncated";
            return 1;
        }
    }

    s_serverRows[table.store->stem][id] = row;
    s_localOriginals.erase({ table.store->stem, id });

    if (!WriteRow(table, id, row)) {
        return 1;
    }

    Lua::SignalEvent("DBC_RECORD_UPDATED", "%s%d", table.store->stem, id);
    return 1;
}

static int32_t Packet_SMSG_DBC_EDITOR_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    int32_t ok = packet.GetUInt8() != 0;
    std::string name = ReadCString(packet);
    int32_t id = static_cast<int32_t>(packet.GetUInt32());
    std::string message = ReadCString(packet);

    if (const Store* store = FindStore(name.c_str())) {
        name = store->stem;
    }

    Lua::SignalEvent("DBC_EDITOR_RESULT", "%b%s%d%s", ok, name.c_str(), id, message.c_str());
    return 1;
}

// DbcEditor_GetTables() -> { "Spell", ... } for every table with a binding the client can edit.
static int32_t Script_GetTables(lua_State* L) {
    lua_newtable(L);
    int result = lua_gettop(L);
    int n = 0;

    for (const Store& store : Stores()) {
        Table table;

        if (Resolve(store.stem, table)) {
            lua_pushstring(L, store.stem);
            lua_rawseti(L, result, ++n);
        }
    }

    return 1;
}

// DbcEditor_GetSchema(table) -> { { name, type, stored, isName }, ... } in column order.
static int32_t Script_GetSchema(lua_State* L) {
    Table table;

    if (!TableArgument(L, 1, table)) {
        lua_pushnil(L);
        return 1;
    }

    const Schema& schema = *table.schema;
    lua_createtable(L, static_cast<int>(schema.columns.size()), 0);
    int result = lua_gettop(L);

    for (size_t i = 0; i < schema.columns.size(); i++) {
        const Column& column = schema.columns[i];
        lua_createtable(L, 0, 3);
        lua_pushstring(L, column.name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushstring(L, TypeName(column.type));
        lua_setfield(L, -2, "type");
        lua_pushboolean(L, column.slot != Slot::Dropped);
        lua_setfield(L, -2, "stored");

        if (static_cast<int32_t>(i) == schema.nameColumn) {
            lua_pushboolean(L, true);
            lua_setfield(L, -2, "isName");
        }

        if (column.slot == Slot::Value && column.size == 1) {
            lua_pushnumber(L, 1);
            lua_setfield(L, -2, "size");
        }

        lua_rawseti(L, result, static_cast<int>(i + 1));
    }

    return 1;
}

static void PushRow(lua_State* L, const Schema& schema, const Row& row) {
    lua_createtable(L, 0, static_cast<int>(schema.columns.size()));
    int result = lua_gettop(L);

    for (size_t i = 0; i < schema.columns.size(); i++) {
        PushValue(L, schema.columns[i], row[i]);
        lua_setfield(L, result, schema.columns[i].name.c_str());
    }
}

// DbcEditor_GetRow(table, id) -> { column = value, ... } or nil.
static int32_t Script_GetRow(lua_State* L) {
    Table table;
    int32_t id;
    Row row;

    if (!TableArgument(L, 1, table) || !IdArgument(L, 2, id) || !ReadRow(table, id, row)) {
        lua_pushnil(L);
        return 1;
    }

    PushRow(L, *table.schema, row);
    return 1;
}

static bool ContainsNoCase(const char* haystack, const std::string& needleLower) {
    if (needleLower.empty()) {
        return true;
    }

    std::string text = Lower(haystack ? haystack : "");
    return text.find(needleLower) != std::string::npos;
}

// DbcEditor_Find(table, query [, limit]) or DbcEditor_Find(table, column, query [, limit]) -> { { id, name, text }, ... }.
static int32_t Script_Find(lua_State* L) {
    Table table;

    if (!TableArgument(L, 1, table)) {
        lua_pushnil(L);
        return 1;
    }

    const Schema& schema = *table.schema;
    int queryIndex = 2;
    int32_t column = -1;

    if (lua_type(L, 2) == LUA_TSTRING && (lua_type(L, 3) == LUA_TSTRING || lua_type(L, 3) == LUA_TNUMBER)) {
        column = FindColumn(schema, lua_tostring(L, 2));

        if (column >= 0) {
            queryIndex = 3;
        }
    }

    if (column >= 0 && schema.columns[column].slot == Slot::Dropped) {
        lua_newtable(L);
        return 1;
    }

    int32_t limit = lua_type(L, queryIndex + 1) == LUA_TNUMBER ? static_cast<int32_t>(lua_tonumber(L, queryIndex + 1)) : kDefaultFindLimit;
    limit = std::clamp(limit, 1, kMaxFindLimit);

    const char* queryText = lua_type(L, queryIndex) == LUA_TSTRING || lua_type(L, queryIndex) == LUA_TNUMBER ? lua_tostring(L, queryIndex) : "";
    std::string needle = Lower(queryText ? queryText : "");
    int32_t queryId = 0;
    bool numeric = IdArgument(L, queryIndex, queryId);

    const Column* searchColumn = column >= 0 ? &schema.columns[column] : (schema.nameColumn >= 0 ? &schema.columns[schema.nameColumn] : nullptr);
    uint32_t queryBits = 0;

    if (column >= 0 && !IsStringField(*searchColumn)) {
        Value value;

        if (!LuaToValue(L, queryIndex, *searchColumn, value)) {
            lua_newtable(L);
            return 1;
        }

        queryBits = value.raw;
    }

    const Column* nameColumn = schema.nameColumn >= 0 ? &schema.columns[schema.nameColumn] : nullptr;
    std::vector<uint8_t> record(table.store->recordSize);

    lua_newtable(L);
    int result = lua_gettop(L);
    int found = 0;

    auto push = [&](int32_t id, const uint8_t* rec) {
        const char* name = nameColumn ? ReadPointer(rec, nameColumn->offset) : "";
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, id);
        lua_setfield(L, -2, "id");
        lua_pushstring(L, name);
        lua_setfield(L, -2, "name");
        lua_pushstring(L, name);
        lua_setfield(L, -2, "text");
        lua_rawseti(L, result, ++found);
    };

    int32_t pinned = 0;
    bool hasPinned = false;

    if (column < 0 && numeric && ReadRecord(table, queryId, record.data())) {
        push(queryId, record.data());
        pinned = queryId;
        hasPinned = true;
    }

    int32_t total = table.store->db->m_numRecords;

    for (int32_t index = 0; index < total && found < limit; index++) {
        if (!ReadRecordIndex(table, index, record.data())) {
            continue;
        }

        int32_t id;
        memcpy(&id, record.data(), sizeof(id));

        if (hasPinned && id == pinned) {
            continue;
        }

        bool match = false;

        if (column >= 0) {
            if (IsStringField(*searchColumn)) {
                match = ContainsNoCase(ReadPointer(record.data(), searchColumn->offset), needle);
            } else {
                uint32_t raw = 0;

                if (searchColumn->size == 1) {
                    raw = record[searchColumn->offset];
                } else {
                    memcpy(&raw, record.data() + searchColumn->offset, sizeof(raw));
                }

                match = raw == queryBits;
            }
        } else if (searchColumn && !needle.empty()) {
            match = ContainsNoCase(ReadPointer(record.data(), searchColumn->offset), needle);
        }

        if (match) {
            push(id, record.data());
        }
    }

    return 1;
}

static constexpr int32_t kMaxFilterLimit = 200000;

struct FilterCondition {
    std::vector<std::pair<const Column*, uint32_t>> columns;
};

static uint32_t ReadRaw(const uint8_t* record, const Column& column) {
    uint32_t raw = 0;

    if (column.size == 1) {
        raw = record[column.offset];
    } else {
        memcpy(&raw, record + column.offset, sizeof(raw));
    }

    return raw;
}

// DbcEditor_Filter(table, { text = "", where = { { columns = { ... }, value = v }, ... } } [, limit]) -> { { id, name }, ... }.
// A row matches when its name holds the text and every condition has one of its columns equal to the value.
static int32_t Script_Filter(lua_State* L) {
    Table table;

    if (!TableArgument(L, 1, table) || lua_type(L, 2) != LUA_TTABLE) {
        lua_pushnil(L);
        return 1;
    }

    const Schema& schema = *table.schema;
    int32_t limit = lua_type(L, 3) == LUA_TNUMBER ? static_cast<int32_t>(lua_tonumber(L, 3)) : kMaxFilterLimit;
    limit = std::clamp(limit, 1, kMaxFilterLimit);

    lua_getfield(L, 2, "text");
    const char* queryText = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "";
    std::string needle = Lower(queryText ? queryText : "");
    lua_pop(L, 1);

    std::vector<FilterCondition> conditions;
    lua_getfield(L, 2, "where");

    if (lua_type(L, -1) == LUA_TTABLE) {
        int where = lua_gettop(L);
        int count = static_cast<int>(lua_objlen(L, where));

        for (int i = 1; i <= count; i++) {
            lua_rawgeti(L, where, i);
            int entry = lua_gettop(L);

            if (lua_type(L, entry) == LUA_TTABLE) {
                FilterCondition condition;
                lua_getfield(L, entry, "columns");
                int columns = lua_gettop(L);
                lua_getfield(L, entry, "value");
                int value = lua_gettop(L);

                if (lua_type(L, columns) == LUA_TTABLE) {
                    int columnCount = static_cast<int>(lua_objlen(L, columns));

                    for (int c = 1; c <= columnCount; c++) {
                        lua_rawgeti(L, columns, c);
                        int32_t index = lua_type(L, -1) == LUA_TSTRING ? FindColumn(schema, lua_tostring(L, -1)) : -1;
                        lua_pop(L, 1);

                        if (index < 0 || schema.columns[index].slot == Slot::Dropped || IsStringField(schema.columns[index])) {
                            continue;
                        }

                        Value parsed;

                        if (LuaToValue(L, value, schema.columns[index], parsed)) {
                            condition.columns.emplace_back(&schema.columns[index], parsed.raw);
                        }
                    }
                }

                lua_settop(L, entry - 1);

                // A condition on columns this table lacks can never match.
                if (condition.columns.empty()) {
                    lua_settop(L, where - 1);
                    lua_newtable(L);
                    return 1;
                }

                conditions.push_back(std::move(condition));
            } else {
                lua_pop(L, 1);
            }
        }
    }

    lua_settop(L, 3);

    const Column* nameColumn = schema.nameColumn >= 0 ? &schema.columns[schema.nameColumn] : nullptr;
    std::vector<uint8_t> record(table.store->recordSize);

    lua_newtable(L);
    int result = lua_gettop(L);
    int found = 0;
    int32_t total = table.store->db->m_numRecords;

    for (int32_t index = 0; index < total && found < limit; index++) {
        if (!ReadRecordIndex(table, index, record.data())) {
            continue;
        }

        const char* name = nameColumn ? ReadPointer(record.data(), nameColumn->offset) : "";

        if (!needle.empty() && !ContainsNoCase(name, needle)) {
            continue;
        }

        bool match = true;

        for (const FilterCondition& condition : conditions) {
            bool any = false;

            for (const auto& [column, raw] : condition.columns) {
                if (ReadRaw(record.data(), *column) == raw) {
                    any = true;
                    break;
                }
            }

            if (!any) {
                match = false;
                break;
            }
        }

        if (!match) {
            continue;
        }

        int32_t id;
        memcpy(&id, record.data(), sizeof(id));
        lua_createtable(L, 0, 2);
        lua_pushnumber(L, id);
        lua_setfield(L, -2, "id");
        lua_pushstring(L, name ? name : "");
        lua_setfield(L, -2, "name");
        lua_rawseti(L, result, ++found);
    }

    return 1;
}

// DbcEditor_GetMaxId(table) -> the highest id in the loaded table, 0 when it is empty.
static int32_t Script_GetMaxId(lua_State* L) {
    Table table;

    if (!TableArgument(L, 1, table)) {
        lua_pushnil(L);
        return 1;
    }

    WowClientDB_Base* db = table.store->db;
    lua_pushnumber(L, db->m_numRecords > 0 && db->m_maxID > 0 ? db->m_maxID : 0);
    return 1;
}

static bool BuildRow(lua_State* L, const Table& table, int32_t id, int valuesIndex, Row& row) {
    if (!ReadRow(table, id, row)) {
        DefaultRow(*table.schema, id, row);
    }

    MergeLuaValues(L, valuesIndex, *table.schema, row);
    row[0].raw = static_cast<uint32_t>(id);
    return true;
}

// DbcEditor_ApplyLocal(table, id, values) patches the live client row only, values may be partial.
static int32_t Script_ApplyLocal(lua_State* L) {
    Table table;
    int32_t id;

    if (!TableArgument(L, 1, table) || !IdArgument(L, 2, id) || id < 0 || id > kMaxServerRecordId || lua_type(L, 3) != LUA_TTABLE) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Row row;
    std::pair<std::string, int32_t> key(table.store->stem, id);

    if (!s_localOriginals.count(key)) {
        Row original;
        s_localOriginals[key] = ReadRow(table, id, original) ? std::optional<Row>(std::move(original)) : std::nullopt;
    }

    BuildRow(L, table, id, 3, row);
    lua_pushboolean(L, WriteRow(table, id, row));
    return 1;
}

// DbcEditor_RevertLocal(table, id) restores the row from before the first ApplyLocal, new rows stay.
static int32_t Script_RevertLocal(lua_State* L) {
    Table table;
    int32_t id;

    if (!TableArgument(L, 1, table) || !IdArgument(L, 2, id)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    auto it = s_localOriginals.find({ table.store->stem, id });

    if (it == s_localOriginals.end()) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool ok = it->second && WriteRow(table, id, *it->second);
    s_localOriginals.erase(it);
    lua_pushboolean(L, ok);
    return 1;
}

static std::vector<uint8_t> s_tooltipRecord;
static int32_t s_tooltipPatchId = 0;

static bool SpellRecordArgument(lua_State* L, int32_t& id, std::vector<uint8_t>& record) {
    Table table;

    if (!Resolve("Spell", table) || !IdArgument(L, 1, id)) {
        return false;
    }

    Row row;

    if (lua_type(L, 2) == LUA_TTABLE) {
        BuildRow(L, table, id, 2, row);
    } else if (!ReadRow(table, id, row)) {
        return false;
    }

    return EncodeRow(table, id, row, record) && record.size() == sizeof(SpellRec);
}

// DbcEditor_ParseSpellText(id [, values] [, aura]) runs the client's own description parser on the row, aura picks the buff text.
static int32_t Script_ParseSpellText(lua_State* L) {
    int32_t id;
    std::vector<uint8_t> record;

    if (!SpellRecordArgument(L, id, record)) {
        lua_pushnil(L);
        return 1;
    }

    SpellRec spell;
    memcpy(&spell, record.data(), sizeof(spell));

    char text[2048];
    text[0] = 0;
    SpellParserParseText(&spell, text, sizeof(text), 0, 0, 0, lua_toboolean(L, 3) ? 1 : 0, 1, 0);
    lua_pushstring(L, text);
    return 1;
}

static void ApplyTooltipRecord(SpellRec& rec) {
    memcpy(&rec, s_tooltipRecord.data(), sizeof(rec));
}

// DbcEditor_PatchSpellTooltip(id, values) makes client lookups see the unsaved values until DbcEditor_UnpatchSpellTooltip.
static int32_t Script_PatchSpellTooltip(lua_State* L) {
    int32_t id;

    if (s_tooltipPatchId) {
        UnpatchSpell(s_tooltipPatchId);
        s_tooltipPatchId = 0;
    }

    if (!SpellRecordArgument(L, id, s_tooltipRecord) || !PatchSpell(id, ApplyTooltipRecord)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    s_tooltipPatchId = id;
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t Script_UnpatchSpellTooltip(lua_State* L) {
    if (s_tooltipPatchId) {
        UnpatchSpell(s_tooltipPatchId);
        s_tooltipPatchId = 0;
    }

    return 0;
}

static constexpr size_t kClassMaskBits = 96;
static constexpr size_t kClassMaskNames = 3;

static const std::vector<ClassMaskBit>& ClassMaskBits(int32_t family) {
    auto it = s_classMaskBits.find(family);

    if (it != s_classMaskBits.end()) {
        return it->second;
    }

    std::vector<ClassMaskBit> bits(kClassMaskBits);

    auto add = [&](const int32_t* mask, const char* spellName) {
        std::string name = spellName ? spellName : "";

        for (size_t bit = 0; bit < kClassMaskBits; bit++) {
            if (!(static_cast<uint32_t>(mask[bit / 32]) & (1u << (bit % 32)))) {
                continue;
            }

            ClassMaskBit& entry = bits[bit];
            entry.spells++;

            if (!name.empty() && entry.names.size() < kClassMaskNames && std::find(entry.names.begin(), entry.names.end(), name) == entry.names.end()) {
                entry.names.push_back(name);
            }
        }
    };

    Table spells;

    if (Resolve("Spell", spells)) {
        std::vector<uint8_t> record(spells.store->recordSize);
        int32_t total = spells.store->db->m_numRecords;

        for (int32_t index = 0; index < total; index++) {
            if (spells.store->readIndex(index, record.data())) {
                const SpellRec* spell = reinterpret_cast<const SpellRec*>(record.data());

                if (spell->m_spellClassSet == family) {
                    add(spell->m_spellClassMask, spell->m_name);
                }
            }
        }

        // Extension blocks put a spell into more families, the client and the server match spell mods against them too.
        Table extensions;

        if (Resolve("SpellClassMaskExtension", extensions)) {
            std::vector<uint8_t> row(extensions.store->recordSize);
            int32_t rows = extensions.store->db->m_numRecords;

            for (int32_t index = 0; index < rows; index++) {
                if (!extensions.store->readIndex(index, row.data())) {
                    continue;
                }

                const HoT::DB::SpellClassMaskExtensionRec* ext = reinterpret_cast<const HoT::DB::SpellClassMaskExtensionRec*>(row.data());
                const char* name = spells.store->read(ext->m_id, record.data()) ? reinterpret_cast<const SpellRec*>(record.data())->m_name : nullptr;

                for (const auto& block : ext->m_blocks) {
                    if (block.m_spellClassSet == family) {
                        add(block.m_spellClassMask, name);
                    }
                }
            }
        }
    }

    return s_classMaskBits.emplace(family, std::move(bits)).first->second;
}

// DbcEditor_GetClassMaskNames(family) -> { [bit 0-95] = { name, tooltip } } for the bits spells of that family carry, extension blocks included.
static int32_t Script_GetClassMaskNames(lua_State* L) {
    int32_t family;

    if (!IdArgument(L, 1, family)) {
        lua_pushnil(L);
        return 1;
    }

    const std::vector<ClassMaskBit> bits = ClassMaskBits(family);
    lua_newtable(L);
    int result = lua_gettop(L);

    for (size_t bit = 0; bit < bits.size(); bit++) {
        const ClassMaskBit& entry = bits[bit];

        if (!entry.spells) {
            continue;
        }

        std::string name;

        for (const std::string& part : entry.names) {
            name += (name.empty() ? "" : ", ") + part;
        }

        lua_createtable(L, 0, 2);
        lua_pushstring(L, name.c_str());
        lua_setfield(L, -2, "name");
        std::string tooltip = std::to_string(entry.spells) + (entry.spells == 1 ? " spell carries" : " spells carry") + " the bit in this family";
        lua_pushstring(L, tooltip.c_str());
        lua_setfield(L, -2, "tooltip");
        lua_rawseti(L, result, static_cast<int>(bit));
    }

    return 1;
}

// DbcEditor_GetClassMaskSpells(family, mask0, mask1, mask2[, limit]) -> { { id, name, extension } } for the spells a spell mod of that family and mask affects.
static int32_t Script_GetClassMaskSpells(lua_State* L) {
    int32_t family;

    if (!IdArgument(L, 1, family) || family <= 0) {
        lua_pushnil(L);
        return 1;
    }

    uint32_t mask[3];

    for (int i = 0; i < 3; i++) {
        mask[i] = lua_type(L, 2 + i) == LUA_TNUMBER ? static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 2 + i))) : 0;
    }

    int32_t limit = lua_type(L, 5) == LUA_TNUMBER ? static_cast<int32_t>(lua_tonumber(L, 5)) : 2000;
    lua_newtable(L);
    int result = lua_gettop(L);

    if (!(mask[0] | mask[1] | mask[2])) {
        return 1;
    }

    auto overlaps = [&](const int32_t* other) {
        for (int i = 0; i < 3; i++) {
            if (mask[i] & static_cast<uint32_t>(other[i])) {
                return true;
            }
        }

        return false;
    };

    int count = 0;

    auto push = [&](int32_t id, const char* name, bool extension) {
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, id);
        lua_setfield(L, -2, "id");
        lua_pushstring(L, name ? name : "");
        lua_setfield(L, -2, "name");
        lua_pushboolean(L, extension);
        lua_setfield(L, -2, "extension");
        lua_rawseti(L, result, ++count);
    };

    Table spells;

    if (!Resolve("Spell", spells)) {
        return 1;
    }

    std::vector<uint8_t> record(spells.store->recordSize);
    int32_t total = spells.store->db->m_numRecords;

    for (int32_t index = 0; index < total && count < limit; index++) {
        if (spells.store->readIndex(index, record.data())) {
            const SpellRec* spell = reinterpret_cast<const SpellRec*>(record.data());

            if (spell->m_spellClassSet == family && overlaps(spell->m_spellClassMask)) {
                push(spell->m_id, spell->m_name, false);
            }
        }
    }

    Table extensions;

    if (Resolve("SpellClassMaskExtension", extensions)) {
        std::vector<uint8_t> row(extensions.store->recordSize);
        int32_t rows = extensions.store->db->m_numRecords;

        for (int32_t index = 0; index < rows && count < limit; index++) {
            if (!extensions.store->readIndex(index, row.data())) {
                continue;
            }

            const HoT::DB::SpellClassMaskExtensionRec* ext = reinterpret_cast<const HoT::DB::SpellClassMaskExtensionRec*>(row.data());

            for (const auto& block : ext->m_blocks) {
                if (block.m_spellClassSet == family && overlaps(block.m_spellClassMask)) {
                    const char* name = spells.store->read(ext->m_id, record.data()) ? reinterpret_cast<const SpellRec*>(record.data())->m_name : nullptr;
                    push(ext->m_id, name, true);
                    break;
                }
            }
        }
    }

    return 1;
}

// DbcEditor_Save(table, id, values) sends the full row, unspecified columns come from the current row or zero.
static int32_t Script_Save(lua_State* L) {
    Table table;
    int32_t id;

    if (!TableArgument(L, 1, table) || !IdArgument(L, 2, id) || id <= 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Row row;
    BuildRow(L, table, id, 3, row);

    const Schema& schema = *table.schema;
    Net::Packet packet(Net::CMSG_DBC_EDITOR_SAVE);
    packet.PutString(table.store->stem);
    packet.PutUInt32(static_cast<uint32_t>(id));
    packet.PutUInt16(static_cast<uint16_t>(schema.columns.size()));

    for (size_t i = 0; i < schema.columns.size(); i++) {
        WriteWireValue(packet, schema.columns[i], row[i]);
    }

    packet.Send();
    lua_pushboolean(L, 1);
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
    Net::RegisterWorldHandler(Net::SMSG_DBC_RECORD_UPDATE, Packet_SMSG_DBC_RECORD_UPDATE);
    Net::RegisterWorldHandler(Net::SMSG_DBC_EDITOR_RESULT, Packet_SMSG_DBC_EDITOR_RESULT);

    Lua::RegisterFunction("DbcEditor_GetTables", Gated<Script_GetTables>);
    Lua::RegisterFunction("DbcEditor_GetSchema", Gated<Script_GetSchema>);
    Lua::RegisterFunction("DbcEditor_GetRow", Gated<Script_GetRow>);
    Lua::RegisterFunction("DbcEditor_Find", Gated<Script_Find>);
    Lua::RegisterFunction("DbcEditor_Filter", Gated<Script_Filter>);
    Lua::RegisterFunction("DbcEditor_GetMaxId", Gated<Script_GetMaxId>);
    Lua::RegisterFunction("DbcEditor_ApplyLocal", Gated<Script_ApplyLocal>);
    Lua::RegisterFunction("DbcEditor_RevertLocal", Gated<Script_RevertLocal>);
    Lua::RegisterFunction("DbcEditor_Save", Gated<Script_Save>);
    Lua::RegisterFunction("DbcEditor_ParseSpellText", Gated<Script_ParseSpellText>);
    Lua::RegisterFunction("DbcEditor_PatchSpellTooltip", Gated<Script_PatchSpellTooltip>);
    Lua::RegisterFunction("DbcEditor_UnpatchSpellTooltip", Gated<Script_UnpatchSpellTooltip>);
    Lua::RegisterFunction("DbcEditor_GetClassMaskNames", Gated<Script_GetClassMaskNames>);
    Lua::RegisterFunction("DbcEditor_GetClassMaskSpells", Gated<Script_GetClassMaskSpells>);
}

void OnIdle() {
    if (s_spellsDirty && ClntObjMgrGetActivePlayerObj()) {
        s_spellsDirty = false;
        CGSpellBook::UpdateSpells(0, 0, 0);
    }

    if (s_modelAttachDirty) {
        s_modelAttachDirty = false;
        SpellVisualsInitializeModelAttachLookup();
    }

    if (s_effectNamesDirty) {
        s_effectNamesDirty = false;
        SpecialUnitEffectsInitialize();
    }
}

void OnGameDestroy() {
    s_spellsDirty = false;
    s_localOriginals.clear();
    s_classMaskBits.clear();
}

}
