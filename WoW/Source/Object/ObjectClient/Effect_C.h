#ifndef WOW_SOURCE_OBJECT_OBJECTCLIENT_EFFECT_C_H
#define WOW_SOURCE_OBJECT_OBJECTCLIENT_EFFECT_C_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"

class C44Matrix;
class CGObject_C;
class CGUnit_C;
class CMapEntity;
class LightningObject;
class MountTransitionObject;
class CM2Lighting;
class CM2Model;
class SpellVisualEffectNameRec;
class SpellVisualKitModelAttachRec;
class SpellVisualKitRec;

enum UNITEFFECTSPECIALS : int32_t {
    SPECIALEFFECT_FOOTSTEPSPRAYWATER = 0,
    SPECIALEFFECT_FOOTSTEPSPRAYWATERWALK = 1,
    SPECIALEFFECT_UNDERWATERBUBBLES = 2,
    SPECIALEFFECT_COLDBREATH = 3,
    SPECIALEFFECT_LOOTART = 4,
    SPECIALEFFECT_LEVELUP = 5,
    SPECIALEFFECT_MOUNTPOOF = 6,
    SPECIALEFFECT_INEBRIATEDBUBBLES = 7,
    SPECIALEFFECT_MEETINGSTONEJOIN = 8,
    SPECIALEFFECT_REPUTATION = 9,
    SPECIALEFFECT_RESISTSPELL = 10,
    SPECIALEFFECT_ACHIEVEMENTBASE = 11,
    NUM_UNITEFFECTSPECIALS = 12,
};

class CEffect {
    public:

    enum EFFECT_FLAGS : uint32_t {
        EFFECT_NONE                                  = 0x00000000u,
        EFFECT_SOUND_LOOPING                         = 0x00000001u,
        EFFECT_WORLD_SPACE                           = 0x00000002u,
        EFFECT_FREEZE_ANIMATION                      = 0x00000004u,
        EFFECT_LOOT_ART                              = 0x00000008u,
        EFFECT_PLAY_CAMERA_SHAKE                     = 0x00000010u,
        EFFECT_CALLBACK_LIFETIME                     = 0x00000020u,
        EFFECT_PRECAST                               = 0x00000040u,
        EFFECT_SET_SPECIAL_IDLE                      = 0x00000080u,
        EFFECT_DESTROY_PENDING                       = 0x00000100u,
        EFFECT_SCALE_TO_TARGET                       = 0x00000200u,
        EFFECT_SUPPRESS_SOUND                        = 0x00000400u,
        EFFECT_POST_INITIALIZED                      = 0x00000800u,
        EFFECT_AURA_ANIMATION                        = 0x00001000u,
        EFFECT_USE_SUPPLIED_POSITION                 = 0x00002000u,
        EFFECT_FOLLOW_SOURCE_POSITION                = 0x00004000u,
        EFFECT_FIXED_FACING                          = 0x00008000u,
        EFFECT_ATTACHMENT_OFFSET                     = 0x00010000u,
        EFFECT_PERSIST_ON_PARTIAL_CLEAR              = 0x00020000u,
        EFFECT_LIGHTNING_ENDPOINT                  = 0x00040000u,
        EFFECT_ITEM_VISUAL                           = 0x00080000u,
        EFFECT_SOUND_ONLY                            = 0x00100000u,
        EFFECT_DETACHED_SOUND                        = 0x00200000u,
        EFFECT_IGNORE_EMISSION_DISTANCE              = 0x00400000u,
        EFFECT_MOUNT_AURA                            = 0x00800000u,
        EFFECT_RAW_ANIMATION_ID                      = 0x01000000u,
        EFFECT_IGNORE_PARENT_EFFECTIVE_ALPHA         = 0x02000000u,
    };

    void ApplyItemVisual();
    static void ApplyItemVisualByItemId(int32_t id, const WOWGUID& guid, void* param, bool success);
    static void MountTransitionCacheCB(int32_t id, const WOWGUID& guid, void* param, bool success);
    int32_t LoadMountTransitionModel();
    void ClearItemVisualEffect();
    void ClearLightning();
    C3Vector TransformPositionByTransport();
    static float __stdcall ComputeModelScaleFromTarget(CGObject_C* object);
    static void ApplyModelAttachRecToMatrix(const SpellVisualKitModelAttachRec* attach, C44Matrix& matrix, float facing);
    C44Matrix* TransformPosition(C44Matrix* matrix, CGObject_C* object);
    void PostInitialize();
    void UpdateWorldObject(float elapsed, uint32_t flags);
    void AddLootEffect(CGUnit_C* unit, void (*callback)(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param));
    static void CleanUpStaticLists();
    static void Destroy();
    static void DrawFishingString(CM2Model* model, const CM2Lighting* lighting, void* arg);

