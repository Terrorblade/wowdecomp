#include "HoT/Editor/Selection.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include "Engine/Source/Os/Gui.h"
#endif
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Storm/H/storm.h"
#include "HoT/Editor/CreatureEditor.h"
#include "HoT/Editor/Editor.h"
#include "HoT/Editor/FreeCam.h"
#include "HoT/Editor/GizmoDraw.h"
#include "HoT/Editor/GizmoPick.h"
#include "HoT/Editor/GobEditor.h"
#include "HoT/Editor/Mode.h"
#include "HoT/Editor/Quat.h"
#include "HoT/Editor/VectorMath.h"
#include "HoT/Editor/WorldPreview.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Object/Movement.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Editor::Selection {

using namespace VectorMath;

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kDragThreshold = 0.004f;
static constexpr float kGizmoScreenScale = 0.12f;
static constexpr float kYawRingScale = 1.3f;

enum class Gizmo : uint8_t {
    Off = 0,
    Move = 1,
    Rotate = 2,
};

enum class Drag : uint8_t {
    None,
    Pending,
    Ground,
    Vertical,
    Plane,
    Axis,
    Ring,
};

struct Transform {
    C3Vector m_position{};
    float m_facing = 0.0f;
    Quat m_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
};

struct DragState {
    Drag m_type = Drag::None;
    bool m_node = false;
    uint32_t m_point = 0;
    float m_downX = 0.0f;
    float m_downY = 0.0f;

    Transform m_start;
    Transform m_current;

    C3Vector m_offset{};
    float m_height = 0.0f;
    C3Vector m_anchor{};
    C3Vector m_planeNormal{};
    float m_planeZ = 0.0f;

    Axis m_axis = Axis::None;
    C3Vector m_ringAxis{};
    C3Vector m_ringRef{};
    C3Vector m_ringPerp{};
    float m_anchorAngle = 0.0f;
};

struct PendingMove {
    Kind m_kind = Kind::None;
    Transform m_original;
    Transform m_target;
};

struct SelectionState {
    Kind m_kind = Kind::None;
    uint64_t m_guid = 0;

    Gizmo m_gizmo = Gizmo::Off;
    Axis m_hoverMove = Axis::None;
    Axis m_hoverRing = Axis::None;

    DragState m_drag;

    std::unordered_map<uint64_t, PendingMove> m_moves;

    Kind m_spawnPending = Kind::None;
};

static SelectionState s_state;

#if defined(_WIN32)
static bool KeyHeld(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}
#else
// Only the modifiers are polled; map them onto OsGuiIsModifierKeyDown (SDL modifier state).
enum { VK_SHIFT = 1, VK_CONTROL = 0, VK_MENU = 2 };

static bool KeyHeld(int key) {
    return OsGuiIsModifierKeyDown(key) != 0;
}
#endif

static const char* KindName(Kind kind) {
    switch (kind) {
        case Kind::Creature:
            return "creature";
        case Kind::GameObject:
            return "gameobject";
        default:
            return "";
    }
}

static Kind KindOf(CGObject_C* object) {
    if (!object) {
        return Kind::None;
    }

    uint32_t type = object->m_obj->m_type;

    if ((type & TYPEMASK_PLAYER) || WorldPreview::IsPreviewGuid(object->m_obj->m_guid.m_guid)) {
        return Kind::None;
    }

    if (type & TYPEMASK_UNIT) {
        return Kind::Creature;
    }

    if (type & TYPEMASK_GAMEOBJECT) {
        return Kind::GameObject;
    }

    return Kind::None;
}

static CGObject_C* ObjectByGuid(uint64_t guid) {
    if (!guid) {
        return nullptr;
    }

    return ClntObjMgrObjectPtr(WOWGUID{ guid }, static_cast<TypeMask>(TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT), __FILE__, __LINE__);
}

CGObject_C* SelectedObject() {
    CGObject_C* object = ObjectByGuid(s_state.m_guid);

    return KindOf(object) == s_state.m_kind ? object : nullptr;
}

Kind SelectedKind() {
    return s_state.m_kind;
}

uint64_t SelectedGuid() {
    return s_state.m_guid;
}

static bool ReadTransform(CGObject_C* object, Transform& out) {
    switch (KindOf(object)) {
        case Kind::Creature: {
            CGUnit_C* unit = static_cast<CGUnit_C*>(object);
            out.m_position = unit->GetPosition();
            out.m_facing = unit->GetFacing();
            out.m_rotation = AxisAngleToQuat(0.0f, 0.0f, 1.0f, out.m_facing);
            return true;
        }
        case Kind::GameObject: {
            CGGameObject_C* gameObject = static_cast<CGGameObject_C*>(object);
            out.m_position = gameObject->m_passenger.m_position;
            out.m_rotation = UnpackQuat(GobEditor::PackedRotation(gameObject));

            float roll = 0.0f;
            float pitch = 0.0f;
            QuatToEuler(out.m_rotation, roll, pitch, out.m_facing);
            return true;
        }
        default:
            return false;
    }
}

static void WriteTransform(CGObject_C* object, const Transform& transform) {
    switch (KindOf(object)) {
        case Kind::Creature: {
            CMovement_C* movement = static_cast<CGUnit_C*>(object)->m_movement;

            // Passengers keep transport local coordinates, the server still gets the world position on commit.
            if (!movement || movement->m_transportGuid.m_guid) {
                return;
            }

            // Teleport drops the server spline and the move flags, a plain position write gets pulled back by them every frame.
            movement->CMovementShared::Teleport(transform.m_position, transform.m_facing, 1);
            movement->m_interpolation = 0;
            object->UpdateWorldObject(0);
            break;
        }
        case Kind::GameObject: {
            CGGameObject_C* gameObject = static_cast<CGGameObject_C*>(object);
            gameObject->m_passenger.m_position = transform.m_position;
            GobEditor::PackedRotation(gameObject) = PackQuat(transform.m_rotation);
            gameObject->UpdateWorldObject(0);
            break;
        }
        default:
            break;
    }
}

// World geometry along the segment, skipping hits on the ignored object so a dragged gameobject never lands on itself.
static uint64_t s_traceIgnore = 0;

bool IgnoresCollision(uint64_t guid) {
    return s_traceIgnore && guid == s_traceIgnore;
}

static bool TraceWorldOnce(const C3Vector& start, const C3Vector& end, uint64_t ignore, C3Vector& hit);

static bool TraceWorld(const C3Vector& start, const C3Vector& end, uint64_t ignore, C3Vector& hit) {
    s_traceIgnore = ignore;
    bool found = TraceWorldOnce(start, end, ignore, hit);
    s_traceIgnore = 0;
    return found;
}

static bool TraceWorldOnce(const C3Vector& start, const C3Vector& end, uint64_t ignore, C3Vector& hit) {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    if (!frame) {
        return false;
    }

    C3Vector from = start;
    C3Vector direction = Normalize(Subtract(end, start));

    for (int32_t attempt = 0; attempt < 6; attempt++) {
        if (Dot(Subtract(end, from), direction) <= 0.0f) {
            return false;
        }

        HITTESTRESULT result{};

        if (!frame->HitTest(from, end, 0x3, &result)) {
            return false;
        }

        C3Vector point = Add(from, Scale(direction, result.m_distance));

        if (ignore && result.m_guid.m_guid == ignore) {
            from = Add(point, Scale(direction, 0.25f));
            continue;
        }

        hit = point;
        return true;
    }

    return false;
}

static bool GroundBelow(const C3Vector& position, float above, uint64_t ignore, float& groundZ) {
    C3Vector top = { position.x, position.y, position.z + above };
    C3Vector bottom = { position.x, position.y, position.z - 500.0f };
    C3Vector hit{};

    if (!TraceWorld(top, bottom, ignore, hit)) {
        return false;
    }

    groundZ = hit.z;
    return true;
}

