#ifndef WOW_SOURCE_GAME_GAMECLIENT_FFXEFFECTS_H
#define WOW_SOURCE_GAME_GAMECLIENT_FFXEFFECTS_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/FFX/FFX.h"

class EffectDeath : public FFX::Effect {
    public:
    virtual ~EffectDeath();

    EffectDeath();
};

class EffectNetherWorld : public FFX::Effect {
    public:
    enum PASS_STAGE : uint32_t {
        BLUR_PASS = 0,
        COMBINE_PASS = 1,
        PASS_COUNT = 2,
    };

    virtual ~EffectNetherWorld();
    void End() override;

    EffectNetherWorld();
};

class EffectSwirlingFog : public FFX::Effect {
    public:
    enum PASS_STAGE : uint32_t {
        SEED_PASS = 0,
        PROPAGATION_PASS = 1,
        COMBINE_PASS = 2,
        PASS_COUNT = 3,
    };

    /* 001C */ FFX::Tex m_fogTex;
    /* 0038 */ FFX::Tex m_seedTex;

    virtual ~EffectSwirlingFog();
    void SetParameters(int32_t count, const int32_t* params) override;

    EffectSwirlingFog();
    void Reset();
};

#endif
