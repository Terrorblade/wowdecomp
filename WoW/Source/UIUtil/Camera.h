#ifndef WOW_SOURCE_UIUTIL_CAMERA_H
#define WOW_SOURCE_UIUTIL_CAMERA_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C33Matrix.h"
#include "Engine/Source/Tempest/C34Matrix.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Base/Handle.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/UIUtil/CSimpleCamera.h"
#include "WoW/Source/UIUtil/Types.h"
#include "Engine/Source/Services/CameraInt.h"
#include "Engine/Source/Tempest/CRange.h"

class CGObject_C;
class CGUnit_C;
class CM2Model;
class CRect;
class CVehicleCamera_C;
struct CVar;

struct CameraView {
    float m_distance;
    float m_pitch;
    float m_yaw;

    // Component order follows the distance/pitch/yaw CVar columns.
    float& Component(uint32_t component) {
        switch (component) {
            case 0: return m_distance;
            case 1: return m_pitch;
            default: return m_yaw;
        }
    }

    const float& Component(uint32_t component) const {
        switch (component) {
            case 0: return m_distance;
            case 1: return m_pitch;
            default: return m_yaw;
        }
    }
};

class CAMERA_DEFAULTS {
    public:
    /* 0000 */ const char* m_values [3];
};

class CAMERA_SMOOTH {
    public:
    /* 0000 */ uint32_t m_startTimeMs;
    /* 0004 */ float m_rate;
    /* 0008 */ float m_target;
    /* 000C */ float m_startValue;
    /* 0010 */ float m_param3;
    /* 0014 */ float m_param4;
};

struct CameraShake : public TSLinkedNode<CameraShake> {
    /* 0008 */ CGCameraShakeType m_type;
    /* 000C */ CGCameraDir m_dir;
    /* 0010 */ float m_amplitude;
    /* 0014 */ float m_frequency;
    /* 0018 */ float m_duration;
    /* 001C */ float m_timeOffset;
    /* 0020 */ float m_damping;
    /* 0024 */ C3Vector m_position;
    /* 0030 */ uint32_t m_startTime;
};

class CGInputControl;

class CGCamera : public CSimpleCamera {
    public:
    enum VIEW_MODE : int32_t {
        VIEW_FIRST_PERSON = 0,
        VIEW_THIRD_PERSON_A = 1,
        VIEW_THIRD_PERSON_B = 2,
        VIEW_THIRD_PERSON_C = 3,
        VIEW_THIRD_PERSON_D = 4,
        VIEW_THIRD_PERSON_E = 5,
        VIEW_COMMENTATOR = 6,
        VIEW_BARBER_SHOP = 7,
        MAX_CAMERA_VIEWS,
    };

    enum VIEW_BLEND_STYLE : int32_t {
        VIEW_BLEND_NONE = 0,
        VIEW_BLEND_SMOOTH = 1,
        VIEW_BLEND_INSTANT = 2,
    };

    enum VIEW_TRANSITION : int32_t {
        VIEW_TRANSITION_NONE = 0,
        VIEW_TRANSITION_TELESCOPE_ENTER = 1,
        VIEW_TRANSITION_TELESCOPE_LEAVE = 2,
    };

    enum VEHICLE_TRACKING_STYLE : uint32_t {
        TRACKING_USER_STYLE = 0,
        TRACKING_IGNORE_FACING = 1,
        TRACKING_FOLLOW_FACING = 2,
    };

    enum MOTION_FLAGS : int32_t {
        FLAG_MOTION_NONE = 0,
        FLAG_MOVE_IN_ACTIVE = 0x1,
        FLAG_MOVE_IN_STOP_REQUESTED = 0x2,
        FLAG_MOVE_OUT_ACTIVE = 0x4,
        FLAG_MOVE_OUT_STOP_REQUESTED = 0x8,
        FLAG_MOVE_RIGHT_ACTIVE = 0x10,
        FLAG_MOVE_RIGHT_STOP_REQUESTED = 0x20,
        FLAG_MOVE_LEFT_ACTIVE = 0x40,
        FLAG_MOVE_LEFT_STOP_REQUESTED = 0x80,
        FLAG_MOVE_UP_ACTIVE = 0x100,
        FLAG_MOVE_UP_STOP_REQUESTED = 0x200,
        FLAG_MOVE_DOWN_ACTIVE = 0x400,
        FLAG_MOVE_DOWN_STOP_REQUESTED = 0x800,
        FLAG_MOVE_IN_MASK = 0x3,
        FLAG_MOVE_OUT_MASK = 0xC,
    };

