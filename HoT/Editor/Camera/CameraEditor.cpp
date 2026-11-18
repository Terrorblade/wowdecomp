#include "HoT/Editor/Camera/CameraEditor.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Model2/M2Cache.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Os/TimeManager.h"
#include "HoT/Editor/Camera/CameraDoc.h"
#include "HoT/Editor/FreeCam.h"
#include "HoT/Editor/GizmoDraw.h"
#include "HoT/Editor/GizmoPick.h"
#include "HoT/Editor/Mode.h"
#include "HoT/Editor/Model/ModelFiles.h"
#include "HoT/Editor/Resources.h"
#include "HoT/Editor/VectorMath.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Platform/Input.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CinematicCameraRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/World.h"

namespace HoT::Editor::CameraEditor {

using namespace Camera;
using namespace VectorMath;
namespace Files = Model::Files;

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr size_t kUndoLimit = 200;
static constexpr float kGizmoScreenScale = 0.12f;
static constexpr float kFallbackTargetDistance = 40.0f;

enum PART : int32_t {
    PART_VALUE = 0,
    PART_IN = 1,
    PART_OUT = 2,
};

struct PointRef {
    int32_t m_track = -1;
    int32_t m_index = -1;
    int32_t m_part = PART_VALUE;

    bool Valid() const {
        return m_track >= 0 && m_index >= 0;
    }

    bool operator==(const PointRef& other) const {
        return m_track == other.m_track && m_index == other.m_index && m_part == other.m_part;
    }
};

enum class DragType {
    None,
    Axis,
    Plane,
};

struct DragState {
    DragType m_type = DragType::None;
    Axis m_axis = Axis::None;
    C3Vector m_start{};
    C3Vector m_anchor{};
    C3Vector m_normal{};
    bool m_pushedUndo = false;
};

struct EngineState {
    bool m_playing = false;
    bool m_ended = false;
    uint32_t m_startMs = 0;
    uint32_t m_counter = 0;
    std::string m_dir;
};

struct State {
    Doc m_doc;
    uint32_t m_revision = 1;
    uint32_t m_savedRevision = 1;
    std::vector<Doc> m_undo;
    std::vector<Doc> m_redo;

    PointRef m_selected;
    PointRef m_hover;
    Axis m_hoverAxis = Axis::None;
    DragState m_drag;

    float m_time = 0.0f;
    float m_rate = 1.0f;
    bool m_playing = false;
    bool m_loop = false;
    bool m_preview = false;
    bool m_active = false;

    bool m_showTarget = true;
    bool m_showHandles = true;
    bool m_allHandles = false;
    bool m_showSpeed = true;
    bool m_showFrustum = true;

    EngineState m_engine;
};

static State s_state;

static CImVector Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return CImVector(b, g, r, a);
}

static bool HasAccess() {
    return Session::HasClearance(kRequiredSecurityLevel);
}

static CGWorldFrame* Frame() {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    return frame && frame->m_camera ? frame : nullptr;
}

static CGCamera* WorldCamera() {
    CGWorldFrame* frame = Frame();

    return frame ? frame->m_camera : nullptr;
}

static C3Vector ViewPosition() {
    CGCamera* camera = WorldCamera();

    return camera ? camera->m_position : C3Vector{};
}

static void Changed() {
    s_state.m_revision++;
}

static int32_t KeyCount(int32_t track) {
    return track >= 0 && track < TRACK_COUNT ? static_cast<int32_t>(s_state.m_doc.m_keys[track].size()) : 0;
}

static void ClampSelection() {
    PointRef& selected = s_state.m_selected;

    if (selected.Valid() && selected.m_index >= KeyCount(selected.m_track)) {
        selected = KeyCount(selected.m_track) > 0 ? PointRef{ selected.m_track, KeyCount(selected.m_track) - 1, PART_VALUE } : PointRef{};
    }

    s_state.m_hover = {};
}

static void PushUndo() {
    s_state.m_undo.push_back(s_state.m_doc);

    if (s_state.m_undo.size() > kUndoLimit) {
        s_state.m_undo.erase(s_state.m_undo.begin());
    }

    s_state.m_redo.clear();
}

static void Undo() {
    if (s_state.m_undo.empty()) {
        return;
    }

    s_state.m_redo.push_back(s_state.m_doc);
    s_state.m_doc = s_state.m_undo.back();
    s_state.m_undo.pop_back();
    ClampSelection();
    Changed();
}

static void Redo() {
    if (s_state.m_redo.empty()) {
        return;
    }

    s_state.m_undo.push_back(s_state.m_doc);
    s_state.m_doc = s_state.m_redo.back();
    s_state.m_redo.pop_back();
    ClampSelection();
    Changed();
}

// Points.

static bool HasHandle(int32_t track, int32_t index, int32_t part) {
    int32_t count = KeyCount(track);

    if (part == PART_IN) {
        return index > 0;
    }

    if (part == PART_OUT) {
        return index + 1 < count;
    }

    return true;
}

static bool IsVisible(const PointRef& ref) {
    if (!ref.Valid() || ref.m_track == TRACK_ROLL || ref.m_index >= KeyCount(ref.m_track)) {
        return false;
    }

    if (ref.m_track == TRACK_TARGET && !s_state.m_showTarget) {
        return false;
    }

    if (ref.m_part == PART_VALUE) {
        return true;
    }

    if (!s_state.m_showHandles || !IsSpline(s_state.m_doc.m_interp[ref.m_track]) || !HasHandle(ref.m_track, ref.m_index, ref.m_part)) {
        return false;
    }

    const PointRef& selected = s_state.m_selected;

    return s_state.m_allHandles || (selected.m_track == ref.m_track && selected.m_index == ref.m_index);
}

static C3Vector PointWorld(const PointRef& ref) {
    const Doc& doc = s_state.m_doc;
    const Key& key = doc.m_keys[ref.m_track][ref.m_index];

    switch (ref.m_part) {
        case PART_IN:
            return ToWorld(doc, Add(key.m_value, key.m_in));
        case PART_OUT:
            return ToWorld(doc, Add(key.m_value, key.m_out));
        default:
            return ToWorld(doc, key.m_value);
    }
}

static void SetPointWorld(const PointRef& ref, const C3Vector& world) {
    Doc& doc = s_state.m_doc;
    Key& key = doc.m_keys[ref.m_track][ref.m_index];

    if (ref.m_part == PART_VALUE) {
        key.m_value = ToLocal(doc, world);
        Normalize(doc, ref.m_track);
        return;
    }

    C3Vector offset = DirToLocal(doc, Subtract(world, ToWorld(doc, key.m_value)));
    SetHandle(doc, ref.m_track, ref.m_index, ref.m_part == PART_OUT, offset);
}

template <class Callback>
static void ForEachPoint(Callback callback) {
    for (int32_t track = TRACK_POSITION; track <= TRACK_TARGET; track++) {
        for (int32_t index = 0; index < KeyCount(track); index++) {
            for (int32_t part = PART_VALUE; part <= PART_OUT; part++) {
                PointRef ref{ track, index, part };

                if (IsVisible(ref)) {
                    callback(ref, PointWorld(ref));
                }
            }
        }
    }
}

