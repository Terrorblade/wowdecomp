// Translates Direct3D 9 shader bytecode (vs_1_1 to vs_3_0, ps_1_1 to ps_3_0) to shader model 4.0
// DXBC with Direct3D 9 semantics, plus the fixed-function steps that follow a shader there (fog and
// alpha test after pixel shaders, clip planes and depth bias after vertex shaders).
//
// Every vertex shader writes the same varyings and every pixel shader reads them at the same
// registers, so any vertex shader links with any pixel shader, as in Direct3D 9.

#ifndef ENGINE_SOURCE_GX_CGXDEVICED3D11_SHADER_SHADERTRANSLATOR_H
#define ENGINE_SOURCE_GX_CGXDEVICED3D11_SHADER_SHADERTRANSLATOR_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Gx/CGxDeviceD3d11/Shader/DxbcWriter.h"
#include "Engine/Source/Gx/Shader/ShaderBytecode.h"
#include "Engine/Source/Gx/Shader/ShaderTypes.h"

namespace GxD3d11 {

using namespace GxShader;

struct PixelShaderVariant {
    // Two bits per sampler (SamplerKind)
    uint32_t m_samplerKinds = 0;
    // ps_1_1 to ps_1_3: three bits per stage 0 to 3, the D3DTTFF count of projected stages
    uint16_t m_projected = 0;
    // The shader discards by the alpha reference
    uint8_t m_alphaTest = 0;
    uint8_t m_padding = 0;

    SamplerKind Kind(uint32_t sampler) const { return static_cast<SamplerKind>((this->m_samplerKinds >> (sampler * 2)) & 0x3); }
    bool operator==(const PixelShaderVariant& other) const;
};

struct ConstantDefinition {
    uint32_t m_register;
    float m_value[4];
};

struct ShaderInfo {
    ShaderStage m_stage = STAGE_VERTEX;
    uint32_t m_major = 0;
    uint32_t m_minor = 0;
    // Float constant registers to upload: [0, m_constantCount)
    uint32_t m_constantCount = 0;
    // Relative reads see def constants through the constant buffer, so the upload writes them in
    bool m_constantRelative = false;
    std::vector<ConstantDefinition> m_definitions;
    uint32_t m_samplerMask = 0;
    uint32_t m_declaredKinds = 0;
    uint32_t m_attributeMask = 0;
};

bool AnalyzeShader(const ShaderProgram& program, ShaderInfo& info, std::string& error);
bool TranslateVertexShader(const ShaderProgram& program, const VertexInputSignature& inputs, std::vector<uint8_t>& code, std::string& error);
bool TranslatePixelShader(const ShaderProgram& program, const PixelShaderVariant& variant, std::vector<uint8_t>& code, std::string& error);

// Registers of the varyings every vertex shader writes and pixel shaders read
enum Varying {
    VARYING_POSITION = 0,
    VARYING_COLOR0 = 1,
    VARYING_COLOR1 = 2,
    VARYING_TEXCOORD0 = 3,
    VARYING_FOG = 11,
    VARYING_CLIP0 = 12,
    VARYING_CLIP1 = 13,
    VARYING_PSIZE = 14,
    VARYING_COUNT = 15,
};

void VaryingSignature(std::vector<GxDxbc::SignatureElement>& elements, bool pixelInput);
void VertexInputSignatureElements(const VertexInputSignature& inputs, uint32_t attributeMask, std::vector<GxDxbc::SignatureElement>& elements);

// Constant buffer slots
enum ConstantBufferSlot {
    CB_FLOAT_CONSTANTS = 0,
    CB_STATE = 1,
    CB_FIXED_FUNCTION = 2,
};

// Layout of cb1 in vertex shaders
struct alignas(16) VertexStateBlock {
    float m_posFixup[4];         // z: depth bias in post-divide units
    float m_clipPlanes[6][4];    // disabled planes are zero
    float m_pointSize[4];        // x: size, y: minimum, z: maximum
};

// Layout of cb1 in pixel shaders
struct alignas(16) PixelStateBlock {
    float m_fogColor[4];
    float m_params[4];           // x: fixed-function fog on, y: alpha reference in 0 to 255
    float m_bumpEnvMat[8][4];    // per stage: 00, 01, 10, 11
    float m_bumpEnvLum[8][4];    // per stage: x scale, y offset
    float m_textureFactor[4];
};

enum PixelStateRegister {
    PSREG_FOG_COLOR = 0,
    PSREG_PARAMS = 1,
    PSREG_BUMP_MAT = 2,
    PSREG_BUMP_LUM = 10,
    PSREG_TEXTURE_FACTOR = 18,
    PSREG_COUNT = 19,
};

enum VertexStateRegister {
    VSREG_POS_FIXUP = 0,
    VSREG_CLIP_PLANES = 1,
    VSREG_POINT_SIZE = 7,
    VSREG_COUNT = 8,
};

// The fog blend and alpha test every pixel shader ends with; color is the temp holding the result,
// scratch and scratch + 1 are free temps
void EmitPixelEpilogue(GxDxbc::Writer& writer, uint32_t color, uint32_t scratch, bool fog, bool alphaTest);

// The position with depth bias, the six clip distances and the clamped point size (pointSize.x) every
// vertex shader ends with
void EmitVertexEpilogue(GxDxbc::Writer& writer, uint32_t position, uint32_t clipSource, uint32_t pointSize);

// Output declarations of the varyings, shared by translated and fixed-function vertex shaders
void DeclareVaryingOutputs(GxDxbc::Writer& writer);

}

#endif
