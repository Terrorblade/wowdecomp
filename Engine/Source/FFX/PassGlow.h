#ifndef ENGINE_SOURCE_FFX_PASSGLOW_H
#define ENGINE_SOURCE_FFX_PASSGLOW_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/FFX/FFX.h"

class CGxShader;

class PassGlowBase : public FFX::Pass {
    public:
    /* 0030 */ CImVector m_color;

    PassGlowBase(FFX::Tex** inputs, uint32_t count, FFX::Tex* output, EGxBlend blend, bool unk);
};

class PassGlow : public PassGlowBase {
    public:
    /* 0034 */ int32_t m_shaderIndex;
    /* 0038 */ CGxShader* m_shaders[TEXCOORD_COUNT];

    ~PassGlow() override;
    bool Valid() override;
    void Execute() override;

    PassGlow(FFX::Tex** inputs, uint32_t count, FFX::Tex* output, EGxBlend blend, bool unk);
};

#endif
