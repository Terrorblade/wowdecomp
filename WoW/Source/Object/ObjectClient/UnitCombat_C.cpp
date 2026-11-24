#include "UnitCombat_C.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Tempest/CRandom.h"
#include "WoW/Common/ComponentCore/Types.h"
#include "WoW/Source/DB/DBClient/CreatureStats_C.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellVisualKitRec.h"
#include "WoW/Source/Magic/MagicClient/SpellVisuals.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Effect_C.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/FrameTime.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Object/Types.h"

// STATIC: 0x00CA1600
static uint32_t s_nextVictimImpactTime;

// VERIFY: 64e4513d auto
// OFFSET: 0x00754EF0
const CGUnitVirtualItem* CGUnit_C::GetParryingItem(bool offhand) const {
    const CGUnitVirtualItem* item = this->GetVirtualItem(VIRTUAL_MONSTER_SLOT_MAINHAND, false);

    if (offhand || (item && item->m_classID != ITEM_CLASS_WEAPON)) {
        item = this->GetVirtualItem(VIRTUAL_MONSTER_SLOT_OFFHAND, false);

        if (item && item->m_classID != ITEM_CLASS_WEAPON && item->m_classID != ITEM_CLASS_ARMOR) {
            return nullptr;
        }
    }

    return item;
}

// VERIFY: 7a0f5b4d auto
// OFFSET: 0x00754F40
int32_t CGUnit_C::GetUnitSize() const {
    int32_t size = this->m_displayInfo->m_sizeClass;

    if (size == -1) {
        return this->m_modelData->m_sizeClass;
    }

    return size;
}

// VERIFY: b8716225 manual
// OFFSET: 0x00755270
void CGUnit_C::HandleEnvironmentDamage(uint32_t type, int32_t damage, int32_t absorbed, int32_t resisted) {
    if (damage || absorbed || resisted) {
        WOWGUID guid = this->m_obj->m_guid;
        UnitCombatLog(guid, type, damage, absorbed, resisted);

        if (this->IsActivePlayer()) {
            uint32_t flags = absorbed ? HIT_INFO_FLAG_FULL_ABSORB : HIT_INFO_FLAG_NONE;

            if (resisted) {
                flags |= HIT_INFO_FLAG_FULL_RESIST;
            }

            WOWGUID target = this->m_obj->m_guid;
            CGGameUI::ShowCombatFeedback(target, damage, 1 << s_environmentalDamageSchools[type], flags);
        }
    }

    SpellVisualKitRec* kit = g_spellVisualKitDB.GetRecord(g_environmentalDamageVisualKits[type]);

    if (kit) {
        PlaySpellVisuaKitData data(nullptr, kit, SPELL_VISUAL_KIT_CAST);
        this->PlaySpellVisualKit(data);
    }
}

// VERIFY: 94a622f4 auto
// OFFSET: 0x00755540
const CGUnitVirtualItem* CGUnit_C::GetAttackingWeapon(COMBATHAND hand, bool requireLoadedModel) const {
    if (!this->m_sheatheState) {
        return nullptr;
    }

    const CGUnitVirtualItem* item = this->GetVirtualItem(static_cast<VIRTUAL_MONSTER_SLOT>(hand != 0), false);

    if (!item || item->m_classID != ITEM_CLASS_WEAPON) {
        return nullptr;
    }

    if (!requireLoadedModel) {
        return item;
    }

    CM2Model* model = this->m_worldModel;

    if (!model || !model->IsLoaded(0, 0)) {
        return nullptr;
    }

    CM2Model* attached = model->m_attachList;

    if (!attached) {
        return nullptr;
    }

    while (attached->m_attachmentId != (hand != 0) + 1) {
        attached = attached->m_attachNext;

        if (!attached) {
            return nullptr;
        }
    }

    if (attached->IsLoaded(0, 0)) {
        return item;
    }

    return nullptr;
}

// VERIFY: 2e85141a manual
// OFFSET: 0x007559E0
void CGUnit_C::StopAttack() {
    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_ATTACK_STOP));
    msg.Finalize();
    ClientServices::Send2(&msg);
    this->m_stopAttackSent = 1;
}