static float ScreenSize(const C3Vector& point, float factor, float minimum, float maximum) {
    float distance = Length(Subtract(point, ViewPosition()));

    return std::clamp(distance * factor, minimum, maximum);
}

static float GizmoScale(const C3Vector& point) {
    return ScreenSize(point, kGizmoScreenScale, 0.75f, 60.0f);
}

static PointRef Pick(const C3Vector& start, const C3Vector& end) {
    C3Vector direction = Normalize(Subtract(end, start));
    PointRef best;
    float bestScore = FLT_MAX;

    ForEachPoint([&](const PointRef& ref, const C3Vector& point) {
        float along = Dot(Subtract(point, start), direction);

        if (along <= 0.0f) {
            return;
        }

        float miss = Length(Subtract(point, Add(start, Scale(direction, along))));
        float tolerance = std::max(0.25f, along * 0.015f);
        // Handles win ties so a handle sitting on its key can still be grabbed.
        float score = miss / tolerance - (ref.m_part != PART_VALUE ? 0.05f : 0.0f);

        if (miss <= tolerance && score < bestScore) {
            bestScore = score;
            best = ref;
        }
    });

    return best;
}

static bool RayPlane(const C3Vector& start, const C3Vector& end, const C3Vector& point, const C3Vector& normal, C3Vector& hit) {
    C3Vector direction = Subtract(end, start);
    float denominator = Dot(direction, normal);

    if (fabsf(denominator) < 1e-5f * Length(direction)) {
        return false;
    }

    float t = Dot(Subtract(point, start), normal) / denominator;

    if (t <= 0.0f) {
        return false;
    }

    hit = Add(start, Scale(direction, t));
    return true;
}

static bool TraceWorld(const C3Vector& start, const C3Vector& end, C3Vector& hit) {
    CGWorldFrame* frame = Frame();

    if (!frame) {
        return false;
    }

    HITTESTRESULT result{};

    if (!frame->HitTest(start, end, 0x3, &result)) {
        return false;
    }

    hit = Add(start, Scale(Normalize(Subtract(end, start)), result.m_distance));
    return true;
}

// Playback.

static void StopEngine() {
    EngineState& engine = s_state.m_engine;

    if (!engine.m_playing) {
        return;
    }

    CGCamera* camera = WorldCamera();

    if (camera && camera->m_model) {
        camera->ClearModelCamera();
    }

    if (!engine.m_dir.empty()) {
        Resources::RemovePreviews(engine.m_dir);
        engine.m_dir.clear();
    }

    engine.m_playing = false;
    engine.m_ended = false;
}

static void StopAll() {
    StopEngine();
    s_state.m_playing = false;
    s_state.m_preview = false;
    s_state.m_drag = {};
}

static void OnEngineSequence(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param) {
    s_state.m_engine.m_ended = true;
}

static std::string FileStemOr(const std::string& path, const char* fallback) {
    std::string stem = path.empty() ? std::string() : Files::FileStem(path);

    return stem.empty() ? std::string(fallback) : stem;
}

static bool PlayEngine(std::string& error) {
    CGCamera* camera = WorldCamera();

    if (!camera) {
        error = "no world camera";
        return false;
    }

    StopEngine();

    if (camera->m_model) {
        error = "the camera is already playing a cinematic";
        return false;
    }

    std::string problems = Validate(s_state.m_doc);

    if (s_state.m_doc.m_keys[TRACK_POSITION].empty() || s_state.m_doc.m_keys[TRACK_TARGET].empty()) {
        error = problems;
        return false;
    }

    std::string stem = FileStemOr(s_state.m_doc.m_path, "Camera");
    std::vector<uint8_t> m2;
    std::vector<uint8_t> skin;
    Build(s_state.m_doc, stem, m2, skin);

    // CM2Cache keys models by file name, a new folder and name each time keeps it from handing back the last build.
    EngineState& engine = s_state.m_engine;
    engine.m_counter++;
    engine.m_dir = std::string(Resources::kPreviewPrefix) + "Camera" + std::to_string(engine.m_counter) + "\\";
    std::string base = engine.m_dir + stem + "_c" + std::to_string(engine.m_counter);
    Resources::AddPreviewFile(base + ".m2", std::make_shared<const std::vector<uint8_t>>(std::move(m2)));
    Resources::AddPreviewFile(base + "00.skin", std::make_shared<const std::vector<uint8_t>>(std::move(skin)));

    s_state.m_playing = false;
    s_state.m_preview = false;

    C3Vector origin = s_state.m_doc.m_origin;

    if (!camera->SetModelCamera((base + ".m2").c_str(), &origin, s_state.m_doc.m_facing, &OnEngineSequence, 0)) {
        Resources::RemovePreviews(engine.m_dir);
        engine.m_dir.clear();
        error = "the client could not load the built camera";
        return false;
    }

    engine.m_playing = true;
    engine.m_ended = false;
    engine.m_startMs = OsGetAsyncTimeMs();

    return true;
}

static void UpdatePlayback(float seconds) {
    EngineState& engine = s_state.m_engine;

    if (engine.m_playing) {
        CGCamera* camera = WorldCamera();
        uint32_t elapsed = OsGetAsyncTimeMs() - engine.m_startMs;

        if (!camera || !camera->m_model || engine.m_ended || elapsed > s_state.m_doc.m_length + 2000) {
            StopEngine();
        } else {
            s_state.m_time = static_cast<float>(std::min(elapsed, s_state.m_doc.m_length));
        }

        return;
    }

    if (!s_state.m_playing) {
        return;
    }

    float length = static_cast<float>(s_state.m_doc.m_length);
    s_state.m_time += seconds * 1000.0f * s_state.m_rate;

    if (s_state.m_time >= length) {
        if (s_state.m_loop && length > 0.0f) {
            s_state.m_time = fmodf(s_state.m_time, length);
        } else {
            s_state.m_time = length;
            s_state.m_playing = false;
        }
    }
}

// Mouse.

static void EndDrag() {
    s_state.m_drag = {};
}

static void UpdateDrag() {
    DragState& drag = s_state.m_drag;

    if (drag.m_type == DragType::None) {
        return;
    }

    if (!Mode::IsMouseButtonHeld(MOUSE_BUTTON_LEFT) || !IsVisible(s_state.m_selected)) {
        EndDrag();
        return;
    }

    C3Vector start{};
    C3Vector end{};

    if (!Mode::GetMouseRay(start, end)) {
        return;
    }

    C3Vector point;

    if (drag.m_type == DragType::Axis) {
        C3Vector axis = AxisDirection(drag.m_axis);
        float t = ClosestRayAxisParameter(start, end, drag.m_start, axis);
        point = Add(drag.m_start, Subtract(Add(drag.m_start, Scale(axis, t)), drag.m_anchor));
    } else {
        C3Vector hit;

        if (!RayPlane(start, end, drag.m_start, drag.m_normal, hit)) {
            return;
        }

        point = Add(drag.m_start, Subtract(hit, drag.m_anchor));
    }

    if (Length(Subtract(point, PointWorld(s_state.m_selected))) < 0.0005f) {
        return;
    }

    if (!drag.m_pushedUndo) {
        PushUndo();
        drag.m_pushedUndo = true;
    }

    SetPointWorld(s_state.m_selected, point);
    Changed();
}

