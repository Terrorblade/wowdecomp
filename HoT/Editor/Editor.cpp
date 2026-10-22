#include "HoT/Editor/Editor.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "HoT/Core/Modules.h"
#include "HoT/Editor/CreatureEditor.h"
#include "HoT/Editor/Dbc/DbcEditor.h"
#include "HoT/Editor/FreeCam.h"
#include "HoT/Editor/GobEditor.h"
#include "HoT/Editor/Light/LightEditor.h"
#include "HoT/Editor/Map/Map.h"
#include "HoT/Editor/Model/ModelEditor.h"
#include "HoT/Editor/Mode.h"
#include "HoT/Editor/MouseFunctions.h"
#include "HoT/Editor/Selection.h"
#include "HoT/Editor/Spell/SpellPreview.h"
#include "HoT/Editor/Spell/WorldTables.h"
#include "HoT/Editor/TemplateSearch.h"
#include "HoT/Editor/WorldPreview.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Editor {

void MouseToWorld(CGWorldFrame* frame, float mouseX, float mouseY, C3Vector* start, C3Vector* end) {
    CGxDevice* device = g_theGxDevicePtr;

    if (!device || !frame || !frame->m_camera) {
        return;
    }

    // HoT: the DLL kept pointers to the live matrices, so its restore wrote back the new ones.
    C44Matrix projection = device->m_projection;
    CGxMatrixStack& stack = device->m_xforms[GxXform_View];
    C44Matrix view = stack.m_mtx[stack.m_level];
    frame->m_camera->SetupWorldProjection(frame->m_viewportDDC);

    if (!frame->GetLineSegment(mouseX, mouseY, start, end)) {
        *start = { 0.0f, 0.0f, 0.0f };
        *end = { 0.0f, 0.0f, 0.0f };
    }

    device->XformSetProjection(projection);
    device->XformSetView(view);
}

void BeforeWorldRender(CGWorldFrame* frame, C3Vector& decalPosition, float& decalSize, int32_t& decalStyle) {
    Mode::OnDecal(decalPosition, decalSize, decalStyle);
}

void AfterWorldRender(CGWorldFrame* frame) {
    CreatureEditor::OnWorldRender(frame);
    LightEditor::OnWorldRender(frame);
    Mode::OnWorldRender(frame);
}

int32_t OnHitTestPoint(int32_t kind, const HITTESTRESULT* result) {
    MouseFunctions::RecordHitTest(kind, result);

    return kind;
}

CameraUpdateScope::~CameraUpdateScope() {
    FreeCam::OnCameraUpdated(this->m_camera);
}

class EditorModule : public Module {
    public:
    const char* Name() const override {
        return "Editor";
    }

    void OnInitializeGlobal() override {
        GobEditor::Apply();
        CreatureEditor::Apply();
        Selection::Apply();
        Map::Apply();
        FreeCam::RegisterScripts();
        MouseFunctions::RegisterScripts();
        Mode::RegisterScripts();
        Dbc::Apply();
        LightEditor::Apply();
        SpellPreview::Apply();
        WorldTables::Apply();
        TemplateSearch::Apply();
        ModelEditor::Apply();
        WorldPreview::Apply();
    }

    void OnPlayerClientInitialize() override {
        FreeCam::OnGameInitialize();
        Mode::OnGameInitialize();
    }

    void OnLoadingScreenStarted() override {
        SpellPreview::Stop();
        WorldPreview::OnLoadingScreenStarted();
        Mode::OnWorldUnloaded();
    }

    void OnIdle() override {
        Dbc::OnIdle();
        SpellPreview::OnIdle();
        WorldPreview::OnIdle();
    }

    void OnScriptsLoaded(Lua::State state) override {
        if (state == Lua::State::Game) {
            Mode::OnScriptsReloaded();
        }
    }

    void OnDestroyGame() override {
        Mode::OnGameDestroy();
        Selection::OnGameDestroy();
        GobEditor::OnGameDestroy();
        CreatureEditor::OnGameDestroy();
        FreeCam::OnGameDestroy();
        SpellPreview::OnGameDestroy();
        WorldPreview::OnGameDestroy();
        Dbc::OnGameDestroy();
        LightEditor::OnGameDestroy();
        WorldTables::OnGameDestroy();
        TemplateSearch::OnGameDestroy();
        ModelEditor::OnGameDestroy();
    }
};

Module* GetModule() {
    static EditorModule module;
    return &module;
}

}