// VERIFY: f28002be manual
// OFFSET: 0x00755E40
void CGUnit_C::PlayVictimRound(const ATTACKROUNDINFO& info) {
    if (this->m_unit->m_unitDynamicFlags & UNIT_DYNFLAG_LOOTABLE) {
        return;
    }

    this->PlayVictimWorldFeedback(info);

    if (info.m_hitInfo & HIT_INFO_FLAG_AFFECTS_VICTIM) {
        this->PlayWoundAnimKit(info.m_hitInfo & HIT_INFO_FLAG_CRITICAL);

        if ((info.m_damage || (info.m_hitInfo & 0x1000000)) && (info.m_victimState == VS_WOUND || info.m_victimState == VS_INTERRUPT)) {
            WOWGUID attacker = info.m_attacker;
            this->ShowBloodSpurt(attacker, info.m_hitInfo & HIT_INFO_FLAG_LARGE_BLOOD_SPURT);
        }
    }

    if (this->DoNotPlayWoundAnim()) {
        uint32_t now = FrameTime::s_curTimeMs;

        if (now <= s_nextVictimImpactTime) {
            return;
        }

        s_nextVictimImpactTime = static_cast<uint32_t>((static_cast<uint64_t>(CRandom::uint32_(g_rndSeed)) * 2000) >> 32) + now + 3000;
    }

    if (info.m_victimState == VS_PARRY) {
        C3Vector position = this->GetPosition();
        this->SetImpactKitEffect(false, &info, &position);
    } else if (info.m_blocked) {
        C3Vector position = this->GetPosition();
        this->SetImpactKitEffect(true, &info, &position);
    } else if (info.m_victimState == VS_DEFLECT) {
        C3Vector position = this->GetPosition();
        position.z += 2.0f;
        SI2::PlaySoundKit("(DONOTRENAME)ShieldWoodImpact", &position, nullptr, nullptr);
    }

    if ((info.m_hitInfo & 0xA0) || info.m_victimState == VS_IMMUNE) {
        C3Vector position = this->GetPosition();
        position.z += 2.0f;
        SI2::PlaySoundKit("(DONOTRENAME)AbsorbGetHit", &position, nullptr, nullptr);
    } else if (info.m_hitInfo & HIT_INFO_FLAG_AFFECTS_VICTIM) {
        if (info.m_hitInfo & HIT_INFO_FLAG_CRUSHING) {
            this->PlayUnitSound(UNITSOUNDTYPE_INJURYCRUSHINGBLOW, 0);
        } else if (info.m_hitInfo & HIT_INFO_FLAG_CRITICAL) {
            this->PlayUnitSound(UNITSOUNDTYPE_INJURYCRITICAL, 0);
        } else if (!(info.m_hitInfo & HIT_INFO_FLAG_MISS)) {
            this->PlayUnitSound(UNITSOUNDTYPE_INJURY, 0);
        }
    }
}

// VERIFY: e2e6478a manual
// OFFSET: 0x00756040
void CGUnit_C::PlayRangedItemPull() {
    CM2Model* ranged = this->m_rangedWeaponModel;

    if (!ranged || !ranged->IsLoaded(0, 0) || !ranged->HasSequence(ANIM_ITEM_BOWPULL)) {
        return;
    }

    if (!this->m_worldModel || !this->m_worldModel->IsLoaded(0, 0)) {
        return;
    }

    M2BoneSequenceInfo info;
    info.m_sequenceId = INVALID_ANIMATION;

    if (this->m_torsoKeyBone == KEY_BONE_SPINE_LOW) {
        this->m_worldModel->GetBoneSequenceInfo(KEY_BONE_SPINE_LOW, info);

        if (info.m_sequenceId == INVALID_ANIMATION) {
            this->m_worldModel->GetBoneSequenceInfo(0xFFFFFFFF, info);
        }
    } else {
        this->m_worldModel->GetBoneSequenceInfo(0xFFFFFFFF, info);
    }

    if (info.m_finished) {
        return;
    }

    int32_t elapsed = static_cast<int32_t>(static_cast<int64_t>(static_cast<int32_t>(info.m_time) / info.m_speed));
    int32_t remaining = info.m_endTime - elapsed - info.m_startTime;

    if (remaining <= 0) {
        return;
    }

    M2SequenceInfo sequence;
    sequence.Clear();
    this->m_rangedWeaponModel->GetSequenceInfo(ANIM_ITEM_BOWPULL, 0, sequence);
    this->m_rangedWeaponModel->SetBoneSequence(0xFFFFFFFF, ANIM_ITEM_BOWPULL, 0, 0, static_cast<float>(static_cast<int32_t>(sequence.m_duration)) / remaining, 1, 1);
}

