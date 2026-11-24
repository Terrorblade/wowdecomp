#ifndef ENGINE_SOURCE_SERVICES_RIBBONEMITTER_H
#define ENGINE_SOURCE_SERVICES_RIBBONEMITTER_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Gx/CGxVertexPCT.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/CAaRect.h"
#include "Engine/Source/Tempest/CImVector.h"

class C44Matrix;
class CTexture;

class CRibbonMat {
    public:
    /* 0000 */ uint32_t m_lit : 1;
    uint32_t m_fogged : 1;
    uint32_t m_depthTest : 1;
    uint32_t m_depthWrite : 1;
    uint32_t m_cull : 1;
    /* 0004 */ uint32_t m_blendMode;

    CRibbonMat();
};

class CRibbonEmitter {
    public:
    enum RUNTIME_FLAGS : int32_t {
        POSITION_SET = 0x1,
        INITIALIZED = 0x2,
        DATA_ENABLED = 0x4,
        USER_ENABLED = 0x8,
        UPDATED = 0x10,
        UNKNOWN_0020 = 0x20,
        UPDATE_STATE_MASK = 0x30,
        CONSTRUCTOR_RESET_MASK = 0x12,
        INITIAL_ENABLED_MASK = 0xE,
    };

    /* 0000 */ int32_t m_refCount;
    /* 0004 */ TSGrowableArray<float> m_edgeAges;
    /* 0014 */ uint32_t m_writePos;
    /* 0018 */ uint32_t m_readPos;
    /* 001C */ float m_edgeFraction;
    /* 0020 */ C3Vector m_prevPos;
    /* 002C */ C3Vector m_localPos;
    /* 0038 */ TSGrowableArray<CGxVertexPCT> m_vertices;
    /* 0048 */ TSGrowableArray<uint16_t> m_indices;
    /* 0058 */ float m_invEdgeLifetime;
    /* 005C */ float m_texCellU;
    /* 0060 */ float m_texCellV;
    /* 0064 */ float m_invTexCellU;
    /* 0068 */ float m_invTexCellV;
    /* 006C */ CAaRect m_texSlotCoords;
    /* 007C */ C3Vector m_prevUp;
    /* 0088 */ C3Vector m_currUp;
    /* 0094 */ C3Vector m_prevDir;
    /* 00A0 */ C3Vector m_currDir;
    /* 00AC */ C3Vector m_startTangent;
    /* 00B8 */ C3Vector m_endTangent;
    /* 00C4 */ C3Vector m_startBelow;
    /* 00D0 */ C3Vector m_endBelow;
    /* 00DC */ C3Vector m_startAbove;
    /* 00E8 */ C3Vector m_endAbove;
    /* 00F4 */ CAaBox m_worldBounds;
    /* 010C */ float m_edgesPerSec;
    /* 0110 */ float m_edgeLifetime;
    /* 0114 */ TSGrowableArray<CRibbonMat> m_materials;
    /* 0124 */ TSGrowableArray<CTexture*> m_textures;
    /* 0134 */ TSGrowableArray<uint32_t> m_textureTypes;
    /* 0144 */ CImVector m_color;
    /* 0148 */ CAaRect m_texCoords;
    /* 0158 */ uint32_t m_rows;
    /* 015C */ uint32_t m_cols;
    /* 0160 */ uint32_t m_flags;
    /* 0164 */ C3Vector m_currPos;
    /* 0170 */ uint32_t m_texSlot;
    /* 0174 */ float m_above;
    /* 0178 */ float m_below;
    /* 017C */ float m_gravity;

    CRibbonEmitter();
    ~CRibbonEmitter();
    void ChangeFrameOfReference(const C44Matrix* matrix);
    void CloseTextureHandles();
    void ConvertTexSlotToTexCoords();
    void DecRef();
    uint32_t GetNumBatches();
    uint32_t GetNumPrimitives();
    void InitInterpDeltas();
    void Initialize(float edgesPerSecond, float edgeLifetime, const CImVector& color, const TSGrowableArray<CTexture*>& textures, const TSGrowableArray<CRibbonMat>& materials, const TSGrowableArray<uint32_t>& textureTypes, const CAaRect& texCoords, uint32_t rows, uint32_t cols);
    void InterpEdge(float age, float t, uint32_t advance);
    int32_t IsDead();
    int32_t Render(const C44Matrix* frameOfReference);
    int32_t ReplaceTexture(uint32_t type, CTexture* texture);
    void SetAbove(float above);
    void SetAlpha(float alpha);
    void SetBelow(float below);
    void SetColor(float r, float g, float b);
    void SetDataEnabled(int32_t enabled);
    void SetGravity(float gravity);
    void SetPos(const C44Matrix& transform, const C3Vector& position, const C44Matrix* frameOfReference);
    void SetTexSlot(uint32_t slot);
    void SetUserEnabled(uint32_t enabled);
    void Update(float deltaTime, int32_t suppressNewEdges);
};

#endif