    enum STATE_FLAGS : uint32_t {
        STATE_NONE                           = 0x00000000u,
        STATE_FREE_LOOK                      = 0x00000001u,
        STATE_UNDERWATER_INITIALIZED         = 0x00000002u,
        STATE_LOADED                         = 0x00000004u,
        STATE_TELESCOPE                      = 0x00000008u,
        STATE_INITIALIZED                    = 0x00000010u,
        STATE_FREE_LOOK_DISABLED             = 0x00000020u,
        STATE_VIEW_MODIFIED                  = 0x00000040u,
        STATE_HEIGHT_INITIALIZED             = 0x00000080u,
        STATE_TRACKING                       = 0x00000100u,
        STATE_BOBBING                        = 0x00000200u,
        STATE_UNCONTROLLED                   = 0x00001000u,
        STATE_TRACKING_IGNORES_FACING        = 0x00002000u,
        STATE_FREE_LOOK_FACING               = 0x00004000u,
        STATE_TELESCOPE_ENTERING             = 0x00008000u,
        STATE_DISTANCE_COLLISION             = 0x00010000u,
        STATE_HEIGHT_COLLISION               = 0x00020000u,
        STATE_LIQUID_SURFACED                = 0x00100000u,
        STATE_LIQUID_SUBMERGED               = 0x00200000u,
        STATE_SMOOTH_FLYING_HEIGHT           = 0x00400000u,
        STATE_MOUNT_HEIGHT_INITIALIZED       = 0x00800000u,
        STATE_SMOOTH_YAW                     = 0x01000000u,
        STATE_SMOOTH_PITCH                   = 0x02000000u,
        STATE_SMOOTH_DISTANCE                = 0x04000000u,
        STATE_SMOOTH_TARGET_OFFSET           = 0x08000000u,
        STATE_SMOOTH_GROUND_TILT             = 0x10000000u,
        STATE_SMOOTH_HEIGHT                  = 0x20000000u,
        STATE_SMOOTH_FOV                     = 0x40000000u,
        STATE_TELESCOPE_LEAVING              = 0x80000000u,
        STATE_COLLISION_MASK = STATE_DISTANCE_COLLISION | STATE_HEIGHT_COLLISION,
        STATE_LIQUID_MASK = STATE_LIQUID_SURFACED | STATE_LIQUID_SUBMERGED,
        STATE_TELESCOPE_TRANSITION_MASK = STATE_TELESCOPE_ENTERING | STATE_TELESCOPE_LEAVING,
    };

    enum FLAGS : uint32_t {
        FLAG_NONE                            = 0x00000000u,
        FLAG_AUTO_INTERACT                   = 0x00000001u,
        FLAG_TARGET_FREE_LOOK                = 0x00000002u,
        FLAG_PRESERVE_FREE_LOOK_FACING       = 0x00000004u,
        FLAG_VEHICLE_ZOOM_MIN                = 0x00000008u,
        FLAG_VEHICLE_ZOOM_MAX                = 0x00000010u,
        FLAG_VEHICLE_EXIT_ZOOM               = 0x00000020u,
        FLAG_ZOOM_TO_DISTANCE                = 0x00000040u,
        FLAG_MOUNT_CAMERA_POSITION           = 0x00000080u,
    };

