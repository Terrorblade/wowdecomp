#include "HoT/Editor/WorldPreview.h"
#include <cmath>
#include <cstring>
#include <map>
#include <vector>
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Os/TimeManager.h"
#include "HoT/Editor/Model/PreviewAttach.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/LoadingScreen.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/CMovementStatus.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Types.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace HoT::Editor::WorldPreview {

namespace PreviewAttach = ModelEditor::PreviewAttach;

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr uint32_t kMaxObjects = 32;
static constexpr uint64_t kHighUnit = 0xF130;
static constexpr uint64_t kHighVehicle = 0xF150;
static constexpr uint64_t kHighGameObject = 0xF110;
static constexpr uint32_t kFirstCounter = 0xF00000;
static constexpr uint32_t kCounterMask = 0xFFFFFF;
static constexpr float kDefaultDistance = 6.0f;
static constexpr uint32_t kAnimRecheckMs = 250;
static constexpr uint32_t kNoAnim = 0xFFFFFFFF;

static constexpr uint32_t UPDATE_CREATE_OBJECT = 2;
static constexpr uint16_t UPDATEFLAG_LIVING = 0x20;
static constexpr uint16_t UPDATEFLAG_STATIONARY = 0x40;
static constexpr uint16_t UPDATEFLAG_VEHICLE = 0x80;
static constexpr uint16_t UPDATEFLAG_ROTATION = 0x200;

static constexpr uint32_t UNIT_BYTES_0 = 23;
static constexpr uint32_t UNIT_HEALTH = 24;
static constexpr uint32_t UNIT_MAXHEALTH = 32;
static constexpr uint32_t UNIT_LEVEL = 54;
static constexpr uint32_t UNIT_FACTIONTEMPLATE = 55;
static constexpr uint32_t UNIT_FLAGS = 59;
static constexpr uint32_t UNIT_BOUNDINGRADIUS = 65;
static constexpr uint32_t UNIT_COMBATREACH = 66;
static constexpr uint32_t UNIT_DISPLAYID = 67;
static constexpr uint32_t UNIT_NATIVEDISPLAYID = 68;
static constexpr uint32_t UNIT_MOD_CAST_SPEED = 80;
static constexpr uint32_t UNIT_HOVERHEIGHT = 146;
static constexpr uint32_t UNIT_DWORDS = 148;
static constexpr uint32_t UNIT_FLAG_NOT_SELECTABLE = 0x02000000;

static constexpr uint32_t GO_DISPLAYID = 8;
static constexpr uint32_t GO_FLAGS = 9;
static constexpr uint32_t GO_PARENTROTATION = 10;
static constexpr uint32_t GO_FACTION = 15;
static constexpr uint32_t GO_LEVEL = 16;
static constexpr uint32_t GO_BYTES_1 = 17;
static constexpr uint32_t GO_DWORDS = 18;
static constexpr uint32_t GO_FLAG_NOT_SELECTABLE = 0x10;
static constexpr uint32_t GO_TYPE_GENERIC = 5;

static constexpr uint32_t kFriendlyFaction = 35;

enum class Kind {
    Unit,
    GameObject,
};

struct Object {
    Kind kind = Kind::Unit;
    uint32_t displayId = 0;
    uint32_t entry = 0;
    uint32_t vehicleId = 0;
    uint32_t parent = 0;
    uint8_t seat = 0;
    float scale = 1.0f;
    uint32_t anim = kNoAnim;
    uint64_t guid = 0;
    CM2Model* animModel = nullptr;
    uint32_t animCheckedAt = 0;
};

struct Anchor {
    bool valid = false;
    C3Vector position = { 0.0f, 0.0f, 0.0f };
    float facing = 0.0f;
};

static std::map<uint32_t, Object> s_objects;
static Anchor s_anchor;
static float s_distance = kDefaultDistance;
static uint32_t s_counter = kFirstCounter;
static bool s_respawnPending = false;

static uint32_t FloatBits(float value) {
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return raw;
}

static uint64_t NextGuid(uint64_t high, uint32_t entry) {
    s_counter = s_counter >= kCounterMask ? kFirstCounter : s_counter + 1;
    return (high << 48) | (static_cast<uint64_t>(entry & 0xFFFFFF) << 24) | s_counter;
}

static void PutGuid(CDataStore& msg, uint64_t guid) {
    SmartGUID packed{};
    packed.m_guid = guid;
    msg << packed;
}

