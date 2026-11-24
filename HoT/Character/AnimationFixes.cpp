#include <algorithm>
#include <cstdlib>
#include <vector>
#include "HoT/Character/AnimationFixes.h"
#include "HoT/Core/Modules.h"
#include "HoT/DB/Tables.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/AnimationDataRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/SpellEnums.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Types.h"

namespace HoT::Animation {

static const int32_t ANIM_NONE = 506;
static const uint8_t CLASS_MONK = 10;
static const uint32_t ROOT_BONE = 0xFFFFFFFF;
static const uint32_t ANY_VARIATION = 0xFFFFFFFF;

static const uint32_t MOVE_BACKWARD = 0x00000002;
static const uint32_t MOVE_DIRECTIONAL = 0x0000000F;
static const uint32_t MOVE_FALLING = 0x00003000;
static const uint32_t MOVE_SWIMMING = 0x00200000;
static const uint32_t MOVE_FLYING = 0x02000000;

enum AnimId : int32_t {
    ANIMATION_STAND = 0,
    ANIMATION_WALK = 4,
    ANIMATION_RUN = 5,
    ANIMATION_SHUFFLE_LEFT = 11,
    ANIMATION_SHUFFLE_RIGHT = 12,
    ANIMATION_RUN_BACKWARDS = 13,
    ANIMATION_ATTACK_1H = 17,
    ANIMATION_ATTACK_2H = 18,
    ANIMATION_ATTACK_2H_LOOSE = 19,
    ANIMATION_JUMP_START = 37,
    ANIMATION_FALL = 40,
    ANIMATION_SWIM_IDLE = 41,
    ANIMATION_SWIM = 42,
    ANIMATION_SWIM_LEFT = 43,
    ANIMATION_SWIM_RIGHT = 44,
    ANIMATION_SWIM_BACKWARDS = 45,
    ANIMATION_SPECIAL_1H = 57,
    ANIMATION_SPECIAL_2H = 58,
    ANIMATION_ATTACK_1H_PIERCE = 85,
    ANIMATION_ATTACK_2H_LOOSE_PIERCE = 86,
    ANIMATION_ATTACK_OFFHAND = 87,
    ANIMATION_ATTACK_OFFHAND_PIERCE = 88,
    ANIMATION_RUN_RIGHT = 92,
    ANIMATION_RUN_LEFT = 93,
    ANIMATION_ATTACK_UNARMED = 16,
    ANIMATION_READY_UNARMED = 25,
    ANIMATION_ATTACK_UNARMED_OFFHAND = 117,
    ANIMATION_SPECIAL_UNARMED = 118,
    ANIMATION_CHANNEL_CAST_DIRECTED = 124,
    ANIMATION_CHANNEL_CAST_OMNI = 125,
    ANIMATION_SPRINT = 143,
    ANIMATION_FLY = 135,
    ANIMATION_HOVER = 193,
    ANIMATION_FLY_STAND = 229,
    ANIMATION_FLY_WALK = 233,
    ANIMATION_FLY_RUN = 234,
    ANIMATION_FLY_SHUFFLE_LEFT = 240,
    ANIMATION_FLY_SHUFFLE_RIGHT = 241,
    ANIMATION_FLY_WALK_BACKWARDS = 242,
    ANIMATION_FLY_RUN_RIGHT = 321,
    ANIMATION_FLY_RUN_LEFT = 322,
    ANIMATION_FLY_FLY = 364,
    ANIMATION_SWIM_RUN = 524,
    ANIMATION_SWIM_WALK = 526,
    ANIMATION_SWIM_WALK_BACKWARDS = 528,
    ANIMATION_SWIM_SPRINT = 530,
    ANIMATION_MOUNT_SWIM_IDLE = 532,
    ANIMATION_MOUNT_SWIM_BACKWARDS = 534,
    ANIMATION_MOUNT_SWIM_LEFT = 536,
    ANIMATION_MOUNT_SWIM_RIGHT = 538,
    ANIMATION_MOUNT_SWIM_RUN = 540,
    ANIMATION_MOUNT_SWIM_SPRINT = 542,
    ANIMATION_MOUNT_SWIM_WALK = 544,
    ANIMATION_MOUNT_SWIM_WALK_BACKWARDS = 546,
    ANIMATION_MOUNT_FLIGHT_IDLE = 548,
    ANIMATION_MOUNT_FLIGHT_BACKWARDS = 550,
    ANIMATION_MOUNT_FLIGHT_LEFT = 552,
    ANIMATION_MOUNT_FLIGHT_RIGHT = 554,
    ANIMATION_MOUNT_FLIGHT_RUN = 556,
    ANIMATION_MOUNT_FLIGHT_SPRINT = 558,
    ANIMATION_MOUNT_FLIGHT_WALK = 560,
    ANIMATION_MOUNT_FLIGHT_WALK_BACKWARDS = 562,
};

static bool s_monkSpellsScanned = false;
static std::vector<int32_t> s_monkSpellIds;
static CGUnit_C* s_rootReentry = nullptr;

static bool EnsureMonkSpellScan() {
    if (s_monkSpellsScanned) {
        return true;
    }

    if (!g_spellDB.m_loaded) {
        return false;
    }

    SpellRec spell;

    for (int32_t id = g_spellDB.m_minID; id <= g_spellDB.m_maxID; id++) {
        if (!g_spellDB.GetRecordPrefix(id, &spell, offsetof(SpellRec, m_effectAura) + sizeof(spell.m_effectAura))) {
            continue;
        }

        for (int32_t i = 0; i < 3; i++) {
            if (spell.m_effectAura[i] == SPELL_AURA_MONK_ANIMS) {
                s_monkSpellIds.push_back(spell.m_id);
                break;
            }
        }
    }

    std::sort(s_monkSpellIds.begin(), s_monkSpellIds.end());
    s_monkSpellsScanned = true;

    return true;
}

static bool HasMonkAura(CGUnit_C* unit) {
    if (!EnsureMonkSpellScan() || s_monkSpellIds.empty()) {
        return false;
    }

    uint32_t count = unit->GetAuraCount();

    for (uint32_t i = 0; i < count; i++) {
        AuraData* aura = unit->GetAura(i);

        if (aura && std::binary_search(s_monkSpellIds.begin(), s_monkSpellIds.end(), aura->m_spellId)) {
            return true;
        }
    }

    return false;
}

static bool KnowsMonkSpell(CGUnit_C* unit) {
    if (!EnsureMonkSpellScan()) {
        return false;
    }

    for (int32_t spellId : s_monkSpellIds) {
        if (unit->IsSpellKnown(spellId)) {
            return true;
        }
    }

    return false;
}

bool UsesUnarmedAnimations(const CGUnit_C* unit) {
    if (!unit) {
        return false;
    }

    CGUnit_C* u = const_cast<CGUnit_C*>(unit);

    return u->m_unit->m_unitFieldBytes0[1] == CLASS_MONK || HasMonkAura(u) || KnowsMonkSpell(u);
}

static int32_t Behavior(int32_t animId) {
    if (animId < 0) {
        return -1;
    }

    AnimationDataRec* rec = g_animationDataDB.GetRecord(animId);

    return rec ? rec->m_behaviorID : animId;
}

static bool IsMonkOffhandAttack(int32_t behavior) {
    return behavior == ANIMATION_ATTACK_OFFHAND || behavior == ANIMATION_ATTACK_OFFHAND_PIERCE || behavior == ANIMATION_ATTACK_UNARMED_OFFHAND;
}

static bool IsMonkMainhandAttack(int32_t behavior) {
    switch (behavior) {
        case ANIMATION_ATTACK_1H:
        case ANIMATION_ATTACK_2H:
        case ANIMATION_ATTACK_2H_LOOSE:
        case ANIMATION_SPECIAL_1H:
        case ANIMATION_SPECIAL_2H:
        case ANIMATION_ATTACK_1H_PIERCE:
        case ANIMATION_ATTACK_2H_LOOSE_PIERCE:
        case ANIMATION_SPECIAL_UNARMED:
            return true;
        default:
            return false;
    }
}

static int32_t MonkAttackAnim(int32_t animId) {
    if (animId < 0) {
        return -1;
    }

    if (IsMonkOffhandAttack(animId)) {
        return MONK_ATTACK_OFFHAND;
    }

    if (IsMonkMainhandAttack(animId)) {
        return MONK_ATTACK_MAINHAND;
    }

    AnimationDataRec* rec = g_animationDataDB.GetRecord(animId);

    if (!rec) {
        return -1;
    }

    if (IsMonkOffhandAttack(rec->m_behaviorID)) {
        return MONK_ATTACK_OFFHAND;
    }

    if (IsMonkMainhandAttack(rec->m_behaviorID)) {
        return MONK_ATTACK_MAINHAND;
    }

    return -1;
}

uint32_t RemapMonkAttack(CGUnit_C* unit, uint32_t animId) {
    int32_t monk = MonkAttackAnim(static_cast<int32_t>(animId));

    if (monk != -1 && UsesUnarmedAnimations(unit)) {
        return static_cast<uint32_t>(monk);
    }

    return animId;
}

uint32_t UnarmedAttackAnim(const CGUnit_C* unit, bool offhand) {
    if (!UsesUnarmedAnimations(unit)) {
        return offhand ? ANIMATION_ATTACK_UNARMED_OFFHAND : ANIMATION_ATTACK_UNARMED;
    }

    if (offhand) {
        return MONK_ATTACK_OFFHAND;
    }

    return rand() % 100 < 30 ? MONK_ATTACK_OFFHAND : MONK_ATTACK_MAINHAND;
}

uint32_t UnarmedReadyAnim(const CGUnit_C* unit) {
    return UsesUnarmedAnimations(unit) ? MONK_READY : ANIMATION_READY_UNARMED;
}

static bool IsChannelCast(int32_t animId) {
    if (animId == ANIMATION_CHANNEL_CAST_DIRECTED || animId == ANIMATION_CHANNEL_CAST_OMNI) {
        return true;
    }

    if (animId < 0) {
        return false;
    }

    AnimationDataRec* rec = g_animationDataDB.GetRecord(animId);

    return rec && (rec->m_behaviorID == ANIMATION_CHANNEL_CAST_DIRECTED || rec->m_behaviorID == ANIMATION_CHANNEL_CAST_OMNI);
}

static bool IsAirborne(int32_t animId) {
    if (animId == ANIMATION_JUMP_START || animId == ANIMATION_FALL) {
        return true;
    }

    if (animId < 0) {
        return false;
    }

    AnimationDataRec* rec = g_animationDataDB.GetRecord(animId);

    return rec && rec->m_behaviorID >= 37 && (rec->m_behaviorID <= 40 || rec->m_behaviorID == 467);
}

static uint32_t MoveFlags(const CGUnit_C* unit) {
    return unit->m_movement ? unit->m_movement->m_flags : 0;
}

static bool IsMovingLowerBody(const CGUnit_C* unit) {
    return (MoveFlags(unit) & (MOVE_DIRECTIONAL | MOVE_FALLING | MOVE_SWIMMING)) != 0;
}

static bool IsJumpingUpward(const CGUnit_C* unit) {
    if (!(MoveFlags(unit) & MOVE_FALLING)) {
        return false;
    }

    return unit->m_movement->m_fallVelocity > 0.0f;
}

static bool IsChanneling(const CGUnit_C* unit) {
    return unit->m_channelSpellId || unit->m_unit->m_unitChannelSpell;
}

// Reading a bone of a model that is still loading would block on the load, so it reads as no sequence.
static int32_t BoneSequence(CM2Model* model, uint32_t boneId) {
    if (!model || !(model->m_fFlags & 1)) {
        return 0;
    }

    return static_cast<int32_t>(model->GetBoneSequenceId(boneId));
}

static void SetRootSequence(CGUnit_C* unit, CM2Model* model, int32_t animId) {
    unit->SetKeyBoneSequence(model, ROOT_BONE, animId, ANY_VARIATION, 0, 1.0f, 0, 1, 0);
}

static void SetTorsoSequence(CGUnit_C* unit, CM2Model* model, int32_t animId) {
    if (unit->m_torsoKeyBone == -1) {
        return;
    }

    unit->SetKeyBoneSequence(model, unit->m_torsoKeyBone, animId, ANY_VARIATION, 0, 1.0f, 0, 1, 0);
}

static int32_t TorsoChannelAnim(CGUnit_C* unit, CM2Model* model) {
    if (unit->m_torsoKeyBone == -1) {
        return 0;
    }

    int32_t anim = BoneSequence(model, unit->m_torsoKeyBone);

    return IsChannelCast(anim) ? anim : 0;
}

static int32_t RootChannelAnim(CM2Model* model) {
    int32_t anim = BoneSequence(model, ROOT_BONE);

    return IsChannelCast(anim) ? anim : 0;
}

static void MoveRootChannelToTorso(CGUnit_C* unit, CM2Model* model) {
    if (TorsoChannelAnim(unit, model)) {
        return;
    }

    int32_t channel = RootChannelAnim(model);

    if (channel) {
        SetTorsoSequence(unit, model, channel);
    }
}

static void RestoreRootChannelFromTorso(CGUnit_C* unit, CM2Model* model) {
    if (unit->m_torsoKeyBone == -1) {
        return;
    }

    int32_t channel = TorsoChannelAnim(unit, model);

    if (!channel) {
        return;
    }

    if (!RootChannelAnim(model)) {
        SetRootSequence(unit, model, channel);
    }

    unit->UnsetKeyBoneSequence(model, unit->m_torsoKeyBone, 1, 0, 1);
}

static int32_t LowerBodyAnim(CGUnit_C* unit, uint32_t animFlags, int32_t mask, bool allowAirborne) {
    uint32_t flags = MoveFlags(unit);

    if (flags & MOVE_SWIMMING) {
        if (flags & MOVE_BACKWARD) {
            return ANIMATION_SWIM_BACKWARDS;
        }

        return (flags & MOVE_DIRECTIONAL) ? ANIMATION_SWIM : ANIMATION_SWIM_IDLE;
    }

    if (allowAirborne && (flags & MOVE_FALLING)) {
        int32_t root = BoneSequence(unit->m_worldModel, ROOT_BONE);

        if (root == ANIMATION_JUMP_START) {
            if (IsJumpingUpward(unit)) {
                return root;
            }
        } else if (IsAirborne(root)) {
            return root;
        }

        if (IsJumpingUpward(unit)) {
            return ANIMATION_JUMP_START;
        }

        uint32_t resolvedFlags = animFlags;
        int32_t resolved = static_cast<int32_t>(unit->ChooseAnimation(static_cast<uint32_t>(mask | 4), &resolvedFlags, nullptr));

        if (resolved != ANIM_NONE && !IsChannelCast(resolved)) {
            return resolved;
        }

        return ANIMATION_FALL;
    }

    if (flags & MOVE_DIRECTIONAL) {
        if (flags & MOVE_BACKWARD) {
            return ANIMATION_RUN_BACKWARDS;
        }

        float speed = unit->m_movement->GetBaseSpeed(0);

        if (speed >= 11.0f) {
            return ANIMATION_SPRINT;
        }

        return speed > unit->m_movement->m_walkSpeed * 2.0f ? ANIMATION_RUN : ANIMATION_WALK;
    }

    return -1;
}

static bool ShouldRouteToTorso(CGUnit_C* unit, int32_t animId) {
    return unit && IsMovingLowerBody(unit) && IsChanneling(unit) && IsChannelCast(animId) && unit->m_torsoKeyBone != -1;
}

bool RouteChannelToTorso(CGUnit_C* unit, uint32_t animId) {
    return ShouldRouteToTorso(unit, static_cast<int32_t>(animId));
}

void OnSetKeyBoneSequence(CGUnit_C* unit, CM2Model* model, uint32_t& boneId, uint32_t& sequenceId, uint32_t time, int32_t blend, int32_t primary, int32_t force) {
    // The root sequence set below goes straight to the stock code.
    if (s_rootReentry == unit) {
        s_rootReentry = nullptr;
        return;
    }

    if (boneId == ROOT_BONE && ShouldRouteToTorso(unit, static_cast<int32_t>(sequenceId))) {
        int32_t root = LowerBodyAnim(unit, 0, 4, true);

        if (root != -1) {
            s_rootReentry = unit;
            unit->SetKeyBoneSequence(model, ROOT_BONE, root, ANY_VARIATION, time, 1.0f, blend, primary, force);
            s_rootReentry = nullptr;
        }

        boneId = unit->m_torsoKeyBone;
    }

    sequenceId = RemapMonkAttack(unit, sequenceId);
}

void OnPreAnimate(CGUnit_C* unit) {
    if (!IsChanneling(unit) || !IsMovingLowerBody(unit)) {
        return;
    }

    CM2Model* model = unit->m_worldModel;

    if (!model) {
        return;
    }

    int32_t root = BoneSequence(model, ROOT_BONE);

    if (MoveFlags(unit) & MOVE_FALLING) {
        if (!IsAirborne(root)) {
            MoveRootChannelToTorso(unit, model);
            SetRootSequence(unit, model, ANIMATION_JUMP_START);
        }

        return;
    }

    if (!IsAirborne(root)) {
        return;
    }

    MoveRootChannelToTorso(unit, model);

    int32_t lower = LowerBodyAnim(unit, 0, 4, false);

    if (lower != -1) {
        SetRootSequence(unit, model, lower);
    }
}

UpdateBaseAnimationScope::UpdateBaseAnimationScope(CGUnit_C* unit, uint32_t animFlags, int32_t mask) : m_unit(unit), m_animFlags(animFlags), m_mask(mask), m_rootChannelAnim(0) {
    if (IsChanneling(unit) && unit->m_worldModel) {
        this->m_rootChannelAnim = RootChannelAnim(unit->m_worldModel);
    }
}

UpdateBaseAnimationScope::~UpdateBaseAnimationScope() {
    CGUnit_C* unit = this->m_unit;

    if (!IsChanneling(unit)) {
        return;
    }

    CM2Model* model = unit->m_worldModel;

    if (!model) {
        return;
    }

    if (!IsMovingLowerBody(unit)) {
        RestoreRootChannelFromTorso(unit, model);
        return;
    }

    MoveRootChannelToTorso(unit, model);

    if (this->m_rootChannelAnim && !TorsoChannelAnim(unit, model)) {
        SetTorsoSequence(unit, model, this->m_rootChannelAnim);
    }

    int32_t lower = LowerBodyAnim(unit, this->m_animFlags, this->m_mask, true);

    if (lower != -1) {
        SetRootSequence(unit, model, lower);
    }
}

FallLandScope::~FallLandScope() {
    CGUnit_C* unit = this->m_unit;

    if (!IsChanneling(unit)) {
        return;
    }

    CM2Model* model = unit->m_worldModel;

    if (!model) {
        return;
    }

    MoveRootChannelToTorso(unit, model);

    int32_t lower = LowerBodyAnim(unit, 0, 4, false);

    if (lower != -1) {
        SetRootSequence(unit, model, lower);
    }
}

static bool HasDirectSequence(CM2Model* model, int32_t animId) {
    if (!model || animId < 0 || !model->m_shared || !model->m_shared->m_data) {
        return false;
    }

    return model->HasSequence(model->m_shared->m_data, static_cast<uint32_t>(animId)) != 0;
}

static int32_t ResolveExtendedAnim(CGUnit_C* unit, int32_t animId, CM2Model* model) {
    if (animId <= ANIM_NONE) {
        return -1;
    }

    if (!model && unit) {
        model = unit->m_worldModel;
    }

    if (HasDirectSequence(model, animId)) {
        return animId;
    }

    int32_t current = animId;

    for (int32_t depth = 0; depth < 16; depth++) {
        AnimationDataRec* rec = g_animationDataDB.GetRecord(current);

        if (!rec || rec->m_fallback == current) {
            break;
        }

        current = rec->m_fallback;

        if (current >= ANIM_NONE && HasDirectSequence(model, current)) {
            return current;
        }

        if (current < ANIM_NONE) {
            return current;
        }
    }

    return -1;
}

enum MountMode {
    MOUNT_MODE_NONE,
    MOUNT_MODE_SWIM,
    MOUNT_MODE_FLIGHT,
};

enum MountMotion {
    MOTION_NONE,
    MOTION_IDLE,
    MOTION_FORWARD,
    MOTION_WALK,
    MOTION_SPRINT,
    MOTION_LEFT,
    MOTION_RIGHT,
    MOTION_BACKWARD,
    MOTION_COUNT,
};

struct MotionEntry {
    int32_t m_behavior;
    MountMotion m_motion;
};

static const MotionEntry s_mountMotions[] = {
    { ANIMATION_STAND, MOTION_IDLE },
    { ANIMATION_SWIM_IDLE, MOTION_IDLE },
    { ANIMATION_HOVER, MOTION_IDLE },
    { ANIMATION_RUN, MOTION_FORWARD },
    { ANIMATION_SWIM, MOTION_FORWARD },
    { ANIMATION_SWIM_RUN, MOTION_FORWARD },
    { ANIMATION_FLY, MOTION_FORWARD },
    { ANIMATION_WALK, MOTION_WALK },
    { ANIMATION_SWIM_WALK, MOTION_WALK },
    { ANIMATION_SPRINT, MOTION_SPRINT },
    { ANIMATION_SWIM_SPRINT, MOTION_SPRINT },
    { ANIMATION_SHUFFLE_LEFT, MOTION_LEFT },
    { ANIMATION_SWIM_LEFT, MOTION_LEFT },
    { ANIMATION_RUN_LEFT, MOTION_LEFT },
    { ANIMATION_SHUFFLE_RIGHT, MOTION_RIGHT },
    { ANIMATION_SWIM_RIGHT, MOTION_RIGHT },
    { ANIMATION_RUN_RIGHT, MOTION_RIGHT },
    { ANIMATION_RUN_BACKWARDS, MOTION_BACKWARD },
    { ANIMATION_SWIM_BACKWARDS, MOTION_BACKWARD },
    { ANIMATION_SWIM_WALK_BACKWARDS, MOTION_BACKWARD },
};

static const int32_t s_flightIdle[] = { ANIMATION_MOUNT_FLIGHT_IDLE, ANIMATION_FLY_STAND, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_MOUNT_FLIGHT_WALK };
static const int32_t s_flightForward[] = { ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_FLY_FLY, ANIMATION_MOUNT_FLIGHT_WALK };
static const int32_t s_flightWalk[] = { ANIMATION_MOUNT_FLIGHT_WALK, ANIMATION_FLY_WALK, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN };
static const int32_t s_flightSprint[] = { ANIMATION_MOUNT_FLIGHT_SPRINT, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_FLY_FLY };
static const int32_t s_flightLeft[] = { ANIMATION_MOUNT_FLIGHT_LEFT, ANIMATION_FLY_RUN_LEFT, ANIMATION_FLY_SHUFFLE_LEFT, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_MOUNT_FLIGHT_WALK };
static const int32_t s_flightRight[] = { ANIMATION_MOUNT_FLIGHT_RIGHT, ANIMATION_FLY_RUN_RIGHT, ANIMATION_FLY_SHUFFLE_RIGHT, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_MOUNT_FLIGHT_WALK };
static const int32_t s_flightBackward[] = { ANIMATION_MOUNT_FLIGHT_BACKWARDS, ANIMATION_MOUNT_FLIGHT_WALK_BACKWARDS, ANIMATION_FLY_WALK_BACKWARDS, ANIMATION_FLY_WALK, ANIMATION_MOUNT_FLIGHT_RUN, ANIMATION_FLY_RUN, ANIMATION_MOUNT_FLIGHT_WALK };

static const int32_t s_swimIdle[] = { ANIMATION_MOUNT_SWIM_IDLE };
static const int32_t s_swimForward[] = { ANIMATION_MOUNT_SWIM_RUN, ANIMATION_MOUNT_SWIM_WALK };
static const int32_t s_swimWalk[] = { ANIMATION_MOUNT_SWIM_WALK, ANIMATION_MOUNT_SWIM_RUN };
static const int32_t s_swimSprint[] = { ANIMATION_MOUNT_SWIM_SPRINT, ANIMATION_MOUNT_SWIM_RUN };
static const int32_t s_swimLeft[] = { ANIMATION_MOUNT_SWIM_LEFT, ANIMATION_MOUNT_SWIM_RUN };
static const int32_t s_swimRight[] = { ANIMATION_MOUNT_SWIM_RIGHT, ANIMATION_MOUNT_SWIM_RUN };
static const int32_t s_swimBackward[] = { ANIMATION_MOUNT_SWIM_BACKWARDS, ANIMATION_MOUNT_SWIM_WALK_BACKWARDS, ANIMATION_MOUNT_SWIM_RUN };

template <class T, size_t N>
static constexpr size_t Count(const T (&)[N]) {
    return N;
}

struct MotionCandidates {
    const int32_t* m_flight;
    size_t m_flightCount;
    const int32_t* m_swim;
    size_t m_swimCount;
};

static const MotionCandidates s_motionCandidates[] = {
    { nullptr, 0, nullptr, 0 },
    { s_flightIdle, Count(s_flightIdle), s_swimIdle, Count(s_swimIdle) },
    { s_flightForward, Count(s_flightForward), s_swimForward, Count(s_swimForward) },
    { s_flightWalk, Count(s_flightWalk), s_swimWalk, Count(s_swimWalk) },
    { s_flightSprint, Count(s_flightSprint), s_swimSprint, Count(s_swimSprint) },
    { s_flightLeft, Count(s_flightLeft), s_swimLeft, Count(s_swimLeft) },
    { s_flightRight, Count(s_flightRight), s_swimRight, Count(s_swimRight) },
    { s_flightBackward, Count(s_flightBackward), s_swimBackward, Count(s_swimBackward) },
};

static_assert(Count(s_motionCandidates) == MOTION_COUNT, "mount motion table out of sync");

static MountMode GetMountMode(CGUnit_C* unit) {
    if (!unit || !unit->m_mountModel) {
        return MOUNT_MODE_NONE;
    }

    uint32_t flags = MoveFlags(unit);

    if ((flags & MOVE_FLYING) || unit->m_animTier == ANIM_TIER_FLY) {
        return MOUNT_MODE_FLIGHT;
    }

    if ((flags & MOVE_SWIMMING) || unit->m_animTier == ANIM_TIER_SWIM) {
        return MOUNT_MODE_SWIM;
    }

    return MOUNT_MODE_NONE;
}

static MountMotion GetMountMotion(int32_t behavior) {
    if (behavior < 0) {
        return MOTION_NONE;
    }

    for (const MotionEntry& entry : s_mountMotions) {
        if (entry.m_behavior == behavior) {
            return entry.m_motion;
        }
    }

    return MOTION_NONE;
}

static int32_t MountedAnim(CGUnit_C* unit, int32_t animId, CM2Model* model) {
    if (animId < 0 || !model) {
        return -1;
    }

    MountMode mode = GetMountMode(unit);

    if (mode == MOUNT_MODE_NONE) {
        return -1;
    }

    MountMotion motion = GetMountMotion(Behavior(animId));

    if (motion == MOTION_NONE) {
        return -1;
    }

    const MotionCandidates& candidates = s_motionCandidates[motion];
    bool flight = mode == MOUNT_MODE_FLIGHT;
    const int32_t* ids = flight ? candidates.m_flight : candidates.m_swim;
    size_t count = flight ? candidates.m_flightCount : candidates.m_swimCount;

    for (size_t i = 0; i < count; i++) {
        if (HasDirectSequence(model, ids[i])) {
            return ids[i];
        }
    }

    return -1;
}

uint32_t ResolveAnimForModel(CGUnit_C* unit, uint32_t& animId, CM2Model* model) {
    animId = RemapMonkAttack(unit, animId);

    int32_t anim = static_cast<int32_t>(animId);
    int32_t mounted = MountedAnim(unit, anim, model ? model : unit->m_worldModel);

    if (mounted != -1) {
        return static_cast<uint32_t>(mounted);
    }

    if (anim > ANIM_NONE) {
        int32_t extended = ResolveExtendedAnim(unit, anim, model);

        if (extended != -1) {
            return static_cast<uint32_t>(extended);
        }
    }

    return 0xFFFFFFFF;
}

void InvalidateSpellScan() {
    s_monkSpellsScanned = false;
    s_monkSpellIds.clear();
}

static void OnSpellReloaded(const char* stem) {
    InvalidateSpellScan();
}

class AnimationModule : public Module {
    public:
    const char* Name() const override {
        return "AnimationFixes";
    }

    void OnInitializeGlobal() override {
        DB::OnTableReloaded("Spell", OnSpellReloaded);
    }
};

Module* GetModule() {
    static AnimationModule module;
    return &module;
}

}
