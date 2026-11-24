#ifndef ENGINE_SOURCE_FFX_EFFECTGLOW_H
#define ENGINE_SOURCE_FFX_EFFECTGLOW_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/FFX/FFX.h"

class EffectGlow : public FFX::Effect {
    public:

    enum PASS_STAGE : uint32_t {
        DOWNSAMPLE_PASS = 0,
        BLUR_PASS = 1,
        COMBINE_PASS = 2,
        PASS_COUNT = 3,
    };

    enum PASS_SEQUENCE : int32_t {
        STANDARD_PASSES = 0,
        ALTERNATE_PASSES = 1,
    };

    enum {
        NUM_PARAMS = 3,
    };

    /* 001C */ TSGrowableArray<FFX::Pass*> m_altPasses;
    /* 002C */ int32_t m_mode;

    virtual ~EffectGlow();
    virtual bool Valid();
    virtual void Execute();
    virtual void SetParameters(int32_t count, const int32_t* params);

    EffectGlow();
};

#endif