static void UpdateHover() {
    s_state.m_hoverAxis = Axis::None;
    s_state.m_hover = {};

    if (s_state.m_drag.m_type != DragType::None || !Mode::IsMouseOverWorld() || !Mode::ToolHasPrefix("camera.")) {
        return;
    }

    C3Vector start{};
    C3Vector end{};

    if (!Mode::GetMouseRay(start, end)) {
        return;
    }

    if (IsVisible(s_state.m_selected) && !s_state.m_preview) {
        C3Vector point = PointWorld(s_state.m_selected);
        s_state.m_hoverAxis = PickTranslationGizmo(start, end, point, GizmoScale(point));

        if (s_state.m_hoverAxis != Axis::None) {
            return;
        }
    }

    s_state.m_hover = Pick(start, end);
}

static bool MouseDown(MOUSEBUTTON button) {
    if (button != MOUSE_BUTTON_LEFT || !s_state.m_active) {
        return false;
    }

    C3Vector start{};
    C3Vector end{};

    if (!Mode::GetMouseRay(start, end)) {
        return true;
    }

    PointRef& selected = s_state.m_selected;

    // Alt click drops the selected point on the surface under the mouse.
    if (Platform::ModifierHeld(Platform::Modifier::Alt) && IsVisible(selected)) {
        C3Vector hit;

        if (TraceWorld(start, end, hit)) {
            PushUndo();
            SetPointWorld(selected, hit);
            Changed();
        }

        return true;
    }

    if (IsVisible(selected) && !s_state.m_preview) {
        C3Vector point = PointWorld(selected);
        Axis axis = PickTranslationGizmo(start, end, point, GizmoScale(point));

        if (axis != Axis::None) {
            DragState& drag = s_state.m_drag;
            drag = {};
            drag.m_type = DragType::Axis;
            drag.m_axis = axis;
            drag.m_start = point;
            C3Vector direction = AxisDirection(axis);
            drag.m_anchor = Add(point, Scale(direction, ClosestRayAxisParameter(start, end, point, direction)));
            return true;
        }
    }

    PointRef hit = Pick(start, end);

    if (!hit.Valid()) {
        if (selected.Valid()) {
            selected = {};
            Changed();
        }

        return true;
    }

    selected = hit;
    Changed();

    // Dragging a point slides it across the view, ctrl keeps it level.
    DragState& drag = s_state.m_drag;
    drag = {};
    drag.m_type = DragType::Plane;
    drag.m_start = PointWorld(hit);
    drag.m_normal = Platform::ModifierHeld(Platform::Modifier::Control) ? C3Vector{ 0.0f, 0.0f, 1.0f } : Normalize(Subtract(drag.m_start, start));

    if (!RayPlane(start, end, drag.m_start, drag.m_normal, drag.m_anchor)) {
        drag.m_anchor = drag.m_start;
    }

    return true;
}

// Drawing.

static void SubmitLines(const std::vector<C3Vector>& verts, const std::vector<CImVector>& colors) {
    constexpr size_t kChunk = 32000;

    for (size_t first = 0; first < verts.size(); first += kChunk) {
        uint32_t count = static_cast<uint32_t>(std::min(kChunk, verts.size() - first));
        count &= ~1u;

        if (count == 0) {
            break;
        }

        std::vector<uint16_t> indices(count);

        for (uint32_t i = 0; i < count; i++) {
            indices[i] = static_cast<uint16_t>(i);
        }

        GxPrimVertexPtr(count, verts.data() + first, sizeof(C3Vector), nullptr, 0, colors.data() + first, sizeof(CImVector), nullptr, 0, nullptr, 0);
        GxPrimIndexPtr(count, indices.data());

        CGxBatch batch{};
        batch.m_primType = GxPrim_Lines;
        batch.m_minIndex = 0;
        batch.m_maxIndex = static_cast<uint16_t>(-1);
        batch.m_start = 0;
        batch.m_count = count;
        GxDraw(&batch, 1);
    }
}

static CImVector SpeedColor(float fraction) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);

    if (fraction < 0.5f) {
        float t = fraction * 2.0f;
        return Color(static_cast<uint8_t>(40 * (1.0f - t)), static_cast<uint8_t>(120 + 135 * t), static_cast<uint8_t>(255 * (1.0f - t)));
    }

    float t = (fraction - 0.5f) * 2.0f;
    return Color(static_cast<uint8_t>(255 * t), static_cast<uint8_t>(255 * (1.0f - t)), 0);
}

static void DrawPath(int32_t track, const C3Vector& camera, CImVector color, bool bySpeed) {
    const Doc& doc = s_state.m_doc;

    if (doc.m_keys[track].empty() || doc.m_length == 0) {
        return;
    }

    uint32_t steps = std::clamp<uint32_t>(doc.m_length / 10, 64, 3000);
    float stepTime = static_cast<float>(doc.m_length) / static_cast<float>(steps);
    std::vector<C3Vector> points(steps + 1);

    for (uint32_t i = 0; i <= steps; i++) {
        points[i] = EvaluateWorld(doc, track, stepTime * i);
    }

    std::vector<float> speeds(steps, 0.0f);
    float maxSpeed = 0.0f;

    for (uint32_t i = 0; i < steps; i++) {
        speeds[i] = Length(Subtract(points[i + 1], points[i])) / stepTime;
        maxSpeed = std::max(maxSpeed, speeds[i]);
    }

    std::vector<C3Vector> verts;
    std::vector<CImVector> colors;
    verts.reserve(steps * 2);
    colors.reserve(steps * 2);

    for (uint32_t i = 0; i < steps; i++) {
        if (speeds[i] <= 0.0f && bySpeed) {
            continue;
        }

        CImVector segment = bySpeed && maxSpeed > 0.0f ? SpeedColor(speeds[i] / maxSpeed) : color;
        verts.push_back(Subtract(points[i], camera));
        verts.push_back(Subtract(points[i + 1], camera));
        colors.push_back(segment);
        colors.push_back(segment);
    }

    SubmitLines(verts, colors);
}

static CImVector PointColor(const PointRef& ref) {
    if (ref == s_state.m_selected) {
        return Color(255, 230, 40);
    }

    if (ref == s_state.m_hover) {
        return Color(255, 255, 255);
    }

    if (ref.m_part == PART_IN) {
        return Color(200, 120, 255);
    }

    if (ref.m_part == PART_OUT) {
        return Color(120, 255, 170);
    }

    return ref.m_track == TRACK_POSITION ? Color(90, 170, 255) : Color(255, 150, 40);
}

