#ifndef ENGINE_SOURCE_MODEL2_CM2SCENERENDER_H
#define ENGINE_SOURCE_MODEL2_CM2SCENERENDER_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Gx/Types.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Model2/Types.h"
#include "Engine/Source/Tempest/C44Matrix.h"

class C3Vector;
class CM2Cache;
class CM2Lighting;
class CM2Model;
class CM2Scene;
class CM2Shared;
class CParticleEmitter2;
class CShaderEffect;
class M2Element;
struct CGxBuf;

class CM2SceneRender {
    public:

    enum FOG_MODE : uint32_t {
        FOG_DISABLED = 0,
        FOG_LIGHTING_COLOR = 1,
        FOG_BLACK = 2,
        FOG_WHITE = 3,
        FOG_GRAY = 4,
        FOG_UNSET = 0xFFFFFFFF,
    };

    /* 0000 */ C44Matrix m_matrix;
    /* 0040 */ CM2Scene* m_scene;
    /* 0044 */ CM2Cache* m_cache;
    /* 0048 */ M2Data* m_data;
    /* 004C */ M2PASS m_pass;
    /* 0050 */ M2Element* m_curElement;
    /* 0054 */ M2Element* m_prevElement;
    /* 0058 */ int32_t m_curType;
    /* 005C */ int32_t m_prevType;
    /* 0060 */ CM2Model* m_curModel;
    /* 0064 */ CM2Model* m_prevModel;
    /* 0068 */ CM2Shared* m_curShared;
    /* 006C */ CM2Shared* m_prevShared;
    /* 0070 */ CM2Lighting* m_curLighting;
    /* 0074 */ CM2Lighting* m_prevLighting;
    /* 0078 */ int32_t m_curLocalLighting;
    /* 007C */ int32_t m_prevLocalLighting;
    /* 0080 */ uint32_t m_curFogMode;
    /* 0084 */ uint32_t m_prevFogMode;
    /* 0088 */ M2Batch* m_curBatch;
    /* 008C */ M2Batch* m_prevBatch;
    /* 0090 */ M2SkinSection* m_curSkinSection;
    /* 0094 */ M2SkinSection* m_prevSkinSection;
    /* 0098 */ M2Material* m_curMaterial;
    /* 009C */ M2Material* m_prevMaterial;
    /* 00A0 */ CShaderEffect* m_particleEffect;
    /* 00A4 */ CShaderEffect* m_particleUnlitEffect;
    /* 00A8 */ CShaderEffect* m_projectedModModEffect;
    /* 00AC */ CShaderEffect* m_projectedModModUnlitEffect;
    /* 00B0 */ CShaderEffect* m_projectedModAddEffect;
    /* 00B4 */ CShaderEffect* m_projectedModAddUnlitEffect;
    /* 00B8 */ M2Material m_defaultMaterial;

    static void EndFixed();
    static int32_t LightingColorsMatch(const CM2Lighting* a, const CM2Lighting* b);

    CM2SceneRender(CM2Scene* scene);
    void BeginFixed(const C3Vector& position);
    void Draw(M2PASS pass, M2Element* elements, uint32_t* indices, uint32_t count);
    void DrawBatch();
    uint32_t DrawBatchedParticle(uint32_t index, M2Element* elements, uint32_t* indices, uint32_t count);
    void DrawBatchDoodad(M2Element* elements, uint32_t* indices);
    void DrawBatchProj();
    void DrawCallback();
    uint32_t DrawParticle(uint32_t index, M2Element* elements, uint32_t* indices, uint32_t count);
    void DrawRibbon();
    void RenderBatchedParticle(CParticleEmitter2* emitter, CGxBuf* buf, uint32_t instanceCount);
    void SetBatchVertices(int32_t sharedVertices);
    void SetupBillboardView(const C3Vector& position);
    void SetupLighting();
    void SetupMaterial();
    void SetupTextures();
};

#endif