    int32_t GetCameraDistance(float& distance, const C3Vector& position, const C3Vector& target, uint32_t flags);
    void ResetView(int32_t view);
    void SetView(int32_t view, int32_t blendStyle, int32_t transition);
    C3Vector BumpCameraPositionFromLiquid(const C3Vector& target, float distance, const C3Vector& position);
    int32_t CanSmoothTarget();
    int32_t CanSmoothYaw(float yawMin, float yawMax);
    uint32_t CollideCameraWithWorld(const C3Vector& target, float& distance, float& height, const C3Vector& offset, float liquid, float& extent);
    void RunBobs(C3Vector& offset);
    void RunShakes(const C3Vector& position, C3Vector& offset);
    void UpdateMotion(uint32_t time);
    void UpdateTargetSmoothing(CGObject_C* target, uint32_t time);
    void UpdateUncontrolledState(int32_t uncontrolled);
    void UpdateVehiclePitchOffset(uint32_t time);
    void UpdateVehicleYawOffset(uint32_t time);
    void CalcModelCamera(uint32_t time);
    void CalcTargetCamera(CGObject_C* target, uint32_t time);
    void FaceTarget(CGObject_C* target, uint32_t time);
    void SyncFreeLookFacing();
    void UpdateFreeLookFacing(float x, float y, float* a3);
    void SetTelescope(int32_t telescope);
    static int32_t UpdateCallback(const void* data, void* param);
    static int32_t CCommand_PitchLimit(const char* command, const char* arguments);
    static uint32_t GetCollisionQueryFlags();
    static float GetDefaultZoomMaxDistance();
    float GetDefaultZoomMinDistance();
    static void __stdcall AddShakeOffset(C3Vector& offset, float facing, float amplitude, float time, float frequency, CGCameraShakeType type, float damping, CGCameraDir dir);
    void SetPositionAndTargetWithRoll(const C3Vector& position, const C3Vector& target, float roll);
    void CommentatorSetViewAndPosition(const C3Vector& position, float facing, float pitch);
    void BarberShopSetView(float distance, float facing, float pitch);
    int32_t FinishLoadingModel();

