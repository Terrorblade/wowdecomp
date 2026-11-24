#ifndef WOW_SOURCE_GAME_GAMECLIENT_PROJECTTEX_H
#define WOW_SOURCE_GAME_GAMECLIENT_PROJECTTEX_H

#include <cstddef>
#include <cstdint>

class C44Matrix;
class CAaBox;
class CGxTex;
class CImVector;

enum PROJECT_TEX_FLAGS : uint32_t {
    PROJECT_TEX_NONE = 0x0,
    PROJECT_TEX_DOODADS = 0x1,
    PROJECT_TEX_MATERIAL_COLOR = 0x2,
    PROJECT_TEX_ALL_FACES = 0x4,
};

namespace ProjectTex {
    void Render2d(const CAaBox& box, const CImVector& color, const C44Matrix* matrix, float fade, uint32_t a5, uint32_t a6, float polygonOffset);
}

CGxTex* ProjectTex2dGetAddFadeTex();
void ProjectTex2dDrawShaderEffect(const CAaBox& box, const CImVector& color, uint32_t triFlags, uint32_t flags, float polygonOffset);
void ProjectTex2dMakeMatrices(C44Matrix& texMatrix, C44Matrix& fadeMatrix, const CAaBox& box, const C44Matrix* rotation, float fade, int32_t local);

#endif