static bool MouseRay(C3Vector& start, C3Vector& end) {
    return Mode::GetMouseRay(start, end);
}

static bool RayPlane(const C3Vector& start, const C3Vector& end, const C3Vector& point, const C3Vector& normal, C3Vector& hit) {
    C3Vector direction = Subtract(end, start);
    float denominator = Dot(direction, normal);

    if (fabsf(denominator) < 1e-5f * Length(direction)) {
        return false;
    }

    float t = Dot(Subtract(point, start), normal) / denominator;

    if (t <= 0.0f || t > 1.0f) {
        return false;
    }

    hit = Add(start, Scale(direction, t));
    return true;
}

static C3Vector CameraPosition() {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    return frame && frame->m_camera ? frame->m_camera->m_position : C3Vector{};
}

static float GizmoScale(const C3Vector& position) {
    float distance = Length(Subtract(position, CameraPosition()));
    float scale = distance * kGizmoScreenScale;

    return scale < 0.75f ? 0.75f : (scale > 60.0f ? 60.0f : scale);
}

static void SignalSelection() {
    Lua::SignalEvent("GAME_EDITOR_SELECTION_CHANGED", "%s%s", KindName(s_state.m_kind), GobEditor::GuidToString(s_state.m_guid).c_str());
}

static void SetGizmo(Gizmo gizmo) {
    if (s_state.m_gizmo == gizmo) {
        return;
    }

    s_state.m_gizmo = gizmo;
    s_state.m_hoverMove = Axis::None;
    s_state.m_hoverRing = Axis::None;

    static const char* const s_names[] = { "off", "move", "rotate" };
    Lua::SignalEvent("GAME_EDITOR_GIZMO_CHANGED", "%s", s_names[static_cast<uint8_t>(gizmo)]);
}

static void SignalMoves() {
    Lua::SignalEvent("GAME_EDITOR_MOVES_CHANGED", "%d", static_cast<int32_t>(s_state.m_moves.size()));
}

// Every move is a local preview, the first one remembers where the object stood so Revert can put it back.
static void PreviewTransform(uint64_t guid, CGObject_C* object, const Transform& transform) {
    Kind kind = KindOf(object);

    if (!guid || kind == Kind::None) {
        return;
    }

    auto it = s_state.m_moves.find(guid);
    bool added = false;

    if (it == s_state.m_moves.end()) {
        PendingMove move;
        move.m_kind = kind;

        if (!ReadTransform(object, move.m_original)) {
            return;
        }

        it = s_state.m_moves.emplace(guid, move).first;
        added = true;
    }

    WriteTransform(object, transform);
    it->second.m_target = transform;

    if (added) {
        SignalMoves();
    }
}

static bool IsDraggingObject(uint64_t guid) {
    const DragState& drag = s_state.m_drag;

    return guid == s_state.m_guid && !drag.m_node && drag.m_type != Drag::None && drag.m_type != Drag::Pending;
}

static void CancelDrag(bool restore) {
    DragState& drag = s_state.m_drag;

    if (restore && drag.m_type != Drag::None && drag.m_type != Drag::Pending) {
        if (drag.m_node) {
            CreatureEditor::PreviewNode(drag.m_point, drag.m_start.m_position);
        } else if (CGObject_C* object = SelectedObject()) {
            PreviewTransform(s_state.m_guid, object, drag.m_start);
        }
    }

    drag = DragState();
}

static void SendMove(uint64_t guid, const PendingMove& move) {
    if (move.m_kind == Kind::Creature) {
        CreatureEditor::Move(guid, move.m_target.m_position, move.m_target.m_facing);
    } else if (move.m_kind == Kind::GameObject) {
        GobEditor::CommitTransform(guid, move.m_target.m_position, move.m_target.m_rotation);
    }
}

bool SaveMove(uint64_t guid) {
    if (IsDraggingObject(guid)) {
        CancelDrag(false);
    }

    auto it = s_state.m_moves.find(guid);

    if (it == s_state.m_moves.end()) {
        return false;
    }

    PendingMove move = it->second;
    s_state.m_moves.erase(it);
    SendMove(guid, move);
    SignalMoves();
    return true;
}

bool RevertMove(uint64_t guid) {
    if (IsDraggingObject(guid)) {
        CancelDrag(false);
    }

    auto it = s_state.m_moves.find(guid);

    if (it == s_state.m_moves.end()) {
        return false;
    }

    if (CGObject_C* object = ObjectByGuid(guid); object && KindOf(object) == it->second.m_kind) {
        WriteTransform(object, it->second.m_original);
    }

    s_state.m_moves.erase(it);
    SignalMoves();
    return true;
}

// Server splines and updates keep arriving for a moved creature, the preview wins until it is saved or reverted.
static void HoldPendingMoves() {
    for (auto& [guid, move] : s_state.m_moves) {
        if (IsDraggingObject(guid)) {
            continue;
        }

        CGObject_C* object = ObjectByGuid(guid);
        Transform current;

        if (!object || KindOf(object) != move.m_kind || !ReadTransform(object, current)) {
            continue;
        }

        C3Vector offset = Subtract(current.m_position, move.m_target.m_position);
        bool drifted = Dot(offset, offset) > 0.0004f;

        if (move.m_kind == Kind::Creature) {
            drifted = drifted || fabsf(NormalizeSignedAngleDelta(current.m_facing - move.m_target.m_facing)) > 0.01f;
            drifted = drifted || static_cast<CGUnit_C*>(object)->m_movement->IsOnSpline();
        }

        if (drifted) {
            WriteTransform(object, move.m_target);
        }
    }
}

void BeginLocalMove(uint64_t guid) {
    CGObject_C* object = ObjectByGuid(guid);
    Kind kind = KindOf(object);

    if (kind == Kind::None || s_state.m_moves.count(guid)) {
        return;
    }

    PendingMove move;
    move.m_kind = kind;

    if (ReadTransform(object, move.m_original)) {
        move.m_target = move.m_original;
        s_state.m_moves.emplace(guid, move);
        SignalMoves();
    }
}

void EndLocalMove(uint64_t guid) {
    auto it = s_state.m_moves.find(guid);
    CGObject_C* object = ObjectByGuid(guid);

    if (it != s_state.m_moves.end() && object) {
        ReadTransform(object, it->second.m_target);
    }
}

static bool SelectInternal(uint64_t guid, bool request) {
    CGObject_C* object = ObjectByGuid(guid);
    Kind kind = KindOf(object);

    if (kind == Kind::None) {
        return false;
    }

    if (guid == s_state.m_guid && kind == s_state.m_kind) {
        return true;
    }

    CancelDrag(true);

    s_state.m_kind = kind;
    s_state.m_guid = guid;
    CreatureEditor::SetSelectedPoint(0);

    if (Mode::ToolIs("select") || Mode::ToolIs("move") || Mode::ToolIs("rotate")) {
        SetGizmo(Mode::ToolIs("rotate") ? Gizmo::Rotate : Gizmo::Move);
    }

    if (request) {
        if (kind == Kind::Creature) {
            CreatureEditor::Request(guid);
        } else {
            GobEditor::Request(guid);
        }
    }

    SignalSelection();
    return true;
}

bool Select(uint64_t guid) {
    return SelectInternal(guid, true);
}

void Clear() {
    if (s_state.m_kind == Kind::None && !s_state.m_guid) {
        return;
    }

    CancelDrag(true);

    s_state.m_kind = Kind::None;
    s_state.m_guid = 0;
    s_state.m_hoverMove = Axis::None;
    s_state.m_hoverRing = Axis::None;
    CreatureEditor::SetSelectedPoint(0);

    SignalSelection();
}

