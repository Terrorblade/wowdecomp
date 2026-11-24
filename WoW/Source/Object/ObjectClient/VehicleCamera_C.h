#ifndef WOW_SOURCE_OBJECT_OBJECTCLIENT_VEHICLECAMERA_C_H
#define WOW_SOURCE_OBJECT_OBJECTCLIENT_VEHICLECAMERA_C_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Object/ObjectClient/Missile_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"

class CGObject_C;
class CGUnit_C;
class VehicleRec;
class VehicleSeatRec;
class CGInputControl;

class CVehicleCamera_C;

class CVehicleCameraHeap {
    public:
    /* 0000 */ uint32_t m_heapId;

    CVehicleCamera_C* New();
};

class CVehicleCamera_C {
    public:
    enum CAMERA_STATE : uint32_t {
        STATE_NONE = 0,
        STATE_ENTER_DELAY = 1,
        STATE_ENTERING = 2,
        STATE_ATTACHED = 3,
        STATE_EXIT_DELAY = 4,
        STATE_EXITING = 5,
    };

    enum CAMERA_FLAGS : uint32_t {
        FLAG_NONE = 0,
        FLAG_MOTION_TIMING_SET = 0x1,
        FLAG_POSITION_INTERPOLATION = 0x2,
        FLAG_SYNC_FACING = 0x4,
        FLAG_WORLD_STATE_UPDATED = 0x8,
        FLAG_FACING_INTERPOLATION = 0x10,
        FLAG_RESET_END_CHASERS = 0x20,
        FLAG_SAFE_POSITION_VALID = 0x40,
        FLAG_CHANGING_TARGET = 0x80,
    };

    static CVehicleCameraHeap* s_cameraPool;
    static uint32_t s_cameraCount;

    static void InitSystem();
    static void UninitSystem();
    static CVehicleCamera_C* Create(CGUnit_C* unit, uint32_t time);
    static int32_t ConvertFromRawToWorld(float& facing, CGObject_C* relativeTo);
    static int32_t ConvertFromRawOrOffsetToWorld(C3Vector& position, CGObject_C* relativeTo);
    static int32_t ConvertFromWorldToRawOrOffset(C3Vector& position, CGObject_C* relativeTo);
    static int32_t ConvertSmoothFacingFromRawToWorld(float& facing, CGObject_C* relativeTo);
    static int32_t ConvertSmoothFacingFromWorldToRaw(float& facing, CGObject_C* relativeTo);

    /* 0000 */ uint32_t m_memHandle;
    /* 0004 */ CGUnit_C* m_unit;
    /* 0008 */ uint32_t m_flags;
    /* 000C */ VehicleRec* m_vehicleRec;
    /* 0010 */ VehicleSeatRec* m_seatRec;
    /* 0014 */ C3Vector m_startWorldPos;
    /* 0020 */ C3Vector m_startRawPos;
    /* 002C */ C3Vector m_curWorldPos;
    /* 0038 */ C3Vector m_safeCurWorldPos;
    /* 0044 */ float m_collideFraction;
    /* 0048 */ float m_collideVelocity;
    /* 004C */ ChaserState m_rawPosChaser;
    /* 0064 */ ChaserState m_endPosChaser;
    /* 007C */ float m_startFacing;
    /* 0080 */ float m_worldFacing;
    /* 0084 */ float m_facing;
    /* 0088 */ float m_targetFacing;
    /* 008C */ CSmoothAngle m_rawFacingChaser;
    /* 0094 */ CSmoothAngle m_endFacingChaser;
    /* 009C */ uint32_t m_motionDuration;
    /* 00A0 */ uint32_t m_lastUpdateTime;
    /* 00A4 */ uint32_t m_state;
    /* 00A8 */ uint32_t m_transitionStartTime;
    /* 00AC */ uint32_t m_transitionDuration;
    /* 00B0 */ WOWGUID m_targetGuid;
    /* 00B8 */ WOWGUID m_nextCameraGuid;
    /* 00C0 */ WOWGUID m_relativeToGuid;

    CVehicleCamera_C();
    uint64_t GetRelativeTo() const;
    int32_t WouldAddingNextCameraCreateLoop(const CVehicleCamera_C* next) const;
    void Destroy();
    int32_t IsHierarchyChasingFacing() const;
    int32_t IsTransitioningFacing() const;
    int32_t UpdateTransitionFlags();
    float GetTransitionProgress(uint32_t time) const;
    int32_t AttachToHierarchy();
    void DetachFromHierarchy();
    void BeginTransition(uint32_t time);
    void ComputeSafeCurWorldPos();
    void Update(uint32_t time);
    C3Vector ComputeEndWorldPosition(CGUnit_C* target);
    void UpdatePosition(CGUnit_C* target, uint32_t time, float progress);
    void UpdateFacing(CGUnit_C* target, uint32_t time, float progress, CGObject_C* relativeTo);
    void UpdateWorldCameraState(uint32_t time, uint32_t duration);
    void SetTarget(WOWGUID target, WOWGUID next);
    void MaybeChangeTarget(CGUnit_C* vehicle, VehicleSeatRec* seat, int32_t exiting, WOWGUID& relativeTo);
    void OnPassengerTransitionMotionBegin(uint32_t time, uint32_t duration);
};

#endif
