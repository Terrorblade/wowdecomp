#include "HoT/Editor/Model/ModelEditor.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "Engine/Source/Model2/M2Cache.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Services/CameraInt.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "HoT/Core/Log.h"
#include "HoT/Editor/Model/ModelDoc.h"
#include "HoT/Editor/Model/ModelFiles.h"
#include "HoT/Editor/Model/PreviewAttach.h"
#include "HoT/Editor/Model/TextureImport.h"
#include "HoT/Editor/Resources.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Models/FileDataId.h"
#include "HoT/Platform/Files.h"
#include "WoW/Source/WorldClient/World.h"

namespace HoT::Editor::ModelEditor {

namespace Files = Model::Files;
namespace TextureImport = Model::TextureImport;
using Model::ModelDoc;
using Model::PartTrack;
using Model::Track;
using Model::TrackKeys;
using Model::ValueType;

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr size_t kPreviewsKept = 4;
static constexpr uint32_t kMaxSkinProfiles = 8;

struct Entry {
    int32_t m_id = 0;
    ModelDoc m_doc;
    std::deque<std::string> m_previewDirs;
    uint32_t m_publishedRevision = 0;
    std::string m_previewPath;
};

static std::map<int32_t, std::unique_ptr<Entry>> s_docs;
static int32_t s_nextDoc = 1;
static uint32_t s_nextPreview = 1;

// Lua helpers.

static int32_t AbsIndex(lua_State* L, int32_t index) {
    return index < 0 && index > LUA_REGISTRYINDEX ? lua_gettop(L) + index + 1 : index;
}

static int32_t PushNilError(lua_State* L, const std::string& error) {
    lua_pushnil(L);
    lua_pushstring(L, error.c_str());
    return 2;
}

static int32_t PushFalseError(lua_State* L, const std::string& error) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, error.c_str());
    return 2;
}

static void SetNumber(lua_State* L, const char* key, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
}

static void SetString(lua_State* L, const char* key, const std::string& value) {
    lua_pushstring(L, value.c_str());
    lua_setfield(L, -2, key);
}

static void SetBool(lua_State* L, const char* key, bool value) {
    lua_pushboolean(L, value ? 1 : 0);
    lua_setfield(L, -2, key);
}

static void PushVector(lua_State* L, const float* values, uint32_t count) {
    static const char* const names[3] = { "x", "y", "z" };
    lua_createtable(L, 0, static_cast<int32_t>(count));

    for (uint32_t i = 0; i < count && i < 3; i++) {
        SetNumber(L, names[i], values[i]);
    }
}

static bool ToNumber(lua_State* L, int32_t index, double& out) {
    if (lua_type(L, index) != LUA_TNUMBER) {
        return false;
    }

    out = lua_tonumber(L, index);
    return std::isfinite(out);
}

static bool ToInteger(lua_State* L, int32_t index, double low, double high, int64_t& out) {
    double value;

    if (!ToNumber(L, index, value) || value != floor(value) || value < low || value > high) {
        return false;
    }

    out = static_cast<int64_t>(value);
    return true;
}

static bool IndexArg(lua_State* L, int32_t index, uint32_t& out) {
    int64_t value;

    if (!ToInteger(L, index, 0, 4294967295.0, value)) {
        return false;
    }

    out = static_cast<uint32_t>(value);
    return true;
}

// Reads t[key] or t[position] onto the stack top, returns false and pops when absent.
static bool PushMember(lua_State* L, int32_t table, const char* key, int32_t position) {
    lua_getfield(L, table, key);

    if (lua_isnil(L, -1) && position > 0) {
        lua_pop(L, 1);
        lua_rawgeti(L, table, position);
    }

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    return true;
}

static bool ReadVector(lua_State* L, int32_t index, float* out, uint32_t count, std::string& error) {
    static const char* const names[3] = { "x", "y", "z" };
    index = AbsIndex(L, index);

    if (lua_type(L, index) != LUA_TTABLE) {
        error = "expected a vector table";
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        double value = 0.0;
        bool ok = PushMember(L, index, names[i], static_cast<int32_t>(i + 1)) && ToNumber(L, -1, value);

        if (ok) {
            lua_pop(L, 1);
        }

        if (!ok || fabs(value) > 1.0e7) {
            error = std::string("vector component ") + names[i] + " missing or out of range";
            return false;
        }

        out[i] = static_cast<float>(value);
    }

    return true;
}

static bool ReadValue(lua_State* L, int32_t index, ValueType type, float* out, std::string& error) {
    uint32_t components = Model::ValueComponents(type);

    if (components == 1) {
        double value;

        if (!ToNumber(L, index, value)) {
            error = "expected a number value";
            return false;
        }

        out[0] = static_cast<float>(value);
        return true;
    }

    return ReadVector(L, index, out, components, error);
}

static void PushValue(lua_State* L, ValueType type, const float* values) {
    uint32_t components = Model::ValueComponents(type);

    if (components == 1) {
        lua_pushnumber(L, values[0]);
    } else {
        PushVector(L, values, components);
    }
}

static void PushTrack(lua_State* L, const Track& track, ValueType type) {
    uint32_t components = Model::ValueComponents(type);
    lua_createtable(L, 0, 3);
    SetNumber(L, "interpolation", track.m_interpolation);
    SetNumber(L, "globalSequence", track.m_globalSequence);
    lua_createtable(L, static_cast<int32_t>(track.m_sequences.size()), 0);

    for (size_t i = 0; i < track.m_sequences.size(); i++) {
        const TrackKeys& keys = track.m_sequences[i];
        lua_createtable(L, 0, 3);
        SetNumber(L, "sequence", keys.m_sequence);
        lua_createtable(L, static_cast<int32_t>(keys.m_times.size()), 0);

        for (size_t k = 0; k < keys.m_times.size(); k++) {
            lua_pushnumber(L, keys.m_times[k]);
            lua_rawseti(L, -2, static_cast<int32_t>(k + 1));
        }

        lua_setfield(L, -2, "times");
        lua_createtable(L, static_cast<int32_t>(keys.m_times.size()), 0);

        for (size_t k = 0; k < keys.m_times.size(); k++) {
            PushValue(L, type, &keys.m_values[k * components]);
            lua_rawseti(L, -2, static_cast<int32_t>(k + 1));
        }

        lua_setfield(L, -2, "values");
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "sequences");
}

static void PushPartTrack(lua_State* L, const PartTrack& track, ValueType type) {
    uint32_t components = Model::ValueComponents(type);
    size_t values = track.m_values.size() / components;
    lua_createtable(L, 0, 2);
    lua_createtable(L, static_cast<int32_t>(track.m_times.size()), 0);

    for (size_t i = 0; i < track.m_times.size(); i++) {
        lua_pushnumber(L, track.m_times[i]);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "times");
    lua_createtable(L, static_cast<int32_t>(values), 0);

    for (size_t i = 0; i < values; i++) {
        PushValue(L, type, &track.m_values[i * components]);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "values");
}

static bool ReadValueList(lua_State* L, int32_t table, const char* key, ValueType type, std::vector<float>& out, std::string& error) {
    out.clear();
    lua_getfield(L, table, key);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return true;
    }

    if (lua_type(L, -1) != LUA_TTABLE) {
        lua_pop(L, 1);
        error = std::string(key) + " must be a list";
        return false;
    }

    int32_t list = lua_gettop(L);
    size_t count = lua_objlen(L, list);
    uint32_t components = Model::ValueComponents(type);
    out.resize(count * components);

    for (size_t i = 0; i < count; i++) {
        lua_rawgeti(L, list, static_cast<int32_t>(i + 1));
        bool ok = ReadValue(L, -1, type, &out[i * components], error);
        lua_pop(L, 1);

        if (!ok) {
            lua_pop(L, 1);
            error = std::string(key) + " entry " + std::to_string(i + 1) + ": " + error;
            return false;
        }
    }

    lua_pop(L, 1);
    return true;
}

static bool ReadTrackTable(lua_State* L, int32_t index, ValueType type, uint16_t currentType, uint16_t currentLoop, Track& out, std::string& error) {
    index = AbsIndex(L, index);

    if (lua_type(L, index) != LUA_TTABLE) {
        error = "expected a track table";
        return false;
    }

    out = Track();
    out.m_interpolation = currentType;
    out.m_globalSequence = currentLoop == 0xFFFF ? -1 : currentLoop;
    int64_t value;

    lua_getfield(L, index, "interpolation");

    if (!lua_isnil(L, -1)) {
        if (!ToInteger(L, -1, 0, 3, value)) {
            lua_pop(L, 1);
            error = "interpolation must be 0 to 3";
            return false;
        }

        out.m_interpolation = static_cast<uint16_t>(value);
    }

    lua_pop(L, 1);
    lua_getfield(L, index, "globalSequence");

    if (!lua_isnil(L, -1)) {
        if (!ToInteger(L, -1, -1, 65534, value)) {
            lua_pop(L, 1);
            error = "globalSequence must be -1 or an index";
            return false;
        }

        out.m_globalSequence = static_cast<int32_t>(value);
    }

    lua_pop(L, 1);
    lua_getfield(L, index, "sequences");

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return true;
    }

    if (lua_type(L, -1) != LUA_TTABLE) {
        lua_pop(L, 1);
        error = "sequences must be a list";
        return false;
    }

    int32_t list = lua_gettop(L);
    size_t count = lua_objlen(L, list);

    for (size_t i = 0; i < count; i++) {
        lua_rawgeti(L, list, static_cast<int32_t>(i + 1));
        int32_t entry = lua_gettop(L);
        TrackKeys keys;
        bool ok = lua_type(L, entry) == LUA_TTABLE;

        if (ok) {
            lua_getfield(L, entry, "sequence");
            ok = ToInteger(L, -1, 0, 65535, value);
            lua_pop(L, 1);
            keys.m_sequence = static_cast<uint32_t>(value);
        }

        if (!ok) {
            lua_settop(L, list - 1);
            error = "sequence entry " + std::to_string(i + 1) + " needs a sequence index";
            return false;
        }

        lua_getfield(L, entry, "times");

        if (lua_type(L, -1) == LUA_TTABLE) {
            int32_t times = lua_gettop(L);
            size_t timeCount = lua_objlen(L, times);

            for (size_t k = 0; k < timeCount; k++) {
                lua_rawgeti(L, times, static_cast<int32_t>(k + 1));
                ok = ToInteger(L, -1, 0, 4294967295.0, value);
                lua_pop(L, 1);

                if (!ok) {
                    lua_settop(L, list - 1);
                    error = "sequence " + std::to_string(keys.m_sequence) + " times must be whole milliseconds";
                    return false;
                }

                keys.m_times.push_back(static_cast<uint32_t>(value));
            }
        } else if (!lua_isnil(L, -1)) {
            lua_settop(L, list - 1);
            error = "times must be a list";
            return false;
        }

        lua_pop(L, 1);

        if (!ReadValueList(L, entry, "values", type, keys.m_values, error)) {
            lua_settop(L, list - 1);
            error = "sequence " + std::to_string(keys.m_sequence) + ": " + error;
            return false;
        }

        lua_pop(L, 1);
        out.m_sequences.push_back(std::move(keys));
    }

    lua_pop(L, 1);
    return true;
}

static bool ReadPartTrackTable(lua_State* L, int32_t index, ValueType type, PartTrack& out, std::string& error) {
    index = AbsIndex(L, index);

    if (lua_type(L, index) != LUA_TTABLE) {
        error = "expected a particle track table";
        return false;
    }

    out = PartTrack();

    if (!ReadValueList(L, index, "times", ValueType::Float, out.m_times, error)) {
        return false;
    }

    return ReadValueList(L, index, "values", type, out.m_values, error);
}

// Documents.

static bool HasAccess() {
    return Session::HasClearance(kRequiredSecurityLevel);
}

static Entry* DocArg(lua_State* L, int32_t index) {
    uint32_t id;

    if (!IndexArg(L, index, id)) {
        return nullptr;
    }

    auto it = s_docs.find(static_cast<int32_t>(id));
    return it == s_docs.end() ? nullptr : it->second.get();
}

static void Changed(Entry* entry) {
    entry->m_doc.MarkChanged();
    Lua::SignalEvent("MODEL_EDITOR_CHANGED", "%d", entry->m_id);
}

static bool IsPreviewPath(const std::string& path) {
    return Files::Lower(path).rfind(Files::Lower(Resources::kPreviewPrefix), 0) == 0;
}

// Normalizes a model path and turns .mdx and .mdl into .m2 like CM2Cache::CreateShared.
static bool NormalizeModelPath(const std::string& input, std::string& output, std::string& error) {
    if (!Files::NormalizeGamePath(input, output, error)) {
        return false;
    }

    if (Files::EndsWith(output, ".mdx") || Files::EndsWith(output, ".mdl")) {
        output = output.substr(0, output.size() - 4) + ".m2";
    }

    if (!Files::EndsWith(output, ".m2")) {
        error = "model paths end in .m2";
        return false;
    }

    if (IsPreviewPath(output)) {
        error = "preview paths cannot be opened";
        return false;
    }

    return true;
}

