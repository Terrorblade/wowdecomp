#include "HoT/Editor/GobEditor.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "HoT/Editor/Quat.h"
#include "HoT/Editor/Selection.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"

namespace HoT::Editor::GobEditor {

static constexpr int8_t kRequiredSecurityLevel = 3;

enum FieldType : uint8_t {
    FIELD_UINT = 0,
    FIELD_INT = 1,
    FIELD_FLOAT = 2,
    FIELD_STRING = 3,
    FIELD_UINT64 = 4,
    FIELD_INT64 = 5,
};

enum Scope : uint8_t {
    SCOPE_TEMPLATE = 0,
    SCOPE_SPAWN = 1,
    SCOPE_TEMPLATE_ADDON = 2,
    SCOPE_SPAWN_ADDON = 3,
    SCOPE_COUNT = 4,
};

struct Field {
    std::string m_key;
    uint8_t m_type = FIELD_UINT;
    double m_number = 0.0;
    std::string m_string;
};

struct Snapshot {
    bool m_valid = false;
    uint64_t m_guid = 0;
    uint32_t m_entry = 0;
    uint32_t m_spawnId = 0;
    std::string m_name;
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_z = 0.0f;
    float m_o = 0.0f;
    float m_rotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    uint8_t m_state = 0;
    uint32_t m_type = 0;
    uint32_t m_displayId = 0;
    bool m_has[SCOPE_COUNT] = { true, true, false, false };
    std::vector<Field> m_fields[SCOPE_COUNT];
};

static Snapshot s_data;
static Snapshot s_template;
static uint32_t s_templateEntry;

DecodedGuid DecodeGuid(uint64_t full) {
    DecodedGuid guid{};
    guid.m_full = full;
    guid.m_type = static_cast<uint16_t>(full >> 48);
    guid.m_entry = static_cast<uint32_t>((full >> 24) & 0xFFFFFF);
    guid.m_low = static_cast<uint32_t>(full & 0xFFFFFF);

    return guid;
}

std::string GuidToString(uint64_t guid) {
    return std::to_string(guid);
}

CGGameObject_C* GameObjectByGuid(uint64_t guid) {
    if (!guid) {
        return nullptr;
    }

    return static_cast<CGGameObject_C*>(ClntObjMgrObjectPtr(WOWGUID{ guid }, TYPE_GAMEOBJECT, __FILE__, __LINE__));
}

uint64_t& PackedRotation(CGGameObject_C* object) {
    return *reinterpret_cast<uint64_t*>(&object->m_passenger.m_facing);
}

static uint64_t GuidArgument(lua_State* L, int index, uint64_t fallback) {
    if (lua_type(L, index) == LUA_TSTRING) {
        if (const char* text = lua_tostring(L, index)) {
            return std::strtoull(text, nullptr, 10);
        }
    }

    return fallback;
}

static void ReadFields(Net::Packet& packet, std::vector<Field>& fields) {
    char buffer[512];
    uint16_t count = packet.GetUInt16();
    fields.clear();
    fields.reserve(count);

    for (uint16_t i = 0; i < count && packet.CanRead(1); i++) {
        Field field;
        packet.GetString(buffer, sizeof(buffer));
        field.m_key = buffer;
        field.m_type = packet.GetUInt8();

        switch (field.m_type) {
            case FIELD_UINT:
                field.m_number = packet.GetUInt32();
                break;
            case FIELD_INT:
                field.m_number = packet.GetInt32();
                break;
            case FIELD_FLOAT:
                field.m_number = packet.GetFloat();
                break;
            case FIELD_STRING:
                packet.GetString(buffer, sizeof(buffer));
                field.m_string = buffer;
                break;
            case FIELD_UINT64:
                field.m_string = std::to_string(packet.GetUInt64());
                break;
            case FIELD_INT64:
                field.m_string = std::to_string(packet.GetInt64());
                break;
            default:
                return;
        }

        fields.push_back(std::move(field));
    }
}

static int32_t Packet_SMSG_GOB_EDITOR_DATA(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    char buffer[512];

    if (packet.GetUInt8() != 1) {
        packet.GetString(buffer, sizeof(buffer));
        Lua::SignalEvent("HOT_GOB_EDITOR_RESULT", "%d%s", 0, buffer);
        return 1;
    }

    Snapshot data;
    data.m_guid = packet.GetUInt64();
    data.m_entry = packet.GetUInt32();
    data.m_spawnId = packet.GetUInt32();
    packet.GetString(buffer, sizeof(buffer));
    data.m_name = buffer;
    data.m_x = packet.GetFloat();
    data.m_y = packet.GetFloat();
    data.m_z = packet.GetFloat();
    data.m_o = packet.GetFloat();

    for (float& value : data.m_rotation) {
        value = packet.GetFloat();
    }

    data.m_state = packet.GetUInt8();
    data.m_type = packet.GetUInt32();
    data.m_displayId = packet.GetUInt32();

    ReadFields(packet, data.m_fields[SCOPE_TEMPLATE]);
    ReadFields(packet, data.m_fields[SCOPE_SPAWN]);

    for (uint8_t scope = SCOPE_TEMPLATE_ADDON; scope < SCOPE_COUNT; scope++) {
        data.m_has[scope] = packet.CanRead(1) && packet.GetUInt8() != 0;

        if (data.m_has[scope]) {
            ReadFields(packet, data.m_fields[scope]);
        }
    }

    data.m_valid = true;

    // A zero guid is a template snapshot, it never touches the selection.
    if (!data.m_guid) {
        s_template = std::move(data);
        Lua::SignalEvent("HOT_GOB_EDITOR_DATA", "%d", 1);
        return 1;
    }

    uint64_t previous = s_data.m_valid ? s_data.m_guid : 0;
    bool sameSpawn = previous && data.m_spawnId && data.m_spawnId == s_data.m_spawnId;
    s_data = std::move(data);

    if (sameSpawn && previous != s_data.m_guid) {
        Selection::Replace(previous, s_data.m_guid);
    }

    Selection::OnGameObjectSnapshot(s_data.m_guid);

    Lua::SignalEvent("HOT_GOB_EDITOR_DATA", "%d", 0);
    return 1;
}

static int32_t Packet_SMSG_GOB_EDITOR_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    char buffer[1024];
    int32_t ok = 0;

