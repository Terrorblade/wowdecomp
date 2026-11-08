#include "HoT/Editor/Map/Map.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Tempest/CRect.h"
#include "HoT/Editor/FreeCam.h"
#include "HoT/Editor/Map/ChunkEdit.h"
#include "HoT/Editor/Map/EditorScripts.h"
#include "HoT/Editor/Map/EditorUndo.h"
#include "HoT/Editor/Map/EditorWorld.h"
#include "HoT/Editor/Map/ObjectEdit.h"
#include "HoT/Editor/Map/Runtime.h"
#include "HoT/Editor/Map/TerrainEdit.h"
#include "HoT/Editor/Map/TextureEdit.h"
#include "HoT/Editor/Map/WorldHooks.h"
#include "HoT/Editor/Mode.h"
#include "HoT/Game/Session.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/Map.h"

namespace HoT::Editor::Map {

static const int8_t kRequiredSecurityLevel = 3;
static const float kMaxRayLength = 2000.0f;

static bool AltDown() {
    return OsInputIsKeyDown(KEY_LALT) || OsInputIsKeyDown(KEY_RALT);
}

static bool ShiftDown() {
    return OsInputIsKeyDown(KEY_LSHIFT) || OsInputIsKeyDown(KEY_RSHIFT);
}

static bool CtrlDown() {
    return OsInputIsKeyDown(KEY_LCONTROL) || OsInputIsKeyDown(KEY_RCONTROL);
}

static float PixelHeight() {
    CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (g_theGxDevicePtr) {
        g_theGxDevicePtr->CapsWindowSize(window);
    }

    float height = window.maxY - window.minY;
    return height > 1.0f ? height : 1.0f;
}

static void EndStrokes() {
    TerrainEdit::EndStroke();
    TextureEdit::EndStroke();
    ChunkEdit::EndStroke();
}

class MapHandler : public Mode::Handler {
    public:
    bool m_leftDown = false;
    bool m_middleDown = false;
    bool m_leftDragged = false;
    int32_t m_pressX = 0;
    int32_t m_pressY = 0;
    int32_t m_lastX = 0;
    int32_t m_lastY = 0;
    bool m_cursorValid = false;
    C3Vector m_cursor = { 0.0f, 0.0f, 0.0f };
    bool m_rayValid = false;
    C3Vector m_rayStart = { 0.0f, 0.0f, 0.0f };
    C3Vector m_rayDir = { 0.0f, 0.0f, 0.0f };
    float m_rayLength = 0.0f;
    float m_fps = 0.0f;
    float m_fpsTime = 0.0f;
    uint32_t m_fpsFrames = 0;

    bool OwnsTool(const char* tool) const override {
        return tool && !strncmp(tool, "map.", 4);
    }

    bool OnMouseDown(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) override;
    bool OnMouseUp(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) override;
    bool OnMouseWheel(int32_t delta, uint32_t metaKeys) override;
    void OnUpdate(float seconds) override;
    void OnWorldRender(CGWorldFrame* frame) override;
    void OnDecal(C3Vector& position, float& size, int32_t& style) override;
    void OnActivate() override;
    void OnDeactivate() override;
    void OnToolChanged(const char* tool) override;
    void OnWorldUnloaded() override;