static std::string FileName(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string TexturePath(const ModelDoc& doc, uint32_t index, uint32_t& fileDataId) {
    std::string name;
    doc.ReadString(doc.RecordPos(Model::HEADER_TEXTURES, Model::TEXTURE_STRIDE, index) + 8, name);
    fileDataId = doc.TextureFileDataId(index);

    // FinishModel only uses TXID for type 0 textures without a name.
    uint32_t type = 0;
    doc.Get(doc.RecordPos(Model::HEADER_TEXTURES, Model::TEXTURE_STRIDE, index), type);

    if (name.size() < 1 && type == 0 && fileDataId) {
        const char* resolved = Models::Fdid::ResolveTexture(fileDataId);
        name = resolved ? resolved : "";
    }

    return name;
}

// ModelEditor_Open(path) -> doc or nil, err.
static int32_t Script_Open(lua_State* L) {
    const char* input = lua_type(L, 1) == LUA_TSTRING ? lua_tostring(L, 1) : nullptr;
    std::string path;
    std::string error;

    if (!input) {
        return PushNilError(L, "expected a model path");
    }

    if (!NormalizeModelPath(input, path, error)) {
        return PushNilError(L, error);
    }

    std::string key = Files::Lower(path);

    for (auto& pair : s_docs) {
        if (Files::Lower(pair.second->m_doc.Path()) == key) {
            lua_pushnumber(L, pair.first);
            return 1;
        }
    }

    std::unique_ptr<Entry> entry(new Entry());

    if (!entry->m_doc.Load(path, error)) {
        return PushNilError(L, error);
    }

    entry->m_id = s_nextDoc++;
    int32_t id = entry->m_id;
    s_docs[id] = std::move(entry);
    lua_pushnumber(L, id);
    return 1;
}

static void DropPreviews(Entry* entry) {
    for (const std::string& dir : entry->m_previewDirs) {
        Resources::RemovePreviews(dir);
    }

    entry->m_previewDirs.clear();
    entry->m_previewPath.clear();
    entry->m_publishedRevision = 0;
}

// ModelEditor_Close(doc).
static int32_t Script_Close(lua_State* L) {
    Entry* entry = DocArg(L, 1);

    if (entry) {
        DropPreviews(entry);
        s_docs.erase(entry->m_id);
    }

    return 0;
}

// ModelEditor_GetDocs() -> { { doc, path, dirty }, ... }.
static int32_t Script_GetDocs(lua_State* L) {
    lua_createtable(L, static_cast<int32_t>(s_docs.size()), 0);
    int32_t n = 0;

    for (auto& pair : s_docs) {
        lua_createtable(L, 0, 3);
        SetNumber(L, "doc", pair.first);
        SetString(L, "path", pair.second->m_doc.Path());
        SetBool(L, "dirty", pair.second->m_doc.Dirty());
        lua_rawseti(L, -2, ++n);
    }

    return 1;
}

static bool IsMultiTexture(const ModelDoc& doc, uint32_t flags) {
    return doc.IsModern() && (flags & Model::PARTICLE_FLAG_MULTI_TEXTURE);
}

// ModelEditor_GetInfo(doc) -> the document summary of the contract.
static std::map<std::string, bool> s_animExists;

static bool AnimExists(const std::string& path) {
    std::string key = Files::Lower(path);
    auto it = s_animExists.find(key);

    if (it == s_animExists.end()) {
        it = s_animExists.emplace(key, Files::GameFileExists(path)).first;
    }

    return it->second;
}

// A sequence plays when its keys are in the file or its .anim file exists, aliases follow their target.
static bool SequenceAvailable(const ModelDoc& doc, uint32_t index) {
    uint32_t count = doc.Count(Model::HEADER_SEQUENCES);

    for (uint32_t hops = 0; index < count && hops <= count; hops++) {
        uint32_t pos = doc.RecordPos(Model::HEADER_SEQUENCES, Model::SEQUENCE_STRIDE, index);
        uint16_t id = 0;
        uint16_t variation = 0;
        uint32_t flags = 0;
        uint16_t alias = 0;
        doc.Get(pos, id);
        doc.Get(pos + 2, variation);
        doc.Get(pos + 0x0C, flags);
        doc.Get(pos + 0x3E, alias);

        if (flags & Model::SEQUENCE_FLAG_ALIAS) {
            index = alias;
            continue;
        }

        if (flags & Model::SEQUENCE_FLAG_INLINE) {
            return true;
        }

        uint32_t fileDataId = doc.AnimFileDataId(id, variation);
        const char* path = fileDataId ? HoT::Models::Fdid::ResolveModel(fileDataId) : nullptr;

        if (path && AnimExists(path)) {
            return true;
        }

        char suffix[32];
        snprintf(suffix, sizeof(suffix), "%04u-%02u.anim", static_cast<uint32_t>(id), static_cast<uint32_t>(variation));
        return AnimExists(Files::StripExtension(doc.Path()) + suffix);
    }

    return false;
}

static int32_t Script_GetInfo(lua_State* L) {
    Entry* entry = DocArg(L, 1);

    if (!entry) {
        return PushNilError(L, "unknown document");
    }

    const ModelDoc& doc = entry->m_doc;
    std::string name;
    doc.ReadString(Model::HEADER_NAME, name);

    lua_createtable(L, 0, 32);
    SetNumber(L, "doc", entry->m_id);
    SetString(L, "path", doc.Path());
    SetString(L, "name", name);
    SetBool(L, "modern", doc.IsModern());
    SetNumber(L, "version", doc.Version());
    SetNumber(L, "sourceVersion", doc.Version());
    SetBool(L, "dirty", doc.Dirty());
    SetNumber(L, "revision", doc.Revision());
    SetNumber(L, "fileSize", static_cast<double>(doc.BuildSize()));
    SetNumber(L, "skinProfiles", doc.Count(Model::HEADER_SKIN_PROFILES));
    SetNumber(L, "boneCount", doc.Count(Model::HEADER_BONES));
    SetNumber(L, "vertexCount", doc.Count(Model::HEADER_VERTICES));
    SetBool(L, "skinsByFileDataId", doc.IsModern() && doc.SkinFileDataId(0) != 0);
    SetBool(L, "animsByFileDataId", doc.HasAnimFileDataIds());

    std::vector<std::string> chunks = doc.ChunkNames();
    lua_createtable(L, static_cast<int32_t>(chunks.size()), 0);

    for (size_t i = 0; i < chunks.size(); i++) {
        lua_pushstring(L, chunks[i].c_str());
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "chunks");

    uint32_t count = doc.Count(Model::HEADER_SEQUENCES);
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t pos = doc.RecordPos(Model::HEADER_SEQUENCES, Model::SEQUENCE_STRIDE, i);
        uint16_t id = 0;
        uint16_t variation = 0;
        uint32_t duration = 0;
        uint32_t flags = 0;
        doc.Get(pos, id);
        doc.Get(pos + 2, variation);
        doc.Get(pos + 4, duration);
        doc.Get(pos + 0x0C, flags);
        lua_createtable(L, 0, 6);
        SetNumber(L, "index", i);
        SetNumber(L, "id", id);
        SetNumber(L, "variation", variation);
        SetNumber(L, "duration", duration);
        SetNumber(L, "flags", flags);
        SetBool(L, "external", !(flags & Model::SEQUENCE_FLAG_INLINE));
        SetBool(L, "available", SequenceAvailable(doc, i));
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "sequences");

    count = doc.Count(Model::HEADER_TEXTURES);
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t pos = doc.RecordPos(Model::HEADER_TEXTURES, Model::TEXTURE_STRIDE, i);
        uint32_t type = 0;
        uint32_t flags = 0;
        uint32_t fileDataId = 0;
        doc.Get(pos, type);
        doc.Get(pos + 4, flags);
        std::string path = TexturePath(doc, i, fileDataId);
        lua_createtable(L, 0, 6);
        SetNumber(L, "index", i);
        SetNumber(L, "type", type);
        SetNumber(L, "flags", flags);
        SetString(L, "path", path);
        SetBool(L, "replaceable", type != 0);
        SetNumber(L, "fileDataId", fileDataId);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "textures");

    count = doc.Count(Model::HEADER_TEXTURE_COMBOS);
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        uint16_t texture = 0;
        doc.Get(doc.RecordPos(Model::HEADER_TEXTURE_COMBOS, 2, i), texture);
        lua_createtable(L, 0, 2);
        SetNumber(L, "index", i);
        SetNumber(L, "texture", texture);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "textureCombos");

    const std::vector<M2Batch>& batches = doc.Batches();
    lua_createtable(L, static_cast<int32_t>(batches.size()), 0);

    for (size_t i = 0; i < batches.size(); i++) {
        const M2Batch& batch = batches[i];
        lua_createtable(L, 0, 8);
        SetNumber(L, "index", static_cast<double>(i));
        SetNumber(L, "skinSection", batch.m_skinSectionIndex);
        SetNumber(L, "geoset", batch.m_geosetIndex);
        SetNumber(L, "textureCombo", batch.m_textureComboIndex);
        SetNumber(L, "textureCount", batch.m_textureCount);
        SetNumber(L, "material", batch.m_materialIndex);
        SetNumber(L, "priorityPlane", batch.m_priorityPlane);
        SetNumber(L, "shader", batch.m_shader);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "batches");

    count = doc.Count(Model::HEADER_MATERIALS);
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        uint16_t flags = 0;
        uint16_t blend = 0;
        uint32_t pos = doc.RecordPos(Model::HEADER_MATERIALS, Model::MATERIAL_STRIDE, i);
        doc.Get(pos, flags);
        doc.Get(pos + 2, blend);
        lua_createtable(L, 0, 3);
        SetNumber(L, "index", i);
        SetNumber(L, "flags", flags);
        SetNumber(L, "blendMode", blend);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "materials");

    count = doc.Count(Model::HEADER_PARTICLES);
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t pos = doc.RecordPos(Model::HEADER_PARTICLES, doc.ParticleStride(), i);
        int32_t id = 0;
        uint32_t flags = 0;
        uint16_t bone = 0;
        uint16_t texture = 0;
        uint8_t blend = 0;
        uint8_t emitter = 0;
        doc.Get(pos, id);
        doc.Get(pos + 4, flags);
        doc.Get(pos + 0x14, bone);
        doc.Get(pos + 0x16, texture);
        doc.Get(pos + 0x28, blend);
        doc.Get(pos + 0x29, emitter);
        lua_createtable(L, 0, 6);
        SetNumber(L, "index", i);
        SetNumber(L, "id", id);
        SetNumber(L, "emitterType", emitter);
        SetNumber(L, "texture", IsMultiTexture(doc, flags) ? (texture & 0x1F) : texture);
        SetNumber(L, "blendMode", blend);
        SetNumber(L, "bone", bone);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "particles");

    count = doc.Count(Model::HEADER_RIBBONS);
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t pos = doc.RecordPos(Model::HEADER_RIBBONS, Model::RIBBON_STRIDE, i);
        int32_t id = 0;
        uint16_t bone = 0;
        doc.Get(pos, id);
        doc.Get(pos + 4, bone);
        lua_createtable(L, 0, 3);
        SetNumber(L, "index", i);
        SetNumber(L, "id", id);
        SetNumber(L, "bone", bone);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "ribbons");

    count = doc.Count(Model::HEADER_BONES);
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t pos = doc.RecordPos(Model::HEADER_BONES, Model::BONE_STRIDE, i);
        int32_t id = 0;
        int16_t parent = 0;
        doc.Get(pos, id);
        doc.Get(pos + 8, parent);
        lua_createtable(L, 0, 3);
        SetNumber(L, "index", i);
        SetNumber(L, "id", id);
        SetNumber(L, "parent", parent);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "bones");

    count = doc.Count(Model::HEADER_ATTACHMENTS);
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t pos = doc.RecordPos(Model::HEADER_ATTACHMENTS, Model::ATTACHMENT_STRIDE, i);
        uint32_t id = 0;
        uint16_t bone = 0;
        float position[3] = {};
        doc.Get(pos, id);
        doc.Get(pos + 4, bone);
        doc.Read(pos + 8, position, sizeof(position));
        lua_createtable(L, 0, 4);
        SetNumber(L, "index", i);
        SetNumber(L, "id", id);
        SetNumber(L, "bone", bone);
        PushVector(L, position, 3);
        lua_setfield(L, -2, "position");
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    lua_setfield(L, -2, "attachments");
    return 1;
}

// Textures and materials.

static bool ReadTextureFields(lua_State* L, int32_t index, bool& hasType, uint32_t& type, bool& hasFlags, uint32_t& flags, bool& hasPath, std::string& path, std::string& error) {
    if (lua_type(L, index) != LUA_TTABLE) {
        error = "expected a texture table";
        return false;
    }

    int64_t value;
    lua_getfield(L, index, "type");
    hasType = !lua_isnil(L, -1);

    if (hasType && !ToInteger(L, -1, 0, 255, value)) {
        lua_pop(L, 1);
        error = "texture type must be 0 to 255";
        return false;
    }

    type = hasType ? static_cast<uint32_t>(value) : 0;
    lua_pop(L, 1);
    lua_getfield(L, index, "flags");
    hasFlags = !lua_isnil(L, -1);

    if (hasFlags && !ToInteger(L, -1, 0, 4294967295.0, value)) {
        lua_pop(L, 1);
        error = "texture flags must be a whole number";
        return false;
    }

    flags = hasFlags ? static_cast<uint32_t>(value) : 0;
    lua_pop(L, 1);
    lua_getfield(L, index, "path");
    hasPath = !lua_isnil(L, -1);

    if (hasPath) {
        if (lua_type(L, -1) != LUA_TSTRING) {
            lua_pop(L, 1);
            error = "texture path must be a string";
            return false;
        }

        std::string text = lua_tostring(L, -1);
        path.clear();

        if (text.find_first_not_of(" \t") != std::string::npos && !Files::NormalizeGamePath(text, path, error)) {
            lua_pop(L, 1);
            return false;
        }
    }

    lua_pop(L, 1);
    return true;
}

