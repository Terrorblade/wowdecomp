// What every shader backend needs to know about the client's vertex attributes and textures, shared
// by the Metal and Direct3D 11 translators.

#ifndef ENGINE_SOURCE_GX_SHADER_SHADERTYPES_H
#define ENGINE_SOURCE_GX_SHADER_SHADERTYPES_H

#include <cstdint>

namespace GxShader {

// Vertex attribute indices. The client builds vertex declarations from a fixed set of usages, so
// each (usage, usage index) pair it uses has its own attribute index.
enum VertexAttribute {
    ATTRIB_POSITION = 0,
    ATTRIB_BLENDWEIGHT = 1,
    ATTRIB_BLENDINDICES = 2,
    ATTRIB_NORMAL = 3,
    ATTRIB_COLOR0 = 4,
    ATTRIB_COLOR1 = 5,
    ATTRIB_TEXCOORD0 = 6,
    ATTRIB_COUNT = 14,
};

// Returns the attribute index of a declaration usage, or -1 if it has none.
int32_t AttributeIndex(uint32_t usage, uint32_t usageIndex);

// How a vertex attribute reaches the vertex shader: its format decides whether the shader reads it as
// a float, unsigned or signed integer vector.
enum AttributeClass : uint8_t {
    ATTRIBCLASS_NONE = 0,
    ATTRIBCLASS_FLOAT = 1,
    ATTRIBCLASS_UINT = 2,
    ATTRIBCLASS_SINT = 3,
};

struct VertexInputSignature {
    uint8_t m_class[ATTRIB_COUNT] = {};

    bool operator==(const VertexInputSignature& other) const;
};

// What the bound textures are, for samplers whose kind the shader does not declare (shader
// model 1) or cannot know (depth textures sample as shadow comparisons).
enum SamplerKind : uint32_t {
    SAMPLERKIND_2D = 0,
    SAMPLERKIND_CUBE = 1,
    SAMPLERKIND_DEPTH = 2,
    SAMPLERKIND_VOLUME = 3,
};

}

#endif
