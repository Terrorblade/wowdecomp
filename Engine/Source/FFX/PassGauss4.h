#ifndef ENGINE_SOURCE_FFX_PASSGAUSS4_H
#define ENGINE_SOURCE_FFX_PASSGAUSS4_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/FFX/FFX.h"

class CGxShader;

class PassGauss4 : public FFX::Pass {
    public:
    /* 0030 */ int32_t m_shaderIndex;
    /* 0034 */ CGxShader* m_shaders[TEXCOORD_COUNT];

    ~PassGauss4() override;
    bool Valid() override;
    void Execute() override;

    PassGauss4(FFX::Tex** inputs, uint32_t count, FFX::Tex* output, EGxBlend blend, bool unk);
};

#endif
