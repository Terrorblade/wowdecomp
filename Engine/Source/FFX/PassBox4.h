#ifndef ENGINE_SOURCE_FFX_PASSBOX4_H
#define ENGINE_SOURCE_FFX_PASSBOX4_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/FFX/FFX.h"

class CGxShader;

class PassBox4 : public FFX::Pass {
    public:
    /* 0030 */ int32_t m_shaderIndex;
    /* 0034 */ CGxShader* m_shaders[TEXCOORD_COUNT];

    ~PassBox4() override;
    bool Valid() override;
    void Execute() override;

    PassBox4(FFX::Tex** inputs, uint32_t count, FFX::Tex* output, EGxBlend blend, bool unk);
};

#endif
