#include "ShaderTranslator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace GxMetal {

const char* const kVertexFunctionName = "d3d_vertex_main";
const char* const kFragmentFunctionName = "d3d_fragment_main";

const char* const kShaderPrelude = R"MSL(#include <metal_stdlib>
using namespace metal;

struct D3DVertexState {
    float4 posFixup;
    float4 clipPlanes[6];
    uint4 flags;
    float4 pointSize;
};

struct D3DPixelState {
    float4 fogColor;
    uint4 flags;
    float4 bumpEnvMat[8];
    float4 bumpEnvLum[8];
    float4 textureFactor;
};

struct D3DVaryings {
    float4 position [[position, invariant]];
    float4 color0 [[user(d3d_color0)]];
    float4 color1 [[user(d3d_color1)]];
    float4 texcoord0 [[user(d3d_texcoord0)]];
    float4 texcoord1 [[user(d3d_texcoord1)]];
    float4 texcoord2 [[user(d3d_texcoord2)]];
    float4 texcoord3 [[user(d3d_texcoord3)]];
    float4 texcoord4 [[user(d3d_texcoord4)]];
    float4 texcoord5 [[user(d3d_texcoord5)]];
    float4 texcoord6 [[user(d3d_texcoord6)]];
    float4 texcoord7 [[user(d3d_texcoord7)]];
    float fog [[user(d3d_fog)]];
    float pointSize [[point_size]];
    float clipDistance [[clip_distance]] [6];
};

// Direct3D 9 defines results for inputs where IEEE math produces infinities or NaNs
static inline float d3d_rcp(float x) {
    return x == 0.0f ? FLT_MAX : 1.0f / x;
}

static inline float d3d_rsq(float x) {
    float a = abs(x);
    return a == 0.0f ? FLT_MAX : rsqrt(a);
}

static inline float d3d_log(float x) {
    float a = abs(x);
    return a == 0.0f ? -FLT_MAX : log2(a);
}

static inline float d3d_pow(float base, float exponent) {
    float a = abs(base);

    if (exponent == 0.0f) {
        return 1.0f;
    }

    if (a == 0.0f) {
        return exponent > 0.0f ? 0.0f : FLT_MAX;
    }

    return exp2(exponent * log2(a));
}

static inline float4 d3d_nrm(float4 v) {
    float d = dot(v.xyz, v.xyz);
    return d == 0.0f ? float4(0.0f) : v * rsqrt(d);
}

static inline float4 d3d_lit(float4 s) {
    float4 d = float4(1.0f, 0.0f, 0.0f, 1.0f);

    if (s.x > 0.0f) {
        d.y = s.x;

        if (s.y > 0.0f) {
            d.z = d3d_pow(s.y, clamp(s.w, -127.9961f, 127.9961f));
        }
    }

    return d;
}

static inline bool d3d_compare(uint func, float a, float b) {
    switch (func) {
        case 1: return false;
        case 2: return a < b;
        case 3: return a == b;
        case 4: return a <= b;
        case 5: return a > b;
        case 6: return a != b;
        case 7: return a >= b;
        default: return true;
    }
}

// Alpha test in the 8-bit precision of the render target, as Direct3D 9 hardware does it
static inline bool d3d_alpha_test(constant D3DPixelState& state, float alpha) {
    uint func = state.flags.y;

    if (func == 8u) {
        return true;
    }

    return d3d_compare(func, rint(alpha * 255.0f), float(state.flags.z));
}

static inline float4 d3d_fog(constant D3DPixelState& state, float4 color, float fog) {
    if (state.flags.x != 0u) {
        color.rgb = mix(state.fogColor.rgb, color.rgb, saturate(fog));
    }

    return color;
}
)MSL";

int32_t AttributeIndex(uint32_t usage, uint32_t usageIndex) {
    switch (usage) {
        case USAGE_POSITION:
            return usageIndex == 0 ? ATTRIB_POSITION : -1;
        case USAGE_BLENDWEIGHT:
            return usageIndex == 0 ? ATTRIB_BLENDWEIGHT : -1;
        case USAGE_BLENDINDICES:
            return usageIndex == 0 ? ATTRIB_BLENDINDICES : -1;
        case USAGE_NORMAL:
            return usageIndex == 0 ? ATTRIB_NORMAL : -1;
        case USAGE_COLOR:
            return usageIndex < 2 ? ATTRIB_COLOR0 + static_cast<int32_t>(usageIndex) : -1;
        case USAGE_TEXCOORD:
            return usageIndex < 8 ? ATTRIB_TEXCOORD0 + static_cast<int32_t>(usageIndex) : -1;
        default:
            return -1;
    }
}

bool VertexInputSignature::operator==(const VertexInputSignature& other) const {
    return memcmp(this->m_class, other.m_class, sizeof(this->m_class)) == 0;
}

bool PixelShaderVariant::operator==(const PixelShaderVariant& other) const {
    return this->m_samplerKinds == other.m_samplerKinds && this->m_projected == other.m_projected && this->m_pointSprite == other.m_pointSprite;
}

namespace {

std::string Format(const char* format, ...) __attribute__((format(printf, 1, 2)));

std::string Format(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < static_cast<int>(sizeof(buffer))) {
        return std::string(buffer, length);
    }

    std::string result(length, '\0');
    va_start(args, format);
    vsnprintf(&result[0], length + 1, format, args);
    va_end(args);
    return result;
}

// Exact float literal: the bit pattern, so values like NaN or denormals survive
std::string FloatLiteral(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);

    if (std::isfinite(value) && value == static_cast<float>(static_cast<int32_t>(value)) && value > -16777216.0f && value < 16777216.0f && !(value == 0.0f && (bits >> 31))) {
        return Format("%d.0f", static_cast<int32_t>(value));
    }

    return Format("as_type<float>(0x%08Xu)", bits);
}

const char kComponents[] = "xyzw";

std::string MaskString(uint32_t mask) {
    std::string result;
    for (uint32_t i = 0; i < 4; i++) {
        if (mask & (1u << i)) {
            result += kComponents[i];
        }
    }
    return result;
}

uint32_t FirstComponent(uint32_t mask) {
    for (uint32_t i = 0; i < 4; i++) {
        if (mask & (1u << i)) {
            return i;
        }
    }
    return 0;
}

const char* ComparisonOperator(uint32_t comparison) {
    switch (comparison) {
        case CMP_GT: return ">";
        case CMP_EQ: return "==";
        case CMP_GE: return ">=";
        case CMP_LT: return "<";
        case CMP_NE: return "!=";
        case CMP_LE: return "<=";
        default: return nullptr;
    }
}

const char* SemanticField(uint32_t usage, uint32_t usageIndex) {
    static const char* const s_texcoords[8] = { "texcoord0", "texcoord1", "texcoord2", "texcoord3", "texcoord4", "texcoord5", "texcoord6", "texcoord7" };

    switch (usage) {
        case USAGE_COLOR:
            return usageIndex == 0 ? "color0" : usageIndex == 1 ? "color1" : nullptr;
        case USAGE_TEXCOORD:
            return usageIndex < 8 ? s_texcoords[usageIndex] : nullptr;
        case USAGE_FOG:
            return usageIndex == 0 ? "fog" : nullptr;
        default:
            return nullptr;
    }
}

// Input or output register of a shader model 3 shader, declared with a semantic
struct Declaration {
    uint32_t m_usage = 0;
    uint32_t m_usageIndex = 0;
    uint32_t m_mask = 0;
};

class Translator {
public:
    Translator(const ShaderProgram& program, std::string& error) : m_program(program), m_error(error) {}

    bool Analyze();
    void FillInfo(ShaderInfo& info) const;
    bool EmitVertex(const VertexInputSignature& inputs, std::string& source);
    bool EmitPixel(const PixelShaderVariant& variant, std::string& source);

private:
    bool Fail(const char* format, ...) __attribute__((format(printf, 2, 3)));

