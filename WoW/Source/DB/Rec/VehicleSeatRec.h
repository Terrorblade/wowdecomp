#ifndef WOW_SOURCE_DB_REC_VEHICLESEATREC_H
#define WOW_SOURCE_DB_REC_VEHICLESEATREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum VEHICLE_SEAT_FLAGS : int32_t {
    VEHICLE_SEAT_FLAG_NONE                                         = 0x00000000,
    VEHICLE_SEAT_FLAG_HAS_LOWER_ANIM_FOR_ENTER                     = 0x00000001,
    VEHICLE_SEAT_FLAG_HAS_LOWER_ANIM_FOR_RIDE                      = 0x00000002,
    VEHICLE_SEAT_FLAG_HAS_UPPER_ANIM_FOR_RIDE                      = 0x00000004,
    VEHICLE_SEAT_FLAG_USE_EXIT_ANIM_ON_VOLUNTARY_EXIT              = 0x00000008,
    VEHICLE_SEAT_FLAG_UNKNOWN_4                                    = 0x00000010,
    VEHICLE_SEAT_FLAG_ENTER_EASE_IN                                = 0x00000020,
    VEHICLE_SEAT_FLAG_ENTER_EASE_OUT                               = 0x00000040,
    VEHICLE_SEAT_FLAG_EXIT_EASE_IN                                 = 0x00000080,
    VEHICLE_SEAT_FLAG_EXIT_EASE_OUT                                = 0x00000100,
    VEHICLE_SEAT_FLAG_HIDE_PASSENGER                               = 0x00000200,
    VEHICLE_SEAT_FLAG_ALLOW_TURNING                                = 0x00000400,
    VEHICLE_SEAT_FLAG_CAN_CONTROL                                  = 0x00000800,
    VEHICLE_SEAT_FLAG_CAN_CAST_MOUNT_SPELL                         = 0x00001000,
    VEHICLE_SEAT_FLAG_CAN_CAST_SHAPESHIFT_SPELL                    = 0x00002000,
    VEHICLE_SEAT_FLAG_CAN_ATTACK                                   = 0x00004000,
    VEHICLE_SEAT_FLAG_USE_EXIT_ANIM_ON_FORCED_EXIT                 = 0x00008000,
    VEHICLE_SEAT_FLAG_UNKNOWN_16                                   = 0x00010000,
    VEHICLE_SEAT_FLAG_HAS_VEHICLE_RIDE_ANIM                        = 0x00020000,
    VEHICLE_SEAT_FLAG_HAS_VEHICLE_EXIT_ANIM_ON_VOLUNTARY_EXIT      = 0x00040000,
    VEHICLE_SEAT_FLAG_HAS_VEHICLE_EXIT_ANIM_ON_FORCED_EXIT         = 0x00080000,
    VEHICLE_SEAT_FLAG_PASSENGER_NOT_SELECTABLE                     = 0x00100000,
    VEHICLE_SEAT_FLAG_FORCE_EXIT_END_ANIM                          = 0x00200000,
    VEHICLE_SEAT_FLAG_HAS_VEHICLE_ENTER_ANIM                       = 0x00400000,
    VEHICLE_SEAT_FLAG_IS_USING_VEHICLE_CONTROLS                    = 0x00800000,
    VEHICLE_SEAT_FLAG_ENABLE_VEHICLE_ZOOM                          = 0x01000000,
    VEHICLE_SEAT_FLAG_CAN_EXIT                                     = 0x02000000,
    VEHICLE_SEAT_FLAG_CAN_SWITCH                                   = 0x04000000,
    VEHICLE_SEAT_FLAG_TRANSITION_ANIM_ENTER_TIMER                  = 0x08000000,
    VEHICLE_SEAT_FLAG_TRANSITION_ANIM_EXIT_TIMER                   = 0x10000000,
    VEHICLE_SEAT_FLAG_HAS_VEHICLE_UI                               = 0x20000000,
    VEHICLE_SEAT_FLAG_UNKNOWN_30                                   = 0x40000000,
    VEHICLE_SEAT_FLAG_ALLOWS_INTERACTION                           = (-2147483647 - 1),
    VEHICLE_SEAT_FLAG_CONTROLLED_SUMMON_RESTRICTION_MASK = VEHICLE_SEAT_FLAG_CAN_CONTROL
        | VEHICLE_SEAT_FLAG_UNKNOWN_30,
};