// VERIFY: ee364054 manual
// OFFSET: 0x007561E0
void CGUnit_C::OnDeathAnimate() {
    if (!this->DoNotLogDeath()) {
        UnitCombatLogUnitDead(this);
    }

    this->PlayDeathAnimation(0);

    if (!(this->m_obj->m_type & TYPE_PLAYER) || !(static_cast<CGPlayer_C*>(this)->m_player->m_playerFlags & PLAYER_FLAG_OUT_OF_BOUNDS)) {
        this->OnAiReaction(AI_REACTION_SOUND_DEATH);
    }

    this->KillCreatureLoopSound();

    if (this->IsActivePlayer()) {
        CGGameUI::UpdateActivePlayer();
    }
}

// VERIFY: 4078ec8f manual
// OFFSET: 0x00755380
void CGUnit_C::ShowBloodSpurt(const WOWGUID& attacker, int32_t large) {
    UnitBloodRec* blood = this->GetBloodRecord();

    if (!blood) {
        return;
    }

    MODEL_ATTACHMENT_ID attachType = ATTACH_TORSOBLOODFRONT;
    CGUnit_C* attackerUnit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(attacker, TYPE_UNIT, __FILE__, __LINE__));

    if (attackerUnit) {
        C3Vector position = this->GetPosition();
        C3Vector attackerPosition = attackerUnit->GetPosition();
        float dx = attackerPosition.x - position.x;
        float dy = attackerPosition.y - position.y;
        float facing = this->CGUnit_C::GetRenderFacing();
        float cosFacing = cosf(facing);
        float sinFacing = sinf(facing);

        if (dy * sinFacing + dx * cosFacing < 0.0f) {
            attachType = ATTACH_TORSOBLOODBACK;
        }
    }

    const BLOODSPURTSIZES size = large ? BLOODSPURT_LARGE : BLOODSPURT_SMALL;
    int32_t effectId = attachType == ATTACH_TORSOBLOODFRONT ? blood->m_combatBloodSpurtFront[size] : blood->m_combatBloodSpurtBack[size];
    SpellVisualEffectNameRec* effectName = g_spellVisualEffectNameDB.GetRecord(effectId);

    if (!effectName) {
        return;
    }

    CEffect* effect = new (SMemAlloc(sizeof(CEffect), __FILE__, __LINE__, 0)) CEffect();
    WOWGUID source = this->m_obj->m_guid;
    effect->AddEffect(attachType, 0, nullptr, effectName, &attacker, &source, CEffect::EFFECT_CALLBACK_LIFETIME, CGObject_C::EffectOnceHandler, nullptr, 0, nullptr);
    effect->Release();
}

// VERIFY: 5b00631f auto
// OFFSET: 0x007555E0
void CGUnit_C::ShakeCameraOnHit() {
    if (this->m_unit->m_unitFieldPetnumber) {
        return;
    }

    int32_t shakeId = this->m_modelData->m_deathThudShakeSize;

    if (shakeId) {
        C3Vector position = this->GetPosition();
        CGWorldFrame::GetActiveCamera()->AddShakeById(shakeId, &position);
    }
}

