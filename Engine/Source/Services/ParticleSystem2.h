#ifndef ENGINE_SOURCE_SERVICES_PARTICLESYSTEM2_H
#define ENGINE_SOURCE_SERVICES_PARTICLESYSTEM2_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Gx/Types.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Spline_Bezier3.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/C4Quaternion.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Tempest/CMemBlock.h"
#include "Engine/Source/Tempest/CRandom.h"

class C2Vector;
class CGxShader;
class CM2Model;
class CM2Scene;
class CParticleEmitter2;
class CRange;
class CTexture;
struct CGxBuf;
struct CGxPool;

class CParticle2 {
    public:
    /* 0000 */ float m_age;
    /* 0004 */ C3Vector m_position;
    /* 0010 */ C3Vector m_velocity;
    /* 001C */ fixed16 m_lifeSeed;
    /* 001E */ uint16_t m_randomSeed;
};

class CParticle2_Model : public CParticle2 {
    public:
    /* 0020 */ C4Quaternion m_orientation;
    /* 0030 */ C3Vector m_angularVelocity;
    /* 003C */ CM2Model* m_model;

    CParticle2_Model();
    CParticle2_Model(const CParticle2_Model& source);
    ~CParticle2_Model();
};

class CParticleMat {
    public:
    enum FLAGS : uint32_t {
        NONE = 0,
        LIT = 1,
        FOGGED = 2,
        DEPTH_WRITE = 4,
        DEFAULT = 7,
    };

    /* 0000 */ uint32_t m_blendMode;
    /* 0004 */ uint32_t m_flags;
};

struct CSortableParticleRecord {
    /* 0000 */ float m_depth;
    /* 0004 */ CParticle2* m_particle;
};

class CSortableParticleQueue : public CMemBlock {
    public:
    /* 0014 */ uint32_t m_chunk;
    /* 0018 */ uint32_t m_capacity;
    /* 001C */ uint32_t m_count;

    CSortableParticleQueue(const uint32_t* params);
    void Erase(uint32_t index, uint32_t count);
    CSortableParticleRecord Pop();
    void Push(float depth, CParticle2* particle);
};

class CParticleEmitter2 {
    protected:
    static const float VEL_UPDATE_TIME;
    static const float MIN_ZSOURCE;

    public:
    enum BATCH_FLAGS : int32_t {
        BATCH_FIRST_RENDER = 0x1,
    };

    enum PARTICLE_EMITTER_TYPE : int32_t {
        PET_BASE_EMITTER = 0,
        PET_PLANE_EMITTER = 1,
        PET_SPHERE_EMITTER = 2,
        PET_SPLINE_EMITTER = 3,
        PET_NUMS_PETS = 4,
    };

    enum : uint32_t {
        MAX_RECURSIVE_PARTICLES = 0x1000,
        RND_TABLE_SIZE = 128,
        RND_TABLE_MASK = 0x7F,
    };

    enum : uint32_t {
        NUM_PARTICLE_KEYS = 2,
        MAX_CHILD_EMITTERS = 4,
    };

    enum PARTICLE_TYPE : uint32_t {
        PT_QUAD = 0,
        PT_MODEL = 1,
    };

    // One quad, the vertex shader expands it to m_center + corner.x * m_axisU + corner.y * m_axisV.
    struct ParticleInstance {
        C3Vector m_center;
        C3Vector m_axisU;
        C3Vector m_axisV;
        CImVector m_color;
        C2Vector m_texBase;
        C2Vector m_texCell;
    };

    struct ParticleVertex {
        ParticleInstance* m_instance;
        uint32_t m_count;
    };

    struct SimpleKey {
        /* 0000 */ CImVector m_color;
        /* 0004 */ int32_t m_deltaR;
        /* 0008 */ int32_t m_deltaG;
        /* 000C */ int32_t m_deltaB;
        /* 0010 */ int32_t m_deltaA;
        /* 0014 */ float m_scale;
        /* 0018 */ float m_deltaScale;
        /* 001C */ int32_t m_headCell;
        /* 0020 */ int32_t m_deltaHeadCell;
        /* 0024 */ int32_t m_tailCell;
        /* 0028 */ int32_t m_deltaTailCell;

        SimpleKey();
    };

