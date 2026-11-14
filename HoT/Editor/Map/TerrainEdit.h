#ifndef HOT_EDITOR_MAP_TERRAINEDIT_H
#define HOT_EDITOR_MAP_TERRAINEDIT_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "HoT/Editor/Map/EditorUndo.h"

class CMapChunk;

namespace HoT::Editor::Map::TerrainEdit {

enum MODE {
    MODE_RAISE_LOWER = 0,
    MODE_FLATTEN_BLUR = 1,
    MODE_TEXTURE_PAINT = 2,
    MODE_OBJECT = 3,
    MODE_HOLES = 4,
    MODE_AREA = 5,
    MODE_IMPASS = 6,
    MODE_VERTEX = 7,
    MODE_FLIGHT = 8,
    NUM_MODES = 9,
};

enum TERRAIN_TYPE {
    TERRAIN_FLAT = 0,
    TERRAIN_LINEAR = 1,
    TERRAIN_SMOOTH = 2,
    TERRAIN_POLYNOM = 3,
    TERRAIN_TRIGO = 4,
    TERRAIN_QUADRA = 5,
    TERRAIN_GAUSSIAN = 6,
    NUM_TERRAIN_TYPES = 7,
};

enum FLATTEN_TYPE {
    FLATTEN_FLAT = 0,
    FLATTEN_LINEAR = 1,
    FLATTEN_SMOOTH = 2,
    FLATTEN_ORIGIN = 3,
    NUM_FLATTEN_TYPES = 4,
};

enum OPERATION {
    OP_RAISE = 0,
    OP_LOWER = 1,
    OP_FLATTEN = 2,
    OP_BLUR = 3,
};

struct RaiseLowerSettings {
    int32_t m_type;
    float m_radius;
    float m_innerRadius;
    float m_speed;
};

struct FlattenBlurSettings {
    int32_t m_type;
    float m_radius;
    float m_speed;
    bool m_raise;
    bool m_lower;
    bool m_angled;
    float m_angle;
    float m_orientation;
    bool m_lock;
    C3Vector m_lockPos;
};

extern MODE s_mode;
extern RaiseLowerSettings s_raiseLower;
extern FlattenBlurSettings s_flattenBlur;

float GetRadius();
void ChangeRadius(float delta);
void ChangeInnerRadius(float delta);
void ChangeSpeed(float delta);
bool GetHeight(float x, float y, float* z);
int32_t GetAreaId(float x, float y);
bool Raycast(const C3Vector& start, const C3Vector& dir, float maxDistance, C3Vector* hit);
void Apply(OPERATION op, const C3Vector& pos, float elapsedSec);
void EndStroke();
bool IsStroking();
bool CaptureHeights(int32_t row, int32_t col, EditorUndo::HeightSnapshot& snapshot);
void RestoreHeights(const EditorUndo::HeightSnapshot& snapshot);
CMapChunk* GetChunk(int32_t row, int32_t col);
int32_t ChunkCoord(float coord);
void DrawBrush(const C3Vector& pos, const C3Vector& camPos);
void Reset();

}

#endif