    /* 0048 */ CM2Model* m_model;
    /* 004C */ uint32_t m_someTime;
    /* 0050 */ HOBJECT m_modelCameraHandle;
    /* 0054 */ C34Matrix m_modelMatrix;
    /* 0084 */ uint32_t unused_0084;
    /* 0088 */ WOWGUID m_targetGUID;
    /* 0090 */ WOWGUID m_guid0090;
    /* 0098 */ uint32_t m_state;
    /* 009C */ uint32_t m_flags;
    /* 00A0 */ WOWGUID m_relativeTo;
    /* 00A8 */ float m_terrainTilt;
    /* 00AC */ int32_t m_ignoreFacingRefs;
    /* 00B0 */ int32_t m_groundTiltRefs;
    /* 00B4 */ uint32_t m_viewIndex;
    /* 00B8 */ CameraView m_views [MAX_CAMERA_VIEWS];
    /* 0118 */ float m_distance;
    /* 011C */ float m_yaw;
    /* 0120 */ float m_pitch;
    /* 0124 */ float m_roll;
    /* 0128 */ float m_height;
    /* 012C */ float m_yawOffset;
    /* 0130 */ float m_targetOffset;
    /* 0134 */ float m_groundTilt;
    /* 0138 */ float m_targetFov;
    /* 013C */ float m_flyingMountHeight;
    /* 0140 */ C3Vector m_bobOffset;
    /* 014C */ float m_targetHeightNear;
    /* 0150 */ float m_targetHeightFar;
    /* 0154 */ float m_targetHeightSwim;
    /* 0158 */ float m_mountHeight;
    /* 015C */ uint32_t m_heightChangedTimeMs;
    /* 0160 */ uint32_t m_motionFlags;
    /* 0164 */ uint32_t m_motionStartTime [NUM_CAMERA_MOTIONS];
    /* 017C */ uint32_t m_motionStopTime [NUM_CAMERA_MOTIONS];
    /* 0194 */ uint32_t m_motionEndTime [NUM_CAMERA_MOTIONS];
    /* 01AC */ float m_motionSpeed [NUM_CAMERA_MOTIONS];
    /* 01C4 */ C3Vector m_targetPosition;
    /* 01D0 */ float unused_01D0;
    /* 01D4 */ float m_targetFacing;
    /* 01D8 */ uint32_t m_terrainTiltTime;
    /* 01DC */ float m_freeLookYaw;
    /* 01E0 */ CAMERA_SMOOTH m_smoothDistance;
    /* 01F8 */ CAMERA_SMOOTH m_smoothGroundTilt;
    /* 0210 */ CAMERA_SMOOTH m_smoothHeight;
    /* 0228 */ CAMERA_SMOOTH m_smoothPitch;
    /* 0240 */ CAMERA_SMOOTH m_smoothTargetOffset;
    /* 0258 */ CAMERA_SMOOTH m_smoothYaw;
    /* 0270 */ CAMERA_SMOOTH m_smoothFoV;
    /* 0288 */ uint32_t m_bobTime;
    /* 028C */ float m_bobDuration;
    /* 0290 */ C3Vector m_bobStart;
    /* 029C */ uint32_t unused_029C;
    /* 02A0 */ int32_t m_underwater;
    /* 02A4 */ uint32_t m_wasFlying;
    /* 02A8 */ CAMERA_SMOOTH m_smoothFlyingHeight;
    /* 02C0 */ float m_collideExtent;
    /* 02C4 */ uint32_t m_vehicleZoomEnabled;
    /* 02C8 */ float m_overrideDistanceMin;
    /* 02CC */ float m_overrideDistanceMax;
    /* 02D0 */ float m_vehicleExitZoomDistance;
    /* 02D4 */ uint32_t m_vehicleYawStartTime;
    /* 02D8 */ uint32_t m_vehicleYawEndTime;
    /* 02DC */ float m_vehicleYawFrom;
    /* 02E0 */ float m_vehicleYawTo;
    /* 02E4 */ float m_vehicleYawOffset;
    /* 02E8 */ uint32_t m_vehiclePitchStartTime;
    /* 02EC */ uint32_t m_vehiclePitchEndTime;
    /* 02F0 */ float m_vehiclePitchFrom;
    /* 02F4 */ float m_vehiclePitchTo;
    /* 02F8 */ float m_vehiclePitchOffset;
    /* 02FC */ uint32_t m_vehicleTrackingStyle;
    /* 0300 */ TSList<CameraShake> m_cameraShakeList;
    /* 030C */ uint32_t m_prevViewIndex;
    /* 0310 */ float m_prevDistance;
    /* 0314 */ float m_preTelescopeDistance;
    /* 0318 */ float m_telescopePitchOffset;
    /* 031C */ CVehicleCamera_C* m_vehicleCamera;

    float FOV() const override;
    C3Vector Forward() const override;
    C3Vector Right() const override;
    C3Vector Up() const override;

