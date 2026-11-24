#ifndef ENGINE_SOURCE_GX_CGXLIGHT_H
#define ENGINE_SOURCE_GX_CGXLIGHT_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C4Vector.h"

class CGxLight {
    public:
    enum FLAGS : uint32_t {
        F_ENABLED = 0x1,
        F_OMNI = 0x2,
        F_STATE_MASK = 0x3,
    };

    /* 0000 */ uint32_t m_flags;
    /* 0004 */ C3Vector m_dir;
    /* 0010 */ C3Vector m_ambientColor;
    /* 001C */ C3Vector m_dirColor;
    /* 0028 */ C3Vector m_specularColor;
    /* 0034 */ float m_constantAttenuation;
    /* 0038 */ float m_linearAttenuation;
    /* 003C */ float m_quadraticAttenuation;

    CGxLight();
};

class CGxApiLight {
    public:
    enum FIELD_MASK : uint16_t {
        FIELD_ENABLE = 0x01,
        FIELD_DIRECTION_OR_POSITION = 0x02,
        FIELD_AMBIENT = 0x04,
        FIELD_DIFFUSE = 0x08,
        FIELD_SPECULAR = 0x10,
        FIELD_CONSTANT_ATTENUATION = 0x20,
        FIELD_LINEAR_ATTENUATION = 0x40,
        FIELD_QUADRATIC_ATTENUATION = 0x80,
        FIELD_ATTENUATION = 0xE0,
        FIELD_ALL = 0xFF,
    };

    /* 0000 */ C4Vector m_dir;
    /* 0010 */ C3Vector m_ambColor;
    /* 001C */ C3Vector m_dirColor;
    /* 0028 */ C3Vector m_specColor;
    /* 0034 */ float m_constantAttenuation;
    /* 0038 */ float m_linearAttenuation;
    /* 003C */ float m_quadraticAttenuation;
    /* 0040 */ int32_t m_enable;
    /* 0044 */ uint16_t m_flags;
    /* 0046 */ uint16_t unk_0046;

    CGxApiLight();
    CGxApiLight& operator=(const CGxLight& light);
};

#endif
