#include "HoT/Editor/CreatureEditor.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "HoT/Editor/GizmoDraw.h"
#include "HoT/Editor/GobEditor.h"
#include "HoT/Editor/Selection.h"
#include "HoT/Editor/VectorMath.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Editor::CreatureEditor {

static constexpr int8_t kRequiredSecurityLevel = 3;

enum FieldType : uint8_t {
    FIELD_UINT = 0,
    FIELD_INT = 1,
    FIELD_FLOAT = 2,
    FIELD_STRING = 3,
    FIELD_UINT64 = 4,
    FIELD_INT64 = 5,
};

enum WaypointOp : uint8_t {
    WP_CREATE_PATH = 0,
    WP_ADD_POINT = 1,
    WP_MOVE_POINT = 2,
    WP_DELETE_POINT = 3,
    WP_SET_POINT = 4,
    WP_ASSIGN = 5,
    WP_UNASSIGN = 6,
    WP_PREVIEW = 7,
    WP_STOP = 8,
};

struct Field {
    std::string m_key;
    uint8_t m_type = FIELD_UINT;
    uint32_t m_uint = 0;
    int32_t m_int = 0;
    float m_float = 0.0f;
    std::string m_string;
};

struct Addon {
    bool m_exists = false;
    uint32_t m_pathId = 0;
    uint32_t m_mount = 0;
    uint8_t m_standState = 0;
    uint8_t m_animTier = 0;
    uint8_t m_visFlags = 0;
    uint8_t m_sheathState = 0;
    uint8_t m_pvpFlags = 0;
    uint32_t m_emote = 0;
    uint8_t m_visibilityDistanceType = 0;
    std::vector<uint32_t> m_auras;
};

struct EquipmentSet {
    uint8_t m_id = 0;
    uint32_t m_items[3] = { 0, 0, 0 };
};

struct LiveAura {
    uint32_t m_spellId = 0;
    uint8_t m_flags = 0;
};

struct Node {
    uint32_t m_point = 0;
    C3Vector m_position{};
    bool m_hasOrientation = false;
    float m_orientation = 0.0f;
    uint32_t m_delay = 0;
    uint8_t m_moveType = 0;
};

struct Snapshot {
    bool m_valid = false;
    uint64_t m_guid = 0;
    uint32_t m_entry = 0;
    uint32_t m_spawnId = 0;
    std::string m_name;
    C3Vector m_position{};
    float m_orientation = 0.0f;
    uint8_t m_defaultMovement = 0;
    uint32_t m_assignedPath = 0;
    uint32_t m_level = 0;
    std::vector<Field> m_templateFields;
    std::vector<Field> m_spawnFields;
    uint8_t m_equipmentId = 0;
    std::vector<EquipmentSet> m_equipment;
    Addon m_templateAddon;
    Addon m_spawnAddon;
    uint8_t m_liveStandState = 0;
    uint8_t m_liveAnimTier = 0;
    uint8_t m_liveVisFlags = 0;
    uint8_t m_liveSheathState = 0;
    uint8_t m_livePvpFlags = 0;
    uint32_t m_liveEmote = 0;
    uint32_t m_liveMount = 0;
    std::vector<LiveAura> m_liveAuras;
    uint32_t m_pathId = 0;
    std::vector<Node> m_nodes;
};

static Snapshot s_data;
static Snapshot s_template;
static uint32_t s_templateEntry;
static bool s_drawPath;
static uint32_t s_nodeDelay;
static uint8_t s_nodeMoveType;
static bool s_pathForced;
static uint32_t s_selectedPoint;

static bool TableHas(lua_State* L, int tableIdx, const char* key) {
    lua_getfield(L, tableIdx, key);
    bool present = lua_type(L, -1) != LUA_TNIL;
    lua_pop(L, 1);
    return present;
}

static double TableNumber(lua_State* L, int tableIdx, const char* key, double fallback = 0.0) {
    lua_getfield(L, tableIdx, key);
    double value = fallback;
    int type = lua_type(L, -1);

    if (type == LUA_TNUMBER) {
        value = lua_tonumber(L, -1);
    } else if (type == LUA_TBOOLEAN) {
        value = lua_toboolean(L, -1) ? 1.0 : 0.0;
    } else if (type == LUA_TSTRING) {
        if (const char* text = lua_tostring(L, -1)) {
            value = std::atof(text);
        }
    }

    lua_pop(L, 1);
    return value;
}

static std::vector<uint32_t> TableNumberArray(lua_State* L, int tableIdx, const char* key) {
    std::vector<uint32_t> values;
    lua_getfield(L, tableIdx, key);

    if (lua_type(L, -1) == LUA_TTABLE) {
        int arrayIdx = lua_gettop(L);
        int count = static_cast<int>(lua_objlen(L, arrayIdx));

        for (int i = 1; i <= count; i++) {
            lua_rawgeti(L, arrayIdx, i);

            if (lua_type(L, -1) == LUA_TNUMBER) {
                values.push_back(static_cast<uint32_t>(lua_tonumber(L, -1)));
            }

            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);
    return values;
}

static uint32_t ToUInt32(double value) {
    if (value <= 0.0) {
        return 0;
    }

    if (value >= 4294967295.0) {
        return 0xFFFFFFFF;
    }

    return static_cast<uint32_t>(value);
}

static void SetNumberField(lua_State* L, int tableIdx, const char* key, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, tableIdx, key);
}

