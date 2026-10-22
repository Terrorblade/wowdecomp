#ifndef HOT_EDITOR_MODEL_PREVIEWATTACH_H
#define HOT_EDITOR_MODEL_PREVIEWATTACH_H

#include <cstdint>

class CM2Model;
class CSimpleModel;
struct lua_State;

// Markers and naked human male riders on attachment ids of editor Model frames or of world preview objects.
namespace HoT::Editor::ModelEditor::PreviewAttach {

void Apply(bool (*hasAccess)());
void OnGameDestroy();
void OnFrameDestroyed(CSimpleModel* frame);

bool SetWorldSlot(lua_State* L, uint32_t host, uint32_t index, int32_t options);
void UpdateWorld(uint32_t host, CM2Model* parent);
void DetachWorld(uint32_t host);
void FreeWorld(uint32_t host);

}

#endif
