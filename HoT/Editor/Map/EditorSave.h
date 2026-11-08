#ifndef HOT_EDITOR_MAP_EDITORSAVE_H
#define HOT_EDITOR_MAP_EDITORSAVE_H

#include <cstdint>
#include <string>
#include <vector>
#include "WoW/Source/WorldClient/Map.h"

class CMapArea;

namespace HoT::Editor::Map::EditorSave {

enum MODE {
    SAVE_CURRENT = 0,
    SAVE_CHANGED = 1,
    SAVE_ALL = 2,
};

struct AddedObject {
    int32_t m_kind;
    std::string m_fileName;
    SMDoodadDef m_doodad;
    SMMapObjDef m_mapObj;
};

struct ObjectChanges {
    std::vector<uint32_t> m_removedDoodads;
    std::vector<uint32_t> m_removedMapObjs;
    std::vector<AddedObject> m_added;
};

bool BuildTile(CMapArea* area, std::vector<uint8_t>& out, const ObjectChanges* changes = nullptr);
bool ReloadArea(CMapArea* area, const std::vector<uint8_t>& data);
bool ApplyObjectChanges(CMapArea* area, const ObjectChanges& changes);

// Writes a loose file at its game path, keeping a .bak of the original.
bool WriteGameFile(const std::string& gamePath, const void* data, size_t size);

// Writes loose ADTs under <exe dir>\World\Maps\<map>, returns the tile count or -1 with error set.
int32_t Save(MODE mode, std::string& error);

}

#endif