    bool IsVertex() const { return this->m_program.m_stage == STAGE_VERTEX; }
    bool IsPixel() const { return this->m_program.m_stage == STAGE_PIXEL; }
    uint32_t Major() const { return this->m_program.m_major; }
    uint32_t Minor() const { return this->m_program.m_minor; }
    bool IsPs1() const { return this->IsPixel() && this->Major() == 1; }
    // ps_1_1 to ps_1_3, the texture-register model
    bool IsPs11() const { return this->IsPs1() && this->Minor() < 4; }

    uint32_t FloatConstantLimit() const;

    void Line(const std::string& text);

    // Operands
    bool AddressIndex(const RelativeAddress& address, uint32_t base, std::string& out);
    bool RegisterValue(const SourceOperand& operand, std::string& out);
    bool ConstantRegister(uint32_t index, bool relative, const RelativeAddress& address, std::string& out);
    bool Source(const SourceOperand& operand, std::string& out);
    bool IntConstant(const SourceOperand& operand, std::string& out);
    bool BoolConstant(const SourceOperand& operand, std::string& out);
    bool DestRegister(const DestOperand& operand, std::string& out, bool& scalar);
    bool Write(const DestOperand& operand, const std::string& value, std::vector<std::string>& lines);
    bool Sample(uint32_t sampler, const std::string& coord, uint32_t control, const std::string& extra, std::string& out);

    bool EmitInstruction(size_t& index);
    bool EmitArithmetic(const Instruction& instruction, std::vector<std::string>& writes);
    bool EmitBody();

    const ShaderProgram& m_program;
    std::string& m_error;

    std::string m_body;
    uint32_t m_indent = 1;
    uint32_t m_temporaryCounter = 0;
    uint32_t m_loopDepth = 0;

    // Analysis
    std::map<uint32_t, std::array<float, 4>> m_floatDefs;
    std::map<uint32_t, std::array<int32_t, 4>> m_intDefs;
    std::map<uint32_t, bool> m_boolDefs;
    uint32_t m_tempMask = 0;
    uint32_t m_maxTemp = 0;
    bool m_constRelative = false;
    uint32_t m_maxConst = 0;
    bool m_usesConst = false;
    bool m_usesIntConst = false;
    bool m_usesBoolConst = false;
    bool m_usesAddress = false;
    uint32_t m_samplerMask = 0;
    uint32_t m_declaredKinds = 0;
    std::map<uint32_t, Declaration> m_inputs;     // v# (vertex, ps_3_0), by register
    std::map<uint32_t, Declaration> m_outputs;    // o# (vs_3_0), by register
    uint32_t m_inputMask = 0;                     // registers v# read
    uint32_t m_textureMask = 0;                   // registers t# read or written
    uint32_t m_writtenColorMask = 0;              // oD# written (vs_1_x, vs_2_x)
    uint32_t m_writtenTexcoordMask = 0;           // oT# written
    bool m_writesFog = false;
    bool m_writesPointSize = false;
    bool m_writesDepth = false;
    bool m_usesPosition = false;                  // vPos
    bool m_usesFace = false;                      // vFace

    // Pixel shader variant being emitted
    PixelShaderVariant m_variant;
};

bool Translator::Fail(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    m_error = Format("%cs_%u_%u: %s", this->IsVertex() ? 'v' : 'p', this->Major(), this->Minor(), buffer);
    return false;
}

uint32_t Translator::FloatConstantLimit() const {
    if (this->IsVertex()) {
        return 256;
    }

    switch (this->Major()) {
        case 1: return 8;
        case 2: return 32;
        default: return 224;
    }
}

void Translator::Line(const std::string& text) {
    this->m_body.append(this->m_indent * 4, ' ');
    this->m_body += text;
    this->m_body += '\n';
}

bool Translator::Analyze() {
    for (const Instruction& instruction : this->m_program.m_instructions) {
        if (instruction.m_predicated) {
            return this->Fail("predicated instructions are not supported");
        }

        switch (instruction.m_opcode) {
            case OP_DEF:
                this->m_floatDefs[instruction.m_dest.m_index] = { instruction.m_immediate.m_float[0], instruction.m_immediate.m_float[1], instruction.m_immediate.m_float[2], instruction.m_immediate.m_float[3] };
                continue;

            case OP_DEFI:
                this->m_intDefs[instruction.m_dest.m_index] = { instruction.m_immediate.m_int[0], instruction.m_immediate.m_int[1], instruction.m_immediate.m_int[2], instruction.m_immediate.m_int[3] };
                continue;

            case OP_DEFB:
                this->m_boolDefs[instruction.m_dest.m_index] = instruction.m_immediate.m_bool != 0;
                continue;

            case OP_DCL: {
                const DestOperand& dest = instruction.m_dest;
                Declaration declaration;
                declaration.m_usage = instruction.m_usage;
                declaration.m_usageIndex = instruction.m_usageIndex;
                declaration.m_mask = dest.m_writeMask;

                if (dest.m_type == REG_SAMPLER) {
                    uint32_t kind;
                    switch (instruction.m_samplerType) {
                        case SAMPLER_2D: kind = SAMPLERKIND_2D; break;
                        case SAMPLER_CUBE: kind = SAMPLERKIND_CUBE; break;
                        case SAMPLER_VOLUME: kind = SAMPLERKIND_VOLUME; break;
                        default: return this->Fail("unknown sampler type %u", instruction.m_samplerType);
                    }
                    this->m_declaredKinds |= kind << (dest.m_index * 2);
                } else if (dest.m_type == REG_INPUT) {
                    this->m_inputs[dest.m_index] = declaration;
                } else if (dest.m_type == REG_OUTPUT && this->IsVertex() && this->Major() >= 3) {
                    this->m_outputs[dest.m_index] = declaration;
                } else if (dest.m_type == REG_TEXTURE && this->IsPixel()) {
                    this->m_textureMask |= 1u << dest.m_index;
                } else if (dest.m_type == REG_MISCTYPE) {
                    // vPos and vFace are recognized where they are read
                } else {
                    return this->Fail("unsupported declaration of register type %u", dest.m_type);
                }
                continue;
            }

            default:
                break;
        }

        auto noteSource = [&](const SourceOperand& source) {
            switch (source.m_type) {
                case REG_TEMP:
                    this->m_tempMask |= 1u << source.m_index;
                    break;
                case REG_INPUT:
                    this->m_inputMask |= 1u << source.m_index;
                    break;
                case REG_CONST:
                    this->m_usesConst = true;
                    if (source.m_relative) {
                        this->m_constRelative = true;
                    } else if (source.m_index + 1 > this->m_maxConst) {
                        this->m_maxConst = source.m_index + 1;
                    }
                    break;
                case REG_TEXTURE:
                    if (this->IsPixel()) {
                        this->m_textureMask |= 1u << source.m_index;
                    }
                    break;
                case REG_CONSTINT:
                    this->m_usesIntConst = true;
                    break;
                case REG_CONSTBOOL:
                    this->m_usesBoolConst = true;
                    break;
                case REG_SAMPLER:
                    this->m_samplerMask |= 1u << source.m_index;
                    break;
                case REG_MISCTYPE:
                    if (source.m_index == 0) {
                        this->m_usesPosition = true;
                    } else {
                        this->m_usesFace = true;
                    }
                    break;
                default:
                    break;
            }

            if (source.m_relative && source.m_address.m_type == REG_ADDR) {
                this->m_usesAddress = true;
            }
        };

        for (const SourceOperand& source : instruction.m_sources) {
            noteSource(source);
        }

        // Matrix instructions read consecutive constant registers
        if (instruction.m_opcode >= OP_M4x4 && instruction.m_opcode <= OP_M3x2 && instruction.m_sources.size() == 2) {
            const SourceOperand& matrix = instruction.m_sources[1];
            uint32_t rows = instruction.m_opcode == OP_M4x4 || instruction.m_opcode == OP_M3x4 ? 4 : instruction.m_opcode == OP_M3x2 ? 2 : 3;
            if (matrix.m_type == REG_CONST && !matrix.m_relative && matrix.m_index + rows > this->m_maxConst) {
                this->m_maxConst = matrix.m_index + rows;
            }
            if (matrix.m_type == REG_TEMP) {
                for (uint32_t i = 0; i < rows; i++) {
                    this->m_tempMask |= 1u << (matrix.m_index + i);
                }
            }
        }

        // ps_1_1 to ps_1_3 texture instructions sample the stage of their destination
        if (this->IsPs1() && instruction.m_hasDest) {
            switch (instruction.m_opcode) {
                case OP_TEX:
                case OP_TEXBEM:
                case OP_TEXBEML:
                    this->m_samplerMask |= 1u << instruction.m_dest.m_index;
                    break;
                default:
                    break;
            }
        }

        if (instruction.m_hasDest) {
            const DestOperand& dest = instruction.m_dest;

            switch (dest.m_type) {
                case REG_TEMP:
                    this->m_tempMask |= 1u << dest.m_index;
                    break;
                case REG_ADDR:
                    if (this->IsVertex()) {
                        this->m_usesAddress = true;
                    } else {
                        this->m_textureMask |= 1u << dest.m_index;
                    }
                    break;
                case REG_RASTOUT:
                    if (dest.m_index == 1) {
                        this->m_writesFog = true;
                    } else if (dest.m_index == 2) {
                        this->m_writesPointSize = true;
                    }
                    break;
                case REG_ATTROUT:
                    this->m_writtenColorMask |= 1u << dest.m_index;
                    break;
                case REG_OUTPUT:
                    if (this->Major() < 3) {
                        this->m_writtenTexcoordMask |= 1u << dest.m_index;
                    }
                    break;
                case REG_DEPTHOUT:
                    this->m_writesDepth = true;
                    break;
                default:
                    break;
            }
        }
    }

    for (uint32_t i = 0; i < 32; i++) {
        if (this->m_tempMask & (1u << i)) {
            this->m_maxTemp = i + 1;
        }
    }

    if (this->IsPs1()) {
        // ps_1_x outputs r0
        this->m_tempMask |= 1;
    }

    return true;
}

