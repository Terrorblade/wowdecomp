#include "HoT/Editor/Map/WmoDoodadEdit.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/C4Quaternion.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "HoT/Editor/Map/EditorSave.h"
#include "WoW/Source/WorldClient/CMapBaseObjLink.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapDoodadDef.h"
#include "WoW/Source/WorldClient/MapObj.h"
#include "WoW/Source/WorldClient/MapObjDef.h"
#include "WoW/Source/WorldClient/MapObjGroup.h"

namespace HoT::Editor::Map::WmoDoodadEdit {

static const uint32_t TOKEN_MOHD = 0x4D4F4844;
static const uint32_t TOKEN_MODD = 0x4D4F4444;
static const uint32_t TOKEN_MOGP = 0x4D4F4750;
static const uint32_t TOKEN_MODR = 0x4D4F4452;
static const uint32_t GROUP_HEADER_SIZE = 0x44;
static const float s_degToRad = 0.017453292f;
static const float s_pi = 3.1415927f;

struct Handle {
    uint32_t m_mapObjUniqueId;
    uint16_t m_doodadRef;
};

struct FileEdits {
    std::string m_path;
    std::map<uint16_t, SMODoodadDef> m_poses;
    std::set<uint16_t> m_removed;
    // MODR as it was before the first save removed anything, so undo can still save a doodad back.
    std::map<uint32_t, std::vector<uint16_t>> m_groupRefs;
    bool m_dirty = false;
};

static std::vector<Handle> s_handles;
static std::map<std::string, FileEdits> s_files;

static std::string FileKey(const char* path) {
    std::string key = path;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(c == '/' ? '\\' : std::tolower(c)); });
    return key;
}

static FileEdits& EditsFor(CMapObj* mapObj) {
    FileEdits& file = s_files[FileKey(mapObj->m_wmoName)];
    file.m_path = mapObj->m_wmoName;
    file.m_dirty = true;
    return file;
}

static CMapObjDef* FindMapObjDef(uint32_t uniqueId) {
    return CMap::mapObjDefHash.Ptr(uniqueId, CMap::s_mapObjDefHashKey);
}

static CMapDoodadDef* FindLive(CMapObjDef* mapObjDef, uint16_t doodadRef) {
    HASHKEY_DWORD key = { mapObjDef->m_hashval + 1 };
    return CMap::doodadDefHash.Ptr(doodadRef, key);
}

static bool Lookup(uint32_t handle, CMapObjDef*& mapObjDef, uint16_t& doodadRef) {
    if (handle >= s_handles.size()) {
        return false;
    }

    mapObjDef = FindMapObjDef(s_handles[handle].m_mapObjUniqueId);
    doodadRef = s_handles[handle].m_doodadRef;
    return mapObjDef && mapObjDef->m_owner && mapObjDef->m_owner->m_isGroupLoaded && doodadRef < mapObjDef->m_owner->m_doodadDefCount;
}

template <typename Func>
static void ForEachPlacement(CMapObj* mapObj, Func func) {
    for (CMapObjDef* def = CMap::mapObjDefHash.Head(); def; def = CMap::mapObjDefHash.Next(def)) {
        if (def->m_owner && _stricmp(def->m_owner->m_wmoName, mapObj->m_wmoName) == 0) {
            func(def);
        }
    }
}

static C44Matrix BuildLocal(const SMODoodadDef& def, float scale) {
    C44Matrix matrix;
    matrix.Translate(def.m_position);
    matrix.Rotate(def.m_orientation);

    if (scale != 1.0f) {
        matrix.Scale(scale);
    }

    return matrix;
}

static C4Quaternion ToQuaternion(const C44Matrix& m) {
    C4Quaternion q;
    float trace = m.a0 + m.b1 + m.c2;

    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        q.w = s * 0.25f;
        q.x = (m.b2 - m.c1) / s;
        q.y = (m.c0 - m.a2) / s;
        q.z = (m.a1 - m.b0) / s;
    } else if (m.a0 > m.b1 && m.a0 > m.c2) {
        float s = sqrtf(1.0f + m.a0 - m.b1 - m.c2) * 2.0f;
        q.w = (m.b2 - m.c1) / s;
        q.x = s * 0.25f;
        q.y = (m.a1 + m.b0) / s;
        q.z = (m.c0 + m.a2) / s;
    } else if (m.b1 > m.c2) {
        float s = sqrtf(1.0f + m.b1 - m.a0 - m.c2) * 2.0f;
        q.w = (m.c0 - m.a2) / s;
        q.x = (m.a1 + m.b0) / s;
        q.y = s * 0.25f;
        q.z = (m.b2 + m.c1) / s;
    } else {
        float s = sqrtf(1.0f + m.c2 - m.a0 - m.b1) * 2.0f;
        q.w = (m.a1 - m.b0) / s;
        q.x = (m.c0 + m.a2) / s;
        q.y = (m.b2 + m.c1) / s;
        q.z = s * 0.25f;
    }

    return q;
}

