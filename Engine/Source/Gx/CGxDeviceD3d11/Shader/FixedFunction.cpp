#include "Engine/Source/Gx/CGxDeviceD3d11/Shader/FixedFunction.h"

#include <cfloat>
#include <cstdio>

namespace GxD3d11 {

using GxDxbc::CB;
using GxDxbc::ImmF;
using GxDxbc::O;
using GxDxbc::Operand;
using GxDxbc::R;
using GxDxbc::Sampler;
using GxDxbc::SignatureElement;
using GxDxbc::Texture;
using GxDxbc::V;
using GxDxbc::Writer;

namespace {

enum LightRegister {
    LIGHT_DIFFUSE = 0,
    LIGHT_SPECULAR = 1,
    LIGHT_AMBIENT = 2,
    LIGHT_POSITION = 3,
    LIGHT_DIRECTION = 4,
    LIGHT_RANGE = 5,         // x: range, y z w: attenuation 0 1 2
};

Operand Splat(float value) {
    return ImmF(value, value, value, value);
}

Operand Dst(uint32_t reg, uint32_t mask = 0xF) {
    return R(reg).Mask(mask);
}

Operand Src(uint32_t reg) {
    return R(reg).Swz("xyzw");
}

Operand Ff(uint32_t reg) {
    return CB(CB_FIXED_FUNCTION, reg).Swz("xyzw");
}

Operand Light(uint32_t light, uint32_t reg) {
    return Ff(FFREG_LIGHTS + light * FFREG_LIGHT_SIZE + reg);
}

class VertexEmitter {
    public:
    VertexEmitter(const FixedVertexKey& key, Writer& writer) : m_key(key), m_writer(writer) {}

    bool Emit();

    uint32_t m_attributeMask = 0;
    uint32_t m_tempCount = 0;

    private:
    Writer& W() { return this->m_writer; }
    uint32_t Temp() { return this->m_tempCount++; }
    uint32_t Load(uint32_t attribute, float x, float y, float z, float w);
    void Transform(uint32_t dst, uint32_t mask, uint32_t src, uint32_t base, uint32_t rows);
    void Normalize(uint32_t reg);
    void Lighting(uint32_t vtx, uint32_t n, uint32_t color0, uint32_t color1);
    Operand MaterialColor(uint8_t source, uint32_t color0, uint32_t color1, uint32_t material);