static void DrawFrustum(const C3Vector& position, const C3Vector& target, float roll, const C3Vector& camera) {
    C3Vector forward = Normalize(Subtract(target, position));

    if (Length(forward) <= 0.0f) {
        return;
    }

    // Same up vector as CGCamera::SetPositionAndTargetWithRoll.
    C3Vector up{ 0.0f, sinf(roll), cosf(roll) };
    C3Vector right = Normalize(Cross(forward, up));

    if (Length(right) <= 0.0f) {
        right = { 1.0f, 0.0f, 0.0f };
    }

    up = Cross(right, forward);

    float depth = ScreenSize(position, 0.05f, 1.0f, 25.0f);
    float halfHeight = depth * 0.41f;
    float halfWidth = halfHeight * 1.6f;
    C3Vector center = Add(position, Scale(forward, depth));
    C3Vector corners[4] = {
        Add(center, Add(Scale(right, -halfWidth), Scale(up, halfHeight))),
        Add(center, Add(Scale(right, halfWidth), Scale(up, halfHeight))),
        Add(center, Add(Scale(right, halfWidth), Scale(up, -halfHeight))),
        Add(center, Add(Scale(right, -halfWidth), Scale(up, -halfHeight))),
    };

    CImVector color = Color(255, 255, 255);
    C3Vector eye = Subtract(position, camera);

    for (int32_t i = 0; i < 4; i++) {
        C3Vector a = Subtract(corners[i], camera);
        C3Vector b = Subtract(corners[(i + 1) % 4], camera);
        GizmoDraw::DrawLine(eye, a, color);
        GizmoDraw::DrawLine(a, b, color);
    }

    // The tick on top shows which way is up after roll.
    C3Vector topMiddle = Add(center, Scale(up, halfHeight));
    C3Vector tip = Add(center, Scale(up, halfHeight * 1.5f));
    GizmoDraw::DrawLine(Subtract(Add(topMiddle, Scale(right, -halfWidth * 0.3f)), camera), Subtract(tip, camera), Color(255, 230, 40));
    GizmoDraw::DrawLine(Subtract(Add(topMiddle, Scale(right, halfWidth * 0.3f)), camera), Subtract(tip, camera), Color(255, 230, 40));
    GizmoDraw::DrawLine(eye, Subtract(target, camera), Color(255, 255, 255, 140));
    GizmoDraw::DrawSphere(Subtract(target, camera), ScreenSize(target, 0.006f, 0.05f, 2.0f), Color(255, 80, 80));
}

static void Render(CGWorldFrame* frame) {
    CGxDevice* device = g_theGxDevicePtr;

    if (!device) {
        return;
    }

    const Doc& doc = s_state.m_doc;
    C3Vector camera = frame->m_camera->m_position;

    device->RsPush();
    GxRsSet(GxRs_VertexShader, nullptr);
    GxRsSet(GxRs_PixelShader, nullptr);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_Lighting, 0);

    CImVector transparent;
    GxSceneClear(2, transparent);
    GxRsSet(GxRs_DepthTest, 1);

    // Origin and facing.
    C3Vector origin = Subtract(doc.m_origin, camera);
    float originSize = ScreenSize(doc.m_origin, 0.03f, 0.5f, 15.0f);
    GizmoDraw::DrawLine(origin, Add(origin, Scale(DirToWorld(doc, { 1.0f, 0.0f, 0.0f }), originSize * 2.0f)), Color(255, 80, 80));
    GizmoDraw::DrawLine(origin, Add(origin, Scale(DirToWorld(doc, { 0.0f, 1.0f, 0.0f }), originSize)), Color(80, 255, 80));
    GizmoDraw::DrawLine(origin, Add(origin, { 0.0f, 0.0f, originSize }), Color(80, 140, 255));

    DrawPath(TRACK_POSITION, camera, Color(120, 200, 255), s_state.m_showSpeed);

    if (s_state.m_showTarget) {
        DrawPath(TRACK_TARGET, camera, Color(255, 190, 60, 220), false);

        // Where each position key looks.
        for (const Key& key : doc.m_keys[TRACK_POSITION]) {
            if (doc.m_keys[TRACK_TARGET].empty()) {
                break;
            }

            C3Vector from = Subtract(ToWorld(doc, key.m_value), camera);
            C3Vector to = Subtract(EvaluateWorld(doc, TRACK_TARGET, static_cast<float>(key.m_time)), camera);
            GizmoDraw::DrawLine(from, to, Color(255, 255, 255, 70));
        }
    }

    ForEachPoint([&](const PointRef& ref, const C3Vector& point) {
        C3Vector drawPoint = Subtract(point, camera);

        if (ref.m_part != PART_VALUE) {
            const Key& key = doc.m_keys[ref.m_track][ref.m_index];
            GizmoDraw::DrawLine(Subtract(ToWorld(doc, key.m_value), camera), drawPoint, Color(200, 200, 200, 200));
            GizmoDraw::DrawSphere(drawPoint, ScreenSize(point, 0.005f, 0.04f, 2.0f), PointColor(ref));
            return;
        }

        GizmoDraw::DrawSphere(drawPoint, ScreenSize(point, 0.008f, 0.06f, 4.0f), PointColor(ref));
    });

    if (s_state.m_showFrustum && !s_state.m_preview && !s_state.m_engine.m_playing && !doc.m_keys[TRACK_POSITION].empty()) {
        C3Vector position = EvaluateWorld(doc, TRACK_POSITION, s_state.m_time);
        C3Vector target = EvaluateWorld(doc, TRACK_TARGET, s_state.m_time);
        float roll = Evaluate(doc, TRACK_ROLL, s_state.m_time).x;
        DrawFrustum(position, target, roll, camera);
    }

    if (IsVisible(s_state.m_selected) && !s_state.m_preview && Mode::ToolHasPrefix("camera.")) {
        C3Vector point = PointWorld(s_state.m_selected);
        GxSceneClear(2, transparent);
        Axis axis = s_state.m_drag.m_type == DragType::Axis ? s_state.m_drag.m_axis : s_state.m_hoverAxis;
        GizmoDraw::DrawTranslationGizmo(Subtract(point, camera), GizmoScale(point), axis);
    }

    device->RsPop();
}

// Saving and loading.

static bool NormalizeModelPath(const std::string& input, std::string& path, std::string& error) {
    if (!Files::NormalizeGamePath(input, path, error)) {
        return false;
    }

    std::string lower = Files::Lower(path);

    if (Files::EndsWith(lower, ".mdx") || Files::EndsWith(lower, ".mdl")) {
        path = Files::StripExtension(path) + ".m2";
    } else if (!Files::EndsWith(lower, ".m2")) {
        path += ".m2";
    }

    return true;
}

// Drops cached shared models of that name so the next load reads the new file, models in use keep theirs.
static void DetachCachedModels(const std::string& gamePath) {
    std::string name = Files::Lower(Files::FileStem(gamePath) + ".m2");
    CM2Cache* caches[2] = { World::s_m2Scene ? World::s_m2Scene->m_cache : nullptr, nullptr };

    if (CGCamera* camera = WorldCamera()) {
        caches[1] = camera->GetScene()->m_cache;
    }

    for (int32_t i = 0; i < 2; i++) {
        CM2Cache* cache = caches[i];

        if (!cache || (i == 1 && cache == caches[0])) {
            continue;
        }

        std::vector<CM2Shared*> found;

        for (CM2Shared* bucket : cache->m_shared) {
            for (CM2Shared* shared = bucket; shared; shared = shared->m_next) {
                if (shared->m_fileNameWithoutPath && name == Files::Lower(shared->m_fileNameWithoutPath)) {
                    found.push_back(shared);
                }
            }
        }

        for (CM2Shared* shared : found) {
            if (shared->m_previous) {
                *shared->m_previous = shared->m_next;
            }

            if (shared->m_next) {
                shared->m_next->m_previous = shared->m_previous;
            }

            shared->m_previous = nullptr;
            shared->m_next = nullptr;
        }
    }
}