void Replace(uint64_t oldGuid, uint64_t newGuid) {
    if (!oldGuid || oldGuid == newGuid) {
        return;
    }

    if (auto it = s_state.m_moves.find(oldGuid); it != s_state.m_moves.end()) {
        PendingMove move = it->second;
        s_state.m_moves.erase(it);
        s_state.m_moves[newGuid] = move;
    }

    if (oldGuid != s_state.m_guid) {
        return;
    }

    CancelDrag(false);
    s_state.m_guid = newGuid;

    SignalSelection();
}

// A snapshot arrived, a spawn the editor asked for becomes the selection.
static void OnSnapshot(Kind kind, uint64_t guid) {
    if (s_state.m_spawnPending == kind && guid) {
        s_state.m_spawnPending = Kind::None;
        SelectInternal(guid, false);
    }
}

struct RayScan {
    C3Vector m_start;
    C3Vector m_direction;
    float m_maxAlong = 0.0f;
    float m_bestAngle = 0.0f;
    uint64_t m_best = 0;
};

static int32_t CollectClosestToRay(WOWGUID guid, void* param) {
    RayScan* scan = static_cast<RayScan*>(param);
    CGObject_C* object = ObjectByGuid(guid.m_guid);
    Kind kind = KindOf(object);
    Transform transform;

    if (kind == Kind::None || !ReadTransform(object, transform)) {
        return 1;
    }

    float scale = object->GetScale();
    float height = (kind == Kind::Creature ? 2.0f : 1.0f) * (scale > 0.1f ? scale : 1.0f);

    for (float lift : { 0.0f, 0.5f, 1.0f }) {
        C3Vector point = { transform.m_position.x, transform.m_position.y, transform.m_position.z + height * lift };
        C3Vector toPoint = Subtract(point, scan->m_start);
        float along = Dot(toPoint, scan->m_direction);

        if (along <= 0.5f || along > scan->m_maxAlong) {
            continue;
        }

        float miss = Length(Subtract(toPoint, Scale(scan->m_direction, along)));
        float angle = (miss - height * 0.5f) / along;

        if (angle < scan->m_bestAngle) {
            scan->m_bestAngle = angle;
            scan->m_best = guid.m_guid;
        }
    }

    return 1;
}

// Nothing under the cursor hit exactly, the creature or gameobject nearest the click ray wins when it is close enough.
static uint64_t PickClosestToRay(const C3Vector& start, const C3Vector& end) {
    RayScan scan;
    scan.m_start = start;
    scan.m_direction = Normalize(Subtract(end, start));
    scan.m_maxAlong = Length(Subtract(end, start));
    scan.m_bestAngle = 0.05f;

    if (scan.m_maxAlong > 200.0f) {
        scan.m_maxAlong = 200.0f;
    }

    C3Vector hit{};

    if (TraceWorld(start, end, 0, hit)) {
        float blocked = Length(Subtract(hit, start)) + 4.0f;

        if (blocked < scan.m_maxAlong) {
            scan.m_maxAlong = blocked;
        }
    }

    ClntObjMgrEnumVisibleObjects(CollectClosestToRay, &scan);

    return scan.m_best;
}

static uint64_t PickObject(float x, float y) {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    if (!frame) {
        return 0;
    }

    float distance = 0.0f;
    uint64_t guid = WorldFrame_PickEditorModel(frame, x, y, TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT, 0, &distance);

    if (guid) {
        return guid;
    }

    C3Vector start{};
    C3Vector end{};

    if (!MouseRay(start, end)) {
        return 0;
    }

    // Gameobjects drawn as WMOs only show up through world collision.
    HITTESTRESULT result{};

    if (frame->HitTest(start, end, 0x3, &result) == 3 && KindOf(ObjectByGuid(result.m_guid.m_guid)) == Kind::GameObject) {
        return result.m_guid.m_guid;
    }

    return PickClosestToRay(start, end);
}

bool ClickSelect(float x, float y) {
    if (!Session::HasClearance(3)) {
        return false;
    }

    uint64_t picked = PickObject(x, y);

    return picked && Select(picked);
}

static bool PickNode(const C3Vector& start, const C3Vector& end, uint32_t& point, C3Vector& position) {
    C3Vector direction = Normalize(Subtract(end, start));
    float best = FLT_MAX;
    bool found = false;

    for (size_t i = 0; i < CreatureEditor::NodeCount(); i++) {
        uint32_t nodePoint = 0;
        C3Vector nodePosition{};

        if (!CreatureEditor::GetNode(i, nodePoint, nodePosition)) {
            continue;
        }

        C3Vector center = { nodePosition.x, nodePosition.y, nodePosition.z + CreatureEditor::kNodeLift };
        C3Vector toCenter = Subtract(center, start);
        float along = Dot(toCenter, direction);

        if (along <= 0.0f) {
            continue;
        }

        float miss = Length(Subtract(toCenter, Scale(direction, along)));
        float radius = 0.6f + along * 0.01f;

        if (miss <= radius && along < best) {
            best = along;
            point = nodePoint;
            position = nodePosition;
            found = true;
        }
    }

    return found;
}

static bool CurrentTarget(Transform& out) {
    DragState& drag = s_state.m_drag;

    if (drag.m_node) {
        for (size_t i = 0; i < CreatureEditor::NodeCount(); i++) {
            uint32_t point = 0;
            C3Vector position{};

            if (CreatureEditor::GetNode(i, point, position) && point == drag.m_point) {
                out = Transform();
                out.m_position = position;
                return true;
            }
        }

        return false;
    }

    CGObject_C* object = SelectedObject();

    return object && ReadTransform(object, out);
}

static uint64_t DragIgnore() {
    return s_state.m_drag.m_node ? 0 : s_state.m_guid;
}

// Starts or restarts the free drag in the sub mode the held modifiers pick, anchored where the object is now.
static void AnchorFreeDrag() {
    DragState& drag = s_state.m_drag;
    C3Vector start{};
    C3Vector end{};
    const C3Vector& position = drag.m_current.m_position;

    if (KeyHeld(VK_SHIFT)) {
        drag.m_type = Drag::Vertical;
        C3Vector toCamera = Subtract(CameraPosition(), position);
        toCamera.z = 0.0f;
        drag.m_planeNormal = Length(toCamera) > 0.001f ? Normalize(toCamera) : C3Vector{ 1.0f, 0.0f, 0.0f };
        drag.m_anchor = position;

        if (MouseRay(start, end)) {
            RayPlane(start, end, position, drag.m_planeNormal, drag.m_anchor);
        }

        drag.m_planeZ = position.z;
        return;
    }

    Drag previous = drag.m_type;
    drag.m_type = KeyHeld(VK_MENU) ? Drag::Plane : Drag::Ground;
    drag.m_planeZ = position.z;
    drag.m_offset = {};

    if (MouseRay(start, end)) {
        C3Vector hit{};
        bool found = false;

        if (drag.m_type == Drag::Ground && TraceWorld(start, end, DragIgnore(), hit)) {
            drag.m_planeZ = hit.z;
            found = true;
        } else {
            found = RayPlane(start, end, position, { 0.0f, 0.0f, 1.0f }, hit);
        }

        if (found) {
            drag.m_offset = { position.x - hit.x, position.y - hit.y, 0.0f };
        }
    }

    // A ground drag hugs the ground, only a height the developer set during this drag with Shift, Alt or the wheel is kept.
    float groundZ = 0.0f;
    bool keepHeight = previous == Drag::Vertical || previous == Drag::Plane;
    drag.m_height = keepHeight && GroundBelow(position, 2.0f, DragIgnore(), groundZ) ? position.z - groundZ : 0.0f;

    if (drag.m_height < 0.05f) {
        drag.m_height = 0.0f;
    }
}

