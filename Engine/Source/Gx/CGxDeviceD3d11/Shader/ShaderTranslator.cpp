#include "Engine/Source/Gx/CGxDeviceD3d11/Shader/ShaderTranslator.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>

namespace GxD3d11 {

// GxShader and GxDxbc both name their opcodes OP_*, so DXBC opcodes are always qualified
using GxDxbc::CB;
using GxDxbc::CBRelative;
using GxDxbc::ImmF;
using GxDxbc::ImmI;
using GxDxbc::ImmU;
using GxDxbc::O;
using GxDxbc::Operand;
using GxDxbc::OutputDepth;
using GxDxbc::R;
using GxDxbc::Sampler;
using GxDxbc::SignatureElement;
using GxDxbc::Texture;
using GxDxbc::V;
using GxDxbc::Writer;
using GxDxbc::COMPONENT_FLOAT;
using GxDxbc::COMPONENT_SINT;
using GxDxbc::COMPONENT_UINT;
using GxDxbc::DIM_TEXTURE2D;
using GxDxbc::DIM_TEXTURE3D;
using GxDxbc::DIM_TEXTURECUBE;
using GxDxbc::INTERP_LINEAR;
using GxDxbc::INTERP_LINEAR_NOPERSPECTIVE;
using GxDxbc::SV_CLIP_DISTANCE;
using GxDxbc::SV_IS_FRONT_FACE;
using GxDxbc::SV_NONE;
using GxDxbc::SV_POSITION;
using GxDxbc::TYPE_IMM32;

bool PixelShaderVariant::operator==(const PixelShaderVariant& other) const {
    return this->m_samplerKinds == other.m_samplerKinds && this->m_projected == other.m_projected && this->m_alphaTest == other.m_alphaTest;
}

void VaryingSignature(std::vector<SignatureElement>& elements, bool pixelInput) {
    elements.clear();
    elements.push_back({ "SV_Position", 0, SV_POSITION, COMPONENT_FLOAT, VARYING_POSITION, 0xF, static_cast<uint8_t>(pixelInput ? 0x0 : 0x0) });
    elements.push_back({ "COLOR", 0, SV_NONE, COMPONENT_FLOAT, VARYING_COLOR0, 0xF, 0 });
    elements.push_back({ "COLOR", 1, SV_NONE, COMPONENT_FLOAT, VARYING_COLOR1, 0xF, 0 });

    for (uint32_t i = 0; i < 8; i++) {
        elements.push_back({ "TEXCOORD", i, SV_NONE, COMPONENT_FLOAT, VARYING_TEXCOORD0 + i, 0xF, 0 });
    }

    elements.push_back({ "FOG", 0, SV_NONE, COMPONENT_FLOAT, VARYING_FOG, 0x1, 0 });
    elements.push_back({ "SV_ClipDistance", 0, SV_CLIP_DISTANCE, COMPONENT_FLOAT, VARYING_CLIP0, 0xF, 0 });
    elements.push_back({ "SV_ClipDistance", 1, SV_CLIP_DISTANCE, COMPONENT_FLOAT, VARYING_CLIP1, 0x3, 0 });
    elements.push_back({ "PSIZE", 0, SV_NONE, COMPONENT_FLOAT, VARYING_PSIZE, 0x1, 0 });
}

void VertexInputSignatureElements(const VertexInputSignature& inputs, uint32_t attributeMask, std::vector<SignatureElement>& elements) {
    elements.clear();

    for (uint32_t i = 0; i < ATTRIB_COUNT; i++) {
        if (!(attributeMask & (1u << i))) {
            continue;
        }

        uint32_t type = inputs.m_class[i] == ATTRIBCLASS_UINT ? COMPONENT_UINT : inputs.m_class[i] == ATTRIBCLASS_SINT ? COMPONENT_SINT : COMPONENT_FLOAT;
        elements.push_back({ "ATTRIB", i, SV_NONE, type, i, 0xF, 0xF });
    }
}

void DeclareVaryingOutputs(Writer& writer) {
    writer.DclOutputSiv(VARYING_POSITION, 0xF, SV_POSITION);

    for (uint32_t i = VARYING_COLOR0; i < VARYING_FOG; i++) {
        writer.DclOutput(i, 0xF);
    }

    writer.DclOutput(VARYING_FOG, 0x1);
    writer.DclOutputSiv(VARYING_CLIP0, 0xF, SV_CLIP_DISTANCE);
    writer.DclOutputSiv(VARYING_CLIP1, 0x3, SV_CLIP_DISTANCE);
    writer.DclOutput(VARYING_PSIZE, 0x1);
}

void EmitVertexEpilogue(Writer& writer, uint32_t position, uint32_t clipSource, uint32_t pointSize) {
    // Direct3D 9 applies depth bias after the divide
    writer.Op(GxDxbc::OP_MOV, { O(VARYING_POSITION).Mask(0xB), R(position).Swz("xyzw") });
    writer.Op(GxDxbc::OP_MAD, { O(VARYING_POSITION).Mask(0x4), CB(CB_STATE, VSREG_POS_FIXUP).Sel(2), R(position).Sel(3), R(position).Sel(2) });

    for (uint32_t i = 0; i < 6; i++) {
        uint32_t reg = i < 4 ? VARYING_CLIP0 : VARYING_CLIP1;
        writer.Op(GxDxbc::OP_DP4, { O(reg).Mask(1u << (i & 3)), R(clipSource).Swz("xyzw"), CB(CB_STATE, VSREG_CLIP_PLANES + i).Swz("xyzw") });
    }

    writer.Op(GxDxbc::OP_MAX, { R(pointSize).Mask(0x1), R(pointSize).Sel(0), CB(CB_STATE, VSREG_POINT_SIZE).Sel(1) });
    writer.Op(GxDxbc::OP_MIN, { O(VARYING_PSIZE).Mask(0x1), R(pointSize).Sel(0), CB(CB_STATE, VSREG_POINT_SIZE).Sel(2) });
}

void EmitPixelEpilogue(Writer& writer, uint32_t color, uint32_t scratch, bool fog, bool alphaTest) {
    if (fog) {
        // factor = enabled ? saturate(fog) : 1, then color = fogColor + factor * (color - fogColor)
        writer.Op(GxDxbc::OP_MOV, { R(scratch).Mask(0x1), V(VARYING_FOG).Sel(0) }, true);
        writer.Op(GxDxbc::OP_ADD, { R(scratch).Mask(0x1), R(scratch).Sel(0), ImmF(-1.0f) });
        writer.Op(GxDxbc::OP_MAD, { R(scratch).Mask(0x1), CB(CB_STATE, PSREG_PARAMS).Sel(0), R(scratch).Sel(0), ImmF(1.0f) });
        writer.Op(GxDxbc::OP_ADD, { R(scratch + 1).Mask(0x7), R(color).Swz("xyzw"), CB(CB_STATE, PSREG_FOG_COLOR).Swz("xyzw").Neg() });
        writer.Op(GxDxbc::OP_MAD, { R(color).Mask(0x7), R(scratch).Swz("xxxx"), R(scratch + 1).Swz("xyzw"), CB(CB_STATE, PSREG_FOG_COLOR).Swz("xyzw") });
    }

    if (alphaTest) {
        // In the 8-bit precision of the target, as Direct3D 9 hardware compares
        writer.Op(GxDxbc::OP_MUL, { R(scratch).Mask(0x1), R(color).Sel(3), ImmF(255.0f) });
        writer.Op(GxDxbc::OP_ROUND_NE, { R(scratch).Mask(0x1), R(scratch).Sel(0) });
        writer.Op(GxDxbc::OP_LT, { R(scratch).Mask(0x1), R(scratch).Sel(0), CB(CB_STATE, PSREG_PARAMS).Sel(1) });
        writer.OpTest(GxDxbc::OP_DISCARD, R(scratch).Sel(0), true);
    }
}

namespace {

std::string Format(const char* format, ...) GXSHADER_PRINTF(1, 2);

std::string Format(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

float BitsFloat(uint32_t bits) {
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

Operand Splat(float value) {
    return ImmF(value, value, value, value);
}

bool IsImmediate(const Operand& operand) {
    return operand.m_type == TYPE_IMM32;
}

Operand Swizzled(const Operand& operand, uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    if (IsImmediate(operand)) {
        Operand result = operand;
        const uint32_t* values = operand.m_immediate;
        uint32_t lanes[4] = { x, y, z, w };

        for (uint32_t i = 0; i < 4; i++) {
            result.m_immediate[i] = operand.m_components == 1 ? values[0] : values[lanes[i] & 3];
        }

        result.m_components = 4;
        return result;
    }

    return operand.Swz(x, y, z, w);
}

Operand Replicate(const Operand& operand, uint32_t component) {
    return Swizzled(operand, component, component, component, component);
}

// A source read as a single component, for operands that take select-1 form
Operand Select(const Operand& operand, uint32_t component) {
    if (IsImmediate(operand)) {
        return ImmU(operand.m_components == 1 ? operand.m_immediate[0] : operand.m_immediate[component & 3]);
    }

    return operand.Sel(component);
}

Operand Negated(const Operand& operand) {
    if (IsImmediate(operand)) {
        Operand result = operand;
        for (uint32_t i = 0; i < 4; i++) {
            result.m_immediate[i] ^= 0x80000000u;
        }
        return result;
    }

    return operand.Neg();
}

Operand Absolute(const Operand& operand) {
    if (IsImmediate(operand)) {
        Operand result = operand;
        for (uint32_t i = 0; i < 4; i++) {
            result.m_immediate[i] &= 0x7FFFFFFFu;
        }
        return result;
    }

    return operand.Abs();
}

Operand Dst(uint32_t reg, uint32_t mask = 0xF) {
    return R(reg).Mask(mask);
}

Operand Src(uint32_t reg) {
    return R(reg).Swz("xyzw");
}

uint32_t FirstComponent(uint32_t mask) {
    for (uint32_t i = 0; i < 4; i++) {
        if (mask & (1u << i)) {
            return i;
        }
    }
    return 0;
}

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
    bool EmitVertex(const VertexInputSignature& inputs, std::vector<uint8_t>& code);
    bool EmitPixel(const PixelShaderVariant& variant, std::vector<uint8_t>& code);

    private:
    struct PendingWrite {
        DestOperand m_dest;
        uint32_t m_value;
    };

    struct Flow {
        enum Kind {
            Kind_If,
            Kind_Rep,
            Kind_Loop,
        };

        Kind m_kind;
        uint32_t m_counter;
        uint32_t m_aL;
        int32_t m_step;
    };

    bool Fail(const char* format, ...) GXSHADER_PRINTF(2, 3);

    bool IsVertex() const { return this->m_program.m_stage == STAGE_VERTEX; }
    bool IsPixel() const { return this->m_program.m_stage == STAGE_PIXEL; }
    uint32_t Major() const { return this->m_program.m_major; }
    uint32_t Minor() const { return this->m_program.m_minor; }
    bool IsPs1() const { return this->IsPixel() && this->Major() == 1; }
    bool IsPs11() const { return this->IsPs1() && this->Minor() < 4; }
    uint32_t FloatConstantLimit() const;

    Writer& W() { return *this->m_writer; }
    uint32_t Persistent() { return this->m_nextPersistent++; }
    uint32_t Scratch();
    Operand Varying(uint32_t reg);

    bool AddressRegister(const RelativeAddress& address, uint32_t& reg, uint32_t& component);
    bool ConstantValue(uint32_t index, bool relative, const RelativeAddress& address, Operand& out);
    bool RegisterValue(const SourceOperand& operand, Operand& out);
    bool Source(const SourceOperand& operand, Operand& out);
    bool IntConstant(const SourceOperand& operand, std::array<int32_t, 4>& out);
    bool BoolConstant(const SourceOperand& operand, bool& out);
    bool DestRegister(const DestOperand& operand, uint32_t& reg, bool& scalar);
    bool FlushWrites(std::vector<PendingWrite>& writes);
    bool Sample(uint32_t sampler, Operand coord, uint32_t control, uint32_t mode, const Operand* extra, uint32_t& result);
    bool Compare(uint32_t comparison, const Operand& a, const Operand& b, uint32_t& result);
    void Finite(uint32_t reg, uint32_t mask);

    bool EmitInstruction(size_t& index);
    bool EmitArithmetic(const Instruction& instruction, std::vector<PendingWrite>& writes);
    bool EmitBody();
    void DeclareTemps();

    const ShaderProgram& m_program;
    std::string& m_error;
    Writer* m_writer = nullptr;

    // Analysis
    std::map<uint32_t, std::array<float, 4>> m_floatDefs;
    std::map<uint32_t, std::array<int32_t, 4>> m_intDefs;
    std::map<uint32_t, bool> m_boolDefs;
    uint32_t m_tempMask = 0;
    bool m_constRelative = false;
    uint32_t m_maxConst = 0;
    bool m_usesConst = false;
    bool m_usesAddress = false;
    uint32_t m_samplerMask = 0;
    uint32_t m_declaredKinds = 0;
    std::map<uint32_t, Declaration> m_inputs;
    std::map<uint32_t, Declaration> m_outputs;
    uint32_t m_inputMask = 0;
    uint32_t m_textureMask = 0;
    uint32_t m_writtenColorMask = 0;
    bool m_writesDepth = false;
    bool m_usesPosition = false;
    bool m_usesFace = false;
    uint32_t m_loopCount = 0;

    // Emission
    PixelShaderVariant m_variant;
    uint32_t m_nextPersistent = 32;
    uint32_t m_scratchBase = 0;
    uint32_t m_scratchNext = 0;
    uint32_t m_tempCount = 32;
    uint32_t m_usedVaryings = 0;
    uint32_t m_attributeMask = 0;
    Operand m_inputOperands[16];
    uint32_t m_regA0 = 0;
    uint32_t m_regPos = 0;
    uint32_t m_regFog = 0;
    uint32_t m_regPts = 0;
    uint32_t m_regColor[2] = {};
    uint32_t m_regTexcoordOut[8] = {};
    uint32_t m_regOutput[16] = {};
    uint32_t m_regColorOut = 0;
    uint32_t m_regDepthOut = 0;
    uint32_t m_regTexture[8] = {};
    uint32_t m_regVPos = 0;
    uint32_t m_regVFace = 0;
    std::vector<uint32_t> m_loopRegisters;
    uint32_t m_loopOrdinal = 0;
    std::vector<Flow> m_flow;
};

bool Translator::Fail(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    this->m_error = Format("%cs_%u_%u: %s", this->IsVertex() ? 'v' : 'p', this->Major(), this->Minor(), buffer);
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

uint32_t Translator::Scratch() {
    uint32_t reg = this->m_scratchNext++;

    if (this->m_scratchNext > this->m_tempCount) {
        this->m_tempCount = this->m_scratchNext;
    }

    return reg;
}

Operand Translator::Varying(uint32_t reg) {
    this->m_usedVaryings |= 1u << reg;
    return V(reg).Swz("xyzw");
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

            case OP_LOOP:
            case OP_REP:
                this->m_loopCount++;
                break;

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
                } else if (dest.m_type != REG_MISCTYPE) {
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
                    } else {
                        this->m_usesAddress = true;
                    }
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
                case REG_ATTROUT:
                    this->m_writtenColorMask |= 1u << dest.m_index;
                    break;
                case REG_DEPTHOUT:
                    this->m_writesDepth = true;
                    break;
                default:
                    break;
            }
        }
    }