    {
        Net::Packet packet(msg);
        ok = packet.GetUInt8();
        packet.GetString(buffer, sizeof(buffer));
    }

    Lua::SignalEvent("HOT_GOB_EDITOR_RESULT", "%d%s", ok, buffer);
    return 1;
}

static int32_t Packet_SMSG_GOB_EDITOR_SELECT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    uint64_t guid = 0;

    {
        Net::Packet packet(msg);
        guid = packet.GetUInt64();
    }

    if (!Selection::Select(guid)) {
        return 1;
    }

    if (CGGameObject_C* object = GameObjectByGuid(guid)) {
        // HoT: the DLL passed a std::string object through the varargs for %s.
        std::string text = GuidToString(guid);
        const C3Vector& position = object->m_passenger.m_position;
        Lua::SignalEvent("HOT_GOB_SELECTED", "%s%u%f%f%f", text.c_str(), 0u, position.x, position.y, position.z);
    }

    return 1;
}

static void PushFields(lua_State* L, const std::vector<Field>& fields) {
    lua_createtable(L, 0, static_cast<int>(fields.size()));
    int tableIdx = lua_gettop(L);

    for (const Field& field : fields) {
        if (field.m_type == FIELD_STRING || field.m_type == FIELD_UINT64 || field.m_type == FIELD_INT64) {
            lua_pushstring(L, field.m_string.c_str());
        } else {
            lua_pushnumber(L, field.m_number);
        }

        lua_setfield(L, tableIdx, field.m_key.c_str());
    }
}