static void PutValues(CDataStore& msg, uint32_t dwords, const std::map<uint32_t, uint32_t>& values) {
    uint8_t blocks = static_cast<uint8_t>((dwords + 31) / 32);
    std::vector<uint32_t> masks(blocks, 0);

    for (const auto& pair : values) {
        masks[pair.first >> 5] |= 1u << (pair.first & 31);
    }

    msg.Put(blocks);

    for (uint32_t mask : masks) {
        msg.Put(mask);
    }

    for (const auto& pair : values) {
        msg.Put(pair.second);
    }
}

static void PutObjectValues(std::map<uint32_t, uint32_t>& values, uint64_t guid, uint32_t type, uint32_t entry, float scale) {
    values[0] = static_cast<uint32_t>(guid);
    values[1] = static_cast<uint32_t>(guid >> 32);
    values[2] = type;
    values[3] = entry;
    values[4] = FloatBits(scale);
}

static void Dispatch(CDataStore& msg) {
    msg.Finalize();
    Packet_SMSG_UPDATE_OBJECT(nullptr, SMSG_UPDATE_OBJECT, OsGetAsyncTimeMs(), &msg);
}

static CGObject_C* Find(uint64_t guid) {
    return ClntObjMgrObjectPtr(WOWGUID{ guid }, TYPEMASK_OBJECT, __FILE__, __LINE__);
}

static CM2Model* ObjectModel(const Object& object) {
    CGObject_C* found = object.guid ? Find(object.guid) : nullptr;
    return found ? found->GetObjectModel() : nullptr;
}

static void SendUnit(const Object& object, const Object* vehicle) {
    CMovementStatus status;
    status.m_gameTime = OsGetAsyncTimeMs();
    status.m_position = s_anchor.position;
    status.m_facing = s_anchor.facing;
    status.m_fallTime = 0;

    if (vehicle) {
        status.m_moveFlags = MOVEMENTFLAG_ONTRANSPORT;
        status.m_transportGuid = WOWGUID{ vehicle->guid };
        status.m_transportPosition = { 0.0f, 0.0f, 0.0f };
        status.m_transportFacing = 0.0f;
        status.m_transportTime = 0;
        status.m_transportSeat = object.seat;
    }

    uint16_t flags = UPDATEFLAG_LIVING | (object.vehicleId ? UPDATEFLAG_VEHICLE : 0);

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(1));
    msg.Put(static_cast<uint8_t>(UPDATE_CREATE_OBJECT));
    PutGuid(msg, object.guid);
    msg.Put(static_cast<uint8_t>(ID_UNIT));
    msg.Put(flags);
    msg << status;

    static const float speeds[9] = { 2.5f, 7.0f, 4.5f, 4.722222f, 2.5f, 7.0f, 4.5f, 3.141594f, 3.141594f };

    for (float speed : speeds) {
        msg.Put(speed);
    }

    if (object.vehicleId) {
        msg.Put(object.vehicleId);
        msg.Put(s_anchor.facing);
    }

    std::map<uint32_t, uint32_t> values;
    PutObjectValues(values, object.guid, TYPEMASK_OBJECT | TYPEMASK_UNIT, object.entry, object.scale);
    values[UNIT_BYTES_0] = 0x00000100;
    values[UNIT_HEALTH] = 1;
    values[UNIT_MAXHEALTH] = 1;
    values[UNIT_LEVEL] = 1;
    values[UNIT_FACTIONTEMPLATE] = kFriendlyFaction;
    values[UNIT_FLAGS] = UNIT_FLAG_NOT_SELECTABLE;
    values[UNIT_BOUNDINGRADIUS] = FloatBits(0.389f);
    values[UNIT_COMBATREACH] = FloatBits(1.5f);
    values[UNIT_DISPLAYID] = object.displayId;
    values[UNIT_NATIVEDISPLAYID] = object.displayId;
    values[UNIT_MOD_CAST_SPEED] = FloatBits(1.0f);
    values[UNIT_HOVERHEIGHT] = FloatBits(1.0f);
    PutValues(msg, UNIT_DWORDS, values);
    Dispatch(msg);
}

