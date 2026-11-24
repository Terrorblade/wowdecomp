#ifndef WOW_SOURCE_OBJECT_OBJECTCLIENT_OBJECTEFFECT_H
#define WOW_SOURCE_OBJECT_OBJECTCLIENT_OBJECTEFFECT_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Base/BitArray.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include "WoW/Source/Object/ObjectClient/Types.h"

class CGObject_C;
class CObjectEffectDef;
class CObjectEffectManager;
class CObjectEffectGroupDef;
class CObjectEffectPackageDef;
class ObjectEffectModifierRec;

class CObjectEffect : public TSLinkedNode<CObjectEffect> {
    public:
    /* 0008 */ SE2SoundKitObject m_soundKitObject;
    /* 001C */ CObjectEffectDef* m_def;
    /* 0020 */ CObjectEffectManager* m_manager;
    /* 0024 */ int32_t m_playCount;
    /* 0028 */ int32_t m_refCount;

    CObjectEffect();
    ~CObjectEffect();
    C3Vector GetWorldPosition();
    void PlayOneShot();
    void TriggerLoopingEffect();
    void UpdateLoopingEffect(CObjectEffectManager* manager);
};

class CObjectEffectRef : public TSLinkedNode<CObjectEffectRef> {
    public:
    /* 0008 */ CObjectEffect* m_effect;

    CObjectEffectRef();
};

struct OBJ_EFFECT_REF_LOOKUP : public TSHashObject<OBJ_EFFECT_REF_LOOKUP, HASHKEY_NONE> {
    /* 0018 */ CObjectEffect* m_effect;
};

struct OBJ_EFFECT_GROUP_LOOKUP : public TSHashObject<OBJ_EFFECT_GROUP_LOOKUP, HASHKEY_NONE> {
    /* 0018 */ TSHashTable<OBJ_EFFECT_REF_LOOKUP, HASHKEY_NONE> m_effects;
};

struct OBJ_EFFECT_REF_LIST_LOOKUP : public TSHashObject<OBJ_EFFECT_REF_LIST_LOOKUP, HASHKEY_NONE> {
    /* 0018 */ TSList<CObjectEffectRef> m_refs;
};

class CObjectEffectManager {
    public:
    /* 0000 */ int32_t m_packageId;
    /* 0004 */ CBitArray m_states;
    /* 0010 */ CGObject_C* m_owner;
    /* 0014 */ CObjectEffectPackageDef* m_packageDef;
    /* 0018 */ TSHashTable<OBJ_EFFECT_GROUP_LOOKUP, HASHKEY_NONE> m_groups;
    /* 0040 */ TSHashTable<OBJ_EFFECT_REF_LIST_LOOKUP, HASHKEY_NONE> m_refLists;
    /* 0068 */ TSList<CObjectEffect> m_loopingEffects;
    /* 0074 */ TSList<CObjectEffect> m_oneShotEffects;

    CObjectEffectManager();
    ~CObjectEffectManager();
    void EnterState(uint32_t state, int32_t playEnterEffects, int32_t refresh);
    void ExitState(uint32_t state, int32_t playExitEffects);
    int32_t Init(int32_t packageId, CGObject_C* owner);
    void PlayLoopingEffect(CObjectEffectDef* def, int32_t groupId);
    void PlayOneShotEffect(CObjectEffectDef* def);
    void PropertyChanged(int32_t modifierId);
    void ShutDown();
    void UpdateAllPlayingEffects();
};

struct ObjectEffectAnimState {
    /* 0000 */ int32_t m_animID;
    /* 0004 */ int32_t m_stateID;
    /* 0008 */ int32_t m_altStateID;
};

class CObjectEffectModifier {
    public:
    enum INPUT_TYPE : int32_t {
        INPUT_NONE = 0,
        INPUT_SPEED = 1,
        NUM_INPUT_TYPES,
    };

    enum MAP_TYPE : int32_t {
        MAP_NONE = 0,
        MAP_LINEAR = 1,
        NUM_MAP_TYPES,
    };

    enum OUTPUT_TYPE : int32_t {
        OUTPUT_NONE = 0,
        OUTPUT_PITCH = 1,
        NUM_OUTPUT_TYPES,
    };