static void SetNumber(lua_State* L, int tableIdx, const char* key, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, tableIdx, key);
}

// GobEditor_Request([guid string]) asks for the snapshot of the guid, the selection or the target.
static int32_t Script_Request(lua_State* L) {
    uint64_t guid = Selection::SelectedKind() == Selection::Kind::GameObject ? Selection::SelectedGuid() : 0;
    guid = GuidArgument(L, 1, guid);

    if (!guid) {
        guid = CGGameUI::m_lockedTarget.m_guid;
    }

    if (!guid) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Request(guid);
    lua_pushboolean(L, 1);
    return 1;
}

static uint32_t EntryArgument(lua_State* L, int index) {
    if (!lua_isnumber(L, index)) {
        return 0;
    }

    double value = lua_tonumber(L, index);
    return value <= 0.0 || value >= 4294967295.0 ? 0 : static_cast<uint32_t>(value);
}

// GobEditor_RequestTemplate(entry) asks for the template snapshot of the entry, no gameobject needed.
static int32_t Script_RequestTemplate(lua_State* L) {
    uint32_t entry = EntryArgument(L, 1);

    if (!entry) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Net::Packet(Net::CMSG_GOB_EDITOR_REQUEST).PutUInt64(0).PutUInt32(entry).Send();

    lua_pushboolean(L, 1);
    return 1;
}

// GobEditor_SetTemplateMode(entry or 0), while on GetData returns the template and edits go to it.
static int32_t Script_SetTemplateMode(lua_State* L) {
    s_templateEntry = EntryArgument(L, 1);
    return 0;
}

static int32_t Script_GetTemplateMode(lua_State* L) {
    if (!s_templateEntry) {
        return 0;
    }

    lua_pushnumber(L, s_templateEntry);
    return 1;
}

// GobEditor_GetData(["template" | "selection"]), without an argument it follows the template mode.
static int32_t Script_GetData(lua_State* L) {
    const char* which = lua_type(L, 1) == LUA_TSTRING ? lua_tostring(L, 1) : nullptr;
    bool useTemplate = which ? std::strcmp(which, "template") == 0 : s_templateEntry != 0;
    const Snapshot& data = useTemplate ? s_template : s_data;

    if (!data.m_valid || (useTemplate && s_templateEntry && data.m_entry != s_templateEntry)) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 18);
    int root = lua_gettop(L);

    lua_pushstring(L, GuidToString(data.m_guid).c_str());
    lua_setfield(L, root, "guid");
    SetNumber(L, root, "entry", data.m_entry);
    SetNumber(L, root, "spawnId", data.m_spawnId);
    lua_pushstring(L, data.m_name.c_str());
    lua_setfield(L, root, "name");
    SetNumber(L, root, "x", data.m_x);
    SetNumber(L, root, "y", data.m_y);
    SetNumber(L, root, "z", data.m_z);
    SetNumber(L, root, "o", data.m_o);
    SetNumber(L, root, "state", data.m_state);
    SetNumber(L, root, "type", data.m_type);
    SetNumber(L, root, "displayId", data.m_displayId);

    lua_createtable(L, 0, 4);
    int rotation = lua_gettop(L);
    SetNumber(L, rotation, "x", data.m_rotation[0]);
    SetNumber(L, rotation, "y", data.m_rotation[1]);
    SetNumber(L, rotation, "z", data.m_rotation[2]);
    SetNumber(L, rotation, "w", data.m_rotation[3]);
    lua_setfield(L, root, "rotation");

    static const char* const s_names[SCOPE_COUNT] = { "template", "spawn", "templateAddon", "spawnAddon" };

    for (uint8_t scope = 0; scope < SCOPE_COUNT; scope++) {
        if (scope == SCOPE_SPAWN && !data.m_spawnId) {
            continue;
        }

        if (!data.m_has[scope]) {
            continue;
        }

        PushFields(L, data.m_fields[scope]);
        lua_setfield(L, root, s_names[scope]);
    }

    lua_settop(L, root);
    return 1;
}