static void SendGameObject(const Object& object) {
    float half = s_anchor.facing * 0.5f;
    float qz = sinf(half);
    float qw = cosf(half);

    if (qw < 0.0f) {
        qz = -qz;
        qw = -qw;
    }

    int64_t packedZ = static_cast<int64_t>(static_cast<int32_t>(qz * (1 << 20))) & ((1 << 21) - 1);

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(1));
    msg.Put(static_cast<uint8_t>(UPDATE_CREATE_OBJECT));
    PutGuid(msg, object.guid);
    msg.Put(static_cast<uint8_t>(ID_GAMEOBJECT));
    msg.Put(static_cast<uint16_t>(UPDATEFLAG_STATIONARY | UPDATEFLAG_ROTATION));
    msg.Put(s_anchor.position.x);
    msg.Put(s_anchor.position.y);
    msg.Put(s_anchor.position.z);
    msg.Put(s_anchor.facing);
    msg.Put(static_cast<uint64_t>(packedZ));

    std::map<uint32_t, uint32_t> values;
    PutObjectValues(values, object.guid, TYPEMASK_OBJECT | TYPEMASK_GAMEOBJECT, object.entry, object.scale);
    values[GO_DISPLAYID] = object.displayId;
    values[GO_FLAGS] = GO_FLAG_NOT_SELECTABLE;
    values[GO_PARENTROTATION + 0] = FloatBits(0.0f);
    values[GO_PARENTROTATION + 1] = FloatBits(0.0f);
    values[GO_PARENTROTATION + 2] = FloatBits(qz);
    values[GO_PARENTROTATION + 3] = FloatBits(qw);
    values[GO_FACTION] = kFriendlyFaction;
    values[GO_LEVEL] = 1;
    values[GO_BYTES_1] = 1 | (GO_TYPE_GENERIC << 8) | (255u << 24);
    PutValues(msg, GO_DWORDS, values);
    Dispatch(msg);
}

static void SendDestroy(uint64_t guid) {
    if (!guid || !Find(guid)) {
        return;
    }

    CDataStore msg;
    msg.Put(guid);
    msg.Put(static_cast<uint8_t>(0));
    msg.Finalize();
    Packet_SMSG_DESTROY_OBJECT(nullptr, SMSG_DESTROY_OBJECT, OsGetAsyncTimeMs(), &msg);
}

static bool HasPlayer() {
    return ClntObjMgrGetActivePlayerObj() && !LoadingScreenDrawing();
}

static void PlaceAnchor() {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return;
    }

    C3Vector position = player->GetPosition();
    float facing = player->GetFacing();
    s_anchor.position = { position.x + cosf(facing) * s_distance, position.y + sinf(facing) * s_distance, position.z };
    s_anchor.facing = facing + 3.14159265f;

    if (s_anchor.facing > 6.2831853f) {
        s_anchor.facing -= 6.2831853f;
    }

    s_anchor.valid = true;
}

// Riders go first so the vehicle never loses a passenger it still shows.
static void Despawn(uint32_t slot) {
    auto it = s_objects.find(slot);

    if (it == s_objects.end() || !it->second.guid) {
        return;
    }

    for (auto& pair : s_objects) {
        if (pair.second.parent == slot) {
            Despawn(pair.first);
        }
    }

    PreviewAttach::DetachWorld(slot);
    SendDestroy(it->second.guid);
    it->second.guid = 0;
    it->second.animModel = nullptr;
}

static void DespawnAll() {
    for (auto& pair : s_objects) {
        PreviewAttach::DetachWorld(pair.first);
    }

    for (auto& pair : s_objects) {
        if (pair.second.parent) {
            SendDestroy(pair.second.guid);
            pair.second.guid = 0;
        }
    }

    for (auto& pair : s_objects) {
        SendDestroy(pair.second.guid);
        pair.second.guid = 0;
        pair.second.animModel = nullptr;
    }
}

static void Spawn(uint32_t slot) {
    auto it = s_objects.find(slot);

    if (it == s_objects.end() || !HasPlayer()) {
        return;
    }

    Object& object = it->second;
    Despawn(slot);

    if (!s_anchor.valid) {
        PlaceAnchor();
    }

    const Object* vehicle = nullptr;

    if (object.parent) {
        auto parent = s_objects.find(object.parent);

        if (parent == s_objects.end() || !parent->second.guid || !Find(parent->second.guid)) {
            return;
        }

        vehicle = &parent->second;
    }

    if (object.kind == Kind::GameObject) {
        object.guid = NextGuid(kHighGameObject, object.entry);
        SendGameObject(object);
    } else {
        object.guid = NextGuid(object.vehicleId ? kHighVehicle : kHighUnit, object.entry);
        SendUnit(object, vehicle);
    }

    object.animModel = nullptr;
    object.animCheckedAt = 0;

    // Riders of this vehicle come back on it.
    for (auto& pair : s_objects) {
        if (pair.second.parent == slot && !pair.second.guid) {
            Spawn(pair.first);
        }
    }
}