static void ResetDoc(const Doc& doc) {
    StopAll();
    s_state.m_doc = doc;
    s_state.m_undo.clear();
    s_state.m_redo.clear();
    s_state.m_selected = {};
    s_state.m_hover = {};
    s_state.m_time = 0.0f;
    Changed();
    s_state.m_savedRevision = s_state.m_revision;
}

static bool LoadModel(const std::string& input, const C3Vector& origin, float facing, std::string& error) {
    std::string path;

    if (!NormalizeModelPath(input, path, error)) {
        return false;
    }

    std::vector<uint8_t> data;

    if (!Files::ReadGameFile(path, data)) {
        error = path + " was not found";
        return false;
    }

    Doc doc;
    doc.m_path = path;
    doc.m_origin = origin;
    doc.m_facing = facing;

    if (!Parse(data.data(), data.size(), doc, error)) {
        error = path + ": " + error;
        return false;
    }

    ResetDoc(doc);
    return true;
}

static bool Save(const std::string& input, std::string& error) {
    std::string path;

    if (!NormalizeModelPath(input, path, error)) {
        return false;
    }

    std::vector<uint8_t> m2;
    std::vector<uint8_t> skin;
    Build(s_state.m_doc, Files::FileStem(path), m2, skin);

    if (!Files::WriteLoose(path, m2.data(), m2.size(), error) || !Files::WriteLoose(Files::StripExtension(path) + "00.skin", skin.data(), skin.size(), error)) {
        return false;
    }

    Files::RefreshLooseIndex();
    DetachCachedModels(path);
    s_state.m_doc.m_path = path;
    s_state.m_savedRevision = s_state.m_revision;

    return true;
}

static void NewDoc() {
    Doc doc;
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (player) {
        doc.m_origin = player->GetPosition();
    }

    ResetDoc(doc);
}

// Keys from the view.

static C3Vector ViewTarget(const C3Vector& position, const C3Vector& forward, float distance) {
    if (distance > 0.0f) {
        return Add(position, Scale(forward, distance));
    }

    C3Vector hit;

    if (TraceWorld(position, Add(position, Scale(forward, 500.0f)), hit)) {
        return hit;
    }

    return Add(position, Scale(forward, kFallbackTargetDistance));
}

static bool CaptureView(float distance, std::string& error) {
    CGCamera* camera = WorldCamera();

    if (!camera) {
        error = "no world camera";
        return false;
    }

    if (s_state.m_preview || s_state.m_engine.m_playing) {
        error = "turn off look through first, the view is the camera being edited";
        return false;
    }

    Doc& doc = s_state.m_doc;
    uint32_t time = static_cast<uint32_t>(std::lround(std::max(0.0f, s_state.m_time)));
    C3Vector position = camera->m_position;
    C3Vector target = ViewTarget(position, Normalize(camera->Forward()), distance);

    PushUndo();
    int32_t index = SetKey(doc, TRACK_POSITION, time, ToLocal(doc, position));
    SetKey(doc, TRACK_TARGET, time, ToLocal(doc, target));
    s_state.m_selected = { TRACK_POSITION, index, PART_VALUE };
    Changed();

    return true;
}

static int32_t AddKey(int32_t track, uint32_t time) {
    Doc& doc = s_state.m_doc;
    C3Vector value;

    if (!doc.m_keys[track].empty()) {
        value = Evaluate(doc, track, static_cast<float>(time));
    } else if (track == TRACK_POSITION) {
        value = ToLocal(doc, ViewPosition());
    } else if (track == TRACK_TARGET) {
        CGCamera* camera = WorldCamera();
        value = camera ? ToLocal(doc, ViewTarget(camera->m_position, Normalize(camera->Forward()), kFallbackTargetDistance)) : C3Vector{};
    }

    PushUndo();
    int32_t index = SetKey(doc, track, time, value);
    s_state.m_selected = { track, index, PART_VALUE };
    Changed();

    return index;
}

// Camera override.

void OnCameraUpdated(CGCamera* camera, const C3Vector& position) {
    CGWorldFrame* frame = Frame();

    if (!camera || !frame || camera != frame->m_camera || !s_state.m_active || !Mode::IsActive()) {
        return;
    }

    // The model camera placed the view, the free camera must not move it.
    if (s_state.m_engine.m_playing && camera->m_model) {
        camera->m_position = position;
        return;
    }

    const Doc& doc = s_state.m_doc;

    if (!s_state.m_preview || doc.m_keys[TRACK_POSITION].empty()) {
        return;
    }

    C3Vector eye = EvaluateWorld(doc, TRACK_POSITION, s_state.m_time);
    C3Vector target = doc.m_keys[TRACK_TARGET].empty() ? Add(eye, DirToWorld(doc, { 1.0f, 0.0f, 0.0f })) : EvaluateWorld(doc, TRACK_TARGET, s_state.m_time);
    camera->SetPositionAndTargetWithRoll(eye, target, Evaluate(doc, TRACK_ROLL, s_state.m_time).x);

    // Terrain streams around the free camera, and turning look through off leaves the view here.
    if (FreeCam::IsActive()) {
        FreeCam::SetPosition(eye);
    }
}

// Handler.

class CameraHandler : public Mode::Handler {
    public:
    bool OwnsTool(const char* tool) const override {
        return tool && strncmp(tool, "camera.", 7) == 0;
    }

    bool OnMouseDown(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) override {
        return MouseDown(button);
    }

    bool OnMouseUp(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) override {
        if (button == MOUSE_BUTTON_LEFT) {
            EndDrag();
        }

        return true;
    }

    void OnUpdate(float seconds) override {
        if (!s_state.m_active || !HasAccess()) {
            return;
        }

        UpdateDrag();
        UpdateHover();
        UpdatePlayback(seconds);
    }

    void OnWorldRender(CGWorldFrame* frame) override {
        if (s_state.m_active && HasAccess() && frame && frame->m_camera) {
            Render(frame);
        }
    }

    void OnDeactivate() override {
        StopAll();
    }

    void OnWorldUnloaded() override {
        StopAll();
    }
};

static CameraHandler s_handler;

// Lua.

static int32_t TrackArg(lua_State* L, int32_t index) {
    int32_t track = static_cast<int32_t>(lua_tonumber(L, index)) - 1;

    return track >= 0 && track < TRACK_COUNT ? track : -1;
}

static int32_t KeyArg(lua_State* L, int32_t index, int32_t track) {
    int32_t key = static_cast<int32_t>(lua_tonumber(L, index)) - 1;

    return track >= 0 && key >= 0 && key < KeyCount(track) ? key : -1;
}

static void SetField(lua_State* L, const char* name, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, -2, name);
}

static void SetField(lua_State* L, const char* name, bool value) {
    lua_pushboolean(L, value ? 1 : 0);
    lua_setfield(L, -2, name);
}

static void SetField(lua_State* L, const char* name, const std::string& value) {
    lua_pushstring(L, value.c_str());
    lua_setfield(L, -2, name);
}

static int32_t PushResult(lua_State* L, bool ok, const std::string& error) {
    lua_pushboolean(L, ok ? 1 : 0);

    if (ok) {
        return 1;
    }

    lua_pushstring(L, error.c_str());
    return 2;
}