// Snapshot that edits target, the template while template mode is on.
static const Snapshot* EditTarget() {
    if (s_templateEntry) {
        return s_template.m_valid && s_template.m_entry == s_templateEntry ? &s_template : nullptr;
    }

    return s_data.m_valid ? &s_data : nullptr;
}

static uint8_t FieldTypeFor(const Snapshot& target, uint8_t scope, const std::string& key) {
    if (scope >= SCOPE_COUNT) {
        return 0xFF;
    }

    for (const Field& field : target.m_fields[scope]) {
        if (field.m_key == key) {
            return field.m_type;
        }
    }

    return 0xFF;
}

// GobEditor_SetFields(scope, { key = value }), scopes are template, spawn, template addon and spawn addon.
static int32_t Script_SetFields(lua_State* L) {
    const Snapshot* target = EditTarget();

    if (!target || lua_type(L, 2) != LUA_TTABLE) {
        lua_pushboolean(L, 0);
        return 1;
    }

    uint8_t scope = static_cast<uint8_t>(lua_tonumber(L, 1));

    if (scope >= SCOPE_COUNT) {
        lua_pushboolean(L, 0);
        return 1;
    }

    std::vector<Field> pending;
    lua_pushnil(L);

    while (lua_next(L, 2)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            int valueType = lua_type(L, -1);
            Field field;
            field.m_key = lua_tostring(L, -2);
            field.m_type = FieldTypeFor(*target, scope, field.m_key);

            if (valueType == LUA_TNUMBER) {
                field.m_number = lua_tonumber(L, -1);
            } else if (valueType == LUA_TBOOLEAN) {
                field.m_number = lua_toboolean(L, -1) ? 1.0 : 0.0;
            } else if (valueType == LUA_TSTRING) {
                if (const char* text = lua_tostring(L, -1)) {
                    field.m_string = text;
                    field.m_number = std::atof(text);
                }
            }

            if (field.m_type == 0xFF) {
                if (valueType == LUA_TSTRING) {
                    field.m_type = FIELD_STRING;
                } else if (field.m_number != std::floor(field.m_number)) {
                    field.m_type = FIELD_FLOAT;
                } else {
                    field.m_type = field.m_number < 0.0 ? FIELD_INT : FIELD_UINT;
                }
            }

            pending.push_back(std::move(field));
        }

        lua_pop(L, 1);
    }

    if (pending.empty()) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Net::Packet packet(Net::CMSG_GOB_EDITOR_SET_FIELDS);
    packet.PutUInt64(target->m_guid);

    // A zero guid tells the server that the template entry follows.
    if (!target->m_guid) {
        packet.PutUInt32(target->m_entry);
    }

    packet.PutUInt8(scope);
    packet.PutUInt16(static_cast<uint16_t>(pending.size()));

    for (const Field& field : pending) {
        packet.PutString(field.m_key.c_str());
        packet.PutUInt8(field.m_type);

        switch (field.m_type) {
            case FIELD_UINT:
                packet.PutUInt32(field.m_number <= 0.0 ? 0u : static_cast<uint32_t>(field.m_number));
                break;
            case FIELD_INT:
                packet.PutInt32(static_cast<int32_t>(field.m_number));
                break;
            case FIELD_FLOAT:
                packet.PutFloat(static_cast<float>(field.m_number));
                break;
            case FIELD_UINT64:
                packet.PutUInt64(field.m_string.empty() ? (field.m_number <= 0.0 ? 0ull : static_cast<uint64_t>(field.m_number)) : std::strtoull(field.m_string.c_str(), nullptr, 0));
                break;
            case FIELD_INT64:
                packet.PutInt64(field.m_string.empty() ? static_cast<int64_t>(field.m_number) : std::strtoll(field.m_string.c_str(), nullptr, 0));
                break;
            default:
                packet.PutString(field.m_string.c_str());
                break;
        }
    }

    packet.Send();

    lua_pushboolean(L, 1);
    return 1;
}

