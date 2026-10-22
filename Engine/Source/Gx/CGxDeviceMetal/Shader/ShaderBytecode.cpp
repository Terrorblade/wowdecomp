#include "ShaderBytecode.h"

#include <cstdarg>
#include <cstdio>

namespace GxMetal {

namespace {

struct OpcodeInfo {
    const char* m_name;
    int8_t m_dest;      // 1 if the instruction writes a destination register
    int8_t m_sources;   // source count for shader model 1 streams, which carry no length field
};

const OpcodeInfo* LookupOpcode(uint32_t opcode) {
    static const OpcodeInfo s_arithmetic[] = {
        { "nop", 0, 0 },     { "mov", 1, 1 },     { "add", 1, 2 },     { "sub", 1, 2 },
        { "mad", 1, 3 },     { "mul", 1, 2 },     { "rcp", 1, 1 },     { "rsq", 1, 1 },
        { "dp3", 1, 2 },     { "dp4", 1, 2 },     { "min", 1, 2 },     { "max", 1, 2 },
        { "slt", 1, 2 },     { "sge", 1, 2 },     { "exp", 1, 1 },     { "log", 1, 1 },
        { "lit", 1, 1 },     { "dst", 1, 2 },     { "lrp", 1, 3 },     { "frc", 1, 1 },
        { "m4x4", 1, 2 },    { "m4x3", 1, 2 },    { "m3x4", 1, 2 },    { "m3x3", 1, 2 },
        { "m3x2", 1, 2 },    { "call", 0, 1 },    { "callnz", 0, 2 },  { "loop", 0, 2 },
        { "ret", 0, 0 },     { "endloop", 0, 0 }, { "label", 0, 1 },   { "dcl", 0, 0 },
        { "pow", 1, 2 },     { "crs", 1, 2 },     { "sgn", 1, 3 },     { "abs", 1, 1 },
        { "nrm", 1, 1 },     { "sincos", 1, 3 },  { "rep", 0, 1 },     { "endrep", 0, 0 },
        { "if", 0, 1 },      { "ifc", 0, 2 },     { "else", 0, 0 },    { "endif", 0, 0 },
        { "break", 0, 0 },   { "breakc", 0, 2 },  { "mova", 1, 1 },    { "defb", 0, 0 },
        { "defi", 0, 0 },
    };
    static const OpcodeInfo s_texture[] = {
        { "texcoord", 1, 0 },     { "texkill", 1, 0 },      { "tex", 1, 0 },
        { "texbem", 1, 1 },       { "texbeml", 1, 1 },      { "texreg2ar", 1, 1 },
        { "texreg2gb", 1, 1 },    { "texm3x2pad", 1, 1 },   { "texm3x2tex", 1, 1 },
        { "texm3x3pad", 1, 1 },   { "texm3x3tex", 1, 1 },   { "reserved0", 0, 0 },
        { "texm3x3spec", 1, 2 },  { "texm3x3vspec", 1, 1 }, { "expp", 1, 1 },
        { "logp", 1, 1 },         { "cnd", 1, 3 },          { "def", 0, 0 },
        { "texreg2rgb", 1, 1 },   { "texdp3tex", 1, 1 },    { "texm3x2depth", 1, 1 },
        { "texdp3", 1, 1 },       { "texm3x3", 1, 1 },      { "texdepth", 1, 0 },
        { "cmp", 1, 3 },          { "bem", 1, 2 },          { "dp2add", 1, 3 },
        { "dsx", 1, 1 },          { "dsy", 1, 1 },          { "texldd", 1, 4 },
        { "setp", 1, 2 },         { "texldl", 1, 2 },       { "breakp", 0, 1 },
    };
    static const OpcodeInfo s_phase = { "phase", 0, 0 };

    if (opcode < sizeof(s_arithmetic) / sizeof(s_arithmetic[0])) {
        return &s_arithmetic[opcode];
    }
    if (opcode >= OP_TEXCOORD && opcode - OP_TEXCOORD < sizeof(s_texture) / sizeof(s_texture[0])) {
        return &s_texture[opcode - OP_TEXCOORD];
    }
    if (opcode == OP_PHASE) {
        return &s_phase;
    }
    return nullptr;
}

uint32_t RegisterTypeOf(uint32_t token) {
    return ((token >> 28) & 0x7) | ((token >> 8) & 0x18);
}

class Reader {
public:
    Reader(const uint32_t* tokens, size_t count) : m_tokens(tokens), m_count(count) {}

