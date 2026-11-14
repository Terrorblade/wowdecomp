#ifndef HOT_EDITOR_MAP_FLIGHTBOUNDSEDIT_H
#define HOT_EDITOR_MAP_FLIGHTBOUNDSEDIT_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "HoT/Editor/Map/EditorUndo.h"

// MFBO editing, the per-tile 3x3 lower (fall into void camera) and upper (flying ceiling) planes.
namespace HoT::Editor::Map::FlightBoundsEdit {

enum LAYER {
    LAYER_LOWER = 0,
    LAYER_UPPER = 1,
};

struct Settings {
    int32_t m_layer;
    int32_t m_height;
    float m_radius;
    bool m_fromCursor;
    float m_offset;
    bool m_showUpper;
};

struct Info {
    int32_t m_tileX;
    int32_t m_tileY;
    bool m_hasChunk;
    float m_lowerAt;
    float m_upperAt;
    int32_t m_vertexLower;
    int32_t m_vertexUpper;
};

extern Settings s_settings;

// Height a paint stroke writes, the setting or the cursor height minus the offset.
int32_t StrokeHeight(const C3Vector& cursor);
// Sets the active layer on every vertex in the brush, the nearest vertex at radius 0.
void Paint(const C3Vector& cursor);
// Copies the nearest vertex of the active layer into the height setting.
bool PickHeight(const C3Vector& cursor);
void FillTile(const C3Vector& pos);
void FillLoaded();
bool GetInfo(const C3Vector& pos, Info* info);
void EndStroke();
bool Capture(uint32_t areaIndex, EditorUndo::FlightSnapshot& snapshot);
void Restore(const EditorUndo::FlightSnapshot& snapshot);
void DrawOverlay(const C3Vector& camPos, const C3Vector* cursor);

}

#endif
