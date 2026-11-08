#ifndef HOT_EDITOR_MAP_OBJECTEDIT_H
#define HOT_EDITOR_MAP_OBJECTEDIT_H

#include <cstdint>
#include <vector>
#include "Engine/Source/Tempest/C3Vector.h"
#include "HoT/Editor/Map/EditorSave.h"
#include "HoT/Editor/Map/EditorUndo.h"

class CMapArea;

namespace HoT::Editor::Map::ObjectEdit {

enum KIND {
    KIND_DOODAD = 0,
    KIND_WMO = 1,
    KIND_WMO_DOODAD = 2,
};

struct Settings {
    bool m_followCursor;
    bool m_snapMultiToGround;
    bool m_pickWmoDoodads;
};

extern Settings s_settings;

bool Pick(const C3Vector& start, const C3Vector& dir, float maxDistance, int32_t* kind, uint32_t* uniqueId, float* distance);
void Select(int32_t kind, uint32_t uniqueId, bool add);
void ClearSelection();
void Reset();
CMapArea* FindArea(int32_t kind, uint32_t uniqueId);
void DeleteSelection();
void CopySelection();
bool HasClipboard();
void Paste(const C3Vector* worldPos);
void Duplicate();
bool SetClipboardModel(const char* fileName);
uint32_t GetModelCount();
const char* GetModel(uint32_t index);
void RemoveObjects(const std::vector<EditorSave::AddedObject>& objects, bool record);
void AddObjects(const std::vector<EditorSave::AddedObject>& objects, bool record);
uint32_t GetSelectionCount();
bool HasWmoDoodadSelection();
bool GetSelectionInfo(uint32_t index, int32_t* kind, uint32_t* uniqueId, const char** fileName);
bool GetTransform(C3Vector* position, C3Vector* rotation, float* scale);
void SetTransform(const C3Vector* position, const C3Vector* rotation, const float* scale);
void BeginEdit();
void EndEdit();
void MoveBy(const C3Vector& delta);
void MoveTo(const C3Vector& worldPos);
void RotateBy(const C3Vector& degrees);
void ScaleBy(float factor);
void SnapToGround();
bool Capture(int32_t kind, uint32_t uniqueId, EditorUndo::ObjectSnapshot& snapshot);
void Restore(const EditorUndo::ObjectSnapshot& snapshot);
void DrawSelection(const C3Vector& camPos);

}

#endif
