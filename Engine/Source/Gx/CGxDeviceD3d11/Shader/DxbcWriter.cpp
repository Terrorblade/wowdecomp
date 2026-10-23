#include "Engine/Source/Gx/CGxDeviceD3d11/Shader/DxbcWriter.h"

#include <cstring>

namespace GxDxbc {

Operand Operand::Mask(uint32_t mask) const {
    Operand operand = *this;
    operand.m_mode = MODE_MASK;
    operand.m_mask = static_cast<uint8_t>(mask & 0xF);
    return operand;
}

Operand Operand::Swz(const char* components) const {
    uint32_t swizzle[4] = { 0, 1, 2, 3 };
    uint32_t count = 0;

    for (; components[count] && count < 4; count++) {
        switch (components[count]) {
            case 'x': case 'r': swizzle[count] = 0; break;
            case 'y': case 'g': swizzle[count] = 1; break;
            case 'z': case 'b': swizzle[count] = 2; break;
            default: swizzle[count] = 3; break;
        }
    }

    for (uint32_t i = count; i < 4 && count; i++) {
        swizzle[i] = swizzle[count - 1];
    }

    return this->Swz(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
}

Operand Operand::Swz(uint32_t x, uint32_t y, uint32_t z, uint32_t w) const {
    Operand operand = *this;
    operand.m_mode = MODE_SWIZZLE;
    uint32_t source[4] = { x, y, z, w };

    // Composes with the swizzle the operand already has
    for (uint32_t i = 0; i < 4; i++) {
        operand.m_swizzle[i] = this->m_swizzle[source[i] & 3];
    }

    return operand;
}

Operand Operand::Sel(uint32_t component) const {
    Operand operand = *this;
    operand.m_mode = MODE_SELECT;
    operand.m_swizzle[0] = this->m_swizzle[component & 3];
    return operand;
}

Operand Operand::Neg() const {
    Operand operand = *this;
    operand.m_neg = !operand.m_neg;
    return operand;
}

Operand Operand::Abs() const {
    Operand operand = *this;
    operand.m_abs = true;
    operand.m_neg = false;
    return operand;
}

namespace {

Operand Register(uint32_t type, uint32_t index) {
    Operand operand;
    operand.m_type = type;
    operand.m_indexCount = 1;
    operand.m_index[0] = index;
    return operand;
}

uint32_t FloatBits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return bits;
}

}

Operand R(uint32_t index) {
    return Register(TYPE_TEMP, index);
}

Operand V(uint32_t index) {
    return Register(TYPE_INPUT, index);
}

Operand O(uint32_t index) {
    return Register(TYPE_OUTPUT, index);
}

Operand CB(uint32_t slot, uint32_t index) {
    Operand operand = Register(TYPE_CONSTANT_BUFFER, slot);
    operand.m_indexCount = 2;
    operand.m_index[1] = index;
    return operand;
}

Operand CBRelative(uint32_t slot, uint32_t base, uint32_t reg, uint32_t component) {
    Operand operand = CB(slot, base);
    operand.m_relative = true;
    operand.m_relativeDim = 1;
    operand.m_relativeRegister = reg;
    operand.m_relativeComponent = static_cast<uint8_t>(component & 3);
    return operand;
}

Operand ImmF(float x, float y, float z, float w) {
    return ImmU(FloatBits(x), FloatBits(y), FloatBits(z), FloatBits(w));
}

Operand ImmF(float x) {
    return ImmU(FloatBits(x));
}

Operand ImmU(uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    Operand operand;
    operand.m_type = TYPE_IMM32;
    operand.m_indexCount = 0;
    operand.m_components = 4;
    operand.m_immediate[0] = x;
    operand.m_immediate[1] = y;
    operand.m_immediate[2] = z;
    operand.m_immediate[3] = w;
    return operand;
}

Operand ImmU(uint32_t x) {
    Operand operand;
    operand.m_type = TYPE_IMM32;
    operand.m_indexCount = 0;
    operand.m_components = 1;
    operand.m_immediate[0] = x;
    return operand;
}

Operand ImmI(int32_t x) {
    return ImmU(static_cast<uint32_t>(x));
}

Operand Texture(uint32_t slot) {
    return Register(TYPE_RESOURCE, slot);
}

Operand Sampler(uint32_t slot) {
    Operand operand = Register(TYPE_SAMPLER, slot);
    operand.m_components = 0;
    return operand;
}

Operand OutputDepth() {
    Operand operand;
    operand.m_type = TYPE_OUTPUT_DEPTH;
    operand.m_indexCount = 0;
    operand.m_components = 1;
    return operand;
}

Operand Null() {
    Operand operand;
    operand.m_type = TYPE_NULL;
    operand.m_indexCount = 0;
    operand.m_components = 0;
    return operand;
}

void EncodeOperand(std::vector<uint32_t>& out, const Operand& operand) {
    uint32_t token = 0;

    if (operand.m_type == TYPE_IMM32) {
        token |= operand.m_components == 1 ? 1 : 2;
    } else if (operand.m_components == 1) {
        token |= 1;
    } else if (operand.m_components == 4) {
        token |= 2;
        token |= static_cast<uint32_t>(operand.m_mode) << 2;

        switch (operand.m_mode) {
            case Operand::MODE_MASK:
                token |= static_cast<uint32_t>(operand.m_mask) << 4;
                break;
            case Operand::MODE_SWIZZLE:
                token |= static_cast<uint32_t>(operand.m_swizzle[0] | (operand.m_swizzle[1] << 2) | (operand.m_swizzle[2] << 4) | (operand.m_swizzle[3] << 6)) << 4;
                break;
            default:
                token |= static_cast<uint32_t>(operand.m_swizzle[0]) << 4;
                break;
        }
    }

    token |= operand.m_type << 12;
    token |= operand.m_indexCount << 20;

    for (uint32_t dim = 0; dim < operand.m_indexCount; dim++) {
        uint32_t representation = operand.m_relative && operand.m_relativeDim == dim ? 3 : 0;
        token |= representation << (22 + dim * 3);
    }

    bool extended = operand.m_neg || operand.m_abs;

    if (extended) {
        token |= 1u << 31;
    }

    out.push_back(token);

    if (extended) {
        uint32_t modifier = (operand.m_neg ? 1 : 0) | (operand.m_abs ? 2 : 0);
        out.push_back(1 | (modifier << 6));
    }

    for (uint32_t dim = 0; dim < operand.m_indexCount; dim++) {
        out.push_back(operand.m_index[dim]);

        if (operand.m_relative && operand.m_relativeDim == dim) {
            EncodeOperand(out, R(operand.m_relativeRegister).Sel(operand.m_relativeComponent));
        }
    }

    if (operand.m_type == TYPE_IMM32) {
        for (uint32_t i = 0; i < operand.m_components; i++) {
            out.push_back(operand.m_immediate[i]);
        }
    }
}

namespace {

void Instruction(std::vector<uint32_t>& out, uint32_t opcode, uint32_t controls, std::initializer_list<Operand> operands, std::initializer_list<uint32_t> extra = {}) {
    std::vector<uint32_t> tokens;

    for (const Operand& operand : operands) {
        EncodeOperand(tokens, operand);
    }

    tokens.insert(tokens.end(), extra.begin(), extra.end());
    out.push_back(opcode | controls | (static_cast<uint32_t>(tokens.size() + 1) << 24));
    out.insert(out.end(), tokens.begin(), tokens.end());
}

}

void Writer::Op(uint32_t opcode, std::initializer_list<Operand> operands, bool saturate) {
    Instruction(this->m_code, opcode, saturate ? (1u << 13) : 0, operands);
}

void Writer::OpTest(uint32_t opcode, const Operand& operand, bool nonZero) {
    Instruction(this->m_code, opcode, nonZero ? (1u << 18) : 0, { operand });
}

void Writer::Op0(uint32_t opcode) {
    this->m_code.push_back(opcode | (1u << 24));
}

void Writer::DclGlobalFlags(uint32_t flags) {
    this->m_declarations.push_back(OP_DCL_GLOBAL_FLAGS | (flags << 11) | (1u << 24));
}

void Writer::DclConstantBuffer(uint32_t slot, uint32_t size, bool dynamicIndexed) {
    Instruction(this->m_declarations, OP_DCL_CONSTANT_BUFFER, dynamicIndexed ? (1u << 11) : 0, { CB(slot, size) });
}

void Writer::DclSampler(uint32_t slot, bool comparison) {
    Instruction(this->m_declarations, OP_DCL_SAMPLER, comparison ? (1u << 11) : 0, { Sampler(slot) });
}

void Writer::DclResource(uint32_t slot, uint32_t dimension) {
    Operand operand = Texture(slot);
    operand.m_components = 0;
    Instruction(this->m_declarations, OP_DCL_RESOURCE, dimension << 11, { operand }, { 0x5555u });
}

void Writer::DclInput(uint32_t reg, uint32_t mask) {
    Instruction(this->m_declarations, OP_DCL_INPUT, 0, { V(reg).Mask(mask) });
}

void Writer::DclInputPs(uint32_t reg, uint32_t mask, uint32_t interpolation) {
    Instruction(this->m_declarations, OP_DCL_INPUT_PS, interpolation << 11, { V(reg).Mask(mask) });
}

void Writer::DclInputPsSiv(uint32_t reg, uint32_t mask, uint32_t interpolation, uint32_t systemValue) {
    Instruction(this->m_declarations, OP_DCL_INPUT_PS_SIV, interpolation << 11, { V(reg).Mask(mask) }, { systemValue });
}

void Writer::DclInputPsSgv(uint32_t reg, uint32_t mask, uint32_t systemValue) {
    Instruction(this->m_declarations, OP_DCL_INPUT_PS_SGV, INTERP_CONSTANT << 11, { V(reg).Mask(mask) }, { systemValue });
}

void Writer::DclOutput(uint32_t reg, uint32_t mask) {
    Instruction(this->m_declarations, OP_DCL_OUTPUT, 0, { O(reg).Mask(mask) });
}

void Writer::DclOutputSiv(uint32_t reg, uint32_t mask, uint32_t systemValue) {
    Instruction(this->m_declarations, OP_DCL_OUTPUT_SIV, 0, { O(reg).Mask(mask) }, { systemValue });
}

void Writer::DclOutputDepth() {
    Instruction(this->m_declarations, OP_DCL_OUTPUT, 0, { OutputDepth() });
}

void Writer::DclTemps(uint32_t count) {
    this->m_declarations.push_back(OP_DCL_TEMPS | (2u << 24));
    this->m_declarations.push_back(count);
}

namespace {

void Append32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 24));
}

