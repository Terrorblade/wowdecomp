#ifndef ENGINE_SOURCE_FFX_PASSGLOWWAVE_H
#define ENGINE_SOURCE_FFX_PASSGLOWWAVE_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/FFX/FFX.h"
#include "Engine/Source/FFX/PassGlow.h"

class CGxShader;

class PassGlowWave : public PassGlowBase {
    public:

    enum SHADER_VARIANT : int32_t {
        NORMALIZED_UV88_CAPABLE = 0,
        RECTANGLE_UV88_CAPABLE = 1,
        NORMALIZED_UV88_FALLBACK = 2,
        RECTANGLE_UV88_FALLBACK = 3,
        VARIANT_COUNT = 4,
    };

    static FFX::Tex s_waveTex;

    /* 0034 */ int32_t m_shaderIndex;
    /* 0038 */ CGxShader* m_shaders[VARIANT_COUNT];

    ~PassGlowWave() override;
    bool Valid() override;
    void Execute() override;

    PassGlowWave(FFX::Tex** inputs, uint32_t count, FFX::Tex* output, EGxBlend blend, bool unk);
};

#endif