// The surface under the cursor, never the dragged object itself, falls back to the anchor plane when the ray hits nothing.
static bool GroundCursor(const C3Vector& start, const C3Vector& end, C3Vector& cursor) {
    if (TraceWorld(start, end, DragIgnore(), cursor)) {
        return true;
    }

    return RayPlane(start, end, { 0.0f, 0.0f, s_state.m_drag.m_planeZ }, { 0.0f, 0.0f, 1.0f }, cursor);
}

static Drag WantedFreeMode() {
    if (KeyHeld(VK_SHIFT)) {
        return Drag::Vertical;
    }

    return KeyHeld(VK_MENU) ? Drag::Plane : Drag::Ground;
}

static void ApplyDragPreview() {
    DragState& drag = s_state.m_drag;

    if (drag.m_node) {
        CreatureEditor::PreviewNode(drag.m_point, drag.m_current.m_position);
        return;
    }

    if (CGObject_C* object = SelectedObject()) {
        PreviewTransform(s_state.m_guid, object, drag.m_current);
    } else {
        CancelDrag(false);
    }
}

static void UpdateFreeDrag() {
    DragState& drag = s_state.m_drag;

    if (WantedFreeMode() != drag.m_type) {
        AnchorFreeDrag();
    }

    C3Vector start{};
    C3Vector end{};

    if (!MouseRay(start, end)) {
        return;
    }

    C3Vector& position = drag.m_current.m_position;

    if (drag.m_type == Drag::Vertical) {
        C3Vector hit{};

        if (RayPlane(start, end, drag.m_anchor, drag.m_planeNormal, hit)) {
            position.z = drag.m_planeZ + (hit.z - drag.m_anchor.z);
        }

        return;
    }

    C3Vector hit{};

    if (drag.m_type == Drag::Ground) {
        if (!GroundCursor(start, end, hit)) {
            return;
        }
    } else if (!RayPlane(start, end, { position.x, position.y, drag.m_planeZ }, { 0.0f, 0.0f, 1.0f }, hit)) {
        return;
    }

    C3Vector target = { hit.x + drag.m_offset.x, hit.y + drag.m_offset.y, drag.m_type == Drag::Plane ? drag.m_planeZ : hit.z };

    // Far off hits mean the ray is almost flat, the object would fly away.
    if (Length(Subtract(target, CameraPosition())) > 1500.0f) {
        return;
    }

    if (drag.m_type == Drag::Ground) {
        float groundZ = 0.0f;

        if (GroundBelow(target, 4.0f, DragIgnore(), groundZ)) {
            target.z = groundZ;
        }

        target.z += drag.m_height;
    }

    position = target;
}

static void UpdateGizmoDrag() {
    DragState& drag = s_state.m_drag;
    C3Vector start{};
    C3Vector end{};

    if (!MouseRay(start, end)) {
        return;
    }

    if (drag.m_type == Drag::Axis) {
        C3Vector axis = AxisDirection(drag.m_axis);
        float t = ClosestRayAxisParameter(start, end, drag.m_start.m_position, axis);
        C3Vector onAxis = Add(drag.m_start.m_position, Scale(axis, t));
        drag.m_current.m_position = Add(drag.m_start.m_position, Subtract(onAxis, drag.m_anchor));
        return;
    }

    float angle = 0.0f;

    if (!RayAngleOnRing(start, end, drag.m_start.m_position, drag.m_ringAxis, drag.m_ringRef, drag.m_ringPerp, angle)) {
        return;
    }

    float delta = NormalizeSignedAngleDelta(angle - drag.m_anchorAngle);

    if (KeyHeld(VK_CONTROL)) {
        float step = 15.0f * kPi / 180.0f;
        delta = roundf(delta / step) * step;
    }

    Quat deltaRotation = AxisAngleToQuat(drag.m_ringAxis.x, drag.m_ringAxis.y, drag.m_ringAxis.z, delta);
    drag.m_current.m_rotation = NormalizeQuat(MultiplyQuat(deltaRotation, drag.m_start.m_rotation));

    if (drag.m_axis == Axis::Z) {
        drag.m_current.m_facing = drag.m_start.m_facing + delta;
    }
}

static bool BeginGizmoDrag(const C3Vector& start, const C3Vector& end) {
    Transform transform;

    if (s_state.m_gizmo == Gizmo::Off || !CurrentTarget(transform)) {
        return false;
    }

    DragState& drag = s_state.m_drag;

    if (s_state.m_hoverMove != Axis::None) {
        C3Vector axis = AxisDirection(s_state.m_hoverMove);
        float t = ClosestRayAxisParameter(start, end, transform.m_position, axis);

        drag = DragState();
        drag.m_type = Drag::Axis;
        drag.m_axis = s_state.m_hoverMove;
        drag.m_start = transform;
        drag.m_current = transform;
        drag.m_anchor = Add(transform.m_position, Scale(axis, t));
        return true;
    }

    if (s_state.m_hoverRing != Axis::None) {
        C3Vector axis{};
        C3Vector ref{};
        C3Vector perp{};
        RingBasis(s_state.m_hoverRing, axis, ref, perp);

        float angle = 0.0f;

        if (!RayAngleOnRing(start, end, transform.m_position, axis, ref, perp, angle)) {
            return false;
        }

        drag = DragState();
        drag.m_type = Drag::Ring;
        drag.m_axis = s_state.m_hoverRing;
        drag.m_start = transform;
        drag.m_current = transform;
        drag.m_ringAxis = axis;
        drag.m_ringRef = ref;
        drag.m_ringPerp = perp;
        drag.m_anchorAngle = angle;
        return true;
    }

    return false;
}

static void FinishDrag() {
    DragState& drag = s_state.m_drag;
    Drag type = drag.m_type;

    if (type == Drag::None || type == Drag::Pending) {
        drag = DragState();
        return;
    }

    ApplyDragPreview();

    if (drag.m_node) {
        CreatureEditor::MoveNode(drag.m_point, drag.m_current.m_position);
    }

    drag = DragState();
}

static void UpdateHover() {
    s_state.m_hoverMove = Axis::None;
    s_state.m_hoverRing = Axis::None;

    Transform transform;

    if (s_state.m_gizmo == Gizmo::Off || !Mode::IsMouseOverWorld() || !CurrentTarget(transform)) {
        return;
    }

    C3Vector start{};
    C3Vector end{};

    if (!MouseRay(start, end)) {
        return;
    }

    float scale = GizmoScale(transform.m_position);

    if (s_state.m_gizmo == Gizmo::Move) {
        s_state.m_hoverMove = PickTranslationGizmo(start, end, transform.m_position, scale);

        if (s_state.m_hoverMove == Axis::None && s_state.m_kind == Kind::Creature) {
            float ringScale = scale * kYawRingScale;
            PickResult ring = PickRing(start, end, transform.m_position, { 0.0f, 0.0f, 1.0f }, ringScale, 0.10f * ringScale);
            s_state.m_hoverRing = ring.m_hit ? Axis::Z : Axis::None;
        }

        return;
    }

    if (s_state.m_kind == Kind::Creature || s_state.m_drag.m_node) {
        PickResult ring = PickRing(start, end, transform.m_position, { 0.0f, 0.0f, 1.0f }, scale, 0.10f * scale);
        s_state.m_hoverRing = ring.m_hit ? Axis::Z : Axis::None;
        return;
    }

    s_state.m_hoverRing = PickRotationGizmo(start, end, transform.m_position, scale);
}