    struct Params {
        /* 0000 */ uint32_t m_hasModel;
        /* 0004 */ float m_emissionRate;
        /* 0008 */ float m_emissionRateVariation;
        /* 000C */ float m_life;
        /* 0010 */ float m_lifeVariation;
        /* 0014 */ float m_tailLength;
        /* 0018 */ float m_speed;
        /* 001C */ float m_gravity;
        /* 0020 */ float m_variation;
        /* 0024 */ float m_zsource;
        /* 0028 */ float m_initialSpin;
        /* 002C */ float m_initialSpinVariation;
        /* 0030 */ float m_spin;
        /* 0034 */ float m_spinVariation;
        /* 0038 */ uint32_t m_materialBlend;
        /* 003C */ uint32_t m_materialFlags;
        /* 0040 */ const M2PartTrack<C3Vector>* m_colorTrack;
        /* 0044 */ const M2PartTrack<fixed16>* m_alphaTrack;
        /* 0048 */ const M2PartTrack<C2Vector>* m_scaleTrack;
        /* 004C */ float m_scaleVariationX;
        /* 0050 */ float m_scaleVariationY;
        /* 0054 */ const M2PartTrack<uint16_t>* m_headCellTrack;
        /* 0058 */ const M2PartTrack<uint16_t>* m_tailCellTrack;
        /* 005C */ C3Vector m_replacementColors [3];
        /* 0080 */ float m_simpleMidTime;
        /* 0084 */ SimpleKey* m_simpleKeys;

        Params();
    };

    static CGxBuf* s_indexBuf;
    static CGxPool* s_indexPool;
    static CGxBuf* s_cornerBuf;
    static CGxPool* s_cornerPool;
    static CGxShader* s_instancedShaders[60];
    static uint32_t s_initCount;
    static uint8_t s_noIndexBuffer;
    // FIX: per thread so particle vertices can be built on the animation workers
    static thread_local CSortableParticleQueue s_pq;
    static float s_rndTable[CParticleEmitter2::RND_TABLE_SIZE];
    static thread_local float s_viewDist;

    static void Destroy();
    static void Init();
    static void PrebuildClear();
    static uint32_t PrebuildCount();
    static void PrebuildRun(uint32_t index);
    static void __stdcall RenderIndices(CGxBuf* buf);
    static void RenderCorners(CGxBuf* buf);
    static void RenderParticleInstances(CGxBuf* buf, uint32_t instanceCount, int32_t lit);

    /* 0004 */ int32_t m_refCount;
    /* 0008 */ float m_emitAccumulator;
    /* 000C */ uint32_t m_textureCellShift;
    /* 0010 */ float m_textureCellWidth;
    /* 0014 */ float m_textureCellHeight;
    /* 0018 */ uint32_t m_priorityPlane;
    /* 001C */ uint32_t m_renderedParticles;
    /* 0020 */ PARTICLE_EMITTER_TYPE m_emitterType;
    /* 0024 */ CRndSeed m_random;
    /* 002C */ TSGrowableArray<CParticle2> m_particles;
    /* 003C */ TSGrowableArray<CParticle2_Model> m_particleModels;
    /* 004C */ TSGrowableArray<uint32_t> m_liveList;
    /* 005C */ TSGrowableArray<uint32_t> m_freeList;
    /* 006C */ uint32_t m_childEmitterCount;
    /* 0070 */ CParticleEmitter2* m_childEmitters [CParticleEmitter2::MAX_CHILD_EMITTERS];
    /* 0080 */ CM2Model* m_model;
    /* 0084 */ CM2Scene* m_scene;
    /* 0088 */ CM2Model* m_childModel;
    /* 008C */ uint32_t m_vertsPerParticle;
    /* 0090 */ uint32_t m_indicesPerParticle;
    /* 0094 */ uint32_t unused_0094;
    /* 0098 */ CParticleEmitter2::Params m_params;
    /* 0120 */ uint32_t m_textureRows;
    /* 0124 */ uint32_t m_textureCols;
    /* 0128 */ CTexture* m_texture;
    /* 012C */ uint32_t unused_012C;
    /* 0130 */ float m_alphaScale;
    /* 0134 */ uint32_t m_flags;
    /* 0138 */ uint32_t m_batchFlags;
    /* 013C */ float m_twinkleFPS;
    /* 0140 */ float m_twinkleOnOff;
    /* 0144 */ float m_twinkleScaleBase;
    /* 0148 */ float m_twinkleScaleSpan;
    /* 014C */ float m_ivelScale;
    /* 0150 */ float m_tumbleBaseX;
    /* 0154 */ float m_tumbleSpanX;
    /* 0158 */ float m_tumbleBaseY;
    /* 015C */ float m_tumbleSpanY;
    /* 0160 */ float m_tumbleBaseZ;
    /* 0164 */ float m_tumbleSpanZ;
    /* 0168 */ float m_drag;
    /* 016C */ float m_windVectorX;
    /* 0170 */ float m_windVectorY;
    /* 0174 */ float m_windVectorZ;
    /* 0178 */ float m_windTime;
    /* 017C */ float m_followIntercept;
    /* 0180 */ float m_followSlope;
    /* 0184 */ C44Matrix m_xform;
    /* 01C4 */ C3Vector m_viewPos;
    /* 01D0 */ C3Vector m_parentPosition;
    /* 01DC */ float m_ivelTimer;
    /* 01E0 */ C3Vector m_parentVelocity;
    /* 01EC */ float m_xformScale;
    /* 01F0 */ uint32_t m_prebuiltSlot;
    /* 01F4 */ C3Vector m_followOffset;
    /* 0200 */ C3Vector m_burstOffset;
    /* 020C */ C3Vector m_billboardAxis;
    /* 0218 */ CAaBox m_worldBounds;
    /* 0230 */ uint32_t m_updateQueued;

