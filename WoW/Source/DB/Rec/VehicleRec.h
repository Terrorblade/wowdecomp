#ifndef WOW_SOURCE_DB_REC_VEHICLEREC_H
#define WOW_SOURCE_DB_REC_VEHICLEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_VEHICLE_POWER_DISPLAYS = 3;

enum VEHICLE_UI_LOCOMOTION_TYPE : int32_t {
    VEHICLE_UI_LOCOMOTION_DRIVE = 0,
    VEHICLE_UI_LOCOMOTION_FLY = 1,
    VEHICLE_UI_LOCOMOTION_IDLE = 2,
    VEHICLE_UI_LOCOMOTION_AIRSHIP_HORDE = 3,
    VEHICLE_UI_LOCOMOTION_AIRSHIP_ALLIANCE = 4,
    NUM_VEHICLE_UI_LOCOMOTION_TYPES = 5,
};

class VehicleRec {
    public:

    enum FLAGS : int32_t {
        USE_UNIT_PITCH = 0x10,
        USE_PITCH_LIMITS = 0x40,
        UNSTOPPABLE_GROUND = 0x80,
        UNSTOPPABLE_SWIMMING_OR_FLYING = 0x100,
        USE_PITCH_BONE = 0x200,
        ADJUSTABLE_AIM_ANGLE = 0x400,
        ADJUSTABLE_AIM_POWER = 0x800,
        FIXED_FACING = 0x1000,
        HIGH_PRIORITY_CASTING_ANIMATION = 0x10000,
        ALLOW_CAMERA_PITCH_INPUT = 0x40000,
        CUSTOM_CAMERA_FADE_DISTANCE = 0x80000,
        CUSTOM_POWER_DISPLAY = 0x100000,
        LIMIT_FACING = 0x200000,
        TRAJECTORY_ALWAYS = 0x800000,
        TRAJECTORY_MOUSE_DRAG = 0x1000000,
        TRAJECTORY_PITCH_INPUT = 0x2000000,
        TRAJECTORY_FREE_LOOK = 0x4000000,
        TRAJECTORY_CAST_MISSILE = 0x8000000,
        BATTLEFIELD_VEHICLE = 0x10000000,
        PREVENT_ATTACKS_WITH_PASSENGERS = 0x20000000,
        UNKNOWN_40000000 = 0x40000000,
        TRAJECTORY_MASK = 0xF800000,
        MOUSE_AIM_CAMERA_PITCH_MASK = 0x40040000,
    };

    static const int32_t s_numColumns = 40;
    static const int32_t s_rowSize = 160;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_flags;
    /* 0008 */ float m_turnSpeed;
    /* 000C */ float m_pitchSpeed;
    /* 0010 */ float m_pitchMin;
    /* 0014 */ float m_pitchMax;
    /* 0018 */ int32_t m_seatID [8];
    /* 0038 */ float m_mouseLookOffsetPitch;
    /* 003C */ float m_cameraFadeDistScalarMin;
    /* 0040 */ float m_cameraFadeDistScalarMax;
    /* 0044 */ float m_cameraPitchOffset;
    /* 0048 */ float m_facingLimitRight;
    /* 004C */ float m_facingLimitLeft;
    /* 0050 */ float m_msslTrgtTurnLingering;
    /* 0054 */ float m_msslTrgtPitchLingering;
    /* 0058 */ float m_msslTrgtMouseLingering;
    /* 005C */ float m_msslTrgtEndOpacity;
    /* 0060 */ float m_msslTrgtArcSpeed;
    /* 0064 */ float m_msslTrgtArcRepeat;
    /* 0068 */ float m_msslTrgtArcWidth;
    /* 006C */ float m_msslTrgtImpactRadius [2];
    /* 0074 */ const char* m_msslTrgtArcTexture;
    /* 0078 */ const char* m_msslTrgtImpactTexture;
    /* 007C */ const char* m_msslTrgtImpactModel [2];
    /* 0084 */ float m_cameraYawOffset;
    /* 0088 */ int32_t m_uiLocomotionType;
    /* 008C */ float m_msslTrgtImpactTexRadius;
    /* 0090 */ int32_t m_vehicleUiindicatorID;
    /* 0094 */ int32_t m_powerDisplayID [NUM_VEHICLE_POWER_DISPLAYS];

    VehicleRec();
    ~VehicleRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