// VERIFY: 4da467b9 manual
// OFFSET: 0x00755630
void ATTACKROUNDINFO::Unpack(CDataStore& msg) {
    msg.Get(this->m_hitInfo);
    msg >> *reinterpret_cast<SmartGUID*>(&this->m_attacker);
    msg >> *reinterpret_cast<SmartGUID*>(&this->m_target);
    msg.Get(reinterpret_cast<uint32_t&>(this->m_damage));
    msg.Get(reinterpret_cast<uint32_t&>(this->m_overkill));
    uint8_t count = 0;
    msg.Get(count);
    const uint32_t maxCount = sizeof(this->m_schoolMask) / sizeof(this->m_schoolMask[0]);

    for (uint8_t i = 0; i < count; i++) {
        if (i < maxCount) {
            msg.Get(this->m_schoolMask[i]);
            msg.Get(this->m_subDamage[i]);
            msg.Get(this->m_subDamageInt[i]);
        } else {
            uint32_t schoolMask;
            float subDamage;
            uint32_t subDamageInt;
            msg.Get(schoolMask);
            msg.Get(subDamage);
            msg.Get(subDamageInt);
        }
    }

    if (this->m_hitInfo & (HIT_INFO_FLAG_FULL_ABSORB | HIT_INFO_FLAG_PARTIAL_ABSORB)) {
        for (uint8_t i = 0; i < count; i++) {
            uint32_t absorbed;
            msg.Get(i < maxCount ? this->m_absorbed[i] : absorbed);
        }
    }

    if (this->m_hitInfo & (HIT_INFO_FLAG_FULL_RESIST | HIT_INFO_FLAG_PARTIAL_RESIST)) {
        for (uint8_t i = 0; i < count; i++) {
            uint32_t resisted;
            msg.Get(i < maxCount ? this->m_resisted[i] : resisted);
        }
    }

    uint8_t victimState = 0;
    msg.Get(victimState);
    this->m_victimState = static_cast<VICTIMSTATES>(victimState);
    msg.Get(this->unk_0058);
    msg.Get(this->m_meleeSpellId);

    if (this->m_hitInfo & HIT_INFO_FLAG_BLOCK) {
        msg.Get(this->m_blocked);
    }

    if (this->m_hitInfo & HIT_INFO_FLAG_RAGE_GAIN) {
        msg.Get(this->m_rageGained);
    }

    if (this->m_hitInfo & HIT_INFO_FLAG_DEBUG_DATA) {
        msg.Get(this->unk_0068);

        for (uint32_t i = 0; i < 8; i++) {
            msg.Get(this->unk_006C[i]);
        }

        for (uint32_t i = 0; i < MAX_DAMAGE_TYPES; i++) {
            msg.Get(this->unk_0024[i]);
            msg.Get(this->unk_002C[i]);
        }

        msg.Get(this->unk_008C[0]);
    }

    if (ClntObjMgrObjectPtr(this->m_target, TYPE_UNIT, __FILE__, __LINE__) && !this->m_damage && this->m_blocked) {
        this->m_victimState = VS_BLOCK;
    }
}

// VERIFY: 49638f19 manual
// OFFSET: 0x00755850
void CGUnit_C::PlayCombatSpellMissEffects(uint32_t reason, const WOWGUID& source) {
    if ((this->m_unit->m_unitFieldFlags2 & UNIT_FLAG2_FEIGN_DEATH) || this->GetClientStandState() == UNIT_DEAD) {
        return;
    }

    CGUnit_C* attacker = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(source, TYPE_UNIT, __FILE__, __LINE__));
    const CGUnitVirtualItem* weapon = nullptr;

    if (attacker && attacker->m_sheatheState) {
        const CGUnitVirtualItem* item = attacker->GetVirtualItem(VIRTUAL_MONSTER_SLOT_MAINHAND, false);
        weapon = item && item->m_classID == ITEM_CLASS_WEAPON ? item : nullptr;
    }

    WOWGUID mover = CGUnit_C::s_activeMover;

    switch (reason) {
        case MISS_DODGED:
            this->PlayBaseAnimation(ANIM_DODGE, PLAY_NONE);
            break;

        case MISS_PARRIED: {
            this->PlayParryAnimation();
            C3Vector position = this->GetPosition();
            SI2::PlayParrySound(weapon, this->GetParryingItem(false), 0, &position, mover == source);
            break;
        }

        case MISS_BLOCKED: {
            this->PlayBaseAnimation(ANIM_SHIELD_BLOCK, PLAY_NONE);
            this->GetVirtualItem(VIRTUAL_MONSTER_SLOT_MAINHAND, false);
            const CGUnitVirtualItem* shield = this->GetVirtualItem(VIRTUAL_MONSTER_SLOT_OFFHAND, false);

            if (shield && shield->m_classID != ITEM_CLASS_WEAPON && shield->m_classID != ITEM_CLASS_ARMOR) {
                shield = nullptr;
            }

            C3Vector position = this->GetPosition();
            SI2::PlayParrySound(nullptr, shield, 0, &position, mover == source);
            break;
        }

        default:
            break;
    }
}