enum VEHICLE_SEAT_FLAGS_B : int32_t {
    VEHICLE_SEAT_FLAG_B_NONE                                       = 0x00000000,
    VEHICLE_SEAT_FLAG_B_CAMERA_INTERPOLATE_POSITION                = 0x00000001,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_1                                  = 0x00000002,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_2                                  = 0x00000004,
    VEHICLE_SEAT_FLAG_B_TARGETS_IN_RAID_UI                         = 0x00000008,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_4                                  = 0x00000010,
    VEHICLE_SEAT_FLAG_B_EJECTABLE                                  = 0x00000020,
    VEHICLE_SEAT_FLAG_B_CAMERA_ENTER_TIMING                        = 0x00000040,
    VEHICLE_SEAT_FLAG_B_CAMERA_EXIT_TIMING                         = 0x00000080,
    VEHICLE_SEAT_FLAG_B_CAMERA_USE_MODEL_POSITION                  = 0x00000100,
    VEHICLE_SEAT_FLAG_B_CAMERA_USE_VEHICLE_POSITION                = 0x00000200,
    VEHICLE_SEAT_FLAG_B_CAMERA_USE_SEAT_ZOOM                       = 0x00000400,
    VEHICLE_SEAT_FLAG_B_CAMERA_HAS_ENTER_ZOOM                      = 0x00000800,
    VEHICLE_SEAT_FLAG_B_CAMERA_SKIP_ZOOM_OUT                       = 0x00001000,
    VEHICLE_SEAT_FLAG_B_CAMERA_SKIP_ZOOM_IN                        = 0x00002000,
    VEHICLE_SEAT_FLAG_B_CAMERA_HAS_ZOOM_MIN                        = 0x00004000,
    VEHICLE_SEAT_FLAG_B_CAMERA_HAS_ZOOM_MAX                        = 0x00008000,
    VEHICLE_SEAT_FLAG_B_MIRROR_VEHICLE_ANIMS                       = 0x00010000,
    VEHICLE_SEAT_FLAG_B_CAMERA_ENTER_CHASE                         = 0x00020000,
    VEHICLE_SEAT_FLAG_B_CAMERA_EXIT_CHASE                          = 0x00040000,
    VEHICLE_SEAT_FLAG_B_CAMERA_INTERPOLATE_FACING                  = 0x00080000,
    VEHICLE_SEAT_FLAG_B_EXIT_ON_CAST                               = 0x00100000,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_21                                 = 0x00200000,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_22                                 = 0x00400000,
    VEHICLE_SEAT_FLAG_B_FORCE_ENTER_ANIM_FACING                    = 0x00800000,
    VEHICLE_SEAT_FLAG_B_ALLOW_PASSENGER_TRANSITION                 = 0x01000000,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_25                                 = 0x02000000,
    VEHICLE_SEAT_FLAG_B_UNUSED_26                                  = 0x04000000,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_27                                 = 0x08000000,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_28                                 = 0x10000000,
    VEHICLE_SEAT_FLAG_B_UNUSED_29                                  = 0x20000000,
    VEHICLE_SEAT_FLAG_B_UNKNOWN_30                                 = 0x40000000,
    VEHICLE_SEAT_FLAG_B_UNUSED_31                                  = (-2147483647 - 1),
    VEHICLE_SEAT_FLAG_B_CAMERA_FACING_INTERPOLATION_MASK = VEHICLE_SEAT_FLAG_B_CAMERA_INTERPOLATE_POSITION
        | VEHICLE_SEAT_FLAG_B_CAMERA_INTERPOLATE_FACING,
};

class VehicleSeatRec {
    public:
    static const int32_t s_numColumns = 58;
    static const int32_t s_rowSize = 232;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_flags;
    /* 0008 */ int32_t m_attachmentID;
    /* 000C */ float m_attachmentOffset [3];
    /* 0018 */ float m_field33512213004;
    /* 001C */ float m_enterSpeed;
    /* 0020 */ float m_enterGravity;
    /* 0024 */ float m_enterMinDuration;
    /* 0028 */ float m_enterMaxDuration;
    /* 002C */ float m_enterMinArcHeight;
    /* 0030 */ float m_enterMaxArcHeight;
    /* 0034 */ int32_t m_enterAnimStart;
    /* 0038 */ int32_t m_enterAnimLoop;
    /* 003C */ int32_t m_rideAnimStart;
    /* 0040 */ int32_t m_rideAnimLoop;
    /* 0044 */ int32_t m_rideUpperAnimStart;
    /* 0048 */ int32_t m_rideUpperAnimLoop;
    /* 004C */ float m_field33512213017;
    /* 0050 */ float m_exitSpeed;
    /* 0054 */ float m_exitGravity;
    /* 0058 */ float m_exitMinDuration;
    /* 005C */ float m_exitMaxDuration;
    /* 0060 */ float m_exitMinArcHeight;
    /* 0064 */ float m_exitMaxArcHeight;
    /* 0068 */ int32_t m_exitAnimStart;
    /* 006C */ int32_t m_exitAnimLoop;
    /* 0070 */ int32_t m_exitAnimEnd;
    /* 0074 */ float m_field33512213027;
    /* 0078 */ float m_passengerPitch;
    /* 007C */ float m_field33512213029;
    /* 0080 */ int32_t m_passengerAttachmentID;
    /* 0084 */ int32_t m_vehicleEnterAnim;
    /* 0088 */ int32_t m_vehicleExitAnim;
    /* 008C */ int32_t m_vehicleRideAnimLoop;
    /* 0090 */ int32_t m_field33512213034;
    /* 0094 */ int32_t m_vehicleExitAnimBone;
    /* 0098 */ int32_t m_vehicleEnterAnimBone;
    /* 009C */ float m_field33512213037;
    /* 00A0 */ float m_field33512213038;
    /* 00A4 */ int32_t m_vehicleAbilityDisplay;
    /* 00A8 */ int32_t m_enterUisoundID;
    /* 00AC */ int32_t m_field33512213041;
    /* 00B0 */ int32_t m_uiSkin;
    /* 00B4 */ int32_t m_flagsB;
    /* 00B8 */ float m_field33512213044;
    /* 00BC */ float m_field33512213045;
    /* 00C0 */ float m_field33512213046;
    /* 00C4 */ float m_field33512213047;
    /* 00C8 */ float m_field33512213048;
    /* 00CC */ float m_field33512213049;
    /* 00D0 */ float m_field33512213050;
    /* 00D4 */ float m_field33512213051;
    /* 00D8 */ float m_field33512213052;
    /* 00DC */ float m_field33512213053;
    /* 00E0 */ float m_field33512213054;
    /* 00E4 */ float m_field33512213055;

    VehicleSeatRec();
    ~VehicleSeatRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
