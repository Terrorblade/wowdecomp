#ifndef WOW_SOURCE_GAME_GAMECLIENT_PASSNETHERBLUR_H
#define WOW_SOURCE_GAME_GAMECLIENT_PASSNETHERBLUR_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/FFX/FFX.h"
#include "Engine/Source/Tempest/CRandom.h"

class CGxShader;

class PassNetherBlur : public FFX::Pass {
    public:
    enum NOISE_TABLE_ROLE : uint32_t {
        BLEND_FROM = 0,
        BLEND_TO = 1,
        REGENERATE = 2,
        ROLE_COUNT = 3,
    };

    /* 0030 */ uint32_t m_useRect;
    /* 0034 */ float m_noise[ROLE_COUNT][36];
    /* 01E4 */ float m_blendedNoise[36];
    /* 0274 */ float* m_noiseTables[ROLE_COUNT];
    /* 0280 */ float m_noiseBlend;
    /* 0284 */ CRndSeed m_seed;
    /* 028C */ CGxShader* m_vertexShader;
    /* 0290 */ CGxShader* m_pixelShaders[2];

    ~PassNetherBlur() override;
    bool Valid() override;
    void Execute() override;

    PassNetherBlur(FFX::Tex** inputs, uint32_t count, FFX::Tex* output, EGxBlend blend, bool unk);
};

class PassNetherCombine : public FFX::Pass {
    public:
    /* 0030 */ uint32_t m_useRect;
    /* 0034 */ CGxShader* m_pixelShaders[2];
    /* 003C */ float m_fade;

    ~PassNetherCombine() override;
    bool Valid() override;
    void Execute() override;

    PassNetherCombine(FFX::Tex** inputs, uint32_t count, FFX::Tex* output, EGxBlend blend, bool unk);
};

#endif