void Translator::FillInfo(ShaderInfo& info) const {
    info.m_stage = this->m_program.m_stage;
    info.m_major = this->Major();
    info.m_minor = this->Minor();

    if (this->m_usesConst) {
        info.m_constantCount = this->m_constRelative ? this->FloatConstantLimit() : std::min(this->m_maxConst, this->FloatConstantLimit());
    }

    info.m_usesIntConstants = this->m_usesIntConst;
    info.m_usesBoolConstants = this->m_usesBoolConst;
    info.m_samplerMask = this->m_samplerMask;
    info.m_declaredKinds = this->m_declaredKinds;

    if (this->IsVertex()) {
        for (const auto& [reg, declaration] : this->m_inputs) {
            int32_t attribute = AttributeIndex(declaration.m_usage, declaration.m_usageIndex);
            if (attribute >= 0 && (this->m_inputMask & (1u << reg))) {
                info.m_attributeMask |= 1u << attribute;
            }
        }
    }
}

bool Translator::AddressIndex(const RelativeAddress& address, uint32_t base, std::string& out) {
    if (address.m_type == REG_ADDR) {
        out = Format("a0.%c + %u", kComponents[address.m_component], base);
        return true;
    }

    if (address.m_type == REG_LOOP) {
        if (this->m_loopDepth == 0) {
            return this->Fail("aL used outside of a loop");
        }
        out = Format("aL + %u", base);
        return true;
    }

    return this->Fail("unsupported relative address register type %u", address.m_type);
}

bool Translator::ConstantRegister(uint32_t index, bool relative, const RelativeAddress& address, std::string& out) {
    bool clamp = this->IsPs1();

    if (relative) {
        std::string addressIndex;
        if (!this->AddressIndex(address, index, addressIndex)) {
            return false;
        }
        out = Format("load_c(c, %s)", addressIndex.c_str());
    } else {
        auto def = this->m_floatDefs.find(index);

        if (def != this->m_floatDefs.end()) {
            std::array<float, 4> value = def->second;

            if (clamp) {
                for (float& component : value) {
                    component = component < -1.0f ? -1.0f : component > 1.0f ? 1.0f : component;
                }
            }

            out = "float4(" + FloatLiteral(value[0]) + ", " + FloatLiteral(value[1]) + ", " + FloatLiteral(value[2]) + ", " + FloatLiteral(value[3]) + ")";
            return true;
        }

        if (index >= this->FloatConstantLimit()) {
            return this->Fail("constant register c%u out of range", index);
        }

        out = Format("c[%u]", index);
    }

    if (clamp) {
        out = "clamp(" + out + ", -1.0f, 1.0f)";
    }

    return true;
}

bool Translator::RegisterValue(const SourceOperand& operand, std::string& out) {
    switch (operand.m_type) {
        case REG_TEMP:
            out = Format("r%u", operand.m_index);
            return true;

        case REG_INPUT:
            out = Format("v%u", operand.m_index);
            return true;

        case REG_CONST:
            return this->ConstantRegister(operand.m_index, operand.m_relative, operand.m_address, out);

        case REG_ADDR:
            if (this->IsVertex()) {
                out = "float4(a0)";
            } else {
                out = Format("t%u", operand.m_index);
            }
            return true;

        case REG_LOOP:
            out = "float4(float(aL))";
            return true;

        case REG_MISCTYPE:
            out = operand.m_index == 0 ? "vPos" : "vFace";
            return true;

        default:
            return this->Fail("unsupported source register type %u", operand.m_type);
    }
}

bool Translator::Source(const SourceOperand& operand, std::string& out) {
    std::string value;
    if (!this->RegisterValue(operand, value)) {
        return false;
    }

    // _dz and _dw divide by a component of the register, before the swizzle
    if (operand.m_modifier == SRCMOD_DZ) {
        value = "(" + value + " / " + value + ".z)";
    } else if (operand.m_modifier == SRCMOD_DW) {
        value = "(" + value + " / " + value + ".w)";
    }

    bool identity = operand.m_swizzle[0] == 0 && operand.m_swizzle[1] == 1 && operand.m_swizzle[2] == 2 && operand.m_swizzle[3] == 3;
    if (!identity) {
        value += '.';
        for (uint32_t i = 0; i < 4; i++) {
            value += kComponents[operand.m_swizzle[i]];
        }
    }

    switch (operand.m_modifier) {
        case SRCMOD_NONE:
        case SRCMOD_DZ:
        case SRCMOD_DW:
            out = value;
            break;
        case SRCMOD_NEG:
            out = "(-" + value + ")";
            break;
        case SRCMOD_BIAS:
            out = "(" + value + " - 0.5f)";
            break;
        case SRCMOD_BIASNEG:
            out = "(0.5f - " + value + ")";
            break;
        case SRCMOD_SIGN:
            out = "(" + value + " * 2.0f - 1.0f)";
            break;
        case SRCMOD_SIGNNEG:
            out = "(1.0f - " + value + " * 2.0f)";
            break;
        case SRCMOD_COMP:
            out = "(1.0f - " + value + ")";
            break;
        case SRCMOD_X2:
            out = "(" + value + " * 2.0f)";
            break;
        case SRCMOD_X2NEG:
            out = "(" + value + " * -2.0f)";
            break;
        case SRCMOD_ABS:
            out = "abs(" + value + ")";
            break;
        case SRCMOD_ABSNEG:
            out = "(-abs(" + value + "))";
            break;
        default:
            return this->Fail("unsupported source modifier %u", operand.m_modifier);
    }

    return true;
}

bool Translator::IntConstant(const SourceOperand& operand, std::string& out) {
    if (operand.m_type != REG_CONSTINT) {
        return this->Fail("loop counter is not an integer constant");
    }

    auto def = this->m_intDefs.find(operand.m_index);
    if (def != this->m_intDefs.end()) {
        out = Format("int4(%d, %d, %d, %d)", def->second[0], def->second[1], def->second[2], def->second[3]);
    } else {
        out = Format("ci[%u]", operand.m_index);
    }

    return true;
}