static void SetStringField(lua_State* L, int tableIdx, const char* key, const char* value) {
    lua_pushstring(L, value);
    lua_setfield(L, tableIdx, key);
}

static void SetBoolField(lua_State* L, int tableIdx, const char* key, bool value) {
    lua_pushboolean(L, value ? 1 : 0);
    lua_setfield(L, tableIdx, key);
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
                field.m_uint = packet.GetUInt32();
                break;
            case FIELD_INT:
                field.m_int = packet.GetInt32();
                break;
            case FIELD_FLOAT:
                field.m_float = packet.GetFloat();
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

static void ReadAddon(Net::Packet& packet, Addon& addon) {
    addon = Addon();
    addon.m_exists = packet.GetUInt8() != 0;

    if (!addon.m_exists) {
        return;
    }

    addon.m_pathId = packet.GetUInt32();
    addon.m_mount = packet.GetUInt32();
    addon.m_standState = packet.GetUInt8();
    addon.m_animTier = packet.GetUInt8();
    addon.m_visFlags = packet.GetUInt8();
    addon.m_sheathState = packet.GetUInt8();
    addon.m_pvpFlags = packet.GetUInt8();
    addon.m_emote = packet.GetUInt32();
    addon.m_visibilityDistanceType = packet.GetUInt8();

    uint8_t auraCount = packet.GetUInt8();

    for (uint8_t i = 0; i < auraCount && packet.CanRead(4); i++) {
        addon.m_auras.push_back(packet.GetUInt32());
    }
}

static int32_t Packet_SMSG_CREATURE_EDITOR_DATA(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    char buffer[512];
    Snapshot data;

    if (packet.GetUInt8() != 1) {
        return 1;
    }

    data.m_guid = packet.GetUInt64();
    data.m_entry = packet.GetUInt32();
    data.m_spawnId = packet.GetUInt32();
    packet.GetString(buffer, sizeof(buffer));
    data.m_name = buffer;
    data.m_position.x = packet.GetFloat();
    data.m_position.y = packet.GetFloat();
    data.m_position.z = packet.GetFloat();
    data.m_orientation = packet.GetFloat();
    data.m_defaultMovement = packet.GetUInt8();
    data.m_assignedPath = packet.GetUInt32();
    data.m_level = packet.GetUInt32();

    ReadFields(packet, data.m_templateFields);
    ReadFields(packet, data.m_spawnFields);

    data.m_equipmentId = packet.GetUInt8();
    uint8_t setCount = packet.GetUInt8();

    for (uint8_t i = 0; i < setCount && packet.CanRead(1); i++) {
        EquipmentSet set;
        set.m_id = packet.GetUInt8();

        for (uint32_t& item : set.m_items) {
            item = packet.GetUInt32();
        }

        data.m_equipment.push_back(set);
    }

    ReadAddon(packet, data.m_templateAddon);
    ReadAddon(packet, data.m_spawnAddon);

    data.m_liveStandState = packet.GetUInt8();
    data.m_liveAnimTier = packet.GetUInt8();
    data.m_liveVisFlags = packet.GetUInt8();
    data.m_liveSheathState = packet.GetUInt8();
    data.m_livePvpFlags = packet.GetUInt8();
    data.m_liveEmote = packet.GetUInt32();
    data.m_liveMount = packet.GetUInt32();

    uint16_t auraCount = packet.GetUInt16();

    for (uint16_t i = 0; i < auraCount && packet.CanRead(5); i++) {
        LiveAura aura;
        aura.m_spellId = packet.GetUInt32();
        aura.m_flags = packet.GetUInt8();
        data.m_liveAuras.push_back(aura);
    }

    data.m_pathId = packet.GetUInt32();
    uint16_t nodeCount = packet.GetUInt16();

    for (uint16_t i = 0; i < nodeCount && packet.CanRead(1); i++) {
        Node node;
        node.m_point = packet.GetUInt32();
        node.m_position.x = packet.GetFloat();
        node.m_position.y = packet.GetFloat();
        node.m_position.z = packet.GetFloat();
        node.m_hasOrientation = packet.GetUInt8() != 0;
        node.m_orientation = packet.GetFloat();
        node.m_delay = packet.GetUInt32();
        node.m_moveType = packet.GetUInt8();
        data.m_nodes.push_back(node);
    }

    data.m_valid = true;

    // A zero guid is a template snapshot, it never touches the selection.
    if (!data.m_guid) {
        s_template = std::move(data);
        Lua::SignalEvent("HOT_CREATURE_EDITOR_DATA", "%d", 1);
        return 1;
    }

    uint64_t previous = s_data.m_valid ? s_data.m_guid : 0;
    bool sameSpawn = previous && data.m_spawnId && data.m_spawnId == s_data.m_spawnId;
    s_data = std::move(data);

    if (sameSpawn && previous != s_data.m_guid) {
        Selection::Replace(previous, s_data.m_guid);
    }

    Selection::OnCreatureSnapshot(s_data.m_guid);

    Lua::SignalEvent("HOT_CREATURE_EDITOR_DATA", "%d", 0);
    return 1;
}

static int32_t Packet_SMSG_CREATURE_EDITOR_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    char buffer[1024];
    int32_t ok = 0;

    {
        Net::Packet packet(msg);
        ok = packet.GetUInt8();
        packet.GetString(buffer, sizeof(buffer));
    }

    Lua::SignalEvent("HOT_CREATURE_EDITOR_RESULT", "%d%s", ok, buffer);
    return 1;
}