// Writes a texture name and keeps a modern TXID entry only when it still names the same file.
static bool ApplyTexturePath(ModelDoc& doc, uint32_t index, const std::string& path, std::string& error) {
    uint32_t pos = doc.RecordPos(Model::HEADER_TEXTURES, Model::TEXTURE_STRIDE, index);

    if (!doc.WriteString(pos + 8, path, "t" + std::to_string(index) + ".path", error)) {
        return false;
    }

    uint32_t fileDataId = doc.TextureFileDataId(index);

    if (fileDataId) {
        const char* resolved = Models::Fdid::ResolveTexture(fileDataId);

        if (path.empty() || !resolved || Files::Lower(resolved) != Files::Lower(path)) {
            doc.SetTextureFileDataId(index, 0);
        }
    }

    return true;
}

// ModelEditor_SetTexture(doc, index, { type, flags, path }) -> ok, err.
static int32_t Script_SetTexture(lua_State* L) {
    Entry* entry = DocArg(L, 1);
    uint32_t index;
    std::string error;

    if (!entry) {
        return PushFalseError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;

    if (!IndexArg(L, 2, index) || index >= doc.Count(Model::HEADER_TEXTURES)) {
        return PushFalseError(L, "texture index out of range");
    }

    bool hasType;
    bool hasFlags;
    bool hasPath;
    uint32_t type;
    uint32_t flags;
    std::string path;

    if (!ReadTextureFields(L, 3, hasType, type, hasFlags, flags, hasPath, path, error)) {
        return PushFalseError(L, error);
    }

    uint32_t pos = doc.RecordPos(Model::HEADER_TEXTURES, Model::TEXTURE_STRIDE, index);

    if (hasType) {
        doc.Put(pos, type);
    }

    if (hasFlags) {
        doc.Put(pos + 4, flags);
    }

    if (hasPath && !ApplyTexturePath(doc, index, path, error)) {
        return PushFalseError(L, error);
    }

    Changed(entry);
    lua_pushboolean(L, 1);
    return 1;
}

// ModelEditor_AddTexture(doc, { type, flags, path }) -> index or nil, err.
static int32_t Script_AddTexture(lua_State* L) {
    Entry* entry = DocArg(L, 1);
    std::string error;

    if (!entry) {
        return PushNilError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;
    bool hasType;
    bool hasFlags;
    bool hasPath;
    uint32_t type;
    uint32_t flags;
    std::string path;

    if (!ReadTextureFields(L, 2, hasType, type, hasFlags, flags, hasPath, path, error)) {
        return PushNilError(L, error);
    }

    uint32_t count = doc.Count(Model::HEADER_TEXTURES);
    std::vector<uint8_t> records;

    if (count >= 0xFFFF || !doc.CopyRecords(Model::HEADER_TEXTURES, Model::TEXTURE_STRIDE, records)) {
        return PushNilError(L, "the texture list cannot grow");
    }

    uint8_t record[Model::TEXTURE_STRIDE] = {};
    memcpy(record, &type, 4);
    memcpy(record + 4, &flags, 4);
    records.insert(records.end(), record, record + sizeof(record));

    if (!doc.ReplaceRecords(Model::HEADER_TEXTURES, records, count + 1, error) || (!path.empty() && !ApplyTexturePath(doc, count, path, error))) {
        return PushNilError(L, error);
    }

    Changed(entry);
    lua_pushnumber(L, count);
    return 1;
}

// ModelEditor_SetTextureCombo(doc, comboIndex, textureIndex) -> ok, err.
static int32_t Script_SetTextureCombo(lua_State* L) {
    Entry* entry = DocArg(L, 1);
    uint32_t combo;
    uint32_t texture;
    std::string error;

    if (!entry) {
        return PushFalseError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;

    if (!IndexArg(L, 2, combo) || combo >= doc.Count(Model::HEADER_TEXTURE_COMBOS)) {
        return PushFalseError(L, "texture combo index out of range");
    }

    if (!IndexArg(L, 3, texture) || texture >= doc.Count(Model::HEADER_TEXTURES)) {
        return PushFalseError(L, "texture index out of range");
    }

    std::vector<uint8_t> bytes;

    if (!doc.CopyRecords(Model::HEADER_TEXTURE_COMBOS, 2, bytes)) {
        return PushFalseError(L, "texture combos outside the file");
    }

    // The combo list is often shared with other lookups, so it is copied before the change.
    std::vector<uint16_t> values(bytes.size() / 2);
    memcpy(values.data(), bytes.data(), values.size() * 2);
    values[combo] = static_cast<uint16_t>(texture);

    if (!doc.WriteU16Array(Model::HEADER_TEXTURE_COMBOS, values, "combos", error)) {
        return PushFalseError(L, error);
    }

    Changed(entry);
    lua_pushboolean(L, 1);
    return 1;
}

// ModelEditor_SetMaterial(doc, index, flags, blendMode) -> ok, err.
static int32_t Script_SetMaterial(lua_State* L) {
    Entry* entry = DocArg(L, 1);
    uint32_t index;
    int64_t flags;
    int64_t blend;

    if (!entry) {
        return PushFalseError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;

    if (!IndexArg(L, 2, index) || index >= doc.Count(Model::HEADER_MATERIALS)) {
        return PushFalseError(L, "material index out of range");
    }

    if (!ToInteger(L, 3, 0, 65535, flags)) {
        return PushFalseError(L, "material flags must be 0 to 65535");
    }

    if (!ToInteger(L, 4, 0, 7, blend)) {
        return PushFalseError(L, "blend mode must be 0 to 7");
    }

    uint32_t pos = doc.RecordPos(Model::HEADER_MATERIALS, Model::MATERIAL_STRIDE, index);
    doc.Put(pos, static_cast<uint16_t>(flags));
    doc.Put(pos + 2, static_cast<uint16_t>(blend));
    Changed(entry);
    lua_pushboolean(L, 1);
    return 1;
}

// Emitter field tables.

enum class Kind : uint8_t {
    U32,
    I32,
    U16,
    I16,
    U8,
    Float,
    Vec2,
    Vec3,
    Range,
    Box,
    String,
    AnimTrack,
    LifeTrack,
    Vec3Array,
    U16Array,
    Fp69Pairs,
};

enum class Check : uint8_t {
    None,
    Bone,
    Texture,
    TextureList,
    MaterialList,
    Blend,
    Emitter,
    PowerOfTwo,
    Cells,
};

struct Field {
    const char* name;
    Kind kind;
    uint32_t offset;
    ValueType value;
    Check check;
    bool needsKey;
    bool modernOnly;
};

static const uint32_t PARTICLE_GRAVITY = 0x84;
static const uint32_t PARTICLE_FLAGS = 0x04;
static const uint32_t PARTICLE_TEXTURE = 0x16;

static const Field s_particleFields[] = {
    { "particleId", Kind::I32, 0x00, ValueType::Float, Check::None, false, false },
    { "flags", Kind::U32, 0x04, ValueType::Float, Check::None, false, false },
    { "position", Kind::Vec3, 0x08, ValueType::Float, Check::None, false, false },
    { "boneIndex", Kind::U16, 0x14, ValueType::Float, Check::Bone, false, false },
    { "textureIndex", Kind::U16, 0x16, ValueType::Float, Check::Texture, false, false },
    { "geometryMdl", Kind::String, 0x18, ValueType::Float, Check::None, false, false },
    { "recursionMdl", Kind::String, 0x20, ValueType::Float, Check::None, false, false },
    { "blendMode", Kind::U8, 0x28, ValueType::Float, Check::Blend, false, false },
    { "emitterType", Kind::U8, 0x29, ValueType::Float, Check::Emitter, false, false },
    { "colorIndex", Kind::U16, 0x2A, ValueType::Float, Check::None, false, false },
    { "priorityPlane", Kind::I16, 0x2E, ValueType::Float, Check::None, false, false },
    { "rows", Kind::U16, 0x30, ValueType::Float, Check::PowerOfTwo, false, false },
    { "cols", Kind::U16, 0x32, ValueType::Float, Check::PowerOfTwo, false, false },
    { "speedTrack", Kind::AnimTrack, 0x34, ValueType::Float, Check::None, false, false },
    { "variationTrack", Kind::AnimTrack, 0x48, ValueType::Float, Check::None, false, false },
    { "latitudeTrack", Kind::AnimTrack, 0x5C, ValueType::Float, Check::None, false, false },
    { "longitudeTrack", Kind::AnimTrack, 0x70, ValueType::Float, Check::None, false, false },
    { "gravityTrack", Kind::AnimTrack, 0x84, ValueType::Float, Check::None, false, false },
    { "lifeTrack", Kind::AnimTrack, 0x98, ValueType::Float, Check::None, false, false },
    { "lifeVariation", Kind::Float, 0xAC, ValueType::Float, Check::None, false, false },
    { "emissionRateTrack", Kind::AnimTrack, 0xB0, ValueType::Float, Check::None, false, false },
    { "emissionRateVariation", Kind::Float, 0xC4, ValueType::Float, Check::None, false, false },
    { "widthTrack", Kind::AnimTrack, 0xC8, ValueType::Float, Check::None, false, false },
    { "lengthTrack", Kind::AnimTrack, 0xDC, ValueType::Float, Check::None, false, false },
    { "zsourceTrack", Kind::AnimTrack, 0xF0, ValueType::Float, Check::None, false, false },
    { "colorTrack", Kind::LifeTrack, 0x104, ValueType::Vec3, Check::None, true, false },
    { "alphaTrack", Kind::LifeTrack, 0x114, ValueType::Fixed16, Check::None, true, false },
    { "scaleTrack", Kind::LifeTrack, 0x124, ValueType::Vec2, Check::None, true, false },
    { "scaleVariation", Kind::Vec2, 0x134, ValueType::Float, Check::None, false, false },
    { "headCellTrack", Kind::LifeTrack, 0x13C, ValueType::UInt16, Check::None, false, false },
    { "tailCellTrack", Kind::LifeTrack, 0x14C, ValueType::UInt16, Check::None, false, false },
    { "tailLength", Kind::Float, 0x15C, ValueType::Float, Check::None, false, false },
    { "twinkleFPS", Kind::Float, 0x160, ValueType::Float, Check::None, false, false },
    { "twinkleOnOff", Kind::Float, 0x164, ValueType::Float, Check::None, false, false },
    { "twinkleScale", Kind::Range, 0x168, ValueType::Float, Check::None, false, false },
    { "ivelScale", Kind::Float, 0x170, ValueType::Float, Check::None, false, false },
    { "drag", Kind::Float, 0x174, ValueType::Float, Check::None, false, false },
    { "initialSpin", Kind::Float, 0x178, ValueType::Float, Check::None, false, false },
    { "initialSpinVariation", Kind::Float, 0x17C, ValueType::Float, Check::None, false, false },
    { "spin", Kind::Float, 0x180, ValueType::Float, Check::None, false, false },
    { "spinVariation", Kind::Float, 0x184, ValueType::Float, Check::None, false, false },
    { "tumble", Kind::Box, 0x188, ValueType::Float, Check::None, false, false },
    { "windVector", Kind::Vec3, 0x1A0, ValueType::Float, Check::None, false, false },
    { "windTime", Kind::Float, 0x1AC, ValueType::Float, Check::None, false, false },
    { "followSpeed1", Kind::Float, 0x1B0, ValueType::Float, Check::None, false, false },
    { "followScale1", Kind::Float, 0x1B4, ValueType::Float, Check::None, false, false },
    { "followSpeed2", Kind::Float, 0x1B8, ValueType::Float, Check::None, false, false },
    { "followScale2", Kind::Float, 0x1BC, ValueType::Float, Check::None, false, false },
    { "spline", Kind::Vec3Array, 0x1C0, ValueType::Float, Check::None, false, false },
    { "visibilityTrack", Kind::AnimTrack, 0x1C8, ValueType::UInt8, Check::None, false, false },
    { "multiTextureParam0", Kind::Fp69Pairs, 0x1DC, ValueType::Float, Check::None, false, true },
    { "multiTextureParam1", Kind::Fp69Pairs, 0x1E4, ValueType::Float, Check::None, false, true },
};

static const Field s_ribbonFields[] = {
    { "ribbonId", Kind::I32, 0x00, ValueType::Float, Check::None, false, false },
    { "boneIndex", Kind::U16, 0x04, ValueType::Float, Check::Bone, false, false },
    { "position", Kind::Vec3, 0x08, ValueType::Float, Check::None, false, false },
    { "textureIndices", Kind::U16Array, 0x14, ValueType::Float, Check::TextureList, false, false },
    { "materialIndices", Kind::U16Array, 0x1C, ValueType::Float, Check::MaterialList, false, false },
    { "colorTrack", Kind::AnimTrack, 0x24, ValueType::Vec3, Check::None, false, false },
    { "alphaTrack", Kind::AnimTrack, 0x38, ValueType::Fixed16, Check::None, false, false },
    { "heightAboveTrack", Kind::AnimTrack, 0x4C, ValueType::Float, Check::None, false, false },
    { "heightBelowTrack", Kind::AnimTrack, 0x60, ValueType::Float, Check::None, false, false },
    { "edgesPerSecond", Kind::Float, 0x74, ValueType::Float, Check::None, false, false },
    { "edgeLifetime", Kind::Float, 0x78, ValueType::Float, Check::None, false, false },
    { "gravity", Kind::Float, 0x7C, ValueType::Float, Check::None, false, false },
    { "textureRows", Kind::U16, 0x80, ValueType::Float, Check::Cells, false, false },
    { "textureCols", Kind::U16, 0x82, ValueType::Float, Check::Cells, false, false },
    { "textureSlotTrack", Kind::AnimTrack, 0x84, ValueType::UInt16, Check::None, false, false },
    { "visibilityTrack", Kind::AnimTrack, 0x98, ValueType::UInt8, Check::None, false, false },
    { "priorityPlane", Kind::I16, 0xAC, ValueType::Float, Check::None, false, false },
};

static const Field s_attachmentFields[] = {
    { "attachmentId", Kind::U32, 0x00, ValueType::Float, Check::None, false, false },
    { "boneIndex", Kind::U16, 0x04, ValueType::Float, Check::Bone, false, false },
    { "position", Kind::Vec3, 0x08, ValueType::Float, Check::None, false, false },
    { "animateTrack", Kind::AnimTrack, 0x14, ValueType::UInt8, Check::None, false, false },
};

struct RecordKind {
    uint32_t header;
    const Field* fields;
    size_t fieldCount;
    char prefix;
};

static const RecordKind s_particles = { Model::HEADER_PARTICLES, s_particleFields, sizeof(s_particleFields) / sizeof(s_particleFields[0]), 'p' };
static const RecordKind s_ribbons = { Model::HEADER_RIBBONS, s_ribbonFields, sizeof(s_ribbonFields) / sizeof(s_ribbonFields[0]), 'r' };
static const RecordKind s_attachments = { Model::HEADER_ATTACHMENTS, s_attachmentFields, sizeof(s_attachmentFields) / sizeof(s_attachmentFields[0]), 'a' };

static uint32_t Stride(const ModelDoc& doc, const RecordKind& kind) {
    if (kind.header == Model::HEADER_PARTICLES) {
        return doc.ParticleStride();
    }

    return kind.header == Model::HEADER_ATTACHMENTS ? Model::ATTACHMENT_STRIDE : Model::RIBBON_STRIDE;
}

static bool IsPackedGravity(const ModelDoc& doc, uint32_t pos) {
    uint32_t flags = 0;
    return doc.IsModern() && doc.Get(pos + PARTICLE_FLAGS, flags) && (flags & Model::PARTICLE_FLAG_PACKED_GRAVITY);
}

// Pushes one field, false when the bytes are malformed.
static bool PushField(lua_State* L, const ModelDoc& doc, uint32_t pos, const Field& field, std::string& error) {
    uint32_t at = pos + field.offset;

    switch (field.kind) {
        case Kind::U32: {
            uint32_t v = 0;
            doc.Get(at, v);
            lua_pushnumber(L, v);
            return true;
        }
        case Kind::U16: {
            uint16_t v = 0;
            doc.Get(at, v);
            lua_pushnumber(L, v);
            return true;
        }
        case Kind::I32: {
            int32_t v = 0;
            doc.Get(at, v);
            lua_pushnumber(L, v);
            return true;
        }
        case Kind::I16: {
            int16_t v = 0;
            doc.Get(at, v);
            lua_pushnumber(L, v);
            return true;
        }
        case Kind::U8: {
            uint8_t v = 0;
            doc.Get(at, v);
            lua_pushnumber(L, v);
            return true;
        }
        case Kind::Float: {
            float v = 0.0f;
            doc.Get(at, v);
            lua_pushnumber(L, v);
            return true;
        }
        case Kind::Vec2:
        case Kind::Vec3: {
            float v[3] = {};
            uint32_t n = field.kind == Kind::Vec2 ? 2 : 3;
            doc.Read(at, v, n * 4);
            PushVector(L, v, n);
            return true;
        }
        case Kind::Range: {
            float v[2] = {};
            doc.Read(at, v, 8);
            lua_createtable(L, 0, 2);
            SetNumber(L, "min", v[0]);
            SetNumber(L, "max", v[1]);
            return true;
        }
        case Kind::Box: {
            float v[6] = {};
            doc.Read(at, v, 24);
            lua_createtable(L, 0, 2);
            PushVector(L, v, 3);
            lua_setfield(L, -2, "min");
            PushVector(L, v + 3, 3);
            lua_setfield(L, -2, "max");
            return true;
        }
        case Kind::String: {
            std::string text;

            if (!doc.ReadString(at, text)) {
                error = std::string(field.name) + " outside the file";
                return false;
            }

            lua_pushstring(L, text.c_str());
            return true;
        }
        case Kind::AnimTrack: {
            Track track;
            bool packed = field.offset == PARTICLE_GRAVITY && &field >= s_particleFields && &field < s_particleFields + s_particles.fieldCount && IsPackedGravity(doc, pos);

            if (!doc.ReadTrack(at, packed ? ValueType::PackedGravity : field.value, track, error)) {
                error = std::string(field.name) + ": " + error;
                return false;
            }

            if (packed) {
                // The scalar view is what ExpandGravityKeys gives the client, the vectors are under gravityVectors.
                for (TrackKeys& keys : track.m_sequences) {
                    std::vector<float> scalars(keys.m_times.size());

                    for (size_t k = 0; k < scalars.size(); k++) {
                        scalars[k] = -keys.m_values[k * 3 + 2];
                    }

                    keys.m_values.swap(scalars);
                }
            }

            PushTrack(L, track, field.value);
            return true;
        }
        case Kind::LifeTrack: {
            PartTrack track;

            if (!doc.ReadPartTrack(at, field.value, track, error)) {
                error = std::string(field.name) + ": " + error;
                return false;
            }

            PushPartTrack(L, track, field.value);
            return true;
        }
        case Kind::Vec3Array: {
            std::vector<float> values;

            if (!doc.ReadVec3Array(at, values)) {
                error = std::string(field.name) + " outside the file";
                return false;
            }

            lua_createtable(L, static_cast<int32_t>(values.size() / 3), 0);

            for (size_t i = 0; i < values.size() / 3; i++) {
                PushVector(L, &values[i * 3], 3);
                lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
            }

            return true;
        }
        case Kind::U16Array: {
            std::vector<uint16_t> values;

            if (!doc.ReadU16Array(at, values)) {
                error = std::string(field.name) + " outside the file";
                return false;
            }

            lua_createtable(L, static_cast<int32_t>(values.size()), 0);

            for (size_t i = 0; i < values.size(); i++) {
                lua_pushnumber(L, values[i]);
                lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
            }

            return true;
        }
        case Kind::Fp69Pairs: {
            int16_t raw[4] = {};
            doc.Read(at, raw, 8);
            lua_createtable(L, 2, 0);

            for (int32_t i = 0; i < 2; i++) {
                float v[2] = { raw[i * 2] / 512.0f, raw[i * 2 + 1] / 512.0f };
                PushVector(L, v, 2);
                lua_rawseti(L, -2, i + 1);
            }

            return true;
        }
    }

    return false;
}

static int32_t GetRecord(lua_State* L, const RecordKind& kind) {
    Entry* entry = DocArg(L, 1);
    uint32_t index;
    std::string error;

    if (!entry) {
        return PushNilError(L, "unknown document");
    }

    const ModelDoc& doc = entry->m_doc;

    if (!IndexArg(L, 2, index) || index >= doc.Count(kind.header)) {
        return PushNilError(L, "index out of range");
    }

    uint32_t pos = doc.RecordPos(kind.header, Stride(doc, kind), index);
    lua_createtable(L, 0, static_cast<int32_t>(kind.fieldCount + 4));
    int32_t table = lua_gettop(L);

    for (size_t i = 0; i < kind.fieldCount; i++) {
        const Field& field = kind.fields[i];

        if (field.modernOnly && !doc.IsModern()) {
            continue;
        }

        if (!PushField(L, doc, pos, field, error)) {
            lua_settop(L, table - 1);
            return PushNilError(L, error);
        }

        lua_setfield(L, table, field.name);
    }

    SetNumber(L, "index", index);

    if (kind.header == Model::HEADER_PARTICLES) {
        uint32_t flags = 0;
        uint16_t texture = 0;
        doc.Get(pos + PARTICLE_FLAGS, flags);
        doc.Get(pos + PARTICLE_TEXTURE, texture);
        bool packed = IsPackedGravity(doc, pos);
        SetBool(L, "gravityPacked", packed);
        SetBool(L, "multiTexture", IsMultiTexture(doc, flags));

        if (IsMultiTexture(doc, flags)) {
            lua_createtable(L, 3, 0);

            for (int32_t i = 0; i < 3; i++) {
                lua_pushnumber(L, (texture >> (5 * i)) & 0x1F);
                lua_rawseti(L, -2, i + 1);
            }

            lua_setfield(L, table, "textures");
        }

        if (packed) {
            Track vectors;

            if (!doc.ReadTrack(pos + PARTICLE_GRAVITY, ValueType::PackedGravity, vectors, error)) {
                lua_settop(L, table - 1);
                return PushNilError(L, error);
            }

            PushTrack(L, vectors, ValueType::Vec3);
            lua_setfield(L, table, "gravityVectors");
        }
    }

    return 1;
}

struct Pending {
    const Field* field = nullptr;
    double number = 0.0;
    float values[6] = {};
    std::string text;
    Track track;
    PartTrack part;
    std::vector<float> floats;
    std::vector<uint16_t> list;
};

static bool IntegerRange(Kind kind, double& low, double& high) {
    switch (kind) {
        case Kind::U32:
            low = 0;
            high = 4294967295.0;
            return true;
        case Kind::U16:
            low = 0;
            high = 65535;
            return true;
        case Kind::I32:
            low = -2147483648.0;
            high = 2147483647.0;
            return true;
        case Kind::I16:
            low = -32768;
            high = 32767;
            return true;
        case Kind::U8:
            low = 0;
            high = 255;
            return true;
        default:
            return false;
    }
}

// Reads the field value at the stack top into pending and checks it on its own.
static bool ParseField(lua_State* L, const ModelDoc& doc, uint32_t pos, const Field& field, Pending& pending, std::string& error) {
    pending.field = &field;
    int32_t top = lua_gettop(L);
    double low;
    double high;

    if (IntegerRange(field.kind, low, high)) {
        int64_t value;

        if (!ToInteger(L, top, low, high, value)) {
            error = "must be a whole number from " + std::to_string(static_cast<int64_t>(low)) + " to " + std::to_string(static_cast<int64_t>(high));
            return false;
        }

        pending.number = static_cast<double>(value);

        switch (field.check) {
            case Check::Bone:
                if (value >= doc.Count(Model::HEADER_BONES)) {
                    error = "bone index out of range";
                    return false;
                }

                break;
            case Check::Blend:
                if (value > 7) {
                    error = "blend mode must be 0 to 7";
                    return false;
                }

                break;
            case Check::Emitter:
                if (value < 1 || value > 3) {
                    error = "emitter type must be 1 plane, 2 sphere or 3 spline";
                    return false;
                }

                break;
            case Check::PowerOfTwo:
                if (value < 1 || value > 256 || (value & (value - 1))) {
                    error = "must be a power of two from 1 to 256";
                    return false;
                }

                break;
            case Check::Cells:
                if (value < 1 || value > 256) {
                    error = "must be 1 to 256";
                    return false;
                }

                break;
            default:
                break;
        }

        return true;
    }

    switch (field.kind) {
        case Kind::Float:
            if (!ToNumber(L, top, pending.number) || fabs(pending.number) > 1.0e7) {
                error = "must be a number";
                return false;
            }

            return true;
        case Kind::Vec2:
            return ReadVector(L, top, pending.values, 2, error);
        case Kind::Vec3:
            return ReadVector(L, top, pending.values, 3, error);
        case Kind::Range: {
            if (lua_type(L, top) != LUA_TTABLE) {
                error = "expected { min, max }";
                return false;
            }

            const char* names[2] = { "min", "max" };

            for (int32_t i = 0; i < 2; i++) {
                double value = 0.0;
                bool ok = PushMember(L, top, names[i], i + 1) && ToNumber(L, -1, value);

                if (ok) {
                    lua_pop(L, 1);
                }

                if (!ok || fabs(value) > 1.0e7) {
                    error = "expected { min, max } numbers";
                    return false;
                }

                pending.values[i] = static_cast<float>(value);
            }

            return true;
        }
        case Kind::Box: {
            if (lua_type(L, top) != LUA_TTABLE) {
                error = "expected { min = {x,y,z}, max = {x,y,z} }";
                return false;
            }

            const char* names[2] = { "min", "max" };

            for (int32_t i = 0; i < 2; i++) {
                if (!PushMember(L, top, names[i], i + 1)) {
                    error = std::string("missing ") + names[i];
                    return false;
                }

                bool ok = ReadVector(L, -1, pending.values + i * 3, 3, error);
                lua_pop(L, 1);

                if (!ok) {
                    return false;
                }
            }

            return true;
        }
        case Kind::String: {
            if (lua_type(L, top) != LUA_TSTRING) {
                error = "must be a string";
                return false;
            }

            std::string text = lua_tostring(L, top);

            if (text.find_first_not_of(" \t") != std::string::npos && !Files::NormalizeGamePath(text, pending.text, error)) {
                return false;
            }

            return true;
        }
        case Kind::AnimTrack: {
            uint16_t trackType = 0;
            uint16_t loop = 0xFFFF;
            doc.Get(pos + field.offset, trackType);
            doc.Get(pos + field.offset + 2, loop);

            if (!ReadTrackTable(L, top, field.value, trackType, loop, pending.track, error)) {
                return false;
            }

            return doc.ValidateTrack(field.value, pending.track, error);
        }
        case Kind::LifeTrack:
            if (!ReadPartTrackTable(L, top, field.value, pending.part, error)) {
                return false;
            }

            return doc.ValidatePartTrack(field.value, pending.part, field.needsKey, error);
        case Kind::Vec3Array: {
            if (lua_type(L, top) != LUA_TTABLE) {
                error = "expected a list of vectors";
                return false;
            }

            size_t count = lua_objlen(L, top);

            if (count > 1024) {
                error = "too many points";
                return false;
            }

            pending.floats.resize(count * 3);

            for (size_t i = 0; i < count; i++) {
                lua_rawgeti(L, top, static_cast<int32_t>(i + 1));
                bool ok = ReadVector(L, -1, &pending.floats[i * 3], 3, error);
                lua_pop(L, 1);

                if (!ok) {
                    return false;
                }
            }

            return true;
        }
        case Kind::U16Array: {
            if (lua_type(L, top) != LUA_TTABLE) {
                error = "expected a list of indexes";
                return false;
            }

            size_t count = lua_objlen(L, top);
            uint32_t limit = field.check == Check::TextureList ? doc.Count(Model::HEADER_TEXTURES) : doc.Count(Model::HEADER_MATERIALS);

            if (count > 256) {
                error = "too many entries";
                return false;
            }

            for (size_t i = 0; i < count; i++) {
                lua_rawgeti(L, top, static_cast<int32_t>(i + 1));
                int64_t value;
                bool ok = ToInteger(L, -1, 0, 65535, value) && value < limit;
                lua_pop(L, 1);

                if (!ok) {
                    error = field.check == Check::TextureList ? "texture index out of range" : "material index out of range";
                    return false;
                }

                pending.list.push_back(static_cast<uint16_t>(value));
            }

            return true;
        }
        case Kind::Fp69Pairs: {
            if (lua_type(L, top) != LUA_TTABLE) {
                error = "expected { {x,y}, {x,y} }";
                return false;
            }

            for (int32_t i = 0; i < 2; i++) {
                lua_rawgeti(L, top, i + 1);
                bool ok = ReadVector(L, -1, pending.values + i * 2, 2, error);
                lua_pop(L, 1);

                if (!ok) {
                    return false;
                }

                for (int32_t c = 0; c < 2; c++) {
                    if (fabsf(pending.values[i * 2 + c]) > 63.99f) {
                        error = "multi texture params are limited to plus or minus 64";
                        return false;
                    }
                }
            }

            return true;
        }
        default:
            break;
    }

    error = "unsupported field";
    return false;
}

static bool ApplyField(ModelDoc& doc, uint32_t pos, const std::string& owner, const Pending& pending, ValueType trackType, std::string& error) {
    const Field& field = *pending.field;
    uint32_t at = pos + field.offset;

    switch (field.kind) {
        case Kind::U32:
            return doc.Put(at, static_cast<uint32_t>(pending.number));
        case Kind::U16:
            return doc.Put(at, static_cast<uint16_t>(pending.number));
        case Kind::I32:
            return doc.Put(at, static_cast<int32_t>(pending.number));
        case Kind::I16:
            return doc.Put(at, static_cast<int16_t>(pending.number));
        case Kind::U8:
            return doc.Put(at, static_cast<uint8_t>(pending.number));
        case Kind::Float:
            return doc.Put(at, static_cast<float>(pending.number));
        case Kind::Vec2:
        case Kind::Range:
            return doc.Write(at, pending.values, 8);
        case Kind::Vec3:
            return doc.Write(at, pending.values, 12);
        case Kind::Box:
            return doc.Write(at, pending.values, 24);
        case Kind::String:
            return doc.WriteString(at, pending.text, owner + field.name, error);
        case Kind::AnimTrack:
            return doc.WriteTrack(at, trackType, pending.track, owner + field.name, error);
        case Kind::LifeTrack:
            return doc.WritePartTrack(at, field.value, pending.part, field.needsKey, owner + field.name, error);
        case Kind::Vec3Array:
            return doc.WriteVec3Array(at, pending.floats, owner + field.name, error);
        case Kind::U16Array:
            return doc.WriteU16Array(at, pending.list, owner + field.name, error);
        case Kind::Fp69Pairs: {
            int16_t raw[4];

            for (int32_t i = 0; i < 4; i++) {
                raw[i] = static_cast<int16_t>(std::clamp<long>(lroundf(pending.values[i] * 512.0f), -32767, 32767));
            }

            return doc.Write(at, raw, 8);
        }
    }

    return false;
}

static const Field* FindField(const RecordKind& kind, const char* name) {
    for (size_t i = 0; i < kind.fieldCount; i++) {
        if (!strcmp(kind.fields[i].name, name)) {
            return &kind.fields[i];
        }
    }

    return nullptr;
}

static const Pending* FindPending(const std::vector<Pending>& pending, const char* name) {
    for (const Pending& p : pending) {
        if (!strcmp(p.field->name, name)) {
            return &p;
        }
    }

    return nullptr;
}

// Applies a table of fields to one record after every field and the combination checked out.
static bool SetRecordFields(lua_State* L, int32_t table, ModelDoc& doc, const RecordKind& kind, uint32_t index, std::string& error) {
    if (lua_type(L, table) != LUA_TTABLE) {
        error = "expected a table of fields";
        return false;
    }

    bool particle = kind.header == Model::HEADER_PARTICLES;
    uint32_t pos = doc.RecordPos(kind.header, Stride(doc, kind), index);
    std::vector<Pending> pending;
    bool hasTextures = false;
    uint16_t packedTextures = 0;
    bool hasVectors = false;
    Track vectors;

    lua_pushnil(L);

    while (lua_next(L, table)) {
        if (lua_type(L, -2) != LUA_TSTRING) {
            lua_pop(L, 1);
            continue;
        }

        std::string key = lua_tostring(L, -2);

        if (key == "index" || key == "gravityPacked" || key == "multiTexture") {
            lua_pop(L, 1);
            continue;
        }

        if (particle && key == "textures") {
            hasTextures = true;

            for (int32_t i = 0; i < 3; i++) {
                int64_t value = 0;

                if (lua_type(L, -1) == LUA_TTABLE) {
                    lua_rawgeti(L, -1, i + 1);

                    if (!lua_isnil(L, -1) && !ToInteger(L, -1, 0, 31, value)) {
                        lua_pop(L, 3);
                        error = "textures entries must be 0 to 31";
                        return false;
                    }

                    lua_pop(L, 1);
                }

                if (value >= doc.Count(Model::HEADER_TEXTURES)) {
                    lua_pop(L, 2);
                    error = "texture index out of range";
                    return false;
                }

                packedTextures = static_cast<uint16_t>(packedTextures | (value << (5 * i)));
            }

            lua_pop(L, 1);
            continue;
        }

        if (particle && key == "gravityVectors") {
            uint16_t trackType = 0;
            uint16_t loop = 0xFFFF;
            doc.Get(pos + PARTICLE_GRAVITY, trackType);
            doc.Get(pos + PARTICLE_GRAVITY + 2, loop);

            if (!ReadTrackTable(L, -1, ValueType::PackedGravity, trackType, loop, vectors, error) || !doc.ValidateTrack(ValueType::PackedGravity, vectors, error)) {
                lua_pop(L, 2);
                error = "gravityVectors: " + error;
                return false;
            }

            hasVectors = true;
            lua_pop(L, 1);
            continue;
        }

        const Field* field = FindField(kind, key.c_str());

        if (!field || (field->modernOnly && !doc.IsModern())) {
            lua_pop(L, 2);
            error = "unknown field " + key;
            return false;
        }

        Pending entry;

        if (!ParseField(L, doc, pos, *field, entry, error)) {
            lua_pop(L, 2);
            error = key + ": " + error;
            return false;
        }

        pending.push_back(std::move(entry));
        lua_pop(L, 1);
    }

    if (particle) {
        uint32_t flags = 0;
        doc.Get(pos + PARTICLE_FLAGS, flags);
        bool packed = IsPackedGravity(doc, pos);
        const Pending* flagEdit = FindPending(pending, "flags");

        if (flagEdit && doc.IsModern()) {
            // The packed gravity bit follows the key data and is not taken from the table.
            uint32_t value = static_cast<uint32_t>(flagEdit->number);
            value = (value & ~Model::PARTICLE_FLAG_PACKED_GRAVITY) | (flags & Model::PARTICLE_FLAG_PACKED_GRAVITY);
            const_cast<Pending*>(flagEdit)->number = value;
            flags = value;
        } else if (flagEdit) {
            flags = static_cast<uint32_t>(flagEdit->number);
        }

        bool multi = IsMultiTexture(doc, flags);
        const Pending* textureEdit = FindPending(pending, "textureIndex");

        if (hasTextures && textureEdit) {
            error = "set either textures or textureIndex";
            return false;
        }

        if (hasTextures && !multi) {
            error = "textures needs the multi texture flag 0x10000000 on a modern model";
            return false;
        }

        uint16_t texture = 0;
        doc.Get(pos + PARTICLE_TEXTURE, texture);

        if (textureEdit) {
            texture = static_cast<uint16_t>(textureEdit->number);
        } else if (hasTextures) {
            texture = packedTextures;
        }

        uint32_t textureCount = doc.Count(Model::HEADER_TEXTURES);

        if (textureEdit || hasTextures || flagEdit) {
            uint32_t value = texture;
            bool ok = multi ? ((value & 0x1F) < textureCount && ((value >> 5) & 0x1F) < textureCount && ((value >> 10) & 0x1F) < textureCount && value < 0x8000) : value < textureCount;

            if (!ok) {
                error = "texture index out of range";
                return false;
            }
        }

        if (hasVectors && FindPending(pending, "gravityTrack")) {
            error = "set either gravityTrack or gravityVectors";
            return false;
        }

        if (hasVectors && !packed) {
            error = "gravityVectors needs an emitter with packed gravity";
            return false;
        }

        const Pending* emitterEdit = FindPending(pending, "emitterType");
        const Pending* splineEdit = FindPending(pending, "spline");

        if (emitterEdit || splineEdit) {
            uint8_t emitter = 0;
            doc.Get(pos + 0x29, emitter);

            if (emitterEdit) {
                emitter = static_cast<uint8_t>(emitterEdit->number);
            }

            size_t points = 0;

            if (splineEdit) {
                points = splineEdit->floats.size() / 3;
            } else {
                std::vector<float> spline;
                doc.ReadVec3Array(pos + 0x1C0, spline);
                points = spline.size() / 3;
            }

            if (emitter == 3 && points < 2) {
                error = "a spline emitter needs at least 2 spline points";
                return false;
            }
        }

        std::string owner = std::string(1, kind.prefix) + std::to_string(index) + ".";
        bool converted = false;

        for (const Pending& entry : pending) {
            if (!ApplyField(doc, pos, owner, entry, entry.field->value, error)) {
                return false;
            }

            converted = converted || (packed && entry.field->offset == PARTICLE_GRAVITY);
        }

        if (converted) {
            // Plain floats were written, so the client must stop expanding packed keys.
            doc.Get(pos + PARTICLE_FLAGS, flags);
            doc.Put(pos + PARTICLE_FLAGS, flags & ~Model::PARTICLE_FLAG_PACKED_GRAVITY);
        }

        if (hasTextures) {
            doc.Put(pos + PARTICLE_TEXTURE, texture);
        }

        if (hasVectors && !doc.WriteTrack(pos + PARTICLE_GRAVITY, ValueType::PackedGravity, vectors, owner + "gravityTrack", error)) {
            return false;
        }

        return true;
    }

    const Pending* texturesEdit = FindPending(pending, "textureIndices");
    const Pending* materialsEdit = FindPending(pending, "materialIndices");

    if (texturesEdit || materialsEdit) {
        std::vector<uint16_t> textures;
        std::vector<uint16_t> materials;
        doc.ReadU16Array(pos + 0x14, textures);
        doc.ReadU16Array(pos + 0x1C, materials);

        if (texturesEdit) {
            textures = texturesEdit->list;
        }

        if (materialsEdit) {
            materials = materialsEdit->list;
        }

        if (textures.empty() || materials.size() < textures.size()) {
            error = "a ribbon needs at least one texture and a material for every texture";
            return false;
        }
    }

    std::string owner = std::string(1, kind.prefix) + std::to_string(index) + ".";

    for (const Pending& entry : pending) {
        if (!ApplyField(doc, pos, owner, entry, entry.field->value, error)) {
            return false;
        }
    }

    return true;
}

static int32_t SetRecord(lua_State* L, const RecordKind& kind) {
    Entry* entry = DocArg(L, 1);
    uint32_t index;
    std::string error;

    if (!entry) {
        return PushFalseError(L, "unknown document");
    }

    if (!IndexArg(L, 2, index) || index >= entry->m_doc.Count(kind.header)) {
        return PushFalseError(L, "index out of range");
    }

    lua_settop(L, 3);

    if (!SetRecordFields(L, 3, entry->m_doc, kind, index, error)) {
        return PushFalseError(L, error);
    }

    Changed(entry);
    lua_pushboolean(L, 1);
    return 1;
}

// A track with one key, on sequence 0 when it is stored inline since other sequences fall back to slot 0, else on every inline sequence.
static Track ConstantTrack(const ModelDoc& doc, ValueType type, const float* value) {
    Track track;
    uint32_t sequences = doc.Count(Model::HEADER_SEQUENCES);
    uint32_t components = Model::ValueComponents(type);

    for (uint32_t i = 0; i < sequences; i++) {
        if (!doc.IsInlineSlot(0xFFFF, i)) {
            continue;
        }

        TrackKeys keys;
        keys.m_sequence = i;
        keys.m_times.push_back(0);
        keys.m_values.assign(value, value + components);
        track.m_sequences.push_back(keys);

        if (i == 0) {
            break;
        }
    }

    return track;
}

static PartTrack ThreeKeys(ValueType type, const float* a, const float* b, const float* c) {
    uint32_t components = Model::ValueComponents(type);
    PartTrack track;
    track.m_times = { 0.0f, 0.5f, 1.0f };
    track.m_values.insert(track.m_values.end(), a, a + components);
    track.m_values.insert(track.m_values.end(), b, b + components);
    track.m_values.insert(track.m_values.end(), c, c + components);
    return track;
}

static bool WriteParticleDefaults(ModelDoc& doc, uint32_t index, std::string& error) {
    uint32_t pos = doc.RecordPos(Model::HEADER_PARTICLES, doc.ParticleStride(), index);
    std::string owner = "p" + std::to_string(index) + ".";

    for (size_t i = 0; i < s_particles.fieldCount; i++) {
        if (s_particleFields[i].kind == Kind::AnimTrack) {
            doc.Put(pos + s_particleFields[i].offset + 2, static_cast<uint16_t>(0xFFFF));
        }
    }

    doc.Put(pos, 0xFFFFFFFFu);
    doc.Put(pos + 0x28, static_cast<uint8_t>(2));
    doc.Put(pos + 0x29, static_cast<uint8_t>(1));
    doc.Put(pos + 0x30, static_cast<uint16_t>(1));
    doc.Put(pos + 0x32, static_cast<uint16_t>(1));
    float twinkle[2] = { 1.0f, 1.0f };
    doc.Write(pos + 0x168, twinkle, 8);

    struct Default {
        uint32_t offset;
        float value;
    };

    const Default tracks[] = {
        { 0x34, 1.0f },
        { 0x48, 0.0f },
        { 0x5C, 0.0f },
        { 0x70, 0.0f },
        { 0x84, 0.0f },
        { 0x98, 1.0f },
        { 0xB0, 10.0f },
        { 0xC8, 1.0f },
        { 0xDC, 1.0f },
        { 0xF0, 0.0f },
    };

    for (const Default& entry : tracks) {
        if (!doc.WriteTrack(pos + entry.offset, ValueType::Float, ConstantTrack(doc, ValueType::Float, &entry.value), "", error)) {
            return false;
        }
    }

    float visible = 1.0f;

    if (!doc.WriteTrack(pos + 0x1C8, ValueType::UInt8, ConstantTrack(doc, ValueType::UInt8, &visible), "", error)) {
        return false;
    }

    float white[3] = { 255.0f, 255.0f, 255.0f };
    float alpha[3] = { 1.0f, 1.0f, 0.0f };
    float scale[2] = { 1.0f, 1.0f };
    float cell = 0.0f;

    return doc.WritePartTrack(pos + 0x104, ValueType::Vec3, ThreeKeys(ValueType::Vec3, white, white, white), true, "", error) && doc.WritePartTrack(pos + 0x114, ValueType::Fixed16, ThreeKeys(ValueType::Fixed16, &alpha[0], &alpha[1], &alpha[2]), true, "", error) && doc.WritePartTrack(pos + 0x124, ValueType::Vec2, ThreeKeys(ValueType::Vec2, scale, scale, scale), true, "", error) && doc.WritePartTrack(pos + 0x13C, ValueType::UInt16, ThreeKeys(ValueType::UInt16, &cell, &cell, &cell), false, "", error) && doc.WritePartTrack(pos + 0x14C, ValueType::UInt16, ThreeKeys(ValueType::UInt16, &cell, &cell, &cell), false, "", error);
}

static bool WriteRibbonDefaults(ModelDoc& doc, uint32_t index, std::string& error) {
    uint32_t pos = doc.RecordPos(Model::HEADER_RIBBONS, Model::RIBBON_STRIDE, index);

    for (size_t i = 0; i < s_ribbons.fieldCount; i++) {
        if (s_ribbonFields[i].kind == Kind::AnimTrack) {
            doc.Put(pos + s_ribbonFields[i].offset + 2, static_cast<uint16_t>(0xFFFF));
        }
    }

    doc.Put(pos, 0xFFFFFFFFu);
    doc.Put(pos + 0x74, 30.0f);
    doc.Put(pos + 0x78, 1.0f);
    doc.Put(pos + 0x80, static_cast<uint16_t>(1));
    doc.Put(pos + 0x82, static_cast<uint16_t>(1));

    float white[3] = { 1.0f, 1.0f, 1.0f };
    float one = 1.0f;
    float half = 0.5f;
    float zero = 0.0f;
    std::vector<uint16_t> first = { 0 };

    return doc.WriteU16Array(pos + 0x14, first, "", error) && doc.WriteU16Array(pos + 0x1C, first, "", error) && doc.WriteTrack(pos + 0x24, ValueType::Vec3, ConstantTrack(doc, ValueType::Vec3, white), "", error) && doc.WriteTrack(pos + 0x38, ValueType::Fixed16, ConstantTrack(doc, ValueType::Fixed16, &one), "", error) && doc.WriteTrack(pos + 0x4C, ValueType::Float, ConstantTrack(doc, ValueType::Float, &half), "", error) && doc.WriteTrack(pos + 0x60, ValueType::Float, ConstantTrack(doc, ValueType::Float, &half), "", error) && doc.WriteTrack(pos + 0x84, ValueType::UInt16, ConstantTrack(doc, ValueType::UInt16, &zero), "", error) && doc.WriteTrack(pos + 0x98, ValueType::UInt8, ConstantTrack(doc, ValueType::UInt8, &one), "", error);
}

static int32_t AddRecord(lua_State* L, const RecordKind& kind) {
    Entry* entry = DocArg(L, 1);
    std::string error;

    if (!entry) {
        return PushNilError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;
    uint32_t stride = Stride(doc, kind);
    uint32_t count = doc.Count(kind.header);
    bool copy = !lua_isnoneornil(L, 2);
    uint32_t source = 0;

    if (copy && (!IndexArg(L, 2, source) || source >= count)) {
        return PushNilError(L, "copy index out of range");
    }

    if (count >= 0xFFFF) {
        return PushNilError(L, "too many emitters");
    }

    if (!copy) {
        if (!doc.Count(Model::HEADER_BONES)) {
            return PushNilError(L, "the model has no bones to attach an emitter to");
        }

        if (!doc.Count(Model::HEADER_TEXTURES)) {
            return PushNilError(L, "add a texture first");
        }

        if (kind.header == Model::HEADER_RIBBONS && !doc.Count(Model::HEADER_MATERIALS)) {
            return PushNilError(L, "the model has no material for a ribbon");
        }
    }

    std::vector<uint8_t> records;

    if (!doc.CopyRecords(kind.header, stride, records)) {
        return PushNilError(L, "emitter list outside the file");
    }

    if (copy) {
        std::vector<uint8_t> record(records.begin() + static_cast<size_t>(source) * stride, records.begin() + static_cast<size_t>(source + 1) * stride);
        records.insert(records.end(), record.begin(), record.end());
    } else {
        records.resize(records.size() + stride, 0);
    }

    // The copy shares its array data with the source until one of them is edited, edits always write fresh arrays.
    if (!doc.ReplaceRecords(kind.header, records, count + 1, error)) {
        return PushNilError(L, error);
    }

    if (kind.header == Model::HEADER_PARTICLES) {
        std::string notes;
        doc.AdjustParticleChunks(count, -1, copy ? static_cast<int32_t>(source) : -1, notes);

        if (!notes.empty()) {
            LOG_WARN << "model editor: '" << doc.Path() << "'" << notes;
        }
    }

    bool ok = copy || (kind.header == Model::HEADER_PARTICLES ? WriteParticleDefaults(doc, count, error) : WriteRibbonDefaults(doc, count, error));

    if (!ok) {
        return PushNilError(L, error);
    }

    Changed(entry);
    lua_pushnumber(L, count);
    return 1;
}

static int32_t RemoveRecord(lua_State* L, const RecordKind& kind) {
    Entry* entry = DocArg(L, 1);
    uint32_t index;
    std::string error;

    if (!entry) {
        return PushFalseError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;
    uint32_t stride = Stride(doc, kind);
    uint32_t count = doc.Count(kind.header);

    if (!IndexArg(L, 2, index) || index >= count) {
        return PushFalseError(L, "index out of range");
    }

    std::vector<uint8_t> records;

    if (!doc.CopyRecords(kind.header, stride, records)) {
        return PushFalseError(L, "emitter list outside the file");
    }

    records.erase(records.begin() + static_cast<size_t>(index) * stride, records.begin() + static_cast<size_t>(index + 1) * stride);

    if (!doc.ReplaceRecords(kind.header, records, count - 1, error)) {
        return PushFalseError(L, error);
    }

    if (kind.header == Model::HEADER_PARTICLES) {
        std::string notes;
        doc.AdjustParticleChunks(count, static_cast<int32_t>(index), -1, notes);

        if (!notes.empty()) {
            LOG_WARN << "model editor: '" << doc.Path() << "'" << notes;
        }
    }

    Changed(entry);
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t Script_GetParticle(lua_State* L) {
    return GetRecord(L, s_particles);
}

static int32_t Script_SetParticle(lua_State* L) {
    return SetRecord(L, s_particles);
}

static int32_t Script_AddParticle(lua_State* L) {
    return AddRecord(L, s_particles);
}

static int32_t Script_RemoveParticle(lua_State* L) {
    return RemoveRecord(L, s_particles);
}

static int32_t Script_GetRibbon(lua_State* L) {
    return GetRecord(L, s_ribbons);
}

static int32_t Script_SetRibbon(lua_State* L) {
    return SetRecord(L, s_ribbons);
}

static int32_t Script_AddRibbon(lua_State* L) {
    return AddRecord(L, s_ribbons);
}

static int32_t Script_RemoveRibbon(lua_State* L) {
    return RemoveRecord(L, s_ribbons);
}

// Attachments.

static constexpr uint32_t kMaxAttachmentId = 255;

static uint32_t AttachmentId(const ModelDoc& doc, uint32_t index) {
    uint32_t id = 0;
    doc.Get(doc.RecordPos(Model::HEADER_ATTACHMENTS, Model::ATTACHMENT_STRIDE, index), id);
    return id;
}

// The id lookup the client resolves attachments through, the first attachment of an id wins.
static bool RebuildAttachmentLookup(ModelDoc& doc, std::string& error) {
    uint32_t count = doc.Count(Model::HEADER_ATTACHMENTS);
    std::vector<uint16_t> lookup;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t id = AttachmentId(doc, i);

        if (id > kMaxAttachmentId) {
            error = "attachment ids go from 0 to " + std::to_string(kMaxAttachmentId);
            return false;
        }

        if (lookup.size() <= id) {
            lookup.resize(id + 1, 0xFFFF);
        }

        if (lookup[id] == 0xFFFF) {
            lookup[id] = static_cast<uint16_t>(i);
        }
    }

    return doc.WriteU16Array(Model::HEADER_ATTACHMENT_LOOKUP, lookup, "attachmentLookup", error);
}

static uint32_t FreeAttachmentId(const ModelDoc& doc, uint32_t from) {
    uint32_t count = doc.Count(Model::HEADER_ATTACHMENTS);

    for (uint32_t id = from; id <= kMaxAttachmentId; id++) {
        bool used = false;

        for (uint32_t i = 0; i < count && !used; i++) {
            used = AttachmentId(doc, i) == id;
        }

        if (!used) {
            return id;
        }
    }

    return 0;
}

static int32_t Script_GetAttachment(lua_State* L) {
    return GetRecord(L, s_attachments);
}

// ModelEditor_SetAttachment(doc, index, { attachmentId, boneIndex, position, animateTrack }) -> ok, err.
static int32_t Script_SetAttachment(lua_State* L) {
    if (lua_type(L, 3) == LUA_TTABLE) {
        lua_getfield(L, 3, "attachmentId");
        bool bad = !lua_isnil(L, -1) && (!lua_isnumber(L, -1) || lua_tonumber(L, -1) < 0 || lua_tonumber(L, -1) > kMaxAttachmentId);
        lua_pop(L, 1);

        if (bad) {
            return PushFalseError(L, "attachment ids go from 0 to " + std::to_string(kMaxAttachmentId));
        }
    }

    int32_t results = SetRecord(L, s_attachments);
    Entry* entry = DocArg(L, 1);
    std::string error;

    if (entry && lua_toboolean(L, -1) && !RebuildAttachmentLookup(entry->m_doc, error)) {
        lua_pop(L, results);
        return PushFalseError(L, error);
    }

    return results;
}

// ModelEditor_AddAttachment(doc [, copyIndex] [, id]) -> new index, a copy keeps everything but takes a free id.
static int32_t Script_AddAttachment(lua_State* L) {
    Entry* entry = DocArg(L, 1);
    std::string error;

    if (!entry) {
        return PushNilError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;
    uint32_t count = doc.Count(Model::HEADER_ATTACHMENTS);
    bool copy = !lua_isnoneornil(L, 2);
    uint32_t source = 0;
    int64_t wanted = -1;

    if (copy && (!IndexArg(L, 2, source) || source >= count)) {
        return PushNilError(L, "copy index out of range");
    }

    if (!lua_isnoneornil(L, 3) && !ToInteger(L, 3, 0, kMaxAttachmentId, wanted)) {
        return PushNilError(L, "attachment ids go from 0 to " + std::to_string(kMaxAttachmentId));
    }

    if (!doc.Count(Model::HEADER_BONES)) {
        return PushNilError(L, "the model has no bones to attach to");
    }

    std::vector<uint8_t> records;

    if (!doc.CopyRecords(Model::HEADER_ATTACHMENTS, Model::ATTACHMENT_STRIDE, records)) {
        return PushNilError(L, "attachment list outside the file");
    }

    uint32_t id = wanted >= 0 ? static_cast<uint32_t>(wanted) : FreeAttachmentId(doc, copy ? AttachmentId(doc, source) : 0);

    if (copy) {
        std::vector<uint8_t> record(records.begin() + static_cast<size_t>(source) * Model::ATTACHMENT_STRIDE, records.begin() + static_cast<size_t>(source + 1) * Model::ATTACHMENT_STRIDE);
        records.insert(records.end(), record.begin(), record.end());
    } else {
        records.resize(records.size() + Model::ATTACHMENT_STRIDE, 0);
    }

    if (!doc.ReplaceRecords(Model::HEADER_ATTACHMENTS, records, count + 1, error)) {
        return PushNilError(L, error);
    }

    uint32_t pos = doc.RecordPos(Model::HEADER_ATTACHMENTS, Model::ATTACHMENT_STRIDE, count);
    doc.Put(pos, id);

    if (!copy) {
        float one = 1.0f;
        doc.Put(pos + 0x14 + 2, static_cast<uint16_t>(0xFFFF));

        if (!doc.WriteTrack(pos + 0x14, ValueType::UInt8, ConstantTrack(doc, ValueType::UInt8, &one), "", error)) {
            return PushNilError(L, error);
        }
    }

    if (!RebuildAttachmentLookup(doc, error)) {
        return PushNilError(L, error);
    }

    Changed(entry);
    lua_pushnumber(L, count);
    return 1;
}

static int32_t Script_RemoveAttachment(lua_State* L) {
    Entry* entry = DocArg(L, 1);
    uint32_t index;
    std::string error;

    if (!entry) {
        return PushFalseError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;
    uint32_t count = doc.Count(Model::HEADER_ATTACHMENTS);

    if (!IndexArg(L, 2, index) || index >= count) {
        return PushFalseError(L, "index out of range");
    }

    std::vector<uint8_t> records;

    if (!doc.CopyRecords(Model::HEADER_ATTACHMENTS, Model::ATTACHMENT_STRIDE, records)) {
        return PushFalseError(L, "attachment list outside the file");
    }

    records.erase(records.begin() + static_cast<size_t>(index) * Model::ATTACHMENT_STRIDE, records.begin() + static_cast<size_t>(index + 1) * Model::ATTACHMENT_STRIDE);

    if (!doc.ReplaceRecords(Model::HEADER_ATTACHMENTS, records, count - 1, error) || !RebuildAttachmentLookup(doc, error)) {
        return PushFalseError(L, error);
    }

    Changed(entry);
    lua_pushboolean(L, 1);
    return 1;
}

// Preview.

// ModelEditor_GetPreviewPath(doc) -> a virtual path of the current bytes, a new one per revision.
static float OptFloat(lua_State* L, int32_t index, float fallback) {
    return lua_isnumber(L, index) ? static_cast<float>(lua_tonumber(L, index)) : fallback;
}

// ModelEditor_SetPreviewView(modelFrame, yaw, pitch, zoom, panX, panY) -> true once the model is loaded and framed.
// Orbits a perspective camera around the bounding box, the frame's own fallback is a flat screen projection.
static int32_t Script_SetPreviewView(lua_State* L) {
    CSimpleModel* frame = static_cast<CSimpleModel*>(FrameScript_GetObjectThis(L, CSimpleModel::GetObjectType()));
    float yaw = OptFloat(L, 2, 0.0f);
    float pitch = OptFloat(L, 3, 0.25f);
    float zoom = OptFloat(L, 4, 1.0f);
    float panX = OptFloat(L, 5, 0.0f);
    float panY = OptFloat(L, 6, 0.0f);

    if (!frame || !frame->m_model || !frame->m_model->IsLoaded(0, 0)) {
        lua_pushboolean(L, false);
        return 1;
    }

    pitch = std::clamp(pitch, -1.5f, 1.5f);
    zoom = std::clamp(zoom, 0.02f, 50.0f);

    float scale = frame->m_modelScale * frame->m_layoutScale * NDCToDDCHeight(1.0f) * 1.6666666f;
    CAaBox box = frame->m_model->GetBoundingBox();
    C3Vector size(box.t.x - box.b.x, box.t.y - box.b.y, box.t.z - box.b.z);
    float radius = 0.5f * sqrtf(size.x * size.x + size.y * size.y + size.z * size.z);

    if (!(radius > 0.001f) || !std::isfinite(radius)) {
        box.b = C3Vector(-1.0f, -1.0f, -1.0f);
        box.t = C3Vector(1.0f, 1.0f, 1.0f);
        radius = 1.732f;
    }

    radius *= scale;
    float cosFacing = cosf(frame->m_facing);
    float sinFacing = sinf(frame->m_facing);
    C3Vector local((box.b.x + box.t.x) * 0.5f * scale, (box.b.y + box.t.y) * 0.5f * scale, (box.b.z + box.t.z) * 0.5f * scale);
    C3Vector center(local.x * cosFacing - local.y * sinFacing + frame->m_position.x * frame->m_layoutScale,
        local.x * sinFacing + local.y * cosFacing + frame->m_position.y * frame->m_layoutScale, local.z + frame->m_position.z * frame->m_layoutScale);

    const float fov = 0.7853982f;
    float distance = radius / sinf(fov * 0.5f) * zoom;
    C3Vector forward(cosf(pitch) * cosf(yaw), cosf(pitch) * sinf(yaw), sinf(pitch));
    C3Vector right(-sinf(yaw), cosf(yaw), 0.0f);
    C3Vector up(-sinf(pitch) * cosf(yaw), -sinf(pitch) * sinf(yaw), cosf(pitch));
    C3Vector target(center.x + (right.x * panX + up.x * panY) * radius, center.y + (right.y * panX + up.y * panY) * radius, center.z + (up.z * panY) * radius);
    C3Vector eye(target.x + forward.x * distance, target.y + forward.y * distance, target.z + forward.z * distance);

    static CCamera* s_camera = nullptr;

    if (!s_camera) {
        s_camera = CameraCreate();
    }

    if (!s_camera) {
        lua_pushboolean(L, false);
        return 1;
    }

    if (frame->m_camera != s_camera) {
        frame->SetCamera(s_camera);
    }

    CameraSetFloat(s_camera, 4, fov);
    CameraSetFloat(s_camera, 3, std::max(distance * 0.02f, distance - radius * 4.0f));
    CameraSetFloat(s_camera, 2, distance + radius * 8.0f);
    CameraSetFloat(s_camera, 5, 0.0f);
    CameraSetFloat(s_camera, 6, 0.0f);
    CameraSetCoord(s_camera, 7, eye);
    CameraSetCoord(s_camera, 8, target);
    lua_pushboolean(L, true);
    return 1;
}

// ModelEditor_ClearPreviewView(modelFrame) drops the orbit camera so SetCamera(index) can take over.
static int32_t Script_ClearPreviewView(lua_State* L) {
    CSimpleModel* frame = static_cast<CSimpleModel*>(FrameScript_GetObjectThis(L, CSimpleModel::GetObjectType()));

    if (frame && frame->m_camera) {
        frame->SetCamera(nullptr);
    }

    return 0;
}

static int32_t Script_GetPreviewPath(lua_State* L) {
    Entry* entry = DocArg(L, 1);

    if (!entry) {
        return PushNilError(L, "unknown document");
    }

    ModelDoc& doc = entry->m_doc;

    if (entry->m_publishedRevision == doc.Revision() && !entry->m_previewPath.empty()) {
        lua_pushstring(L, entry->m_previewPath.c_str());
        return 1;
    }

    // CM2Cache keys models by file name alone, so the revision is part of the file name too.
    uint32_t revision = s_nextPreview++;
    std::string dir = std::string(Resources::kPreviewPrefix) + std::to_string(revision) + "\\";
    std::string base = dir + Files::FileStem(doc.Path()) + "_p" + std::to_string(revision);
    std::string path = base + ".m2";
    Resources::AddPreviewFile(path, std::make_shared<const std::vector<uint8_t>>(doc.Build()), doc.Path());
    Resources::AddPreviewRedirect(base, Files::StripExtension(doc.Path()));
    entry->m_previewDirs.push_back(dir);

    while (entry->m_previewDirs.size() > kPreviewsKept) {
        Resources::RemovePreviews(entry->m_previewDirs.front());
        entry->m_previewDirs.pop_front();
    }

    entry->m_publishedRevision = doc.Revision();
    entry->m_previewPath = path;
    lua_pushstring(L, path.c_str());
    return 1;
}

// Saving.

// Drops cached shared models of that file name from the CM2Cache hash so new loads read the new file, models in use keep theirs.
static uint32_t DetachCachedModels(const std::string& gamePath) {
    CM2Scene* scene = World::s_m2Scene;

    if (!scene || !scene->m_cache) {
        return 0;
    }

    CM2Cache* cache = scene->m_cache;
    std::string name = Files::Lower(FileName(gamePath));
    std::vector<CM2Shared*> found;

    for (CM2Shared* bucket : cache->m_shared) {
        for (CM2Shared* shared = bucket; shared; shared = shared->m_next) {
            if (shared->m_fileNameWithoutPath && name == shared->m_fileNameWithoutPath) {
                found.push_back(shared);
            }
        }
    }

    for (CM2Shared* shared : found) {
        if (shared->m_previous) {
            *shared->m_previous = shared->m_next;
        }

        if (shared->m_next) {
            shared->m_next->m_previous = shared->m_previous;
        }

        shared->m_previous = nullptr;
        shared->m_next = nullptr;
    }

    return static_cast<uint32_t>(found.size());
}

struct Output {
    std::string path;
    std::vector<uint8_t> data;
};

static bool OptionBool(lua_State* L, int32_t table, const char* key) {
    if (lua_type(L, table) != LUA_TTABLE) {
        return false;
    }

    lua_getfield(L, table, key);
    bool value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

static std::string OptionString(lua_State* L, int32_t table, const char* key) {
    if (lua_type(L, table) != LUA_TTABLE) {
        return std::string();
    }

    lua_getfield(L, table, key);
    std::string value = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);
    return value;
}

// ModelEditor_Save(doc, destPath or nil, { overwrite, copyTextures, textureFolder }) -> ok, message.
static int32_t Script_Save(lua_State* L) {
    Entry* entry = DocArg(L, 1);
    std::string error;

    if (!entry) {
        return PushFalseError(L, "unknown document");
    }

    lua_settop(L, 3);
    ModelDoc& doc = entry->m_doc;
    std::string dest = doc.Path();
    bool copy = false;

    if (lua_type(L, 2) == LUA_TSTRING) {
        if (!NormalizeModelPath(lua_tostring(L, 2), dest, error)) {
            return PushFalseError(L, error);
        }

        copy = Files::Lower(dest) != Files::Lower(doc.Path());
    } else if (!lua_isnil(L, 2)) {
        return PushFalseError(L, "destination must be a path or nil");
    } else if (!Files::IsLooseFile(doc.Path())) {
        return PushFalseError(L, doc.Path() + " is not a loose file in the game folder, save it with a destination path");
    }

    bool overwrite = OptionBool(L, 3, "overwrite");
    bool copyTextures = OptionBool(L, 3, "copyTextures");
    std::string textureFolder = OptionString(L, 3, "textureFolder");

    if (copy && !overwrite && (Files::GameFileExists(dest) || Files::IsLooseFile(dest))) {
        return PushFalseError(L, dest + " already exists, pass overwrite to replace it");
    }

    if (!textureFolder.empty() && !Files::NormalizeGamePath(textureFolder, textureFolder, error)) {
        return PushFalseError(L, "textureFolder: " + error);
    }

    if (textureFolder.empty()) {
        textureFolder = Files::Folder(dest);
    }

    ModelDoc work = doc;
    std::vector<Output> outputs;
    std::string notes;
    bool edited = copy;

    if (copy) {
        if (!work.WriteString(Model::HEADER_NAME, Files::FileStem(dest), "name", error)) {
            return PushFalseError(L, error);
        }

        std::string sourceBase = Files::StripExtension(doc.Path());
        std::string destBase = Files::StripExtension(dest);
        uint32_t profiles = (std::min)(work.Count(Model::HEADER_SKIN_PROFILES), kMaxSkinProfiles);
        uint32_t shared = 0;

        for (uint32_t i = 0; i < profiles; i++) {
            char suffix[16];
            snprintf(suffix, sizeof(suffix), "%02u.skin", i);
            Output output;

            if (Files::ReadGameFile(sourceBase + suffix, output.data)) {
                output.path = destBase + suffix;
                outputs.push_back(std::move(output));
            } else if (work.IsModern() && work.SkinFileDataId(i)) {
                shared++;
            } else {
                notes += " Missing skin " + sourceBase + suffix + ".";
            }
        }

        uint32_t sequences = work.Count(Model::HEADER_SEQUENCES);
        uint32_t sharedAnims = 0;

        for (uint32_t i = 0; i < sequences; i++) {
            uint32_t pos = work.RecordPos(Model::HEADER_SEQUENCES, Model::SEQUENCE_STRIDE, i);
            uint16_t id = 0;
            uint16_t variation = 0;
            uint32_t flags = 0;
            work.Get(pos, id);
            work.Get(pos + 2, variation);
            work.Get(pos + 0x0C, flags);

            if (flags & (Model::SEQUENCE_FLAG_INLINE | Model::SEQUENCE_FLAG_ALIAS)) {
                continue;
            }

            char suffix[32];
            snprintf(suffix, sizeof(suffix), "%04u-%02u.anim", id, variation);
            Output output;

            if (Files::ReadGameFile(sourceBase + suffix, output.data)) {
                output.path = destBase + suffix;
                outputs.push_back(std::move(output));
            } else if (work.HasAnimFileDataIds()) {
                sharedAnims++;
            } else {
                notes += " Missing anim " + sourceBase + suffix + ".";
            }
        }

        if (shared || sharedAnims) {
            notes += " The copy shares " + std::to_string(shared) + " skins and " + std::to_string(sharedAnims) + " anims with the original through file data ids.";
        }
    }

    if (copyTextures) {
        std::vector<std::pair<std::string, std::string>> placed;
        uint32_t count = work.Count(Model::HEADER_TEXTURES);

        for (uint32_t i = 0; i < count; i++) {
            uint32_t type = 0;
            work.Get(work.RecordPos(Model::HEADER_TEXTURES, Model::TEXTURE_STRIDE, i), type);
            uint32_t fileDataId = 0;
            std::string source = TexturePath(work, i, fileDataId);

            if (type != 0 || source.empty()) {
                continue;
            }

            std::string target = textureFolder.empty() ? FileName(source) : textureFolder + "\\" + FileName(source);

            if (Files::Lower(target) == Files::Lower(source)) {
                continue;
            }

            bool clash = false;
            bool done = false;

            for (const auto& pair : placed) {
                if (Files::Lower(pair.first) == Files::Lower(target)) {
                    clash = Files::Lower(pair.second) != Files::Lower(source);
                    done = !clash;
                }
            }

            if (clash) {
                notes += " Skipped " + source + " since another texture has the same file name.";
                continue;
            }

            if (!done) {
                Output output;

                if (!Files::ReadGameFile(source, output.data)) {
                    notes += " Could not read " + source + ".";
                    continue;
                }

                output.path = target;
                outputs.push_back(std::move(output));
                placed.push_back({ target, source });
            }

            if (!ApplyTexturePath(work, i, target, error)) {
                return PushFalseError(L, error);
            }

            edited = true;
        }
    }

    Output model;
    model.path = dest;
    model.data = work.Build();
    outputs.push_back(std::move(model));

    for (const Output& output : outputs) {
        if (!Files::WriteLoose(output.path, output.data.data(), output.data.size(), error)) {
            Files::RefreshLooseIndex();
            return PushFalseError(L, error);
        }
    }

    Files::RefreshLooseIndex();
    uint32_t detached = DetachCachedModels(dest);

    work.SetPath(dest);
    doc = std::move(work);

    if (edited) {
        doc.MarkChanged();
    }

    doc.MarkSaved();

    std::string message = "Saved " + dest + " (" + std::to_string(outputs.size()) + " files).";

    if (detached) {
        message += " New loads read the saved file, models already shown keep the old one.";
    }

    message += notes;
    Lua::SignalEvent("MODEL_EDITOR_SAVED", "%d%s", entry->m_id, dest.c_str());
    lua_pushboolean(L, 1);
    lua_pushstring(L, message.c_str());
    return 2;
}

// ModelEditor_FileExists(path) -> true when the client can open it.
static int32_t Script_FileExists(lua_State* L) {
    std::string path;
    std::string error;
    bool exists = false;

    if (lua_type(L, 1) == LUA_TSTRING && Files::NormalizeGamePath(lua_tostring(L, 1), path, error)) {
        exists = Files::GameFileExists(path);

        if (!exists && (Files::EndsWith(path, ".mdx") || Files::EndsWith(path, ".mdl"))) {
            exists = Files::GameFileExists(path.substr(0, path.size() - 4) + ".m2");
        }
    }

    lua_pushboolean(L, exists ? 1 : 0);
    return 1;
}

static std::wstring ImportDirectory() {
    return Files::DiskPath("Import");
}

// ModelEditor_ListImportFiles() -> { "name.png", ... } from the Import folder.
static int32_t Script_ListImportFiles(lua_State* L) {
    std::vector<std::string> names;
    std::error_code error;

    for (std::filesystem::directory_iterator it(std::filesystem::path(ImportDirectory()), error); !error && it != std::filesystem::directory_iterator(); it.increment(error)) {
        std::wstring name = it->path().filename().wstring();

        if (!it->is_directory(error) && TextureImport::IsImportExtension(name)) {
            names.push_back(Files::Narrow(name));
        }
    }

    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
    lua_createtable(L, static_cast<int32_t>(names.size()), 0);

    for (size_t i = 0; i < names.size(); i++) {
        lua_pushstring(L, names[i].c_str());
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    return 1;
}

// ModelEditor_ImportTexture(sourceName, destPath) -> ok, message or false, err.
static int32_t Script_ImportTexture(lua_State* L) {
    if (lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        return PushFalseError(L, "expected a source name and a destination path");
    }

    std::string sourceName = lua_tostring(L, 1);
    std::string dest;
    std::string error;
    std::wstring source;

    if (Platform::IsAbsoluteHostPath(sourceName)) {
        source = Files::Widen(sourceName);
    } else if (sourceName.find_first_of("\\/") != std::string::npos || sourceName.find("..") != std::string::npos || sourceName.empty()) {
        return PushFalseError(L, "source must be a file name from the Import folder or an absolute path");
    } else {
        source = (std::filesystem::path(ImportDirectory()) / Files::Widen(sourceName)).wstring();
    }

    if (!TextureImport::IsImportExtension(source)) {
        return PushFalseError(L, "source must be png, jpg, jpeg, bmp, tga or blp");
    }

    if (!Files::NormalizeGamePath(lua_tostring(L, 2), dest, error)) {
        return PushFalseError(L, error);
    }

    if (Files::StripExtension(dest) == dest) {
        dest += ".blp";
    }

    if (!Files::EndsWith(dest, ".blp")) {
        return PushFalseError(L, "destination must be a .blp path");
    }

    std::vector<uint8_t> blp;
    std::string message;

    if (!TextureImport::ConvertFile(source, blp, message, error) || !Files::WriteLoose(dest, blp.data(), blp.size(), error)) {
        return PushFalseError(L, error);
    }

    Files::RefreshLooseIndex();
    message = "Wrote " + dest + ", " + message;
    lua_pushboolean(L, 1);
    lua_pushstring(L, message.c_str());
    return 2;
}

// ModelEditor_GetTextureInfo(path) -> { width, height, format, alphaBits, mips } or nil.
static int32_t Script_GetTextureInfo(lua_State* L) {
    std::string path;
    std::string error;
    TextureImport::BlpInfo info;

    if (lua_type(L, 1) != LUA_TSTRING || !Files::NormalizeGamePath(lua_tostring(L, 1), path, error) || !TextureImport::ReadInfo(path, info)) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 5);
    SetNumber(L, "width", info.m_width);
    SetNumber(L, "height", info.m_height);
    SetString(L, "format", info.m_format);
    SetNumber(L, "alphaBits", info.m_alphaBits);
    SetNumber(L, "mips", info.m_mips);
    return 1;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!HasAccess()) {
        return 0;
    }

    return Function(L);
}

void Apply() {
    Lua::RegisterFunction("ModelEditor_Open", Gated<Script_Open>);
    Lua::RegisterFunction("ModelEditor_SetPreviewView", Gated<Script_SetPreviewView>);
    Lua::RegisterFunction("ModelEditor_ClearPreviewView", Gated<Script_ClearPreviewView>);
    Lua::RegisterFunction("ModelEditor_Close", Gated<Script_Close>);
    Lua::RegisterFunction("ModelEditor_GetInfo", Gated<Script_GetInfo>);
    Lua::RegisterFunction("ModelEditor_GetDocs", Gated<Script_GetDocs>);
    Lua::RegisterFunction("ModelEditor_SetTexture", Gated<Script_SetTexture>);
    Lua::RegisterFunction("ModelEditor_AddTexture", Gated<Script_AddTexture>);
    Lua::RegisterFunction("ModelEditor_SetTextureCombo", Gated<Script_SetTextureCombo>);
    Lua::RegisterFunction("ModelEditor_SetMaterial", Gated<Script_SetMaterial>);
    Lua::RegisterFunction("ModelEditor_GetParticle", Gated<Script_GetParticle>);
    Lua::RegisterFunction("ModelEditor_SetParticle", Gated<Script_SetParticle>);
    Lua::RegisterFunction("ModelEditor_AddParticle", Gated<Script_AddParticle>);
    Lua::RegisterFunction("ModelEditor_RemoveParticle", Gated<Script_RemoveParticle>);
    Lua::RegisterFunction("ModelEditor_GetRibbon", Gated<Script_GetRibbon>);
    Lua::RegisterFunction("ModelEditor_SetRibbon", Gated<Script_SetRibbon>);
    Lua::RegisterFunction("ModelEditor_AddRibbon", Gated<Script_AddRibbon>);
    Lua::RegisterFunction("ModelEditor_RemoveRibbon", Gated<Script_RemoveRibbon>);
    Lua::RegisterFunction("ModelEditor_GetPreviewPath", Gated<Script_GetPreviewPath>);
    Lua::RegisterFunction("ModelEditor_Save", Gated<Script_Save>);
    Lua::RegisterFunction("ModelEditor_FileExists", Gated<Script_FileExists>);
    Lua::RegisterFunction("ModelEditor_ListImportFiles", Gated<Script_ListImportFiles>);
    Lua::RegisterFunction("ModelEditor_ImportTexture", Gated<Script_ImportTexture>);
    Lua::RegisterFunction("ModelEditor_GetTextureInfo", Gated<Script_GetTextureInfo>);
    Lua::RegisterFunction("ModelEditor_GetAttachment", Gated<Script_GetAttachment>);
    Lua::RegisterFunction("ModelEditor_SetAttachment", Gated<Script_SetAttachment>);
    Lua::RegisterFunction("ModelEditor_AddAttachment", Gated<Script_AddAttachment>);
    Lua::RegisterFunction("ModelEditor_RemoveAttachment", Gated<Script_RemoveAttachment>);
    PreviewAttach::Apply(&HasAccess);
}

void OnGameDestroy() {
    PreviewAttach::OnGameDestroy();
    s_animExists.clear();

    for (auto& pair : s_docs) {
        DropPreviews(pair.second.get());
    }

    s_docs.clear();
}

}
