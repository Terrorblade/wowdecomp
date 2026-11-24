#ifndef ENGINE_SOURCE_MODEL2_TYPES_H
#define ENGINE_SOURCE_MODEL2_TYPES_H

#include <cstddef>
#include <cstdint>

enum M2_FLAGS : uint32_t {
    M2_NONE = 0,
    M2_INITIALIZED = 1,
    M2_DRAWABLE = 2,
    M2_FORCE_PROJECTED_TEXTURES = 0x4,
    M2_DRAW_ENABLED = 0x8,
    M2_UNKNOWN_10 = 0x10,
    M2_UNKNOWN_20 = 0x20,
    M2_DEPTH_SORT_ALLOWED = 0x40,
    M2_ATTACHED_DRAW_ENABLED = 0x80,
    M2_ATTACHMENTS_LOADED = 0x100,
    M2_ATTACHMENTS_DRAWABLE = 0x200,
    M2_CONSTRUCTOR_SET_MASK = 0x340,
    M2_HAS_ACTIVE_PARTICLES = 0x400,
    M2_UNKNOWN_800 = 0x800,
    M2_INITIALIZE_SET_MASK = 0x801,
    M2_STATIC_SINGLE_BONE_OPTIMIZED = 0x1000,
    M2_CULLED = 0x2000,
    M2_UNKNOWN_4000 = 0x4000,
    M2_WORLD_TRANSFORM_VALID = 0x8000,
    M2_PARTICLES_ENABLED = 0x10000,
    M2_DRAW_AND_PARTICLES_MASK = 0x10008,
    M2_ATTACHED_PARTICLES_ENABLED = 0x20000,
    M2_ATTACHED_DRAW_AND_PARTICLES_MASK = 0x20080,
    M2_ATTACHED_TO_ROOT = 0x40000,
    M2_IGNORE_PARENT_EMISSIVE = 0x80000,
    M2_IGNORE_PARENT_EFFECTIVE_ALPHA = 0x100000,
    M2_UNKNOWN_400000 = 0x400000,
    M2_CONSTRUCTOR_KEEP_MASK = 0xFF800000,
    M2_INITIALIZE_KEEP_MASK = 0xFFFFF7FC,
};

enum M2_BITS : int32_t {
    M2_BIT_FORCE_PROJECTED_TEXTURES = 2,
    M2_BIT_DRAW_ENABLED = 3,
    M2_BIT_UNKNOWN_10 = 4,
    M2_BIT_ATTACHED_DRAW_ENABLED = 7,
    M2_BIT_HAS_ACTIVE_PARTICLES = 10,
    M2_BIT_CULLED = 13,
    M2_BIT_PARTICLES_ENABLED = 16,
    M2_BIT_ATTACHED_PARTICLES_ENABLED = 17,
    M2_BIT_ATTACHED_TO_ROOT = 18,
    M2_BIT_IGNORE_PARENT_EMISSIVE = 19,
};

enum M2_MODEL_CREATE_FLAGS : uint32_t {
    M2_MODEL_CREATE_FLAG_DIRECT_ACCESS = 0x4,
    M2_MODEL_CREATE_FLAG_UNCACHED = 0x8,
    M2_MODEL_CREATE_FLAG_HASH_FULL_PATH = 0x10,
    M2_MODEL_CREATE_FLAG_TEXTURE_PRIORITY_AND_DEFERRED_CANCEL = 0x40,
    M2_MODEL_CREATE_FLAG_ANY_EXTENSION = 0x1000,
};

constexpr uint32_t M2EventFourCC(const char (&id)[5]) {
    return static_cast<uint32_t>(static_cast<uint8_t>(id[0]))
        | (static_cast<uint32_t>(static_cast<uint8_t>(id[1])) << 8)
        | (static_cast<uint32_t>(static_cast<uint8_t>(id[2])) << 16)
        | (static_cast<uint32_t>(static_cast<uint8_t>(id[3])) << 24);
}