void Write32(std::vector<uint8_t>& out, size_t offset, uint32_t value) {
    out[offset + 0] = static_cast<uint8_t>(value);
    out[offset + 1] = static_cast<uint8_t>(value >> 8);
    out[offset + 2] = static_cast<uint8_t>(value >> 16);
    out[offset + 3] = static_cast<uint8_t>(value >> 24);
}

std::vector<uint8_t> SignatureChunk(const std::vector<SignatureElement>& elements) {
    std::vector<uint8_t> chunk;
    Append32(chunk, static_cast<uint32_t>(elements.size()));
    Append32(chunk, 8);

    size_t names = 8 + elements.size() * 24;
    std::vector<uint8_t> strings;

    for (const SignatureElement& element : elements) {
        Append32(chunk, static_cast<uint32_t>(names + strings.size()));
        Append32(chunk, element.m_semanticIndex);
        Append32(chunk, element.m_systemValue);
        Append32(chunk, element.m_componentType);
        Append32(chunk, element.m_register);
        Append32(chunk, static_cast<uint32_t>(element.m_mask) | (static_cast<uint32_t>(element.m_readWriteMask) << 8));
        strings.insert(strings.end(), element.m_name.begin(), element.m_name.end());
        strings.push_back(0);
    }

    while (strings.size() % 4) {
        strings.push_back(0xAB);
    }

    chunk.insert(chunk.end(), strings.begin(), strings.end());
    return chunk;
}

}

