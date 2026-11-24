#ifndef WOW_SOURCE_WORLDCLIENT_WATERRIPPLE_H
#define WOW_SOURCE_WORLDCLIENT_WATERRIPPLE_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C3Vector.h"

class CGxShader;

class CWaterRipple {
    public:
    enum TYPE : int32_t {
        SPLASH = 0,
        WAKE = 1,
    };

    /* 0000 */ C3Vector m_position;
    /* 000C */ float m_rotation;
    /* 0010 */ float m_size;
    /* 0014 */ float m_growth;
    /* 0018 */ float m_alpha;
    /* 001C */ float m_maxAlpha;
    /* 0020 */ float m_fadeIn;
    /* 0024 */ float m_fadeOut;
    /* 0028 */ float m_endTime;
    /* 002C */ int32_t m_type;
    /* 0030 */ TSLink<CWaterRipple> m_link;
    /* 0038 */ TSLink<CWaterRipple> m_bucketLink;
    /* 0040 */ TSGrowableArray<C3Vector> m_verts;

    CWaterRipple();
    ~CWaterRipple();

    void InitRipple(const C3Vector& pos, float rotation, float size, float alpha, float duration, float growth, int32_t type);
};

class WaterRipple {
    public:
    static void Create(const C3Vector& pos, float facing, float size, float alpha, float duration, float growth, int32_t type, int32_t player);
    static uint32_t s_nextPlayerRipple;
    static uint32_t s_nextUnitRipple;
    static CGxShader* s_psWaterRipples;
    static CGxShader* s_vsWaterRipples;

    static void Destroy();
    static void Initialize();
};

#endif
