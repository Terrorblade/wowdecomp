// Direct3D 9 shader bytecode: the token format of vs_1_1 to vs_3_0 and ps_1_1 to ps_3_0, as the
// client's .bls files carry it, parsed into instructions the Metal and D3D11 translators work from.
//
// This file has no Metal or engine dependencies so tools can use it on their own.

#ifndef ENGINE_SOURCE_GX_SHADER_SHADERBYTECODE_H
#define ENGINE_SOURCE_GX_SHADER_SHADERBYTECODE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define GXSHADER_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define GXSHADER_PRINTF(fmt, args)
#endif

namespace GxShader {

enum ShaderStage {
    STAGE_VERTEX,
    STAGE_PIXEL,
};

// D3DSHADER_PARAM_REGISTER_TYPE
enum RegisterType {
    REG_TEMP = 0,
    REG_INPUT = 1,
    REG_CONST = 2,
    REG_ADDR = 3,           // vertex shaders; REG_TEXTURE in pixel shaders
    REG_TEXTURE = 3,
    REG_RASTOUT = 4,
    REG_ATTROUT = 5,
    REG_TEXCRDOUT = 6,      // REG_OUTPUT in vs_3_0
    REG_OUTPUT = 6,
    REG_CONSTINT = 7,
    REG_COLOROUT = 8,
    REG_DEPTHOUT = 9,
    REG_SAMPLER = 10,
    REG_CONST2 = 11,
    REG_CONST3 = 12,
    REG_CONST4 = 13,
    REG_CONSTBOOL = 14,
    REG_LOOP = 15,
    REG_TEMPFLOAT16 = 16,
    REG_MISCTYPE = 17,
    REG_LABEL = 18,
    REG_PREDICATE = 19,
};

// D3DSHADER_INSTRUCTION_OPCODE_TYPE
enum Opcode {
    OP_NOP = 0,
    OP_MOV = 1,
    OP_ADD = 2,
    OP_SUB = 3,
    OP_MAD = 4,
    OP_MUL = 5,
    OP_RCP = 6,
    OP_RSQ = 7,
    OP_DP3 = 8,
    OP_DP4 = 9,
    OP_MIN = 10,
    OP_MAX = 11,
    OP_SLT = 12,
    OP_SGE = 13,
    OP_EXP = 14,
    OP_LOG = 15,
    OP_LIT = 16,
    OP_DST = 17,
    OP_LRP = 18,
    OP_FRC = 19,
    OP_M4x4 = 20,
    OP_M4x3 = 21,
    OP_M3x4 = 22,
    OP_M3x3 = 23,
    OP_M3x2 = 24,
    OP_CALL = 25,
    OP_CALLNZ = 26,
    OP_LOOP = 27,
    OP_RET = 28,
    OP_ENDLOOP = 29,
    OP_LABEL = 30,
    OP_DCL = 31,
    OP_POW = 32,
    OP_CRS = 33,
    OP_SGN = 34,
    OP_ABS = 35,
    OP_NRM = 36,
    OP_SINCOS = 37,
    OP_REP = 38,
    OP_ENDREP = 39,
    OP_IF = 40,
    OP_IFC = 41,
    OP_ELSE = 42,
    OP_ENDIF = 43,
    OP_BREAK = 44,
    OP_BREAKC = 45,
    OP_MOVA = 46,
    OP_DEFB = 47,
    OP_DEFI = 48,
    OP_TEXCOORD = 64,
    OP_TEXKILL = 65,
    OP_TEX = 66,
    OP_TEXBEM = 67,
    OP_TEXBEML = 68,
    OP_TEXREG2AR = 69,
    OP_TEXREG2GB = 70,
    OP_TEXM3x2PAD = 71,
    OP_TEXM3x2TEX = 72,
    OP_TEXM3x3PAD = 73,
    OP_TEXM3x3TEX = 74,
    OP_RESERVED0 = 75,
    OP_TEXM3x3SPEC = 76,
    OP_TEXM3x3VSPEC = 77,
    OP_EXPP = 78,
    OP_LOGP = 79,
    OP_CND = 80,
    OP_DEF = 81,
    OP_TEXREG2RGB = 82,
    OP_TEXDP3TEX = 83,
    OP_TEXM3x2DEPTH = 84,
    OP_TEXDP3 = 85,
    OP_TEXM3x3 = 86,
    OP_TEXDEPTH = 87,
    OP_CMP = 88,
    OP_BEM = 89,
    OP_DP2ADD = 90,
    OP_DSX = 91,
    OP_DSY = 92,
    OP_TEXLDD = 93,
    OP_SETP = 94,
    OP_TEXLDL = 95,
    OP_BREAKP = 96,
    OP_PHASE = 0xFFFD,
    OP_COMMENT = 0xFFFE,
    OP_END = 0xFFFF,
};

// Source modifiers (D3DSHADER_PARAM_SRCMOD_TYPE)
enum SourceModifier {
    SRCMOD_NONE = 0,
    SRCMOD_NEG = 1,
    SRCMOD_BIAS = 2,
    SRCMOD_BIASNEG = 3,
    SRCMOD_SIGN = 4,
    SRCMOD_SIGNNEG = 5,
    SRCMOD_COMP = 6,
    SRCMOD_X2 = 7,
    SRCMOD_X2NEG = 8,
    SRCMOD_DZ = 9,
    SRCMOD_DW = 10,
    SRCMOD_ABS = 11,
    SRCMOD_ABSNEG = 12,
    SRCMOD_NOT = 13,
};

// dcl usages (D3DDECLUSAGE)
enum DeclUsage {
    USAGE_POSITION = 0,
    USAGE_BLENDWEIGHT = 1,
    USAGE_BLENDINDICES = 2,
    USAGE_NORMAL = 3,
    USAGE_PSIZE = 4,
    USAGE_TEXCOORD = 5,
    USAGE_TANGENT = 6,
    USAGE_BINORMAL = 7,
    USAGE_TESSFACTOR = 8,
    USAGE_POSITIONT = 9,
    USAGE_COLOR = 10,
    USAGE_FOG = 11,
    USAGE_DEPTH = 12,
    USAGE_SAMPLE = 13,
};

// Sampler texture types (D3DSAMPLER_TEXTURE_TYPE >> 27)
enum SamplerType {
    SAMPLER_UNKNOWN = 0,
    SAMPLER_2D = 2,
    SAMPLER_CUBE = 3,
    SAMPLER_VOLUME = 4,
};

// Comparison of ifc, breakc and setp (instruction control bits)
enum Comparison {
    CMP_GT = 1,
    CMP_EQ = 2,
    CMP_GE = 3,
    CMP_LT = 4,
    CMP_NE = 5,
    CMP_LE = 6,
};

// Texture load control of texld (ps_2_0 and up)
enum TexldControl {
    TEXLD_NORMAL = 0,
    TEXLD_PROJECT = 1,
    TEXLD_BIAS = 2,
};

struct RelativeAddress {
    uint32_t m_type = 0;        // REG_ADDR (a0) or REG_LOOP (aL)
    uint32_t m_index = 0;
    uint32_t m_component = 0;   // the swizzled component of the address register used
};

struct SourceOperand {
    uint32_t m_type = 0;
    uint32_t m_index = 0;
    uint8_t m_swizzle[4] = { 0, 1, 2, 3 };
    uint32_t m_modifier = SRCMOD_NONE;
    bool m_relative = false;
    RelativeAddress m_address;
};

struct DestOperand {
    uint32_t m_type = 0;
    uint32_t m_index = 0;
    uint32_t m_writeMask = 0xF;
    bool m_saturate = false;
    bool m_partialPrecision = false;
    bool m_centroid = false;
    int32_t m_shift = 0;        // ps_1_x result shift: -1 for _d2 ... 1 for _x2 etc.
    bool m_relative = false;
    RelativeAddress m_address;
};

struct Instruction {
    uint32_t m_opcode = OP_NOP;
    uint32_t m_control = 0;     // opcode-specific bits 16-23
    bool m_coissue = false;
    bool m_predicated = false;
    SourceOperand m_predicate;
    bool m_hasDest = false;
    DestOperand m_dest;
    std::vector<SourceOperand> m_sources;

    // dcl
    uint32_t m_usage = 0;
    uint32_t m_usageIndex = 0;
    uint32_t m_samplerType = SAMPLER_UNKNOWN;

    // def, defi, defb
    union {
        float m_float[4];
        int32_t m_int[4];
        uint32_t m_bool;
    } m_immediate = {};
};

struct ShaderProgram {
    ShaderStage m_stage = STAGE_VERTEX;
    uint32_t m_major = 0;
    uint32_t m_minor = 0;
    std::vector<Instruction> m_instructions;
};

// Parses a token stream that starts with the version token and ends with the end token. Returns
// false with a description on malformed input.
bool ParseShader(const uint32_t* tokens, size_t tokenCount, ShaderProgram& program, std::string& error);

const char* OpcodeName(uint32_t opcode);

}

#endif