// Snapshot that edits target, the template while template mode is on.
static const Snapshot* EditTarget() {
    if (s_templateEntry) {
        return s_template.m_valid && s_template.m_entry == s_templateEntry ? &s_template : nullptr;
    }

    return s_data.m_valid ? &s_data : nullptr;
}

// A zero guid tells the server that the template entry follows.
static void PutTarget(Net::Packet& packet, const Snapshot& target) {
    packet.PutUInt64(target.m_guid);

    if (!target.m_guid) {
        packet.PutUInt32(target.m_entry);
    }
}

static uint8_t FieldTypeFor(const Snapshot& target, uint8_t scope, const std::string& key) {
    const std::vector<Field>& fields = scope == 1 ? target.m_spawnFields : target.m_templateFields;

    for (const Field& field : fields) {
        if (field.m_key == key) {
            return field.m_type;
        }
    }

    return 0xFF;
}

static void PushFields(lua_State* L, const std::vector<Field>& fields) {
    lua_createtable(L, 0, static_cast<int>(fields.size()));
    int tableIdx = lua_gettop(L);

    for (const Field& field : fields) {
        switch (field.m_type) {
            case FIELD_UINT:
                lua_pushnumber(L, field.m_uint);
                break;
            case FIELD_INT:
                lua_pushnumber(L, field.m_int);
                break;
            case FIELD_FLOAT:
                lua_pushnumber(L, field.m_float);
                break;
            default:
                lua_pushstring(L, field.m_string.c_str());
                break;
        }

        lua_setfield(L, tableIdx, field.m_key.c_str());
    }
}

static void PushAddon(lua_State* L, const Addon& addon) {
    if (!addon.m_exists) {
        lua_pushnil(L);
        return;
    }

    lua_createtable(L, 0, 10);
    int tableIdx = lua_gettop(L);
    SetNumberField(L, tableIdx, "path_id", addon.m_pathId);
    SetNumberField(L, tableIdx, "mount", addon.m_mount);
    SetNumberField(L, tableIdx, "standState", addon.m_standState);
    SetNumberField(L, tableIdx, "animTier", addon.m_animTier);
    SetNumberField(L, tableIdx, "visFlags", addon.m_visFlags);
    SetNumberField(L, tableIdx, "sheathState", addon.m_sheathState);
    SetNumberField(L, tableIdx, "pvpFlags", addon.m_pvpFlags);
    SetNumberField(L, tableIdx, "emote", addon.m_emote);
    SetNumberField(L, tableIdx, "visibilityDistanceType", addon.m_visibilityDistanceType);

    lua_createtable(L, static_cast<int>(addon.m_auras.size()), 0);
    int aurasIdx = lua_gettop(L);

    for (size_t i = 0; i < addon.m_auras.size(); i++) {
        lua_pushnumber(L, addon.m_auras[i]);
        lua_rawseti(L, aurasIdx, static_cast<int>(i + 1));
    }

    lua_setfield(L, tableIdx, "auras");
}

// CreatureEditor_Request([guid string], [pathId]) asks the server for a snapshot of the guid or the current target.
static int32_t Script_Request(lua_State* L) {
    uint64_t guid = 0;

    if (lua_type(L, 1) == LUA_TSTRING) {
        if (const char* text = lua_tostring(L, 1)) {
            guid = std::strtoull(text, nullptr, 10);
        }
    } else {
        guid = CGGameUI::m_lockedTarget.m_guid;
    }

    if (!guid) {
        lua_pushboolean(L, 0);
        return 1;
    }

    uint32_t pathId = lua_isnumber(L, 2) ? ToUInt32(lua_tonumber(L, 2)) : 0;
    Net::Packet(Net::CMSG_GAME_EDITOR_CREATURE_REQUEST).PutUInt64(guid).PutUInt32(pathId).Send();

    lua_pushboolean(L, 1);
    return 1;
}

// CreatureEditor_RequestTemplate(entry) asks for the template snapshot of the entry, no creature needed.
static int32_t Script_RequestTemplate(lua_State* L) {
    uint32_t entry = lua_isnumber(L, 1) ? ToUInt32(lua_tonumber(L, 1)) : 0;

    if (!entry) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Net::Packet(Net::CMSG_GAME_EDITOR_CREATURE_REQUEST).PutUInt64(0).PutUInt32(entry).PutUInt32(0).Send();

    lua_pushboolean(L, 1);
    return 1;
}

// CreatureEditor_SetTemplateMode(entry or 0), while on GetData returns the template and edits go to it.
static int32_t Script_SetTemplateMode(lua_State* L) {
    s_templateEntry = lua_isnumber(L, 1) ? ToUInt32(lua_tonumber(L, 1)) : 0;
    return 0;
}

static int32_t Script_GetTemplateMode(lua_State* L) {
    if (!s_templateEntry) {
        return 0;
    }

    lua_pushnumber(L, s_templateEntry);
    return 1;
}