bool Translator::BoolConstant(const SourceOperand& operand, std::string& out) {
    if (operand.m_type != REG_CONSTBOOL) {
        return this->Fail("condition is not a boolean constant");
    }

    auto def = this->m_boolDefs.find(operand.m_index);
    if (def != this->m_boolDefs.end()) {
        out = def->second ? "true" : "false";
    } else {
        out = Format("(cb[%u] != 0u)", operand.m_index);
    }

    if (operand.m_modifier == SRCMOD_NOT) {
        out = "!" + out;
    }

    return true;
}

bool Translator::DestRegister(const DestOperand& operand, std::string& out, bool& scalar) {
    scalar = false;

    if (operand.m_relative) {
        return this->Fail("relative addressing of destination registers is not supported");
    }

    switch (operand.m_type) {
        case REG_TEMP:
            out = Format("r%u", operand.m_index);
            return true;

        case REG_ADDR:
            out = this->IsVertex() ? "a0" : Format("t%u", operand.m_index);
            return true;

        case REG_RASTOUT:
            switch (operand.m_index) {
                case 0:
                    out = "oPos";
                    return true;
                case 1:
                    out = "oFog";
                    scalar = true;
                    return true;
                case 2:
                    out = "oPts";
                    scalar = true;
                    return true;
                default:
                    return this->Fail("unknown rasterizer output %u", operand.m_index);
            }

        case REG_ATTROUT:
            if (operand.m_index >= 2) {
                return this->Fail("unknown color output %u", operand.m_index);
            }
            out = Format("oD%u", operand.m_index);
            return true;

        case REG_OUTPUT:
            if (this->Major() < 3 && operand.m_index >= 8) {
                return this->Fail("unknown texture coordinate output %u", operand.m_index);
            }
            out = Format(this->Major() < 3 ? "oT%u" : "o%u", operand.m_index);
            return true;

        case REG_COLOROUT:
            if (operand.m_index != 0) {
                return this->Fail("multiple render targets are not supported");
            }
            out = "oC0";
            return true;

        case REG_DEPTHOUT:
            out = "oDepth";
            scalar = true;
            return true;

        default:
            return this->Fail("unsupported destination register type %u", operand.m_type);
    }
}

bool Translator::Write(const DestOperand& operand, const std::string& value, std::vector<std::string>& lines) {
    std::string target;
    bool scalar;
    if (!this->DestRegister(operand, target, scalar)) {
        return false;
    }

    std::string result = value;

    if (operand.m_shift) {
        float scale = operand.m_shift > 0 ? static_cast<float>(1 << operand.m_shift) : 1.0f / static_cast<float>(1 << -operand.m_shift);
        result = "(" + result + ") * " + FloatLiteral(scale);
    }

    if (operand.m_saturate) {
        result = "saturate(" + result + ")";
    }

    uint32_t mask = operand.m_writeMask ? operand.m_writeMask : 0xF;

    if (scalar) {
        lines.push_back(Format("%s = (%s).%c;", target.c_str(), result.c_str(), kComponents[FirstComponent(mask)]));
    } else if (mask == 0xF) {
        lines.push_back(Format("%s = %s;", target.c_str(), result.c_str()));
    } else {
        std::string components = MaskString(mask);
        lines.push_back(Format("%s.%s = (%s).%s;", target.c_str(), components.c_str(), result.c_str(), components.c_str()));
    }

    return true;
}

bool Translator::Sample(uint32_t sampler, const std::string& coord, uint32_t control, const std::string& extra, std::string& out) {
    if (sampler >= 16) {
        return this->Fail("sampler %u out of range", sampler);
    }

    SamplerKind kind = this->m_variant.Kind(sampler);
    std::string c = coord;

    if (control == TEXLD_PROJECT) {
        c = Format("(%s / (%s).w)", coord.c_str(), coord.c_str());
    }

    std::string options = extra;
    if (control == TEXLD_BIAS) {
        options = Format(", bias((%s).w)", coord.c_str());
    }

    switch (kind) {
        case SAMPLERKIND_2D:
            out = Format("tex%u.sample(smp%u, (%s).xy%s)", sampler, sampler, c.c_str(), options.c_str());
            return true;

        case SAMPLERKIND_CUBE:
            out = Format("tex%u.sample(smp%u, (%s).xyz%s)", sampler, sampler, c.c_str(), options.c_str());
            return true;

        case SAMPLERKIND_VOLUME:
            out = Format("tex%u.sample(smp%u, (%s).xyz%s)", sampler, sampler, c.c_str(), options.c_str());
            return true;

        case SAMPLERKIND_DEPTH:
            // A depth texture samples as a shadow comparison of the coordinate's z with the
            // stored depth, filtered by the sampler (hardware PCF). Depth textures have a
            // single level, so explicit levels and biases select level 0.
            out = Format("float4(tex%u.sample_compare(smp%u, (%s).xy, (%s).z, level(0)))", sampler, sampler, c.c_str(), c.c_str());
            return true;
    }

    return false;
}