enum M2_EVENT_ID : uint32_t {
    WeaponTop_WTT                          = M2EventFourCC("$WTT"),
    BowMiddle_BMD                          = M2EventFourCC("$BMD"),
    WeaponBottom_WTB                       = M2EventFourCC("$WTB"),
    FishingPole_CCH                        = M2EventFourCC("$CCH"),
    ModelWeaponRightTop_WRT                = M2EventFourCC("$WRT"),
    ModelWeaponRightBot_WRB                = M2EventFourCC("$WRB"),
    ModelWeaponLeftTop_WLT                 = M2EventFourCC("$WLT"),
    ModelWeaponLeftBot_WLB                 = M2EventFourCC("$WLB"),
    FootLeft_FL0                           = M2EventFourCC("$FL0"),
    FootLeft_FL1                           = M2EventFourCC("$FL1"),
    FootLeft_FL2                           = M2EventFourCC("$FL2"),
    FootLeft_FL3                           = M2EventFourCC("$FL3"),
    FootRight_FR0                          = M2EventFourCC("$FR0"),
    FootRight_FR1                          = M2EventFourCC("$FR1"),
    FootRight_FR2                          = M2EventFourCC("$FR2"),
    FootRight_FR3                          = M2EventFourCC("$FR3"),
    FootLeft_BL0                           = M2EventFourCC("$BL0"),
    FootLeft_BL1                           = M2EventFourCC("$BL1"),
    FootLeft_BL2                           = M2EventFourCC("$BL2"),
    FootLeft_BL3                           = M2EventFourCC("$BL3"),
    FootRight_BR0                          = M2EventFourCC("$BR0"),
    FootRight_BR1                          = M2EventFourCC("$BR1"),
    FootRight_BR2                          = M2EventFourCC("$BR2"),
    FootRight_BR3                          = M2EventFourCC("$BR3"),
    FootLeft_RL0                           = M2EventFourCC("$RL0"),
    FootLeft_RL1                           = M2EventFourCC("$RL1"),
    FootLeft_RL2                           = M2EventFourCC("$RL2"),
    FootLeft_RL3                           = M2EventFourCC("$RL3"),
    FootRight_RR0                          = M2EventFourCC("$RR0"),
    FootRight_RR1                          = M2EventFourCC("$RR1"),
    FootRight_RR2                          = M2EventFourCC("$RR2"),
    FootRight_RR3                          = M2EventFourCC("$RR3"),
    FootLeft_SL0                           = M2EventFourCC("$SL0"),
    FootLeft_SL1                           = M2EventFourCC("$SL1"),
    FootLeft_SL2                           = M2EventFourCC("$SL2"),
    FootLeft_SL3                           = M2EventFourCC("$SL3"),
    FootRight_SR0                          = M2EventFourCC("$SR0"),
    FootRight_SR1                          = M2EventFourCC("$SR1"),
    FootRight_SR2                          = M2EventFourCC("$SR2"),
    FootRight_SR3                          = M2EventFourCC("$SR3"),
    FootLeft_WL0                           = M2EventFourCC("$WL0"),
    FootLeft_WL1                           = M2EventFourCC("$WL1"),
    FootLeft_WL2                           = M2EventFourCC("$WL2"),
    FootLeft_WL3                           = M2EventFourCC("$WL3"),
    FootRight_WR0                          = M2EventFourCC("$WR0"),
    FootRight_WR1                          = M2EventFourCC("$WR1"),
    FootRight_WR2                          = M2EventFourCC("$WR2"),
    FootRight_WR3                          = M2EventFourCC("$WR3"),
    MissileFirePosition_AIM                = M2EventFourCC("$AIM"),
    PlayBirthSound_BRT                     = M2EventFourCC("$BRT"),
    Breath_BTH                             = M2EventFourCC("$BTH"),
    RangedWeaponPull_BWP                   = M2EventFourCC("$BWP"),
    RangedWeaponRelease_BWR                = M2EventFourCC("$BWR"),
    ResolveAttackRound_CAH                 = M2EventFourCC("$CAH"),
    MountHeightOffset_CFM                  = M2EventFourCC("$CFM"),
    CameraPosition_CMA                     = M2EventFourCC("$CMA"),
    CombatDefense_CPP                      = M2EventFourCC("$CPP"),
    PlayEmoteSound_CSD                     = M2EventFourCC("$CSD"),
    MissileReleaseLeft_CSL                 = M2EventFourCC("$CSL"),
    MissileReleaseRight_CSR                = M2EventFourCC("$CSR"),
    PlayWeaponSwooshSound_CSS              = M2EventFourCC("$CSS"),
    MissileRelease_CST                     = M2EventFourCC("$CST"),
    StopSoundKit_DSE                       = M2EventFourCC("$DSE"),
    PlayLoopingSoundKit_DSL                = M2EventFourCC("$DSL"),
    PlayOneShotSoundKit_DSO                = M2EventFourCC("$DSO"),
    Death_DTH                              = M2EventFourCC("$DTH"),
    ObjectPackageEnterState2_EWT           = M2EventFourCC("$EWT"),
    ObjectPackageEnterState3_EAC           = M2EventFourCC("$EAC"),
    ObjectPackageEnterState4_EMV           = M2EventFourCC("$EMV"),
    ObjectPackageEnterState5_EDC           = M2EventFourCC("$EDC"),
    PlayEmoteStateSound_ESD                = M2EventFourCC("$ESD"),
    PlayStandSound_FDX                     = M2EventFourCC("$FDX"),
    Footfall_FSD                           = M2EventFourCC("$FSD"),
    PlayWoundAnimKit_HIT                   = M2EventFourCC("$HIT"),
    PlayDirectedSpellCastSound_SCD         = M2EventFourCC("$SCD"),
    CameraShake_SHK                        = M2EventFourCC("$SHK"),
    ExchangeSheathedWeaponLeft_SHL         = M2EventFourCC("$SHL"),
    ExchangeSheathedWeaponRight_SHR        = M2EventFourCC("$SHR"),
    PlaySubmergedSound_SMD                 = M2EventFourCC("$SMD"),
    PlaySubmergeSound_SMG                  = M2EventFourCC("$SMG"),
    PlaySoundKit_SND                       = M2EventFourCC("$SND"),
    MountTransitionBegin_STB               = M2EventFourCC("$STB"),
    MountTransitionEnd_STE                 = M2EventFourCC("$STE"),
    SpellEventSound_TRD                    = M2EventFourCC("$TRD"),
    PlayWingGlideSound_WGG                 = M2EventFourCC("$WGG"),
    PlayWingFlapSound_WNG                  = M2EventFourCC("$WNG"),
    PlayCustomAttackSound_AH0              = M2EventFourCC("$AH0"),
    PlayCustomAttackSound_AH1              = M2EventFourCC("$AH1"),
    PlayCustomAttackSound_AH2              = M2EventFourCC("$AH2"),
    PlayCustomAttackSound_AH3              = M2EventFourCC("$AH3"),
    PlayFidgetSound1_FD1                   = M2EventFourCC("$FD1"),
    PlayFidgetSound2_FD2                   = M2EventFourCC("$FD2"),
    PlayFidgetSound3_FD3                   = M2EventFourCC("$FD3"),
    PlayFidgetSound4_FD4                   = M2EventFourCC("$FD4"),
    PlayFidgetSound5_FD5                   = M2EventFourCC("$FD5"),
    FidgetSound6_FD6                       = M2EventFourCC("$FD6"),
    FidgetSound7_FD7                       = M2EventFourCC("$FD7"),
    FidgetSound8_FD8                       = M2EventFourCC("$FD8"),
    FidgetSound9_FD9                       = M2EventFourCC("$FD9"),
    GameObjectSound_GO0                    = M2EventFourCC("$GO0"),
    GameObjectSound_GO1                    = M2EventFourCC("$GO1"),
    GameObjectSound_GO2                    = M2EventFourCC("$GO2"),
    GameObjectSound_GO3                    = M2EventFourCC("$GO3"),
    GameObjectSound_GO4                    = M2EventFourCC("$GO4"),
    GameObjectSound_GO5                    = M2EventFourCC("$GO5"),
    GameObjectCustomSound_GC0              = M2EventFourCC("$GC0"),
    GameObjectCustomSound_GC1              = M2EventFourCC("$GC1"),
    GameObjectCustomSound_GC2              = M2EventFourCC("$GC2"),
    GameObjectCustomSound_GC3              = M2EventFourCC("$GC3"),
    VehicleBoneGrab_VG0                    = M2EventFourCC("$VG0"),
    VehicleBoneGrab_VG1                    = M2EventFourCC("$VG1"),
    VehicleBoneGrab_VG2                    = M2EventFourCC("$VG2"),
    VehicleBoneGrab_VG3                    = M2EventFourCC("$VG3"),
    VehicleBoneGrab_VG4                    = M2EventFourCC("$VG4"),
    VehicleBoneGrab_VG5                    = M2EventFourCC("$VG5"),
    VehicleBoneGrab_VG6                    = M2EventFourCC("$VG6"),
    VehicleBoneGrab_VG7                    = M2EventFourCC("$VG7"),
    VehicleBoneGrab_VG8                    = M2EventFourCC("$VG8"),
    VehicleBoneThrow_VT0                   = M2EventFourCC("$VT0"),
    VehicleBoneThrow_VT1                   = M2EventFourCC("$VT1"),
    VehicleBoneThrow_VT2                   = M2EventFourCC("$VT2"),
    VehicleBoneThrow_VT3                   = M2EventFourCC("$VT3"),
    VehicleBoneThrow_VT4                   = M2EventFourCC("$VT4"),
    VehicleBoneThrow_VT5                   = M2EventFourCC("$VT5"),
    VehicleBoneThrow_VT6                   = M2EventFourCC("$VT6"),
    VehicleBoneThrow_VT7                   = M2EventFourCC("$VT7"),
    VehicleBoneThrow_VT8                   = M2EventFourCC("$VT8"),
    WeaponTrailTop_WTT = WeaponTop_WTT,
    WeaponTrailBottom_WTB = WeaponBottom_WTB,
};