    const FixedVertexKey& m_key;
    Writer& m_writer;
    uint32_t m_normalizeScratch = 0;
};

// A vertex attribute in a temp, or the fallback when the vertex format has none
uint32_t VertexEmitter::Load(uint32_t attribute, float x, float y, float z, float w) {
    uint32_t reg = this->Temp();
    uint8_t attributeClass = this->m_key.m_attributes[attribute];

    switch (attributeClass) {
        case ATTRIBCLASS_FLOAT:
            W().Op(GxDxbc::OP_MOV, { Dst(reg), V(attribute).Swz("xyzw") });
            break;
        case ATTRIBCLASS_UINT:
            W().Op(GxDxbc::OP_UTOF, { Dst(reg), V(attribute).Swz("xyzw") });
            break;
        case ATTRIBCLASS_SINT:
            W().Op(GxDxbc::OP_ITOF, { Dst(reg), V(attribute).Swz("xyzw") });
            break;
        default:
            W().Op(GxDxbc::OP_MOV, { Dst(reg), ImmF(x, y, z, w) });
            return reg;
    }

    this->m_attributeMask |= 1u << attribute;
    return reg;
}

// dst = src * M for the client's row-major matrix at cb2[base], over the first rows components of src
void VertexEmitter::Transform(uint32_t dst, uint32_t mask, uint32_t src, uint32_t base, uint32_t rows) {
    W().Op(GxDxbc::OP_MUL, { Dst(dst, mask), R(src).Swz("xxxx"), Ff(base) });

    for (uint32_t i = 1; i < rows; i++) {
        W().Op(GxDxbc::OP_MAD, { Dst(dst, mask), R(src).Swz(i, i, i, i), Ff(base + i), Src(dst) });
    }
}

// Normalizes reg.xyz, leaving a zero vector at zero
void VertexEmitter::Normalize(uint32_t reg) {
    uint32_t s = this->m_normalizeScratch;
    W().Op(GxDxbc::OP_DP3, { Dst(s, 0x1), Src(reg), Src(reg) });
    W().Op(GxDxbc::OP_RSQ, { Dst(s, 0x1), R(s).Sel(0) });
    W().Op(GxDxbc::OP_MIN, { Dst(s, 0x1), R(s).Sel(0), ImmF(FLT_MAX) });
    W().Op(GxDxbc::OP_MUL, { Dst(reg, 0x7), Src(reg), R(s).Swz("xxxx") });
}

Operand VertexEmitter::MaterialColor(uint8_t source, uint32_t color0, uint32_t color1, uint32_t material) {
    switch (source) {
        case MaterialSource_Color0: return Src(color0);
        case MaterialSource_Color1: return Src(color1);
        default: return Ff(material);
    }
}

// View-space lighting with a local viewer, as Direct3D 9 computes it
void VertexEmitter::Lighting(uint32_t vtx, uint32_t n, uint32_t color0, uint32_t color1) {
    uint32_t ambientLight = this->Temp();
    uint32_t diffuseLight = this->Temp();
    uint32_t specularLight = this->Temp();
    uint32_t eye = this->Temp();
    uint32_t hit = this->Temp();
    uint32_t mid = this->Temp();
    // x: attenuation, y: distance then hit dot, z: mid dot, w: specularness
    uint32_t terms = this->Temp();

    W().Op(GxDxbc::OP_MOV, { Dst(ambientLight), Splat(0.0f) });
    W().Op(GxDxbc::OP_MOV, { Dst(diffuseLight), Splat(0.0f) });
    W().Op(GxDxbc::OP_MOV, { Dst(specularLight), Splat(0.0f) });
    W().Op(GxDxbc::OP_MOV, { Dst(eye, 0x7), Src(vtx) });
    this->Normalize(eye);

    for (uint32_t i = 0; i < kMaxLights; i++) {
        uint32_t type = this->m_key.m_lightTypes[i];
        if (type == LightType_None) {
            continue;
        }

        if (type == LightType_Directional) {
            W().Op(GxDxbc::OP_MOV, { Dst(hit, 0x7), Light(i, LIGHT_DIRECTION).Neg() });
            this->Normalize(hit);
            W().Op(GxDxbc::OP_MOV, { Dst(terms, 0x1), ImmF(1.0f) });
        } else {
            W().Op(GxDxbc::OP_ADD, { Dst(hit, 0x7), Light(i, LIGHT_POSITION), R(vtx).Swz("xyzw").Neg() });
            W().Op(GxDxbc::OP_DP3, { Dst(terms, 0x2), Src(hit), Src(hit) });
            W().Op(GxDxbc::OP_SQRT, { Dst(terms, 0x2), R(terms).Sel(1) });
            this->Normalize(hit);

            // atten = d > range ? 0 : min(1 / (a0 + d * (a1 + d * a2)), FLT_MAX)
            W().Op(GxDxbc::OP_MAD, { Dst(terms, 0x1), R(terms).Sel(1), CB(CB_FIXED_FUNCTION, FFREG_LIGHTS + i * FFREG_LIGHT_SIZE + LIGHT_RANGE).Sel(3), CB(CB_FIXED_FUNCTION, FFREG_LIGHTS + i * FFREG_LIGHT_SIZE + LIGHT_RANGE).Sel(2) });
            W().Op(GxDxbc::OP_MAD, { Dst(terms, 0x1), R(terms).Sel(1), R(terms).Sel(0), CB(CB_FIXED_FUNCTION, FFREG_LIGHTS + i * FFREG_LIGHT_SIZE + LIGHT_RANGE).Sel(1) });
            W().Op(GxDxbc::OP_DIV, { Dst(terms, 0x1), ImmF(1.0f), R(terms).Sel(0) });
            W().Op(GxDxbc::OP_MIN, { Dst(terms, 0x1), R(terms).Sel(0), ImmF(FLT_MAX) });
            W().Op(GxDxbc::OP_LT, { Dst(terms, 0x4), CB(CB_FIXED_FUNCTION, FFREG_LIGHTS + i * FFREG_LIGHT_SIZE + LIGHT_RANGE).Sel(0), R(terms).Sel(1) });
            W().Op(GxDxbc::OP_MOVC, { Dst(terms, 0x1), R(terms).Sel(2), ImmF(0.0f), R(terms).Sel(0) });
        }

        // The dot products are not clamped to 1, which unnormalized normals exceed
        W().Op(GxDxbc::OP_DP3, { Dst(terms, 0x2), Src(n), Src(hit) });
        W().Op(GxDxbc::OP_MAX, { Dst(terms, 0x2), R(terms).Sel(1), ImmF(0.0f) });
        W().Op(GxDxbc::OP_ADD, { Dst(mid, 0x7), Src(hit), Src(eye).Neg() });
        this->Normalize(mid);
        W().Op(GxDxbc::OP_DP3, { Dst(terms, 0x4), Src(n), Src(mid) });
        W().Op(GxDxbc::OP_MAX, { Dst(terms, 0x4), R(terms).Sel(2), ImmF(0.0f) });

        // specularness = hitDot > 0 && midDot > 0 ? midDot ^ power * atten : 0
        W().Op(GxDxbc::OP_LOG, { Dst(terms, 0x8), R(terms).Sel(2) });
        W().Op(GxDxbc::OP_MUL, { Dst(terms, 0x8), R(terms).Sel(3), CB(CB_FIXED_FUNCTION, FFREG_MATERIAL_POWER).Sel(0) });
        W().Op(GxDxbc::OP_EXP, { Dst(terms, 0x8), R(terms).Sel(3) });
        W().Op(GxDxbc::OP_MUL, { Dst(terms, 0x8), R(terms).Sel(3), R(terms).Sel(0) });
        W().Op(GxDxbc::OP_LT, { Dst(mid, 0x3), ImmF(0.0f, 0.0f, 0.0f, 0.0f), R(terms).Swz("zyzy") });
        W().Op(GxDxbc::OP_AND, { Dst(mid, 0x1), R(mid).Sel(0), R(mid).Sel(1) });
        W().Op(GxDxbc::OP_MOVC, { Dst(terms, 0x8), R(mid).Sel(0), R(terms).Sel(3), ImmF(0.0f) });

        W().Op(GxDxbc::OP_MAD, { Dst(ambientLight), Light(i, LIGHT_AMBIENT), R(terms).Swz("xxxx"), Src(ambientLight) });
        W().Op(GxDxbc::OP_MUL, { Dst(mid, 0x1), R(terms).Sel(1), R(terms).Sel(0) });
        W().Op(GxDxbc::OP_MAD, { Dst(diffuseLight), Light(i, LIGHT_DIFFUSE), R(mid).Swz("xxxx"), Src(diffuseLight) });
        W().Op(GxDxbc::OP_MAD, { Dst(specularLight), Light(i, LIGHT_SPECULAR), R(terms).Swz("wwww"), Src(specularLight) });
    }

    Operand diffuse = this->MaterialColor(this->m_key.m_diffuseSource, color0, color1, FFREG_MATERIAL_DIFFUSE);
    Operand ambient = this->MaterialColor(this->m_key.m_ambientSource, color0, color1, FFREG_MATERIAL_AMBIENT);
    Operand emissive = this->MaterialColor(this->m_key.m_emissiveSource, color0, color1, FFREG_MATERIAL_EMISSIVE);
    Operand specular = this->MaterialColor(this->m_key.m_specularSource, color0, color1, FFREG_MATERIAL_SPECULAR);

    uint32_t lit = this->Temp();
    W().Op(GxDxbc::OP_MAD, { Dst(lit), ambient, Ff(FFREG_GLOBAL_AMBIENT), emissive });
    W().Op(GxDxbc::OP_MAD, { Dst(lit), ambient, Src(ambientLight), Src(lit) });
    W().Op(GxDxbc::OP_MAD, { Dst(lit), diffuse, Src(diffuseLight), Src(lit) });
    W().Op(GxDxbc::OP_MOV, { Dst(lit, 0x8), diffuse });
    W().Op(GxDxbc::OP_MOV, { O(VARYING_COLOR0).Mask(0xF), Src(lit) }, true);
    W().Op(GxDxbc::OP_MUL, { O(VARYING_COLOR1).Mask(0xF), specular, Src(specularLight) }, true);
}

bool VertexEmitter::Emit() {
    const FixedVertexKey& key = this->m_key;
    this->m_normalizeScratch = this->Temp();

    bool needsNormal = key.m_lighting != 0;
    for (uint32_t i = 0; i < 8; i++) {
        needsNormal = needsNormal || key.m_texgen[i] == TexGen_ViewNormal || key.m_texgen[i] == TexGen_ViewReflection;
    }

    uint32_t position = this->Load(ATTRIB_POSITION, 0.0f, 0.0f, 0.0f, 1.0f);
    uint32_t color0 = this->Load(ATTRIB_COLOR0, 1.0f, 1.0f, 1.0f, 1.0f);
    uint32_t color1 = this->Load(ATTRIB_COLOR1, 0.0f, 0.0f, 0.0f, 0.0f);

    uint32_t worldPos = this->Temp();
    uint32_t vtx = this->Temp();
    uint32_t oPos = this->Temp();
    this->Transform(worldPos, 0xF, position, FFREG_WORLD, 4);
    this->Transform(vtx, 0xF, position, FFREG_WORLD_VIEW, 4);
    this->Transform(oPos, 0xF, vtx, FFREG_PROJECTION, 4);

    uint32_t n = 0;
    if (needsNormal) {
        uint32_t normal = this->Load(ATTRIB_NORMAL, 0.0f, 0.0f, 0.0f, 0.0f);
        n = this->Temp();
        this->Transform(n, 0x7, normal, FFREG_NORMAL, 3);
        W().Op(GxDxbc::OP_MOV, { Dst(n, 0x8), ImmF(0.0f) });

        if (key.m_normalizeNormals) {
            this->Normalize(n);
        }
    }

    // Texture coordinates: generation, then the texture transform
    uint32_t tc = this->Temp();
    uint32_t transformed = this->Temp();
    uint32_t reflection = 0;

    for (uint32_t i = 0; i < 8; i++) {
        uint32_t inputIndex = key.m_texcoordIndex[i] & 7;
        uint32_t transform = key.m_transformCount[i] > 4 ? 4 : key.m_transformCount[i];
        uint32_t texcoordCount = key.m_texcoordComponents[inputIndex];

        switch (key.m_texgen[i]) {
            case TexGen_ViewNormal:
                W().Op(GxDxbc::OP_MOV, { Dst(tc, 0x7), Src(n) });
                W().Op(GxDxbc::OP_MOV, { Dst(tc, 0x8), ImmF(1.0f) });
                break;

            case TexGen_ViewPosition:
                W().Op(GxDxbc::OP_MOV, { Dst(tc, 0x7), Src(vtx) });
                W().Op(GxDxbc::OP_MOV, { Dst(tc, 0x8), ImmF(1.0f) });
                break;

            case TexGen_ViewReflection:
                // reflect(I, n) = I - 2 * dot(n, I) * n with I the normalized view position
                if (!reflection) {
                    reflection = this->Temp();
                    W().Op(GxDxbc::OP_MOV, { Dst(reflection, 0x7), Src(vtx) });
                    this->Normalize(reflection);
                    W().Op(GxDxbc::OP_DP3, { Dst(reflection, 0x8), Src(n), Src(reflection) });
                    W().Op(GxDxbc::OP_ADD, { Dst(reflection, 0x8), R(reflection).Sel(3), R(reflection).Sel(3) });
                    W().Op(GxDxbc::OP_MAD, { Dst(reflection, 0x7), R(reflection).Swz("wwww").Neg(), Src(n), Src(reflection) });
                }
                W().Op(GxDxbc::OP_MOV, { Dst(tc, 0x7), Src(reflection) });
                W().Op(GxDxbc::OP_MOV, { Dst(tc, 0x8), ImmF(1.0f) });
                break;

            default: {
                uint32_t input = this->Load(ATTRIB_TEXCOORD0 + inputIndex, 0.0f, 0.0f, 0.0f, 1.0f);
                W().Op(GxDxbc::OP_MOV, { Dst(tc), Src(input) });

                if (!texcoordCount) {
                    transform = 0;
                }

                // Before the transform the first missing component reads as 1 and the rest as 0
                for (uint32_t j = texcoordCount; transform && j < 4; j++) {
                    W().Op(GxDxbc::OP_MOV, { Dst(tc, 1u << j), ImmF(j > texcoordCount ? 0.0f : 1.0f) });
                }
                break;
            }
        }

        uint32_t result = tc;

        if (transform) {
            this->Transform(transformed, 0xF, tc, FFREG_TEXTURE + i * 4, 4);

            // Components past the count repeat the last one, the divisor of projected lookups
            for (uint32_t j = transform; j < 4; j++) {
                W().Op(GxDxbc::OP_MOV, { Dst(transformed, 1u << j), R(transformed).Sel(transform - 1) });
            }

            result = transformed;
        }

        W().Op(GxDxbc::OP_MOV, { O(VARYING_TEXCOORD0 + i).Mask(0xF), Src(result) });
    }

    if (key.m_lighting) {
        this->Lighting(vtx, n, color0, color1);
    } else {
        W().Op(GxDxbc::OP_MOV, { O(VARYING_COLOR0).Mask(0xF), Src(color0) }, true);
        W().Op(GxDxbc::OP_MOV, { O(VARYING_COLOR1).Mask(0xF), Src(color1) }, true);
    }

    // Linear vertex fog by view depth (the client's only fog mode)
    if (key.m_fog) {
        uint32_t fog = this->Temp();
        W().Op(GxDxbc::OP_ADD, { Dst(fog, 0x1), CB(CB_FIXED_FUNCTION, FFREG_FOG).Sel(1), R(vtx).Sel(2).Abs().Neg() });
        W().Op(GxDxbc::OP_MUL, { O(VARYING_FOG).Mask(0x1), R(fog).Sel(0), CB(CB_FIXED_FUNCTION, FFREG_FOG).Sel(2) }, true);
    } else {
        W().Op(GxDxbc::OP_MOV, { O(VARYING_FOG).Mask(0x1), ImmF(1.0f) });
    }

    // Point size, scaled by distance when point scaling is on
    uint32_t pointSize = this->Temp();
    W().Op(GxDxbc::OP_MOV, { Dst(pointSize, 0x1), CB(CB_STATE, VSREG_POINT_SIZE).Sel(0) });

    if (key.m_pointScale) {
        // size *= height * sqrt(1 / (A + d * (B + d * C)))
        W().Op(GxDxbc::OP_DP3, { Dst(pointSize, 0x2), Src(vtx), Src(vtx) });
        W().Op(GxDxbc::OP_SQRT, { Dst(pointSize, 0x2), R(pointSize).Sel(1) });
        W().Op(GxDxbc::OP_MAD, { Dst(pointSize, 0x4), R(pointSize).Sel(1), CB(CB_FIXED_FUNCTION, FFREG_POINT_SCALE).Sel(2), CB(CB_FIXED_FUNCTION, FFREG_POINT_SCALE).Sel(1) });
        W().Op(GxDxbc::OP_MAD, { Dst(pointSize, 0x4), R(pointSize).Sel(1), R(pointSize).Sel(2), CB(CB_FIXED_FUNCTION, FFREG_POINT_SCALE).Sel(0) });
        W().Op(GxDxbc::OP_DIV, { Dst(pointSize, 0x4), ImmF(1.0f), R(pointSize).Sel(2) });
        W().Op(GxDxbc::OP_SQRT, { Dst(pointSize, 0x4), R(pointSize).Sel(2) });
        W().Op(GxDxbc::OP_MUL, { Dst(pointSize, 0x4), R(pointSize).Sel(2), CB(CB_FIXED_FUNCTION, FFREG_POINT_SCALE).Sel(3) });
        W().Op(GxDxbc::OP_MUL, { Dst(pointSize, 0x1), R(pointSize).Sel(0), R(pointSize).Sel(2) });
    }

    // User clip planes are in world space for fixed-function vertex processing
    EmitVertexEpilogue(this->m_writer, oPos, worldPos, pointSize);
    return true;
}

class PixelEmitter {
    public:
    PixelEmitter(const FixedPixelKey& key, Writer& writer, std::string& error) : m_key(key), m_writer(writer), m_error(error) {}