// Inverse of ObjectEdit's X * Y * Z placement rotation.
static C3Vector ToEuler(const C44Matrix& m) {
    float cosY = sqrtf(m.b2 * m.b2 + m.c2 * m.c2);
    float x = atan2f(-m.a2, cosY);
    float z;
    float y;

    if (cosY > 0.0001f) {
        y = atan2f(m.a1, m.a0);
        z = atan2f(m.b2, m.c2);
    } else {
        y = atan2f(-m.b0, m.b1);
        z = 0.0f;
    }

    return C3Vector(x / s_degToRad, (y - s_pi) / s_degToRad, z / s_degToRad);
}

static C44Matrix BuildWorldRotation(const C3Vector& world, const C3Vector& rotation) {
    C44Matrix matrix;
    matrix.d0 = world.x;
    matrix.d1 = world.y;
    matrix.d2 = world.z;
    matrix.RotateAroundZ(rotation.y * s_degToRad + s_pi);
    matrix.RotateAroundY(rotation.x * s_degToRad);
    matrix.RotateAroundX(rotation.z * s_degToRad);
    return matrix;
}

uint32_t GetHandle(uint32_t mapObjUniqueId, uint16_t doodadRef) {
    for (uint32_t i = 0; i < s_handles.size(); i++) {
        if (s_handles[i].m_mapObjUniqueId == mapObjUniqueId && s_handles[i].m_doodadRef == doodadRef) {
            return i;
        }
    }

    s_handles.push_back({ mapObjUniqueId, doodadRef });
    return static_cast<uint32_t>(s_handles.size() - 1);
}

SMODoodadDef* Resolve(CMapObj* mapObj, uint16_t doodadRef) {
    if (!s_files.empty()) {
        auto file = s_files.find(FileKey(mapObj->m_wmoName));

        if (file != s_files.end()) {
            if (file->second.m_removed.count(doodadRef)) {
                return nullptr;
            }

            auto pose = file->second.m_poses.find(doodadRef);

            if (pose != file->second.m_poses.end()) {
                return &pose->second;
            }
        }
    }

    return &mapObj->m_doodadDefList[doodadRef];
}

bool Exists(uint32_t handle) {
    CMapObjDef* mapObjDef;
    uint16_t doodadRef;
    return Lookup(handle, mapObjDef, doodadRef) && Resolve(mapObjDef->m_owner, doodadRef) != nullptr;
}

bool GetBounds(uint32_t handle, CAaBox& box, C44Matrix& matrix) {
    CMapObjDef* mapObjDef;
    uint16_t doodadRef;

    if (!Exists(handle) || !Lookup(handle, mapObjDef, doodadRef)) {
        return false;
    }

    CMapDoodadDef* doodad = FindLive(mapObjDef, doodadRef);

    if (!doodad) {
        return false;
    }

    matrix = doodad->m_mat;

    if (doodad->m_model && doodad->m_model->IsLoaded(0, 0)) {
        box = doodad->m_model->GetBoundingBox();
    } else {
        box.b = C3Vector(-1.0f, -1.0f, 0.0f);
        box.t = C3Vector(1.0f, 1.0f, 2.0f);
    }

    return true;
}

const char* GetFileName(uint32_t handle) {
    CMapObjDef* mapObjDef;
    uint16_t doodadRef;

    if (!Lookup(handle, mapObjDef, doodadRef)) {
        return "";
    }

    CMapObj* mapObj = mapObjDef->m_owner;
    return &mapObj->m_doodadNameList[mapObj->m_doodadDefList[doodadRef].m_flags & 0xFFFFFF];
}

bool Read(uint32_t handle, C3Vector& world, C3Vector& rotation, float& scale) {
    CMapObjDef* mapObjDef;
    uint16_t doodadRef;

    if (!Lookup(handle, mapObjDef, doodadRef)) {
        return false;
    }

    SMODoodadDef* def = Resolve(mapObjDef->m_owner, doodadRef);

    if (!def) {
        return false;
    }

    C44Matrix matrix = BuildLocal(*def, 1.0f) * mapObjDef->m_mat;
    world = C3Vector(matrix.d0, matrix.d1, matrix.d2);
    rotation = ToEuler(matrix);
    scale = def->m_scale;
    return true;
}

