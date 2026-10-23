// The client's fixed-function pipeline as cache keys and the constant block its generated vertex
// shaders read, shared by the Metal and Direct3D 11 devices: vertex transformation, lighting (the sun
// and up to seven point lights), texture coordinate generation and transforms, vertex fog and point
// size, and the texture stage cascade of the client's colour and alpha operations.
//
// The keys are built from the device's native state, which IRsSendToHw and the IStateSync functions
// fill from the client's render states. The arithmetic follows what the original Direct3D 9 device
// computes for the states CGxDeviceD3d sets, so the result matches the Windows client.

#ifndef ENGINE_SOURCE_GX_SHADER_FIXEDFUNCTION_H
#define ENGINE_SOURCE_GX_SHADER_FIXEDFUNCTION_H

#include <cstdint>
#include <cstring>
#include "Engine/Source/Gx/Shader/ShaderTypes.h"

namespace GxShader {

static const uint32_t kMaxLights = 8;

// Where a lit colour takes its material colour from
enum MaterialSource : uint8_t {
    MaterialSource_Material = 0,
    MaterialSource_Color0 = 1,
    MaterialSource_Color1 = 2,
};

enum LightType : uint8_t {
    LightType_None = 0,
    LightType_Point = 1,
    LightType_Directional = 2,
};

// Texture coordinate generation in view space (CGxDeviceD3d::ISetTexCoordIndex): object and eye
// linear generation take the view-space position and the client's texgen matrix; the sphere map
// takes the reflection vector and a matrix that scales it into [0, 1]
enum TexGen : uint8_t {
    TexGen_None = 0,
    TexGen_ViewPosition = 1,
    TexGen_ViewNormal = 2,
    TexGen_ViewReflection = 3,
};

// The client's texture stage operations (GxRs_ColorOp*, GxRs_AlphaOp*), each combining the
// stage's texture with the result of the previous stages ("current")
enum StageOp : uint8_t {
    StageOp_Modulate = 0,       // texture * current
    StageOp_Modulate2x = 1,     // texture * current * 2
    StageOp_Add = 2,            // texture + current
    StageOp_PassThrough = 3,    // current
    StageOp_Decal = 4,          // current over texture by current alpha
    StageOp_Fade = 5,           // texture over current by vertex (diffuse) alpha
    StageOp_Disable = 6,        // the stage and every later one are off
};

struct FixedVertexKey {
    uint8_t m_attributes[ATTRIB_COUNT];   // AttributeClass per attribute
    uint8_t m_texcoordComponents[8];      // components of each input texture coordinate set
    uint8_t m_lighting;
    uint8_t m_lightTypes[kMaxLights];     // LightType of enabled lights
    uint8_t m_diffuseSource;              // MaterialSource, resolved against the vertex format
    uint8_t m_ambientSource;
    uint8_t m_specularSource;
    uint8_t m_emissiveSource;
    uint8_t m_normalizeNormals;
    uint8_t m_texcoordIndex[8];           // input texture coordinate set of each stage
    uint8_t m_texgen[8];                  // TexGen
    uint8_t m_transformCount[8];          // components the texture transform produces, 0 without one
    uint8_t m_fog;                        // linear vertex fog
    uint8_t m_pointScale;
    uint8_t m_padding[2];

    FixedVertexKey() { memset(this, 0, sizeof(*this)); }
    bool operator==(const FixedVertexKey& other) const { return memcmp(this, &other, sizeof(*this)) == 0; }
};

struct FixedPixelStage {
    uint8_t m_colorOp;                    // StageOp
    uint8_t m_alphaOp;                    // StageOp
    uint8_t m_samplerKind;                // SamplerKind of the bound texture
    uint8_t m_textureBound;
    uint8_t m_projected;                  // divide by component m_projected - 1 (0: no division)
    uint8_t m_padding[3];
};

struct FixedPixelKey {
    FixedPixelStage m_stages[8];
    uint8_t m_specularEnable;
    uint8_t m_pointSprite;
    uint8_t m_padding[2];

    FixedPixelKey() { memset(this, 0, sizeof(*this)); }
    bool operator==(const FixedPixelKey& other) const { return memcmp(this, &other, sizeof(*this)) == 0; }
};

// Layout of the fixed-function vertex constants. Matrices are the client's row-major matrices
// (v * M), one row per register.
struct alignas(16) FixedLightBlock {
    float m_diffuse[4];
    float m_specular[4];
    float m_ambient[4];
    float m_position[4];                  // view space
    float m_direction[4];                 // view space
    float m_range;
    float m_attenuation[3];
};

struct alignas(16) FixedVertexStateBlock {
    float m_world[16];
    float m_worldView[16];
    float m_normal[16];                   // inverse transpose of world * view
    float m_projection[16];
    float m_texture[8][16];
    float m_globalAmbient[4];
    float m_materialDiffuse[4];
    float m_materialAmbient[4];
    float m_materialSpecular[4];
    float m_materialEmissive[4];
    float m_materialPower[4];
    FixedLightBlock m_lights[kMaxLights];
    float m_fog[4];                       // x: start, y: end, z: 1 / (end - start)
    float m_pointScale[4];                // x: A, y: B, z: C, w: viewport height
};

}

#endif