static int32_t Script_SetActive(lua_State* L) {
    bool active = lua_toboolean(L, 1) != 0;

    if (!active && s_state.m_active) {
        StopAll();
    }

    s_state.m_active = active;
    return 0;
}

static int32_t Script_GetRevision(lua_State* L) {
    lua_pushnumber(L, s_state.m_revision);
    return 1;
}

static int32_t Script_GetDoc(lua_State* L) {
    const Doc& doc = s_state.m_doc;
    lua_newtable(L);
    SetField(L, "path", doc.m_path);
    SetField(L, "length", static_cast<double>(doc.m_length));
    SetField(L, "x", doc.m_origin.x);
    SetField(L, "y", doc.m_origin.y);
    SetField(L, "z", doc.m_origin.z);
    SetField(L, "facing", doc.m_facing);
    SetField(L, "fov", doc.m_fieldOfView);
    SetField(L, "dirty", s_state.m_revision != s_state.m_savedRevision);
    SetField(L, "canUndo", !s_state.m_undo.empty());
    SetField(L, "canRedo", !s_state.m_redo.empty());
    SetField(L, "pathLength", PathLength(doc, TRACK_POSITION));
    SetField(L, "warnings", Validate(doc));

    lua_newtable(L);

    for (int32_t track = 0; track < TRACK_COUNT; track++) {
        lua_pushnumber(L, doc.m_interp[track]);
        lua_rawseti(L, -2, track + 1);
    }

    lua_setfield(L, -2, "interp");

    lua_newtable(L);

    for (int32_t track = 0; track < TRACK_COUNT; track++) {
        lua_pushnumber(L, static_cast<double>(doc.m_keys[track].size()));
        lua_rawseti(L, -2, track + 1);
    }

    lua_setfield(L, -2, "counts");

    return 1;
}

static int32_t Script_GetKeys(lua_State* L) {
    int32_t track = TrackArg(L, 1);

    if (track < 0) {
        return 0;
    }

    const Doc& doc = s_state.m_doc;
    const std::vector<Key>& keys = doc.m_keys[track];
    lua_createtable(L, static_cast<int32_t>(keys.size()), 0);

    for (size_t k = 0; k < keys.size(); k++) {
        const Key& key = keys[k];
        C3Vector world = track == TRACK_ROLL ? key.m_value : ToWorld(doc, key.m_value);
        lua_newtable(L);
        SetField(L, "time", static_cast<double>(key.m_time));
        SetField(L, "x", key.m_value.x);
        SetField(L, "y", key.m_value.y);
        SetField(L, "z", key.m_value.z);
        SetField(L, "wx", world.x);
        SetField(L, "wy", world.y);
        SetField(L, "wz", world.z);
        SetField(L, "inX", key.m_in.x);
        SetField(L, "inY", key.m_in.y);
        SetField(L, "inZ", key.m_in.z);
        SetField(L, "outX", key.m_out.x);
        SetField(L, "outY", key.m_out.y);
        SetField(L, "outZ", key.m_out.z);
        SetField(L, "mode", static_cast<double>(key.m_mode));
        lua_rawseti(L, -2, static_cast<int32_t>(k) + 1);
    }

    return 1;
}

static int32_t Script_GetSelection(lua_State* L) {
    const PointRef& selected = s_state.m_selected;

    if (!selected.Valid()) {
        return 0;
    }

    lua_pushnumber(L, selected.m_track + 1);
    lua_pushnumber(L, selected.m_index + 1);
    lua_pushnumber(L, selected.m_part);
    return 3;
}

static int32_t Script_Select(lua_State* L) {
    int32_t track = TrackArg(L, 1);
    int32_t key = KeyArg(L, 2, track);
    int32_t part = lua_isnumber(L, 3) ? static_cast<int32_t>(lua_tonumber(L, 3)) : PART_VALUE;
    PointRef selected = key >= 0 ? PointRef{ track, key, std::clamp(part, 0, 2) } : PointRef{};

    if (!(selected == s_state.m_selected)) {
        s_state.m_selected = selected;
        Changed();
    }

    return 0;
}

static int32_t Script_SetKeyValue(lua_State* L) {
    int32_t track = TrackArg(L, 1);
    int32_t key = KeyArg(L, 2, track);

    if (key < 0) {
        return 0;
    }

    Doc& doc = s_state.m_doc;
    C3Vector value(static_cast<float>(lua_tonumber(L, 3)), static_cast<float>(lua_tonumber(L, 4)), static_cast<float>(lua_tonumber(L, 5)));

    if (lua_toboolean(L, 6) && track != TRACK_ROLL) {
        value = ToLocal(doc, value);
    }

    PushUndo();
    doc.m_keys[track][key].m_value = value;
    Normalize(doc, track);
    Changed();
    return 0;
}

static int32_t Script_SetKeyTime(lua_State* L) {
    int32_t track = TrackArg(L, 1);
    int32_t key = KeyArg(L, 2, track);

    if (key < 0) {
        return 0;
    }

    PushUndo();
    double time = std::max(0.0, lua_tonumber(L, 3));
    int32_t index = SetKeyTime(s_state.m_doc, track, key, static_cast<uint32_t>(std::llround(time)));
    Changed();
    lua_pushnumber(L, index + 1);
    return 1;
}

static int32_t Script_SetHandle(lua_State* L) {
    int32_t track = TrackArg(L, 1);
    int32_t key = KeyArg(L, 2, track);

    if (key < 0) {
        return 0;
    }

    C3Vector offset(static_cast<float>(lua_tonumber(L, 4)), static_cast<float>(lua_tonumber(L, 5)), static_cast<float>(lua_tonumber(L, 6)));
    PushUndo();
    SetHandle(s_state.m_doc, track, key, lua_toboolean(L, 3) != 0, offset);
    Changed();
    return 0;
}

static int32_t Script_SetKeyMode(lua_State* L) {
    int32_t track = TrackArg(L, 1);
    int32_t key = KeyArg(L, 2, track);

    if (key < 0) {
        return 0;
    }

    PushUndo();
    SetMode(s_state.m_doc, track, key, static_cast<int32_t>(lua_tonumber(L, 3)));
    Changed();
    return 0;
}

static int32_t Script_SetTrackMode(lua_State* L) {
    int32_t track = TrackArg(L, 1);

    if (track < 0) {
        return 0;
    }

    PushUndo();
    SetModeAll(s_state.m_doc, track, static_cast<int32_t>(lua_tonumber(L, 2)));
    Changed();
    return 0;
}

static int32_t Script_AddKey(lua_State* L) {
    int32_t track = TrackArg(L, 1);

    if (track < 0) {
        return 0;
    }

    float time = lua_isnumber(L, 2) ? static_cast<float>(lua_tonumber(L, 2)) : s_state.m_time;
    lua_pushnumber(L, AddKey(track, static_cast<uint32_t>(std::lround(std::max(0.0f, time)))) + 1);
    return 1;
}

static int32_t Script_DeleteKey(lua_State* L) {
    int32_t track = TrackArg(L, 1);
    int32_t key = KeyArg(L, 2, track);

    if (key < 0) {
        return 0;
    }

    PushUndo();
    DeleteKey(s_state.m_doc, track, key);
    ClampSelection();
    Changed();
    return 0;
}

