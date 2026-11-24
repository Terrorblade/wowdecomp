#include <cstdint>
#include "Engine/Source/Gx/CGxFormat.h"

// VERIFY: f33003a7 auto
// OFFSET: 0x00681950
CGxFormat::CGxFormat() {
    this->m_size.x = 0;
    this->m_size.y = 0;
    this->m_pos.x = 0;
    this->m_pos.y = 0;
    this->m_multisampleQuality = 0.0f;
    this->m_maximize = CGxFormat::Maximize_None;
    this->m_stereoEnabled = false;
    this->m_multisampleCount = 1;
    this->m_aspect = true;

    for (uint32_t i = 0; i < 6; i++) {
        this->m_fixedFunction[i] = -1;
    }
}

// VERIFY: 22ad4f7a auto
// OFFSET: 0x00681990
CGxFormat::CGxFormat(bool window, const C2iVector& size, CGxFormat::Format colorFormat, CGxFormat::Format depthFormat, uint32_t refreshRate, uint32_t vsync, bool hwTnL, bool fixLag, bool hwCursor, bool aspect, int32_t maximize) {
    this->m_pos.x = 0;
    this->m_pos.y = 0;
    this->m_hwTnL = hwTnL;
    this->m_hwCursor = hwCursor;
    this->m_fixLag = fixLag;
    this->m_window = window;
    this->m_depthFormat = depthFormat;
    this->m_size = size;
    this->m_multisampleQuality = 0.0f;
    this->m_colorFormat = colorFormat;
    this->m_refreshRate = refreshRate;
    this->m_vsync = vsync;
    this->m_aspect = aspect;
    this->m_stereoEnabled = false;
    this->m_maximize = maximize;
    this->m_backBufferCount = 1;
    this->m_multisampleCount = 1;

    for (uint32_t i = 0; i < 6; i++) {
        this->m_fixedFunction[i] = -1;
    }
}