    /* 0000 */ CM2Model* m_model;
    /* 0004 */ CM2Model* m_lineModel;
    /* 0008 */ WOWGUID m_sourceGuid;
    /* 0010 */ WOWGUID m_targetGuid;
    /* 0018 */ uint32_t m_spellId;
    /* 001C */ const SpellVisualKitRec* m_kitRec;
    /* 0020 */ const SpellVisualEffectNameRec* m_effectNameRec;
    /* 0024 */ int32_t m_attachmentId;
    /* 0028 */ C3Vector m_offset;
    /* 0034 */ SE2SoundKitObject m_soundKitObject;
    /* 0048 */ uint32_t m_flags;
    /* 004C */ LightningObject* m_lightning [12];
    /* 007C */ uint32_t unused_007C;
    /* 0080 */ uint32_t unused_0080;
    /* 0084 */ uint32_t unused_0084;
    /* 0088 */ float m_facing;
    /* 008C */ int32_t m_lightningAttachment;
    /* 0090 */ MountTransitionObject* m_mountTransition;
    /* 0094 */ int32_t m_pendingCacheId;
    /* 0098 */ int32_t m_displayId;
    /* 009C */ int32_t m_itemSlot;
    /* 00A0 */ uint32_t m_fadeTime;
    /* 00A4 */ C3Vector m_position;
    /* 00B0 */ C3Vector m_worldPosition;
    /* 00BC */ uint32_t unused_00BC;
    /* 00C0 */ WOWGUID m_transportGuid;
    /* 00C8 */ uint32_t m_startTime;
    /* 00CC */ uint32_t m_expireTime;
    /* 00D0 */ uint32_t unk_00D0;
    /* 00D4 */ uint32_t m_attachToModel;
    /* 00D8 */ uint32_t m_attachToMount;
    /* 00DC */ uint32_t unused_00DC;
    /* 00E0 */ float unused_00E0;
    /* 00E4 */ float unused_00E4;
    /* 00E8 */ float m_alpha;
    /* 00EC */ CImVector m_color;
    /* 00F0 */ CMapEntity* m_mapEntity;
    /* 00F4 */ int32_t m_refCount;
    /* 00F8 */ uint32_t m_lightningRefCount;
    /* 00FC */ int32_t m_lightningState;
    /* 0100 */ const SpellVisualKitModelAttachRec* m_attachRec;
    /* 0104 */ CEffect** m_prevLink;
    /* 0108 */ CEffect* m_next;
    /* 010C */ uint32_t unused_010C;

    CEffect();
    ~CEffect();
    void AddFishingLine(CGObject_C* object, const WOWGUID& target);
    static void CreateModelFromEffect(CM2Model* model, void* arg);
    void AddAnimKitEffect(int32_t spellId, const SpellVisualKitRec* kit, const WOWGUID* guid, uint32_t flags, uint32_t fadeTime);
    void AddSoundEffect(int32_t soundId, int32_t expireTime, int32_t spellId, const C3Vector* position, CGObject_C* object, uint32_t fadeTime);
    static void __stdcall DestroyMatchingEffects(CGObject_C* object, const SpellVisualEffectNameRec* effectName, int32_t attachType, int32_t spellId, int32_t kitId, const SpellVisualKitModelAttachRec* attach);
    static void EffectEvent(CM2Model* model, uint32_t boneId, uint32_t eventId, uint32_t data, const C3Vector* position, uint32_t time, uint64_t param);
    void AddEffect(int32_t attachType, int32_t spellId, const SpellVisualKitRec* kit, const SpellVisualEffectNameRec* effectName, const WOWGUID* target, const WOWGUID* source, uint32_t flags, void (*callback)(CM2Model*, uint32_t, uint32_t, int32_t, uint32_t, uint64_t), const C3Vector* position, uint32_t a10, const SpellVisualKitModelAttachRec* attach);
    void InitFromUnit(C3Vector position, uint32_t time, int32_t spellId, const SpellVisualKitRec* kit, const SpellVisualEffectNameRec* effectName, const WOWGUID* guid, uint32_t flags, void (*callback)(CM2Model*, uint32_t, uint32_t, int32_t, uint32_t, uint64_t), uint32_t a9, const SpellVisualKitModelAttachRec* attach, WOWGUID transport);
    void AddSpecialEffect(int32_t type, const WOWGUID& target, void (*callback)(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param));
    void AddSpecialByPosition(int32_t type, C3Vector position, const WOWGUID* guid, uint32_t flags, void (*callback)(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param));
    void ConnectToLightningThisFrame();
    void DetachFromParent();
    int32_t UpdateAttachment();
    void InitLightningEndpoint();
    void LinkTo(CEffect** head);
    void AddLightning(int32_t expireTime, uint32_t spellId, const SpellVisualKitRec* kit, const WOWGUID* guid, uint32_t flags);
    void AddMountTransition(uint32_t spellId, const SpellVisualKitRec* kit, const WOWGUID* guid);
    void InitAtPosition(int32_t spellId, const SpellVisualKitRec* kit, const SpellVisualEffectNameRec* effectName, uint32_t flags, void (*callback)(CM2Model*, uint32_t, uint32_t, int32_t, uint32_t, uint64_t), const C3Vector* position, uint32_t a8, WOWGUID transport);
    void CreateProceduralEffectImpl(int32_t expireTime, float param, uint32_t spellId, const SpellVisualKitRec* kit, const WOWGUID* guid, uint32_t flags);
    void InitProceduralEffect(int32_t expireTime, int32_t param, uint32_t spellId, const SpellVisualKitRec* kit, const WOWGUID* guid, uint32_t flags);
    void MoveToDestroyList();
    void ReleaseLightningRef();
    void EffectUpdate();
    void MoveToOrphanList();
    void Release();
    void SetEmissionRateIgnoresDistance(int32_t ignore);
    void SetEnabled(int32_t enabled);
    void UnLinkSelf();
};

const SpellVisualEffectNameRec* GetSpecialSpellVisualEffectNameRec(int32_t effect);
int32_t SpecialUnitEffectsInitialize();

#endif
