#include "HoT/Editor/Map/ObjectEdit.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <cmath>
#include <vector>
#include "Engine/Source/Gx/CGxBuf.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxVertexPC.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "WoW/Source/WorldClient/CMapBaseObjLink.h"
#include "WoW/Source/WorldClient/CWorldMath.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapArea.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/MapDoodadDef.h"
#include "WoW/Source/WorldClient/MapObj.h"
#include "WoW/Source/WorldClient/MapObjDef.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Editor/Map/TerrainEdit.h"
#include "HoT/Editor/Map/TextureEdit.h"
#include "HoT/Editor/Map/WmoDoodadEdit.h"

namespace HoT::Editor::Map::ObjectEdit {

static const float s_mapOrigin = 17066.666f;
static const float s_degToRad = 0.017453292f;

struct Selected {
    int32_t m_kind;
    uint32_t m_uniqueId;
};

struct FileDef {
    CMapArea* m_area;
    SMDoodadDef* m_doodad;
    SMMapObjDef* m_mapObj;
};

Settings s_settings = { true, false, false };

static std::vector<Selected> s_selection;
static bool s_editing;

static CMapDoodadDef* FindDoodad(uint32_t uniqueId) {
    HASHKEY_DWORD key = { 0 };
    return CMap::doodadDefHash.Ptr(uniqueId, key);
}

static CMapObjDef* FindMapObj(uint32_t uniqueId) {
    return CMap::mapObjDefHash.Ptr(uniqueId, CMap::s_mapObjDefHashKey);
}

static bool FindFileDef(int32_t kind, uint32_t uniqueId, FileDef& out) {
    for (uint32_t i = 0; i < 4096; i++) {
        CMapArea* area = CMap::m_areaTable[i];

        if (!area || area->m_asyncObject || !area->m_header) {
            continue;
        }

        if (kind == KIND_DOODAD) {
            for (int32_t j = 0; j < area->m_doodadDefCount; j++) {
                if (area->m_doodadDef[j].m_uniqueId == uniqueId) {
                    out.m_area = area;
                    out.m_doodad = &area->m_doodadDef[j];
                    out.m_mapObj = nullptr;
                    return true;
                }
            }
        } else {
            for (int32_t j = 0; j < area->m_mapObjDefCount; j++) {
                if (area->m_mapObjDef[j].m_uniqueId == uniqueId) {
                    out.m_area = area;
                    out.m_doodad = nullptr;
                    out.m_mapObj = &area->m_mapObjDef[j];
                    return true;
                }
            }
        }
    }

    return false;
}

static bool IsWorldDoodad(CMapDoodadDef* doodad) {
    for (CMapBaseObjLink* link = doodad->m_parentLinkList.Head(); link; link = doodad->m_parentLinkList.Next(link)) {
        if (link->m_ref && !(link->m_ref->m_type & 0x4)) {
            return false;
        }
    }

    return doodad->m_parentLinkList.Head() != nullptr;
}

static C3Vector FileToWorld(const C3Vector& file) {
    return C3Vector(s_mapOrigin - file.z, s_mapOrigin - file.x, file.y);
}

static C3Vector WorldToFile(const C3Vector& world) {
    return C3Vector(s_mapOrigin - world.y, world.z, s_mapOrigin - world.x);
}

static C44Matrix BuildMatrix(const C3Vector& world, const C3Vector& rotation, float scale) {
    C44Matrix matrix;
    matrix.d0 = world.x;
    matrix.d1 = world.y;
    matrix.d2 = world.z;
    matrix.RotateAroundZ(rotation.y * s_degToRad + 3.1415927f);
    matrix.RotateAroundY(rotation.x * s_degToRad);
    matrix.RotateAroundX(rotation.z * s_degToRad);

    if (scale != 1.0f) {
        matrix.Scale(scale);
    }

    return matrix;
}

static bool LocalBounds(int32_t kind, uint32_t uniqueId, CAaBox& box, C44Matrix& matrix) {
    if (kind == KIND_WMO_DOODAD) {
        return WmoDoodadEdit::GetBounds(uniqueId, box, matrix);
    }

    if (kind == KIND_DOODAD) {
        CMapDoodadDef* doodad = FindDoodad(uniqueId);

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

    CMapObjDef* mapObj = FindMapObj(uniqueId);

    if (!mapObj || !mapObj->m_owner) {
        return false;
    }

    matrix = mapObj->m_mat;

    if (mapObj->m_owner->m_isGroupLoaded) {
        mapObj->m_owner->GetBounds(&box);
    } else {
        box.b = C3Vector(-5.0f, -5.0f, 0.0f);
        box.t = C3Vector(5.0f, 5.0f, 10.0f);
    }

    return true;
}

static bool RayBox(const C3Vector& start, const C3Vector& end, const CAaBox& box, float* t) {
    float t0 = 0.0f;
    float t1 = 1.0f;
    const float s[3] = { start.x, start.y, start.z };
    const float e[3] = { end.x, end.y, end.z };
    const float b[3] = { box.b.x, box.b.y, box.b.z };
    const float tp[3] = { box.t.x, box.t.y, box.t.z };

    for (int32_t axis = 0; axis < 3; axis++) {
        float d = e[axis] - s[axis];

        if (fabsf(d) < 0.000001f) {
            if (s[axis] < b[axis] || s[axis] > tp[axis]) {
                return false;
            }

            continue;
        }

        float a = (b[axis] - s[axis]) / d;
        float c = (tp[axis] - s[axis]) / d;

        if (a > c) {
            std::swap(a, c);
        }

        t0 = std::max(t0, a);
        t1 = std::min(t1, c);

        if (t0 > t1) {
            return false;
        }
    }

    *t = t0;
    return true;
}

static bool HitObject(int32_t kind, uint32_t uniqueId, const C3Vector& start, const C3Vector& end, float* t) {
    CAaBox box;
    C44Matrix matrix;

    if (!LocalBounds(kind, uniqueId, box, matrix)) {
        return false;
    }

    C44Matrix inverse = matrix.Inverse();
    C3Vector localStart = start * inverse;
    C3Vector localEnd = end * inverse;

    if (localStart.x >= box.b.x && localStart.x <= box.t.x && localStart.y >= box.b.y && localStart.y <= box.t.y && localStart.z >= box.b.z && localStart.z <= box.t.z) {
        return false;
    }

    return RayBox(localStart, localEnd, box, t);
}

bool Pick(const C3Vector& start, const C3Vector& dir, float maxDistance, int32_t* kind, uint32_t* uniqueId, float* distance) {
    C3Vector rayStart = start;
    C3Vector rayEnd(start.x + dir.x * maxDistance, start.y + dir.y * maxDistance, start.z + dir.z * maxDistance);
    float best = 1.0f;
    bool found = false;

    CMap::s_queryTag++;
    float mapObjT = 1.0f;
    CMapObj* mapObj = nullptr;
    CMapObjDef* mapObjDef = nullptr;
    CMapObjDefGroup* mapObjDefGroup = nullptr;

    if (CMap::VectorIntersect(&rayStart, &rayEnd, 0x110, 0x100, &mapObjT, nullptr, &mapObj, &mapObjDef, &mapObjDefGroup) && mapObjDef) {
        FileDef def;
        best = mapObjT;

        if (FindFileDef(KIND_WMO, mapObjDef->m_hashval, def)) {
            *kind = KIND_WMO;
            *uniqueId = mapObjDef->m_hashval;
            found = true;
        }
    }

    CM2Scene* scene = World::s_m2Scene;

    if (scene) {
        scene->BeginHitTest();

        for (CMapDoodadDef* doodad = CMap::doodadDefHash.Head(); doodad; doodad = CMap::doodadDefHash.Next(doodad)) {
            CM2Model* model = doodad->m_model;

            bool pickable = IsWorldDoodad(doodad) || (s_settings.m_pickWmoDoodads && doodad->m_key.m_key != 0);

            if (!model || !(model->m_fFlags & 1) || !pickable || !CWorldMath::VectorIntersectAABox2(doodad->m_bbox, rayStart, rayEnd)) {
                continue;
            }

            if (!model->m_hitTestPrev) {
                model->m_hitTestPrev = &model->m_scene->m_hitTestList;
                model->m_hitTestNext = model->m_scene->m_hitTestList;
                model->m_scene->m_hitTestList = model;

                if (model->m_hitTestNext) {
                    model->m_hitTestNext->m_hitTestPrev = &model->m_hitTestNext;
                }
            }

            model->m_hitTestMode = 0;
            model->m_hitTestOwner = doodad;
            model->m_hitTestGroup = 0;
        }

        float doodadT = best;
        C3Vector localStart = rayStart * scene->m_view;
        C3Vector localEnd = rayEnd * scene->m_view;
        CMapDoodadDef* hitDoodad = static_cast<CMapDoodadDef*>(scene->EndHitTest(localStart, localEnd, doodadT, 0));

        if (hitDoodad && doodadT < best && (hitDoodad->m_type & 0x40)) {
            best = doodadT;
            *kind = hitDoodad->m_key.m_key ? KIND_WMO_DOODAD : KIND_DOODAD;
            *uniqueId = hitDoodad->m_key.m_key ? WmoDoodadEdit::GetHandle(hitDoodad->m_key.m_key - 1, static_cast<uint16_t>(hitDoodad->m_hashval)) : hitDoodad->m_hashval;
            found = true;
        }
    }

    if (found) {
        *distance = best * maxDistance;
    }

    return found;
}

static bool IsSelected(int32_t kind, uint32_t uniqueId, size_t* index) {
    for (size_t i = 0; i < s_selection.size(); i++) {
        if (s_selection[i].m_kind == kind && s_selection[i].m_uniqueId == uniqueId) {
            *index = i;
            return true;
        }
    }

    return false;
}

static bool ObjectExists(int32_t kind, uint32_t uniqueId) {
    if (kind == KIND_WMO_DOODAD) {
        return WmoDoodadEdit::Exists(uniqueId);
    }

    FileDef def;
    return FindFileDef(kind, uniqueId, def);
}

void Select(int32_t kind, uint32_t uniqueId, bool add) {
    if (!ObjectExists(kind, uniqueId)) {
        return;
    }

    size_t index;

    if (add) {
        if (IsSelected(kind, uniqueId, &index)) {
            s_selection.erase(s_selection.begin() + index);
        } else {
            s_selection.push_back({ kind, uniqueId });
        }

        return;
    }

    s_selection.clear();
    s_selection.push_back({ kind, uniqueId });
}

void ClearSelection() {
    s_selection.clear();
}

CMapArea* FindArea(int32_t kind, uint32_t uniqueId) {
    FileDef def;
    return kind != KIND_WMO_DOODAD && FindFileDef(kind, uniqueId, def) ? def.m_area : nullptr;
}

static void PruneSelection();

static std::vector<EditorSave::AddedObject> s_clipboard;
static uint32_t s_lastUniqueId;

static bool CollectObject(int32_t kind, uint32_t uniqueId, EditorSave::AddedObject& out) {
    if (kind == KIND_WMO_DOODAD) {
        out = EditorSave::AddedObject();
        out.m_kind = kind;
        out.m_doodad.m_uniqueId = uniqueId;
        out.m_fileName = WmoDoodadEdit::GetFileName(uniqueId);
        return WmoDoodadEdit::Exists(uniqueId);
    }

    FileDef def;

    if (!FindFileDef(kind, uniqueId, def)) {
        return false;
    }

    out = EditorSave::AddedObject();
    out.m_kind = kind;

    if (def.m_doodad) {
        out.m_doodad = *def.m_doodad;
        out.m_fileName = &def.m_area->m_m2FileNames[def.m_area->m_modelFilenamesOffsets[def.m_doodad->m_nameId]];
    } else {
        out.m_mapObj = *def.m_mapObj;
        out.m_fileName = &def.m_area->m_wmoFileNames[def.m_area->m_wmoFilenamesOffsets[def.m_mapObj->m_nameId]];
    }

    return true;
}

static uint32_t NextUniqueId() {
    uint32_t highest = s_lastUniqueId;

    for (uint32_t i = 0; i < 4096; i++) {
        CMapArea* area = CMap::m_areaTable[i];

        if (!area || area->m_asyncObject || !area->m_header) {
            continue;
        }

        for (int32_t j = 0; j < area->m_doodadDefCount; j++) {
            highest = std::max(highest, area->m_doodadDef[j].m_uniqueId);
        }

        for (int32_t j = 0; j < area->m_mapObjDefCount; j++) {
            highest = std::max(highest, area->m_mapObjDef[j].m_uniqueId);
        }
    }

    s_lastUniqueId = highest + 1;
    return s_lastUniqueId;
}

static void PurgeFromScene(int32_t kind, uint32_t uniqueId) {
    CMapBaseObj* object = kind == KIND_DOODAD ? static_cast<CMapBaseObj*>(FindDoodad(uniqueId)) : static_cast<CMapBaseObj*>(FindMapObj(uniqueId));

    if (!object) {
        return;
    }

    for (CMapBaseObjLink* link = object->m_parentLinkList.Head(); link;) {
        CMapBaseObjLink* next = object->m_parentLinkList.Next(link);
        CMap::FreeBaseObjLink(link);
        link = next;
    }

    if (kind == KIND_DOODAD) {
        CMapDoodadDef::Purge(static_cast<CMapDoodadDef*>(object));
    } else {
        CMap::PurgeMapObjDef(static_cast<CMapObjDef*>(object));
    }
}

static C3Vector FilePosition(const EditorSave::AddedObject& object) {
    return object.m_kind == KIND_DOODAD ? object.m_doodad.m_position : object.m_mapObj.m_position;
}

void RemoveObjects(const std::vector<EditorSave::AddedObject>& objects, bool record) {
    std::vector<EditorSave::AddedObject> removed;

    for (const EditorSave::AddedObject& object : objects) {
        uint32_t uniqueId = object.m_kind == KIND_WMO ? object.m_mapObj.m_uniqueId : object.m_doodad.m_uniqueId;
        EditorSave::AddedObject current;

        if (record && CollectObject(object.m_kind, uniqueId, current)) {
            removed.push_back(current);
        }

        if (object.m_kind == KIND_WMO_DOODAD) {
            WmoDoodadEdit::Remove(uniqueId);
        } else {
            PurgeFromScene(object.m_kind, uniqueId);
        }
    }

    for (uint32_t i = 0; i < 4096; i++) {
        CMapArea* area = CMap::m_areaTable[i];

        if (!area || area->m_asyncObject || !area->m_header) {
            continue;
        }

        EditorSave::ObjectChanges changes;

        for (const EditorSave::AddedObject& object : objects) {
            if (object.m_kind == KIND_WMO_DOODAD) {
                continue;
            }

            if (object.m_kind == KIND_DOODAD) {
                for (int32_t j = 0; j < area->m_doodadDefCount; j++) {
                    if (area->m_doodadDef[j].m_uniqueId == object.m_doodad.m_uniqueId) {
                        changes.m_removedDoodads.push_back(object.m_doodad.m_uniqueId);
                    }
                }
            } else {
                for (int32_t j = 0; j < area->m_mapObjDefCount; j++) {
                    if (area->m_mapObjDef[j].m_uniqueId == object.m_mapObj.m_uniqueId) {
                        changes.m_removedMapObjs.push_back(object.m_mapObj.m_uniqueId);
                    }
                }
            }
        }

        if (!changes.m_removedDoodads.empty() || !changes.m_removedMapObjs.empty()) {
            EditorSave::ApplyObjectChanges(area, changes);
        }
    }

    PruneSelection();

    if (record && !removed.empty()) {
        EditorUndo::RecordPresence(removed, std::vector<EditorSave::AddedObject>());
    }
}

void AddObjects(const std::vector<EditorSave::AddedObject>& objects, bool record) {
    std::vector<CMapArea*> areas;
    std::vector<EditorSave::ObjectChanges> changes;
    std::vector<EditorSave::AddedObject> added;

    for (const EditorSave::AddedObject& object : objects) {
        if (object.m_kind == KIND_WMO_DOODAD) {
            WmoDoodadEdit::Restore(object.m_doodad.m_uniqueId);
            continue;
        }

        C3Vector position = FilePosition(object);
        int32_t areaX = static_cast<int32_t>(position.x / 533.33333f);
        int32_t areaY = static_cast<int32_t>(position.z / 533.33333f);

        if (areaX < 0 || areaX > 63 || areaY < 0 || areaY > 63) {
            continue;
        }

        CMapArea* area = CMap::m_areaTable[64 * areaY + areaX];

        if (!area || area->m_asyncObject || !area->m_header) {
            continue;
        }

        size_t index = std::find(areas.begin(), areas.end(), area) - areas.begin();

        if (index == areas.size()) {
            areas.push_back(area);
            changes.emplace_back();
        }

        changes[index].m_added.push_back(object);
        added.push_back(object);
    }

    for (size_t i = 0; i < areas.size(); i++) {
        EditorSave::ApplyObjectChanges(areas[i], changes[i]);
    }

    if (record && !added.empty()) {
        EditorUndo::RecordPresence(std::vector<EditorSave::AddedObject>(), added);
    }
}

void DeleteSelection() {
    PruneSelection();
    std::vector<EditorSave::AddedObject> objects;

    for (const Selected& selected : s_selection) {
        EditorSave::AddedObject object;

        if (CollectObject(selected.m_kind, selected.m_uniqueId, object)) {
            objects.push_back(object);
        }
    }

    s_selection.clear();

    if (!objects.empty()) {
        RemoveObjects(objects, true);
    }
}

void CopySelection() {
    PruneSelection();
    s_clipboard.clear();

    for (const Selected& selected : s_selection) {
        EditorSave::AddedObject object;

        if (selected.m_kind != KIND_WMO_DOODAD && CollectObject(selected.m_kind, selected.m_uniqueId, object)) {
            s_clipboard.push_back(object);
        }
    }
}

bool HasClipboard() {
    return !s_clipboard.empty();
}

void Paste(const C3Vector* worldPos) {
    if (s_clipboard.empty()) {
        return;
    }

    C3Vector offset(0.0f, 0.0f, 0.0f);

    if (worldPos) {
        C3Vector target = WorldToFile(*worldPos);
        C3Vector origin = FilePosition(s_clipboard.front());
        offset = C3Vector(target.x - origin.x, target.y - origin.y, target.z - origin.z);
    }

    std::vector<EditorSave::AddedObject> objects = s_clipboard;
    s_selection.clear();

    for (EditorSave::AddedObject& object : objects) {
        uint32_t uniqueId = NextUniqueId();

        if (object.m_kind == KIND_DOODAD) {
            object.m_doodad.m_uniqueId = uniqueId;
            object.m_doodad.m_position = C3Vector(object.m_doodad.m_position.x + offset.x, object.m_doodad.m_position.y + offset.y, object.m_doodad.m_position.z + offset.z);
        } else {
            object.m_mapObj.m_uniqueId = uniqueId;
            object.m_mapObj.m_position = C3Vector(object.m_mapObj.m_position.x + offset.x, object.m_mapObj.m_position.y + offset.y, object.m_mapObj.m_position.z + offset.z);
            object.m_mapObj.m_extents.b = C3Vector(object.m_mapObj.m_extents.b.x + offset.x, object.m_mapObj.m_extents.b.y + offset.y, object.m_mapObj.m_extents.b.z + offset.z);
            object.m_mapObj.m_extents.t = C3Vector(object.m_mapObj.m_extents.t.x + offset.x, object.m_mapObj.m_extents.t.y + offset.y, object.m_mapObj.m_extents.t.z + offset.z);
        }

        s_selection.push_back(Selected { object.m_kind, uniqueId });
    }

    AddObjects(objects, true);
}

static std::vector<std::string> s_models;
static bool s_modelsBuilt;

static bool EndsWithI(const std::string& text, const char* suffix) {
    size_t length = strlen(suffix);
    return text.size() >= length && _stricmp(text.c_str() + text.size() - length, suffix) == 0;
}

static int32_t CollectModel(const char* fileName, void* param) {
    std::string name = fileName;

    if (!EndsWithI(name, ".m2") && !EndsWithI(name, ".wmo")) {
        return 1;
    }

    if (EndsWithI(name, ".wmo") && name.size() > 8 && name[name.size() - 8] == '_' && isdigit(static_cast<unsigned char>(name[name.size() - 7])) && isdigit(static_cast<unsigned char>(name[name.size() - 6])) && isdigit(static_cast<unsigned char>(name[name.size() - 5]))) {
        return 1;
    }

    for (char& c : name) {
        c = c == '/' ? '\\' : c;
    }

    static_cast<std::vector<std::string>*>(param)->push_back(name);
    return 1;
}

static void BuildModels() {
    if (s_modelsBuilt) {
        return;
    }

    s_modelsBuilt = true;
    TextureEdit::ListArchives(CollectModel, &s_models);
    std::sort(s_models.begin(), s_models.end(), [](const std::string& a, const std::string& b) {
        return _stricmp(a.c_str(), b.c_str()) < 0;
    });
    s_models.erase(std::unique(s_models.begin(), s_models.end(), [](const std::string& a, const std::string& b) {
        return _stricmp(a.c_str(), b.c_str()) == 0;
    }), s_models.end());
}

uint32_t GetModelCount() {
    BuildModels();
    return static_cast<uint32_t>(s_models.size());
}

const char* GetModel(uint32_t index) {
    BuildModels();
    return index < s_models.size() ? s_models[index].c_str() : nullptr;
}

bool SetClipboardModel(const char* fileName) {
    if (!fileName || !*fileName) {
        return false;
    }

    std::string name = fileName;
    EditorSave::AddedObject object = EditorSave::AddedObject();
    object.m_fileName = name;

    if (EndsWithI(name, ".m2") || EndsWithI(name, ".mdx")) {
        object.m_kind = KIND_DOODAD;
        object.m_doodad.m_scale = 1024;
    } else if (EndsWithI(name, ".wmo")) {
        void* data = nullptr;
        uint32_t size = 0;

        if (!SFile::LoadFile(fileName, &data, &size, 0, nullptr) || !data) {
            return false;
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        CAaBox bounds;
        bounds.b = C3Vector(0.0f, 0.0f, 0.0f);
        bounds.t = C3Vector(0.0f, 0.0f, 0.0f);

        for (uint32_t offset = 0; offset + 8 <= size;) {
            uint32_t token;
            uint32_t chunkSize;
            memcpy(&token, bytes + offset, 4);
            memcpy(&chunkSize, bytes + offset + 4, 4);

            if (token == 0x4D4F4844 && chunkSize >= sizeof(SMOHeader) && offset + 8 + sizeof(SMOHeader) <= size) {
                SMOHeader header;
                memcpy(&header, bytes + offset + 8, sizeof(header));
                bounds = header.m_boundingBox;
                break;
            }

            offset += 8 + chunkSize;
        }

        SFile::Unload(data);
        object.m_kind = KIND_WMO;
        object.m_mapObj.m_extents.b = C3Vector(bounds.b.y, bounds.b.z, bounds.b.x);
        object.m_mapObj.m_extents.t = C3Vector(bounds.t.y, bounds.t.z, bounds.t.x);
    } else {
        return false;
    }

    s_clipboard.clear();
    s_clipboard.push_back(object);
    return true;
}

void Duplicate() {
    CopySelection();
    Paste(nullptr);
}

void Reset() {
    s_selection.clear();
    s_editing = false;
    WmoDoodadEdit::Reset();
}

static void PruneSelection() {
    s_selection.erase(std::remove_if(s_selection.begin(), s_selection.end(), [](const Selected& selected) {
        return !ObjectExists(selected.m_kind, selected.m_uniqueId);
    }), s_selection.end());
}

uint32_t GetSelectionCount() {
    PruneSelection();
    return static_cast<uint32_t>(s_selection.size());
}

bool HasWmoDoodadSelection() {
    return std::any_of(s_selection.begin(), s_selection.end(), [](const Selected& selected) {
        return selected.m_kind == KIND_WMO_DOODAD;
    });
}

bool GetSelectionInfo(uint32_t index, int32_t* kind, uint32_t* uniqueId, const char** fileName) {
    if (index >= s_selection.size()) {
        return false;
    }

    *kind = s_selection[index].m_kind;
    *uniqueId = s_selection[index].m_uniqueId;
    *fileName = "";

    if (*kind == KIND_WMO_DOODAD) {
        *fileName = WmoDoodadEdit::GetFileName(*uniqueId);
        return true;
    }

    FileDef def;

    if (FindFileDef(*kind, *uniqueId, def)) {
        if (def.m_doodad) {
            *fileName = &def.m_area->m_m2FileNames[def.m_area->m_modelFilenamesOffsets[def.m_doodad->m_nameId]];
        } else {
            *fileName = &def.m_area->m_wmoFileNames[def.m_area->m_wmoFilenamesOffsets[def.m_mapObj->m_nameId]];
        }
    }

    return true;
}

static void LinkToChunk(CMapBaseObj* object, const C3Vector& world, bool doodad) {
    CMapChunk* chunk = TerrainEdit::GetChunk(TerrainEdit::ChunkCoord(world.x), TerrainEdit::ChunkCoord(world.y));

    if (!chunk) {
        return;
    }

    for (CMapBaseObjLink* link = object->m_parentLinkList.Head(); link; link = object->m_parentLinkList.Next(link)) {
        if (link->m_ref == chunk) {
            return;
        }
    }

    CMapBaseObjLink* link = CMap::AllocBaseObjLink(object);
    link->m_ref = chunk;

    if (doodad) {
        chunk->m_doodadDefLinkList.LinkToTail(link);
    } else {
        chunk->m_mapObjDefLinkList.LinkToTail(link);
    }
}

static void ApplyState(int32_t kind, uint32_t uniqueId, const C3Vector& filePos, const C3Vector& rotation, float scale) {
    if (kind == KIND_WMO_DOODAD) {
        WmoDoodadEdit::Write(uniqueId, FileToWorld(filePos), rotation, scale);
        return;
    }

    FileDef def;

    if (!FindFileDef(kind, uniqueId, def)) {
        return;
    }

    C3Vector world = FileToWorld(filePos);

    if (kind == KIND_DOODAD) {
        CMapDoodadDef* doodad = FindDoodad(uniqueId);
        scale = std::min(std::max(scale, 0.01f), 63.0f);
        def.m_doodad->m_position = filePos;
        def.m_doodad->m_rotation = rotation;
        def.m_doodad->m_scale = static_cast<uint16_t>(lroundf(scale * 1024.0f));

        if (!doodad) {
            return;
        }

        doodad->m_position = world;
        doodad->m_scale = scale;
        doodad->m_mat = BuildMatrix(world, rotation, scale);
        LinkToChunk(doodad, world, true);
        CMap::UpdateDoodadDefBounds(doodad);
        return;
    }

    CMapObjDef* mapObj = FindMapObj(uniqueId);
    def.m_mapObj->m_position = filePos;
    def.m_mapObj->m_rotation = rotation;

    if (!mapObj) {
        return;
    }

    CMapObjDef::UpdateMatrix(mapObj, BuildMatrix(world, rotation, 1.0f));
    LinkToChunk(mapObj, world, false);
}

static bool ReadState(int32_t kind, uint32_t uniqueId, C3Vector& filePos, C3Vector& rotation, float& scale) {
    if (kind == KIND_WMO_DOODAD) {
        C3Vector world;

        if (!WmoDoodadEdit::Read(uniqueId, world, rotation, scale)) {
            return false;
        }

        filePos = WorldToFile(world);
        return true;
    }

    FileDef def;

    if (!FindFileDef(kind, uniqueId, def)) {
        return false;
    }

    if (def.m_doodad) {
        filePos = def.m_doodad->m_position;
        rotation = def.m_doodad->m_rotation;
        scale = def.m_doodad->m_scale / 1024.0f;
    } else {
        filePos = def.m_mapObj->m_position;
        rotation = def.m_mapObj->m_rotation;
        scale = 1.0f;
    }

    return true;
}

bool GetTransform(C3Vector* position, C3Vector* rotation, float* scale) {
    PruneSelection();

    if (s_selection.empty()) {
        return false;
    }

    return ReadState(s_selection.back().m_kind, s_selection.back().m_uniqueId, *position, *rotation, *scale);
}

void BeginEdit() {
    if (s_editing) {
        return;
    }

    PruneSelection();
    s_editing = true;
    EditorUndo::BeginStep();

    for (const Selected& selected : s_selection) {
        EditorUndo::RecordObject(selected.m_kind, selected.m_uniqueId);
    }
}

void EndEdit() {
    if (!s_editing) {
        return;
    }

    s_editing = false;
    EditorUndo::EndStep();
}

static void ForEachSelected(void (*func)(int32_t kind, uint32_t uniqueId, C3Vector& filePos, C3Vector& rotation, float& scale, const void* arg), const void* arg) {
    bool ownStep = !s_editing;

    if (ownStep) {
        BeginEdit();
    }

    for (const Selected& selected : s_selection) {
        C3Vector filePos;
        C3Vector rotation;
        float scale;

        if (!ReadState(selected.m_kind, selected.m_uniqueId, filePos, rotation, scale)) {
            continue;
        }

        func(selected.m_kind, selected.m_uniqueId, filePos, rotation, scale, arg);
        ApplyState(selected.m_kind, selected.m_uniqueId, filePos, rotation, scale);
    }

    if (ownStep) {
        EndEdit();
    }
}

void SetTransform(const C3Vector* position, const C3Vector* rotation, const float* scale) {
    struct Args {
        const C3Vector* m_position;
        const C3Vector* m_rotation;
        const float* m_scale;
    } args = { position, rotation, scale };

    ForEachSelected([](int32_t kind, uint32_t uniqueId, C3Vector& filePos, C3Vector& rot, float& sc, const void* arg) {
        const Args* values = static_cast<const Args*>(arg);

        if (values->m_position) {
            filePos = *values->m_position;
        }

        if (values->m_rotation) {
            rot = *values->m_rotation;
        }

        if (values->m_scale) {
            sc = *values->m_scale;
        }
    }, &args);
}

void MoveBy(const C3Vector& delta) {
    ForEachSelected([](int32_t kind, uint32_t uniqueId, C3Vector& filePos, C3Vector& rot, float& sc, const void* arg) {
        const C3Vector* worldDelta = static_cast<const C3Vector*>(arg);
        C3Vector world = FileToWorld(filePos);
        world.x += worldDelta->x;
        world.y += worldDelta->y;
        world.z += worldDelta->z;
        filePos = WorldToFile(world);
    }, &delta);
}

void MoveTo(const C3Vector& worldPos) {
    PruneSelection();

    if (s_selection.empty()) {
        return;
    }

    C3Vector center(0.0f, 0.0f, 0.0f);
    uint32_t count = 0;

    for (const Selected& selected : s_selection) {
        C3Vector filePos;
        C3Vector rotation;
        float scale;

        if (ReadState(selected.m_kind, selected.m_uniqueId, filePos, rotation, scale)) {
            C3Vector world = FileToWorld(filePos);
            center.x += world.x;
            center.y += world.y;
            center.z += world.z;
            count++;
        }
    }

    if (!count) {
        return;
    }

    C3Vector delta(worldPos.x - center.x / count, worldPos.y - center.y / count, worldPos.z - center.z / count);
    MoveBy(delta);

    if (s_settings.m_snapMultiToGround && s_selection.size() > 1) {
        SnapToGround();
    }
}

void RotateBy(const C3Vector& degrees) {
    ForEachSelected([](int32_t kind, uint32_t uniqueId, C3Vector& filePos, C3Vector& rot, float& sc, const void* arg) {
        const C3Vector* delta = static_cast<const C3Vector*>(arg);
        rot.x = fmodf(rot.x + delta->x, 360.0f);
        rot.y = fmodf(rot.y + delta->y, 360.0f);
        rot.z = fmodf(rot.z + delta->z, 360.0f);
    }, &degrees);
}

void ScaleBy(float factor) {
    ForEachSelected([](int32_t kind, uint32_t uniqueId, C3Vector& filePos, C3Vector& rot, float& sc, const void* arg) {
        if (kind != KIND_WMO) {
            sc *= *static_cast<const float*>(arg);
        }
    }, &factor);
}

void SnapToGround() {
    ForEachSelected([](int32_t kind, uint32_t uniqueId, C3Vector& filePos, C3Vector& rot, float& sc, const void* arg) {
        C3Vector world = FileToWorld(filePos);
        float height;

        // Terrain under a building says nothing about the floor inside it.
        if (kind != KIND_WMO_DOODAD && TerrainEdit::GetHeight(world.x, world.y, &height)) {
            filePos.y = height;
        }
    }, nullptr);
}

bool Capture(int32_t kind, uint32_t uniqueId, EditorUndo::ObjectSnapshot& snapshot) {
    snapshot.m_kind = kind;
    snapshot.m_uniqueId = uniqueId;
    return ReadState(kind, uniqueId, snapshot.m_position, snapshot.m_rotation, snapshot.m_scale);
}

void Restore(const EditorUndo::ObjectSnapshot& snapshot) {
    ApplyState(snapshot.m_kind, snapshot.m_uniqueId, snapshot.m_position, snapshot.m_rotation, snapshot.m_scale);
}

void DrawSelection(const C3Vector& camPos) {
    PruneSelection();

    if (s_selection.empty()) {
        return;
    }

    static const int32_t s_edges[24] = { 0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 2, 6, 3, 7 };
    uint32_t vertexCount = static_cast<uint32_t>(s_selection.size()) * 24;
    CGxBuf* buf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPC), vertexCount);
    CGxVertexPC* vertices = reinterpret_cast<CGxVertexPC*>(g_theGxDevicePtr->BufLock(buf));
    uint32_t written = 0;

    CImVector color;
    color.r = 0xFF;
    color.g = 0xFF;
    color.b = 0xFF;
    color.a = 0xFF;
    GxFormatColor(color);

    for (const Selected& selected : s_selection) {
        CAaBox box;
        C44Matrix matrix;

        if (!LocalBounds(selected.m_kind, selected.m_uniqueId, box, matrix)) {
            continue;
        }

        C3Vector corners[8];

        for (int32_t i = 0; i < 8; i++) {
            C3Vector local((i & 1) ? box.t.x : box.b.x, (i & 2) ? box.t.y : box.b.y, (i & 4) ? box.t.z : box.b.z);
            corners[i] = local * matrix;
        }

        for (int32_t i = 0; i < 24; i++) {
            vertices[written].p = corners[s_edges[i]];
            vertices[written].c = color;
            written++;
        }
    }

    GxBufUnlock(buf, 0);

    if (!written) {
        return;
    }

    g_theGxDevicePtr->RsPush();
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_VertexShader, nullptr);
    GxRsSet(GxRs_PixelShader, nullptr);
    GxRsSet(GxRs_Texture0, nullptr);
    GxRsSet(GxRs_DepthTest, 0);
    GxRsSet(GxRs_DepthWrite, 0);

    C44Matrix world;
    world.Translate(C3Vector(-camPos.x, -camPos.y, -camPos.z));
    GxXformPush(GxXform_World, world);
    GxPrimVertexPtr(buf, GxVBF_PC);

    CGxBatch batch;
    batch.m_primType = GxPrim_Lines;
    batch.m_start = 0;
    batch.m_count = written;
    batch.m_minIndex = 0;
    batch.m_maxIndex = static_cast<uint16_t>(written - 1);
    g_theGxDevicePtr->Draw(&batch, 0);

    GxXformPop(GxXform_World);
    g_theGxDevicePtr->RsPop();
}

}
