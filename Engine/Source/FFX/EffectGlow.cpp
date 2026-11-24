#include "Engine/Source/FFX/EffectGlow.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FFX/PassBox4.h"
#include "Engine/Source/FFX/PassGauss4.h"
#include "Engine/Source/FFX/PassGlow.h"
#include "Engine/Source/FFX/PassGlowWave.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Storm/H/storm.h"

// VERIFY: 4c7e5407 auto
// OFFSET: 0x008BFD30
bool EffectGlow::Valid() {
    bool valid = this->m_cvar->m_intValue != 0;

    if (this->m_mode) {
        for (uint32_t i = 0; i < this->m_altPasses.Count(); i++) {
            valid &= this->m_altPasses[i]->Valid();
        }
    } else {
        for (uint32_t i = 0; i < this->m_passes.Count(); i++) {
            valid &= this->m_passes[i]->Valid();
        }
    }

    return valid;
}

// VERIFY: 56a0fab0 auto
// OFFSET: 0x008BFD90
void EffectGlow::Execute() {
    if (this->m_mode) {
        for (uint32_t i = 0; i < this->m_altPasses.Count(); i++) {
            this->m_altPasses[i]->Execute();
        }
    } else {
        for (uint32_t i = 0; i < this->m_passes.Count(); i++) {
            this->m_passes[i]->Execute();
        }
    }
}

// VERIFY: 61e7eb60 auto
// OFFSET: 0x008BFDE0
void EffectGlow::SetParameters(int32_t count, const int32_t* params) {
    this->m_mode = params[0];

    CImVector color;
    color.b = params[2];
    color.g = params[2];
    color.r = params[2];
    color.a = params[1];

    static_cast<PassGlowBase*>(this->m_altPasses[COMBINE_PASS])->m_color = color;
    static_cast<PassGlowBase*>(this->m_passes[COMBINE_PASS])->m_color = color;
}

// VERIFY: 46600cfa manual
// OFFSET: 0x008BFE80
EffectGlow::EffectGlow() {
    this->m_cvar = CVar::Register("ffxGlow", "full screen glow effect", CVAR_ARCHIVE, this->m_defaultEnabled ? "1" : "0", FFX::CVarCallback, 1, false, nullptr, false);

    FFX::Tex* fullInputs[] = { &FFX::s_full[0] };
    FFX::Tex* quartInputs[] = { &FFX::s_quart[0] };
    FFX::Tex* glowInputs[] = { &FFX::s_full[0], &FFX::s_quart[0] };
    FFX::Tex* waveInputs[] = { &PassGlowWave::s_waveTex, &FFX::s_full[0], &FFX::s_quart[0] };

    FFX::Pass* pass = new (__FILE__, __LINE__) PassBox4(fullInputs, 1, &FFX::s_quart[0], GxBlend_Opaque, false);
    this->m_passes.New(pass);

    pass = new (__FILE__, __LINE__) PassGauss4(quartInputs, 1, &FFX::s_quart[0], GxBlend_Opaque, false);
    this->m_passes.New(pass);

    pass = new (__FILE__, __LINE__) PassGlow(glowInputs, 2, &FFX::s_screen, GxBlend_Opaque, true);
    this->m_passes.New(pass);

    pass = new (__FILE__, __LINE__) PassBox4(fullInputs, 1, &FFX::s_quart[0], GxBlend_Opaque, false);
    *this->m_altPasses.New() = pass;

    pass = new (__FILE__, __LINE__) PassGauss4(quartInputs, 1, &FFX::s_quart[0], GxBlend_Opaque, false);
    *this->m_altPasses.New() = pass;

    if (FFX::s_useRECT && g_theGxDevicePtr->Caps().m_texNonPow2) {
        pass = new (__FILE__, __LINE__) PassGlow(glowInputs, 2, &FFX::s_screen, GxBlend_Opaque, true);
        *this->m_altPasses.New() = pass;
    } else {
        pass = new (__FILE__, __LINE__) PassGlowWave(waveInputs, 3, &FFX::s_screen, GxBlend_Opaque, true);
        *this->m_altPasses.New() = pass;
    }

    this->m_mode = EffectGlow::STANDARD_PASSES;
}

// VERIFY: ae406627 auto
// OFFSET: [0x008C01D0, 0x008C0260]
EffectGlow::~EffectGlow() {
    for (uint32_t i = 0; i < this->m_passes.Count(); i++) {
        delete this->m_passes[i];
    }

    for (uint32_t i = 0; i < this->m_altPasses.Count(); i++) {
        delete this->m_altPasses[i];
    }
}
