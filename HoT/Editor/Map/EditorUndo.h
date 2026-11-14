#ifndef HOT_EDITOR_MAP_EDITORUNDO_H
#define HOT_EDITOR_MAP_EDITORUNDO_H

#include <cstdint>
#include <vector>
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/WorldClient/Types.h"
#include "HoT/Editor/Map/EditorSave.h"

namespace HoT::Editor::Map::EditorUndo {

struct HeightSnapshot {
    int32_t m_row;
    int32_t m_col;
    float m_height[145];
    int8_t m_normals[145 * 3];
};

struct TextureSnapshot {
    int32_t m_row;
    int32_t m_col;
    uint32_t m_layerCount;
    SMLayer m_layers[4];
    uint8_t m_alpha[3][4096];
};

struct ChunkSnapshot {
    int32_t m_row;
    int32_t m_col;
    uint32_t m_flags;
    uint16_t m_holes;
    uint32_t m_areaId;
    bool m_hasColors;
    uint32_t m_colors[145];
};

struct FlightSnapshot {
    uint32_t m_area;
    int16_t m_heights[18];
};

struct ObjectSnapshot {
    int32_t m_kind;
    uint32_t m_uniqueId;
    C3Vector m_position;
    C3Vector m_rotation;
    float m_scale;
};

void BeginStep();
void EndStep();
bool IsOpen();
void RecordHeight(int32_t row, int32_t col);
void RecordTexture(int32_t row, int32_t col);
void RecordObject(int32_t kind, uint32_t uniqueId);
void RecordChunk(int32_t row, int32_t col);
void RecordFlight(uint32_t areaIndex);
void RecordPresence(const std::vector<EditorSave::AddedObject>& removed, const std::vector<EditorSave::AddedObject>& added);
bool Undo();
bool Redo();
uint32_t GetUndoCount();
uint32_t GetRedoCount();
uint32_t GetModifiedChunkCount();
uint32_t GetModifiedObjectCount();
void Clear();
void GetModified(std::vector<uint32_t>& chunks, std::vector<uint64_t>& objects);
void ClearModified();
void MarkArea(uint32_t areaIndex);
const std::vector<uint32_t>& GetDirtyAreas();

}

#endif