enum M2TEXTURETYPE : uint32_t {
    M2TEX_SKIN = 1,
    M2TEX_OBJECT_SKIN = 2,
    OBSOLETE_M2TEX_WEAPON_BLADE = 3,
    OBSOLETE_M2TEX_WEAPON_HANDLE = 4,
    OBSOLETE_M2TEX_ENVIRONMENT = 5,
    M2TEX_HAIR = 6,
    OBSOLETE_M2TEX_FACIAL_HAIR = 7,
    M2TEX_EXTRA_SKIN = 8,
    OBSOLETE_M2TEX_UI_SKIN = 9,
    OBSOLETE_M2TEX_CHARACTER_MISC = 10, // tauren mane
    M2TEX_MONSTER_1 = 11,
    M2TEX_MONSTER_2 = 12,
    M2TEX_MONSTER_3 = 13,
    M2TEX_ITEM_ICON = 14,
};

enum M2TRACKTYPE : uint16_t {
    M2TRACK_NONE = 0,
    M2TRACK_LINEAR = 1,
    M2TRACK_BEZIER = 2,
    M2TRACK_HERMITE = 3,
};

enum M2BLEND : uint16_t {
    M2BLEND_OPAQUE = 0,
    M2BLEND_ALPHA_KEY = 1,
    M2BLEND_ALPHA = 2,
    M2BLEND_NO_ALPHA_ADD = 3,
    M2BLEND_ADD = 4,
    M2BLEND_MOD = 5,
    M2BLEND_MOD2X = 6,
    // HoT: modern BlendAdd material and the internal Screen blend modern particles use.
    M2BLEND_BLEND_ADD = 7,
    M2BLEND_SCREEN = 8,
    M2BLEND_COUNT = 9,
};