// CreatureEditor_GetData(["template" | "selection"]), without an argument it follows the template mode.
static int32_t Script_GetData(lua_State* L) {
    const char* which = lua_type(L, 1) == LUA_TSTRING ? lua_tostring(L, 1) : nullptr;
    bool useTemplate = which ? std::strcmp(which, "template") == 0 : s_templateEntry != 0;
    const Snapshot& data = useTemplate ? s_template : s_data;

    if (!data.m_valid || (useTemplate && s_templateEntry && data.m_entry != s_templateEntry)) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 24);
    int root = lua_gettop(L);

    SetStringField(L, root, "guid", GobEditor::GuidToString(data.m_guid).c_str());
    SetNumberField(L, root, "entry", data.m_entry);
    SetNumberField(L, root, "spawnId", data.m_spawnId);
    SetStringField(L, root, "name", data.m_name.c_str());
    SetNumberField(L, root, "x", data.m_position.x);
    SetNumberField(L, root, "y", data.m_position.y);
    SetNumberField(L, root, "z", data.m_position.z);
    SetNumberField(L, root, "o", data.m_orientation);
    SetNumberField(L, root, "defaultMovement", data.m_defaultMovement);
    SetNumberField(L, root, "assignedPath", data.m_assignedPath);
    SetNumberField(L, root, "level", data.m_level);

    PushFields(L, data.m_templateFields);
    lua_setfield(L, root, "template");

    if (data.m_spawnId) {
        PushFields(L, data.m_spawnFields);
        lua_setfield(L, root, "spawn");
    }

    SetNumberField(L, root, "equipmentId", data.m_equipmentId);
    lua_createtable(L, static_cast<int>(data.m_equipment.size()), 0);
    int equipIdx = lua_gettop(L);

    for (size_t i = 0; i < data.m_equipment.size(); i++) {
        const EquipmentSet& set = data.m_equipment[i];
        lua_createtable(L, 0, 2);
        int setIdx = lua_gettop(L);
        SetNumberField(L, setIdx, "id", set.m_id);

        lua_createtable(L, 3, 0);
        int itemsIdx = lua_gettop(L);

        for (int j = 0; j < 3; j++) {
            lua_pushnumber(L, set.m_items[j]);
            lua_rawseti(L, itemsIdx, j + 1);
        }

        lua_setfield(L, setIdx, "items");
        lua_rawseti(L, equipIdx, static_cast<int>(i + 1));
    }

    lua_setfield(L, root, "equipment");

    PushAddon(L, data.m_templateAddon);
    lua_setfield(L, root, "templateAddon");
    PushAddon(L, data.m_spawnAddon);
    lua_setfield(L, root, "spawnAddon");

    lua_createtable(L, 0, 8);
    int liveIdx = lua_gettop(L);
    SetNumberField(L, liveIdx, "standState", data.m_liveStandState);
    SetNumberField(L, liveIdx, "animTier", data.m_liveAnimTier);
    SetNumberField(L, liveIdx, "visFlags", data.m_liveVisFlags);
    SetNumberField(L, liveIdx, "sheathState", data.m_liveSheathState);
    SetNumberField(L, liveIdx, "pvpFlags", data.m_livePvpFlags);
    SetNumberField(L, liveIdx, "emote", data.m_liveEmote);
    SetNumberField(L, liveIdx, "mount", data.m_liveMount);

    lua_createtable(L, static_cast<int>(data.m_liveAuras.size()), 0);
    int aurasIdx = lua_gettop(L);

    for (size_t i = 0; i < data.m_liveAuras.size(); i++) {
        const LiveAura& aura = data.m_liveAuras[i];
        lua_createtable(L, 0, 3);
        int auraIdx = lua_gettop(L);
        SetNumberField(L, auraIdx, "spell", aura.m_spellId);
        SetBoolField(L, auraIdx, "passive", (aura.m_flags & 0x1) != 0);
        SetBoolField(L, auraIdx, "selfCast", (aura.m_flags & 0x2) != 0);
        lua_rawseti(L, aurasIdx, static_cast<int>(i + 1));
    }

    lua_setfield(L, liveIdx, "auras");
    lua_setfield(L, root, "live");

    SetNumberField(L, root, "pathId", data.m_pathId);
    lua_createtable(L, static_cast<int>(data.m_nodes.size()), 0);
    int nodesIdx = lua_gettop(L);

    for (size_t i = 0; i < data.m_nodes.size(); i++) {
        const Node& node = data.m_nodes[i];
        lua_createtable(L, 0, 8);
        int nodeIdx = lua_gettop(L);
        SetNumberField(L, nodeIdx, "point", node.m_point);
        SetNumberField(L, nodeIdx, "x", node.m_position.x);
        SetNumberField(L, nodeIdx, "y", node.m_position.y);
        SetNumberField(L, nodeIdx, "z", node.m_position.z);

        if (node.m_hasOrientation) {
            SetNumberField(L, nodeIdx, "o", node.m_orientation);
        }

        SetNumberField(L, nodeIdx, "delay", node.m_delay);
        SetNumberField(L, nodeIdx, "moveType", node.m_moveType);
        lua_rawseti(L, nodesIdx, static_cast<int>(i + 1));
    }

    lua_setfield(L, root, "nodes");

    lua_settop(L, root);
    return 1;
}

