#ifndef WOW_SOURCE_LIQUIDSYSTEM_TYPES_H
#define WOW_SOURCE_LIQUIDSYSTEM_TYPES_H

#include "Engine/Source/Tempest/CRange.h"

#include <cstddef>
#include <cstdint>

enum LIQUID_VERTEX_FORMAT : uint16_t {
    LIQUID_VERTEX_HEIGHT_DEPTH = 0,
    LIQUID_VERTEX_HEIGHT_TEXCOORD = 1,
    LIQUID_VERTEX_DEPTH_ONLY = 2,
};

struct SMLiquidChunkDesc {
    /* 0000 */ uint32_t m_offsetInstances;
    /* 0004 */ uint32_t m_layerCount;
    /* 0008 */ uint32_t m_offsetAttributes;
};

struct SMLiquidInstance {
    /* 0000 */ uint16_t m_liquidType;
    /* 0002 */ uint16_t m_vertexFormat;
    /* 0004 */ float m_minHeight;
    /* 0008 */ float m_maxHeight;
    /* 000C */ uint8_t m_xOffset;
    /* 000D */ uint8_t m_yOffset;
    /* 000E */ uint8_t m_width;
    /* 000F */ uint8_t m_height;
    /* 0010 */ uint32_t m_offsetExistsBitmap;
    /* 0014 */ uint32_t m_offsetVertexData;
};

#endif