    bool Emit(bool alphaTest);

    uint32_t m_samplerMask = 0;
    uint32_t m_usedVaryings = 0;
    uint32_t m_tempCount = 0;

    private:
    Writer& W() { return this->m_writer; }
    uint32_t Temp() { return this->m_tempCount++; }
    bool Operation(uint8_t operation, uint32_t dst, uint32_t texel, uint32_t current);

    const FixedPixelKey& m_key;
    Writer& m_writer;
    std::string& m_error;
};

// One texture stage operation of the stage's texture with the current color
bool PixelEmitter::Operation(uint8_t operation, uint32_t dst, uint32_t texel, uint32_t current) {
    switch (operation) {
        case StageOp_Modulate:
            W().Op(GxDxbc::OP_MUL, { Dst(dst), Src(texel), Src(current) });
            return true;
        case StageOp_Modulate2x:
            W().Op(GxDxbc::OP_MUL, { Dst(dst), Src(texel), Src(current) });
            W().Op(GxDxbc::OP_ADD, { Dst(dst), Src(dst), Src(dst) }, true);
            return true;
        case StageOp_Add:
            W().Op(GxDxbc::OP_ADD, { Dst(dst), Src(texel), Src(current) }, true);
            return true;
        case StageOp_PassThrough:
            W().Op(GxDxbc::OP_MOV, { Dst(dst), Src(current) });
            return true;
        case StageOp_Decal:
            // texture + (current - texture) * current.a
            W().Op(GxDxbc::OP_ADD, { Dst(dst), Src(current), Src(texel).Neg() });
            W().Op(GxDxbc::OP_MAD, { Dst(dst), Src(dst), R(current).Swz("wwww"), Src(texel) });
            return true;
        case StageOp_Fade:
            // current + (texture - current) * diffuse.a
            W().Op(GxDxbc::OP_ADD, { Dst(dst), Src(texel), Src(current).Neg() });
            W().Op(GxDxbc::OP_MAD, { Dst(dst), Src(dst), V(VARYING_COLOR0).Swz("wwww"), Src(current) });
            return true;
        default: {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "unsupported texture operation %u", operation);
            this->m_error = buffer;
            return false;
        }
    }
}

bool PixelEmitter::Emit(bool alphaTest) {
    const FixedPixelKey& key = this->m_key;

    // Stages in use: up to the first one whose colour operation is disabled
    uint32_t stageCount = 0;
    while (stageCount < 8 && key.m_stages[stageCount].m_colorOp != StageOp_Disable) {
        stageCount++;
    }

    uint32_t current = this->Temp();
    uint32_t texel = this->Temp();
    uint32_t color = this->Temp();
    uint32_t alpha = this->Temp();
    uint32_t scratch = this->Temp();
    this->Temp();

    this->m_usedVaryings |= 1u << VARYING_COLOR0;
    W().Op(GxDxbc::OP_MOV, { Dst(current), V(VARYING_COLOR0).Swz("xyzw") });

    for (uint32_t i = 0; i < stageCount; i++) {
        const FixedPixelStage& stage = key.m_stages[i];

        if (stage.m_textureBound) {
            // Point sprites need no case here, the geometry shader replaces every texture coordinate
            this->m_samplerMask |= 1u << i;
            this->m_usedVaryings |= 1u << (VARYING_TEXCOORD0 + i);
            Operand coord = V(VARYING_TEXCOORD0 + i).Swz("xyzw");

            if (stage.m_projected) {
                // The divisor is the last counted component, or w
                uint32_t divisor = stage.m_projected >= 2 && stage.m_projected <= 4 ? stage.m_projected - 1 : 3;
                W().Op(GxDxbc::OP_DIV, { Dst(texel), coord, V(VARYING_TEXCOORD0 + i).Swz(divisor, divisor, divisor, divisor) });
                coord = Src(texel);
            }

            if (stage.m_samplerKind == SAMPLERKIND_DEPTH) {
                W().Op(GxDxbc::OP_SAMPLE_C_LZ, { Dst(texel, 0x1), coord, Texture(i).Swz("xxxx"), Sampler(i), coord.Sel(2) });
                W().Op(GxDxbc::OP_MOV, { Dst(texel), R(texel).Swz("xxxx") });
            } else {
                W().Op(GxDxbc::OP_SAMPLE, { Dst(texel), coord, Texture(i).Swz("xyzw"), Sampler(i) });
            }
        } else {
            W().Op(GxDxbc::OP_MOV, { Dst(texel), ImmF(0.0f, 0.0f, 0.0f, 1.0f) });
        }

        if (!this->Operation(stage.m_colorOp, color, texel, current)) {
            return false;
        }

        if (stage.m_colorOp == stage.m_alphaOp) {
            W().Op(GxDxbc::OP_MOV, { Dst(current), Src(color) });
            continue;
        }

        if (stage.m_alphaOp != StageOp_Disable) {
            if (!this->Operation(stage.m_alphaOp, alpha, texel, current)) {
                return false;
            }
            W().Op(GxDxbc::OP_MOV, { Dst(current, 0x8), Src(alpha) });
        }

        W().Op(GxDxbc::OP_MOV, { Dst(current, 0x7), Src(color) });
    }

    if (key.m_specularEnable) {
        this->m_usedVaryings |= 1u << VARYING_COLOR1;
        W().Op(GxDxbc::OP_ADD, { Dst(current, 0x7), Src(current), V(VARYING_COLOR1).Swz("xyzw") });
    }

    this->m_usedVaryings |= 1u << VARYING_FOG;
    EmitPixelEpilogue(this->m_writer, current, scratch, true, alphaTest);
    W().Op(GxDxbc::OP_MOV, { O(0).Mask(0xF), Src(current) });
    return true;
}

}