    static float (*s_inputFuncs[NUM_INPUT_TYPES])(CObjectEffectManager* manager);
    static float (*s_mapFuncs[NUM_MAP_TYPES])(float input, const float* params);
    static void (*s_outputFuncs[NUM_OUTPUT_TYPES])(CObjectEffect* effect, float value);

    static float InputNone(CObjectEffectManager* manager);
    static float InputSpeed(CObjectEffectManager* manager);
    static float MapLinear(float input, const float* params);
    static float MapNone(float input, const float* params);
    static void OutputNone(CObjectEffect* effect, float value);
    static void OutputPitch(CObjectEffect* effect, float value);

    /* 0000 */ int32_t m_inputType;
    /* 0004 */ int32_t m_mapType;
    /* 0008 */ int32_t m_outputType;
    /* 000C */ float* m_params;
    /* 0010 */ int32_t m_numParams;
    /* 0014 */ int32_t m_id;

    CObjectEffectModifier();
    void Apply(CObjectEffectManager* manager, CObjectEffect* effect);
    void Init(const ObjectEffectModifierRec* rec);
    void SetParam(int32_t index, float value);
};

class CObjectEffectDefRef : public TSLinkedNode<CObjectEffectDefRef> {
    public:
    /* 0008 */ CObjectEffectDef* m_effectDef;

    CObjectEffectDefRef();
};

class CObjectEffectGroupDefRef : public TSLinkedNode<CObjectEffectGroupDefRef> {
    public:
    /* 0008 */ CObjectEffectGroupDef* m_groupDef;

    CObjectEffectGroupDefRef();
};

class CObjectEffectDef : public TSLinkedNode<CObjectEffectDef> {
    public:
    /* 0008 */ int32_t m_id;
    /* 000C */ int32_t m_effectRecType;
    /* 0010 */ int32_t m_effectRecID;
    /* 0014 */ int32_t m_attachment;
    /* 0018 */ C3Vector m_offset;
    /* 0024 */ CObjectEffectModifier* m_modifier;
};

class CObjectEffectGroupDef : public TSLinkedNode<CObjectEffectGroupDef> {
    public:
    /* 0008 */ int32_t m_id;
    /* 000C */ TSList<CObjectEffectDefRef> m_enterEffects;
    /* 0018 */ TSList<CObjectEffectDefRef> m_loopEffects;
    /* 0024 */ TSList<CObjectEffectDefRef> m_exitEffects;
    /* 0030 */ TSList<CObjectEffectDefRef> m_eventEffects;
};

struct OBJ_EFFECT_GROUP_DEF_REF_LIST_LOOKUP : public TSHashObject<OBJ_EFFECT_GROUP_DEF_REF_LIST_LOOKUP, HASHKEY_NONE> {
    /* 0018 */ TSList<CObjectEffectGroupDefRef> m_groupDefRefs;
};

class CObjectEffectPackageDef : public TSLinkedNode<CObjectEffectPackageDef> {
    public:
    /* 0008 */ int32_t m_id;
    /* 000C */ TSHashTable<OBJ_EFFECT_GROUP_DEF_REF_LIST_LOOKUP, HASHKEY_NONE> m_stateGroupRefs;
    /* 0034 */ TSHashTable<OBJ_EFFECT_GROUP_DEF_REF_LIST_LOOKUP, HASHKEY_NONE> m_eventGroupRefs;
};

struct OBJ_EFFECT_DEF_LOOKUP : public TSHashObject<OBJ_EFFECT_DEF_LOOKUP, HASHKEY_NONE> {
    /* 0018 */ CObjectEffectDef m_objectEffectDef;
};

struct OBJ_EFFECT_GROUP_DEF_LOOKUP : public TSHashObject<OBJ_EFFECT_GROUP_DEF_LOOKUP, HASHKEY_NONE> {
    /* 0018 */ CObjectEffectGroupDef m_objectEffectGroupDef;
};

struct OBJ_EFFECT_PACKAGE_DEF_LOOKUP : public TSHashObject<OBJ_EFFECT_PACKAGE_DEF_LOOKUP, HASHKEY_NONE> {
    /* 0018 */ CObjectEffectPackageDef m_objectEffectPackageDef;
};

int32_t FindObjectEffectStateFromAnimID(int32_t animId, int32_t alternate);
void InitObjectEffectStateMap();
void ObjectEffectSystem_ShutDown();
void ObjectEffectSystem_Initialize();

#endif