static void Remove(uint32_t slot) {
    auto it = s_objects.find(slot);

    if (it == s_objects.end()) {
        return;
    }

    for (auto& pair : s_objects) {
        if (pair.second.parent == slot) {
            pair.second.parent = 0;
        }
    }

    Despawn(slot);
    PreviewAttach::FreeWorld(slot);
    s_objects.erase(it);
}

static void RemoveAll() {
    DespawnAll();

    for (auto& pair : s_objects) {
        PreviewAttach::FreeWorld(pair.first);
    }

    s_objects.clear();
    s_anchor.valid = false;
    s_respawnPending = false;
}

static bool ModelHasAnim(CM2Model* model, uint32_t anim) {
    M2Data* data = model->m_shared ? model->m_shared->m_data : nullptr;

    if (!data) {
        return false;
    }

    for (uint32_t i = 0; i < data->m_sequences.m_count; i++) {
        if (data->m_sequences[i].m_id == anim) {
            return true;
        }
    }

    return false;
}

// The unit picks its own idle and seat animations, the override plays over them whenever they come back.
static void UpdateAnim(Object& object, CM2Model* model, uint32_t now) {
    if (object.anim == kNoAnim || !model || !model->IsLoaded(0, 0)) {
        return;
    }

    if (model == object.animModel && now - object.animCheckedAt < kAnimRecheckMs) {
        return;
    }

    object.animCheckedAt = now;

    if (!ModelHasAnim(model, object.anim)) {
        object.animModel = model;
        return;
    }

    if (model != object.animModel || model->GetBoneSequenceAnimId(0xFFFFFFFF, nullptr) != object.anim) {
        model->SetBoneSequence(0xFFFFFFFF, object.anim, 0xFFFFFFFF, 0, 1.0f, 1, 1);
        object.animModel = model;
    }
}

static uint32_t SlotArgument(lua_State* L, int32_t index) {
    if (!lua_isnumber(L, index)) {
        return 0;
    }

    double value = lua_tonumber(L, index);
    return value >= 1 && value <= kMaxObjects ? static_cast<uint32_t>(value) : 0;
}

static double Number(lua_State* L, int32_t table, const char* key, double fallback) {
    lua_getfield(L, table, key);
    double value = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : fallback;
    lua_pop(L, 1);
    return value;
}

static uint32_t AnimOption(lua_State* L, int32_t table) {
    double anim = Number(L, table, "anim", -1.0);
    return anim >= 0.0 ? static_cast<uint32_t>(anim) : kNoAnim;
}

// WorldPreview_Spawn(slot, { kind = "unit" or "gameobject", displayId, entry, vehicleId, parent, seat, scale, anim }) -> ok, error.
static int32_t Script_Spawn(lua_State* L) {
    uint32_t slot = SlotArgument(L, 1);

    if (!slot || lua_type(L, 2) != LUA_TTABLE) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Bad arguments");
        return 2;
    }

    if (!HasPlayer()) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Not in the world");
        return 2;
    }

    lua_getfield(L, 2, "kind");
    const char* kindText = lua_tostring(L, -1);
    Kind kind = kindText && !strcmp(kindText, "gameobject") ? Kind::GameObject : Kind::Unit;
    lua_pop(L, 1);

    uint32_t displayId = static_cast<uint32_t>(Number(L, 2, "displayId", 0.0));
    bool known = kind == Kind::GameObject ? g_gameObjectDisplayInfoDB.GetRecord(displayId) != nullptr : g_creatureDisplayInfoDB.GetRecord(displayId) != nullptr;

    if (!displayId || !known) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Unknown display id");
        return 2;
    }

    uint32_t parent = static_cast<uint32_t>(Number(L, 2, "parent", 0.0));

    if (parent == slot || (parent && !s_objects.count(parent))) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Unknown vehicle slot");
        return 2;
    }

    Despawn(slot);
    Object& object = s_objects[slot];
    object.kind = kind;
    object.displayId = displayId;
    object.entry = static_cast<uint32_t>(Number(L, 2, "entry", 0.0));
    object.vehicleId = kind == Kind::Unit ? static_cast<uint32_t>(Number(L, 2, "vehicleId", 0.0)) : 0;
    object.parent = kind == Kind::Unit ? parent : 0;
    object.seat = static_cast<uint8_t>(Number(L, 2, "seat", 0.0));
    object.scale = static_cast<float>(Number(L, 2, "scale", 1.0));
    object.anim = AnimOption(L, 2);

    if (object.scale <= 0.0f) {
        object.scale = 1.0f;
    }

    Spawn(slot);
    lua_pushboolean(L, object.guid != 0);
    return 1;
}