    virtual void Sync();
    virtual void CreateParticle(CParticle2& particle, float time, const C44Matrix& transform);
    virtual void CreateParticle(CParticle2_Model& particle, float time, const C44Matrix& transform);
    virtual void DestroyParticle(CParticle2& particle);
    virtual CParticleEmitter2* Clone(int32_t copyParticles) = 0;
    virtual ~CParticleEmitter2();
    virtual void SetWidth(float width) = 0;
    virtual void SetHeight(float height) = 0;
    virtual void SetLatitude(float latitude) = 0;
    virtual void SetLongitude(float longitude) = 0;
    virtual void SetEmissionRate(float rate);

    CParticleEmitter2();
    CParticleEmitter2(const CParticleEmitter2& source, int32_t copyParticles);
    CParticleEmitter2* AddRef();
    void BuildParticleVertices(ParticleVertex& vertex, uint32_t count, const C44Matrix& transform);
    int32_t BuildVertex(const CParticle2& particle, ParticleVertex& vertex);
    float CalcVelocity();
    void ChangeFrameOfReference(const C44Matrix* matrix);
    void CreateChildEmittersFromModel(CM2Scene* scene, const char* name);
    void DecRef();
    void DetermineIfSimple();
    void EmitNewParticles(float deltaTime, const C44Matrix& transform);
    void EmitParticle(float time, const C44Matrix& transform);
    uint32_t GetNumBatches();
    uint32_t GetNumParticleModels();
    uint32_t GetNumPrimitives();
    CM2Model* GetParticleModelInternal(uint32_t& index);
    void GetReplacementColors(CImVector& color0, CImVector& color1, CImVector& color2);
    int32_t HasLiveParticles();
    void InternalUpdate(float deltaTime, int32_t child);
    void InterpolateAllTracks(const CParticle2& particle, CImVector* color, C2Vector* scale, int32_t* headCell, int32_t* tailCell);
    void InterpolateAllTracksSimple(const CParticle2& particle, CImVector* color, C2Vector* scale, int32_t* headCell, int32_t* tailCell);
    CImVector InterpolateColorTrack(float t);
    int32_t MoveParticle(CParticle2& particle, float deltaTime);
    int32_t MoveParticle(CParticle2_Model& particle, float deltaTime);
    void PlaceParticleModels(const C44Matrix* frameOfReference);
    uint32_t PrebuildAdd(const C44Matrix* frameOfReference, const C44Matrix& view);
    void ProjectParticle(CParticle2& particle);
    void RandomizeSpin(const CParticle2& particle, float* spin, float* spinRate);
    void Render(const C44Matrix* frameOfReference, void* buffer, uint32_t first);
    int32_t RenderParticle(CParticle2_Model& particle, const C44Matrix* frameOfReference);
    void RenderParticles(const C44Matrix* frameOfReference, void* buffer);
    void RenderParticlesPrep(const C44Matrix* frameOfReference, const C44Matrix& view);
    void SetChooseRandomTexture(int32_t choose);
    void SetFollowParams(float speed1, float scale1, float speed2, float scale2);
    void SetMaterial(const CParticleMat& material, CTexture* texture);
    void SetModel(CM2Scene* scene, const char* name);
    void SetParticleColors(const CImVector& color0, const CImVector& color1, const CImVector& color2);
    void SetParticleStyle(int32_t head, int32_t tail, float tailLength, int32_t clampTail);
    void SetSimpleAlphaKeys();
    void SetSimpleColorKeys();
    void SetSimpleHeadCellKeys();
    void SetSimpleScaleKeys();
    void SetSimpleTailCellKeys();
    void SetTexture(CTexture* texture);
    void SetTextureDimensions(uint32_t rows, uint32_t cols);
    void SetTwinkleScale(const CRange& scale);
    void SetZsource(float zsource);
    void StepUpdate(float deltaTime, int32_t child);
    void SyncAllocation(uint32_t count);
    void SyncReserve(uint32_t count, uint32_t used, uint32_t spare);
    void Update(float deltaTime, const C44Matrix& transform, const C3Vector& viewPos, const C44Matrix* frameOfReference);
    int32_t UpdateLiveParticle(float deltaTime, CParticle2* particle, uint32_t index);
    void UpdateXform(const C44Matrix& transform, const C3Vector& viewPos, const C44Matrix* frameOfReference);
    CAaBox& WorldBounds();