    bool AtEnd() const { return m_position >= m_count; }
    size_t Position() const { return m_position; }
    void Seek(size_t position) { m_position = position; }

    bool Next(uint32_t& token) {
        if (m_position >= m_count) {
            return false;
        }
        token = m_tokens[m_position++];
        return true;
    }

private:
    const uint32_t* m_tokens;
    size_t m_count;
    size_t m_position = 0;
};

class Parser {
public:
    Parser(const uint32_t* tokens, size_t count, ShaderProgram& program, std::string& error)
        : m_reader(tokens, count), m_program(program), m_error(error) {}

    bool Run();

private:
    bool Fail(const char* format, ...) __attribute__((format(printf, 2, 3)));

    // Shader model 2 and up mark relative addressing with an extra address token. vs_1_1 has a
    // single address register and no extra token: the index is always a0.x.
    bool HasAddressToken() const { return m_program.m_major >= 2; }

    bool ReadAddress(RelativeAddress& address);
    bool ReadSource(SourceOperand& operand);
    bool ReadDest(DestOperand& operand);
    bool ReadInstruction(uint32_t token);

    Reader m_reader;
    ShaderProgram& m_program;
    std::string& m_error;
};

bool Parser::Fail(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    char position[64];
    snprintf(position, sizeof(position), " (token %zu)", m_reader.Position());

    m_error = buffer;
    m_error += position;
    return false;
}

bool Parser::ReadAddress(RelativeAddress& address) {
    if (!HasAddressToken()) {
        address.m_type = REG_ADDR;
        address.m_index = 0;
        address.m_component = 0;
        return true;
    }

    uint32_t token;
    if (!m_reader.Next(token)) {
        return Fail("truncated relative address");
    }

    address.m_type = RegisterTypeOf(token);
    address.m_index = token & 0x7FF;
    // The address token carries a replicate swizzle; any lane names the component.
    address.m_component = (token >> 16) & 0x3;
    return true;
}

bool Parser::ReadSource(SourceOperand& operand) {
    uint32_t token;
    if (!m_reader.Next(token)) {
        return Fail("truncated source operand");
    }

    operand.m_type = RegisterTypeOf(token);
    operand.m_index = token & 0x7FF;
    for (uint32_t i = 0; i < 4; i++) {
        operand.m_swizzle[i] = (token >> (16 + i * 2)) & 0x3;
    }
    operand.m_modifier = (token >> 24) & 0xF;
    operand.m_relative = (token & (1u << 13)) != 0;

    if (operand.m_relative) {
        return ReadAddress(operand.m_address);
    }
    return true;
}

bool Parser::ReadDest(DestOperand& operand) {
    uint32_t token;
    if (!m_reader.Next(token)) {
        return Fail("truncated destination operand");
    }

    operand.m_type = RegisterTypeOf(token);
    operand.m_index = token & 0x7FF;
    operand.m_writeMask = (token >> 16) & 0xF;

    uint32_t modifiers = (token >> 20) & 0xF;
    operand.m_saturate = (modifiers & 0x1) != 0;
    operand.m_partialPrecision = (modifiers & 0x2) != 0;
    operand.m_centroid = (modifiers & 0x4) != 0;

    // Signed 4-bit shift: 1 = _x2, 2 = _x4, 3 = _x8, 15 = _d2, 14 = _d4, 13 = _d8
    int32_t shift = static_cast<int32_t>((token >> 24) & 0xF);
    operand.m_shift = shift >= 8 ? shift - 16 : shift;

    operand.m_relative = (token & (1u << 13)) != 0;
    if (operand.m_relative) {
        return ReadAddress(operand.m_address);
    }
    return true;
}

bool Parser::ReadInstruction(uint32_t token) {
    Instruction instruction;
    instruction.m_opcode = token & 0xFFFF;
    instruction.m_control = (token >> 16) & 0xFF;
    instruction.m_predicated = (token & (1u << 28)) != 0;
    instruction.m_coissue = (token & (1u << 30)) != 0;

    const OpcodeInfo* info = LookupOpcode(instruction.m_opcode);
    if (!info) {
        return Fail("unknown opcode %u", instruction.m_opcode);
    }

    size_t start = m_reader.Position();
    bool lengthField = m_program.m_major >= 2;
    uint32_t length = (token >> 24) & 0xF;

    switch (instruction.m_opcode) {
        case OP_DCL: {
            uint32_t dcl;
            if (!m_reader.Next(dcl)) {
                return Fail("truncated dcl");
            }
            instruction.m_usage = dcl & 0x1F;
            instruction.m_usageIndex = (dcl >> 16) & 0xF;
            instruction.m_samplerType = (dcl >> 27) & 0xF;
            instruction.m_hasDest = true;
            if (!ReadDest(instruction.m_dest)) {
                return false;
            }
            break;
        }

        case OP_DEF:
        case OP_DEFI:
        case OP_DEFB: {
            instruction.m_hasDest = true;
            if (!ReadDest(instruction.m_dest)) {
                return false;
            }
            uint32_t count = instruction.m_opcode == OP_DEFB ? 1 : 4;
            for (uint32_t i = 0; i < count; i++) {
                uint32_t value;
                if (!m_reader.Next(value)) {
                    return Fail("truncated constant definition");
                }
                instruction.m_immediate.m_int[i] = static_cast<int32_t>(value);
            }
            break;
        }

        default: {
            if (info->m_dest) {
                instruction.m_hasDest = true;
                if (!ReadDest(instruction.m_dest)) {
                    return false;
                }
            }
            if (instruction.m_predicated) {
                if (!ReadSource(instruction.m_predicate)) {
                    return false;
                }
            }

            if (lengthField) {
                // The length field counts every token after the opcode token, address tokens
                // included, which settles the operand count of instructions whose source count
                // depends on the shader model (sincos, tex).
                while (m_reader.Position() - start < length) {
                    SourceOperand source;
                    if (!ReadSource(source)) {
                        return false;
                    }
                    instruction.m_sources.push_back(source);
                }
            } else {
                uint32_t sources = info->m_sources;
                // ps_1_4 texld and texcrd take their coordinate register as a source operand.
                if ((instruction.m_opcode == OP_TEX || instruction.m_opcode == OP_TEXCOORD)
                    && m_program.m_stage == STAGE_PIXEL && m_program.m_major == 1 && m_program.m_minor == 4) {
                    sources = 1;
                }
                for (uint32_t i = 0; i < sources; i++) {
                    SourceOperand source;
                    if (!ReadSource(source)) {
                        return false;
                    }
                    instruction.m_sources.push_back(source);
                }
            }
            break;
        }
    }

    if (lengthField && m_reader.Position() - start != length) {
        return Fail("%s: operand tokens %zu do not match length %u", info->m_name, m_reader.Position() - start, length);
    }

    m_program.m_instructions.push_back(std::move(instruction));
    return true;
}

bool Parser::Run() {
    uint32_t version;
    if (!m_reader.Next(version)) {
        return Fail("empty shader");
    }

    switch (version >> 16) {
        case 0xFFFE:
            m_program.m_stage = STAGE_VERTEX;
            break;
        case 0xFFFF:
            m_program.m_stage = STAGE_PIXEL;
            break;
        default:
            return Fail("bad version token 0x%08X", version);
    }
    m_program.m_major = (version >> 8) & 0xFF;
    m_program.m_minor = version & 0xFF;
    m_program.m_instructions.clear();

    uint32_t token;
    while (m_reader.Next(token)) {
        uint32_t opcode = token & 0xFFFF;

        if (opcode == OP_END) {
            return true;
        }

        if (opcode == OP_COMMENT) {
            uint32_t length = (token >> 16) & 0x7FFF;
            m_reader.Seek(m_reader.Position() + length);
            continue;
        }

        if (!ReadInstruction(token)) {
            return false;
        }
    }

    return Fail("missing end token");
}

}

bool ParseShader(const uint32_t* tokens, size_t tokenCount, ShaderProgram& program, std::string& error) {
    Parser parser(tokens, tokenCount, program, error);
    return parser.Run();
}

const char* OpcodeName(uint32_t opcode) {
    if (opcode == OP_COMMENT) {
        return "comment";
    }
    if (opcode == OP_END) {
        return "end";
    }
    const OpcodeInfo* info = LookupOpcode(opcode);
    return info ? info->m_name : "unknown";
}

}