static int32_t Script_CaptureView(lua_State* L) {
    std::string error;
    bool ok = CaptureView(static_cast<float>(lua_tonumber(L, 1)), error);
    return PushResult(L, ok, error);
}

static int32_t Script_SetInterp(lua_State* L) {
    int32_t track = TrackArg(L, 1);

    if (track < 0) {
        return 0;
    }

    PushUndo();
    SetInterp(s_state.m_doc, track, static_cast<uint16_t>(lua_tonumber(L, 2)));
    Changed();
    return 0;
}

static int32_t Script_SetLength(lua_State* L) {
    double length = lua_tonumber(L, 1);

    if (length < 1.0) {
        return 0;
    }

    PushUndo();
    SetLength(s_state.m_doc, static_cast<uint32_t>(std::llround(length)), lua_toboolean(L, 2) != 0);
    s_state.m_time = std::min(s_state.m_time, static_cast<float>(s_state.m_doc.m_length));
    Changed();
    return 0;
}

static int32_t Script_SetOrigin(lua_State* L) {
    C3Vector origin(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)));
    PushUndo();
    SetOrigin(s_state.m_doc, origin, static_cast<float>(lua_tonumber(L, 4)), lua_toboolean(L, 5) != 0);
    Changed();
    return 0;
}

static int32_t Script_GetPlayerPosition(lua_State* L) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return 0;
    }

    C3Vector position = player->GetPosition();
    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    lua_pushnumber(L, player->GetFacing());
    return 4;
}

static int32_t Script_GetViewPosition(lua_State* L) {
    CGCamera* camera = WorldCamera();

    if (!camera) {
        return 0;
    }

    lua_pushnumber(L, camera->m_position.x);
    lua_pushnumber(L, camera->m_position.y);
    lua_pushnumber(L, camera->m_position.z);
    return 3;
}

static int32_t Script_ConstantSpeed(lua_State* L) {
    PushUndo();
    ConstantSpeed(s_state.m_doc);
    Changed();
    return 0;
}

static int32_t Script_ShiftTimes(lua_State* L) {
    PushUndo();
    ShiftTimes(s_state.m_doc, static_cast<int32_t>(lua_tonumber(L, 1)));
    Changed();
    return 0;
}

static int32_t Script_Undo(lua_State* L) {
    Undo();
    return 0;
}

static int32_t Script_Redo(lua_State* L) {
    Redo();
    return 0;
}

static int32_t Script_New(lua_State* L) {
    NewDoc();
    return 0;
}

static int32_t Script_Load(lua_State* L) {
    const char* path = lua_tostring(L, 1);
    std::string error;
    bool ok = path && LoadModel(path, s_state.m_doc.m_origin, s_state.m_doc.m_facing, error);
    return PushResult(L, ok, path ? error : "no path");
}

static int32_t Script_LoadDbc(lua_State* L) {
    CinematicCameraRec* record = g_cinematicCameraDB.GetRecord(static_cast<int32_t>(lua_tonumber(L, 1)));

    if (!record || !record->m_model) {
        return PushResult(L, false, "no CinematicCamera row with that id");
    }

    std::string error;
    C3Vector origin(record->m_origin[0], record->m_origin[1], record->m_origin[2]);
    bool ok = LoadModel(record->m_model, origin, record->m_originFacing, error);
    return PushResult(L, ok, error);
}

static int32_t Script_GetDbcRows(lua_State* L) {
    lua_newtable(L);
    int32_t count = 0;

    for (int32_t i = 0; i < g_cinematicCameraDB.m_numRecords; i++) {
        const CinematicCameraRec& record = g_cinematicCameraDB.m_records[i];

        if (!record.m_model) {
            continue;
        }

        lua_newtable(L);
        SetField(L, "id", static_cast<double>(record.m_id));
        SetField(L, "model", std::string(record.m_model));
        SetField(L, "x", record.m_origin[0]);
        SetField(L, "y", record.m_origin[1]);
        SetField(L, "z", record.m_origin[2]);
        SetField(L, "facing", record.m_originFacing);
        lua_rawseti(L, -2, ++count);
    }

    return 1;
}

static int32_t Script_Save(lua_State* L) {
    const char* path = lua_tostring(L, 1);
    std::string input = path && *path ? path : s_state.m_doc.m_path;

    if (input.empty()) {
        return PushResult(L, false, "no path to save to");
    }

    std::string error;

    if (!Save(input, error)) {
        return PushResult(L, false, error);
    }

    lua_pushboolean(L, 1);
    lua_pushstring(L, s_state.m_doc.m_path.c_str());
    return 2;
}

static int32_t Script_SetTime(lua_State* L) {
    if (s_state.m_engine.m_playing) {
        return 0;
    }

    s_state.m_time = std::clamp(static_cast<float>(lua_tonumber(L, 1)), 0.0f, static_cast<float>(s_state.m_doc.m_length));
    return 0;
}

static int32_t Script_GetTime(lua_State* L) {
    lua_pushnumber(L, s_state.m_time);
    lua_pushboolean(L, s_state.m_playing ? 1 : 0);
    lua_pushboolean(L, s_state.m_preview ? 1 : 0);
    lua_pushboolean(L, s_state.m_engine.m_playing ? 1 : 0);
    lua_pushboolean(L, s_state.m_loop ? 1 : 0);
    lua_pushnumber(L, s_state.m_rate);
    return 6;
}

static int32_t Script_Play(lua_State* L) {
    bool play = lua_toboolean(L, 1) != 0;
    StopEngine();

    if (play && s_state.m_time >= static_cast<float>(s_state.m_doc.m_length)) {
        s_state.m_time = 0.0f;
    }

    s_state.m_playing = play;
    return 0;
}

static int32_t Script_SetLoop(lua_State* L) {
    s_state.m_loop = lua_toboolean(L, 1) != 0;
    return 0;
}

static int32_t Script_SetRate(lua_State* L) {
    s_state.m_rate = std::clamp(static_cast<float>(lua_tonumber(L, 1)), 0.05f, 8.0f);
    return 0;
}

static int32_t Script_SetPreview(lua_State* L) {
    StopEngine();
    s_state.m_preview = lua_toboolean(L, 1) != 0;
    return 0;
}

static int32_t Script_PlayEngine(lua_State* L) {
    std::string error;
    bool ok = PlayEngine(error);
    return PushResult(L, ok, error);
}

static int32_t Script_StopEngine(lua_State* L) {
    StopEngine();
    return 0;
}

static int32_t Script_GetSpeedGraph(lua_State* L) {
    int32_t track = lua_isnumber(L, 2) ? TrackArg(L, 2) : TRACK_POSITION;
    uint32_t count = static_cast<uint32_t>(std::clamp(lua_tonumber(L, 1), 1.0, 2000.0));

    if (track < 0) {
        return 0;
    }

    std::vector<float> speeds;
    SpeedSamples(s_state.m_doc, track, count, speeds);
    float maximum = 0.0f;
    lua_createtable(L, static_cast<int32_t>(count), 0);

    for (uint32_t i = 0; i < count; i++) {
        maximum = std::max(maximum, speeds[i]);
        lua_pushnumber(L, speeds[i]);
        lua_rawseti(L, -2, static_cast<int32_t>(i) + 1);
    }

    lua_pushnumber(L, maximum);
    return 2;
}

