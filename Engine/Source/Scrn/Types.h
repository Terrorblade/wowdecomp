#ifndef ENGINE_SOURCE_SCRN_TYPES_H
#define ENGINE_SOURCE_SCRN_TYPES_H

#include <cstdint>

enum SCRNSTOCK {
    STOCK_SYSFONT = 0,
    STOCK_PERFFONT = 1,
    SCRNSTOCKOBJECTS = 2,
};

enum SCRN_LAYER_FLAGS : uint32_t {
    SCRN_LAYER_TRANSPARENT = 0x1,
    SCRN_LAYER_ORTHO = 0x2,
    SCRN_LAYER_FULLSCREEN_VIEWPORT = 0x4,
};

struct HLAYER__ {
    /* 0000 */ int32_t m_unused;
};

typedef HLAYER__* HLAYER;

#endif
