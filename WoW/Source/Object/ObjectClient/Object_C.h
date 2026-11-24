#ifndef WOW_SOURCE_OBJECT_OBJECTCLIENT_OBJECT_C_H
#define WOW_SOURCE_OBJECT_OBJECTCLIENT_OBJECT_C_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/ObjectClient/Types.h"
#if defined(_WIN32)
#include <windows.h>
#endif
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Object/Object.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Ui/DBCACHEHASH_NAME.h"

class CObjectEffectManager;
class CTexture;
struct CVar;
class C3Vector;
class C44Matrix;
class C4Quaternion;
class CClientObjCreate;
class CEffect;
class CGBag_C;
class CGObject_C;
class CGWorldFrame;
class CHashKeyGUID;
class CImVector;
class CM2Model;
class CMapEntity;
class PlaySpellVisuaKitData;
class CPassenger;
class PLAYERNAMEDESC;
class SpellRec;
class SpellVisualKitRec;
class SpellVisualEffectNameRec;
class SpellVisualKitModelAttachRec;
class SpellVisualRec;

enum OUT_OF_RANGE_TYPE : int32_t {
    OUT_OF_RANGE_UPDATE = 0,
    OUT_OF_RANGE_DESTROYED = 1,
    OUT_OF_RANGE_UNLOCKED = 2,
};

enum SPELL_VISUAL_KIT_TYPE : int32_t {
    SPELL_VISUAL_KIT_CAST = 0,
    SPELL_VISUAL_KIT_IMPACT = 1,
    SPELL_VISUAL_KIT_STATE = 2,
    SPELL_VISUAL_KIT_AT_POSITION = 3,
    SPELL_VISUAL_KIT_PRECAST = 4,
    SPELL_VISUAL_KIT_CASTER_IMPACT = 5,
    SPELL_VISUAL_KIT_TARGET_IMPACT = 6,
    SPELL_VISUAL_KIT_MISSILE_TARGETING = 7,
    SPELL_VISUAL_KIT_STATE_DONE = 8,
};

enum OBJECT_MODEL_FLAGS : uint32_t {
    OBJECT_MODEL_FLAG_NONE                         = 0x00000000u,
    OBJECT_MODEL_LOCK_COUNT_MASK                   = 0x0000FFFFu,
    OBJECT_MODEL_FLAG_DISABLED                     = 0x00010000u,
    OBJECT_MODEL_FLAG_POST_REENABLE_PENDING        = 0x00020000u,
    OBJECT_MODEL_FLAG_POST_INITED                  = 0x00040000u,
    OBJECT_MODEL_FLAG_UNKNOWN_19                   = 0x00080000u,
    OBJECT_MODEL_FLAG_DISABLE_PENDING              = 0x00100000u,
    OBJECT_MODEL_FLAG_BOUNDS_DIRTY                 = 0x00200000u,
    OBJECT_MODEL_FLAG_PASSIVE_HIGHLIGHT            = 0x00400000u,
    OBJECT_MODEL_FLAG_COLLIDE_MOVEMENT             = 0x00800000u,
    OBJECT_MODEL_FLAG_HIGHLIGHT_OBJSELECTION       = 0x01000000u,
    OBJECT_MODEL_FLAG_HIGHLIGHT_MOUSEOVER          = 0x02000000u,
    OBJECT_MODEL_FLAG_UNKNOWN_26                   = 0x04000000u,
    OBJECT_MODEL_FLAG_UNKNOWN_27                   = 0x08000000u,
    OBJECT_MODEL_FLAG_UNKNOWN_28                   = 0x10000000u,
    OBJECT_MODEL_FLAG_UNKNOWN_29                   = 0x20000000u,
    OBJECT_MODEL_FLAG_UNKNOWN_30                   = 0x40000000u,
    OBJECT_MODEL_FLAG_UNKNOWN_31                   = 0x80000000u,
    OBJECT_MODEL_FLAG_HIGHLIGHT_MASK = OBJECT_MODEL_FLAG_HIGHLIGHT_OBJSELECTION
        | OBJECT_MODEL_FLAG_HIGHLIGHT_MOUSEOVER
        | OBJECT_MODEL_FLAG_UNKNOWN_26,
};
constexpr int32_t OBJECT_MODEL_HIGHLIGHT_TYPE_SHIFT = 24;