// CreatureEditor_SetFields(scope, { key = value }), scope 0 is the template and 1 the spawn.
static int32_t Script_SetFields(lua_State* L) {
    const Snapshot* target = EditTarget();

    if (!target || lua_type(L, 2) != LUA_TTABLE) {
        lua_pushboolean(L, 0);
        return 1;
    }

    struct Pending {
        std::string m_key;
        uint8_t m_type;
        double m_number;
        std::string m_text;
    };

    uint8_t scope = static_cast<uint8_t>(lua_tonumber(L, 1));
    const int tableIdx = 2;
    std::vector<Pending> pending;

    lua_pushnil(L);

    while (lua_next(L, tableIdx)) {
        int valueType = lua_type(L, -1);

        if (lua_type(L, -2) == LUA_TSTRING) {
            Pending entry;
            entry.m_key = lua_tostring(L, -2);
            entry.m_type = FieldTypeFor(*target, scope, entry.m_key);
            entry.m_number = 0.0;

            if (valueType == LUA_TNUMBER) {
                entry.m_number = lua_tonumber(L, -1);
            } else if (valueType == LUA_TBOOLEAN) {
                entry.m_number = lua_toboolean(L, -1) ? 1.0 : 0.0;
            } else if (valueType == LUA_TSTRING) {
                if (const char* text = lua_tostring(L, -1)) {
                    entry.m_text = text;
                    entry.m_number = std::atof(text);
                }
            }

            if (entry.m_type == 0xFF) {
                if (valueType == LUA_TSTRING) {
                    entry.m_type = FIELD_STRING;
                } else if (entry.m_number != std::floor(entry.m_number)) {
                    entry.m_type = FIELD_FLOAT;
                } else {
                    entry.m_type = entry.m_number < 0.0 ? FIELD_INT : FIELD_UINT;
                }
            }

            pending.push_back(std::move(entry));
        }

        lua_pop(L, 1);
    }

    if (pending.empty()) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Net::Packet packet(Net::CMSG_GAME_EDITOR_CREATURE_SET_FIELDS);
    PutTarget(packet, *target);
    packet.PutUInt8(scope);
    packet.PutUInt16(static_cast<uint16_t>(pending.size()));

    for (const Pending& entry : pending) {
        packet.PutString(entry.m_key.c_str());
        packet.PutUInt8(entry.m_type);

        switch (entry.m_type) {
            case FIELD_UINT:
                packet.PutUInt32(ToUInt32(entry.m_number));
                break;
            case FIELD_INT:
                packet.PutInt32(static_cast<int32_t>(entry.m_number));
                break;
            case FIELD_FLOAT:
                packet.PutFloat(static_cast<float>(entry.m_number));
                break;
            case FIELD_UINT64:
                packet.PutUInt64(entry.m_text.empty() ? (entry.m_number <= 0.0 ? 0ull : static_cast<uint64_t>(entry.m_number)) : std::strtoull(entry.m_text.c_str(), nullptr, 0));
                break;
            case FIELD_INT64:
                packet.PutInt64(entry.m_text.empty() ? static_cast<int64_t>(entry.m_number) : std::strtoll(entry.m_text.c_str(), nullptr, 0));
                break;
            default:
                packet.PutString(entry.m_text.c_str());
                break;
        }
    }

    packet.Send();

    lua_pushboolean(L, 1);
    return 1;
}

// CreatureEditor_SetAddon(scope, mode, { path_id, mount, standState, ..., auras = { spellId } }).
static int32_t Script_SetAddon(lua_State* L) {
    const Snapshot* target = EditTarget();

    if (!target || lua_type(L, 3) != LUA_TTABLE) {
        lua_pushboolean(L, 0);
        return 1;
    }

    uint8_t scope = static_cast<uint8_t>(lua_tonumber(L, 1));
    uint8_t mode = static_cast<uint8_t>(lua_tonumber(L, 2));
    const int tableIdx = 3;
    std::vector<uint32_t> auras = TableNumberArray(L, tableIdx, "auras");

    Net::Packet packet(Net::CMSG_GAME_EDITOR_CREATURE_SET_ADDON);
    PutTarget(packet, *target);
    packet.PutUInt8(scope);
    packet.PutUInt8(mode);
    packet.PutUInt32(ToUInt32(TableNumber(L, tableIdx, "path_id")));
    packet.PutUInt32(ToUInt32(TableNumber(L, tableIdx, "mount")));
    packet.PutUInt8(static_cast<uint8_t>(TableNumber(L, tableIdx, "standState")));
    packet.PutUInt8(static_cast<uint8_t>(TableNumber(L, tableIdx, "animTier")));
    packet.PutUInt8(static_cast<uint8_t>(TableNumber(L, tableIdx, "visFlags")));
    packet.PutUInt8(static_cast<uint8_t>(TableNumber(L, tableIdx, "sheathState", 1)));
    packet.PutUInt8(static_cast<uint8_t>(TableNumber(L, tableIdx, "pvpFlags")));
    packet.PutUInt32(ToUInt32(TableNumber(L, tableIdx, "emote")));
    packet.PutUInt8(static_cast<uint8_t>(TableNumber(L, tableIdx, "visibilityDistanceType")));

    uint8_t auraCount = static_cast<uint8_t>(auras.size() > 255 ? 255 : auras.size());
    packet.PutUInt8(auraCount);

    for (uint8_t i = 0; i < auraCount; i++) {
        packet.PutUInt32(auras[i]);
    }

    packet.Send();

    lua_pushboolean(L, 1);
    return 1;
}