bool Translator::EmitArithmetic(const Instruction& instruction, std::vector<std::string>& writes) {
    const std::vector<SourceOperand>& sources = instruction.m_sources;
    std::string s[4];

    auto need = [&](size_t count) -> bool {
        if (sources.size() < count) {
            return this->Fail("%s: expected %zu sources, found %zu", OpcodeName(instruction.m_opcode), count, sources.size());
        }
        for (size_t i = 0; i < count && i < 4; i++) {
            if (!this->Source(sources[i], s[i])) {
                return false;
            }
        }
        return true;
    };

    std::string value;

    switch (instruction.m_opcode) {
        case OP_MOV:
            if (!need(1)) return false;
            if (this->IsVertex() && instruction.m_dest.m_type == REG_ADDR) {
                // vs_1_1 loads the address register with mov, which truncates toward negative infinity
                uint32_t mask = instruction.m_dest.m_writeMask ? instruction.m_dest.m_writeMask : 0xF;
                std::string components = MaskString(mask);
                writes.push_back(Format("a0.%s = int4(floor(%s)).%s;", components.c_str(), s[0].c_str(), components.c_str()));
                return true;
            }
            value = s[0];
            break;

        case OP_MOVA: {
            if (!need(1)) return false;
            uint32_t mask = instruction.m_dest.m_writeMask ? instruction.m_dest.m_writeMask : 0xF;
            std::string components = MaskString(mask);
            writes.push_back(Format("a0.%s = int4(round(%s)).%s;", components.c_str(), s[0].c_str(), components.c_str()));
            return true;
        }

        case OP_ADD: if (!need(2)) return false; value = "(" + s[0] + " + " + s[1] + ")"; break;
        case OP_SUB: if (!need(2)) return false; value = "(" + s[0] + " - " + s[1] + ")"; break;
        case OP_MUL: if (!need(2)) return false; value = "(" + s[0] + " * " + s[1] + ")"; break;
        case OP_MAD: if (!need(3)) return false; value = "(" + s[0] + " * " + s[1] + " + " + s[2] + ")"; break;
        case OP_RCP: if (!need(1)) return false; value = "float4(d3d_rcp((" + s[0] + ").w))"; break;
        case OP_RSQ: if (!need(1)) return false; value = "float4(d3d_rsq((" + s[0] + ").w))"; break;
        case OP_EXP:
        case OP_EXPP: if (!need(1)) return false; value = "float4(exp2((" + s[0] + ").w))"; break;
        case OP_LOG:
        case OP_LOGP: if (!need(1)) return false; value = "float4(d3d_log((" + s[0] + ").w))"; break;
        case OP_DP3: if (!need(2)) return false; value = "float4(dot((" + s[0] + ").xyz, (" + s[1] + ").xyz))"; break;
        case OP_DP4: if (!need(2)) return false; value = "float4(dot(" + s[0] + ", " + s[1] + "))"; break;
        case OP_MIN: if (!need(2)) return false; value = "min(" + s[0] + ", " + s[1] + ")"; break;
        case OP_MAX: if (!need(2)) return false; value = "max(" + s[0] + ", " + s[1] + ")"; break;
        case OP_SLT: if (!need(2)) return false; value = "float4(" + s[0] + " < " + s[1] + ")"; break;
        case OP_SGE: if (!need(2)) return false; value = "float4(" + s[0] + " >= " + s[1] + ")"; break;
        case OP_LIT: if (!need(1)) return false; value = "d3d_lit(" + s[0] + ")"; break;
        case OP_DST: if (!need(2)) return false; value = "float4(1.0f, (" + s[0] + ").y * (" + s[1] + ").y, (" + s[0] + ").z, (" + s[1] + ").w)"; break;
        case OP_LRP: if (!need(3)) return false; value = "mix(" + s[2] + ", " + s[1] + ", " + s[0] + ")"; break;
        case OP_FRC: if (!need(1)) return false; value = "fract(" + s[0] + ")"; break;
        case OP_POW: if (!need(2)) return false; value = "float4(d3d_pow((" + s[0] + ").x, (" + s[1] + ").x))"; break;
        case OP_CRS: if (!need(2)) return false; value = "float4(cross((" + s[0] + ").xyz, (" + s[1] + ").xyz), 0.0f)"; break;
        case OP_SGN: if (!need(1)) return false; value = "sign(" + s[0] + ")"; break;
        case OP_ABS: if (!need(1)) return false; value = "abs(" + s[0] + ")"; break;
        case OP_NRM: if (!need(1)) return false; value = "d3d_nrm(" + s[0] + ")"; break;
        case OP_SINCOS: if (!need(1)) return false; value = "float4(cos((" + s[0] + ").x), sin((" + s[0] + ").x), 0.0f, 0.0f)"; break;
        case OP_CMP: if (!need(3)) return false; value = "select(" + s[2] + ", " + s[1] + ", " + s[0] + " >= 0.0f)"; break;
        case OP_DP2ADD: if (!need(3)) return false; value = "float4(dot((" + s[0] + ").xy, (" + s[1] + ").xy) + (" + s[2] + ").x)"; break;
        case OP_DSX: if (!need(1)) return false; value = "dfdx(" + s[0] + ")"; break;
        case OP_DSY: if (!need(1)) return false; value = "dfdy(" + s[0] + ")"; break;

        case OP_CND:
            if (!need(3)) return false;
            if (this->IsPs11()) {
                // ps_1_1 to ps_1_3 compare r0.a
                value = "(((" + s[0] + ").w > 0.5f) ? " + s[1] + " : " + s[2] + ")";
            } else {
                value = "select(" + s[2] + ", " + s[1] + ", " + s[0] + " > 0.5f)";
            }
            break;

        case OP_M4x4:
        case OP_M4x3:
        case OP_M3x4:
        case OP_M3x3:
        case OP_M3x2: {
            if (!need(1)) return false;
            if (sources.size() < 2) return this->Fail("matrix instruction without a matrix");

            uint32_t rows = instruction.m_opcode == OP_M4x4 || instruction.m_opcode == OP_M3x4 ? 4 : instruction.m_opcode == OP_M3x2 ? 2 : 3;
            bool four = instruction.m_opcode == OP_M4x4 || instruction.m_opcode == OP_M4x3;
            std::string components[4] = { "0.0f", "0.0f", "0.0f", "0.0f" };

            for (uint32_t row = 0; row < rows; row++) {
                SourceOperand matrix = sources[1];
                matrix.m_index += row;
                matrix.m_swizzle[0] = 0;
                matrix.m_swizzle[1] = 1;
                matrix.m_swizzle[2] = 2;
                matrix.m_swizzle[3] = 3;

                std::string rowValue;
                if (!this->Source(matrix, rowValue)) {
                    return false;
                }

                components[row] = four ? "dot(" + s[0] + ", " + rowValue + ")" : "dot((" + s[0] + ").xyz, (" + rowValue + ").xyz)";
            }

            value = "float4(" + components[0] + ", " + components[1] + ", " + components[2] + ", " + components[3] + ")";
            break;
        }

        default:
            return this->Fail("unsupported instruction %s", OpcodeName(instruction.m_opcode));
    }

    if (!instruction.m_hasDest) {
        return this->Fail("%s without a destination", OpcodeName(instruction.m_opcode));
    }

    // Evaluate into a temporary first: co-issued instructions read their sources before either writes
    std::string temporary = Format("t_%u", this->m_temporaryCounter++);
    this->Line("float4 " + temporary + " = " + value + ";");
    return this->Write(instruction.m_dest, temporary, writes);
}

