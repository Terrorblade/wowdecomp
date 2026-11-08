#ifndef HOT_EDITOR_MAP_WMODOODADEDIT_H
#define HOT_EDITOR_MAP_WMODOODADEDIT_H

#include <cstdint>
#include <string>
#include "Engine/Source/Tempest/C3Vector.h"

class C44Matrix;
class CAaBox;
class CMapObj;
struct SMODoodadDef;

namespace HoT::Editor::Map::WmoDoodadEdit {

// A handle names one doodad of one placed WMO and stays valid until the world unloads.
uint32_t GetHandle(uint32_t mapObjUniqueId, uint16_t doodadRef);
bool Exists(uint32_t handle);
bool GetBounds(uint32_t handle, CAaBox& box, C44Matrix& matrix);
const char* GetFileName(uint32_t handle);

// Transforms use the same world position and euler convention as ADT placements.
bool Read(uint32_t handle, C3Vector& world, C3Vector& rotation, float& scale);
void Write(uint32_t handle, const C3Vector& world, const C3Vector& rotation, float scale);
void Remove(uint32_t handle);
void Restore(uint32_t handle);

// The placement CreateRefs should use, null when the doodad was deleted.
SMODoodadDef* Resolve(CMapObj* mapObj, uint16_t doodadRef);

// Writes every edited WMO as loose files, returns the file count written.
int32_t Save(std::string& error);
void Reset();

}

#endif