void Write(uint32_t handle, const C3Vector& world, const C3Vector& rotation, float scale) {
    CMapObjDef* mapObjDef;
    uint16_t doodadRef;

    if (!Lookup(handle, mapObjDef, doodadRef)) {
        return;
    }

    CMapObj* mapObj = mapObjDef->m_owner;
    SMODoodadDef* current = Resolve(mapObj, doodadRef);

    if (!current) {
        return;
    }

    SMODoodadDef def = *current;
    C44Matrix local = BuildWorldRotation(world, rotation) * mapObjDef->m_invMat;
    def.m_position = C3Vector(local.d0, local.d1, local.d2);
    def.m_orientation = ToQuaternion(local);
    def.m_scale = std::min(std::max(scale, 0.01f), 63.0f);
    EditsFor(mapObj).m_poses[doodadRef] = def;

    ForEachPlacement(mapObj, [&](CMapObjDef* placement) {
        CMapDoodadDef* doodad = FindLive(placement, doodadRef);

        if (!doodad) {
            return;
        }

        doodad->m_scale = def.m_scale;
        doodad->m_identity = BuildLocal(def, def.m_scale);
        doodad->UpdateMatrixOnly(placement->m_mat);

        for (CMapBaseObjLink* link = doodad->m_parentLinkList.Head(); link; link = doodad->m_parentLinkList.Next(link)) {
            if (link->m_ref) {
                CMapDoodadDef::ExtendBounds(doodad, link->m_ref);
            }
        }
    });
}

void Remove(uint32_t handle) {
    CMapObjDef* mapObjDef;
    uint16_t doodadRef;

    if (!Lookup(handle, mapObjDef, doodadRef)) {
        return;
    }

    CMapObj* mapObj = mapObjDef->m_owner;
    EditsFor(mapObj).m_removed.insert(doodadRef);

    ForEachPlacement(mapObj, [&](CMapObjDef* placement) {
        CMapDoodadDef* doodad = FindLive(placement, doodadRef);

        if (!doodad) {
            return;
        }

        for (CMapBaseObjLink* link = doodad->m_parentLinkList.Head(); link;) {
            CMapBaseObjLink* next = doodad->m_parentLinkList.Next(link);
            CMap::FreeBaseObjLink(link);
            link = next;
        }

        CMapDoodadDef::Purge(doodad);
    });
}

void Restore(uint32_t handle) {
    CMapObjDef* mapObjDef;
    uint16_t doodadRef;

    if (!Lookup(handle, mapObjDef, doodadRef)) {
        return;
    }

    CMapObj* mapObj = mapObjDef->m_owner;
    EditsFor(mapObj).m_removed.erase(doodadRef);

    ForEachPlacement(mapObj, [&](CMapObjDef* placement) {
        for (CMapBaseObjLink* link = placement->m_mapObjDefGroupLinkList.Head(); link; link = placement->m_mapObjDefGroupLinkList.Next(link)) {
            CMapObjDefGroup* defGroup = static_cast<CMapObjDefGroup*>(link->m_owner);
            CMapObjGroup* group = mapObj->m_mapObjGroupArray[defGroup->m_groupNum];

            if (!(defGroup->m_flags & MAPOBJ_FLAG_REFS_CREATED) || !group || !(group->m_unkLoadedFlag & 1)) {
                continue;
            }

            for (int32_t i = 0; i < group->m_doodadRefListCount; i++) {
                if (group->m_doodadRefList[i] == doodadRef) {
                    CMapObj::CreateRef(mapObj, doodadRef, placement, defGroup);
                    CMap::FreeBaseObjLinksInBounds(&defGroup->m_bbox);
                    break;
                }
            }
        }
    });
}

static uint32_t ReadU32(const uint8_t* data) {
    uint32_t value;
    memcpy(&value, data, 4);
    return value;
}

static bool FindChunk(const std::vector<uint8_t>& data, size_t start, size_t end, uint32_t token, size_t& offset, uint32_t& size) {
    for (size_t at = start; at + 8 <= end;) {
        uint32_t chunkToken = ReadU32(&data[at]);
        uint32_t chunkSize = ReadU32(&data[at + 4]);

        if (at + 8 + chunkSize > end) {
            return false;
        }

        if (chunkToken == token) {
            offset = at;
            size = chunkSize;
            return true;
        }

        at += 8 + chunkSize;
    }

    return false;
}