static void Nudge(const C3Vector& move, float yaw) {
    CGObject_C* object = SelectedObject();
    Transform transform;

    if (!object || !ReadTransform(object, transform)) {
        return;
    }

    C3Vector before = transform.m_position;
    transform.m_position = Add(before, move);

    // Sideways nudges follow the ground and keep the height above it, only an up or down nudge changes that height.
    float oldGround = 0.0f;
    float newGround = 0.0f;

    if (move.z == 0.0f && (move.x != 0.0f || move.y != 0.0f) && GroundBelow(before, 2.0f, s_state.m_guid, oldGround)) {
        float height = before.z - oldGround;

        if (height < 0.05f) {
            height = 0.0f;
        }

        if (GroundBelow(transform.m_position, 4.0f + height, s_state.m_guid, newGround)) {
            transform.m_position.z = newGround + height;
        }
    }

    if (yaw != 0.0f) {
        transform.m_facing += yaw;
        transform.m_rotation = NormalizeQuat(MultiplyQuat(AxisAngleToQuat(0.0f, 0.0f, 1.0f, yaw), transform.m_rotation));
    }

    PreviewTransform(s_state.m_guid, object, transform);
}

static void SnapToGround() {
    CGObject_C* object = SelectedObject();
    Transform transform;
    float groundZ = 0.0f;

    if (!object || !ReadTransform(object, transform) || !GroundBelow(transform.m_position, 2.0f, s_state.m_guid, groundZ)) {
        return;
    }

    transform.m_position.z = groundZ;
    PreviewTransform(s_state.m_guid, object, transform);
}

static void CameraBasis(C3Vector& forward, C3Vector& right) {
    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;
    forward = frame && frame->m_camera ? frame->m_camera->Forward() : C3Vector{ 1.0f, 0.0f, 0.0f };
    forward.z = 0.0f;
    forward = Length(forward) > 0.001f ? Normalize(forward) : C3Vector{ 1.0f, 0.0f, 0.0f };
    right = { forward.y, -forward.x, 0.0f };
}

// "ALT-CTRL-SHIFT-X" -> "X" plus the modifier flags.
static const char* SplitKey(const char* key, bool& shift, bool& ctrl, bool& alt) {
    shift = false;
    ctrl = false;
    alt = false;

    for (;;) {
        if (!strncmp(key, "SHIFT-", 6) && key[6]) {
            shift = true;
            key += 6;
        } else if (!strncmp(key, "CTRL-", 5) && key[5]) {
            ctrl = true;
            key += 5;
        } else if (!strncmp(key, "ALT-", 4) && key[4]) {
            alt = true;
            key += 4;
        } else {
            return key;
        }
    }
}

class SelectionHandler : public Mode::Handler {
    public:
    bool OwnsTool(const char* tool) const override {
        return !strcmp(tool, "select") || !strcmp(tool, "move") || !strcmp(tool, "rotate") || !strcmp(tool, "waypoint");
    }

    bool OnMouseDown(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) override {
        if (button != MOUSE_BUTTON_LEFT) {
            return false;
        }

        C3Vector start{};
        C3Vector end{};

        if (!MouseRay(start, end)) {
            return true;
        }

        if (BeginGizmoDrag(start, end)) {
            return true;
        }

        if (Mode::ToolIs("waypoint") && OnWaypointDown(start, end, x, y)) {
            return true;
        }

        uint64_t picked = PickObject(x, y);

        if (KeyHeld(VK_CONTROL)) {
            if (!picked) {
                SetGizmo(Gizmo::Off);
            } else if (picked == s_state.m_guid) {
                SetGizmo(static_cast<Gizmo>((static_cast<uint8_t>(s_state.m_gizmo) + 1) % 3));
            } else if (Select(picked)) {
                SetGizmo(Mode::ToolIs("rotate") ? Gizmo::Rotate : Gizmo::Move);
            }

            return true;
        }

        if (!picked) {
            if (!Mode::ToolIs("waypoint")) {
                Clear();
            }

            return true;
        }

        if (!Select(picked) || s_state.m_gizmo != Gizmo::Off) {
            return true;
        }

        Transform transform;

        if (CGObject_C* object = SelectedObject(); object && ReadTransform(object, transform)) {
            DragState& drag = s_state.m_drag;
            drag = DragState();
            drag.m_type = Drag::Pending;
            drag.m_downX = x;
            drag.m_downY = y;
            drag.m_start = transform;
            drag.m_current = transform;
        }

        return true;
    }

    bool OnMouseUp(MOUSEBUTTON button, uint32_t metaKeys, float x, float y) override {
        if (button == MOUSE_BUTTON_LEFT) {
            FinishDrag();
        }

        return true;
    }

    bool OnMouseWheel(int32_t delta, uint32_t metaKeys) override {
        DragState& drag = s_state.m_drag;
        float step = KeyHeld(VK_CONTROL) ? 0.05f : 0.25f;

        if (drag.m_type == Drag::Ground) {
            drag.m_height += delta * step;
            return true;
        }

        if (drag.m_type == Drag::Plane || drag.m_type == Drag::Vertical) {
            drag.m_current.m_position.z += delta * step;
            drag.m_planeZ += delta * step;
            return true;
        }

        if (drag.m_type == Drag::None && s_state.m_guid && KeyHeld(VK_CONTROL) && !Mode::ToolIs("waypoint")) {
            float degrees = KeyHeld(VK_SHIFT) ? 1.0f : 5.0f;
            Nudge({}, delta * degrees * kPi / 180.0f);
            return true;
        }

        return false;
    }

    bool OnKey(const char* key, bool down) override {
        bool shift = false;
        bool ctrl = false;
        bool alt = false;
        const char* base = SplitKey(key, shift, ctrl, alt);
        DragState& drag = s_state.m_drag;

        if (!strcmp(base, "ESCAPE")) {
            if (!down) {
                return false;
            }

            if (drag.m_type != Drag::None) {
                CancelDrag(true);
            } else if (s_state.m_gizmo != Gizmo::Off && Mode::ToolIs("select")) {
                SetGizmo(Gizmo::Off);
            } else if (Mode::ToolIs("waypoint") && CreatureEditor::SelectedPoint()) {
                CreatureEditor::SetSelectedPoint(0);
            } else if (s_state.m_guid) {
                Clear();
            } else {
                return false;
            }

            return true;
        }

        if (Mode::ToolIs("waypoint")) {
            if (!strcmp(base, "DELETE") && CreatureEditor::SelectedPoint()) {
                if (down) {
                    CreatureEditor::DeleteNode(CreatureEditor::SelectedPoint());
                    CreatureEditor::SetSelectedPoint(0);
                }

                return true;
            }

            return false;
        }

        if (!s_state.m_guid) {
            return false;
        }

        float step = shift ? 0.1f : (ctrl ? 2.0f : 0.5f);
        float yawStep = (shift ? 1.0f : 15.0f) * kPi / 180.0f;
        C3Vector forward{};
        C3Vector right{};
        CameraBasis(forward, right);

        struct Binding {
            const char* m_key;
            C3Vector m_move;
            float m_yaw;
        };

        const Binding bindings[] = {
            { "PAGEUP", { 0.0f, 0.0f, step }, 0.0f },
            { "PAGEDOWN", { 0.0f, 0.0f, -step }, 0.0f },
            { "UP", Scale(forward, step), 0.0f },
            { "DOWN", Scale(forward, -step), 0.0f },
            { "RIGHT", Scale(right, step), 0.0f },
            { "LEFT", Scale(right, -step), 0.0f },
            { "[", {}, yawStep },
            { "]", {}, -yawStep },
        };

        for (const Binding& binding : bindings) {
            if (strcmp(base, binding.m_key) != 0) {
                continue;
            }

            if (!down) {
                return true;
            }

            if (drag.m_type == Drag::Ground && binding.m_move.z != 0.0f) {
                drag.m_height += binding.m_move.z;
            } else if (drag.m_type == Drag::None) {
                Nudge(binding.m_move, binding.m_yaw);
            }

            return true;
        }

        if (!strcmp(base, "G") && !ctrl && !alt) {
            if (down && drag.m_type == Drag::None) {
                SnapToGround();
            } else if (down && drag.m_type == Drag::Ground) {
                drag.m_height = 0.0f;
            }

            return true;
        }

        return false;
    }