// CreatureEditor_Waypoint(op, [{ pathId, point, afterPoint, scope, x, y, z, o, delay, moveType }]).
static int32_t Script_Waypoint(lua_State* L) {
    if (!s_data.m_valid) {
        lua_pushboolean(L, 0);
        return 1;
    }

    uint8_t op = static_cast<uint8_t>(lua_tonumber(L, 1));
    bool hasArgs = lua_type(L, 2) == LUA_TTABLE;
    const int tableIdx = 2;

    auto number = [&](const char* key, double fallback = 0.0) {
        return hasArgs ? TableNumber(L, tableIdx, key, fallback) : fallback;
    };

    uint32_t pathId = ToUInt32(number("pathId", s_data.m_pathId));
    uint32_t point = ToUInt32(number("point"));
    uint8_t scope = static_cast<uint8_t>(number("scope"));
    bool hasOrientation = hasArgs && TableHas(L, tableIdx, "o");
    float orientation = static_cast<float>(number("o"));
    uint32_t delay = ToUInt32(number("delay"));
    uint8_t moveType = static_cast<uint8_t>(number("moveType"));

    auto putPosition = [&](Net::Packet& packet) {
        packet.PutFloat(static_cast<float>(number("x")));
        packet.PutFloat(static_cast<float>(number("y")));
        packet.PutFloat(static_cast<float>(number("z")));
        packet.PutUInt8(hasOrientation ? 1 : 0);
        packet.PutFloat(orientation);
    };

    Net::Packet packet(Net::CMSG_GAME_EDITOR_CREATURE_WAYPOINT);
    packet.PutUInt64(s_data.m_guid);
    packet.PutUInt8(op);

    switch (op) {
        case WP_CREATE_PATH:
            putPosition(packet);
            packet.PutUInt32(delay);
            packet.PutUInt8(moveType);
            break;
        case WP_ADD_POINT:
            packet.PutUInt32(pathId);
            packet.PutUInt32(ToUInt32(number("afterPoint")));
            putPosition(packet);
            packet.PutUInt32(delay);
            packet.PutUInt8(moveType);
            break;
        case WP_MOVE_POINT:
            packet.PutUInt32(pathId);
            packet.PutUInt32(point);
            putPosition(packet);
            break;
        case WP_DELETE_POINT:
            packet.PutUInt32(pathId);
            packet.PutUInt32(point);
            break;
        case WP_SET_POINT:
            packet.PutUInt32(pathId);
            packet.PutUInt32(point);
            packet.PutUInt32(delay);
            packet.PutUInt8(moveType);
            packet.PutUInt8(hasOrientation ? 1 : 0);
            packet.PutFloat(orientation);
            break;
        case WP_ASSIGN:
            packet.PutUInt32(pathId);
            packet.PutUInt8(scope);
            break;
        case WP_UNASSIGN:
            packet.PutUInt8(scope);
            break;
        case WP_PREVIEW:
            packet.PutUInt32(pathId);
            break;
        case WP_STOP:
            break;
        default:
            lua_pushboolean(L, 0);
            return 1;
    }

    packet.Send();

    lua_pushboolean(L, 1);
    return 1;
}

// CreatureEditor_Equipment(op, setId, [item1], [item2], [item3]).
static int32_t Script_Equipment(lua_State* L) {
    const Snapshot* target = EditTarget();

    if (!target) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Net::Packet packet(Net::CMSG_GAME_EDITOR_CREATURE_EQUIPMENT);
    PutTarget(packet, *target);
    packet.PutUInt8(static_cast<uint8_t>(lua_tonumber(L, 1)));
    packet.PutUInt8(static_cast<uint8_t>(lua_tonumber(L, 2)));

    for (int i = 3; i <= 5; i++) {
        packet.PutUInt32(lua_isnumber(L, i) ? ToUInt32(lua_tonumber(L, i)) : 0);
    }

    packet.Send();

    lua_pushboolean(L, 1);
    return 1;
}

// CreatureEditor_Move([guid string], x, y, z, o) moves the spawn, the guid defaults to the snapshot creature.
static int32_t Script_Move(lua_State* L) {
    int base = 1;
    uint64_t guid = s_data.m_valid ? s_data.m_guid : 0;

    if (lua_type(L, 1) == LUA_TSTRING) {
        if (const char* text = lua_tostring(L, 1)) {
            guid = std::strtoull(text, nullptr, 10);
        }

        base = 2;
    }

    if (!guid || !lua_isnumber(L, base) || !lua_isnumber(L, base + 1) || !lua_isnumber(L, base + 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    C3Vector position = {
        static_cast<float>(lua_tonumber(L, base)),
        static_cast<float>(lua_tonumber(L, base + 1)),
        static_cast<float>(lua_tonumber(L, base + 2)),
    };

    float orientation = lua_isnumber(L, base + 3) ? static_cast<float>(lua_tonumber(L, base + 3)) : s_data.m_orientation;
    Move(guid, position, orientation);

    lua_pushboolean(L, 1);
    return 1;
}

// CreatureEditor_SetNodeDefaults(delay, moveType) sets what nodes added in the world start with.
static int32_t Script_SetNodeDefaults(lua_State* L) {
    s_nodeDelay = lua_isnumber(L, 1) ? ToUInt32(lua_tonumber(L, 1)) : 0;
    s_nodeMoveType = lua_isnumber(L, 2) ? static_cast<uint8_t>(ToUInt32(lua_tonumber(L, 2))) : 0;
    return 0;
}

static int32_t Script_SetPathDrawing(lua_State* L) {
    s_drawPath = lua_toboolean(L, 1) != 0;
    s_selectedPoint = lua_isnumber(L, 2) ? ToUInt32(lua_tonumber(L, 2)) : 0;
    return 0;
}

static int32_t Script_GetPlayerPosition(lua_State* L) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return 0;
    }

    C3Vector position = player->GetPosition();
    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    lua_pushnumber(L, player->GetFacing());
    return 4;
}

