#include "HoT/Editor/Map/EditorUndo.h"
#include <algorithm>
#include <vector>
#include "WoW/Source/WorldClient/MapArea.h"
#include "HoT/Editor/Map/ChunkEdit.h"
#include "HoT/Editor/Map/FlightBoundsEdit.h"
#include "HoT/Editor/Map/ObjectEdit.h"
#include "HoT/Editor/Map/TerrainEdit.h"
#include "HoT/Editor/Map/TextureEdit.h"

namespace HoT::Editor::Map::EditorUndo {

struct Step {
    std::vector<HeightSnapshot> m_heightBefore;
    std::vector<HeightSnapshot> m_heightAfter;
    std::vector<TextureSnapshot> m_textureBefore;
    std::vector<TextureSnapshot> m_textureAfter;
    std::vector<ObjectSnapshot> m_objectBefore;
    std::vector<ObjectSnapshot> m_objectAfter;
    std::vector<ChunkSnapshot> m_chunkBefore;
    std::vector<ChunkSnapshot> m_chunkAfter;
    std::vector<FlightSnapshot> m_flightBefore;
    std::vector<FlightSnapshot> m_flightAfter;
    std::vector<EditorSave::AddedObject> m_removedObjects;
    std::vector<EditorSave::AddedObject> m_addedObjects;