    CGCamera();
    ~CGCamera();
    void AddShake(const C3Vector& position, int32_t type, CGCameraDir dir, float amplitude, float frequency, float duration, float timeOffset, float damping);
    void AddShakeById(int32_t shakeId, const C3Vector* position);
    void BeginBobbingSmooth();
    void CalcTerrainTilt(CGObject_C* target, uint32_t time);
    void CameraTeleported(float facing);
    uint32_t CanSmoothTargetFacing(CGObject_C* target);
    void CancelSmoothPitch();
    void CancelSmoothTargetOffset();
    void CancelSmoothYaw();
    void CheckUnderwater();
    void ClampPitchAndNormalize();
    void ClampPitchToLimits(float delta);
    void ClearModelCamera();
    int32_t SetModelCamera(const char* fileName, const C3Vector* position, float facing, void (*callback)(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param), uint64_t param);
    void ClearVehicleExitZoomDistance();
    void CreateViewFromCamera(uint32_t view);
    void DecIgnoreFacing();
    void DisableFreeLook(int32_t keepSmoothing);
    void EnableFreeLook();
    int32_t FinishLoadingTarget(CGObject_C* target);
    C3Vector GetCameraPosition(const C3Vector& target, float distance, const C3Vector& offset);
    float GetChaseFacing(CGUnit_C* unit) const;
    C3Vector GetSafeWorldPos(CGObject_C* target) const;
    int32_t HasModelCamera();
    int32_t HasTargetOffset();
    void IncIgnoreFacing();
    void InitializeView();
    void RestoreSavedDistancePitch();
    uint32_t IsBobbingAllowed(CGObject_C* target);
    int32_t IsCustomViewSmoothingActive();
    int32_t IsFirstPersonView();
    int32_t IsViewDefault(uint32_t view);
    void LoadCameraSettings();
    void MakeRelativeTo(WOWGUID guid);
    C33Matrix ParentToWorld() const;
    void PerformTerrainTilt(CGObject_C* target, uint32_t time, int32_t immediate);
    void PickVehicleCamera();
    void OnVehicleCameraDestroyed();
    void ResetModelCamera();
    void RotateOffsetByRoll(C3Vector& offset);
    void SaveCameraSettings();
    void SaveViewToCVars(uint32_t view, float distance, float pitch, float yaw);
    int32_t SetDesiredFoVOffset(float offset, float delay, float scale, uint32_t time);
    int32_t SetDesiredGroundOffset(float offset, float delay, float scale, uint32_t time);
    int32_t SetDesiredPitchAngle(float pitch, float delay, float scale, uint32_t time);
    int32_t SetDesiredTargetOffset(float offset, float delay, float scale, uint32_t time);
    void SetDesiredVehiclePitchOffset(float offset, uint32_t startTime, uint32_t endTime);
    void SetDesiredVehicleYawOffset(float offset, uint32_t startTime, uint32_t endTime);
    int32_t SetDesiredYawAngle(float yaw, float delay, float scale, uint32_t time);
    void SetModeFreeLook();
    void SetModeNormal();
    void SetTarget(CGObject_C* target, int32_t freeLook);
    void SetupWorldProjection(const CRect& rect);
    void SetVehicleExitZoomDistance(float distance);
    void SetVehicleSeatZoomMaxDistance(float distance);
    void SetVehicleSeatZoomMaxDistanceToDefault();
    void SetVehicleSeatZoomMinDistance(float distance);
    void SetVehicleSeatZoomMinDistanceToDefault();
    void SetVehicleTrackingStyle(uint32_t style);
    void SetVehicleZoomEnabled(int32_t enabled, float duration, int32_t animate);
    int32_t ShouldSmoothPitch(float minPitch, float maxPitch);
    int32_t SmoothSetDistance(float distance, float delay, float scale, uint32_t time);
    int32_t SmoothSetFlyingMountHeight(float height, float delay, float scale, uint32_t time);
    int32_t SmoothSetHeight(float height, float delay, float scale, uint32_t time);
    float SmoothTargetFacing(float facing, int32_t moved);
    void StartMotion(CGCameraMotion motion, uint32_t time, uint32_t duration, float speed);
    int32_t StartSmoothDistance(float distance, float rate, uint32_t startTime);
    int32_t StartSmoothFlyingMountHeight(float height, float rate, uint32_t startTime);
    int32_t StartSmoothFoV(float offset, float rate, uint32_t startTime);
    int32_t StartSmoothGroundTilt(float offset, float rate, uint32_t startTime);
    int32_t StartSmoothHeight(float height, float rate, uint32_t startTime);
    int32_t StartSmoothPitch(float pitch, float rate, uint32_t startTime);
    int32_t StartSmoothTargetOffset(float offset, float rate, uint32_t startTime);
    int32_t StartSmoothYaw(float yaw, float rate, uint32_t startTime);
    void UpdateAutoInteractState(int32_t enabled);
    void UpdateBobbingState(int32_t moving);
    void SmoothFreeLook(CGInputControl* input, int32_t idle);
    void UpdateTrackingState(int32_t tracking);
    int32_t UpdateInebriation(float drunkness, int32_t immediate);
    float UpdateLiquidSurfaceStatus(CGObject_C* target);
    void UpdateMountHeightOrOffset(CGUnit_C* unit);
    void UpdateTargetFacing(CGObject_C* target, float& yaw, float& pitch, float& roll);
    void UpdateTargetHeight(CGObject_C* target, uint32_t time);
    void UpdateVehicleTarget(uint32_t time);
    void UpdateYaw(float delta);
    void ZoomIn(float distance, uint32_t time, float duration);
    void ZoomOut(float distance, uint32_t time, float duration);
    void ZoomToDistance(float distance, uint32_t time, float duration, int32_t lock);
};