    if (this->IsPs1()) {
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

    info.m_constantRelative = this->m_constRelative;

    if (this->m_constRelative) {
        for (const auto& [reg, value] : this->m_floatDefs) {
            ConstantDefinition definition;
            definition.m_register = reg;
            memcpy(definition.m_value, value.data(), sizeof(definition.m_value));
            info.m_definitions.push_back(definition);
        }
    }

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

bool Translator::AddressRegister(const RelativeAddress& address, uint32_t& reg, uint32_t& component) {
    if (address.m_type == REG_ADDR) {
        reg = this->m_regA0;
        component = address.m_component;
        return true;
    }

    if (address.m_type == REG_LOOP) {
        for (size_t i = this->m_flow.size(); i-- > 0;) {
            if (this->m_flow[i].m_kind == Flow::Kind_Loop) {
                reg = this->m_flow[i].m_aL;
                component = 0;
                return true;
            }
        }

        return this->Fail("aL used outside of a loop");
    }

    return this->Fail("unsupported relative address register type %u", address.m_type);
}

bool Translator::ConstantValue(uint32_t index, bool relative, const RelativeAddress& address, Operand& out) {
    bool clamp = this->IsPs1();

    if (relative) {
        uint32_t reg;
        uint32_t component;

        if (!this->AddressRegister(address, reg, component)) {
            return false;
        }

        out = CBRelative(CB_FLOAT_CONSTANTS, index, reg, component).Swz("xyzw");
    } else {
        auto def = this->m_floatDefs.find(index);

        if (def != this->m_floatDefs.end()) {
            std::array<float, 4> value = def->second;

            if (clamp) {
                for (float& component : value) {
                    component = component < -1.0f ? -1.0f : component > 1.0f ? 1.0f : component;
                }
            }

            out = ImmF(value[0], value[1], value[2], value[3]);
            return true;
        }

        if (index >= this->FloatConstantLimit()) {
            return this->Fail("constant register c%u out of range", index);
        }

        out = CB(CB_FLOAT_CONSTANTS, index).Swz("xyzw");
    }

    if (clamp) {
        uint32_t scratch = this->Scratch();
        W().Op(GxDxbc::OP_MAX, { Dst(scratch), out, Splat(-1.0f) });
        W().Op(GxDxbc::OP_MIN, { Dst(scratch), Src(scratch), Splat(1.0f) });
        out = Src(scratch);
    }

    return true;
}

bool Translator::RegisterValue(const SourceOperand& operand, Operand& out) {
    switch (operand.m_type) {
        case REG_TEMP:
            out = Src(operand.m_index);
            return true;

        case REG_INPUT:
            if (operand.m_index >= 16) {
                return this->Fail("input register v%u out of range", operand.m_index);
            }

            if (this->IsVertex()) {
                out = this->m_inputOperands[operand.m_index];
                return true;
            }

            if (this->Major() < 3) {
                if (operand.m_index >= 2) {
                    return this->Fail("color input v%u out of range", operand.m_index);
                }
                out = this->Varying(VARYING_COLOR0 + operand.m_index);
                return true;
            }

            {
                auto declaration = this->m_inputs.find(operand.m_index);
                if (declaration == this->m_inputs.end()) {
                    return this->Fail("input v%u read without a declaration", operand.m_index);
                }

                const Declaration& dcl = declaration->second;

                switch (dcl.m_usage) {
                    case USAGE_COLOR:
                        if (dcl.m_usageIndex < 2) {
                            out = this->Varying(VARYING_COLOR0 + dcl.m_usageIndex);
                            return true;
                        }
                        break;
                    case USAGE_TEXCOORD:
                        if (dcl.m_usageIndex < 8) {
                            out = this->Varying(VARYING_TEXCOORD0 + dcl.m_usageIndex);
                            return true;
                        }
                        break;
                    case USAGE_FOG:
                        if (dcl.m_usageIndex == 0) {
                            out = this->Varying(VARYING_FOG).Swz("xxxx");
                            return true;
                        }
                        break;
                    default:
                        break;
                }

                return this->Fail("unsupported input semantic %u/%u", dcl.m_usage, dcl.m_usageIndex);
            }

        case REG_CONST:
            return this->ConstantValue(operand.m_index, operand.m_relative, operand.m_address, out);

        case REG_ADDR:
            if (this->IsVertex()) {
                uint32_t scratch = this->Scratch();
                W().Op(GxDxbc::OP_ITOF, { Dst(scratch), Src(this->m_regA0) });
                out = Src(scratch);
                return true;
            }

            if (operand.m_index >= 8) {
                return this->Fail("texture register t%u out of range", operand.m_index);
            }

            out = this->IsPs11() ? Src(this->m_regTexture[operand.m_index]) : this->Varying(VARYING_TEXCOORD0 + operand.m_index);
            return true;

        case REG_LOOP: {
            uint32_t reg;
            uint32_t component;
            RelativeAddress address;
            address.m_type = REG_LOOP;

            if (!this->AddressRegister(address, reg, component)) {
                return false;
            }

            uint32_t scratch = this->Scratch();
            W().Op(GxDxbc::OP_ITOF, { Dst(scratch), R(reg).Swz(component, component, component, component) });
            out = Src(scratch);
            return true;
        }

        case REG_MISCTYPE:
            out = Src(operand.m_index == 0 ? this->m_regVPos : this->m_regVFace);
            return true;

        default:
            return this->Fail("unsupported source register type %u", operand.m_type);
    }
}

bool Translator::Source(const SourceOperand& operand, Operand& out) {
    Operand value;

    if (!this->RegisterValue(operand, value)) {
        return false;
    }

    // _dz and _dw divide by a component of the register, before the swizzle
    if (operand.m_modifier == SRCMOD_DZ || operand.m_modifier == SRCMOD_DW) {
        uint32_t component = operand.m_modifier == SRCMOD_DZ ? 2 : 3;

        if (IsImmediate(value)) {
            float divisor = BitsFloat(value.m_immediate[component]);
            float values[4];
            for (uint32_t i = 0; i < 4; i++) {
                values[i] = BitsFloat(value.m_immediate[i]) / divisor;
            }
            value = ImmF(values[0], values[1], values[2], values[3]);
        } else {
            uint32_t scratch = this->Scratch();
            W().Op(GxDxbc::OP_DIV, { Dst(scratch), value, Replicate(value, component) });
            value = Src(scratch);
        }
    }

    value = Swizzled(value, operand.m_swizzle[0], operand.m_swizzle[1], operand.m_swizzle[2], operand.m_swizzle[3]);

    if (IsImmediate(value)) {
        float values[4];

        for (uint32_t i = 0; i < 4; i++) {
            float x = BitsFloat(value.m_immediate[i]);

            switch (operand.m_modifier) {
                case SRCMOD_NONE:
                case SRCMOD_DZ:
                case SRCMOD_DW: break;
                case SRCMOD_NEG: x = -x; break;
                case SRCMOD_BIAS: x = x - 0.5f; break;
                case SRCMOD_BIASNEG: x = 0.5f - x; break;
                case SRCMOD_SIGN: x = x * 2.0f - 1.0f; break;
                case SRCMOD_SIGNNEG: x = 1.0f - x * 2.0f; break;
                case SRCMOD_COMP: x = 1.0f - x; break;
                case SRCMOD_X2: x = x * 2.0f; break;
                case SRCMOD_X2NEG: x = x * -2.0f; break;
                case SRCMOD_ABS: x = fabsf(x); break;
                case SRCMOD_ABSNEG: x = -fabsf(x); break;
                default: return this->Fail("unsupported source modifier %u", operand.m_modifier);
            }

            values[i] = x;
        }

        out = ImmF(values[0], values[1], values[2], values[3]);
        return true;
    }

    uint32_t scratch;

    switch (operand.m_modifier) {
        case SRCMOD_NONE:
        case SRCMOD_DZ:
        case SRCMOD_DW:
            out = value;
            return true;
        case SRCMOD_NEG:
            out = value.Neg();
            return true;
        case SRCMOD_ABS:
            out = value.Abs();
            return true;
        case SRCMOD_ABSNEG:
            out = value.Abs().Neg();
            return true;
        case SRCMOD_BIAS:
            scratch = this->Scratch();
            W().Op(GxDxbc::OP_ADD, { Dst(scratch), value, Splat(-0.5f) });
            break;
        case SRCMOD_BIASNEG:
            scratch = this->Scratch();
            W().Op(GxDxbc::OP_ADD, { Dst(scratch), value.Neg(), Splat(0.5f) });
            break;
        case SRCMOD_SIGN:
            scratch = this->Scratch();
            W().Op(GxDxbc::OP_MAD, { Dst(scratch), value, Splat(2.0f), Splat(-1.0f) });
            break;
        case SRCMOD_SIGNNEG:
            scratch = this->Scratch();
            W().Op(GxDxbc::OP_MAD, { Dst(scratch), value, Splat(-2.0f), Splat(1.0f) });
            break;
        case SRCMOD_COMP:
            scratch = this->Scratch();
            W().Op(GxDxbc::OP_ADD, { Dst(scratch), value.Neg(), Splat(1.0f) });
            break;
        case SRCMOD_X2:
            scratch = this->Scratch();
            W().Op(GxDxbc::OP_ADD, { Dst(scratch), value, value });
            break;
        case SRCMOD_X2NEG:
            scratch = this->Scratch();
            W().Op(GxDxbc::OP_MUL, { Dst(scratch), value, Splat(-2.0f) });
            break;
        default:
            return this->Fail("unsupported source modifier %u", operand.m_modifier);
    }

    out = Src(scratch);
    return true;
}

bool Translator::IntConstant(const SourceOperand& operand, std::array<int32_t, 4>& out) {
    if (operand.m_type != REG_CONSTINT) {
        return this->Fail("loop counter is not an integer constant");
    }

    auto def = this->m_intDefs.find(operand.m_index);
    out = def != this->m_intDefs.end() ? def->second : std::array<int32_t, 4> { 0, 0, 0, 0 };
    return true;
}

bool Translator::BoolConstant(const SourceOperand& operand, bool& out) {
    if (operand.m_type != REG_CONSTBOOL) {
        return this->Fail("condition is not a boolean constant");
    }

    auto def = this->m_boolDefs.find(operand.m_index);
    out = def != this->m_boolDefs.end() && def->second;

    if (operand.m_modifier == SRCMOD_NOT) {
        out = !out;
    }

    return true;
}

bool Translator::DestRegister(const DestOperand& operand, uint32_t& reg, bool& scalar) {
    scalar = false;

    if (operand.m_relative) {
        return this->Fail("relative addressing of destination registers is not supported");
    }

    switch (operand.m_type) {
        case REG_TEMP:
            if (operand.m_index >= 32) {
                return this->Fail("temporary r%u out of range", operand.m_index);
            }
            reg = operand.m_index;
            return true;

        case REG_ADDR:
            if (this->IsVertex() || operand.m_index >= 8) {
                return this->Fail("unsupported write to register type %u", operand.m_type);
            }
            reg = this->m_regTexture[operand.m_index];
            return true;

        case REG_RASTOUT:
            switch (operand.m_index) {
                case 0:
                    reg = this->m_regPos;
                    return true;
                case 1:
                    reg = this->m_regFog;
                    scalar = true;
                    return true;
                case 2:
                    reg = this->m_regPts;
                    scalar = true;
                    return true;
                default:
                    return this->Fail("unknown rasterizer output %u", operand.m_index);
            }

        case REG_ATTROUT:
            if (operand.m_index >= 2) {
                return this->Fail("unknown color output %u", operand.m_index);
            }
            reg = this->m_regColor[operand.m_index];
            return true;

        case REG_OUTPUT:
            if (this->Major() < 3) {
                if (operand.m_index >= 8) {
                    return this->Fail("unknown texture coordinate output %u", operand.m_index);
                }
                reg = this->m_regTexcoordOut[operand.m_index];
            } else {
                if (operand.m_index >= 16) {
                    return this->Fail("output o%u out of range", operand.m_index);
                }
                reg = this->m_regOutput[operand.m_index];
            }
            return true;

        case REG_COLOROUT:
            if (operand.m_index != 0) {
                return this->Fail("multiple render targets are not supported");
            }
            reg = this->m_regColorOut;
            return true;

        case REG_DEPTHOUT:
            reg = this->m_regDepthOut;
            scalar = true;
            return true;

        default:
            return this->Fail("unsupported destination register type %u", operand.m_type);
    }
}

bool Translator::FlushWrites(std::vector<PendingWrite>& writes) {
    for (const PendingWrite& write : writes) {
        uint32_t reg;
        bool scalar;

        if (!this->DestRegister(write.m_dest, reg, scalar)) {
            return false;
        }

        uint32_t value = write.m_value;

        if (write.m_dest.m_shift) {
            int32_t shift = write.m_dest.m_shift;
            float scale = shift > 0 ? static_cast<float>(1 << shift) : 1.0f / static_cast<float>(1 << -shift);
            uint32_t scaled = this->Scratch();
            W().Op(GxDxbc::OP_MUL, { Dst(scaled), Src(value), Splat(scale) });
            value = scaled;
        }

        uint32_t mask = write.m_dest.m_writeMask ? write.m_dest.m_writeMask : 0xF;

        if (scalar) {
            W().Op(GxDxbc::OP_MOV, { Dst(reg, 0x1), R(value).Sel(FirstComponent(mask)) }, write.m_dest.m_saturate);
        } else {
            W().Op(GxDxbc::OP_MOV, { Dst(reg, mask), Src(value) }, write.m_dest.m_saturate);
        }
    }

    writes.clear();
    return true;
}

// mode 0: normal (control decides projection and bias), 1: explicit level, 2: explicit gradients
bool Translator::Sample(uint32_t sampler, Operand coord, uint32_t control, uint32_t mode, const Operand* extra, uint32_t& result) {
    if (sampler >= 16) {
        return this->Fail("sampler %u out of range", sampler);
    }

    if (this->IsVertex()) {
        return this->Fail("vertex texture fetch is not supported");
    }

    SamplerKind kind = this->m_variant.Kind(sampler);
    result = this->Scratch();

    if (control == TEXLD_PROJECT) {
        uint32_t projected = this->Scratch();
        W().Op(GxDxbc::OP_DIV, { Dst(projected), coord, Replicate(coord, 3) });
        coord = Src(projected);
    }

    if (kind == SAMPLERKIND_DEPTH) {
        // A shadow comparison of the coordinate's z with the stored depth, filtered by the sampler
        W().Op(GxDxbc::OP_SAMPLE_C_LZ, { Dst(result, 0x1), coord, Texture(sampler).Swz("xxxx"), Sampler(sampler), Select(coord, 2) });
        W().Op(GxDxbc::OP_MOV, { Dst(result), R(result).Swz("xxxx") });
        return true;
    }

    if (mode == 1) {
        W().Op(GxDxbc::OP_SAMPLE_L, { Dst(result), coord, Texture(sampler).Swz("xyzw"), Sampler(sampler), Select(*extra, 3) });
    } else if (mode == 2) {
        W().Op(GxDxbc::OP_SAMPLE_D, { Dst(result), coord, Texture(sampler).Swz("xyzw"), Sampler(sampler), extra[0], extra[1] });
    } else if (control == TEXLD_BIAS) {
        W().Op(GxDxbc::OP_SAMPLE_B, { Dst(result), coord, Texture(sampler).Swz("xyzw"), Sampler(sampler), Select(coord, 3) });
    } else {
        W().Op(GxDxbc::OP_SAMPLE, { Dst(result), coord, Texture(sampler).Swz("xyzw"), Sampler(sampler) });
    }

    return true;
}

// Direct3D 9 multiplies 0 by infinity to 0, so infinite results of rcp, rsq and log become the largest
// finite float, which gives the same products everywhere else
void Translator::Finite(uint32_t reg, uint32_t mask) {
    W().Op(GxDxbc::OP_MAX, { Dst(reg, mask), Src(reg), Splat(-FLT_MAX) });
    W().Op(GxDxbc::OP_MIN, { Dst(reg, mask), Src(reg), Splat(FLT_MAX) });
}

// The comparison of ifc, breakc and setp as a mask in result.x
bool Translator::Compare(uint32_t comparison, const Operand& a, const Operand& b, uint32_t& result) {
    result = this->Scratch();
    Operand x = Select(a, 0);
    Operand y = Select(b, 0);

    switch (comparison) {
        case CMP_GT: W().Op(GxDxbc::OP_LT, { Dst(result, 0x1), y, x }); return true;
        case CMP_EQ: W().Op(GxDxbc::OP_EQ, { Dst(result, 0x1), x, y }); return true;
        case CMP_GE: W().Op(GxDxbc::OP_GE, { Dst(result, 0x1), x, y }); return true;
        case CMP_LT: W().Op(GxDxbc::OP_LT, { Dst(result, 0x1), x, y }); return true;
        case CMP_NE: W().Op(GxDxbc::OP_NE, { Dst(result, 0x1), x, y }); return true;
        case CMP_LE: W().Op(GxDxbc::OP_GE, { Dst(result, 0x1), y, x }); return true;
        default: return this->Fail("unknown comparison %u", comparison);
    }
}

bool Translator::EmitArithmetic(const Instruction& instruction, std::vector<PendingWrite>& writes) {
    const std::vector<SourceOperand>& sources = instruction.m_sources;
    Operand s[4];

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

    if (!instruction.m_hasDest) {
        return this->Fail("%s without a destination", OpcodeName(instruction.m_opcode));
    }

    uint32_t S = this->Scratch();

    switch (instruction.m_opcode) {
        case OP_MOV:
            if (!need(1)) return false;
            if (this->IsVertex() && instruction.m_dest.m_type == REG_ADDR) {
                // vs_1_1 loads the address register with mov, which truncates toward negative infinity
                uint32_t mask = instruction.m_dest.m_writeMask ? instruction.m_dest.m_writeMask : 0xF;
                W().Op(GxDxbc::OP_ROUND_NI, { Dst(S), s[0] });
                W().Op(GxDxbc::OP_FTOI, { Dst(this->m_regA0, mask), Src(S) });
                return true;
            }
            W().Op(GxDxbc::OP_MOV, { Dst(S), s[0] });
            break;

        case OP_MOVA: {
            if (!need(1)) return false;
            uint32_t mask = instruction.m_dest.m_writeMask ? instruction.m_dest.m_writeMask : 0xF;
            W().Op(GxDxbc::OP_ROUND_NE, { Dst(S), s[0] });
            W().Op(GxDxbc::OP_FTOI, { Dst(this->m_regA0, mask), Src(S) });
            return true;
        }

        case OP_ADD: if (!need(2)) return false; W().Op(GxDxbc::OP_ADD, { Dst(S), s[0], s[1] }); break;
        case OP_SUB: if (!need(2)) return false; W().Op(GxDxbc::OP_ADD, { Dst(S), s[0], Negated(s[1]) }); break;
        case OP_MUL: if (!need(2)) return false; W().Op(GxDxbc::OP_MUL, { Dst(S), s[0], s[1] }); break;
        case OP_MAD: if (!need(3)) return false; W().Op(GxDxbc::OP_MAD, { Dst(S), s[0], s[1], s[2] }); break;
        case OP_RCP: if (!need(1)) return false; W().Op(GxDxbc::OP_DIV, { Dst(S), Splat(1.0f), Replicate(s[0], 3) }); Finite(S, 0xF); break;
        case OP_RSQ: if (!need(1)) return false; W().Op(GxDxbc::OP_RSQ, { Dst(S), Absolute(Replicate(s[0], 3)) }); Finite(S, 0xF); break;
        case OP_EXP:
        case OP_EXPP: if (!need(1)) return false; W().Op(GxDxbc::OP_EXP, { Dst(S), Replicate(s[0], 3) }); break;
        case OP_LOG:
        case OP_LOGP: if (!need(1)) return false; W().Op(GxDxbc::OP_LOG, { Dst(S), Absolute(Replicate(s[0], 3)) }); Finite(S, 0xF); break;
        case OP_DP3: if (!need(2)) return false; W().Op(GxDxbc::OP_DP3, { Dst(S), s[0], s[1] }); break;
        case OP_DP4: if (!need(2)) return false; W().Op(GxDxbc::OP_DP4, { Dst(S), s[0], s[1] }); break;
        case OP_MIN: if (!need(2)) return false; W().Op(GxDxbc::OP_MIN, { Dst(S), s[0], s[1] }); break;
        case OP_MAX: if (!need(2)) return false; W().Op(GxDxbc::OP_MAX, { Dst(S), s[0], s[1] }); break;
        case OP_FRC: if (!need(1)) return false; W().Op(GxDxbc::OP_FRC, { Dst(S), s[0] }); break;
        case OP_ABS: if (!need(1)) return false; W().Op(GxDxbc::OP_MOV, { Dst(S), Absolute(s[0]) }); break;
        case OP_DSX: if (!need(1)) return false; W().Op(GxDxbc::OP_DERIV_RTX, { Dst(S), s[0] }); break;
        case OP_DSY: if (!need(1)) return false; W().Op(GxDxbc::OP_DERIV_RTY, { Dst(S), s[0] }); break;

        case OP_SLT:
        case OP_SGE:
            if (!need(2)) return false;
            W().Op(instruction.m_opcode == OP_SLT ? GxDxbc::OP_LT : GxDxbc::OP_GE, { Dst(S), s[0], s[1] });
            W().Op(GxDxbc::OP_AND, { Dst(S), Src(S), ImmU(0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u) });
            break;

        case OP_LIT: {
            if (!need(1)) return false;
            uint32_t T = this->Scratch();
            W().Op(GxDxbc::OP_MOV, { Dst(S), ImmF(1.0f, 0.0f, 0.0f, 1.0f) });
            W().Op(GxDxbc::OP_LT, { Dst(T, 0x1), ImmF(0.0f), Select(s[0], 0) });
            W().Op(GxDxbc::OP_LT, { Dst(T, 0x2), ImmF(0.0f), Select(s[0], 1) });
            W().Op(GxDxbc::OP_AND, { Dst(T, 0x2), R(T).Sel(1), R(T).Sel(0) });
            W().Op(GxDxbc::OP_MOVC, { Dst(S, 0x2), R(T).Sel(0), Select(s[0], 0), ImmF(0.0f) });
            W().Op(GxDxbc::OP_MAX, { Dst(T, 0x4), Select(s[0], 3), ImmF(-127.9961f) });
            W().Op(GxDxbc::OP_MIN, { Dst(T, 0x4), R(T).Sel(2), ImmF(127.9961f) });
            W().Op(GxDxbc::OP_LOG, { Dst(T, 0x8), Absolute(Select(s[0], 1)) });
            Finite(T, 0x8);
            W().Op(GxDxbc::OP_MUL, { Dst(T, 0x4), R(T).Sel(2), R(T).Sel(3) });
            W().Op(GxDxbc::OP_EXP, { Dst(T, 0x4), R(T).Sel(2) });
            W().Op(GxDxbc::OP_MOVC, { Dst(S, 0x4), R(T).Sel(1), R(T).Sel(2), ImmF(0.0f) });
            break;
        }

        case OP_DST:
            if (!need(2)) return false;
            W().Op(GxDxbc::OP_MOV, { Dst(S, 0x1), ImmF(1.0f) });
            W().Op(GxDxbc::OP_MUL, { Dst(S, 0x2), Select(s[0], 1), Select(s[1], 1) });
            W().Op(GxDxbc::OP_MOV, { Dst(S, 0x4), Select(s[0], 2) });
            W().Op(GxDxbc::OP_MOV, { Dst(S, 0x8), Select(s[1], 3) });
            break;

        case OP_LRP: {
            if (!need(3)) return false;
            uint32_t T = this->Scratch();
            W().Op(GxDxbc::OP_ADD, { Dst(T), s[1], Negated(s[2]) });
            W().Op(GxDxbc::OP_MAD, { Dst(S), s[0], Src(T), s[2] });
            break;
        }

        case OP_POW: {
            if (!need(2)) return false;
            W().Op(GxDxbc::OP_LOG, { Dst(S, 0x1), Absolute(Select(s[0], 0)) });
            Finite(S, 0x1);
            W().Op(GxDxbc::OP_MUL, { Dst(S, 0x1), R(S).Sel(0), Select(s[1], 0) });
            W().Op(GxDxbc::OP_EXP, { Dst(S), R(S).Swz("xxxx") });
            break;
        }

        case OP_CRS: {
            if (!need(2)) return false;
            uint32_t T = this->Scratch();
            W().Op(GxDxbc::OP_MUL, { Dst(T, 0x7), Swizzled(s[0], 2, 0, 1, 1), Swizzled(s[1], 1, 2, 0, 0) });
            W().Op(GxDxbc::OP_MAD, { Dst(S, 0x7), Swizzled(s[0], 1, 2, 0, 0), Swizzled(s[1], 2, 0, 1, 1), R(T).Swz("xyzw").Neg() });
            W().Op(GxDxbc::OP_MOV, { Dst(S, 0x8), ImmF(0.0f) });
            break;
        }

        case OP_SGN: {
            if (!need(1)) return false;
            uint32_t T = this->Scratch();
            W().Op(GxDxbc::OP_LT, { Dst(S), Splat(0.0f), s[0] });
            W().Op(GxDxbc::OP_LT, { Dst(T), s[0], Splat(0.0f) });
            W().Op(GxDxbc::OP_AND, { Dst(S), Src(S), ImmU(0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u) });
            W().Op(GxDxbc::OP_AND, { Dst(T), Src(T), ImmU(0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u) });
            W().Op(GxDxbc::OP_ADD, { Dst(S), Src(S), Src(T).Neg() });
            break;
        }

        case OP_NRM: {
            if (!need(1)) return false;
            uint32_t T = this->Scratch();
            W().Op(GxDxbc::OP_DP3, { Dst(T, 0x1), s[0], s[0] });
            W().Op(GxDxbc::OP_RSQ, { Dst(T, 0x2), R(T).Sel(0) });
            W().Op(GxDxbc::OP_MUL, { Dst(S), s[0], R(T).Swz("yyyy") });
            W().Op(GxDxbc::OP_EQ, { Dst(T, 0x1), R(T).Sel(0), ImmF(0.0f) });
            W().Op(GxDxbc::OP_MOVC, { Dst(S), R(T).Swz("xxxx"), Splat(0.0f), Src(S) });
            break;
        }

        case OP_SINCOS:
            if (!need(1)) return false;
            W().Op(GxDxbc::OP_SINCOS, { Dst(S, 0x2), Dst(S, 0x1), Replicate(s[0], 0) });
            W().Op(GxDxbc::OP_MOV, { Dst(S, 0xC), Splat(0.0f) });
            break;

        case OP_CMP: {
            if (!need(3)) return false;
            uint32_t T = this->Scratch();
            W().Op(GxDxbc::OP_GE, { Dst(T), s[0], Splat(0.0f) });
            W().Op(GxDxbc::OP_MOVC, { Dst(S), Src(T), s[1], s[2] });
            break;
        }

        case OP_DP2ADD: {
            if (!need(3)) return false;
            uint32_t T = this->Scratch();
            W().Op(GxDxbc::OP_DP2, { Dst(T, 0x1), s[0], s[1] });
            W().Op(GxDxbc::OP_ADD, { Dst(S), R(T).Swz("xxxx"), Replicate(s[2], 0) });
            break;
        }

        case OP_CND: {
            if (!need(3)) return false;
            uint32_t T = this->Scratch();
            // ps_1_1 to ps_1_3 compare r0.a
            W().Op(GxDxbc::OP_LT, { Dst(T), Splat(0.5f), this->IsPs11() ? Replicate(s[0], 3) : s[0] });
            W().Op(GxDxbc::OP_MOVC, { Dst(S), Src(T), s[1], s[2] });
            break;
        }

        case OP_M4x4:
        case OP_M4x3:
        case OP_M3x4:
        case OP_M3x3:
        case OP_M3x2: {
            if (!need(1)) return false;
            if (sources.size() < 2) return this->Fail("matrix instruction without a matrix");

            uint32_t rows = instruction.m_opcode == OP_M4x4 || instruction.m_opcode == OP_M3x4 ? 4 : instruction.m_opcode == OP_M3x2 ? 2 : 3;
            bool four = instruction.m_opcode == OP_M4x4 || instruction.m_opcode == OP_M4x3;
            W().Op(GxDxbc::OP_MOV, { Dst(S), Splat(0.0f) });

            for (uint32_t row = 0; row < rows; row++) {
                SourceOperand matrix = sources[1];
                matrix.m_index += row;
                matrix.m_swizzle[0] = 0;
                matrix.m_swizzle[1] = 1;
                matrix.m_swizzle[2] = 2;
                matrix.m_swizzle[3] = 3;

                Operand rowValue;
                if (!this->Source(matrix, rowValue)) {
                    return false;
                }

                W().Op(four ? GxDxbc::OP_DP4 : GxDxbc::OP_DP3, { Dst(S, 1u << row), s[0], rowValue });
            }
            break;
        }

        default:
            return this->Fail("unsupported instruction %s", OpcodeName(instruction.m_opcode));
    }

    writes.push_back({ instruction.m_dest, S });
    return true;
}

bool Translator::EmitInstruction(size_t& index) {
    const Instruction& instruction = this->m_program.m_instructions[index];
    std::vector<PendingWrite> writes;
    this->m_scratchNext = this->m_scratchBase;

    switch (instruction.m_opcode) {
        case OP_NOP:
        case OP_DCL:
        case OP_DEF:
        case OP_DEFI:
        case OP_DEFB:
        case OP_PHASE:
            return true;

        case OP_REP:
        case OP_LOOP: {
            bool loop = instruction.m_opcode == OP_LOOP;
            std::array<int32_t, 4> counter;

            if (instruction.m_sources.size() < (loop ? 2u : 1u) || !this->IntConstant(instruction.m_sources[loop ? 1 : 0], counter)) {
                return this->m_error.empty() ? this->Fail("%s without a count", OpcodeName(instruction.m_opcode)) : false;
            }

            if (this->m_loopOrdinal >= this->m_loopRegisters.size()) {
                return this->Fail("loop registers exhausted");
            }

            Flow flow;
            flow.m_kind = loop ? Flow::Kind_Loop : Flow::Kind_Rep;
            flow.m_counter = this->m_loopRegisters[this->m_loopOrdinal++];
            flow.m_aL = flow.m_counter;
            flow.m_step = counter[2];

            int32_t count = std::min(std::max(counter[0], 0), 255);
            W().Op(GxDxbc::OP_MOV, { Dst(flow.m_counter, 0x1), ImmI(0) });

            if (loop) {
                W().Op(GxDxbc::OP_MOV, { Dst(flow.m_aL, 0x2), ImmI(counter[1]) });
            }

            W().Op0(GxDxbc::OP_LOOP);
            uint32_t test = this->Scratch();
            W().Op(GxDxbc::OP_IGE, { Dst(test, 0x1), R(flow.m_counter).Sel(0), ImmI(count) });
            W().OpTest(GxDxbc::OP_BREAKC, R(test).Sel(0), true);
            this->m_flow.push_back(flow);
            return true;
        }

        case OP_ENDREP:
        case OP_ENDLOOP: {
            if (this->m_flow.empty() || this->m_flow.back().m_kind == Flow::Kind_If) {
                return this->Fail("unbalanced %s", OpcodeName(instruction.m_opcode));
            }

            Flow flow = this->m_flow.back();
            this->m_flow.pop_back();
            W().Op(GxDxbc::OP_IADD, { Dst(flow.m_counter, 0x1), R(flow.m_counter).Sel(0), ImmI(1) });

            if (flow.m_kind == Flow::Kind_Loop) {
                W().Op(GxDxbc::OP_IADD, { Dst(flow.m_counter, 0x2), R(flow.m_counter).Sel(1), ImmI(flow.m_step) });
            }

            W().Op0(GxDxbc::OP_ENDLOOP);
            return true;
        }

        case OP_BREAK:
            W().Op0(GxDxbc::OP_BREAK);
            return true;

        case OP_BREAKC:
        case OP_IFC: {
            Operand a;
            Operand b;
            uint32_t result;

            if (instruction.m_sources.size() < 2 || !this->Source(instruction.m_sources[0], a) || !this->Source(instruction.m_sources[1], b)) {
                return this->m_error.empty() ? this->Fail("malformed %s", OpcodeName(instruction.m_opcode)) : false;
            }

            if (!this->Compare(instruction.m_control, a, b, result)) {
                return false;
            }

            if (instruction.m_opcode == OP_BREAKC) {
                W().OpTest(GxDxbc::OP_BREAKC, R(result).Sel(0), true);
            } else {
                W().OpTest(GxDxbc::OP_IF, R(result).Sel(0), true);
                this->m_flow.push_back({ Flow::Kind_If, 0, 0, 0 });
            }
            return true;
        }

        case OP_IF: {
            bool condition;
            if (instruction.m_sources.empty() || !this->BoolConstant(instruction.m_sources[0], condition)) {
                return this->m_error.empty() ? this->Fail("if without a condition") : false;
            }
            W().OpTest(GxDxbc::OP_IF, ImmU(condition ? 1u : 0u), true);
            this->m_flow.push_back({ Flow::Kind_If, 0, 0, 0 });
            return true;
        }

        case OP_ELSE:
            if (this->m_flow.empty() || this->m_flow.back().m_kind != Flow::Kind_If) {
                return this->Fail("else outside of if");
            }
            W().Op0(GxDxbc::OP_ELSE);
            return true;

        case OP_ENDIF:
            if (this->m_flow.empty() || this->m_flow.back().m_kind != Flow::Kind_If) {
                return this->Fail("unbalanced endif");
            }
            this->m_flow.pop_back();
            W().Op0(GxDxbc::OP_ENDIF);
            return true;

        case OP_TEXKILL: {
            const DestOperand& dest = instruction.m_dest;
            Operand value;

            if (this->Major() == 1 && dest.m_type == REG_TEXTURE) {
                // ps_1_x test the texture coordinates of the stage
                if (dest.m_index >= 8) {
                    return this->Fail("texkill of t%u", dest.m_index);
                }
                value = this->Varying(VARYING_TEXCOORD0 + dest.m_index);
            } else {
                uint32_t reg;
                bool scalar;
                if (!this->DestRegister(dest, reg, scalar)) {
                    return false;
                }
                value = Src(reg);
            }

            // Shader model 1 tests xyz, later models all four components
            uint32_t T = this->Scratch();
            W().Op(GxDxbc::OP_LT, { Dst(T), value, Splat(0.0f) });
            W().Op(GxDxbc::OP_OR, { Dst(T, 0x1), R(T).Sel(0), R(T).Sel(1) });
            W().Op(GxDxbc::OP_OR, { Dst(T, 0x1), R(T).Sel(0), R(T).Sel(2) });
            if (this->Major() >= 2) {
                W().Op(GxDxbc::OP_OR, { Dst(T, 0x1), R(T).Sel(0), R(T).Sel(3) });
            }
            W().OpTest(GxDxbc::OP_DISCARD, R(T).Sel(0), true);
            return true;
        }

        case OP_TEX: {
            if (!this->IsPixel()) {
                return this->Fail("texld in a vertex shader");
            }

            uint32_t result;

            if (this->IsPs11()) {
                uint32_t stage = instruction.m_dest.m_index;
                if (stage >= 8) {
                    return this->Fail("tex t%u", stage);
                }
                Operand coord = this->Varying(VARYING_TEXCOORD0 + stage);
                uint32_t count = stage < 4 ? (this->m_variant.m_projected >> (stage * 3)) & 0x7 : 0;
                if (count >= 2) {
                    uint32_t projected = this->Scratch();
                    W().Op(GxDxbc::OP_DIV, { Dst(projected), coord, Replicate(coord, count - 1) });
                    coord = Src(projected);
                }
                if (!this->Sample(stage, coord, TEXLD_NORMAL, 0, nullptr, result)) {
                    return false;
                }
            } else if (this->Major() == 1) {
                // ps_1_4: texld r#, t#|r# samples the stage of the destination register
                Operand coord;
                if (instruction.m_sources.empty() || !this->Source(instruction.m_sources[0], coord)) {
                    return this->m_error.empty() ? this->Fail("texld without coordinates") : false;
                }
                if (!this->Sample(instruction.m_dest.m_index, coord, TEXLD_NORMAL, 0, nullptr, result)) {
                    return false;
                }
            } else {
                if (instruction.m_sources.size() < 2 || instruction.m_sources[1].m_type != REG_SAMPLER) {
                    return this->Fail("malformed texld");
                }
                Operand coord;
                if (!this->Source(instruction.m_sources[0], coord)) {
                    return false;
                }
                const SourceOperand& sampler = instruction.m_sources[1];
                if (!this->Sample(sampler.m_index, coord, instruction.m_control, 0, nullptr, result)) {
                    return false;
                }
                // The sampler register's swizzle selects result components
                uint32_t swizzled = this->Scratch();
                W().Op(GxDxbc::OP_MOV, { Dst(swizzled), R(result).Swz(sampler.m_swizzle[0], sampler.m_swizzle[1], sampler.m_swizzle[2], sampler.m_swizzle[3]) });
                result = swizzled;
            }

            writes.push_back({ instruction.m_dest, result });
            break;
        }

        case OP_TEXLDL:
        case OP_TEXLDD: {
            bool lod = instruction.m_opcode == OP_TEXLDL;
            size_t count = lod ? 2 : 4;
            if (instruction.m_sources.size() < count || instruction.m_sources[1].m_type != REG_SAMPLER) {
                return this->Fail("malformed %s", OpcodeName(instruction.m_opcode));
            }

            Operand coord;
            if (!this->Source(instruction.m_sources[0], coord)) {
                return false;
            }

            uint32_t sampler = instruction.m_sources[1].m_index;
            uint32_t result;

            if (lod) {
                if (!this->Sample(sampler, coord, TEXLD_NORMAL, 1, &coord, result)) {
                    return false;
                }
            } else {
                Operand gradients[2];
                if (!this->Source(instruction.m_sources[2], gradients[0]) || !this->Source(instruction.m_sources[3], gradients[1])) {
                    return false;
                }
                if (!this->Sample(sampler, coord, TEXLD_NORMAL, 2, gradients, result)) {
                    return false;
                }
            }

            const SourceOperand& samplerOperand = instruction.m_sources[1];
            uint32_t swizzled = this->Scratch();
            W().Op(GxDxbc::OP_MOV, { Dst(swizzled), R(result).Swz(samplerOperand.m_swizzle[0], samplerOperand.m_swizzle[1], samplerOperand.m_swizzle[2], samplerOperand.m_swizzle[3]) });
            writes.push_back({ instruction.m_dest, swizzled });
            break;
        }

        case OP_TEXCOORD: {
            uint32_t value = this->Scratch();

            if (this->IsPs11()) {
                // texcoord t#: the stage's coordinates clamped to [0, 1], with w = 1
                if (instruction.m_dest.m_index >= 8) {
                    return this->Fail("texcoord t%u", instruction.m_dest.m_index);
                }
                W().Op(GxDxbc::OP_MOV, { Dst(value, 0x7), this->Varying(VARYING_TEXCOORD0 + instruction.m_dest.m_index) }, true);
                W().Op(GxDxbc::OP_MOV, { Dst(value, 0x8), ImmF(1.0f) });
            } else {
                // ps_1_4 texcrd r#, t#
                Operand source;
                if (instruction.m_sources.empty() || !this->Source(instruction.m_sources[0], source)) {
                    return this->m_error.empty() ? this->Fail("texcrd without a source") : false;
                }
                W().Op(GxDxbc::OP_MOV, { Dst(value), source });
            }

            writes.push_back({ instruction.m_dest, value });
            break;
        }

        case OP_TEXBEM:
        case OP_TEXBEML: {
            if (!this->IsPs11() || instruction.m_sources.empty()) {
                return this->Fail("malformed %s", OpcodeName(instruction.m_opcode));
            }

            uint32_t stage = instruction.m_dest.m_index;
            if (stage >= 8) {
                return this->Fail("%s t%u", OpcodeName(instruction.m_opcode), stage);
            }

            Operand bump;
            if (!this->Source(instruction.m_sources[0], bump)) {
                return false;
            }

            // The bump matrix of the stage transforms the source's du, dv
            Operand matrix = CB(CB_STATE, PSREG_BUMP_MAT + stage).Swz("xyzw");
            uint32_t offset = this->Scratch();
            uint32_t coord = this->Scratch();
            W().Op(GxDxbc::OP_MUL, { Dst(offset, 0x3), Replicate(bump, 0), matrix.Swz("xyxx") });
            W().Op(GxDxbc::OP_MAD, { Dst(offset, 0x3), Replicate(bump, 1), matrix.Swz("zwzz"), Src(offset) });
            W().Op(GxDxbc::OP_MOV, { Dst(coord), this->Varying(VARYING_TEXCOORD0 + stage) });
            W().Op(GxDxbc::OP_ADD, { Dst(coord, 0x3), Src(coord), Src(offset) });

            uint32_t result;
            if (!this->Sample(stage, Src(coord), TEXLD_NORMAL, 0, nullptr, result)) {
                return false;
            }

            if (instruction.m_opcode == OP_TEXBEML) {
                Operand luminance = CB(CB_STATE, PSREG_BUMP_LUM + stage);
                W().Op(GxDxbc::OP_MAD, { Dst(offset, 0x1), Select(bump, 2), luminance.Sel(0), luminance.Sel(1) }, true);
                W().Op(GxDxbc::OP_MUL, { Dst(result, 0x7), Src(result), R(offset).Swz("xxxx") });
            }

            writes.push_back({ instruction.m_dest, result });
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

    return this->FlushWrites(writes);
}

bool Translator::EmitBody() {
    for (size_t i = 0; i < this->m_program.m_instructions.size(); i++) {
        if (!this->EmitInstruction(i)) {
            return false;
        }
    }

    if (!this->m_flow.empty()) {
        return this->Fail("unterminated flow control");
    }

    return true;
}

void Translator::DeclareTemps() {
    W().DclTemps(this->m_tempCount);
}

bool Translator::EmitVertex(const VertexInputSignature& inputs, std::vector<uint8_t>& code) {
    Writer writer(Writer::STAGE_VERTEX);
    this->m_writer = &writer;

    this->m_regA0 = this->Persistent();
    this->m_regPos = this->Persistent();
    this->m_regFog = this->Persistent();
    this->m_regPts = this->Persistent();
    this->m_regColor[0] = this->Persistent();
    this->m_regColor[1] = this->Persistent();

    for (uint32_t i = 0; i < 8; i++) {
        this->m_regTexcoordOut[i] = this->Persistent();
    }

    if (this->Major() >= 3) {
        for (const auto& [reg, declaration] : this->m_outputs) {
            if (reg < 16) {
                this->m_regOutput[reg] = this->Persistent();
            }
        }
    }

    for (uint32_t i = 0; i < this->m_loopCount; i++) {
        this->m_loopRegisters.push_back(this->Persistent());
    }

    // Inputs: float attributes are read directly, integer ones converted once
    std::vector<uint32_t> conversions;

    for (uint32_t reg = 0; reg < 16; reg++) {
        this->m_inputOperands[reg] = ImmF(0.0f, 0.0f, 0.0f, 1.0f);

        if (!(this->m_inputMask & (1u << reg))) {
            continue;
        }

        auto declaration = this->m_inputs.find(reg);
        int32_t attribute = declaration != this->m_inputs.end() ? AttributeIndex(declaration->second.m_usage, declaration->second.m_usageIndex) : -1;
        uint8_t attributeClass = attribute >= 0 ? inputs.m_class[attribute] : ATTRIBCLASS_NONE;

        if (attributeClass == ATTRIBCLASS_NONE) {
            continue;
        }

        this->m_attributeMask |= 1u << attribute;

        if (attributeClass == ATTRIBCLASS_FLOAT) {
            this->m_inputOperands[reg] = V(attribute).Swz("xyzw");
        } else {
            uint32_t converted = this->Persistent();
            W().Op(attributeClass == ATTRIBCLASS_UINT ? GxDxbc::OP_UTOF : GxDxbc::OP_ITOF, { Dst(converted), V(attribute).Swz("xyzw") });
            this->m_inputOperands[reg] = Src(converted);
        }
    }

    this->m_scratchBase = this->m_nextPersistent;
    this->m_scratchNext = this->m_scratchBase;
    this->m_tempCount = this->m_scratchBase;

    for (uint32_t i = 0; i < 32; i++) {
        if (this->m_tempMask & (1u << i)) {
            W().Op(GxDxbc::OP_MOV, { Dst(i), Splat(0.0f) });
        }
    }

    if (this->m_usesAddress) {
        W().Op(GxDxbc::OP_MOV, { Dst(this->m_regA0), ImmU(0, 0, 0, 0) });
    }

    W().Op(GxDxbc::OP_MOV, { Dst(this->m_regPos), Splat(0.0f) });
    W().Op(GxDxbc::OP_MOV, { Dst(this->m_regFog), Splat(0.0f) });
    W().Op(GxDxbc::OP_MOV, { Dst(this->m_regPts), CB(CB_STATE, VSREG_POINT_SIZE).Swz("xxxx") });

    if (this->Major() < 3) {
        // Unwritten colors default to opaque white for the diffuse color and zero otherwise
        W().Op(GxDxbc::OP_MOV, { Dst(this->m_regColor[0]), Splat((this->m_writtenColorMask & 1) ? 0.0f : 1.0f) });
        W().Op(GxDxbc::OP_MOV, { Dst(this->m_regColor[1]), Splat(0.0f) });

        for (uint32_t i = 0; i < 8; i++) {
            W().Op(GxDxbc::OP_MOV, { Dst(this->m_regTexcoordOut[i]), Splat(0.0f) });
        }
    } else {
        for (const auto& [reg, declaration] : this->m_outputs) {
            if (reg < 16) {
                W().Op(GxDxbc::OP_MOV, { Dst(this->m_regOutput[reg]), Splat(0.0f) });
            }
        }
    }

    if (!this->EmitBody()) {
        return false;
    }

    // The varyings
    uint32_t position = this->m_regPos;

    if (this->Major() < 3) {
        W().Op(GxDxbc::OP_MOV, { O(VARYING_COLOR0).Mask(0xF), Src(this->m_regColor[0]) }, true);
        W().Op(GxDxbc::OP_MOV, { O(VARYING_COLOR1).Mask(0xF), Src(this->m_regColor[1]) }, true);

        for (uint32_t i = 0; i < 8; i++) {
            W().Op(GxDxbc::OP_MOV, { O(VARYING_TEXCOORD0 + i).Mask(0xF), Src(this->m_regTexcoordOut[i]) });
        }

        W().Op(GxDxbc::OP_MOV, { O(VARYING_FOG).Mask(0x1), R(this->m_regFog).Sel(0) });
    } else {
        bool color0 = false;
        bool written[VARYING_COUNT] = {};
        bool fog = false;

        for (const auto& [reg, declaration] : this->m_outputs) {
            if (reg >= 16) {
                continue;
            }

            uint32_t source = this->m_regOutput[reg];

            switch (declaration.m_usage) {
                case USAGE_POSITION:
                    if (declaration.m_usageIndex == 0) {
                        position = source;
                    }
                    break;
                case USAGE_FOG:
                    W().Op(GxDxbc::OP_MOV, { O(VARYING_FOG).Mask(0x1), R(source).Sel(0) });
                    fog = true;
                    break;
                case USAGE_PSIZE:
                    W().Op(GxDxbc::OP_MOV, { Dst(this->m_regPts, 0x1), R(source).Sel(0) });
                    break;
                case USAGE_COLOR:
                    if (declaration.m_usageIndex < 2) {
                        W().Op(GxDxbc::OP_MOV, { O(VARYING_COLOR0 + declaration.m_usageIndex).Mask(0xF), Src(source) });
                        written[VARYING_COLOR0 + declaration.m_usageIndex] = true;
                        color0 = color0 || declaration.m_usageIndex == 0;
                    }
                    break;
                case USAGE_TEXCOORD:
                    if (declaration.m_usageIndex < 8) {
                        W().Op(GxDxbc::OP_MOV, { O(VARYING_TEXCOORD0 + declaration.m_usageIndex).Mask(0xF), Src(source) });
                        written[VARYING_TEXCOORD0 + declaration.m_usageIndex] = true;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!color0) {
            W().Op(GxDxbc::OP_MOV, { O(VARYING_COLOR0).Mask(0xF), Splat(1.0f) });
        }

        for (uint32_t i = VARYING_COLOR1; i < VARYING_FOG; i++) {
            if (!written[i]) {
                W().Op(GxDxbc::OP_MOV, { O(i).Mask(0xF), Splat(0.0f) });
            }
        }

        if (!fog) {
            W().Op(GxDxbc::OP_MOV, { O(VARYING_FOG).Mask(0x1), ImmF(0.0f) });
        }
    }

    // User clip planes are in clip space for programmable vertex processing
    EmitVertexEpilogue(writer, position, position, this->m_regPts);

    writer.DclGlobalFlags(1);

    if (this->m_usesConst) {
        uint32_t count = this->m_constRelative ? this->FloatConstantLimit() : std::min(this->m_maxConst, this->FloatConstantLimit());
        if (count) {
            writer.DclConstantBuffer(CB_FLOAT_CONSTANTS, count, this->m_constRelative);
        }
    }

    writer.DclConstantBuffer(CB_STATE, VSREG_COUNT, false);

    for (uint32_t i = 0; i < ATTRIB_COUNT; i++) {
        if (this->m_attributeMask & (1u << i)) {
            writer.DclInput(i, 0xF);
        }
    }

    DeclareVaryingOutputs(writer);
    this->DeclareTemps();

    VertexInputSignatureElements(inputs, this->m_attributeMask, writer.m_inputs);
    VaryingSignature(writer.m_outputs, false);
    code = writer.Finish();
    this->m_writer = nullptr;
    return true;
}

bool Translator::EmitPixel(const PixelShaderVariant& variant, std::vector<uint8_t>& code) {
    Writer writer(Writer::STAGE_PIXEL);
    this->m_writer = &writer;
    this->m_variant = variant;

    this->m_regColorOut = this->Persistent();
    this->m_regDepthOut = this->Persistent();
    this->m_regVPos = this->Persistent();
    this->m_regVFace = this->Persistent();

    for (uint32_t i = 0; i < 8; i++) {
        this->m_regTexture[i] = this->Persistent();
    }

    for (uint32_t i = 0; i < this->m_loopCount; i++) {
        this->m_loopRegisters.push_back(this->Persistent());
    }

    this->m_scratchBase = this->m_nextPersistent;
    this->m_scratchNext = this->m_scratchBase;
    this->m_tempCount = this->m_scratchBase;

    const uint32_t faceRegister = VARYING_COUNT;

    if (this->m_usesPosition) {
        this->m_usedVaryings |= 1u << VARYING_POSITION;
        W().Op(GxDxbc::OP_ADD, { Dst(this->m_regVPos, 0x3), V(VARYING_POSITION).Swz("xyzw"), Splat(-0.5f) });
        W().Op(GxDxbc::OP_MOV, { Dst(this->m_regVPos, 0xC), Splat(0.0f) });
    }

    if (this->m_usesFace) {
        W().Op(GxDxbc::OP_MOVC, { Dst(this->m_regVFace), V(faceRegister).Swz("xxxx"), Splat(1.0f), Splat(-1.0f) });
    }

    if (this->IsPs11()) {
        // Texture registers start out undefined and are loaded by texture instructions
        for (uint32_t i = 0; i < 8; i++) {
            if (this->m_textureMask & (1u << i)) {
                W().Op(GxDxbc::OP_MOV, { Dst(this->m_regTexture[i]), Splat(0.0f) });
            }
        }
    }

    for (uint32_t i = 0; i < 32; i++) {
        if (this->m_tempMask & (1u << i)) {
            W().Op(GxDxbc::OP_MOV, { Dst(i), Splat(0.0f) });
        }
    }

    if (this->Major() >= 2) {
        W().Op(GxDxbc::OP_MOV, { Dst(this->m_regColorOut), Splat(0.0f) });
    }

    if (this->m_writesDepth) {
        W().Op(GxDxbc::OP_MOV, { Dst(this->m_regDepthOut), Splat(0.0f) });
    }

    if (!this->EmitBody()) {
        return false;
    }

    uint32_t color = this->Major() == 1 ? 0 : this->m_regColorOut;
    uint32_t scratch = this->m_scratchBase;
    this->m_tempCount = std::max(this->m_tempCount, scratch + 2);

    // Fixed-function fog follows pixel shaders before shader model 3
    bool fog = this->Major() < 3;

    if (fog) {
        this->m_usedVaryings |= 1u << VARYING_FOG;
    }

    EmitPixelEpilogue(writer, color, scratch, fog, variant.m_alphaTest != 0);
    W().Op(GxDxbc::OP_MOV, { O(0).Mask(0xF), Src(color) });

    if (this->m_writesDepth) {
        W().Op(GxDxbc::OP_MOV, { OutputDepth(), R(this->m_regDepthOut).Sel(0) }, true);
    }

    writer.DclGlobalFlags(1);

    if (this->m_usesConst) {
        uint32_t count = this->m_constRelative ? this->FloatConstantLimit() : std::min(this->m_maxConst, this->FloatConstantLimit());
        if (count) {
            writer.DclConstantBuffer(CB_FLOAT_CONSTANTS, count, this->m_constRelative);
        }
    }

    writer.DclConstantBuffer(CB_STATE, PSREG_COUNT, false);

    for (uint32_t i = 0; i < 16; i++) {
        if (this->m_samplerMask & (1u << i)) {
            writer.DclSampler(i, variant.Kind(i) == SAMPLERKIND_DEPTH);
        }
    }

    for (uint32_t i = 0; i < 16; i++) {
        if (!(this->m_samplerMask & (1u << i))) {
            continue;
        }

        switch (variant.Kind(i)) {
            case SAMPLERKIND_CUBE: writer.DclResource(i, DIM_TEXTURECUBE); break;
            case SAMPLERKIND_VOLUME: writer.DclResource(i, DIM_TEXTURE3D); break;
            default: writer.DclResource(i, DIM_TEXTURE2D); break;
        }
    }

    if (this->m_usedVaryings & (1u << VARYING_POSITION)) {
        writer.DclInputPsSiv(VARYING_POSITION, 0x3, INTERP_LINEAR_NOPERSPECTIVE, SV_POSITION);
    }

    for (uint32_t i = VARYING_COLOR0; i <= VARYING_FOG; i++) {
        if (this->m_usedVaryings & (1u << i)) {
            writer.DclInputPs(i, i == VARYING_FOG ? 0x1 : 0xF, INTERP_LINEAR);
        }
    }

    if (this->m_usesFace) {
        writer.DclInputPsSgv(faceRegister, 0x1, SV_IS_FRONT_FACE);
    }

    writer.DclOutput(0, 0xF);

    if (this->m_writesDepth) {
        writer.DclOutputDepth();
    }

    this->DeclareTemps();

    VaryingSignature(writer.m_inputs, true);

    for (SignatureElement& element : writer.m_inputs) {
        if (element.m_register < 32 && (this->m_usedVaryings & (1u << element.m_register))) {
            element.m_readWriteMask = element.m_register == VARYING_POSITION ? 0x3 : element.m_mask;
        }
    }

    if (this->m_usesFace) {
        writer.m_inputs.push_back({ "SV_IsFrontFace", 0, SV_IS_FRONT_FACE, COMPONENT_UINT, faceRegister, 0x1, 0x1 });
    }

    writer.m_outputs.push_back({ "SV_Target", 0, SV_NONE, COMPONENT_FLOAT, 0, 0xF, 0 });

    if (this->m_writesDepth) {
        writer.m_outputs.push_back({ "SV_Depth", 0, SV_NONE, COMPONENT_FLOAT, 0xFFFFFFFFu, 0x1, 0xE });
    }

    code = writer.Finish();
    this->m_writer = nullptr;
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

bool TranslateVertexShader(const ShaderProgram& program, const VertexInputSignature& inputs, std::vector<uint8_t>& code, std::string& error) {
    if (program.m_stage != STAGE_VERTEX) {
        error = "not a vertex shader";
        return false;
    }

    Translator translator(program, error);
    return translator.Analyze() && translator.EmitVertex(inputs, code);
}

bool TranslatePixelShader(const ShaderProgram& program, const PixelShaderVariant& variant, std::vector<uint8_t>& code, std::string& error) {
    if (program.m_stage != STAGE_PIXEL) {
        error = "not a pixel shader";
        return false;
    }

    Translator translator(program, error);
    return translator.Analyze() && translator.EmitPixel(variant, code);
}

}