std::vector<uint8_t> Writer::Finish() const {
    std::vector<uint32_t> program;
    program.push_back((static_cast<uint32_t>(this->m_stage) << 16) | (4 << 4) | 0);
    program.push_back(0);
    program.insert(program.end(), this->m_declarations.begin(), this->m_declarations.end());
    program.insert(program.end(), this->m_code.begin(), this->m_code.end());
    program.push_back(OP_RET | (1u << 24));
    program[1] = static_cast<uint32_t>(program.size());

    std::vector<uint8_t> shader(program.size() * 4);
    memcpy(shader.data(), program.data(), shader.size());

    struct Chunk {
        uint32_t m_tag;
        std::vector<uint8_t> m_data;
    };

    Chunk chunks[3] = {
        { 0x4E475349, SignatureChunk(this->m_inputs) },   // ISGN
        { 0x4E47534F, SignatureChunk(this->m_outputs) },  // OSGN
        { 0x52444853, shader },                           // SHDR
    };

    const uint32_t chunkCount = 3;
    std::vector<uint8_t> container;
    Append32(container, 0x43425844);  // DXBC

    for (uint32_t i = 0; i < 4; i++) {
        Append32(container, 0);
    }

    Append32(container, 1);
    Append32(container, 0);
    Append32(container, chunkCount);

    size_t offsets = container.size();

    for (uint32_t i = 0; i < chunkCount; i++) {
        Append32(container, 0);
    }

    for (uint32_t i = 0; i < chunkCount; i++) {
        Write32(container, offsets + i * 4, static_cast<uint32_t>(container.size()));
        Append32(container, chunks[i].m_tag);
        Append32(container, static_cast<uint32_t>(chunks[i].m_data.size()));
        container.insert(container.end(), chunks[i].m_data.begin(), chunks[i].m_data.end());
    }

    Write32(container, 24, static_cast<uint32_t>(container.size()));
    SignContainer(container);
    return container;
}