bool Translator::EmitInstruction(size_t& index) {
    const Instruction& instruction = this->m_program.m_instructions[index];
    std::vector<std::string> writes;

    switch (instruction.m_opcode) {
        case OP_NOP:
        case OP_DCL:
        case OP_DEF:
        case OP_DEFI:
        case OP_DEFB:
        case OP_PHASE:
            return true;

        // Flow control

        case OP_REP: {
            std::string count;
            if (instruction.m_sources.empty() || !this->IntConstant(instruction.m_sources[0], count)) {
                return this->m_error.empty() ? this->Fail("rep without a count") : false;
            }
            this->Line(Format("for (int rep_%u = 0; rep_%u < min((%s).x, 255); rep_%u++) {", this->m_temporaryCounter, this->m_temporaryCounter, count.c_str(), this->m_temporaryCounter));
            this->m_temporaryCounter++;
            this->m_indent++;
            return true;
        }

        case OP_ENDREP:
        case OP_ENDLOOP:
        case OP_ENDIF:
            if (this->m_indent <= 1) {
                return this->Fail("unbalanced %s", OpcodeName(instruction.m_opcode));
            }
            if (instruction.m_opcode == OP_ENDLOOP) {
                this->m_loopDepth--;
                this->m_indent--;
                this->Line("}");
            }
            this->m_indent--;
            this->Line("}");
            return true;

        case OP_LOOP: {
            std::string counter;
            if (instruction.m_sources.size() < 2 || !this->IntConstant(instruction.m_sources[1], counter)) {
                return this->m_error.empty() ? this->Fail("loop without a counter") : false;
            }
            uint32_t id = this->m_temporaryCounter++;
            this->Line(Format("{ int4 loop_%u = %s;", id, counter.c_str()));
            this->m_indent++;
            this->Line(Format("for (int iteration_%u = 0, aL = loop_%u.y; iteration_%u < min(loop_%u.x, 255); iteration_%u++, aL += loop_%u.z) {", id, id, id, id, id, id));
            this->m_indent++;
            this->m_loopDepth++;
            return true;
        }

        case OP_BREAK:
            this->Line("break;");
            return true;

        case OP_BREAKC:
        case OP_IFC: {
            const char* op = ComparisonOperator(instruction.m_control);
            std::string a;
            std::string b;
            if (!op || instruction.m_sources.size() < 2 || !this->Source(instruction.m_sources[0], a) || !this->Source(instruction.m_sources[1], b)) {
                return this->m_error.empty() ? this->Fail("malformed %s", OpcodeName(instruction.m_opcode)) : false;
            }
            if (instruction.m_opcode == OP_BREAKC) {
                this->Line(Format("if ((%s).x %s (%s).x) break;", a.c_str(), op, b.c_str()));
            } else {
                this->Line(Format("if ((%s).x %s (%s).x) {", a.c_str(), op, b.c_str()));
                this->m_indent++;
            }
            return true;
        }

        case OP_IF: {
            std::string condition;
            if (instruction.m_sources.empty() || !this->BoolConstant(instruction.m_sources[0], condition)) {
                return this->m_error.empty() ? this->Fail("if without a condition") : false;
            }
            this->Line("if (" + condition + ") {");
            this->m_indent++;
            return true;
        }

        case OP_ELSE:
            if (this->m_indent <= 1) {
                return this->Fail("else outside of if");
            }
            this->m_indent--;
            this->Line("} else {");
            this->m_indent++;
            return true;

        // Texture instructions

        case OP_TEXKILL: {
            const DestOperand& dest = instruction.m_dest;
            std::string reg;
            bool scalar;
            if (this->IsPs1() && dest.m_type == REG_TEXTURE && this->IsPs11()) {
                // ps_1_1 to ps_1_3 test the texture coordinates of the stage
                reg = Format("in.texcoord%u", dest.m_index);
            } else if (this->Major() == 1 && dest.m_type == REG_TEXTURE) {
                reg = Format("t%u", dest.m_index);
            } else if (!this->DestRegister(dest, reg, scalar)) {
                return false;
            }
            // Shader model 1 tests xyz, later models all four components
            this->Line(Format("if (any((%s)%s < 0.0f)) discard_fragment();", reg.c_str(), this->Major() >= 2 ? "" : ".xyz"));
            return true;
        }

        case OP_TEX: {
            if (!this->IsPixel()) {
                return this->Fail("texld in a vertex shader");
            }

            std::string sample;

            if (this->IsPs11()) {
                uint32_t stage = instruction.m_dest.m_index;
                std::string coord = this->m_variant.m_pointSprite ? "float4(pointCoord, 0.0f, 1.0f)" : Format("in.texcoord%u", stage);
                uint32_t count = stage < 4 ? (this->m_variant.m_projected >> (stage * 3)) & 0x7 : 0;
                if (count >= 2) {
                    coord = Format("(%s / (%s).%c)", coord.c_str(), coord.c_str(), kComponents[count - 1]);
                }
                if (!this->Sample(stage, coord, TEXLD_NORMAL, "", sample)) {
                    return false;
                }
            } else if (this->Major() == 1) {
                // ps_1_4: texld r#, t#|r# samples the stage of the destination register
                std::string coord;
                if (instruction.m_sources.empty() || !this->Source(instruction.m_sources[0], coord)) {
                    return this->m_error.empty() ? this->Fail("texld without coordinates") : false;
                }
                if (!this->Sample(instruction.m_dest.m_index, coord, TEXLD_NORMAL, "", sample)) {
                    return false;
                }
            } else {
                if (instruction.m_sources.size() < 2 || instruction.m_sources[1].m_type != REG_SAMPLER) {
                    return this->Fail("malformed texld");
                }
                std::string coord;
                if (!this->Source(instruction.m_sources[0], coord)) {
                    return false;
                }
                const SourceOperand& sampler = instruction.m_sources[1];
                if (!this->Sample(sampler.m_index, coord, instruction.m_control, "", sample)) {
                    return false;
                }
                // The sampler register's swizzle selects result components
                bool identity = sampler.m_swizzle[0] == 0 && sampler.m_swizzle[1] == 1 && sampler.m_swizzle[2] == 2 && sampler.m_swizzle[3] == 3;
                if (!identity) {
                    sample = "(" + sample + ")." + std::string(1, kComponents[sampler.m_swizzle[0]]) + kComponents[sampler.m_swizzle[1]] + kComponents[sampler.m_swizzle[2]] + kComponents[sampler.m_swizzle[3]];
                }
            }

            std::string temporary = Format("t_%u", this->m_temporaryCounter++);
            this->Line("float4 " + temporary + " = " + sample + ";");
            if (!this->Write(instruction.m_dest, temporary, writes)) {
                return false;
            }
            break;
        }

        case OP_TEXLDL:
        case OP_TEXLDD: {
            bool lod = instruction.m_opcode == OP_TEXLDL;
            size_t count = lod ? 2 : 4;
            if (instruction.m_sources.size() < count || instruction.m_sources[1].m_type != REG_SAMPLER) {
                return this->Fail("malformed %s", OpcodeName(instruction.m_opcode));
            }

            std::string coord;
            if (!this->Source(instruction.m_sources[0], coord)) {
                return false;
            }

            uint32_t sampler = instruction.m_sources[1].m_index;
            std::string extra;

            if (lod) {
                extra = Format(", level((%s).w)", coord.c_str());
            } else {
                std::string dx;
                std::string dy;
                if (!this->Source(instruction.m_sources[2], dx) || !this->Source(instruction.m_sources[3], dy)) {
                    return false;
                }
                switch (this->m_variant.Kind(sampler)) {
                    case SAMPLERKIND_CUBE:
                        extra = Format(", gradientcube((%s).xyz, (%s).xyz)", dx.c_str(), dy.c_str());
                        break;
                    case SAMPLERKIND_VOLUME:
                        extra = Format(", gradient3d((%s).xyz, (%s).xyz)", dx.c_str(), dy.c_str());
                        break;
                    default:
                        extra = Format(", gradient2d((%s).xy, (%s).xy)", dx.c_str(), dy.c_str());
                        break;
                }
            }

            std::string sample;
            if (this->IsVertex()) {
                return this->Fail("vertex texture fetch is not supported");
            }
            if (!this->Sample(sampler, coord, TEXLD_NORMAL, extra, sample)) {
                return false;
            }

            const SourceOperand& samplerOperand = instruction.m_sources[1];
            bool identity = samplerOperand.m_swizzle[0] == 0 && samplerOperand.m_swizzle[1] == 1 && samplerOperand.m_swizzle[2] == 2 && samplerOperand.m_swizzle[3] == 3;
            if (!identity) {
                sample = "(" + sample + ")." + std::string(1, kComponents[samplerOperand.m_swizzle[0]]) + kComponents[samplerOperand.m_swizzle[1]] + kComponents[samplerOperand.m_swizzle[2]] + kComponents[samplerOperand.m_swizzle[3]];
            }

            std::string temporary = Format("t_%u", this->m_temporaryCounter++);
            this->Line("float4 " + temporary + " = " + sample + ";");
            if (!this->Write(instruction.m_dest, temporary, writes)) {
                return false;
            }
            break;
        }

        case OP_TEXCOORD: {
            std::string value;
            if (this->IsPs11()) {
                // texcoord t#: the stage's coordinates clamped to [0, 1], with w = 1
                value = Format("float4(saturate(in.texcoord%u.xyz), 1.0f)", instruction.m_dest.m_index);
            } else {
                // ps_1_4 texcrd r#, t#
                if (instruction.m_sources.empty() || !this->Source(instruction.m_sources[0], value)) {
                    return this->m_error.empty() ? this->Fail("texcrd without a source") : false;
                }
            }

            std::string temporary = Format("t_%u", this->m_temporaryCounter++);
            this->Line("float4 " + temporary + " = " + value + ";");
            if (!this->Write(instruction.m_dest, temporary, writes)) {
                return false;
            }
            break;
        }

        case OP_TEXBEM:
        case OP_TEXBEML: {
            if (!this->IsPs11() || instruction.m_sources.empty()) {
                return this->Fail("malformed %s", OpcodeName(instruction.m_opcode));
            }

            uint32_t stage = instruction.m_dest.m_index;
            std::string source;
            if (!this->Source(instruction.m_sources[0], source)) {
                return false;
            }

            // The bump matrix of the stage transforms the source's du, dv
            std::string base = this->m_variant.m_pointSprite ? "float4(pointCoord, 0.0f, 1.0f)" : Format("in.texcoord%u", stage);
            uint32_t id = this->m_temporaryCounter++;
            this->Line(Format("float4 bump_%u = %s;", id, source.c_str()));
            this->Line(Format("float4 bem_%u = %s;", id, base.c_str()));
            this->Line(Format("bem_%u.xy += float2(dot(bump_%u.xy, state.bumpEnvMat[%u].xz), dot(bump_%u.xy, state.bumpEnvMat[%u].yw));", id, id, stage, id, stage));

            std::string sample;
            if (!this->Sample(stage, Format("bem_%u", id), TEXLD_NORMAL, "", sample)) {
                return false;
            }

            std::string temporary = Format("t_%u", this->m_temporaryCounter++);
            this->Line("float4 " + temporary + " = " + sample + ";");

            if (instruction.m_opcode == OP_TEXBEML) {
                this->Line(Format("%s.xyz *= saturate(bump_%u.z * state.bumpEnvLum[%u].x + state.bumpEnvLum[%u].y);", temporary.c_str(), id, stage, stage));
            }

            if (!this->Write(instruction.m_dest, temporary, writes)) {
                return false;
            }
            break;
        }

        default:
            if (!this->EmitArithmetic(instruction, writes)) {
                return false;
            }
            break;
    }

    // A co-issued successor reads its sources before this instruction's results are written
    while (index + 1 < this->m_program.m_instructions.size() && this->m_program.m_instructions[index + 1].m_coissue) {
        index++;
        if (!this->EmitArithmetic(this->m_program.m_instructions[index], writes)) {
            return false;
        }
    }

    for (const std::string& write : writes) {
        this->Line(write);
    }

    return true;
}