    void OnUpdate(float seconds) override {
        if (s_state.m_guid && !ObjectByGuid(s_state.m_guid)) {
            CancelDrag(false);
        }

        HoldPendingMoves();

        DragState& drag = s_state.m_drag;

        if (drag.m_type == Drag::Pending) {
            float x = 0.0f;
            float y = 0.0f;
            Mode::GetMouse(x, y);

            if (fabsf(x - drag.m_downX) + fabsf(y - drag.m_downY) < kDragThreshold) {
                return;
            }

            AnchorFreeDrag();
        }

        switch (drag.m_type) {
            case Drag::Ground:
            case Drag::Vertical:
            case Drag::Plane:
                UpdateFreeDrag();
                ApplyDragPreview();
                break;
            case Drag::Axis:
            case Drag::Ring:
                UpdateGizmoDrag();
                ApplyDragPreview();
                break;
            default:
                UpdateHover();
                break;
        }
    }

    void OnWorldRender(CGWorldFrame* frame) override {
        if (!frame || !frame->m_camera || !g_theGxDevicePtr) {
            return;
        }

        Transform transform;
        bool hasTarget = CurrentTarget(transform);

        if (!hasTarget) {
            return;
        }

        CGxDevice* device = g_theGxDevicePtr;
        device->RsPush();
        GxRsSet(GxRs_VertexShader, nullptr);
        GxRsSet(GxRs_PixelShader, nullptr);
        GxRsSet(GxRs_Culling, 0);
        GxRsSet(GxRs_Fog, 0);
        GxRsSet(GxRs_Lighting, 0);

        CImVector transparent;
        GxSceneClear(2, transparent);
        GxRsSet(GxRs_DepthTest, 1);

        C3Vector origin = Subtract(transform.m_position, frame->m_camera->m_position);
        float scale = GizmoScale(transform.m_position);

        if (!s_state.m_drag.m_node) {
            CImVector marker = s_state.m_drag.m_type != Drag::None ? CImVector(255, 220, 60, 255) : CImVector(255, 255, 255, 200);
            GizmoDraw::DrawTorus(origin, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, 0.9f, 0.04f, marker);

            if (s_state.m_drag.m_type == Drag::Vertical || s_state.m_drag.m_height > 0.1f) {
                float groundZ = 0.0f;

                if (GroundBelow(transform.m_position, 0.5f, s_state.m_guid, groundZ)) {
                    C3Vector foot = { origin.x, origin.y, origin.z - (transform.m_position.z - groundZ) };
                    GizmoDraw::DrawLine(origin, foot, marker);
                }
            }
        }

        if (s_state.m_gizmo == Gizmo::Move || s_state.m_drag.m_type == Drag::Axis) {
            Axis axis = s_state.m_drag.m_type == Drag::Axis ? s_state.m_drag.m_axis : s_state.m_hoverMove;
            GizmoDraw::DrawTranslationGizmo(origin, scale, axis);

            if (s_state.m_kind == Kind::Creature && !s_state.m_drag.m_node && s_state.m_drag.m_type != Drag::Axis) {
                Axis ring = s_state.m_drag.m_type == Drag::Ring ? Axis::Z : s_state.m_hoverRing;
                GizmoDraw::DrawRotationGizmo(origin, scale * kYawRingScale, ring, true);
            }
        } else if (s_state.m_gizmo == Gizmo::Rotate || s_state.m_drag.m_type == Drag::Ring) {
            Axis axis = s_state.m_drag.m_type == Drag::Ring ? s_state.m_drag.m_axis : s_state.m_hoverRing;
            GizmoDraw::DrawRotationGizmo(origin, scale, axis, s_state.m_kind == Kind::Creature);
        }

        device->RsPop();
    }

    void OnActivate() override {
        CreatureEditor::SetPathForced(Mode::ToolIs("waypoint"));
    }

    void OnDeactivate() override {
        CancelDrag(true);
        CreatureEditor::SetPathForced(false);
    }

    void OnToolChanged(const char* tool) override {
        CancelDrag(true);
        CreatureEditor::SetPathForced(!strcmp(tool, "waypoint"));

        if (!strcmp(tool, "move")) {
            SetGizmo(Gizmo::Move);
        } else if (!strcmp(tool, "rotate")) {
            SetGizmo(Gizmo::Rotate);
        } else if (!strcmp(tool, "waypoint")) {
            SetGizmo(Gizmo::Off);
        } else if (!strcmp(tool, "select") && s_state.m_guid && s_state.m_gizmo == Gizmo::Off) {
            SetGizmo(Gizmo::Move);
        }
    }

    void OnWorldUnloaded() override {
        CancelDrag(false);
        s_state.m_spawnPending = Kind::None;
        s_state.m_moves.clear();
        SignalMoves();
        Clear();
    }

    private:
    bool OnWaypointDown(const C3Vector& start, const C3Vector& end, float x, float y) {
        uint32_t point = 0;
        C3Vector position{};

        if (PickNode(start, end, point, position)) {
            CreatureEditor::SetSelectedPoint(point);

            DragState& drag = s_state.m_drag;
            drag = DragState();
            drag.m_type = Drag::Pending;
            drag.m_node = true;
            drag.m_point = point;
            drag.m_downX = x;
            drag.m_downY = y;
            drag.m_start.m_position = position;
            drag.m_current.m_position = position;
            return true;
        }

        if (!KeyHeld(VK_SHIFT) || !CreatureEditor::SnapshotGuid()) {
            return false;
        }

        C3Vector hit{};

        if (TraceWorld(start, end, 0, hit)) {
            uint32_t after = CreatureEditor::SelectedPoint();

            if (!after && CreatureEditor::NodeCount()) {
                C3Vector unused{};
                CreatureEditor::GetNode(CreatureEditor::NodeCount() - 1, after, unused);
            }

            CreatureEditor::AddNode(after, hit);
        }

        return true;
    }
};

static SelectionHandler s_handler;

static uint64_t GuidFromLua(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TSTRING) {
        if (const char* text = lua_tostring(L, index)) {
            return std::strtoull(text, nullptr, 10);
        }
    }

    return 0;
}

static Kind KindFromLua(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        return lua_tonumber(L, index) == 1 ? Kind::GameObject : Kind::Creature;
    }

    const char* text = lua_tostring(L, index);

    if (text && !SStrCmpI(text, "gameobject", STORM_MAX_STR)) {
        return Kind::GameObject;
    }

    return Kind::Creature;
}

struct NearestScan {
    C3Vector m_origin;
    Kind m_kind = Kind::None;
    float m_bestDistance = 0.0f;
    uint64_t m_best = 0;
};