    void UpdateCursor();
    void ReleaseButtons();
    void UpdateTool(float elapsedSec, float dx, float dy);
    void EndEdits();
};

static MapHandler s_handler;

static bool CanEdit() {
    return Runtime::HasAccess() && Runtime::IsMapLoaded();
}

void MapHandler::UpdateCursor() {
    this->m_cursorValid = false;
    this->m_rayValid = false;

    if (!CanEdit() || Mode::IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        return;
    }

    if (!this->m_leftDown && !this->m_middleDown && !Mode::IsMouseOverWorld()) {
        return;
    }

    C3Vector start;
    C3Vector end;

    if (!Mode::GetMouseRay(start, end)) {
        return;
    }

    C3Vector dir(end.x - start.x, end.y - start.y, end.z - start.z);
    float length = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

    if (length < 0.001f) {
        return;
    }

    dir.x /= length;
    dir.y /= length;
    dir.z /= length;

    this->m_rayValid = true;
    this->m_rayStart = start;
    this->m_rayDir = dir;
    this->m_rayLength = std::min(length, kMaxRayLength);
    this->m_cursorValid = TerrainEdit::Raycast(start, dir, this->m_rayLength, &this->m_cursor);
}

void MapHandler::ReleaseButtons() {
    if (this->m_middleDown && !Mode::IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        this->m_middleDown = false;
        ObjectEdit::EndEdit();
    }

    if (this->m_leftDown && !Mode::IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        this->m_leftDown = false;
        EndStrokes();
    }
}

void MapHandler::EndEdits() {
    this->m_leftDown = false;
    this->m_middleDown = false;
    EndStrokes();
    ObjectEdit::EndEdit();
}

bool MapHandler::OnMouseDown(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) {
    if (!CanEdit()) {
        return false;
    }

    OsInputGetMousePosition(&this->m_lastX, &this->m_lastY);
    TerrainEdit::MODE mode = TerrainEdit::s_mode;

    if (button == MOUSE_BUTTON_MIDDLE) {
        if (mode == TerrainEdit::MODE_OBJECT && ObjectEdit::GetSelectionCount()) {
            this->m_middleDown = true;
            ObjectEdit::BeginEdit();
        }

        if (mode == TerrainEdit::MODE_VERTEX) {
            this->UpdateCursor();

            if (this->m_cursorValid && ChunkEdit::PickVertexColor(this->m_cursor)) {
                Runtime::RunLua("if MapEditor_OnColorPicked then MapEditor_OnColorPicked() end");
            }
        }

        return true;
    }

    if (button == MOUSE_BUTTON_LEFT) {
        this->m_leftDown = true;
        this->m_leftDragged = false;
        this->m_pressX = this->m_lastX;
        this->m_pressY = this->m_lastY;
        this->UpdateCursor();

        if (mode == TerrainEdit::MODE_TEXTURE_PAINT && CtrlDown() && !ShiftDown() && this->m_cursorValid) {
            TextureEdit::PickTexture(this->m_cursor);
        }

        return true;
    }

    return false;
}

bool MapHandler::OnMouseUp(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) {
    if (button == MOUSE_BUTTON_MIDDLE) {
        if (this->m_middleDown) {
            this->m_middleDown = false;
            ObjectEdit::EndEdit();
        }

        return true;
    }

    if (button != MOUSE_BUTTON_LEFT) {
        return false;
    }

    bool click = this->m_leftDown && !this->m_leftDragged;
    this->m_leftDown = false;
    EndStrokes();

    if (!click || !CanEdit() || TerrainEdit::s_mode != TerrainEdit::MODE_OBJECT || AltDown()) {
        return true;
    }

    this->UpdateCursor();

    bool add = ShiftDown() || CtrlDown();
    int32_t kind;
    uint32_t uniqueId;
    float distance;
    bool hit = this->m_rayValid && ObjectEdit::Pick(this->m_rayStart, this->m_rayDir, this->m_rayLength, &kind, &uniqueId, &distance);

    // Objects behind the terrain under the cursor do not count.
    if (hit && this->m_cursorValid) {
        C3Vector toCursor(this->m_cursor.x - this->m_rayStart.x, this->m_cursor.y - this->m_rayStart.y, this->m_cursor.z - this->m_rayStart.z);
        hit = distance <= sqrtf(toCursor.x * toCursor.x + toCursor.y * toCursor.y + toCursor.z * toCursor.z) + 0.5f;
    }

    if (hit) {
        ObjectEdit::Select(kind, uniqueId, add);
    } else if (!add) {
        ObjectEdit::ClearSelection();
    }

    return true;
}

bool MapHandler::OnMouseWheel(int32_t delta, uint32_t metaKeys) {
    if (!CanEdit()) {
        return false;
    }

    float step = static_cast<float>(delta);

    if (AltDown()) {
        TerrainEdit::ChangeRadius(step * 2.0f);
        return true;
    }

    if (CtrlDown()) {
        TerrainEdit::ChangeSpeed(step * 0.5f);
        return true;
    }

    if (ShiftDown()) {
        TerrainEdit::ChangeInnerRadius(step * 0.05f);
        return true;
    }

    return false;
}

void MapHandler::UpdateTool(float elapsedSec, float dx, float dy) {
    bool alt = AltDown();
    bool shift = ShiftDown();
    bool ctrl = CtrlDown();
    TerrainEdit::MODE mode = TerrainEdit::s_mode;

    if (this->m_middleDown && mode == TerrainEdit::MODE_OBJECT && (dx != 0.0f || dy != 0.0f)) {
        float vertical = -dy / PixelHeight();

        if (ctrl) {
            float amount = (dx + dy) / 3.0f;

            if (alt) {
                ObjectEdit::RotateBy(C3Vector(0.0f, 0.0f, amount));
            } else if (shift) {
                ObjectEdit::RotateBy(C3Vector(amount, 0.0f, 0.0f));
            } else {
                ObjectEdit::RotateBy(C3Vector(0.0f, amount, 0.0f));
            }
        } else if (alt) {
            ObjectEdit::ScaleBy(powf(2.0f, vertical * 4.0f));
        } else if (shift) {
            ObjectEdit::MoveBy(C3Vector(0.0f, 0.0f, vertical * 80.0f));
        } else if (ObjectEdit::s_settings.m_followCursor && this->m_cursorValid && !ObjectEdit::HasWmoDoodadSelection()) {
            ObjectEdit::MoveTo(this->m_cursor);
        } else {
            C3Vector camera = Runtime::CameraPosition();
            C3Vector position;
            C3Vector rotation;
            float scale;
            float distance = 50.0f;

            if (ObjectEdit::GetTransform(&position, &rotation, &scale)) {
                float wx = 17066.666f - position.z - camera.x;
                float wy = 17066.666f - position.x - camera.y;
                distance = std::max(sqrtf(wx * wx + wy * wy), 10.0f);
            }

            CGCamera* activeCamera = CGWorldFrame::GetActiveCamera();
            C3Vector look = activeCamera ? activeCamera->Forward() : C3Vector(1.0f, 0.0f, 0.0f);
            float yaw = atan2f(look.y, look.x);
            float horizontal = dx / PixelHeight();
            C3Vector forward(cosf(yaw), sinf(yaw), 0.0f);
            C3Vector right(sinf(yaw), -cosf(yaw), 0.0f);
            float k = distance * 1.2f;
            ObjectEdit::MoveBy(C3Vector((right.x * horizontal + forward.x * vertical) * k, (right.y * horizontal + forward.y * vertical) * k, 0.0f));
        }
    }

    if (!this->m_leftDown) {
        return;
    }

    int32_t x;
    int32_t y;
    OsInputGetMousePosition(&x, &y);

    if (abs(x - this->m_pressX) + abs(y - this->m_pressY) > 10) {
        this->m_leftDragged = true;
    }

    if (alt && !shift && !ctrl) {
        TerrainEdit::ChangeRadius(dx / 15.0f);
        return;
    }

    if (!this->m_cursorValid) {
        return;
    }

    if (mode == TerrainEdit::MODE_RAISE_LOWER) {
        if (shift) {
            TerrainEdit::Apply(TerrainEdit::OP_RAISE, this->m_cursor, elapsedSec);
        } else if (ctrl) {
            TerrainEdit::Apply(TerrainEdit::OP_LOWER, this->m_cursor, elapsedSec);
        }
    } else if (mode == TerrainEdit::MODE_FLATTEN_BLUR) {
        if (shift) {
            TerrainEdit::Apply(TerrainEdit::OP_FLATTEN, this->m_cursor, elapsedSec);
        } else if (ctrl) {
            TerrainEdit::Apply(TerrainEdit::OP_BLUR, this->m_cursor, elapsedSec);
        }
    } else if (mode == TerrainEdit::MODE_TEXTURE_PAINT && shift && !ctrl) {
        TextureEdit::Paint(this->m_cursor, elapsedSec);
    } else if (mode == TerrainEdit::MODE_HOLES) {
        if (shift) {
            ChunkEdit::SetHoles(this->m_cursor, alt, false);
        } else if (ctrl) {
            ChunkEdit::SetHoles(this->m_cursor, alt, true);
        }
    } else if (mode == TerrainEdit::MODE_AREA) {
        if (shift) {
            ChunkEdit::SetAreaId(this->m_cursor);
        } else if (ctrl && ChunkEdit::PickAreaId(this->m_cursor) >= 0) {
            Runtime::RunLua("if MapEditor_OnAreaPicked then MapEditor_OnAreaPicked() end");
        }
    } else if (mode == TerrainEdit::MODE_IMPASS) {
        if (shift) {
            ChunkEdit::SetImpass(this->m_cursor, true);
        } else if (ctrl) {
            ChunkEdit::SetImpass(this->m_cursor, false);
        }
    } else if (mode == TerrainEdit::MODE_VERTEX && (shift ^ ctrl)) {
        ChunkEdit::PaintVertexColor(this->m_cursor, elapsedSec, shift);
    }
}

void MapHandler::OnUpdate(float seconds) {
    this->m_fpsFrames++;
    this->m_fpsTime += seconds;

    if (this->m_fpsTime >= 0.5f) {
        this->m_fps = this->m_fpsFrames / this->m_fpsTime;
        this->m_fpsFrames = 0;
        this->m_fpsTime = 0.0f;
    }

    int32_t x;
    int32_t y;
    OsInputGetMousePosition(&x, &y);
    float dx = static_cast<float>(x - this->m_lastX);
    float dy = static_cast<float>(y - this->m_lastY);
    this->m_lastX = x;
    this->m_lastY = y;

    if (!CanEdit()) {
        this->m_cursorValid = false;
        this->m_rayValid = false;

        if (this->m_leftDown || this->m_middleDown) {
            this->EndEdits();
        }

        return;
    }

    this->ReleaseButtons();

    if (!Mode::ToolHasPrefix("map.")) {
        this->m_cursorValid = false;
        this->m_rayValid = false;
        return;
    }

    this->UpdateCursor();
    this->UpdateTool(std::min(seconds, 0.25f), dx, dy);
}

void MapHandler::OnWorldRender(CGWorldFrame* frame) {
    if (!frame || !frame->m_camera || !g_theGxDevicePtr || !CanEdit()) {
        return;
    }

    const C3Vector& camera = frame->m_camera->m_position;
    bool toolActive = Mode::ToolHasPrefix("map.");

    ChunkEdit::DrawOverlay(camera, toolActive);

    if (toolActive && this->m_cursorValid && TerrainEdit::s_mode != TerrainEdit::MODE_OBJECT) {
        TerrainEdit::DrawBrush(this->m_cursor, camera);
    }

    ObjectEdit::DrawSelection(camera);
}

void MapHandler::OnDecal(C3Vector& position, float& size, int32_t& style) {
    if (!this->m_cursorValid || !Mode::ToolHasPrefix("map.") || Spell_C_IsTargeting()) {
        return;
    }

    TerrainEdit::MODE mode = TerrainEdit::s_mode;

    if (mode == TerrainEdit::MODE_OBJECT || mode == TerrainEdit::MODE_IMPASS) {
        return;
    }

    float radius = TerrainEdit::GetRadius();

    if (radius <= 0.0f) {
        return;
    }

    position = this->m_cursor;
    size = radius;
    style = 0;
}

void MapHandler::OnActivate() {
    EditorWorld::SaveState();
}

void MapHandler::OnDeactivate() {
    this->EndEdits();
    this->m_cursorValid = false;
    this->m_rayValid = false;
    EditorWorld::RestoreState();
}

void MapHandler::OnToolChanged(const char* tool) {
    this->EndEdits();

    static const struct {
        const char* m_name;
        TerrainEdit::MODE m_mode;
    } s_tools[] = {
        { "map.raise", TerrainEdit::MODE_RAISE_LOWER },
        { "map.flatten", TerrainEdit::MODE_FLATTEN_BLUR },
        { "map.texture", TerrainEdit::MODE_TEXTURE_PAINT },
        { "map.object", TerrainEdit::MODE_OBJECT },
        { "map.holes", TerrainEdit::MODE_HOLES },
        { "map.area", TerrainEdit::MODE_AREA },
        { "map.impass", TerrainEdit::MODE_IMPASS },
        { "map.vertex", TerrainEdit::MODE_VERTEX },
    };

    for (const auto& entry : s_tools) {
        if (tool && !strcmp(tool, entry.m_name)) {
            TerrainEdit::s_mode = entry.m_mode;
            return;
        }
    }
}

void MapHandler::OnWorldUnloaded() {
    this->EndEdits();
    this->m_cursorValid = false;
    this->m_rayValid = false;
}

void BeforePrepareUpdate(bool all) {
    TextureEdit::FreeRetired();
    ChunkEdit::FreeRetired();

    if (!CMap::bActive) {
        return;
    }

    TextureEdit::Rebind();
    ChunkEdit::Rebind();
    TextureEdit::FlushRenderRebuilds();
    EditorWorld::ParkDirtyAreas(all);
}

void AfterPrepareUpdate() {
    EditorWorld::UnparkAreas();
    EditorWorld::ApplyTimeOfDay();
}

void ApplyTimeOverride() {
    EditorWorld::ApplyTimeOfDay();
}

void OnWorldUnload() {
    s_handler.OnWorldUnloaded();
    TerrainEdit::Reset();
    TextureEdit::Reset();
    ChunkEdit::Reset();
    ObjectEdit::Reset();
    EditorUndo::Clear();
}

bool PickObject(const C3Vector& start, const C3Vector& end, int32_t& kind, uint32_t& uniqueId, float& distance) {
    if (!CanEdit()) {
        return false;
    }

    C3Vector dir(end.x - start.x, end.y - start.y, end.z - start.z);
    float length = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

    if (length < 0.001f) {
        return false;
    }

    dir.x /= length;
    dir.y /= length;
    dir.z /= length;

    return ObjectEdit::Pick(start, dir, std::min(length, kMaxRayLength), &kind, &uniqueId, &distance);
}

void SelectObject(int32_t kind, uint32_t uniqueId, bool add) {
    ObjectEdit::Select(kind, uniqueId, add);
}

void ClearObjectSelection() {
    ObjectEdit::ClearSelection();
}

void Apply() {
    Mode::RegisterHandler(&s_handler);
    EditorScripts::Register();
}

}

namespace HoT::Editor::Map::Runtime {

bool HasAccess() {
    return Session::HasClearance(kRequiredSecurityLevel);
}

bool IsMapLoaded() {
    return CMap::bActive != 0 && CMap::mapName[0] != '\0';
}

const char* MapDirectory() {
    return CMap::mapName;
}

uint32_t MapId() {
    return ClntObjMgrGetMapID();
}

bool CursorValid() {
    return s_handler.m_cursorValid;
}

const C3Vector& Cursor() {
    return s_handler.m_cursor;
}

C3Vector CameraPosition() {
    CGCamera* camera = CGWorldFrame::GetActiveCamera();
    return camera ? camera->m_position : FreeCam::GetPosition();
}

float Fps() {
    return s_handler.m_fps;
}

void EndEdits() {
    s_handler.EndEdits();
}

void RunLua(const char* code) {
    FrameScript_Execute(code, "MapEditor", nullptr);
}

}