static bool LoadBytes(const std::string& path, std::vector<uint8_t>& out) {
    void* data = nullptr;
    uint32_t size = 0;

    if (!SFile::LoadFile(path.c_str(), &data, &size, 0, nullptr) || !data) {
        return false;
    }

    out.assign(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
    SFile::Unload(data);
    return true;
}

static bool SaveRoot(FileEdits& file, std::vector<uint8_t>& root, int32_t& saved, std::string& error) {
    size_t offset;
    uint32_t size;

    if (file.m_poses.empty()) {
        return true;
    }

    if (!FindChunk(root, 0, root.size(), TOKEN_MODD, offset, size)) {
        error = "No MODD chunk in " + file.m_path;
        return false;
    }

    std::vector<uint8_t> original = root;

    for (const auto& [doodadRef, pose] : file.m_poses) {
        if ((doodadRef + 1u) * sizeof(SMODoodadDef) > size) {
            continue;
        }

        SMODoodadDef def;
        uint8_t* at = &root[offset + 8 + doodadRef * sizeof(SMODoodadDef)];
        memcpy(&def, at, sizeof(def));
        def.m_position = pose.m_position;
        def.m_orientation = pose.m_orientation;
        def.m_scale = pose.m_scale;
        memcpy(at, &def, sizeof(def));
    }

    if (root == original) {
        return true;
    }

    if (!EditorSave::WriteGameFile(file.m_path, root.data(), root.size())) {
        error = "Failed to write " + file.m_path;
        return false;
    }

    saved++;
    return true;
}

static bool SaveGroup(FileEdits& file, uint32_t index, int32_t& saved, std::string& error) {
    char suffix[16];
    snprintf(suffix, sizeof(suffix), "_%03u.wmo", index);
    std::string path = file.m_path.substr(0, file.m_path.rfind('.')) + suffix;
    std::vector<uint8_t> data;

    if (!LoadBytes(path, data)) {
        return true;
    }

    size_t mogp;
    uint32_t mogpSize;
    size_t modr;
    uint32_t modrSize;

    if (!FindChunk(data, 0, data.size(), TOKEN_MOGP, mogp, mogpSize) || !FindChunk(data, mogp + 8 + GROUP_HEADER_SIZE, mogp + 8 + mogpSize, TOKEN_MODR, modr, modrSize)) {
        return true;
    }

    const uint16_t* current = reinterpret_cast<const uint16_t*>(&data[modr + 8]);
    auto cached = file.m_groupRefs.emplace(index, std::vector<uint16_t>(current, current + modrSize / 2)).first;
    std::vector<uint16_t> refs;

    for (uint16_t ref : cached->second) {
        if (!file.m_removed.count(ref)) {
            refs.push_back(ref);
        }
    }

    if (refs.size() * 2 == modrSize && (refs.empty() || memcmp(refs.data(), current, modrSize) == 0)) {
        return true;
    }

    std::vector<uint8_t> out(data.begin(), data.begin() + modr + 8);
    const uint8_t* refBytes = reinterpret_cast<const uint8_t*>(refs.data());
    out.insert(out.end(), refBytes, refBytes + refs.size() * 2);
    out.insert(out.end(), data.begin() + modr + 8 + modrSize, data.end());

    uint32_t newRefsSize = static_cast<uint32_t>(refs.size() * 2);
    uint32_t newGroupSize = mogpSize - modrSize + newRefsSize;
    memcpy(&out[modr + 4], &newRefsSize, 4);
    memcpy(&out[mogp + 4], &newGroupSize, 4);

    if (!EditorSave::WriteGameFile(path, out.data(), out.size())) {
        error = "Failed to write " + path;
        return false;
    }

    saved++;
    return true;
}

int32_t Save(std::string& error) {
    int32_t saved = 0;

    for (auto& [key, file] : s_files) {
        if (!file.m_dirty) {
            continue;
        }

        std::vector<uint8_t> root;
        size_t mohd;
        uint32_t mohdSize;

        if (!LoadBytes(file.m_path, root) || !FindChunk(root, 0, root.size(), TOKEN_MOHD, mohd, mohdSize) || mohdSize < 8) {
            error = "Failed to read " + file.m_path;
            return saved;
        }

        uint32_t groupCount = ReadU32(&root[mohd + 8 + 4]);

        if (!SaveRoot(file, root, saved, error)) {
            return saved;
        }

        if (!file.m_removed.empty() || !file.m_groupRefs.empty()) {
            for (uint32_t i = 0; i < groupCount; i++) {
                if (!SaveGroup(file, i, saved, error)) {
                    return saved;
                }
            }
        }

        file.m_dirty = false;
    }

    return saved;
}

void Reset() {
    s_handles.clear();

    // Saved edits stay so a WMO still cached from before the save keeps matching the files.
    for (auto it = s_files.begin(); it != s_files.end();) {
        it = it->second.m_dirty ? s_files.erase(it) : std::next(it);
    }
}

}