bool Translator::EmitBody() {
    for (size_t i = 0; i < this->m_program.m_instructions.size(); i++) {
        if (!this->EmitInstruction(i)) {
            return false;
        }
    }

    if (this->m_indent != 1) {
        return this->Fail("unterminated flow control");
    }

    return true;
}

bool Translator::EmitVertex(const VertexInputSignature& inputs, std::string& source) {
    // Relative constant reads honor local constants and return zero out of range
    std::string loader;
    if (this->m_constRelative) {
        loader += "static float4 load_c(constant float4* c, int index) {\n";
        for (const auto& [reg, value] : this->m_floatDefs) {
            loader += Format("    if (index == %u) return float4(%s, %s, %s, %s);\n", reg, FloatLiteral(value[0]).c_str(), FloatLiteral(value[1]).c_str(), FloatLiteral(value[2]).c_str(), FloatLiteral(value[3]).c_str());
        }
        loader += "    return uint(index) < 256u ? c[index] : float4(0.0f);\n}\n\n";
    }

    // Inputs
    std::string inputStruct;
    std::vector<std::string> inputSetup;
    bool hasInputs = false;

    for (uint32_t reg = 0; reg < 16; reg++) {
        if (!(this->m_inputMask & (1u << reg))) {
            continue;
        }

        auto declaration = this->m_inputs.find(reg);
        int32_t attribute = declaration != this->m_inputs.end() ? AttributeIndex(declaration->second.m_usage, declaration->second.m_usageIndex) : -1;
        uint8_t attributeClass = attribute >= 0 ? inputs.m_class[attribute] : ATTRIBCLASS_NONE;

        switch (attributeClass) {
            case ATTRIBCLASS_FLOAT:
                inputStruct += Format("    float4 a%d [[attribute(%d)]];\n", attribute, attribute);
                inputSetup.push_back(Format("float4 v%u = in.a%d;", reg, attribute));
                hasInputs = true;
                break;
            case ATTRIBCLASS_UINT:
                inputStruct += Format("    uint4 a%d [[attribute(%d)]];\n", attribute, attribute);
                inputSetup.push_back(Format("float4 v%u = float4(in.a%d);", reg, attribute));
                hasInputs = true;
                break;
            case ATTRIBCLASS_SINT:
                inputStruct += Format("    int4 a%d [[attribute(%d)]];\n", attribute, attribute);
                inputSetup.push_back(Format("float4 v%u = float4(in.a%d);", reg, attribute));
                hasInputs = true;
                break;
            default:
                // Not in the vertex declaration
                inputSetup.push_back(Format("float4 v%u = float4(0.0f, 0.0f, 0.0f, 1.0f);", reg));
                break;
        }
    }

    // Body
    this->m_indent = 1;
    if (!this->EmitBody()) {
        return false;
    }

    source = kShaderPrelude;
    source += "\n";
    source += loader;

    if (hasInputs) {
        source += "struct D3DVertexInput {\n" + inputStruct + "};\n\n";
    }

    source += Format("vertex D3DVaryings %s(", kVertexFunctionName);
    if (hasInputs) {
        source += "D3DVertexInput in [[stage_in]], ";
    }
    source += Format("constant float4* c [[buffer(%d)]], constant int4* ci [[buffer(%d)]], constant uint* cb [[buffer(%d)]], constant D3DVertexState& state [[buffer(%d)]]) {\n", VS_BUFFER_FLOAT_CONSTANTS, VS_BUFFER_INT_CONSTANTS, VS_BUFFER_BOOL_CONSTANTS, VS_BUFFER_STATE);

    for (const std::string& line : inputSetup) {
        source += "    " + line + "\n";
    }

    for (uint32_t i = 0; i < 32; i++) {
        if (this->m_tempMask & (1u << i)) {
            source += Format("    float4 r%u = float4(0.0f);\n", i);
        }
    }

    if (this->m_usesAddress) {
        source += "    int4 a0 = int4(0);\n";
    }

    source += "    float4 oPos = float4(0.0f);\n";
    source += "    float oFog = 0.0f;\n";
    source += "    float oPts = state.pointSize.x;\n";

    if (this->Major() < 3) {
        // Unwritten colors default to opaque white for the diffuse color and zero otherwise
        source += Format("    float4 oD0 = float4(%s);\n", (this->m_writtenColorMask & 1) ? "0.0f" : "1.0f");
        source += "    float4 oD1 = float4(0.0f);\n";
        for (uint32_t i = 0; i < 8; i++) {
            source += Format("    float4 oT%u = float4(0.0f);\n", i);
        }
    } else {
        for (const auto& [reg, declaration] : this->m_outputs) {
            source += Format("    float4 o%u = float4(0.0f);\n", reg);
        }
    }

    source += this->m_body;

    source += "    D3DVaryings out;\n";

    if (this->Major() < 3) {
        source += "    out.color0 = saturate(oD0);\n";
        source += "    out.color1 = saturate(oD1);\n";
        for (uint32_t i = 0; i < 8; i++) {
            source += Format("    out.texcoord%u = oT%u;\n", i, i);
        }
    } else {
        bool color0 = false;
        source += "    out.color0 = float4(0.0f);\n";
        source += "    out.color1 = float4(0.0f);\n";
        for (uint32_t i = 0; i < 8; i++) {
            source += Format("    out.texcoord%u = float4(0.0f);\n", i);
        }

        for (const auto& [reg, declaration] : this->m_outputs) {
            switch (declaration.m_usage) {
                case USAGE_POSITION:
                    if (declaration.m_usageIndex == 0) {
                        source += Format("    oPos = o%u;\n", reg);
                    }
                    break;
                case USAGE_PSIZE:
                    source += Format("    oPts = o%u.x;\n", reg);
                    break;
                case USAGE_FOG:
                    source += Format("    oFog = o%u.x;\n", reg);
                    break;
                default: {
                    const char* field = SemanticField(declaration.m_usage, declaration.m_usageIndex);
                    if (field) {
                        source += Format("    out.%s = o%u;\n", field, reg);
                        if (declaration.m_usage == USAGE_COLOR && declaration.m_usageIndex == 0) {
                            color0 = true;
                        }
                    }
                    break;
                }
            }
        }

        if (!color0) {
            source += "    out.color0 = float4(1.0f);\n";
        }
    }

    source += "    out.fog = oFog;\n";
    source += "    out.pointSize = clamp(oPts, state.pointSize.y, state.pointSize.z);\n";

    // User clip planes are in clip space for programmable vertex processing
    for (uint32_t i = 0; i < 6; i++) {
        source += Format("    out.clipDistance[%u] = (state.flags.x & %uu) ? dot(oPos, state.clipPlanes[%u]) : 1.0f;\n", i, 1u << i, i);
    }

    // Direct3D 9 pixel centers are at integer coordinates; depth bias is in post-divide units
    source += "    out.position = oPos;\n";
    source += "    out.position.xy += state.posFixup.xy * oPos.w;\n";
    source += "    out.position.z += state.posFixup.z * oPos.w;\n";
    source += "    return out;\n}\n";

    return true;
}