    enum RUNTIME_FLAGS : uint32_t {
        DATA_EMITTING = 0x1,
        USER_ENABLED = 0x2,
        HEAD = 0x4,
        TAIL = 0x8,
        PROP_PARTICLE_COLOR_SET = 0x10,
        SORT_PARTICLES = 0x20,
        BURST_PENDING = 0x40,
        UNKNOWN_00000080 = 0x80,
        UNKNOWN_00000100 = 0x100,
        LOCAL_SPACE = 0x200,
        TRANSFORM_SCALE = 0x400,
        INHERIT_VELOCITY = 0x800,
        KILL_OUTWARD_PARTICLES = 0x1000,
        EMIT_ALONG_MOTION = 0x2000,
        USE_BILLBOARD_AXIS = 0x4000,
        SPHERE_Z_VELOCITY = 0x8000,
        RANDOM_SPIN_DIRECTION = 0x10000,
        CLAMP_TAIL = 0x20000,
        PROJECT = 0x40000,
        FOLLOW = 0x80000,
        RANDOM_TEXTURE = 0x100000,
        VELOCITY_ALIGNED_HEAD = 0x200000,
        IGNORE_DISTANCE = 0x400000,
        SCALE_VARIATION = 0x800000,
        SIMPLE_TRACKS = 0x1000000,
        UNKNOWN_02000000 = 0x2000000,
        EMISSION_ENABLED_MASK = 0x3,
        INITIAL_FLAGS = 0x6,
    };

};

class CPlaneParticleEmitter : public CParticleEmitter2 {
    public:
    /* 0234 */ float m_width;
    /* 0238 */ float m_height;
    /* 023C */ float m_latitude;
    /* 0240 */ float m_longitude;

    CPlaneParticleEmitter();
    CPlaneParticleEmitter(const CPlaneParticleEmitter& source, int32_t copyParticles);
    void CreateParticle(CParticle2& particle, float time, const C44Matrix& transform) override;
    CParticleEmitter2* Clone(int32_t copyParticles) override;
    ~CPlaneParticleEmitter() override;
    void SetWidth(float width) override;
    void SetHeight(float height) override;
    void SetLatitude(float latitude) override;
    void SetLongitude(float longitude) override;
};

class CSphereParticleEmitter : public CParticleEmitter2 {
    public:
    /* 0234 */ float m_minRadius;
    /* 0238 */ float m_maxRadius;
    /* 023C */ float m_radiusSpan;
    /* 0240 */ float m_latitude;
    /* 0244 */ float m_longitude;

    CSphereParticleEmitter();
    CSphereParticleEmitter(const CSphereParticleEmitter& source, int32_t copyParticles);
    void CreateParticle(CParticle2& particle, float time, const C44Matrix& transform) override;
    CParticleEmitter2* Clone(int32_t copyParticles) override;
    ~CSphereParticleEmitter() override;
    void SetWidth(float width) override;
    void SetHeight(float height) override;
    void SetLatitude(float latitude) override;
    void SetLongitude(float longitude) override;
};

class CSplineParticleEmitter : public CParticleEmitter2 {
    public:
    /* 0234 */ float m_ratePerUnit;
    /* 0238 */ float m_start;
    /* 023C */ float m_end;
    /* 0240 */ float m_latitude;
    /* 0244 */ float m_longitude;
    /* 0248 */ int32_t m_emitAtEnd;
    /* 024C */ C3Spline_Bezier3 m_spline;

    CSplineParticleEmitter();
    CSplineParticleEmitter(const CSplineParticleEmitter& source, int32_t copyParticles);
    void CreateParticle(CParticle2& particle, float time, const C44Matrix& transform) override;
    CParticleEmitter2* Clone(int32_t copyParticles) override;
    ~CSplineParticleEmitter() override;
    void SetWidth(float width) override;
    void SetHeight(float height) override;
    void SetLatitude(float latitude) override;
    void SetLongitude(float longitude) override;
    void SetEmissionRate(float rate) override;
    void SetSpline(const C3Vector* points, uint32_t count);
};

#endif