extern float MAX_PITCH_ANGLE;
extern float MIN_PITCH_ANGLE;
extern int32_t s_cameraViewAngular[3];
extern CAMERA_DEFAULTS s_cameraViewDefaults[CGCamera::MAX_CAMERA_VIEWS];
extern const float s_terrainTiltTable[10][2];
extern float s_targetHeightOffset;
extern float s_cameraBobFrequency;
extern float s_cameraBobVerticalAmplitude;
extern float s_cameraBobHorizontalAmplitude;
constexpr float MIN_CAMERA_DIST = 0.0f;
constexpr float MAX_CAMERA_DIST = 50.0f;

enum CAMERA_SMOOTH_STYLE : int32_t {
    CAMERA_SMOOTH_STYLE_NEVER = 0,
    CAMERA_SMOOTH_STYLE_SMART = 1,
    CAMERA_SMOOTH_STYLE_ALWAYS = 2,
    CAMERA_SMOOTH_STYLE_SPLINE = 3,
    CAMERA_SMOOTH_STYLE_SMARTER = 4,
    NUM_CAMERA_SMOOTH_STYLES,
};

enum CAMERA_SMOOTH_STATE : int32_t {
    CAMERA_SMOOTH_STATE_IDLE = 0,
    CAMERA_SMOOTH_STATE_STOP = 1,
    CAMERA_SMOOTH_STATE_TRACK = 2,
    CAMERA_SMOOTH_STATE_MOVE = 3,
    CAMERA_SMOOTH_STATE_STRAFE = 4,
    CAMERA_SMOOTH_STATE_TURN = 5,
    CAMERA_SMOOTH_STATE_FEAR = 6,
    NUM_CAMERA_SMOOTH_STATES,
};
#define CAMERA_SMOOTH_STATE_BIT(state) (1u << (state))
#define CAMERA_SMOOTH_STATE_ALL_BITS (CAMERA_SMOOTH_STATE_BIT(NUM_CAMERA_SMOOTH_STATES) - 1)

enum CAMERA_SMOOTH_PARAM : int32_t {
    CAMERA_SMOOTH_PARAM_DELAY = 0,
    CAMERA_SMOOTH_PARAM_FACTOR = 1,
    NUM_CAMERA_SMOOTH_PARAMS,
};

enum CAMERA_VIEW_DATA : int32_t {
    CAMERA_VIEW_DATA_DISTANCE = 0,
    CAMERA_VIEW_DATA_PITCH = 1,
    CAMERA_VIEW_DATA_YAW = 2,
    NUM_CAMERA_VIEW_DATA,
};

enum CAMERA_TERRAIN_TILT_STATE : int32_t {
    CAMERA_TERRAIN_TILT_STATE_FALL = 0,
    CAMERA_TERRAIN_TILT_STATE_FEAR = 1,
    CAMERA_TERRAIN_TILT_STATE_IDLE = 2,
    CAMERA_TERRAIN_TILT_STATE_JUMP = 3,
    CAMERA_TERRAIN_TILT_STATE_MOVE = 4,
    CAMERA_TERRAIN_TILT_STATE_STRAFE = 5,
    CAMERA_TERRAIN_TILT_STATE_SWIM = 6,
    CAMERA_TERRAIN_TILT_STATE_TAXI = 7,
    CAMERA_TERRAIN_TILT_STATE_TRACK = 8,
    CAMERA_TERRAIN_TILT_STATE_TURN = 9,
    NUM_CAMERA_TERRAIN_TILT_STATES,
};