bool Translator::EmitPixel(const PixelShaderVariant& variant, std::string& source) {
    this->m_variant = variant;

    // Inputs and their setup
    std::vector<std::string> fields;
    std::vector<std::string> setup;
    auto field = [&](const char* name, const char* type) {
        std::string declaration = Format("    %s %s [[user(d3d_%s)]];\n", type, name, name);
        for (const std::string& existing : fields) {
            if (existing == declaration) {
                return;
            }
        }
        fields.push_back(declaration);
    };

    if (this->Major() < 3) {
        for (uint32_t i = 0; i < 2; i++) {
            if (this->m_inputMask & (1u << i)) {
                field(i == 0 ? "color0" : "color1", "float4");
                setup.push_back(Format("float4 v%u = in.color%u;", i, i));
            }
        }

        if (this->IsPs11()) {
            // Texture registers start out undefined and are loaded by texture instructions
            for (uint32_t i = 0; i < 8; i++) {
                if (this->m_textureMask & (1u << i)) {
                    setup.push_back(Format("float4 t%u = float4(0.0f);", i));
                }
            }

            // Texture instructions read the stage's coordinates directly
            for (const Instruction& instruction : this->m_program.m_instructions) {
                if (instruction.m_hasDest && instruction.m_dest.m_type == REG_TEXTURE && !variant.m_pointSprite) {
                    switch (instruction.m_opcode) {
                        case OP_TEX:
                        case OP_TEXCOORD:
                        case OP_TEXBEM:
                        case OP_TEXBEML:
                        case OP_TEXKILL: {
                            static const char* const s_names[8] = { "texcoord0", "texcoord1", "texcoord2", "texcoord3", "texcoord4", "texcoord5", "texcoord6", "texcoord7" };
                            if (instruction.m_dest.m_index < 8) {
                                field(s_names[instruction.m_dest.m_index], "float4");
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        } else {
            // ps_1_4 and ps_2_x: t# are the interpolated texture coordinates
            for (uint32_t i = 0; i < 8; i++) {
                if (this->m_textureMask & (1u << i)) {
                    if (variant.m_pointSprite) {
                        setup.push_back(Format("float4 t%u = float4(pointCoord, 0.0f, 1.0f);", i));
                    } else {
                        static const char* const s_names[8] = { "texcoord0", "texcoord1", "texcoord2", "texcoord3", "texcoord4", "texcoord5", "texcoord6", "texcoord7" };
                        field(s_names[i], "float4");
                        setup.push_back(Format("float4 t%u = in.texcoord%u;", i, i));
                    }
                }
            }
        }

        field("fog", "float");
    } else {
        for (uint32_t reg = 0; reg < 16; reg++) {
            if (!(this->m_inputMask & (1u << reg))) {
                continue;
            }

            auto declaration = this->m_inputs.find(reg);
            if (declaration == this->m_inputs.end()) {
                return this->Fail("input v%u read without a declaration", reg);
            }

            const Declaration& dcl = declaration->second;
            const char* name = SemanticField(dcl.m_usage, dcl.m_usageIndex);
            if (!name) {
                return this->Fail("unsupported input semantic %u/%u", dcl.m_usage, dcl.m_usageIndex);
            }

            if (dcl.m_usage == USAGE_FOG) {
                field(name, "float");
                setup.push_back(Format("float4 v%u = float4(in.fog);", reg));
            } else if (dcl.m_usage == USAGE_TEXCOORD && variant.m_pointSprite) {
                setup.push_back(Format("float4 v%u = float4(pointCoord, 0.0f, 1.0f);", reg));
            } else {
                field(name, "float4");
                setup.push_back(Format("float4 v%u = in.%s;", reg, name));
            }
        }
    }

    if (this->m_usesPosition) {
        setup.push_back("float4 vPos = float4(fragCoord.xy - 0.5f, 0.0f, 0.0f);");
    }

    if (this->m_usesFace) {
        setup.push_back("float4 vFace = float4(frontFacing ? 1.0f : -1.0f);");
    }

    // Body
    this->m_indent = 1;
    if (!this->EmitBody()) {
        return false;
    }

    // Constants
    std::string loader;
    if (this->m_constRelative) {
        uint32_t limit = this->FloatConstantLimit();
        loader += "static float4 load_c(constant float4* c, int index) {\n";
        for (const auto& [reg, value] : this->m_floatDefs) {
            loader += Format("    if (index == %u) return float4(%s, %s, %s, %s);\n", reg, FloatLiteral(value[0]).c_str(), FloatLiteral(value[1]).c_str(), FloatLiteral(value[2]).c_str(), FloatLiteral(value[3]).c_str());
        }
        loader += Format("    return uint(index) < %uu ? c[index] : float4(0.0f);\n}\n\n", limit);
    }

    source = kShaderPrelude;
    source += "\n";
    source += loader;

    if (!fields.empty()) {
        source += "struct D3DFragmentInput {\n";
        for (const std::string& line : fields) {
            source += line;
        }
        source += "};\n\n";
    }

    source += "struct D3DFragmentOutput {\n    float4 color [[color(0)]];\n";
    if (this->m_writesDepth) {
        source += "    float depth [[depth(any)]];\n";
    }
    source += "};\n\n";

    source += Format("fragment D3DFragmentOutput %s(%sconstant float4* c [[buffer(%d)]], constant int4* ci [[buffer(%d)]], constant uint* cb [[buffer(%d)]], constant D3DPixelState& state [[buffer(%d)]]", kFragmentFunctionName, fields.empty() ? "" : "D3DFragmentInput in [[stage_in]], ", PS_BUFFER_FLOAT_CONSTANTS, PS_BUFFER_INT_CONSTANTS, PS_BUFFER_BOOL_CONSTANTS, PS_BUFFER_STATE);

    for (uint32_t i = 0; i < 16; i++) {
        if (!(this->m_samplerMask & (1u << i))) {
            continue;
        }

        const char* type;
        switch (variant.Kind(i)) {
            case SAMPLERKIND_CUBE: type = "texturecube<float>"; break;
            case SAMPLERKIND_VOLUME: type = "texture3d<float>"; break;
            case SAMPLERKIND_DEPTH: type = "depth2d<float>"; break;
            default: type = "texture2d<float>"; break;
        }

        source += Format(", %s tex%u [[texture(%u)]], sampler smp%u [[sampler(%u)]]", type, i, i, i, i);
    }

    if (variant.m_pointSprite) {
        source += ", float2 pointCoord [[point_coord]]";
    }
    if (this->m_usesPosition) {
        source += ", float4 fragCoord [[position]]";
    }
    if (this->m_usesFace) {
        source += ", bool frontFacing [[front_facing]]";
    }
    source += ") {\n";

    for (const std::string& line : setup) {
        source += "    " + line + "\n";
    }

    for (uint32_t i = 0; i < 32; i++) {
        if (this->m_tempMask & (1u << i)) {
            source += Format("    float4 r%u = float4(0.0f);\n", i);
        }
    }

    if (this->Major() >= 2) {
        source += "    float4 oC0 = float4(0.0f);\n";
    }
    if (this->m_writesDepth) {
        source += "    float oDepth = 0.0f;\n";
    }

    source += this->m_body;

    source += "    D3DFragmentOutput out;\n";
    source += Format("    out.color = %s;\n", this->Major() == 1 ? "r0" : "oC0");

    // Fixed-function fog follows pixel shaders before shader model 3
    if (this->Major() < 3) {
        source += "    out.color = d3d_fog(state, out.color, in.fog);\n";
    }

    source += "    if (!d3d_alpha_test(state, out.color.a)) discard_fragment();\n";

    if (this->m_writesDepth) {
        source += "    out.depth = saturate(oDepth);\n";
    }

    source += "    return out;\n}\n";

    return true;
}

}

bool AnalyzeShader(const ShaderProgram& program, ShaderInfo& info, std::string& error) {
    Translator translator(program, error);
    if (!translator.Analyze()) {
        return false;
    }
    translator.FillInfo(info);
    return true;
}

bool TranslateVertexShader(const ShaderProgram& program, const VertexInputSignature& inputs, std::string& source, std::string& error) {
    if (program.m_stage != STAGE_VERTEX) {
        error = "not a vertex shader";
        return false;
    }

    Translator translator(program, error);
    return translator.Analyze() && translator.EmitVertex(inputs, source);
}

bool TranslatePixelShader(const ShaderProgram& program, const PixelShaderVariant& variant, std::string& source, std::string& error) {
    if (program.m_stage != STAGE_PIXEL) {
        error = "not a pixel shader";
        return false;
    }

    Translator translator(program, error);
    return translator.Analyze() && translator.EmitPixel(variant, source);
}

}