enum QUEST_GIVER_STATUS : uint32_t {
    QUEST_GIVER_NONE = 0,
    QUEST_GIVER_UNKNOWN_1 = 1,
    QUEST_GIVER_UNKNOWN_2 = 2,
    QUEST_GIVER_UNKNOWN_3 = 3,
    QUEST_GIVER_UNKNOWN_4 = 4,
    QUEST_GIVER_UNKNOWN_5 = 5,
    QUEST_GIVER_UNKNOWN_6 = 6,
    QUEST_GIVER_UNKNOWN_7 = 7,
    QUEST_GIVER_UNKNOWN_8 = 8,
    QUEST_GIVER_UNKNOWN_9 = 9,
    QUEST_GIVER_UNKNOWN_10 = 10,
    QUEST_GIVER_NUMITEMS = 11,
};

enum OBJECT_NAME_FLAGS : uint32_t {
    OBJECT_NAME_OWN = 0x1,
    OBJECT_NAME_NPC = 0x2,
    OBJECT_NAME_PLAYER_GUILD = 0x4,
    OBJECT_NAME_PLAYER_PVP_TITLE = 0x8,
    OBJECT_NAME_ENEMY_PLAYER = 0x10,
    OBJECT_NAME_ENEMY_PET = 0x20,
    OBJECT_NAME_ENEMY_TOTEM = 0x40,
    OBJECT_NAME_FRIENDLY_PLAYER = 0x80,
    OBJECT_NAME_FRIENDLY_PET = 0x100,
    OBJECT_NAME_FRIENDLY_TOTEM = 0x200,
    OBJECT_NAME_NON_COMBAT_CREATURE = 0x400,
    OBJECT_NAME_UNKNOWN_0x800 = 0x800,
    OBJECT_NAME_ENEMY_GUARDIAN = 0x1000,
    OBJECT_NAME_FRIENDLY_GUARDIAN = 0x2000,
};

enum OUT_OF_RANGE_TYPE : int32_t;
enum SPELL_VISUAL_KIT_TYPE : int32_t;

class CHashKeyGUID {
    public:
    /* 0000 */ WOWGUID m_guid;

    CHashKeyGUID();
    CHashKeyGUID(uint64_t guid);
    bool operator==(const CHashKeyGUID& key);
};

// INLINED
inline CHashKeyGUID::CHashKeyGUID() {
    this->m_guid.m_guid = 0;
}

// INLINED
inline CHashKeyGUID::CHashKeyGUID(uint64_t guid) {
    this->m_guid.m_guid = guid;
}

// INLINED
inline bool CHashKeyGUID::operator==(const CHashKeyGUID& key) {
    return this->m_guid == key.m_guid;
}

class CGObject_C : public CGObject, public TSHashObject<CGObject_C, CHashKeyGUID> {
    public:
    enum INTERACT_ICON : uint32_t {
        INTERACT_ICON_NONE = 0,
        INTERACT_ICON_TALK_TO_ME = 1,
        INTERACT_ICON_QUESTION_LTBLUE = 2,
        INTERACT_ICON_BLUE = 3,
        INTERACT_ICON_TALK_TO_ME_4 = 4,
        INTERACT_ICON_QUESTION_GREY = 5,
        INTERACT_ICON_GREY = 6,
        INTERACT_ICON_GREEN = 7,
        INTERACT_ICON_UNKNOWN_8 = 8,
        INTERACT_ICON_QUESTION_MARK = 9,
        INTERACT_ICON_QUESTION_LTBLUE_10 = 10,
        INTERACT_ICON_BLUE_11 = 11,
        INTERACTICON_NUMITEMS = 12,
    };

    struct InteractIcon {
        /* 0000 */ CM2Model* m_model;
        /* 0004 */ float m_scale;
        /* 0008 */ const char* m_fileName;
    };

    static InteractIcon s_interactIcons[INTERACTICON_NUMITEMS];
    static const uint32_t s_questStatusIcons[QUEST_GIVER_NUMITEMS];
    static CTexture* s_selectionTexture;
    static CVar* s_cvObjectSelectionCircle;

    static void Initialize();
    static void ModelLoadedCallback(CM2Model* model, void* arg);
    static void Shutdown();
    static int32_t UpdateAllObjectEffectManagers();
    static CGObject_C* GetFocusObject();
    static void PrepareToFadeInt(CMapEntity* entity, float alpha, uint64_t transportGuid);
    static int32_t UpdateAllWorldObjects();
    static void LoopEffectHandler(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param);
    static void EffectOnceHandler(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param);
    static void EffectDecayHandler(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param);
    static void AuraDecayHandler(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param);
    static void AuraEffectHoldHandler(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param);
    static void AuraEffectHandler(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param);

