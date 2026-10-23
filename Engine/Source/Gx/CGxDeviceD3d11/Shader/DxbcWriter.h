// Writes shader model 4.0 bytecode in a DXBC container, the format ID3D11Device::Create*Shader takes.

#ifndef ENGINE_SOURCE_GX_CGXDEVICED3D11_SHADER_DXBCWRITER_H
#define ENGINE_SOURCE_GX_CGXDEVICED3D11_SHADER_DXBCWRITER_H

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace GxDxbc {

enum Opcode : uint32_t {
    OP_ADD = 0,
    OP_AND = 1,
    OP_BREAK = 2,
    OP_BREAKC = 3,
    OP_DERIV_RTX = 11,
    OP_DERIV_RTY = 12,
    OP_DISCARD = 13,
    OP_DIV = 14,
    OP_DP2 = 15,
    OP_DP3 = 16,
    OP_DP4 = 17,
    OP_ELSE = 18,
    OP_ENDIF = 21,
    OP_ENDLOOP = 22,
    OP_EQ = 24,
    OP_EXP = 25,
    OP_FRC = 26,
    OP_FTOI = 27,
    OP_FTOU = 28,
    OP_GE = 29,
    OP_IADD = 30,
    OP_IF = 31,
    OP_IEQ = 32,
    OP_IGE = 33,
    OP_ILT = 34,
    OP_IMAD = 35,
    OP_IMAX = 36,
    OP_IMIN = 37,
    OP_INE = 39,
    OP_ITOF = 43,
    OP_LOG = 47,
    OP_LOOP = 48,
    OP_LT = 49,
    OP_MAD = 50,
    OP_MIN = 51,
    OP_MAX = 52,
    OP_MOV = 54,
    OP_MOVC = 55,
    OP_MUL = 56,
    OP_NE = 57,
    OP_NOT = 59,
    OP_OR = 60,
    OP_RET = 62,
    OP_ROUND_NE = 64,
    OP_ROUND_NI = 65,
    OP_ROUND_PI = 66,
    OP_ROUND_Z = 67,
    OP_RSQ = 68,
    OP_SAMPLE = 69,
    OP_SAMPLE_C = 70,
    OP_SAMPLE_C_LZ = 71,
    OP_SAMPLE_L = 72,
    OP_SAMPLE_D = 73,
    OP_SAMPLE_B = 74,
    OP_SQRT = 75,
    OP_SINCOS = 77,
    OP_UTOF = 86,
    OP_DCL_RESOURCE = 88,
    OP_DCL_CONSTANT_BUFFER = 89,
    OP_DCL_SAMPLER = 90,
    OP_DCL_INPUT = 95,
    OP_DCL_INPUT_SGV = 96,
    OP_DCL_INPUT_SIV = 97,
    OP_DCL_INPUT_PS = 98,
    OP_DCL_INPUT_PS_SGV = 99,
    OP_DCL_INPUT_PS_SIV = 100,
    OP_DCL_OUTPUT = 101,
    OP_DCL_OUTPUT_SGV = 102,
    OP_DCL_OUTPUT_SIV = 103,
    OP_DCL_TEMPS = 104,
    OP_DCL_GLOBAL_FLAGS = 106,
};

enum OperandType : uint32_t {
    TYPE_TEMP = 0,
    TYPE_INPUT = 1,
    TYPE_OUTPUT = 2,
    TYPE_IMM32 = 4,
    TYPE_SAMPLER = 6,
    TYPE_RESOURCE = 7,
    TYPE_CONSTANT_BUFFER = 8,
    TYPE_OUTPUT_DEPTH = 12,
    TYPE_NULL = 13,
};

// D3D_NAME values of signatures and system value declarations
enum SystemValue : uint32_t {
    SV_NONE = 0,
    SV_POSITION = 1,
    SV_CLIP_DISTANCE = 2,
    SV_IS_FRONT_FACE = 9,
    SV_TARGET = 64,
    SV_DEPTH = 65,
};

enum ComponentType : uint32_t {
    COMPONENT_UINT = 1,
    COMPONENT_SINT = 2,
    COMPONENT_FLOAT = 3,
};

enum ResourceDimension : uint32_t {
    DIM_TEXTURE2D = 3,
    DIM_TEXTURE3D = 5,
    DIM_TEXTURECUBE = 6,
};

enum Interpolation : uint32_t {
    INTERP_CONSTANT = 1,
    INTERP_LINEAR = 2,
    INTERP_LINEAR_CENTROID = 3,
    INTERP_LINEAR_NOPERSPECTIVE = 4,
};

struct Operand {
    enum Mode : uint8_t {
        MODE_MASK = 0,
        MODE_SWIZZLE = 1,
        MODE_SELECT = 2,
    };

    uint32_t m_type = TYPE_TEMP;
    uint32_t m_indexCount = 1;
    uint32_t m_index[2] = {};
    // m_index[m_relativeDim] is added to r[m_relativeRegister].m_relativeComponent when set
    bool m_relative = false;
    uint32_t m_relativeDim = 0;
    uint32_t m_relativeRegister = 0;
    uint8_t m_relativeComponent = 0;
    uint8_t m_components = 4;
    uint8_t m_mode = MODE_SWIZZLE;
    uint8_t m_mask = 0xF;
    uint8_t m_swizzle[4] = { 0, 1, 2, 3 };
    bool m_neg = false;
    bool m_abs = false;
    uint32_t m_immediate[4] = {};

    // A destination: write mask
    Operand Mask(uint32_t mask) const;
    // A source: component order, from a string such as "xyzw" or "x"
    Operand Swz(const char* components) const;
    Operand Swz(uint32_t x, uint32_t y, uint32_t z, uint32_t w) const;
    // A source read as one component (select-1 mode)
    Operand Sel(uint32_t component) const;
    Operand Neg() const;
    Operand Abs() const;
};

Operand R(uint32_t index);
Operand V(uint32_t index);
Operand O(uint32_t index);
Operand CB(uint32_t slot, uint32_t index);
// cb[slot][base + r[reg].component]
Operand CBRelative(uint32_t slot, uint32_t base, uint32_t reg, uint32_t component);
Operand ImmF(float x, float y, float z, float w);
Operand ImmF(float x);
Operand ImmU(uint32_t x, uint32_t y, uint32_t z, uint32_t w);
Operand ImmU(uint32_t x);
Operand ImmI(int32_t x);
Operand Texture(uint32_t slot);
Operand Sampler(uint32_t slot);
Operand OutputDepth();
Operand Null();

struct SignatureElement {
    std::string m_name;
    uint32_t m_semanticIndex = 0;
    uint32_t m_systemValue = SV_NONE;
    uint32_t m_componentType = COMPONENT_FLOAT;
    uint32_t m_register = 0;
    uint8_t m_mask = 0xF;
    uint8_t m_readWriteMask = 0xF;
};

class Writer {
    public:
    enum Stage {
        STAGE_PIXEL = 0,
        STAGE_VERTEX = 1,
    };

    explicit Writer(Stage stage) : m_stage(stage) {}

    // Instruction with operands; saturate applies to the result
    void Op(uint32_t opcode, std::initializer_list<Operand> operands, bool saturate = false);
    // if, breakc, discard: one select-1 operand tested against zero
    void OpTest(uint32_t opcode, const Operand& operand, bool nonZero);
    void Op0(uint32_t opcode);

    void DclGlobalFlags(uint32_t flags);
    void DclConstantBuffer(uint32_t slot, uint32_t size, bool dynamicIndexed);
    void DclSampler(uint32_t slot, bool comparison);
    void DclResource(uint32_t slot, uint32_t dimension);
    void DclInput(uint32_t reg, uint32_t mask);
    void DclInputPs(uint32_t reg, uint32_t mask, uint32_t interpolation);
    void DclInputPsSiv(uint32_t reg, uint32_t mask, uint32_t interpolation, uint32_t systemValue);
    void DclInputPsSgv(uint32_t reg, uint32_t mask, uint32_t systemValue);
    void DclOutput(uint32_t reg, uint32_t mask);
    void DclOutputSiv(uint32_t reg, uint32_t mask, uint32_t systemValue);
    void DclOutputDepth();
    void DclTemps(uint32_t count);

    std::vector<SignatureElement> m_inputs;
    std::vector<SignatureElement> m_outputs;

    // The finished container: the declarations, then the instructions, then ret
    std::vector<uint8_t> Finish() const;

    private:
    Stage m_stage;
    std::vector<uint32_t> m_declarations;
    std::vector<uint32_t> m_code;
};

void EncodeOperand(std::vector<uint32_t>& out, const Operand& operand);

// The checksum of a container whose header is in place, written into bytes 4 to 19
void SignContainer(std::vector<uint8_t>& container);
void ComputeChecksum(const uint8_t* data, size_t size, uint32_t digest[4]);

}

#endif