// GobEditor_Commit([guid string]) sends the live client position and rotation.
static int32_t Script_Commit(lua_State* L) {
    uint64_t guid = GuidArgument(L, 1, Selection::SelectedKind() == Selection::Kind::GameObject ? Selection::SelectedGuid() : 0);

    if (!GameObjectByGuid(guid)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!Selection::SaveMove(guid)) {
        Commit(guid);
    }

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

void Request(uint64_t guid) {
    if (guid && Session::HasClearance(kRequiredSecurityLevel)) {
        Net::Packet(Net::CMSG_GOB_EDITOR_REQUEST).PutUInt64(guid).Send();
    }
}

void Forget() {
    s_data = Snapshot();
}

void Commit(uint64_t guid) {
    CGGameObject_C* object = GameObjectByGuid(guid);

    if (!object || !Session::HasClearance(kRequiredSecurityLevel)) {
        return;
    }

    CommitTransform(guid, object->m_passenger.m_position, UnpackQuat(PackedRotation(object)));
}

void CommitTransform(uint64_t guid, const C3Vector& position, const Quat& q) {
    if (!guid || !Session::HasClearance(kRequiredSecurityLevel)) {
        return;
    }

    Net::Packet(Net::CMSG_GOB_EDITOR_UPDATE)
        .PutInt64(static_cast<int64_t>(guid))
        .PutFloat(position.x)
        .PutFloat(position.y)
        .PutFloat(position.z)
        .PutFloat(q.x)
        .PutFloat(q.y)
        .PutFloat(q.z)
        .PutFloat(q.w)
        .Send();
}

bool SelectGameObject(uint64_t guid) {
    return GameObjectByGuid(guid) && Selection::Select(guid);
}

void ClearSelection() {
    if (Selection::SelectedKind() == Selection::Kind::GameObject) {
        Selection::Clear();
    }
}

CGGameObject_C* SelectedGameObject() {
    return Selection::SelectedKind() == Selection::Kind::GameObject ? GameObjectByGuid(Selection::SelectedGuid()) : nullptr;
}

uint64_t CurrentSelectedGobGUID() {
    return Selection::SelectedKind() == Selection::Kind::GameObject ? Selection::SelectedGuid() : 0;
}

void Apply() {
    Net::RegisterWorldHandler(Net::SMSG_GOB_EDITOR_SELECT, Packet_SMSG_GOB_EDITOR_SELECT);
    Net::RegisterWorldHandler(Net::SMSG_GOB_EDITOR_DATA, Packet_SMSG_GOB_EDITOR_DATA);
    Net::RegisterWorldHandler(Net::SMSG_GOB_EDITOR_RESULT, Packet_SMSG_GOB_EDITOR_RESULT);

    Lua::RegisterFunction("GobEditor_Request", Gated<Script_Request>);
    Lua::RegisterFunction("GobEditor_RequestTemplate", Gated<Script_RequestTemplate>);
    Lua::RegisterFunction("GobEditor_SetTemplateMode", Gated<Script_SetTemplateMode>);
    Lua::RegisterFunction("GobEditor_GetTemplateMode", Gated<Script_GetTemplateMode>);
    Lua::RegisterFunction("GobEditor_GetData", Gated<Script_GetData>);
    Lua::RegisterFunction("GobEditor_SetFields", Gated<Script_SetFields>);
    Lua::RegisterFunction("GobEditor_Commit", Gated<Script_Commit>);
}

void OnGameDestroy() {
    s_data = Snapshot();
    s_template = Snapshot();
    s_templateEntry = 0;
}

}