static int32_t CollectNearest(WOWGUID guid, void* param) {
    NearestScan* scan = static_cast<NearestScan*>(param);
    CGObject_C* object = ObjectByGuid(guid.m_guid);
    Kind kind = KindOf(object);

    if (kind == Kind::None || (scan->m_kind != Kind::None && kind != scan->m_kind)) {
        return 1;
    }

    if (kind == Kind::GameObject && static_cast<CGGameObject_C*>(object)->m_gameObject->m_createdBy.m_guid) {
        return 1;
    }

    if (kind == Kind::Creature) {
        CGUnitData* unit = static_cast<CGUnit_C*>(object)->m_unit;

        if (unit->m_unitFieldSummonedby.m_guid || unit->m_unitFieldCreatedby.m_guid || unit->m_unitFieldCharmedby.m_guid) {
            return 1;
        }
    }

    Transform transform;

    if (!ReadTransform(object, transform)) {
        return 1;
    }

    float dx = transform.m_position.x - scan->m_origin.x;
    float dy = transform.m_position.y - scan->m_origin.y;
    float dz = transform.m_position.z - scan->m_origin.z;
    float distance = dx * dx + dy * dy + dz * dz;

    if (distance < scan->m_bestDistance) {
        scan->m_bestDistance = distance;
        scan->m_best = guid.m_guid;
    }

    return 1;
}

uint64_t SelectNearest(Kind kind, float maxDistance) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    NearestScan scan;
    scan.m_kind = kind;
    scan.m_bestDistance = maxDistance * maxDistance;

    if (FreeCam::IsActive()) {
        scan.m_origin = FreeCam::GetPosition();
    } else if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
        scan.m_origin = player->GetPosition();
    } else {
        return 0;
    }

    ClntObjMgrEnumVisibleObjects(CollectNearest, &scan);

    return scan.m_best && Select(scan.m_best) ? scan.m_best : 0;
}

// GameEditor_SelectNearest(["creature" | "gameobject"] [, maxDistance]) returns the selected guid string or nil.
static int32_t Script_SelectNearest(lua_State* L) {
    Kind kind = Kind::None;

    if (const char* name = lua_tostring(L, 1)) {
        if (!std::strcmp(name, "creature")) {
            kind = Kind::Creature;
        } else if (!std::strcmp(name, "gameobject")) {
            kind = Kind::GameObject;
        }
    }

    float maxDistance = lua_isnumber(L, 2) ? static_cast<float>(lua_tonumber(L, 2)) : 200.0f;
    uint64_t guid = SelectNearest(kind, maxDistance);

    if (!guid) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, GobEditor::GuidToString(guid).c_str());
    return 1;
}

// Kind, guid string, entry, name, x, y, z, o of the selection, nil when nothing is selected.
static int32_t Script_GetSelection(lua_State* L) {
    CGObject_C* object = SelectedObject();
    Transform transform;

    if (!object || !ReadTransform(object, transform)) {
        if (!s_state.m_guid) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushstring(L, KindName(s_state.m_kind));
        lua_pushstring(L, GobEditor::GuidToString(s_state.m_guid).c_str());
        return 2;
    }

    const char* name = object->GetObjectName();

    lua_pushstring(L, KindName(s_state.m_kind));
    lua_pushstring(L, GobEditor::GuidToString(s_state.m_guid).c_str());
    lua_pushnumber(L, GobEditor::DecodeGuid(s_state.m_guid).m_entry);
    lua_pushstring(L, name ? name : "");
    lua_pushnumber(L, transform.m_position.x);
    lua_pushnumber(L, transform.m_position.y);
    lua_pushnumber(L, transform.m_position.z);
    lua_pushnumber(L, transform.m_facing);
    return 8;
}

// GameEditor_Select([kind], guid string), the kind is optional since the guid already says it.
static int32_t Script_Select(lua_State* L) {
    uint64_t guid = GuidFromLua(L, lua_gettop(L) >= 2 ? 2 : 1);
    lua_pushboolean(L, guid && Select(guid) ? 1 : 0);
    return 1;
}

static int32_t Script_ClearSelection(lua_State* L) {
    Clear();
    return 0;
}

static int32_t Script_SetGizmo(lua_State* L) {
    const char* mode = lua_tostring(L, 1);

    if (!mode || !SStrCmpI(mode, "off", STORM_MAX_STR)) {
        SetGizmo(Gizmo::Off);
    } else if (!SStrCmpI(mode, "rotate", STORM_MAX_STR)) {
        SetGizmo(Gizmo::Rotate);
    } else {
        SetGizmo(Gizmo::Move);
    }

    return 0;
}

static int32_t Script_GetGizmo(lua_State* L) {
    static const char* const s_names[] = { "off", "move", "rotate" };
    lua_pushstring(L, s_names[static_cast<uint8_t>(s_state.m_gizmo)]);
    return 1;
}

static int32_t Script_IsDragging(lua_State* L) {
    Drag type = s_state.m_drag.m_type;
    lua_pushboolean(L, type != Drag::None && type != Drag::Pending ? 1 : 0);
    return 1;
}

// GameEditor_NudgeSelection(dx, dy, dz, [yaw degrees]) moves the selection and saves it shortly after.
static int32_t Script_NudgeSelection(lua_State* L) {
    C3Vector move = {
        static_cast<float>(lua_tonumber(L, 1)),
        static_cast<float>(lua_tonumber(L, 2)),
        static_cast<float>(lua_tonumber(L, 3)),
    };

    Nudge(move, static_cast<float>(lua_tonumber(L, 4)) * kPi / 180.0f);
    return 0;
}