    /* 0038 */ TSLink<void> m_link;
    /* 0040 */ uint32_t m_disableTime;
    /* 0044 */ TSList<void> m_objMirrorLists [6];
    /* 008C */ CM2Model* m_interactIcon;
    /* 0090 */ uint32_t m_questStatus;
    /* 0094 */ uint32_t m_interactIconType;
    /* 0098 */ float m_scale;
    /* 009C */ float m_scaleModifier;
    /* 00A0 */ uint32_t m_scaleChangeTime;
    /* 00A4 */ float m_scaleChangeStart;
    /* 00A8 */ CEffect* m_effectList;
    /* 00AC */ float m_height;
    /* 00B0 */ PLAYERNAMEDESC* m_nameDesc;
    /* 00B4 */ CM2Model* m_worldModel;
    /* 00B8 */ CMapEntity* m_worldObject;
    /* 00BC */ uint32_t m_modelFlags;
    /* 00C0 */ uint32_t m_fadeStartTime;
    /* 00C4 */ uint32_t m_fadeDuration;
    /* 00C8 */ uint8_t m_alpha;
    /* 00C9 */ uint8_t m_fadeStartAlpha;
    /* 00CA */ uint8_t m_fadeEndAlpha;
    /* 00CB */ uint8_t m_alphaScale;
    /* 00CC */ CObjectEffectManager* m_effectManager;

    virtual ~CGObject_C();
    virtual void Disable();
    virtual void Reenable();
    virtual void PostReenable();
    virtual void HandleOutOfRange(OUT_OF_RANGE_TYPE type);
    virtual void UpdateWorldObject(int32_t a2);
    virtual int32_t ShouldFadeout();
    virtual void UpdateDisplayInfo(int32_t force);
    virtual C3Vector GetNamePosition() const;
    virtual CGBag_C* GetBag();
    virtual const CGBag_C* GetBag() const;
    virtual C3Vector GetPosition() const;
    virtual C3Vector GetRawPosition() const;
    virtual float GetFacing() const;
    virtual float GetRawFacing() const;
    virtual float GetScale() const;
    virtual uint64_t GetTransportGUID() const;
    virtual C4Quaternion GetRotation() const;
    virtual void SetFrameOfReference(const C44Matrix* matrix);
    virtual int32_t IsQuestGiver() const;
    virtual void RefreshInteractIcon();
    virtual void UpdateInteractIcon();
    virtual void UpdateInteractIconAttach();
    virtual void UpdateInteractIconScale();
    virtual int32_t GetModelFileName(const char** name) const;
    virtual void ScaleChangeUpdate();
    virtual void ScaleChangeFinished();
    virtual void RenderTargetSelection() const;
    virtual void RenderPetTargetSelection() const;
    virtual void Render();
    virtual int32_t GetSelectionHighlightColor(CImVector* color) const;
    virtual float GetTrueScale() const;
    virtual void ModelLoaded(CM2Model* model);
    virtual void ApplyAlpha(uint32_t time);
    virtual void PreAnimate(CGWorldFrame* frame);
    virtual int32_t Animate(float elapsed);
    virtual void ShouldRender(uint32_t flags, int32_t& render, int32_t& hidden);
    virtual float GetRenderFacing() const;
    virtual void OnSpecialMountAnim();
    virtual int32_t IsSolidSelectable() const;
    virtual int32_t IsCollidable(int32_t flags) const;
    virtual int32_t CanHighlight() const;
    virtual int32_t CanBeTargetted() const;
    virtual int32_t FloatingTooltip() const;
    virtual void OnRightClick();
    virtual int32_t IsHighlightSuppressed() const;
    virtual void OnSpellEffectClear(CEffect* effect);
    virtual const SpellVisualRec* GetAppropriateSpellVisual(const SpellRec* spell, SpellVisualRec& visual, int32_t visualId, int32_t lowViolenceVisualId) const;
    virtual void ConnectToLightningThisFrame(uint32_t spellId, int32_t attachment);
    virtual C44Matrix GetMatrix() const;
    virtual void ObjectNameVisibilityChanged();
    virtual int32_t UpdateObjectNameString(int32_t flags, char* buffer, uint32_t size);
    virtual int32_t ShouldRenderObjectName(uint32_t flags) const;
    virtual CM2Model* GetObjectModel() const;
    virtual const char* GetObjectName() const;
    virtual int32_t GetPageTextID(void (*callback)(int32_t, const WOWGUID&, void*, bool)) const;
    virtual void CleanUpVehicleBoneAnimsBeforeObjectModelChange();
    virtual int32_t ShouldFadeIn() const;
    virtual float GetBaseAlpha() const;
    virtual bool IsTransport() const;
    virtual int32_t IsPointInside(const C3Vector& point) const;
    virtual int32_t AddPassenger(CPassenger* passenger, int32_t a2);
    virtual float GetSpeed() const;
    virtual void PlaySpellVisualKit_PlayAnims(const SpellRec* spell, const SpellVisualKitRec* kit, SPELL_VISUAL_KIT_TYPE type, const C3Vector* position, int32_t a5, int32_t a6, WOWGUID guid, uint32_t a8, const SpellVisualRec* visual, uint32_t a10, int32_t a11);
    virtual void PlaySpellVisualKit_HandleWeapons(const SpellRec* spell, const SpellVisualKitRec* kit, const C3Vector* position, WOWGUID guid, uint32_t a5, uint32_t flags, void (*callback)(CM2Model*, uint32_t, uint32_t, int32_t, uint32_t, uint64_t), int32_t* hasEffect);
    virtual int32_t PlaySpellVisualKit_DelayLightningEffects(const SpellRec* spell, const SpellVisualKitRec* kit, SPELL_VISUAL_KIT_TYPE type, const C3Vector* position, WOWGUID guid, int32_t a6, int32_t a7, int32_t a8, int32_t a9, uint32_t a10, const SpellVisualRec* visual);