// VERIFY: 8b89f8b0 manual
// OFFSET: 0x00755A60
void CGUnit_C::PlayVictimWorldFeedback(const ATTACKROUNDINFO& info) {
    WOWGUID activePlayer = ClntObjMgrGetActivePlayer();

    if (info.m_meleeSpellId && info.m_victimState != VS_WOUND) {
        return;
    }

    VICTIMSTATES victimState = info.m_victimState;

    if (victimState == VS_DEFLECT) {
        WOWGUID source = info.m_attacker;
        this->AddWorldText(source, MISS_DEFLECTED, nullptr, false);
        CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
        return;
    }

    if (victimState == VS_PARRY) {
        WOWGUID source = info.m_attacker;
        this->AddWorldText(source, MISS_PARRIED, nullptr, false);
        CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
        return;
    }

    if (victimState == VS_EVADE) {
        WOWGUID source = info.m_attacker;
        this->AddWorldText(source, MISS_EVADED, nullptr, false);
        CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
        return;
    }

    if (victimState == VS_DODGE) {
        if (info.m_attacker.m_guid) {
            CGUnit_C* attacker = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(info.m_attacker, TYPE_UNIT, __FILE__, __LINE__));

            if (attacker) {
                bool oneHand = !CGUnit_C::IsTwoHanded(attacker->GetVirtualItem(VIRTUAL_MONSTER_SLOT_MAINHAND, false));
                int32_t self = activePlayer == info.m_attacker || activePlayer == info.m_target;
                C3Vector position = attacker->GetPosition();
                SI2::PlayMissedSound(oneHand, &position, self);
            }
        }

        WOWGUID source = info.m_attacker;
        this->AddWorldText(source, MISS_DODGED, nullptr, false);
        CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
        return;
    }

    if (victimState == VS_BLOCK) {
        WOWGUID source = info.m_attacker;
        this->AddWorldText(source, MISS_BLOCKED, nullptr, false);
        CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
        return;
    }

    if (victimState == VS_IMMUNE) {
        WOWGUID source = info.m_attacker;
        this->AddWorldText(source, MISS_IMMUNE, nullptr, false);
        CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
        return;
    }

    if (info.m_damage || (info.m_hitInfo & HIT_INFO_FLAG_FORCE_DAMAGE_EFFECTS)) {
        CGUnit_C* victim = nullptr;
        CGUnit_C* attacker = nullptr;

        if (info.m_attacker.m_guid) {
            attacker = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(info.m_attacker, TYPE_UNIT, __FILE__, __LINE__));
        }

        if (info.m_target.m_guid) {
            victim = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(info.m_target, TYPE_UNIT, __FILE__, __LINE__));
        }

        if (attacker && victim) {
            int32_t self = activePlayer == info.m_attacker || activePlayer == info.m_target;
            C3Vector position = victim->GetPosition();
            SI2::PlayHitSound(attacker->GetAttackingWeapon(static_cast<COMBATHAND>((info.m_hitInfo >> 2) & 1), true), victim->GetImpactType(), info.m_hitInfo & 0x200, &position, self);
        }

        WOWGUID source = info.m_attacker;
        this->AddWorldDamageText(source, info.m_damage, 0, (info.m_hitInfo & HIT_INFO_FLAG_CRITICAL) != 0, false, 0);
        CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
        return;
    }

    if (activePlayer != info.m_target && !(info.m_hitInfo & HIT_INFO_FLAG_SUPPRESS_MISS_WORLD_TEXT)) {
        MISS_REASON reason;

        if (info.m_hitInfo & HIT_INFO_FLAG_FULL_ABSORB) {
            reason = MISS_ABSORBED;
        } else {
            reason = (info.m_hitInfo & HIT_INFO_FLAG_FULL_RESIST) ? MISS_RESIST : MISS_PHYSICAL;
        }

        WOWGUID source = info.m_attacker;
        this->AddWorldText(source, reason, nullptr, false);
    }

    if (info.m_attacker.m_guid) {
        CGUnit_C* attacker = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(info.m_attacker, TYPE_UNIT, __FILE__, __LINE__));

        if (attacker) {
            bool oneHand = !CGUnit_C::IsTwoHanded(attacker->GetVirtualItem(VIRTUAL_MONSTER_SLOT_MAINHAND, false));
            int32_t self = activePlayer == info.m_attacker || activePlayer == info.m_target;
            C3Vector position = attacker->GetPosition();
            SI2::PlayMissedSound(oneHand, &position, self);
            CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
            return;
        }
    }

    CGGameUI::ShowCombatFeedbackAttackRoundInfo(&info);
}

// VERIFY: 6ec92b3f manual
// OFFSET: 0x00756180
void CGUnit_C::ClearAttackRound() {
    if (!this->m_lastAttackRound.m_attacker.m_guid || !this->m_lastAttackRound.m_target.m_guid) {
        return;
    }

    CGUnit_C* target = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(this->m_lastAttackRound.m_target, TYPE_UNIT, __FILE__, __LINE__));

    if (target) {
        target->PlayVictimWorldFeedback(this->m_lastAttackRound);
    }

    memset(&this->m_lastAttackRound, 0, sizeof(ATTACKROUNDINFO));
}