enum CAMERA_TERRAIN_TILT_PARAM : int32_t {
    CAMERA_TERRAIN_TILT_PARAM_ABSORB = 0,
    CAMERA_TERRAIN_TILT_PARAM_DELAY = 1,
    CAMERA_TERRAIN_TILT_PARAM_FACTOR = 2,
    NUM_CAMERA_TERRAIN_TILT_PARAMS,
};

extern CVar* s_cvCameraView;
extern CVar* s_cvCameraViewData[CGCamera::MAX_CAMERA_VIEWS][NUM_CAMERA_VIEW_DATA];
extern CVar* s_cameraSmooth;
extern CVar* s_cvCameraSmoothStyle;
extern CVar* s_cvCameraTerrainTilt;
extern CVar* s_cvCameraTerrainTiltState[NUM_CAMERA_SMOOTH_STYLES][NUM_CAMERA_TERRAIN_TILT_STATES][NUM_CAMERA_TERRAIN_TILT_PARAMS];
extern CVar* s_cvCameraTerrainTiltTimeMin;
extern CVar* s_cvCameraTerrainTiltTimeMax;
extern CVar* s_cvCameraGroundSmoothSpeed;
extern CVar* s_cvCameraFoVSmoothSpeed;
extern CVar* s_cvCameraHeightSmoothSpeed;
extern CVar* s_cvCameraFlyingMountHeightSmoothSpeed;
extern CVar* s_cvCameraHeightIgnoreStandState;
extern CVar* s_cvCameraSavedDistance;
extern CVar* s_cvCameraSavedVehicleDistance;
extern CVar* s_cvCameraSavedPitch;
extern CVar* s_cvCameraPivot;
extern CVar* s_cvCameraSmoothPitch;
extern CVar* s_cvCameraBobbing;
extern CVar* s_cvCameraBobbingSmoothSpeed;
extern CVar* s_cvCameraCustomViewSmoothing;
extern CVar* s_cvCameraDistanceMoveSpeed;
extern CVar* s_cvCameraDistanceSmoothSpeed;
extern CVar* s_cvCameraPitchSmoothSpeed;
extern CVar* s_cvCameraTargetSmoothSpeed;
extern CVar* s_cvCameraYawSmoothSpeed;
extern CVar* s_cameraWaterCollision;
extern CVar* s_cvCameraSmoothTimeMax;
extern CVar* s_cvCameraSmoothTimeMin;
extern CVar* s_cvCameraYawSmoothMax;
extern CVar* s_cvCameraYawSmoothMin;
extern CVar* s_cvCameraPitchSmoothMax;
extern CVar* s_cvCameraPitchSmoothMin;
extern CVar* s_cvCameraDistanceMaxFactor;
extern CVar* s_cvCameraDistanceMax;
extern CVar* s_cvCameraSubmergeFinalPitch;
extern CVar* s_cvCameraSurfaceFinalPitch;
extern CVar* s_cvCameraSubmergePitch;
extern CVar* s_cvCameraSurfacePitch;
extern CVar* s_cvCameraDive;
extern CVar* s_cvCameraPivotDYMin;
extern CVar* s_cvCameraPivotDXMax;
extern CVar* s_cvCameraSmoothViewData[NUM_CAMERA_SMOOTH_STYLES][NUM_CAMERA_VIEW_DATA][NUM_CAMERA_SMOOTH_PARAMS];
extern CVar* s_cvCameraSmoothState[NUM_CAMERA_SMOOTH_STYLES][NUM_CAMERA_SMOOTH_STATES][NUM_CAMERA_SMOOTH_PARAMS];
extern CVar* s_cvCameraSmoothYaw;
extern CVar* s_cvCameraSmoothTrackingStyle;
extern CVar* s_cvCameraViewBlendStyle;
extern CVar* s_cvCameraYawMoveSpeed;
extern CVar* s_cvCameraPitchMoveSpeed;
extern CVar* s_cvMouseInvertPitch;
extern CVar* s_cvMouseInvertYaw;

void CameraRegisterCVars();

void CameraRegisterScriptFunctions();
void CameraUnregisterScriptFunctions();

float CameraEaseInOut(float t);

#endif