// Returns the target's guid string, entry, high guid type and name.
static int32_t Script_GetTargetGuid(lua_State* L) {
    WOWGUID target = CGGameUI::m_lockedTarget;

    if (!target.m_guid) {
        return 0;
    }

    GobEditor::DecodedGuid decoded = GobEditor::DecodeGuid(target.m_guid);
    CGObject_C* unit = ClntObjMgrObjectPtr(target, TYPE_UNIT, __FILE__, __LINE__);
    const char* name = unit ? unit->GetObjectName() : nullptr;

    lua_pushstring(L, GobEditor::GuidToString(target.m_guid).c_str());
    lua_pushnumber(L, decoded.m_entry);
    lua_pushnumber(L, decoded.m_type);
    lua_pushstring(L, name ? name : "");
    return 4;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!Session::HasClearance(kRequiredSecurityLevel)) {
        return 0;
    }

    return Function(L);
}

void OnWorldRender(CGWorldFrame* frame) {
    if (!(s_drawPath || s_pathForced) || !s_data.m_valid || s_data.m_nodes.empty() || !frame || !frame->m_camera) {
        return;
    }

    if (!Session::HasClearance(kRequiredSecurityLevel)) {
        return;
    }

    CGxDevice* device = g_theGxDevicePtr;

    if (!device) {
        return;
    }

    device->RsPush();
    GxRsSet(GxRs_VertexShader, nullptr);
    GxRsSet(GxRs_PixelShader, nullptr);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_Lighting, 0);

    CImVector transparent;
    GxSceneClear(2, transparent);
    GxRsSet(GxRs_DepthTest, 1);

    const C3Vector& camera = frame->m_camera->m_position;
    CImVector lineColor(240, 240, 240, 220);
    CImVector nodeColor(255, 170, 40, 255);
    CImVector firstColor(90, 220, 60, 255);
    CImVector selectedColor(30, 210, 255, 255);
    C3Vector lift = { 0.0f, 0.0f, kNodeLift };

    C3Vector previous{};
    bool hasPrevious = false;

    for (size_t i = 0; i < s_data.m_nodes.size(); i++) {
        const Node& node = s_data.m_nodes[i];
        C3Vector drawPosition = VectorMath::Subtract(VectorMath::Add(node.m_position, lift), camera);

        if (hasPrevious) {
            GizmoDraw::DrawLine(previous, drawPosition, lineColor);
        }

        bool selected = s_selectedPoint && node.m_point == s_selectedPoint;
        CImVector color = selected ? selectedColor : (i == 0 ? firstColor : nodeColor);
        GizmoDraw::DrawSphere(drawPosition, selected ? 0.55f : 0.35f, color);

        if (node.m_hasOrientation) {
            C3Vector facing = drawPosition;
            facing.x += cosf(node.m_orientation) * 1.5f;
            facing.y += sinf(node.m_orientation) * 1.5f;
            GizmoDraw::DrawLine(drawPosition, facing, color);
        }

        previous = drawPosition;
        hasPrevious = true;
    }

    device->RsPop();
}

void OnGameDestroy() {
    s_data = Snapshot();
    s_template = Snapshot();
    s_templateEntry = 0;
    s_drawPath = false;
    s_pathForced = false;
    s_selectedPoint = 0;
}

void Request(uint64_t guid) {
    if (guid && Session::HasClearance(kRequiredSecurityLevel)) {
        Net::Packet(Net::CMSG_GAME_EDITOR_CREATURE_REQUEST).PutUInt64(guid).PutUInt32(0).Send();
    }
}

void Forget() {
    s_data = Snapshot();
    s_selectedPoint = 0;
}

void Move(uint64_t guid, const C3Vector& position, float orientation) {
    if (!guid || !Session::HasClearance(kRequiredSecurityLevel)) {
        return;
    }

    Net::Packet(Net::CMSG_GAME_EDITOR_CREATURE_MOVE)
        .PutUInt64(guid)
        .PutFloat(position.x)
        .PutFloat(position.y)
        .PutFloat(position.z)
        .PutFloat(orientation)
        .Send();
}

uint64_t SnapshotGuid() {
    return s_data.m_valid ? s_data.m_guid : 0;
}

uint32_t PathId() {
    return s_data.m_valid ? s_data.m_pathId : 0;
}