// VERIFY: 7ce6616c manual
// OFFSET: 0x00756240
void CGUnit_C::HandleCombatAnimEvent(uint32_t eventId, uint32_t data, const C3Vector* position, uint32_t time) {
    CGUnit_C* attacker = nullptr;
    CGUnit_C* target = nullptr;

    if (this->m_lastAttackRound.m_attacker) {
        attacker = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(this->m_lastAttackRound.m_attacker, TYPE_UNIT, __FILE__, __LINE__));
    }

    if (this->m_lastAttackRound.m_target) {
        target = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(this->m_lastAttackRound.m_target, TYPE_UNIT, __FILE__, __LINE__));
    }

    switch (eventId) {
        case PlayCustomAttackSound_AH0:
        case PlayCustomAttackSound_AH1:
        case PlayCustomAttackSound_AH2:
        case PlayCustomAttackSound_AH3:
            if (this->m_lastAttackRound.m_damage && attacker && attacker->m_soundData) {
                SE2SoundKitProperties properties;
                properties.ResetToDefaults();
                properties.m_group = SI2_SOUND_GROUP_IMPACT;

                switch (eventId) {
                    case PlayCustomAttackSound_AH0:
                        SI2::PlaySoundKit(attacker->m_soundData->m_customAttack[0], position, nullptr, &properties, 0, nullptr, 1, 0);
                        break;

                    case PlayCustomAttackSound_AH1:
                        SI2::PlaySoundKit(attacker->m_soundData->m_customAttack[1], position, nullptr, &properties, 0, nullptr, 1, 0);
                        break;

                    case PlayCustomAttackSound_AH2:
                        SI2::PlaySoundKit(attacker->m_soundData->m_customAttack[2], position, nullptr, &properties, 0, nullptr, 1, 0);
                        break;

                    case PlayCustomAttackSound_AH3:
                        SI2::PlaySoundKit(attacker->m_soundData->m_customAttack[3], position, nullptr, &properties, 0, nullptr, 1, 0);
                        break;
                }

                this->m_lastAttackRound.m_hitInfo |= HIT_INFO_FLAG_UNKNOWN_21;
            }

        case ResolveAttackRound_CAH:
            if (attacker) {
                VICTIMSTATES victimState = attacker->m_lastAttackRound.m_victimState;

                if (!victimState || victimState == VS_DODGE || victimState == VS_EVADE) {
                    attacker->SetKeyBoneSequenceSpeed(attacker->m_worldModel, 0xFFFFFFFF, 0.5f, 0);
                }
            } else if (!target) {
                if (this->m_obj->m_type & TYPE_PLAYER) {
                    static_cast<CGPlayer_C*>(this)->HandleSpellEventSound();
                } else {
                    CreatureSoundDataRec* soundData = this->GetSoundData();

                    if (soundData) {
                        SI2::PlaySoundKit(soundData->m_soundExertionID, position, nullptr, nullptr, 0, nullptr, 0, 0);
                    }
                }
            }

            if (target) {
                target->PlayVictimRound(this->m_lastAttackRound);
            }

            this->m_lastAttackRound.m_attacker = WOWGUID();
            this->m_lastAttackRound.m_target = WOWGUID();
            return;

        case PlayWeaponSwooshSound_CSS: {
            if (!attacker) {
                return;
            }

            VICTIMSTATES victimState = this->m_lastAttackRound.m_victimState;

            if (!victimState || victimState == VS_DODGE || victimState == VS_EVADE) {
                return;
            }

            WEAPONSWING_SOUNDTYPES swingType = WEAPONSWING_LIGHT;

            if (this->GetWeaponSwingType(!(this->m_lastAttackRound.m_hitInfo & HIT_INFO_FLAG_OFFHAND), &swingType) && this->m_lastAttackRound.m_victimState) {
                int32_t self = CGUnit_C::s_activeMover == this->m_lastAttackRound.m_attacker || CGUnit_C::s_activeMover == this->m_lastAttackRound.m_target;
                SI2::PlayWeaponSwooshSound(swingType, this->m_lastAttackRound.m_hitInfo & 0x200, position, this->m_lastAttackRound.m_hitInfo & 0x10, self);
            }

            return;
        }

        case RangedWeaponPull_BWP: {
            if (this->IsActivePlayer()) {
                SpellRec spell;

                if (!g_spellDB.GetRecord(this->m_castSpellId, &spell) || !(spell.m_attributes & SPELL_ATTR0_USES_RANGED_SLOT)) {
                    if (!g_spellDB.GetRecord(Spell_C_GetAutoRepeatingSpell(), &spell) || !(spell.m_attributes & SPELL_ATTR0_USES_RANGED_SLOT)) {
                        return;
                    }
                }
            }

            if (this->m_movementData.m_flags & MOVEMENTFLAG_LOCOMOTION_MASK) {
                return;
            }

            this->PlayRangedItemPull();
            const CGUnitVirtualItem* item = this->GetVirtualItem(VIRTUAL_MONSTER_SLOT_RANGED, false);

            if (!item || item->m_classID != ITEM_CLASS_WEAPON || (item->m_subclassID != ITEM_SUBCLASS_WEAPON_GUN && item->m_subclassID != ITEM_SUBCLASS_WEAPON_CROSSBOW)) {
                if (this->m_ammoModel && !this->m_ammoModel->m_attachParent) {
                    this->m_ammoModel->AttachToParent(this->m_worldModel, ATTACH_HANDARROW, nullptr, 0);
                }
            }

            this->m_animationState |= UNIT_ANIM_FLAG_RANGED_WEAPON_LOADED;
            return;
        }

        case Death_DTH:
            this->PlayDeathThud();
            this->ShakeCameraOnHit();
            this->ShowLootEffect();
            return;

        case CombatDefense_CPP: {
            if (!target || (target->m_unit->m_unitFieldFlags2 & UNIT_FLAG2_FEIGN_DEATH) || target->GetClientStandState() == UNIT_DEAD) {
                return;
            }

            VICTIMSTATES victimState = this->m_lastAttackRound.m_victimState;

            if (victimState == VS_PARRY) {
                if (!target->m_creatureCacheEntry || !(target->m_creatureCacheEntry->m_typeFlags & CREATURE_TYPE_FLAG_NO_DODGE_PARRY_BLOCK_ANIMS)) {
                    target->PlayParryAnimation();
                }

                this->m_lastAttackRound.m_hitInfo &= ~HIT_INFO_FLAG_AFFECTS_VICTIM;
            } else if (victimState == VS_DODGE || victimState == VS_DEFLECT) {
                if (!target->m_creatureCacheEntry || !(target->m_creatureCacheEntry->m_typeFlags & CREATURE_TYPE_FLAG_NO_DODGE_PARRY_BLOCK_ANIMS)) {
                    target->PlayBaseAnimation(ANIM_DODGE, PLAY_NONE);
                }

                this->m_lastAttackRound.m_hitInfo &= ~HIT_INFO_FLAG_AFFECTS_VICTIM;
            } else if (this->m_lastAttackRound.m_blocked) {
                if (!target->m_creatureCacheEntry || !(target->m_creatureCacheEntry->m_typeFlags & CREATURE_TYPE_FLAG_NO_DODGE_PARRY_BLOCK_ANIMS)) {
                    target->PlayBaseAnimation(ANIM_SHIELD_BLOCK, PLAY_NONE);
                }

                this->m_lastAttackRound.m_hitInfo &= ~HIT_INFO_FLAG_AFFECTS_VICTIM;
            }

            return;
        }

        default:
            return;
    }
}

