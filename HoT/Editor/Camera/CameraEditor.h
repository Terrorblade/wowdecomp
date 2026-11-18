#ifndef HOT_EDITOR_CAMERA_CAMERAEDITOR_H
#define HOT_EDITOR_CAMERA_CAMERAEDITOR_H

#include "Engine/Source/Tempest/C3Vector.h"

class CGCamera;

// Cinematic camera editor: edits a camera M2 in the world, previews it through the game camera and saves it loose.
namespace HoT::Editor::CameraEditor {

void Apply();
void OnGameDestroy();

// After the free camera, position is where the game camera was before the free camera moved it.
void OnCameraUpdated(CGCamera* camera, const C3Vector& position);

}

#endif