bool GenerateFixedVertexShader(const FixedVertexKey& key, std::vector<uint8_t>& code, std::string& error) {
    Writer writer(Writer::STAGE_VERTEX);
    VertexEmitter emitter(key, writer);

    if (!emitter.Emit()) {
        return false;
    }

    writer.DclGlobalFlags(1);
    writer.DclConstantBuffer(CB_STATE, VSREG_COUNT, false);
    writer.DclConstantBuffer(CB_FIXED_FUNCTION, FFREG_COUNT, false);

    for (uint32_t i = 0; i < ATTRIB_COUNT; i++) {
        if (emitter.m_attributeMask & (1u << i)) {
            writer.DclInput(i, 0xF);
        }
    }

    DeclareVaryingOutputs(writer);
    writer.DclTemps(emitter.m_tempCount);

    VertexInputSignature inputs;
    for (uint32_t i = 0; i < ATTRIB_COUNT; i++) {
        inputs.m_class[i] = key.m_attributes[i];
    }

    VertexInputSignatureElements(inputs, emitter.m_attributeMask, writer.m_inputs);
    VaryingSignature(writer.m_outputs, false);
    code = writer.Finish();
    (void)error;
    return true;
}

bool GenerateFixedPixelShader(const FixedPixelKey& key, bool alphaTest, std::vector<uint8_t>& code, std::string& error) {
    Writer writer(Writer::STAGE_PIXEL);
    PixelEmitter emitter(key, writer, error);

    if (!emitter.Emit(alphaTest)) {
        return false;
    }

    writer.DclGlobalFlags(1);
    writer.DclConstantBuffer(CB_STATE, PSREG_COUNT, false);

    for (uint32_t i = 0; i < 8; i++) {
        if (emitter.m_samplerMask & (1u << i)) {
            writer.DclSampler(i, key.m_stages[i].m_samplerKind == SAMPLERKIND_DEPTH);
        }
    }

    for (uint32_t i = 0; i < 8; i++) {
        if (!(emitter.m_samplerMask & (1u << i))) {
            continue;
        }

        switch (key.m_stages[i].m_samplerKind) {
            case SAMPLERKIND_CUBE: writer.DclResource(i, GxDxbc::DIM_TEXTURECUBE); break;
            case SAMPLERKIND_VOLUME: writer.DclResource(i, GxDxbc::DIM_TEXTURE3D); break;
            default: writer.DclResource(i, GxDxbc::DIM_TEXTURE2D); break;
        }
    }

    for (uint32_t i = VARYING_COLOR0; i <= VARYING_FOG; i++) {
        if (emitter.m_usedVaryings & (1u << i)) {
            writer.DclInputPs(i, i == VARYING_FOG ? 0x1 : 0xF, GxDxbc::INTERP_LINEAR);
        }
    }

    writer.DclOutput(0, 0xF);
    writer.DclTemps(emitter.m_tempCount);

    VaryingSignature(writer.m_inputs, true);

    for (SignatureElement& element : writer.m_inputs) {
        if (element.m_register < 32 && (emitter.m_usedVaryings & (1u << element.m_register))) {
            element.m_readWriteMask = element.m_mask;
        }
    }

    writer.m_outputs.push_back({ "SV_Target", 0, GxDxbc::SV_NONE, GxDxbc::COMPONENT_FLOAT, 0, 0xF, 0 });
    code = writer.Finish();
    return true;
}

}
