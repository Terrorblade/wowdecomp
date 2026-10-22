#include "Engine/Source/Gx/CGxDeviceMetal/MetalFixedFunction.h"

#include <cstdarg>
#include <cstdio>

namespace GxMetal {

namespace {

std::string Format(const char* format, ...) __attribute__((format(printf, 1, 2)));

std::string Format(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

const char kComponents[] = "xyzw";

const char* const kFixedVertexStructs = R"MSL(
struct GxFixedLight {
    float4 diffuse;
    float4 specular;
    float4 ambient;
    float4 position;
    float4 direction;
    float range;
    float attenuation0;
    float attenuation1;
    float attenuation2;
};

struct GxFixedVertexState {
    float4x4 world;
    float4x4 worldView;
    float4x4 normal;
    float4x4 projection;
    float4x4 texture[8];
    float4 globalAmbient;
    float4 materialDiffuse;
    float4 materialAmbient;
    float4 materialSpecular;
    float4 materialEmissive;
    float4 materialPower;
    GxFixedLight lights[8];
    float4 fog;
    float4 pointScale;
};

)MSL";

const char* AttributeType(uint8_t attributeClass) {
    switch (attributeClass) {
        case ATTRIBCLASS_UINT: return "uint4";
        case ATTRIBCLASS_SINT: return "int4";
        default: return "float4";
    }
}

const char* MaterialColor(uint8_t source, const char* material) {
    switch (source) {
        case MaterialSource_Color0: return "color0";
        case MaterialSource_Color1: return "color1";
        default: return material;
    }
}

}

bool GenerateFixedVertexShader(const FixedVertexKey& key, std::string& source, std::string& error) {
    source = kShaderPrelude;
    source += kFixedVertexStructs;

    bool hasInputs = false;
    std::string inputs = "struct GxVertexInput {\n";
    for (uint32_t i = 0; i < ATTRIB_COUNT; i++) {
        if (key.m_attributes[i]) {
            inputs += Format("    %s a%u [[attribute(%u)]];\n", AttributeType(key.m_attributes[i]), i, i);
            hasInputs = true;
        }
    }
    inputs += "};\n\n";

    if (hasInputs) {
        source += inputs;
    }

    source += Format("vertex D3DVaryings %s(%sconstant D3DVertexState& state [[buffer(%d)]], constant GxFixedVertexState& ff [[buffer(%d)]]) {\n", kVertexFunctionName, hasInputs ? "GxVertexInput in [[stage_in]], " : "", VS_BUFFER_STATE, VS_BUFFER_FIXED_FUNCTION);

    auto input = [&](uint32_t attribute, const char* fallback) -> std::string {
        if (!key.m_attributes[attribute]) {
            return fallback;
        }
        if (key.m_attributes[attribute] == ATTRIBCLASS_FLOAT) {
            return Format("in.a%u", attribute);
        }
        return Format("float4(in.a%u)", attribute);
    };

    source += "    float4 position = " + input(ATTRIB_POSITION, "float4(0.0f, 0.0f, 0.0f, 1.0f)") + ";\n";
    source += "    float3 normal = (" + input(ATTRIB_NORMAL, "float4(0.0f)") + ").xyz;\n";
    source += "    float4 color0 = " + input(ATTRIB_COLOR0, "float4(1.0f)") + ";\n";
    source += "    float4 color1 = " + input(ATTRIB_COLOR1, "float4(0.0f)") + ";\n";
    source += "    float4 worldPos = ff.world * position;\n";
    source += "    float4 vtx = ff.worldView * position;\n";
    source += "    float3 n = (ff.normal * float4(normal, 0.0f)).xyz;\n";

    if (key.m_normalizeNormals) {
        source += "    n = all(n == 0.0f) ? float3(0.0f) : normalize(n);\n";
    }

    source += "    float4 oPos = ff.projection * vtx;\n";
    source += "    D3DVaryings out;\n";

    // Texture coordinates: generation, then the texture transform
    for (uint32_t i = 0; i < 8; i++) {
        uint32_t inputIndex = key.m_texcoordIndex[i] & 7;
        uint32_t transform = key.m_transformCount[i] & 7;
        uint32_t texcoordCount = key.m_texcoordComponents[inputIndex];
        uint32_t count = 4;
        std::string value;

        switch (key.m_texgen[i]) {
            case TexGen_ViewNormal:
                value = "float4(n, 1.0f)";
                break;

            case TexGen_ViewPosition:
                value = "float4(vtx.xyz, 1.0f)";
                break;

            case TexGen_ViewReflection:
                value = "float4(reflect(normalize(vtx.xyz), n), 1.0f)";
                break;

            default:
                value = input(ATTRIB_TEXCOORD0 + inputIndex, "float4(0.0f, 0.0f, 0.0f, 1.0f)");
                // The transform produces at most the components the input has
                count = transform;
                if (texcoordCount) {
                    if (!count || count > texcoordCount) {
                        count = texcoordCount;
                    }
                } else {
                    transform = 0;
                }
                break;
        }

        source += Format("    float4 tc%u = %s;\n", i, value.c_str());

        if (transform) {
            // Before the transform the first missing component reads as 1 and the rest as 0
            for (uint32_t j = count; j < 4; j++) {
                source += Format("    tc%u.%c = %s;\n", i, kComponents[j], j > texcoordCount ? "0.0f" : "1.0f");
            }

            source += Format("    tc%u = ff.texture[%u] * tc%u;\n", i, i, i);

            // Components past the count repeat the last one, the divisor of projected lookups
            for (uint32_t j = count; j < 4; j++) {
                source += Format("    tc%u.%c = tc%u.%c;\n", i, kComponents[j], i, kComponents[count - 1]);
            }
        }

        source += Format("    out.texcoord%u = tc%u;\n", i, i);
    }

    // Lighting in view space with a local viewer
    if (key.m_lighting) {
        source += "    float4 ambientLight = float4(0.0f);\n";
        source += "    float4 diffuseLight = float4(0.0f);\n";
        source += "    float4 specularLight = float4(0.0f);\n";

        for (uint32_t i = 0; i < kMaxLights; i++) {
            uint32_t type = key.m_lightTypes[i];
            if (type == LightType_None) {
                continue;
            }

            source += "    {\n";
            source += Format("        constant GxFixedLight& light = ff.lights[%u];\n", i);

            if (type == LightType_Directional) {
                source += "        float3 hitDir = normalize(-light.direction.xyz);\n";
                source += "        float atten = 1.0f;\n";
            } else {
                source += "        float3 delta = light.position.xyz - vtx.xyz;\n";
                source += "        float d = length(delta);\n";
                source += "        float3 hitDir = normalize(delta);\n";
                source += "        float atten = d > light.range ? 0.0f : min(1.0f / (light.attenuation0 + d * (light.attenuation1 + d * light.attenuation2)), FLT_MAX);\n";
            }

            source += "        float hitDot = saturate(dot(n, hitDir));\n";
            source += "        float3 mid = normalize(hitDir - normalize(vtx.xyz));\n";
            source += "        float midDot = saturate(dot(n, mid));\n";
            source += "        float specularness = midDot > 0.0f ? d3d_pow(midDot, ff.materialPower.x) * atten : 0.0f;\n";
            source += "        ambientLight += light.ambient * atten;\n";
            source += "        diffuseLight += light.diffuse * (hitDot * atten);\n";
            source += "        specularLight += light.specular * specularness;\n";
            source += "    }\n";
        }

        const char* diffuse = MaterialColor(key.m_diffuseSource, "ff.materialDiffuse");
        const char* ambient = MaterialColor(key.m_ambientSource, "ff.materialAmbient");
        const char* emissive = MaterialColor(key.m_emissiveSource, "ff.materialEmissive");
        const char* specular = MaterialColor(key.m_specularSource, "ff.materialSpecular");

        source += Format("    float4 lit0 = %s * ff.globalAmbient + %s;\n", ambient, emissive);
        source += Format("    lit0 = %s * ambientLight + lit0;\n", ambient);
        source += Format("    lit0 = %s * diffuseLight + lit0;\n", diffuse);
        source += Format("    lit0.w = %s.w;\n", diffuse);
        source += "    out.color0 = saturate(lit0);\n";
        source += Format("    out.color1 = saturate(%s * specularLight);\n", specular);
    } else {
        source += "    out.color0 = saturate(color0);\n";
        source += "    out.color1 = saturate(color1);\n";
    }

    // Linear vertex fog by view depth (the client's only fog mode)
    if (key.m_fog) {
        source += "    float fogDepth = abs(vtx.z);\n";
        source += "    out.fog = saturate((ff.fog.y - fogDepth) * ff.fog.z);\n";
    } else {
        source += "    out.fog = 1.0f;\n";
    }

    // Point size, scaled by distance when point scaling is on
    source += "    float pointSize = state.pointSize.x;\n";
    if (key.m_pointScale) {
        source += "    float eyeDistance = length(vtx.xyz);\n";
        source += "    pointSize *= ff.pointScale.w * sqrt(1.0f / (ff.pointScale.x + eyeDistance * (ff.pointScale.y + eyeDistance * ff.pointScale.z)));\n";
    }
    source += "    out.pointSize = clamp(pointSize, state.pointSize.y, state.pointSize.z);\n";

    // User clip planes are in world space for fixed-function vertex processing
    for (uint32_t i = 0; i < 6; i++) {
        source += Format("    out.clipDistance[%u] = (state.flags.x & %uu) ? dot(worldPos, state.clipPlanes[%u]) : 1.0f;\n", i, 1u << i, i);
    }

    source += "    out.position = oPos;\n";
    source += "    out.position.xy += state.posFixup.xy * oPos.w;\n";
    source += "    out.position.z += state.posFixup.z * oPos.w;\n";
    source += "    return out;\n}\n";

    (void)error;
    return true;
}

bool GenerateFixedPixelShader(const FixedPixelKey& key, std::string& source, std::string& error) {
    // Stages in use: up to the first one whose colour operation is disabled
    uint32_t stageCount = 0;
    while (stageCount < 8 && key.m_stages[stageCount].m_colorOp != StageOp_Disable) {
        stageCount++;
    }

    std::string body;
    std::string params;
    bool usesTexcoord[8] = {};

    body += "    float4 diffuse = in.color0;\n";
    body += "    float4 current = diffuse;\n";

    for (uint32_t i = 0; i < stageCount; i++) {
        const FixedPixelStage& stage = key.m_stages[i];
        // Every operation names the stage's texture as an argument
        bool sampled = stage.m_textureBound != 0;

        body += Format("    // Stage %u\n", i);

        if (sampled) {
            SamplerKind kind = static_cast<SamplerKind>(stage.m_samplerKind);
            const char* type;
            switch (kind) {
                case SAMPLERKIND_CUBE: type = "texturecube<float>"; break;
                case SAMPLERKIND_VOLUME: type = "texture3d<float>"; break;
                case SAMPLERKIND_DEPTH: type = "depth2d<float>"; break;
                default: type = "texture2d<float>"; break;
            }
            params += Format(", %s tex%u [[texture(%u)]], sampler smp%u [[sampler(%u)]]", type, i, i, i, i);

            if (key.m_pointSprite) {
                body += Format("    float4 coord%u = float4(pointCoord, 0.0f, 1.0f);\n", i);
            } else {
                usesTexcoord[i] = true;
                body += Format("    float4 coord%u = in.texcoord%u;\n", i, i);
            }

            if (stage.m_projected) {
                // The divisor is the last counted component, or w
                uint32_t divisor = stage.m_projected >= 2 && stage.m_projected <= 4 ? stage.m_projected - 1 : 3;
                body += Format("    coord%u /= coord%u.%c;\n", i, i, kComponents[divisor]);
            }

            switch (kind) {
                case SAMPLERKIND_DEPTH:
                    body += Format("    float4 texture%u = float4(tex%u.sample_compare(smp%u, coord%u.xy, coord%u.z, level(0)));\n", i, i, i, i, i);
                    break;
                default:
                    body += Format("    float4 texture%u = tex%u.sample(smp%u, coord%u.%s);\n", i, i, i, i, kind == SAMPLERKIND_2D ? "xy" : "xyz");
                    break;
            }
        }

        std::string texture = sampled ? Format("texture%u", i) : "float4(0.0f, 0.0f, 0.0f, 1.0f)";

        auto op = [&](uint8_t operation, std::string& out) -> bool {
            switch (operation) {
                case StageOp_Modulate: out = "(" + texture + " * current)"; return true;
                case StageOp_Modulate2x: out = "saturate(" + texture + " * current * 2.0f)"; return true;
                case StageOp_Add: out = "saturate(" + texture + " + current)"; return true;
                case StageOp_PassThrough: out = "current"; return true;
                case StageOp_Decal: out = "mix(" + texture + ", current, current.wwww)"; return true;
                case StageOp_Fade: out = "mix(current, " + texture + ", diffuse.wwww)"; return true;
                default:
                    error = Format("unsupported texture operation %u", operation);
                    return false;
            }
        };

        std::string color;
        if (!op(stage.m_colorOp, color)) {
            return false;
        }

        if (stage.m_colorOp == stage.m_alphaOp) {
            body += Format("    current = %s;\n", color.c_str());
        } else {
            body += Format("    float4 color%u = %s;\n", i, color.c_str());

            if (stage.m_alphaOp != StageOp_Disable) {
                std::string alpha;
                if (!op(stage.m_alphaOp, alpha)) {
                    return false;
                }
                body += Format("    float alpha%u = (%s).w;\n", i, alpha.c_str());
                body += Format("    current = float4(color%u.xyz, alpha%u);\n", i, i);
            } else {
                body += Format("    current.xyz = color%u.xyz;\n", i);
            }
        }
    }

    if (key.m_specularEnable) {
        body += "    current.xyz += in.color1.xyz;\n";
    }

    source = kShaderPrelude;
    source += "\nstruct GxFragmentInput {\n";
    source += "    float4 color0 [[user(d3d_color0)]];\n";
    if (key.m_specularEnable) {
        source += "    float4 color1 [[user(d3d_color1)]];\n";
    }
    for (uint32_t i = 0; i < 8; i++) {
        if (usesTexcoord[i]) {
            source += Format("    float4 texcoord%u [[user(d3d_texcoord%u)]];\n", i, i);
        }
    }
    source += "    float fog [[user(d3d_fog)]];\n};\n\n";
    source += "struct GxFragmentOutput {\n    float4 color [[color(0)]];\n};\n\n";

    source += Format("fragment GxFragmentOutput %s(GxFragmentInput in [[stage_in]], constant D3DPixelState& state [[buffer(%d)]]%s%s) {\n", kFragmentFunctionName, PS_BUFFER_STATE, params.c_str(), key.m_pointSprite ? ", float2 pointCoord [[point_coord]]" : "");
    source += body;
    source += "    GxFragmentOutput out;\n";
    source += "    out.color = d3d_fog(state, current, in.fog);\n";
    source += "    if (!d3d_alpha_test(state, out.color.a)) discard_fragment();\n";
    source += "    return out;\n}\n";

    return true;
}

}