size_t NodeCount() {
    return s_data.m_valid ? s_data.m_nodes.size() : 0;
}

bool GetNode(size_t index, uint32_t& point, C3Vector& position) {
    if (!s_data.m_valid || index >= s_data.m_nodes.size()) {
        return false;
    }

    point = s_data.m_nodes[index].m_point;
    position = s_data.m_nodes[index].m_position;
    return true;
}

static Node* FindNode(uint32_t point) {
    for (Node& node : s_data.m_nodes) {
        if (node.m_point == point) {
            return &node;
        }
    }

    return nullptr;
}

void PreviewNode(uint32_t point, const C3Vector& position) {
    if (Node* node = FindNode(point)) {
        node->m_position = position;
    }
}

void MoveNode(uint32_t point, const C3Vector& position) {
    Node* node = FindNode(point);

    if (!node || !s_data.m_pathId || !Session::HasClearance(kRequiredSecurityLevel)) {
        return;
    }

    node->m_position = position;

    Net::Packet(Net::CMSG_GAME_EDITOR_CREATURE_WAYPOINT)
        .PutUInt64(s_data.m_guid)
        .PutUInt8(WP_MOVE_POINT)
        .PutUInt32(s_data.m_pathId)
        .PutUInt32(point)
        .PutFloat(position.x)
        .PutFloat(position.y)
        .PutFloat(position.z)
        .PutUInt8(node->m_hasOrientation ? 1 : 0)
        .PutFloat(node->m_orientation)
        .Send();
}

void AddNode(uint32_t afterPoint, const C3Vector& position) {
    if (!s_data.m_valid || !Session::HasClearance(kRequiredSecurityLevel)) {
        return;
    }

    Net::Packet packet(Net::CMSG_GAME_EDITOR_CREATURE_WAYPOINT);
    packet.PutUInt64(s_data.m_guid);

    if (!s_data.m_pathId) {
        packet.PutUInt8(WP_CREATE_PATH);
    } else {
        packet.PutUInt8(WP_ADD_POINT);
        packet.PutUInt32(s_data.m_pathId);
        packet.PutUInt32(afterPoint);
    }

    packet.PutFloat(position.x);
    packet.PutFloat(position.y);
    packet.PutFloat(position.z);
    packet.PutUInt8(0);
    packet.PutFloat(0.0f);
    packet.PutUInt32(s_nodeDelay);
    packet.PutUInt8(s_nodeMoveType);
    packet.Send();
}

void DeleteNode(uint32_t point) {
    if (!FindNode(point) || !s_data.m_pathId || !Session::HasClearance(kRequiredSecurityLevel)) {
        return;
    }

    Net::Packet(Net::CMSG_GAME_EDITOR_CREATURE_WAYPOINT)
        .PutUInt64(s_data.m_guid)
        .PutUInt8(WP_DELETE_POINT)
        .PutUInt32(s_data.m_pathId)
        .PutUInt32(point)
        .Send();
}

uint32_t SelectedPoint() {
    return s_selectedPoint;
}

void SetSelectedPoint(uint32_t point) {
    if (s_selectedPoint == point) {
        return;
    }

    s_selectedPoint = point;
    Lua::SignalEvent("HOT_CREATURE_EDITOR_POINT_SELECTED", "%d", static_cast<int32_t>(point));
}

void SetPathForced(bool forced) {
    s_pathForced = forced;
}

void Apply() {
    Net::RegisterWorldHandler(Net::SMSG_GAME_EDITOR_CREATURE_DATA, Packet_SMSG_CREATURE_EDITOR_DATA);
    Net::RegisterWorldHandler(Net::SMSG_GAME_EDITOR_CREATURE_RESULT, Packet_SMSG_CREATURE_EDITOR_RESULT);

    Lua::RegisterFunction("CreatureEditor_Request", Gated<Script_Request>);
    Lua::RegisterFunction("CreatureEditor_RequestTemplate", Gated<Script_RequestTemplate>);
    Lua::RegisterFunction("CreatureEditor_SetTemplateMode", Gated<Script_SetTemplateMode>);
    Lua::RegisterFunction("CreatureEditor_GetTemplateMode", Gated<Script_GetTemplateMode>);
    Lua::RegisterFunction("CreatureEditor_GetData", Gated<Script_GetData>);
    Lua::RegisterFunction("CreatureEditor_SetFields", Gated<Script_SetFields>);
    Lua::RegisterFunction("CreatureEditor_SetAddon", Gated<Script_SetAddon>);
    Lua::RegisterFunction("CreatureEditor_Waypoint", Gated<Script_Waypoint>);
    Lua::RegisterFunction("CreatureEditor_Equipment", Gated<Script_Equipment>);
    Lua::RegisterFunction("CreatureEditor_Move", Gated<Script_Move>);
    Lua::RegisterFunction("CreatureEditor_SetPathDrawing", Gated<Script_SetPathDrawing>);
    Lua::RegisterFunction("CreatureEditor_SetNodeDefaults", Gated<Script_SetNodeDefaults>);
    Lua::RegisterFunction("CreatureEditor_GetPlayerPosition", Gated<Script_GetPlayerPosition>);
    Lua::RegisterFunction("CreatureEditor_GetTargetGuid", Gated<Script_GetTargetGuid>);
}

}
