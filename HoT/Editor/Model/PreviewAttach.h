#ifndef HOT_EDITOR_MODEL_PREVIEWATTACH_H
#define HOT_EDITOR_MODEL_PREVIEWATTACH_H

class CSimpleModel;

// Riders and markers on the attachment points of editor Model frames, plus skins and sequence lists of what they show.
namespace HoT::Editor::ModelEditor::PreviewAttach {

void Apply(bool (*hasAccess)());

void OnGameDestroy();

// Called first thing in ~CSimpleModel so the attached models go away while the frame scene still exists.
void OnFrameDestroyed(CSimpleModel* frame);

}

#endif