    bool Empty() const {
        return this->m_heightBefore.empty() && this->m_textureBefore.empty() && this->m_objectBefore.empty() && this->m_chunkBefore.empty() && this->m_flightBefore.empty() && this->m_removedObjects.empty() && this->m_addedObjects.empty();
    }
};

static const size_t s_maxSteps = 64;
static bool s_open;
static Step s_current;
static std::vector<Step> s_undo;
static std::vector<Step> s_redo;
static std::vector<uint32_t> s_modifiedChunks;
static std::vector<uint64_t> s_modifiedObjects;
static std::vector<uint32_t> s_dirtyAreas;

void MarkArea(uint32_t areaIndex) {
    if (areaIndex < 4096 && std::find(s_dirtyAreas.begin(), s_dirtyAreas.end(), areaIndex) == s_dirtyAreas.end()) {
        s_dirtyAreas.push_back(areaIndex);
    }
}

const std::vector<uint32_t>& GetDirtyAreas() {
    return s_dirtyAreas;
}

static void MarkChunk(int32_t row, int32_t col) {
    uint32_t key = (static_cast<uint32_t>(row) << 10) | static_cast<uint32_t>(col);

    MarkArea(static_cast<uint32_t>(64 * (row >> 4) + (col >> 4)));

    if (std::find(s_modifiedChunks.begin(), s_modifiedChunks.end(), key) == s_modifiedChunks.end()) {
        s_modifiedChunks.push_back(key);
    }
}

void BeginStep() {
    if (s_open) {
        return;
    }

    s_open = true;
    s_current = Step();
}

bool IsOpen() {
    return s_open;
}

void RecordHeight(int32_t row, int32_t col) {
    BeginStep();

    for (const HeightSnapshot& snapshot : s_current.m_heightBefore) {
        if (snapshot.m_row == row && snapshot.m_col == col) {
            return;
        }
    }

    HeightSnapshot snapshot;

    if (TerrainEdit::CaptureHeights(row, col, snapshot)) {
        s_current.m_heightBefore.push_back(snapshot);
        MarkChunk(row, col);
    }
}

void RecordTexture(int32_t row, int32_t col) {
    BeginStep();

    for (const TextureSnapshot& snapshot : s_current.m_textureBefore) {
        if (snapshot.m_row == row && snapshot.m_col == col) {
            return;
        }
    }

    s_current.m_textureBefore.emplace_back();

    if (TextureEdit::Capture(row, col, s_current.m_textureBefore.back())) {
        MarkChunk(row, col);
    } else {
        s_current.m_textureBefore.pop_back();
    }
}

void RecordChunk(int32_t row, int32_t col) {
    BeginStep();

    for (const ChunkSnapshot& snapshot : s_current.m_chunkBefore) {
        if (snapshot.m_row == row && snapshot.m_col == col) {
            return;
        }
    }

    ChunkSnapshot snapshot;

    if (ChunkEdit::Capture(row, col, snapshot)) {
        s_current.m_chunkBefore.push_back(snapshot);
        MarkChunk(row, col);
    }
}

void RecordFlight(uint32_t areaIndex) {
    BeginStep();

    for (const FlightSnapshot& snapshot : s_current.m_flightBefore) {
        if (snapshot.m_area == areaIndex) {
            return;
        }
    }

    FlightSnapshot snapshot;

    if (FlightBoundsEdit::Capture(areaIndex, snapshot)) {
        s_current.m_flightBefore.push_back(snapshot);
        MarkArea(areaIndex);
    }
}

void RecordPresence(const std::vector<EditorSave::AddedObject>& removed, const std::vector<EditorSave::AddedObject>& added) {
    if (s_open) {
        EndStep();
    }

    BeginStep();
    s_current.m_removedObjects = removed;
    s_current.m_addedObjects = added;
    EndStep();
}

void RecordObject(int32_t kind, uint32_t uniqueId) {
    BeginStep();

    for (const ObjectSnapshot& snapshot : s_current.m_objectBefore) {
        if (snapshot.m_kind == kind && snapshot.m_uniqueId == uniqueId) {
            return;
        }
    }

    ObjectSnapshot snapshot;

    if (ObjectEdit::Capture(kind, uniqueId, snapshot)) {
        s_current.m_objectBefore.push_back(snapshot);
        uint64_t key = (static_cast<uint64_t>(kind) << 32) | uniqueId;
        CMapArea* area = ObjectEdit::FindArea(kind, uniqueId);

        if (area) {
            MarkArea(static_cast<uint32_t>(64 * area->m_index.y + area->m_index.x));
        }

        if (std::find(s_modifiedObjects.begin(), s_modifiedObjects.end(), key) == s_modifiedObjects.end()) {
            s_modifiedObjects.push_back(key);
        }
    }
}

void EndStep() {
    if (!s_open) {
        return;
    }

    s_open = false;

    if (s_current.Empty()) {
        return;
    }

    for (const HeightSnapshot& before : s_current.m_heightBefore) {
        HeightSnapshot after;

        if (TerrainEdit::CaptureHeights(before.m_row, before.m_col, after)) {
            s_current.m_heightAfter.push_back(after);
        }
    }

    for (const TextureSnapshot& before : s_current.m_textureBefore) {
        s_current.m_textureAfter.emplace_back();

        if (!TextureEdit::Capture(before.m_row, before.m_col, s_current.m_textureAfter.back())) {
            s_current.m_textureAfter.pop_back();
        }
    }

    for (const ObjectSnapshot& before : s_current.m_objectBefore) {
        ObjectSnapshot after;

        if (ObjectEdit::Capture(before.m_kind, before.m_uniqueId, after)) {
            s_current.m_objectAfter.push_back(after);
        }
    }

    for (const ChunkSnapshot& before : s_current.m_chunkBefore) {
        ChunkSnapshot after;

        if (ChunkEdit::Capture(before.m_row, before.m_col, after)) {
            s_current.m_chunkAfter.push_back(after);
        }
    }

    for (const FlightSnapshot& before : s_current.m_flightBefore) {
        FlightSnapshot after;

        if (FlightBoundsEdit::Capture(before.m_area, after)) {
            s_current.m_flightAfter.push_back(after);
        }
    }

    s_undo.push_back(std::move(s_current));

    if (s_undo.size() > s_maxSteps) {
        s_undo.erase(s_undo.begin());
    }

    s_redo.clear();
    s_current = Step();
}

static void Apply(const std::vector<HeightSnapshot>& heights, const std::vector<TextureSnapshot>& textures, const std::vector<ObjectSnapshot>& objects, const std::vector<ChunkSnapshot>& chunks, const std::vector<FlightSnapshot>& flights) {
    // A restore can add the MFBO chunk and reload its tile, so it goes before the chunk based restores.
    for (const FlightSnapshot& snapshot : flights) {
        FlightBoundsEdit::Restore(snapshot);
    }

    for (const ChunkSnapshot& snapshot : chunks) {
        ChunkEdit::Restore(snapshot);
    }

    for (const HeightSnapshot& snapshot : heights) {
        TerrainEdit::RestoreHeights(snapshot);
    }

    for (const TextureSnapshot& snapshot : textures) {
        TextureEdit::Restore(snapshot);
    }

    for (const ObjectSnapshot& snapshot : objects) {
        ObjectEdit::Restore(snapshot);
    }
}

bool Undo() {
    if (s_open || s_undo.empty()) {
        return false;
    }

    Step& step = s_undo.back();

    if (!step.m_addedObjects.empty()) {
        ObjectEdit::RemoveObjects(step.m_addedObjects, false);
    }

    if (!step.m_removedObjects.empty()) {
        ObjectEdit::AddObjects(step.m_removedObjects, false);
    }

    Apply(step.m_heightBefore, step.m_textureBefore, step.m_objectBefore, step.m_chunkBefore, step.m_flightBefore);
    s_redo.push_back(std::move(step));
    s_undo.pop_back();
    return true;
}

bool Redo() {
    if (s_open || s_redo.empty()) {
        return false;
    }

    Step& step = s_redo.back();

    if (!step.m_removedObjects.empty()) {
        ObjectEdit::RemoveObjects(step.m_removedObjects, false);
    }

    if (!step.m_addedObjects.empty()) {
        ObjectEdit::AddObjects(step.m_addedObjects, false);
    }

    Apply(step.m_heightAfter, step.m_textureAfter, step.m_objectAfter, step.m_chunkAfter, step.m_flightAfter);
    s_undo.push_back(std::move(step));
    s_redo.pop_back();
    return true;
}

uint32_t GetUndoCount() {
    return static_cast<uint32_t>(s_undo.size());
}

uint32_t GetRedoCount() {
    return static_cast<uint32_t>(s_redo.size());
}

uint32_t GetModifiedChunkCount() {
    return static_cast<uint32_t>(s_modifiedChunks.size());
}

uint32_t GetModifiedObjectCount() {
    return static_cast<uint32_t>(s_modifiedObjects.size());
}

void GetModified(std::vector<uint32_t>& chunks, std::vector<uint64_t>& objects) {
    chunks = s_modifiedChunks;
    objects = s_modifiedObjects;
}

void ClearModified() {
    s_modifiedChunks.clear();
    s_modifiedObjects.clear();
    s_dirtyAreas.clear();
}

void Clear() {
    s_open = false;
    s_current = Step();
    s_undo.clear();
    s_redo.clear();
    s_modifiedChunks.clear();
    s_modifiedObjects.clear();
    s_dirtyAreas.clear();
}

}