static int32_t Script_SetSelectionTransform(lua_State* L) {
    CGObject_C* object = SelectedObject();
    Transform transform;

    if (!object || !ReadTransform(object, transform)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (lua_isnumber(L, 1)) {
        transform.m_position.x = static_cast<float>(lua_tonumber(L, 1));
    }

    if (lua_isnumber(L, 2)) {
        transform.m_position.y = static_cast<float>(lua_tonumber(L, 2));
    }

    if (lua_isnumber(L, 3)) {
        transform.m_position.z = static_cast<float>(lua_tonumber(L, 3));
    }

    if (lua_isnumber(L, 4)) {
        float yaw = static_cast<float>(lua_tonumber(L, 4));
        transform.m_rotation = NormalizeQuat(MultiplyQuat(AxisAngleToQuat(0.0f, 0.0f, 1.0f, yaw - transform.m_facing), transform.m_rotation));
        transform.m_facing = yaw;
    }

    PreviewTransform(s_state.m_guid, object, transform);

    lua_pushboolean(L, 1);
    return 1;
}

// GameEditor_SetSelectionRotation(x, y, z, w) turns the selected gameobject locally, Save sends it.
static int32_t Script_SetSelectionRotation(lua_State* L) {
    CGObject_C* object = SelectedObject();
    Transform transform;

    if (!object || s_state.m_kind != Kind::GameObject || !ReadTransform(object, transform)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Quat rotation = {
        static_cast<float>(lua_tonumber(L, 4)),
        static_cast<float>(lua_tonumber(L, 1)),
        static_cast<float>(lua_tonumber(L, 2)),
        static_cast<float>(lua_tonumber(L, 3)),
    };

    float roll = 0.0f;
    float pitch = 0.0f;
    transform.m_rotation = NormalizeQuat(rotation);
    QuatToEuler(transform.m_rotation, roll, pitch, transform.m_facing);
    PreviewTransform(s_state.m_guid, object, transform);

    lua_pushboolean(L, 1);
    return 1;
}

static uint64_t MoveGuidArgument(lua_State* L) {
    uint64_t guid = GuidFromLua(L, 1);
    return guid ? guid : s_state.m_guid;
}

// GameEditor_HasUnsavedMove([guid]) is true when that object, the selection by default, was moved and not saved.
static int32_t Script_HasUnsavedMove(lua_State* L) {
    lua_pushboolean(L, s_state.m_moves.count(MoveGuidArgument(L)) ? 1 : 0);
    return 1;
}

static int32_t Script_GetUnsavedMoveCount(lua_State* L) {
    lua_pushnumber(L, static_cast<double>(s_state.m_moves.size()));
    return 1;
}

static int32_t Script_SaveMove(lua_State* L) {
    lua_pushboolean(L, SaveMove(MoveGuidArgument(L)) ? 1 : 0);
    return 1;
}

static int32_t Script_RevertMove(lua_State* L) {
    lua_pushboolean(L, RevertMove(MoveGuidArgument(L)) ? 1 : 0);
    return 1;
}

static int32_t Script_SaveAllMoves(lua_State* L) {
    if (IsDraggingObject(s_state.m_guid)) {
        CancelDrag(false);
    }

    size_t count = s_state.m_moves.size();

    for (const auto& [guid, move] : s_state.m_moves) {
        SendMove(guid, move);
    }

    s_state.m_moves.clear();

    if (count) {
        SignalMoves();
    }

    lua_pushnumber(L, static_cast<double>(count));
    return 1;
}

static int32_t Script_RevertAllMoves(lua_State* L) {
    if (IsDraggingObject(s_state.m_guid)) {
        CancelDrag(false);
    }

    size_t count = s_state.m_moves.size();

    for (const auto& [guid, move] : s_state.m_moves) {
        if (CGObject_C* object = ObjectByGuid(guid); object && KindOf(object) == move.m_kind) {
            WriteTransform(object, move.m_original);
        }
    }

    s_state.m_moves.clear();

    if (count) {
        SignalMoves();
    }

    lua_pushnumber(L, static_cast<double>(count));
    return 1;
}

static int32_t Script_SnapToGround(lua_State* L) {
    SnapToGround();
    return 0;
}

// x, y, z of the world under the cursor.
static int32_t Script_GetCursorWorldPosition(lua_State* L) {
    C3Vector start{};
    C3Vector end{};
    C3Vector hit{};

    if (!MouseRay(start, end) || !TraceWorld(start, end, 0, hit)) {
        return 0;
    }

    lua_pushnumber(L, hit.x);
    lua_pushnumber(L, hit.y);
    lua_pushnumber(L, hit.z);
    return 3;
}

// GameEditor_Spawn(kind, entry, [x, y, z, o]), without a position it spawns in front of the camera on the ground.
static int32_t Script_Spawn(lua_State* L) {
    Kind kind = KindFromLua(L, 1);
    uint32_t entry = static_cast<uint32_t>(lua_tonumber(L, 2));

    if (!entry) {
        lua_pushboolean(L, 0);
        return 1;
    }

    C3Vector position{};
    float orientation = 0.0f;

    if (lua_isnumber(L, 3) && lua_isnumber(L, 4) && lua_isnumber(L, 5)) {
        position = {
            static_cast<float>(lua_tonumber(L, 3)),
            static_cast<float>(lua_tonumber(L, 4)),
            static_cast<float>(lua_tonumber(L, 5)),
        };
        orientation = static_cast<float>(lua_tonumber(L, 6));
    } else {
        C3Vector forward{};
        C3Vector right{};
        CameraBasis(forward, right);

        C3Vector camera = CameraPosition();
        C3Vector ahead = Add(camera, Scale(forward, 8.0f));
        float groundZ = 0.0f;
        position = ahead;

        if (GroundBelow(ahead, 20.0f, 0, groundZ)) {
            position.z = groundZ;
        }

        orientation = atan2f(-forward.y, -forward.x);
    }

    s_state.m_spawnPending = kind;

    Net::Packet(Net::CMSG_EDITOR_SPAWN)
        .PutUInt8(kind == Kind::GameObject ? 1 : 0)
        .PutUInt32(entry)
        .PutFloat(position.x)
        .PutFloat(position.y)
        .PutFloat(position.z)
        .PutFloat(orientation)
        .PutUInt32(0)
        .Send();

    lua_pushboolean(L, 1);
    return 1;
}

// GameEditor_DeleteSpawn([kind], [guid string]) deletes the spawn, the selection by default.
static int32_t Script_DeleteSpawn(lua_State* L) {
    uint64_t guid = GuidFromLua(L, 2);

    if (!guid) {
        guid = GuidFromLua(L, 1);
    }

    if (!guid) {
        guid = s_state.m_guid;
    }

    Kind kind = KindOf(ObjectByGuid(guid));

    if (kind == Kind::None) {
        kind = lua_isnoneornil(L, 1) ? s_state.m_kind : KindFromLua(L, 1);
    }

    if (!guid || kind == Kind::None) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (s_state.m_moves.erase(guid)) {
        SignalMoves();
    }

    if (guid == s_state.m_guid) {
        CancelDrag(false);
        Clear();
    }

    if (kind == Kind::Creature && CreatureEditor::SnapshotGuid() == guid) {
        CreatureEditor::Forget();
    } else if (kind == Kind::GameObject) {
        GobEditor::Forget();
    }

    Net::Packet(Net::CMSG_EDITOR_DELETE_SPAWN).PutUInt8(kind == Kind::GameObject ? 1 : 0).PutUInt64(guid).Send();

    lua_pushboolean(L, 1);
    return 1;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    return Function(L);
}

void OnCreatureSnapshot(uint64_t guid) {
    OnSnapshot(Kind::Creature, guid);
}

void OnGameObjectSnapshot(uint64_t guid) {
    OnSnapshot(Kind::GameObject, guid);
}

void Apply() {
    Mode::RegisterHandler(&s_handler);

    Lua::RegisterFunction("GameEditor_GetSelection", Gated<Script_GetSelection>);
    Lua::RegisterFunction("GameEditor_Select", Gated<Script_Select>);
    Lua::RegisterFunction("GameEditor_SelectNearest", Gated<Script_SelectNearest>);
    Lua::RegisterFunction("GameEditor_ClearSelection", Gated<Script_ClearSelection>);
    Lua::RegisterFunction("GameEditor_SetGizmo", Gated<Script_SetGizmo>);
    Lua::RegisterFunction("GameEditor_GetGizmo", Gated<Script_GetGizmo>);
    Lua::RegisterFunction("GameEditor_IsDragging", Gated<Script_IsDragging>);
    Lua::RegisterFunction("GameEditor_NudgeSelection", Gated<Script_NudgeSelection>);
    Lua::RegisterFunction("GameEditor_SetSelectionTransform", Gated<Script_SetSelectionTransform>);
    Lua::RegisterFunction("GameEditor_SetSelectionRotation", Gated<Script_SetSelectionRotation>);
    Lua::RegisterFunction("GameEditor_HasUnsavedMove", Gated<Script_HasUnsavedMove>);
    Lua::RegisterFunction("GameEditor_GetUnsavedMoveCount", Gated<Script_GetUnsavedMoveCount>);
    Lua::RegisterFunction("GameEditor_SaveMove", Gated<Script_SaveMove>);
    Lua::RegisterFunction("GameEditor_SaveAllMoves", Gated<Script_SaveAllMoves>);
    Lua::RegisterFunction("GameEditor_RevertMove", Gated<Script_RevertMove>);
    Lua::RegisterFunction("GameEditor_RevertAllMoves", Gated<Script_RevertAllMoves>);
    Lua::RegisterFunction("GameEditor_SnapToGround", Gated<Script_SnapToGround>);
    Lua::RegisterFunction("GameEditor_GetCursorWorldPosition", Gated<Script_GetCursorWorldPosition>);
    Lua::RegisterFunction("GameEditor_Spawn", Gated<Script_Spawn>);
    Lua::RegisterFunction("GameEditor_DeleteSpawn", Gated<Script_DeleteSpawn>);
}

void OnGameDestroy() {
    s_state = SelectionState();
}

}