static int32_t Script_Evaluate(lua_State* L) {
    float time = lua_isnumber(L, 1) ? static_cast<float>(lua_tonumber(L, 1)) : s_state.m_time;
    const Doc& doc = s_state.m_doc;
    C3Vector position = EvaluateWorld(doc, TRACK_POSITION, time);
    C3Vector target = EvaluateWorld(doc, TRACK_TARGET, time);
    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    lua_pushnumber(L, target.x);
    lua_pushnumber(L, target.y);
    lua_pushnumber(L, target.z);
    lua_pushnumber(L, Evaluate(doc, TRACK_ROLL, time).x);
    return 7;
}

static int32_t Script_SetDisplay(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    bool value = lua_toboolean(L, 2) != 0;

    if (!name) {
        return 0;
    }

    if (!strcmp(name, "target")) {
        s_state.m_showTarget = value;
    } else if (!strcmp(name, "handles")) {
        s_state.m_showHandles = value;
    } else if (!strcmp(name, "allHandles")) {
        s_state.m_allHandles = value;
    } else if (!strcmp(name, "speed")) {
        s_state.m_showSpeed = value;
    } else if (!strcmp(name, "frustum")) {
        s_state.m_showFrustum = value;
    }

    ClampSelection();
    return 0;
}

static int32_t Script_GetDisplay(lua_State* L) {
    lua_newtable(L);
    SetField(L, "target", s_state.m_showTarget);
    SetField(L, "handles", s_state.m_showHandles);
    SetField(L, "allHandles", s_state.m_allHandles);
    SetField(L, "speed", s_state.m_showSpeed);
    SetField(L, "frustum", s_state.m_showFrustum);
    return 1;
}

static int32_t Script_GoToKey(lua_State* L) {
    int32_t track = TrackArg(L, 1);
    int32_t key = KeyArg(L, 2, track);

    if (key < 0 || track == TRACK_ROLL || !FreeCam::IsActive()) {
        return 0;
    }

    FreeCam::SetPosition(ToWorld(s_state.m_doc, s_state.m_doc.m_keys[track][key].m_value));
    lua_pushboolean(L, 1);
    return 1;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!HasAccess()) {
        return 0;
    }

    return Function(L);
}

void Apply() {
    Mode::RegisterHandler(&s_handler);

    Lua::RegisterFunction("CameraEditor_SetActive", Gated<Script_SetActive>);
    Lua::RegisterFunction("CameraEditor_GetRevision", Gated<Script_GetRevision>);
    Lua::RegisterFunction("CameraEditor_GetDoc", Gated<Script_GetDoc>);
    Lua::RegisterFunction("CameraEditor_GetKeys", Gated<Script_GetKeys>);
    Lua::RegisterFunction("CameraEditor_GetSelection", Gated<Script_GetSelection>);
    Lua::RegisterFunction("CameraEditor_Select", Gated<Script_Select>);
    Lua::RegisterFunction("CameraEditor_SetKeyValue", Gated<Script_SetKeyValue>);
    Lua::RegisterFunction("CameraEditor_SetKeyTime", Gated<Script_SetKeyTime>);
    Lua::RegisterFunction("CameraEditor_SetHandle", Gated<Script_SetHandle>);
    Lua::RegisterFunction("CameraEditor_SetKeyMode", Gated<Script_SetKeyMode>);
    Lua::RegisterFunction("CameraEditor_SetTrackMode", Gated<Script_SetTrackMode>);
    Lua::RegisterFunction("CameraEditor_AddKey", Gated<Script_AddKey>);
    Lua::RegisterFunction("CameraEditor_DeleteKey", Gated<Script_DeleteKey>);
    Lua::RegisterFunction("CameraEditor_CaptureView", Gated<Script_CaptureView>);
    Lua::RegisterFunction("CameraEditor_SetInterp", Gated<Script_SetInterp>);
    Lua::RegisterFunction("CameraEditor_SetLength", Gated<Script_SetLength>);
    Lua::RegisterFunction("CameraEditor_SetOrigin", Gated<Script_SetOrigin>);
    Lua::RegisterFunction("CameraEditor_GetPlayerPosition", Gated<Script_GetPlayerPosition>);
    Lua::RegisterFunction("CameraEditor_GetViewPosition", Gated<Script_GetViewPosition>);
    Lua::RegisterFunction("CameraEditor_ConstantSpeed", Gated<Script_ConstantSpeed>);
    Lua::RegisterFunction("CameraEditor_ShiftTimes", Gated<Script_ShiftTimes>);
    Lua::RegisterFunction("CameraEditor_Undo", Gated<Script_Undo>);
    Lua::RegisterFunction("CameraEditor_Redo", Gated<Script_Redo>);
    Lua::RegisterFunction("CameraEditor_New", Gated<Script_New>);
    Lua::RegisterFunction("CameraEditor_Load", Gated<Script_Load>);
    Lua::RegisterFunction("CameraEditor_LoadDbc", Gated<Script_LoadDbc>);
    Lua::RegisterFunction("CameraEditor_GetDbcRows", Gated<Script_GetDbcRows>);
    Lua::RegisterFunction("CameraEditor_Save", Gated<Script_Save>);
    Lua::RegisterFunction("CameraEditor_SetTime", Gated<Script_SetTime>);
    Lua::RegisterFunction("CameraEditor_GetTime", Gated<Script_GetTime>);
    Lua::RegisterFunction("CameraEditor_Play", Gated<Script_Play>);
    Lua::RegisterFunction("CameraEditor_SetLoop", Gated<Script_SetLoop>);
    Lua::RegisterFunction("CameraEditor_SetRate", Gated<Script_SetRate>);
    Lua::RegisterFunction("CameraEditor_SetPreview", Gated<Script_SetPreview>);
    Lua::RegisterFunction("CameraEditor_PlayEngine", Gated<Script_PlayEngine>);
    Lua::RegisterFunction("CameraEditor_StopEngine", Gated<Script_StopEngine>);
    Lua::RegisterFunction("CameraEditor_GetSpeedGraph", Gated<Script_GetSpeedGraph>);
    Lua::RegisterFunction("CameraEditor_Evaluate", Gated<Script_Evaluate>);
    Lua::RegisterFunction("CameraEditor_SetDisplay", Gated<Script_SetDisplay>);
    Lua::RegisterFunction("CameraEditor_GetDisplay", Gated<Script_GetDisplay>);
    Lua::RegisterFunction("CameraEditor_GoToKey", Gated<Script_GoToKey>);
}

void OnGameDestroy() {
    // The camera is gone with the game, only the preview files are left to drop.
    if (!s_state.m_engine.m_dir.empty()) {
        Resources::RemovePreviews(s_state.m_engine.m_dir);
        s_state.m_engine.m_dir.clear();
    }

    s_state.m_engine.m_playing = false;
    s_state.m_engine.m_ended = false;
    s_state.m_playing = false;
    s_state.m_preview = false;
    s_state.m_active = false;
    s_state.m_drag = {};
}

}
