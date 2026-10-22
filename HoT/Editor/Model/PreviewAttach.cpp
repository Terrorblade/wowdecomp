#include "HoT/Editor/Model/PreviewAttach.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "HoT/Character/HD/HDCharacters.h"
#include "HoT/Editor/Model/ModelFiles.h"
#include "HoT/Lua/Script.h"
#include "HoT/Models/ModernM2.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Glue/ComponentData.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"

namespace HoT::Editor::ModelEditor::PreviewAttach {

namespace Files = Model::Files;

static constexpr uint32_t kMaxSlots = 16;
static constexpr uint32_t kRiderRace = 1;
static constexpr uint32_t kRiderSex = 0;
static constexpr uint32_t kMountAnim = 91;
static const char* const kMarkerModel = "Spells\\ErrorCube.mdx";

struct Slot {
    bool m_rider = false;
    uint32_t m_attachment = 0;
    uint32_t m_anim = kMountAnim;
    uint32_t m_appliedAnim = 0xFFFFFFFF;
    float m_offset[3] = {};
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_roll = 0.0f;
    float m_size = 1.0f;
    CCharacterComponent* m_component = nullptr;
    CM2Model* m_model = nullptr;
};

using Slots = std::map<uint32_t, Slot>;

static std::map<CSimpleModel*, Slots> s_frames;
static std::map<uint32_t, Slots> s_world;
static std::map<std::string, bool> s_animExists;
static bool (*s_hasAccess)() = nullptr;

static CSimpleModel* FrameArg(lua_State* L) {
    return static_cast<CSimpleModel*>(FrameScript_GetObjectThis(L, CSimpleModel::GetObjectType()));
}

static float Field(lua_State* L, int32_t table, const char* key, float fallback) {
    lua_getfield(L, table, key);
    float value = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : fallback;
    lua_pop(L, 1);
    return value;
}

static void SetNumber(lua_State* L, const char* key, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
}

static void SetBool(lua_State* L, const char* key, bool value) {
    lua_pushboolean(L, value);
    lua_setfield(L, -2, key);
}

static void FreeSlot(Slot& slot) {
    if (slot.m_model && slot.m_model->m_attachParent) {
        slot.m_model->DetachFromParent();
    }

    if (slot.m_component) {
        CCharacterComponent::FreeComponent(slot.m_component);
    } else if (slot.m_model) {
        slot.m_model->Release();
    }

    slot.m_component = nullptr;
    slot.m_model = nullptr;
    slot.m_appliedAnim = 0xFFFFFFFF;
}

static void FreeSlots(Slots& slots) {
    for (auto& pair : slots) {
        FreeSlot(pair.second);
    }
}

static void FreeFrame(CSimpleModel* frame) {
    auto it = s_frames.find(frame);

    if (it == s_frames.end()) {
        return;
    }

    FreeSlots(it->second);
    s_frames.erase(it);
}

// A naked human male built like the character select one, so skin and geosets come out right.
static bool CreateRider(CM2Scene* scene, Slot& slot) {
    CreatureModelDataRec* modelRec = GetModelDataRec(kRiderRace, kRiderSex);

    if (!modelRec || !modelRec->m_modelName || !*modelRec->m_modelName) {
        return false;
    }

    CCharacterComponent* component = CCharacterComponent::AllocComponent();

    if (!component) {
        return false;
    }

    ComponentData data;
    data.m_preferences.m_raceID = kRiderRace;
    data.m_preferences.m_sexID = kRiderSex;
    data.m_preferences.m_classId = 1;
    data.m_model = scene->CreateModel(HoT::HD::GlueModelPath(modelRec->m_modelName), 0);

    if (!data.m_model) {
        CCharacterComponent::FreeComponent(component);
        return false;
    }

    data.m_flags |= 0x2;
    component->Init(&data, nullptr);
    component->RenderPrep(0);
    slot.m_component = component;
    slot.m_model = component->m_data.m_model;
    return true;
}

// The slot model takes the lighting of its parent, so it looks right in a frame and in the world.
static bool CreateSlotModel(CM2Model* parent, Slot& slot) {
    CM2Scene* scene = parent->m_scene;

    if (!scene) {
        return false;
    }

    if (slot.m_rider) {
        if (!CreateRider(scene, slot)) {
            return false;
        }
    } else {
        slot.m_model = scene->CreateModel(kMarkerModel, 0);

        if (!slot.m_model) {
            return false;
        }
    }

    slot.m_model->m_lightingCallback = parent->m_lightingCallback;
    slot.m_model->m_lightingArg = parent->m_lightingArg;
    slot.m_model->SetAnimating(1);
    return true;
}

static float BoundingRadius(CM2Model* model) {
    CAaBox box = model->GetBoundingBox();
    float x = box.t.x - box.b.x;
    float y = box.t.y - box.b.y;
    float z = box.t.z - box.b.z;
    float radius = 0.5f * sqrtf(x * x + y * y + z * z);
    return radius > 0.001f && std::isfinite(radius) ? radius : 1.0f;
}

// Same seat transform as CVehiclePassenger_C::ComputeModelTransform, the rider keeps its own size whatever the parent scale.
static void PlaceSlot(CM2Model* parent, Slot& slot, float riderScale, float markerScale) {
    CM2Model* model = slot.m_model;
    C44Matrix attachment = parent->GetAttachmentWorldTransform(slot.m_attachment);
    float length = sqrtf(attachment.a0 * attachment.a0 + attachment.a1 * attachment.a1 + attachment.a2 * attachment.a2);
    float wanted = slot.m_rider ? riderScale : (markerScale > 0.0f ? markerScale : length) * BoundingRadius(parent) * 0.06f * slot.m_size;

    C44Matrix matrix;
    matrix.RotateAroundZ(-slot.m_yaw);

    if (slot.m_roll != 0.0f) {
        matrix.RotateAroundX(slot.m_roll);
    }

    if (slot.m_pitch != 0.0f) {
        matrix.RotateAroundY(slot.m_pitch);
    }

    matrix.Scale(length > 0.0001f ? wanted / length : wanted);
    matrix.d0 = slot.m_offset[0];
    matrix.d1 = slot.m_offset[1];
    matrix.d2 = slot.m_offset[2];
    model->m_fFlags |= 0x8000;
    model->m_worldTransform = matrix;
}

// Follows the parent, a frame swaps it on every preview reload and a unit when its display changes.
static bool UpdateSlot(CM2Model* parent, Slot& slot, float riderScale, float markerScale) {
    bool loaded = parent && parent->IsLoaded(0, 0);

    if (!slot.m_model && parent) {
        CreateSlotModel(parent, slot);
    }

    if (!slot.m_model) {
        return false;
    }

    if (!parent || (loaded && !parent->HasAttachment(slot.m_attachment))) {
        if (slot.m_model->m_attachParent) {
            slot.m_model->DetachFromParent();
        }
    } else if (slot.m_model->m_attachParent != parent || slot.m_model->m_attachmentId != slot.m_attachment) {
        slot.m_model->AttachToParent(parent, slot.m_attachment, nullptr, 0);
    }

    bool attached = loaded && slot.m_model->m_attachParent == parent && parent->HasAttachment(slot.m_attachment);

    if (attached) {
        PlaceSlot(parent, slot, riderScale, markerScale);
    }

    if (slot.m_rider && slot.m_appliedAnim != slot.m_anim) {
        slot.m_model->SetBoneSequence(0xFFFFFFFF, slot.m_anim, 0xFFFFFFFF, 0, 1.0f, 1, 1);
        slot.m_appliedAnim = slot.m_anim;
    }

    return attached;
}

static void UpdateFrame(CSimpleModel* frame, lua_State* L) {
    auto it = s_frames.find(frame);

    if (L) {
        lua_createtable(L, 0, 0);
    }

    if (it == s_frames.end()) {
        return;
    }

    float frameScale = frame->m_layoutScale * NDCToDDCHeight(1.0f) * 1.6666666f;

    for (auto& pair : it->second) {
        bool attached = UpdateSlot(frame->m_model, pair.second, frameScale, frameScale * frame->m_modelScale);

        if (L) {
            lua_pushboolean(L, attached);
            lua_rawseti(L, -2, static_cast<int32_t>(pair.first));
        }
    }
}

static bool AnimExists(const std::string& path) {
    std::string key = Files::Lower(path);
    auto it = s_animExists.find(key);

    if (it == s_animExists.end()) {
        it = s_animExists.emplace(key, Files::GameFileExists(path)).first;
    }

    return it->second;
}

// The same file lookup CM2Shared does before it plays an external sequence.
static bool SharedSequenceAvailable(CM2Shared* shared, uint32_t index) {
    M2Data* data = shared->m_data;
    uint32_t count = data->m_sequences.m_count;

    if (index >= count) {
        return false;
    }

    M2Sequence* source = &data->m_sequences[index];

    for (uint32_t hops = 0; (source->m_flags & 0x40) && hops <= count; hops++) {
        if (source->m_aliasNext >= count) {
            return false;
        }

        source = &data->m_sequences[source->m_aliasNext];
    }

    if (source->m_flags & 0x20) {
        return true;
    }

    char suffix[32];
    snprintf(suffix, sizeof(suffix), "%04u-%02u.anim", static_cast<uint32_t>(source->m_id), static_cast<uint32_t>(source->m_variationIndex));

    if (AnimExists(Files::StripExtension(shared->m_fileName) + suffix)) {
        return true;
    }

    if (shared->m_modern) {
        uint32_t sourceIndex = static_cast<uint32_t>(source - data->m_sequences.Data());
        const char* path = HoT::Models::AnimFilePath(shared->m_modern, shared->m_fileName, sourceIndex, source->m_id, source->m_variationIndex);
        return path && AnimExists(path);
    }

    return false;
}

static CM2Model* LoadedModel(CSimpleModel* frame, lua_State* L, int32_t slotIndex) {
    CM2Model* model = frame ? frame->m_model : nullptr;

    if (frame && !lua_isnoneornil(L, slotIndex)) {
        auto it = s_frames.find(frame);
        model = nullptr;

        if (it != s_frames.end()) {
            auto slot = it->second.find(static_cast<uint32_t>(lua_tonumber(L, slotIndex)));
            model = slot != it->second.end() ? slot->second.m_model : nullptr;
        }
    }

    if (!model || !model->IsLoaded(0, 0) || !model->m_shared || !model->m_shared->m_data) {
        return nullptr;
    }

    return model;
}

static void ReadOptions(lua_State* L, int32_t options, Slot& slot) {
    lua_getfield(L, options, "rider");
    bool rider = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    if (rider != slot.m_rider) {
        FreeSlot(slot);
        slot.m_rider = rider;
    }

    slot.m_attachment = static_cast<uint32_t>(std::max(0.0f, Field(L, options, "attachment", 0.0f)));
    slot.m_anim = static_cast<uint32_t>(std::max(0.0f, Field(L, options, "anim", static_cast<float>(kMountAnim))));
    slot.m_yaw = Field(L, options, "yaw", 0.0f);
    slot.m_pitch = Field(L, options, "pitch", 0.0f);
    slot.m_roll = Field(L, options, "roll", 0.0f);
    slot.m_size = Field(L, options, "size", 1.0f);
    lua_getfield(L, options, "offset");
    static const char* const names[3] = { "x", "y", "z" };

    for (int32_t i = 0; i < 3; i++) {
        slot.m_offset[i] = lua_type(L, -1) == LUA_TTABLE ? Field(L, lua_gettop(L), names[i], 0.0f) : 0.0f;
    }

    lua_pop(L, 1);
}

// ModelEditor_SetPreviewAttach(frame, slot, { attachment, rider, anim, offset = { x, y, z }, yaw, pitch, roll, size } or nil).
static int32_t Script_SetPreviewAttach(lua_State* L) {
    CSimpleModel* frame = FrameArg(L);

    if (!frame || !lua_isnumber(L, 2) || lua_tonumber(L, 2) < 1 || lua_tonumber(L, 2) > kMaxSlots) {
        lua_pushboolean(L, false);
        return 1;
    }

    uint32_t index = static_cast<uint32_t>(lua_tonumber(L, 2));

    if (lua_type(L, 3) != LUA_TTABLE) {
        auto it = s_frames.find(frame);

        if (it != s_frames.end()) {
            auto slot = it->second.find(index);

            if (slot != it->second.end()) {
                FreeSlot(slot->second);
                it->second.erase(slot);
            }
        }

        lua_pushboolean(L, true);
        return 1;
    }

    Slot& slot = s_frames[frame][index];
    ReadOptions(L, 3, slot);
    UpdateFrame(frame, nullptr);
    lua_pushboolean(L, true);
    lua_pushstring(L, slot.m_model && slot.m_model->m_shared ? slot.m_model->m_shared->m_fileName : "");
    return 2;
}

// ModelEditor_UpdatePreviewAttach(frame) -> { [slot] = attached }, call it every frame while the preview shows.
static int32_t Script_UpdatePreviewAttach(lua_State* L) {
    CSimpleModel* frame = FrameArg(L);

    if (!frame) {
        return 0;
    }

    UpdateFrame(frame, L);
    return 1;
}

static int32_t Script_ClearPreviewAttach(lua_State* L) {
    CSimpleModel* frame = FrameArg(L);

    if (frame) {
        FreeFrame(frame);
    }

    return 0;
}

// ModelEditor_SetPreviewSkin(frame, displayId) puts the CreatureDisplayInfo texture variations on the frame model.
static int32_t Script_SetPreviewSkin(lua_State* L) {
    CSimpleModel* frame = FrameArg(L);
    CreatureDisplayInfoRec* displayRec = lua_isnumber(L, 2) ? g_creatureDisplayInfoDB.GetRecord(static_cast<int32_t>(lua_tonumber(L, 2))) : nullptr;
    CreatureModelDataRec* modelRec = displayRec ? g_creatureModelDataDB.GetRecord(displayRec->m_modelID) : nullptr;

    if (!frame || !frame->m_model || !modelRec) {
        lua_pushboolean(L, false);
        return 1;
    }

    CCharacterComponent::ReplaceMonsterSkin(frame->m_model, displayRec, modelRec);
    lua_pushboolean(L, true);
    return 1;
}

// ModelEditor_GetPreviewAttachments(frame) -> { { index, id, bone, position }, ... } of the loaded model, nil until it loads.
static int32_t Script_GetPreviewAttachments(lua_State* L) {
    CM2Model* model = LoadedModel(FrameArg(L), L, 2);

    if (!model) {
        return 0;
    }

    M2Data* data = model->m_shared->m_data;
    uint32_t count = data->m_attachments.m_count;
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        const M2Attachment& attachment = data->m_attachments[i];
        lua_createtable(L, 0, 4);
        SetNumber(L, "index", i);
        SetNumber(L, "id", attachment.m_id);
        SetNumber(L, "bone", attachment.m_bone);
        lua_createtable(L, 0, 3);
        SetNumber(L, "x", attachment.m_position.x);
        SetNumber(L, "y", attachment.m_position.y);
        SetNumber(L, "z", attachment.m_position.z);
        lua_setfield(L, -2, "position");
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    return 1;
}

// ModelEditor_GetPreviewSequences(frame [, slot]) -> { { index, id, variation, duration, available }, ... }, nil until it loads.
static int32_t Script_GetPreviewSequences(lua_State* L) {
    CM2Model* model = LoadedModel(FrameArg(L), L, 2);

    if (!model) {
        return 0;
    }

    CM2Shared* shared = model->m_shared;
    M2Data* data = shared->m_data;
    uint32_t count = data->m_sequences.m_count;
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        const M2Sequence& sequence = data->m_sequences[i];
        lua_createtable(L, 0, 5);
        SetNumber(L, "index", i);
        SetNumber(L, "id", sequence.m_id);
        SetNumber(L, "variation", sequence.m_variationIndex);
        SetNumber(L, "duration", sequence.m_duration);
        SetBool(L, "available", SharedSequenceAvailable(shared, i));
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    return 1;
}

bool SetWorldSlot(lua_State* L, uint32_t host, uint32_t index, int32_t options) {
    if (index < 1 || index > kMaxSlots) {
        return false;
    }

    if (lua_type(L, options) != LUA_TTABLE) {
        auto it = s_world.find(host);

        if (it != s_world.end()) {
            auto slot = it->second.find(index);

            if (slot != it->second.end()) {
                FreeSlot(slot->second);
                it->second.erase(slot);
            }
        }

        return true;
    }

    ReadOptions(L, options, s_world[host][index]);
    return true;
}

void UpdateWorld(uint32_t host, CM2Model* parent) {
    auto it = s_world.find(host);

    if (it == s_world.end()) {
        return;
    }

    for (auto& pair : it->second) {
        UpdateSlot(parent, pair.second, 1.0f, 0.0f);
    }
}

void DetachWorld(uint32_t host) {
    auto it = s_world.find(host);

    if (it != s_world.end()) {
        FreeSlots(it->second);
    }
}

void FreeWorld(uint32_t host) {
    auto it = s_world.find(host);

    if (it != s_world.end()) {
        FreeSlots(it->second);
        s_world.erase(it);
    }
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!s_hasAccess || !s_hasAccess()) {
        return 0;
    }