enum M2PARTICLEBLENDMODE : uint8_t {
    M2PARTICLEBLEND_OPAQUE = 0,
    M2PARTICLEBLEND_ALPHA_KEY = 1,
    M2PARTICLEBLEND_ALPHA = 2,
    M2PARTICLEBLEND_NO_ALPHA_ADD = 3,
    M2PARTICLEBLEND_ADD = 4,
    M2PARTICLEBLEND_MOD = 5,
    M2PARTICLEBLEND_MOD2X = 6,
};

enum M2COMBINER : uint32_t {
    M2COMBINER_OPAQUE = 0,
    M2COMBINER_MOD = 1,
    M2COMBINER_DECAL = 2,
    M2COMBINER_ADD = 3,
    M2COMBINER_MOD2X = 4,
    M2COMBINER_FADE = 5,
    M2COMBINER_MOD2X_NA = 6,
    M2COMBINER_ADD_NA = 7,
    M2COMBINER_OP_MASK = 7,
    M2COMBINER_ENVMAP = 8,
};

enum M2_ELEMENT_TYPE : int32_t {
    M2_ELEMENT_UNSET = -1,
    M2_ELEMENT_BATCH = 0,
    M2_ELEMENT_PROJECTED_BATCH = 1,
    M2_ELEMENT_DOODAD_BATCH = 2,
    M2_ELEMENT_RIBBON = 3,
    M2_ELEMENT_PARTICLE = 4,
    M2_ELEMENT_CALLBACK = 5,
};

enum M2PASS {
    M2PASS_0 = 0,
    M2PASS_1 = 1,
    M2PASS_2 = 2,
};

enum M2LIGHTTYPE {
    M2LIGHT_DIRECT = 0,
    M2LIGHT_POINT = 1,
};

// Deferred M2 calls; MDL command indices are not compatible.
enum EModelModQ : uint32_t {
    MODEL_REPLACE_TEXTURE = 0,
    MODEL_SET_GEOMETRY_VISIBLE = 1,
    MODEL_SET_GEOSET_VISIBILITY_RANGE = 2,
    MODEL_OPTIMIZE_VISIBLE_GEOSETS = 3,
    MODEL_SET_BONE_FLAGS = 4,
    MODEL_SET_BONE_SEQUENCE = 5,
    MODEL_UNSET_BONE_SEQUENCE = 6,
    MODEL_SET_BONE_SEQUENCE_TIME = 7,
    MODEL_SET_BONE_SEQUENCE_SPEED = 8,
    MODEL_SET_BONE_PROCEDURAL_TRANSFORM = 9,
    MODEL_SET_LIGHT_ENABLED = 10,
    MODEL_REPLACE_PARTICLE_COLOR = 11,
    MODEL_SET_EMITTERS_ENABLED = 12,
    MODEL_SET_RIBBONS_ENABLED = 13,
    MODEL_LOAD_SEQUENCE = 14,
    MODEL_NUM_COMMANDS = 15,
    MODEL_COMMAND_NOT_QUEUED = 0xFFFFFFFF,
};

class CM2ModelCall;

class CM2ModelCall {
    public:
    /* 0000 */ uint32_t m_type;
    /* 0004 */ CM2ModelCall* m_next;
    /* 0008 */ uint32_t m_time;
    /* 000C */ uintptr_t m_args [17];

    CM2ModelCall();
};

#endif
