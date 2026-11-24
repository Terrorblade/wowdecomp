#ifndef WOW_COMMON_LIGHTNING_H
#define WOW_COMMON_LIGHTNING_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/DB/Rec/SpellChainEffectsRec.h"

class CTexture;

struct LightningJoint {
    /* 0000 */ C3Vector m_pos;
    /* 000C */ C3Vector m_smoothPos;
    /* 0018 */ C3Vector m_velocity;
    /* 0024 */ float m_jumpTimer;
};

class CLightning {
    public:
    enum RUNTIME_FLAGS : int32_t {
        FLAG_NONE = 0,
        FLAG_JOINTS_INITIALIZED = 0x1,
        FLAG_VISIBLE = 0x2,
        FLAG_FLICKER_OFF = 0x4,
        FLAG_INITIAL_PULSE_WRAP = 0x8,
        FLAG_SOURCE_ENDPOINT = 0x10,
        FLAG_DESTINATION_ENDPOINT = 0x20,
        FLAG_RESET_MASK = 0x5,
        FLAG_ENDPOINT_MASK = 0x30,
    };

    static uint32_t DecodeChainEffectsID(uint32_t id);

    /* 0000 */ C3Vector m_srcPos;
    /* 000C */ C3Vector m_dstPos;
    /* 0018 */ CImVector m_color;
    /* 001C */ const SpellChainEffectsRec* m_chainEffects;
    /* 0020 */ TSFixedArray<LightningJoint> m_joints;
    /* 002C */ TSFixedArray<C3Vector> m_points;
    /* 0038 */ TSFixedArray<C3Vector> m_positions;
    /* 0044 */ TSFixedArray<C2Vector> m_texCoords;
    /* 0050 */ TSFixedArray<CImVector> m_colors;
    /* 005C */ TSFixedArray<uint16_t> m_indices;
    /* 0068 */ CAaBox m_bounds;
    /* 0080 */ float m_texOffset;
    /* 0084 */ CTexture* m_texture;
    /* 0088 */ void (*m_coordUpdate)(void*, uint32_t, C3Vector*, C3Vector*);
    /* 008C */ void* m_coordUpdateParam;
    /* 0090 */ uint32_t m_flags;
    /* 0094 */ float m_waveAngle;
    /* 0098 */ float m_waveSpin;
    /* 009C */ float m_wavePhase;
    /* 00A0 */ float m_arcAngle;
    /* 00A4 */ float m_arcSpin;
    /* 00A8 */ float m_flickerTimer;
    /* 00AC */ float m_pulsePos;
    /* 00B0 */ float m_texRandom;

    CLightning();
    ~CLightning();
    void ApplyPulsingToVertexData(int32_t& start, int32_t& count);
    void ApplyWaveToPosition(C3Vector& position, float t, float length);
    void BuildPoint(C3Vector& point, const C3Vector& base, float t, LightningJoint& joint, float scale);
    void BuildPoints();
    void BuildPointsUsingHierarchy();
    void Init(const SpellChainEffectsRec* chainEffects);
    void PrecomputeBeamValues();
    void RandomizeJoint(LightningJoint& joint, float radius);
    void Render(uint32_t index, const C3Vector& cameraPos);
    void Reset();
    void SetDstPos(const C3Vector& pos);
    void SetSrcPos(const C3Vector& pos);
    void SetTexture(CTexture* texture);
    void Update(float elapsed);
    void UpdateJointCount(float length, float radius);
};

class CLightningManager {
    public:
    enum : uint32_t {
        BADBOLT = 0xFFFFFFFF,
    };

    /* 0000 */ TSGrowableArray<CLightning*> m_bolts;
    /* 0010 */ TSGrowableArray<uint32_t> m_freeList;

    CLightningManager();
    ~CLightningManager();
    uint32_t Add(void (*coordUpdate)(void*, uint32_t, C3Vector*, C3Vector*), void* param, const SpellChainEffectsRec* chainEffects);
    CLightning* GetTempPtr(uint32_t index);
    void Move(uint32_t index, C3Vector* srcPos, C3Vector* dstPos);
    void Remove(uint32_t index);
    void Render(const C3Vector& cameraPos);
    void SetBoltVisible(uint32_t index, bool visible);
    void Update(float elapsed);
};

#endif
