// Translates Direct3D 9 shader bytecode (vs_1_1 to vs_3_0, ps_1_1 to ps_3_0) to Metal Shading
// Language with Direct3D 9 semantics: rasterization rules, register clamping, the special cases
// of the scalar math instructions, and the fixed-function stages that follow a shader (fog and
// alpha test after pixel shaders, clip planes and depth bias after vertex shaders).
//
// Every vertex function writes the same varyings struct and every fragment function reads a
// subset of it, so any vertex function links with any fragment function, as in Direct3D 9.

#ifndef ENGINE_SOURCE_GX_CGXDEVICEMETAL_SHADER_SHADERTRANSLATOR_H
#define ENGINE_SOURCE_GX_CGXDEVICEMETAL_SHADER_SHADERTRANSLATOR_H

#include "Engine/Source/Gx/Shader/ShaderBytecode.h"
#include "Engine/Source/Gx/Shader/ShaderTypes.h"
#include <cstdint>
#include <string>

namespace GxMetal {

using namespace GxShader;

struct PixelShaderVariant {
    // Two bits per sampler (SamplerKind)
    uint32_t m_samplerKinds = 0;
    // ps_1_1 to ps_1_3: three bits per stage 0 to 3, the D3DTTFF count when D3DTTFF_PROJECTED is
    // set, which divides the texture coordinates of tex by component count - 1
    uint16_t m_projected = 0;
    // Point sprites replace every texture coordinate with the point coordinate
    uint8_t m_pointSprite = 0;

    SamplerKind Kind(uint32_t sampler) const { return static_cast<SamplerKind>((m_samplerKinds >> (sampler * 2)) & 0x3); }
    bool operator==(const PixelShaderVariant& other) const;
};

// Static properties of a shader the device needs to bind it
struct ShaderInfo {
    ShaderStage m_stage = STAGE_VERTEX;
    uint32_t m_major = 0;
    uint32_t m_minor = 0;
    // Float constant registers to upload: [0, m_constantCount)
    uint32_t m_constantCount = 0;
    bool m_usesIntConstants = false;
    bool m_usesBoolConstants = false;
    // Samplers read by the shader
    uint32_t m_samplerMask = 0;
    // Kinds declared by dcl (shader model 2 and up); shader model 1 takes them from the variant
    uint32_t m_declaredKinds = 0;
    // Vertex attributes read by a vertex shader
    uint32_t m_attributeMask = 0;
};

bool AnalyzeShader(const ShaderProgram& program, ShaderInfo& info, std::string& error);

// Function names in the translated source
extern const char* const kVertexFunctionName;
extern const char* const kFragmentFunctionName;

bool TranslateVertexShader(const ShaderProgram& program, const VertexInputSignature& inputs, std::string& source, std::string& error);
bool TranslatePixelShader(const ShaderProgram& program, const PixelShaderVariant& variant, std::string& source, std::string& error);

// Source shared by translated and fixed-function shaders: the varyings, the per-draw state the
// device passes in (D3DVertexState, D3DPixelState) and Direct3D 9 math helpers.
extern const char* const kShaderPrelude;

// Buffer and texture indices
enum BufferIndex {
    // Vertex stage: vertex streams use buffer indices 0 to 15
    VS_BUFFER_FLOAT_CONSTANTS = 16,
    VS_BUFFER_INT_CONSTANTS = 17,
    VS_BUFFER_BOOL_CONSTANTS = 18,
    VS_BUFFER_STATE = 19,
    VS_BUFFER_FIXED_FUNCTION = 20,

    PS_BUFFER_FLOAT_CONSTANTS = 0,
    PS_BUFFER_INT_CONSTANTS = 1,
    PS_BUFFER_BOOL_CONSTANTS = 2,
    PS_BUFFER_STATE = 3,
};

// Layout of D3DVertexState and D3DPixelState in kShaderPrelude
struct alignas(16) VertexStateBlock {
    float m_posFixup[4];         // xy: half-pixel offset per unit of w, z: depth bias
    float m_clipPlanes[6][4];
    uint32_t m_flags[4];         // x: enabled clip planes
    float m_pointSize[4];        // x: size, y: minimum, z: maximum
};

// Comparison codes of d3d_compare in kShaderPrelude (the alpha test function), numbered as the
// bytecode's own comparison functions
enum ShaderCompare : uint32_t {
    SHADERCOMPARE_NEVER = 1,
    SHADERCOMPARE_LESS = 2,
    SHADERCOMPARE_EQUAL = 3,
    SHADERCOMPARE_LESSEQUAL = 4,
    SHADERCOMPARE_GREATER = 5,
    SHADERCOMPARE_NOTEQUAL = 6,
    SHADERCOMPARE_GREATEREQUAL = 7,
    SHADERCOMPARE_ALWAYS = 8,
};

struct alignas(16) PixelStateBlock {
    float m_fogColor[4];
    uint32_t m_flags[4];         // x: apply fixed-function fog, y: alpha func (ShaderCompare), z: alpha ref
    float m_bumpEnvMat[8][4];    // per stage: 00, 01, 10, 11
    float m_bumpEnvLum[8][4];    // per stage: x scale, y offset
    float m_textureFactor[4];
};

}

#endif