namespace {

struct Md5 {
    uint32_t m_state[4] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u };

    static uint32_t Rotate(uint32_t value, uint32_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    void Transform(const uint8_t* block) {
        static const uint32_t s_k[64] = {
            0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
            0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
            0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
            0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
            0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
            0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
            0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
            0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391,
        };
        static const uint32_t s_shift[64] = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
        };

        uint32_t words[16];

        for (uint32_t i = 0; i < 16; i++) {
            words[i] = block[i * 4] | (block[i * 4 + 1] << 8) | (block[i * 4 + 2] << 16) | (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
        }

        uint32_t a = this->m_state[0];
        uint32_t b = this->m_state[1];
        uint32_t c = this->m_state[2];
        uint32_t d = this->m_state[3];

        for (uint32_t i = 0; i < 64; i++) {
            uint32_t f;
            uint32_t g;

            if (i < 16) {
                f = (b & c) | (~b & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | ~d);
                g = (7 * i) % 16;
            }

            uint32_t next = d;
            d = c;
            c = b;
            b = b + Rotate(a + f + s_k[i] + words[g], s_shift[i]);
            a = next;
        }

        this->m_state[0] += a;
        this->m_state[1] += b;
        this->m_state[2] += c;
        this->m_state[3] += d;
    }
};

}

// MD5 over everything after the checksum, with the length words placed the way DXBC does
void ComputeChecksum(const uint8_t* data, size_t size, uint32_t digest[4]) {
    Md5 md5;
    const size_t skip = 20;
    const uint8_t* bytes = data + skip;
    size_t length = size - skip;
    uint32_t bits = static_cast<uint32_t>(length * 8);
    uint32_t magic = (bits >> 2) | 1;
    size_t full = length - length % 64;
    size_t remainder = length % 64;

    for (size_t offset = 0; offset < full; offset += 64) {
        md5.Transform(bytes + offset);
    }

    uint8_t block[64] = {};

    if (remainder >= 56) {
        memcpy(block, bytes + full, remainder);
        block[remainder] = 0x80;
        md5.Transform(block);

        memset(block, 0, sizeof(block));
        memcpy(block, &bits, 4);
        memcpy(block + 60, &magic, 4);
        md5.Transform(block);
    } else {
        memcpy(block, &bits, 4);
        memcpy(block + 4, bytes + full, remainder);
        block[4 + remainder] = 0x80;
        memcpy(block + 60, &magic, 4);
        md5.Transform(block);
    }

    memcpy(digest, md5.m_state, 16);
}

void SignContainer(std::vector<uint8_t>& container) {
    uint32_t digest[4];
    ComputeChecksum(container.data(), container.size(), digest);
    memcpy(container.data() + 4, digest, 16);
}

}
