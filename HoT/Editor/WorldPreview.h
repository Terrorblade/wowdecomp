#ifndef HOT_EDITOR_WORLDPREVIEW_H
#define HOT_EDITOR_WORLDPREVIEW_H

#include <cstdint>

// Client only units and gameobjects in front of the player for the editor previews, made from fake SMSG_UPDATE_OBJECT packets.
namespace HoT::Editor::WorldPreview {

bool IsPreviewGuid(uint64_t guid);

void Apply();
void OnIdle();
void OnLoadingScreenStarted();
void OnGameDestroy();

}

#endif