    return Function(L);
}

void Apply(bool (*hasAccess)()) {
    s_hasAccess = hasAccess;
    Lua::RegisterFunction("ModelEditor_SetPreviewAttach", Gated<Script_SetPreviewAttach>);
    Lua::RegisterFunction("ModelEditor_UpdatePreviewAttach", Gated<Script_UpdatePreviewAttach>);
    Lua::RegisterFunction("ModelEditor_ClearPreviewAttach", Gated<Script_ClearPreviewAttach>);
    Lua::RegisterFunction("ModelEditor_SetPreviewSkin", Gated<Script_SetPreviewSkin>);
    Lua::RegisterFunction("ModelEditor_GetPreviewAttachments", Gated<Script_GetPreviewAttachments>);
    Lua::RegisterFunction("ModelEditor_GetPreviewSequences", Gated<Script_GetPreviewSequences>);
}

void OnGameDestroy() {
    while (!s_frames.empty()) {
        FreeFrame(s_frames.begin()->first);
    }

    for (auto& pair : s_world) {
        FreeSlots(pair.second);
    }

    s_world.clear();

    s_animExists.clear();
}

void OnFrameDestroyed(CSimpleModel* frame) {
    if (!s_frames.empty()) {
        FreeFrame(frame);
    }
}

}