// WorldPreview_SetAnim(slot, animId or nil) plays the animation over whatever the object picks itself.
static int32_t Script_SetAnim(lua_State* L) {
    auto it = s_objects.find(SlotArgument(L, 1));

    if (it != s_objects.end()) {
        it->second.anim = lua_isnumber(L, 2) && lua_tonumber(L, 2) >= 0 ? static_cast<uint32_t>(lua_tonumber(L, 2)) : kNoAnim;
        it->second.animModel = nullptr;
    }

    return 0;
}

// WorldPreview_SetAttach(slot, attachSlot, options or nil), the options of ModelEditor_SetPreviewAttach.
static int32_t Script_SetAttach(lua_State* L) {
    uint32_t slot = SlotArgument(L, 1);
    bool ok = slot && s_objects.count(slot) && lua_isnumber(L, 2) && PreviewAttach::SetWorldSlot(L, slot, static_cast<uint32_t>(lua_tonumber(L, 2)), 3);
    lua_pushboolean(L, ok);
    return 1;
}

// WorldPreview_Remove(slot) or WorldPreview_Remove() for every object.
static int32_t Script_Remove(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        RemoveAll();
    } else if (uint32_t slot = SlotArgument(L, 1)) {
        Remove(slot);
    }

    return 0;
}

// WorldPreview_MoveHere([distance]) puts every object in front of the player again.
static int32_t Script_MoveHere(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        s_distance = static_cast<float>(lua_tonumber(L, 1));
    }

    if (!HasPlayer()) {
        return 0;
    }

    DespawnAll();
    PlaceAnchor();

    for (auto& pair : s_objects) {
        if (!pair.second.parent) {
            Spawn(pair.first);
        }
    }

    return 0;
}

// WorldPreview_IsSpawned(slot) -> spawned, model loaded.
static int32_t Script_IsSpawned(lua_State* L) {
    auto it = s_objects.find(SlotArgument(L, 1));
    CM2Model* model = it != s_objects.end() ? ObjectModel(it->second) : nullptr;
    lua_pushboolean(L, it != s_objects.end() && it->second.guid && Find(it->second.guid));
    lua_pushboolean(L, model && model->IsLoaded(0, 0));
    return 2;
}

// WorldPreview_HumanModelData() -> the CreatureModelData id of the human male character model.
static int32_t Script_HumanModelData(lua_State* L) {
    CreatureModelDataRec* rec = GetModelDataRec(1, 0);

    if (!rec) {
        return 0;
    }

    lua_pushnumber(L, rec->m_id);
    return 1;
}

bool IsPreviewGuid(uint64_t guid) {
    if (!guid) {
        return false;
    }

    for (const auto& pair : s_objects) {
        if (pair.second.guid == guid) {
            return true;
        }
    }

    return false;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!Session::HasClearance(kRequiredSecurityLevel)) {
        return 0;
    }

    return Function(L);
}

void Apply() {
    Lua::RegisterFunction("WorldPreview_Spawn", Gated<Script_Spawn>);
    Lua::RegisterFunction("WorldPreview_SetAnim", Gated<Script_SetAnim>);
    Lua::RegisterFunction("WorldPreview_SetAttach", Gated<Script_SetAttach>);
    Lua::RegisterFunction("WorldPreview_Remove", Gated<Script_Remove>);
    Lua::RegisterFunction("WorldPreview_MoveHere", Gated<Script_MoveHere>);
    Lua::RegisterFunction("WorldPreview_IsSpawned", Gated<Script_IsSpawned>);
    Lua::RegisterFunction("WorldPreview_HumanModelData", Gated<Script_HumanModelData>);
}

void OnIdle() {
    if (s_objects.empty()) {
        return;
    }

    if (s_respawnPending) {
        if (!HasPlayer()) {
            return;
        }

        s_respawnPending = false;
        PlaceAnchor();

        for (auto& pair : s_objects) {
            if (!pair.second.parent) {
                Spawn(pair.first);
            }
        }
    }

    uint32_t now = OsGetAsyncTimeMs();

    for (auto& pair : s_objects) {
        CM2Model* model = ObjectModel(pair.second);
        PreviewAttach::UpdateWorld(pair.first, model);
        UpdateAnim(pair.second, model, now);
    }
}

// The objects come back in front of the player once the new map is in.
void OnLoadingScreenStarted() {
    if (s_objects.empty()) {
        return;
    }

    DespawnAll();
    s_anchor.valid = false;
    s_respawnPending = true;
}

void OnGameDestroy() {
    RemoveAll();
}

}
