#include <cmath>
#include <cstdint>
#include "Engine/Source/Gx/CGxGammaRamp.h"

// VERIFY: aa5b02d7 manual
// OFFSET: 0x00684070
void CGxGammaRamp::Set(float gamma) {
    for (uint32_t i = 0; i < 256; i++) {
        uint16_t value = static_cast<uint16_t>(static_cast<int64_t>(pow(i * 0.0039215689f, gamma) * 65535.0f));
        this->m_red[i] = value;
        this->m_green[i] = value;
        this->m_blue[i] = value;
    }
}