// VERIFY: a8a61e30 manual
// OFFSET: 0x00756770
void CGUnit_C::OnAttackStop(uint64_t target, int32_t faceTarget) {
    this->m_combat.m_attacking = WOWGUID{ 0 };
    this->m_combat.m_attackSent = 0;
    this->m_stopAttackSent = 0;
    this->ClearAttackRound();

    if (!target || !faceTarget) {
        return;
    }

    CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(WOWGUID{ target }, TYPE_UNIT, __FILE__, __LINE__));

    if (unit) {
        this->m_clientFlags |= UNIT_CLIENT_FLAG_SCRIPTED_FACING;
        C3Vector targetPosition = unit->GetPosition();
        C3Vector position = this->GetPosition();
        this->m_scriptedFacing = CalculateFacingTo(&position, &targetPosition);
    }
}

// VERIFY: 161e4292 manual
// OFFSET: 0x00756800
int32_t Packet_Group_29(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    switch (msgId) {
        case SMSG_ATTACK_START: {
            WOWGUID attacker;
            WOWGUID victim;
            msg->Get(attacker.m_guid);
            msg->Get(victim.m_guid);
            CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(attacker, TYPE_UNIT, __FILE__, __LINE__));

            if (!unit) {
                return 0;
            }

            unit->m_combat.m_attackSent = 0;
            unit->SetAttacking(victim);

            if (!unit->IsActivePlayer()) {
                return 1;
            }

            FrameScript_SignalEvent(SCRIPT_EVENT_PLAYER_ENTER_COMBAT, nullptr);
            CGGameUI::UpdateCombatMode();
            return 1;
        }
        case SMSG_ATTACK_STOP:
        case SMSG_COMBAT_EVENT_FAILED: {
            SmartGUID attacker;
            SmartGUID victim;
            *msg >> attacker;
            *msg >> victim;
            uint32_t faceTarget = 0;
            msg->Get(faceTarget);
            CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(attacker, TYPE_UNIT, __FILE__, __LINE__));

            if (unit) {
                unit->OnAttackStop(victim.m_guid, faceTarget);

                if (unit->IsActivePlayer()) {
                    FrameScript_SignalEvent(SCRIPT_EVENT_PLAYER_LEAVE_COMBAT, nullptr);
                    CGGameUI::UpdateCombatMode();
                }
            }

            return 1;
        }
        case SMSG_ATTACKSWING_NOTINRANGE:
            SetAttackSwingError(1);
            return 0;
        case SMSG_ATTACKSWING_BADFACING:
            SetAttackSwingError(2);
            return 0;
        case SMSG_ATTACKSWING_DEADTARGET:
        case SMSG_ATTACKSWING_CANT_ATTACK: {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

            if (!player) {
                return 0;
            }

            player->SetCombatMode();
            return 0;
        }
        case SMSG_ATTACKER_STATE_UPDATE: {
            ATTACKROUNDINFO info;
            info.Unpack(*msg);
            CGUnit_C* attacker = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(info.m_attacker, TYPE_UNIT, __FILE__, __LINE__));

            if (attacker) {
                if (attacker->m_sheatheState != WEAPONMODE_NORMALMODE) {
                    attacker->SetSheatheState(WEAPONMODE_NORMALMODE, 1, 0);
                }

                attacker->m_combat.m_attackSent = 0;
                attacker->ClearAttackRound();
                attacker->m_lastAttackRound = info;
                CGUnit_C* victim = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(info.m_target, TYPE_UNIT, __FILE__, __LINE__));

                if (attacker->IsActivePlayer() && victim) {
                    static_cast<CGPlayer_C*>(attacker)->UpdateAttackPosition(victim);
                }

                attacker->PlayAttackerRound(info);

                if (attacker->m_lastAttackRound.m_target == ClntObjMgrGetActivePlayer()) {
                    WOWGUID guid = attacker->m_obj->m_guid;
                    TargetAttacker(guid);
                }

                if (attacker->IsAutoTracking() && !attacker->IsActiveMoverTracking(4)) {
                    attacker->ClearTrackingTarget(true, true);
                }
            } else {
                CGUnit_C* victim = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(info.m_target, TYPE_UNIT, __FILE__, __LINE__));

                if (victim) {
                    victim->PlayVictimRound(info);
                }
            }

            UnitCombatLog(info);
            return 0;
        }
        case SMSG_ENVIRONMENTAL_DAMAGE_LOG: {
            uint64_t guid = 0;
            msg->Get(guid);
            uint8_t type = 0;
            msg->Get(type);
            uint32_t damage = 0;
            msg->Get(damage);
            uint32_t absorbed = 0;
            msg->Get(absorbed);
            uint32_t resisted = 0;
            msg->Get(resisted);

            if (type >= sizeof(s_environmentalDamageSchools) / sizeof(s_environmentalDamageSchools[0])) {
                return 1;
            }

            CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(WOWGUID{ guid }, TYPE_UNIT, __FILE__, __LINE__));

            if (unit) {
                unit->HandleEnvironmentDamage(type, damage, absorbed, resisted);
            }

            return 1;
        }
        case SMSG_CLEAR_TARGET: {
            uint64_t guid = 0;
            msg->Get(guid);
            CGGameUI::ClearTarget(WOWGUID{ guid }, 1);
            return 0;
        }
        default:
            return 0;
    }
}