    CGObject_C();
    CGObject_C(uint32_t time, const CClientObjCreate& create);
    void ClearEffectList(int32_t all);
    void ClearSpellEffect(uint32_t spellId, int32_t all);
    void DoFade(float alpha, uint32_t time);
    void PlayEffectName(int32_t attachType, uint32_t a3, int32_t spellId, const SpellVisualKitRec* kit, const SpellVisualEffectNameRec* effectName, uint32_t* flags, void (*callback)(CM2Model*, uint32_t, uint32_t, int32_t, uint32_t, uint64_t), const C3Vector* position, WOWGUID target, uint32_t a11, const SpellVisualKitModelAttachRec* attach);
    C44Matrix GetModelMatrix();
    C3Vector GetModelPosition();
    int32_t GetQuestGiverCursor(int32_t highlighted);
    void HandleScaleUpdate(float scale);
    void HideHighlightType(HIGHLIGHTTYPE type);
    void AddWorldObject();
    int32_t IsObjectLocked();
    int32_t IsObjectModelLoaded();
    int32_t IsReadyToDraw();
    void ModelChanged();
    void PostInit(uint32_t time, const CClientObjCreate& create, bool a3);
    void ReleaseModel();
    void RemoveInteractIcon();
    void SetBlock(int32_t index, uint32_t value);
    void SetBoundsDirty();
    void SetDisablePending(int32_t pending);
    void SetInteractIconVisible(int32_t visible);
    CM2Model* SetModelStart(CM2Model* model);
    void SetObjectLocked(int32_t locked);
    void SetQuestStatus(uint32_t status);
    void SetStorage(uint32_t* storage, uint32_t* saved);
    void SetTypeID(OBJECT_TYPE_ID typeId);
    void ShowHighlightType(HIGHLIGHTTYPE type);
    void UpdateEffectAttachments();
    void Fadeout();
    static void __stdcall BuildSelectionRotMatrix(C44Matrix* matrix, const C3Vector* position);
    static int32_t SetCircleRenderStates();
    void SetModelFinish(CM2Model* model);
    void SetModel(CM2Model* model);
    void PlaySpellVisualKit(const PlaySpellVisuaKitData& data);
    void ClearEffectKit(int32_t spellId, int32_t kitType, int32_t a4, int32_t a5, int32_t a6);
};

struct DBCACHEHASH_NAME : public TSHashObject<DBCACHEHASH_NAME, CHashKeyGUID> {
    /* 0020 */ uint8_t m_record [336];
    /* 0170 */ DBCACHETRAILER_GUID m_trailer;
};

namespace World {
    int32_t QueryObjectVisible(CMapEntity* entity);
    void TickObject(CMapEntity* entity);
}

#endif
