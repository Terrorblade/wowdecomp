#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "SComp.h"
#include "Engine/Source/Storm/H/SCritSect.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Storm/H/storm.h"
#include "ThirdParty/bzip2/bzlib.h"
#include "ThirdParty/zlib/zlib.h"

namespace {

class CBitInput {
    public:
    /* 0000 */ const uint32_t* m_currsource;
    /* 0004 */ uint32_t m_rack;
    /* 0008 */ uint32_t m_rackbits;

    CBitInput(const void* source);
    uint32_t InputBit();
    uint32_t InputBits(uint32_t count, uint32_t mask);
    uint32_t PeekBits(uint32_t count, uint32_t mask);
    void RemoveBits(uint32_t count);
};

class CBitOutput {
    public:
    /* 0000 */ uint8_t* m_basedest;
    /* 0004 */ uint32_t m_bytesleft;
    /* 0008 */ uint8_t* m_currdest;
    /* 000C */ uint32_t m_rack;
    /* 0010 */ uint32_t m_rackbits;

    CBitOutput(void* dest, uint32_t destsize);
    uint32_t GetTotalBytes();
    void OutputBits(uint32_t value, uint32_t count);
    void Pad();
};

}

struct HUFFNODE : TSLinkedNode<HUFFNODE> {
    /* 0008 */ int32_t symbol;
    /* 000C */ uint32_t weight;
    /* 0010 */ HUFFNODE* parent;
    /* 0014 */ HUFFNODE* child;
};

struct _CACHEREC {
    /* 0000 */ uint32_t sequence;
    /* 0004 */ uint32_t bits;
    union {
        /* 0008 */ int32_t symbol;
        /* 0008 */ HUFFNODE* nodeptr;
    };
};

struct _IMAHEADER {
    /* 0000 */ uint8_t reserved;
    /* 0001 */ uint8_t roundshift;
};

struct _COMPRESSALGORITHM {
    /* 0000 */ uint32_t id;
    /* 0004 */ void (*func)(void*, uint32_t*, const void*, uint32_t, uint32_t*, uint32_t);
};

struct _DECOMPRESSALGORITHM {
    /* 0000 */ uint32_t id;
    /* 0004 */ int32_t (*func)(void*, uint32_t*, const void*, uint32_t);
};

struct ZLIBBUFFER {
    /* 0000 */ char* curr;
    /* 0004 */ uint32_t size;
    /* 0008 */ char* base;
};

class CHuffman {
    public:
    /* 0000 */ int32_t m_adaptive;
    /* 0004 */ uint32_t m_changesequence;
    /* 0008 */ HUFFNODE m_nodebuffer[515];
    /* 3050 */ TSList<HUFFNODE> m_nodefreelist;
    /* 305C */ TSList<HUFFNODE> m_nodelist;
    /* 3068 */ uint32_t m_nodesused;
    /* 306C */ HUFFNODE* m_symbol[258];

    CHuffman();
    ~CHuffman();
    void AddSymbol(int32_t symbol);
    HUFFNODE* AllocNode(uint32_t linktype);
    void BuildTree(uint8_t hint);
    void IncrementWeight(HUFFNODE* node);
};

class CHuffmanDecoder : public CHuffman {
    public:
    /* 3474 */ _CACHEREC m_cache[128];

    CHuffmanDecoder();
    uint32_t Decompress(void* dest, uint32_t destsize, CBitInput* input);
    int32_t DecodeSymbol(CBitInput* input);
};

class CHuffmanEncoder : public CHuffman {
    public:
    uint32_t Compress(CBitOutput* output, const void* source, uint32_t sourcesize, uint8_t hint);
    void EncodeSymbol(CBitOutput* output, int32_t symbol);
};

// STATIC: 0x00A3A9F0
static const uint8_t s_probability[9][258] = {
    {
        0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
        0x00, 0x00,
    },
    {
        0x54, 0x16, 0x16, 0x0D, 0x0C, 0x08, 0x06, 0x05, 0x06, 0x05, 0x06, 0x03, 0x04, 0x04, 0x03, 0x05,
        0x0E, 0x0B, 0x14, 0x13, 0x13, 0x09, 0x0B, 0x06, 0x05, 0x04, 0x03, 0x02, 0x03, 0x02, 0x02, 0x02,
        0x0D, 0x07, 0x09, 0x06, 0x06, 0x04, 0x03, 0x02, 0x04, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02,
        0x09, 0x06, 0x04, 0x04, 0x04, 0x04, 0x03, 0x02, 0x03, 0x02, 0x02, 0x02, 0x02, 0x03, 0x02, 0x04,
        0x08, 0x03, 0x04, 0x07, 0x09, 0x05, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02,
        0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x02, 0x01, 0x02, 0x02,
        0x06, 0x0A, 0x08, 0x08, 0x06, 0x07, 0x04, 0x03, 0x04, 0x04, 0x02, 0x02, 0x04, 0x02, 0x03, 0x03,
        0x04, 0x03, 0x07, 0x07, 0x09, 0x06, 0x04, 0x03, 0x03, 0x02, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x0A, 0x02, 0x02, 0x03, 0x02, 0x02, 0x01, 0x01, 0x02, 0x02, 0x02, 0x06, 0x03, 0x05, 0x02, 0x03,
        0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x03, 0x01, 0x01, 0x01,
        0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x04, 0x04, 0x04, 0x07, 0x09, 0x08, 0x0C, 0x02,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x03,
        0x04, 0x01, 0x02, 0x04, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01,
        0x04, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x01, 0x01, 0x02, 0x02, 0x02, 0x06, 0x4B,
        0x00, 0x00,
    },
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x27, 0x00, 0x00, 0x23, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x01, 0x01, 0x06, 0x0E, 0x10, 0x04,
        0x06, 0x08, 0x05, 0x04, 0x04, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03, 0x01, 0x01, 0x02, 0x01, 0x01,
        0x01, 0x04, 0x02, 0x04, 0x02, 0x02, 0x02, 0x01, 0x01, 0x04, 0x01, 0x01, 0x02, 0x03, 0x03, 0x02,
        0x03, 0x01, 0x03, 0x06, 0x04, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x02, 0x01, 0x01,
        0x01, 0x29, 0x07, 0x16, 0x12, 0x40, 0x0A, 0x0A, 0x11, 0x25, 0x01, 0x03, 0x17, 0x10, 0x26, 0x2A,
        0x10, 0x01, 0x23, 0x23, 0x2F, 0x10, 0x06, 0x07, 0x02, 0x09, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    },
    {
        0xFF, 0x0B, 0x07, 0x05, 0x0B, 0x02, 0x02, 0x02, 0x06, 0x02, 0x02, 0x01, 0x04, 0x02, 0x01, 0x03,
        0x09, 0x01, 0x01, 0x01, 0x03, 0x04, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01,
        0x05, 0x01, 0x01, 0x01, 0x0D, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x01, 0x01, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x01,
        0x0A, 0x04, 0x02, 0x01, 0x06, 0x03, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03, 0x01, 0x01, 0x01,
        0x05, 0x02, 0x03, 0x04, 0x03, 0x03, 0x03, 0x02, 0x01, 0x01, 0x01, 0x02, 0x01, 0x02, 0x03, 0x03,
        0x01, 0x03, 0x01, 0x01, 0x02, 0x05, 0x01, 0x01, 0x04, 0x03, 0x05, 0x01, 0x03, 0x01, 0x03, 0x03,
        0x02, 0x01, 0x04, 0x03, 0x0A, 0x06, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x02, 0x01, 0x0A, 0x02, 0x05, 0x01, 0x01, 0x02, 0x07, 0x02, 0x17, 0x01, 0x05, 0x01, 0x01,
        0x0E, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x06, 0x02, 0x01, 0x04, 0x05, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x07, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x11,
        0x00, 0x00,
    },
    {
        0xFF, 0xFB, 0x98, 0x9A, 0x84, 0x85, 0x63, 0x64, 0x3E, 0x3E, 0x22, 0x22, 0x13, 0x13, 0x18, 0x17,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    },
    {
        0xFF, 0xF1, 0x9D, 0x9E, 0x9A, 0x9B, 0x9A, 0x97, 0x93, 0x93, 0x8C, 0x8E, 0x86, 0x88, 0x80, 0x82,
        0x7C, 0x7C, 0x72, 0x73, 0x69, 0x6B, 0x5F, 0x60, 0x55, 0x56, 0x4A, 0x4B, 0x40, 0x41, 0x37, 0x37,
        0x2F, 0x2F, 0x27, 0x27, 0x21, 0x21, 0x1B, 0x1C, 0x17, 0x17, 0x13, 0x13, 0x10, 0x10, 0x0D, 0x0D,
        0x0B, 0x0B, 0x09, 0x09, 0x08, 0x08, 0x07, 0x07, 0x06, 0x05, 0x05, 0x04, 0x04, 0x04, 0x19, 0x18,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    },
    {
        0xC3, 0xCB, 0xF5, 0x41, 0xFF, 0x7B, 0xF7, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xBF, 0xCC, 0xF2, 0x40, 0xFD, 0x7C, 0xF7, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x7A, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    },
    {
        0xC3, 0xD9, 0xEF, 0x3D, 0xF9, 0x7C, 0xE9, 0x1E, 0xFD, 0xAB, 0xF1, 0x2C, 0xFC, 0x5B, 0xFE, 0x17,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xBD, 0xD9, 0xEC, 0x3D, 0xF5, 0x7D, 0xE8, 0x1D, 0xFB, 0xAE, 0xF0, 0x2C, 0xFB, 0x5C, 0xFF, 0x18,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    },
    {
        0xBA, 0xC5, 0xDA, 0x33, 0xE3, 0x6D, 0xD8, 0x18, 0xE5, 0x94, 0xDA, 0x23, 0xDF, 0x4A, 0xD1, 0x10,
        0xEE, 0xAF, 0xE4, 0x2C, 0xEA, 0x5A, 0xDE, 0x15, 0xF4, 0x87, 0xE9, 0x21, 0xF6, 0x43, 0xFC, 0x12,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xB0, 0xC7, 0xD8, 0x33, 0xE3, 0x6B, 0xD6, 0x18, 0xE7, 0x95, 0xD8, 0x23, 0xDB, 0x49, 0xD0, 0x11,
        0xE9, 0xB2, 0xE2, 0x2B, 0xE8, 0x5C, 0xDD, 0x15, 0xF1, 0x87, 0xE7, 0x20, 0xF7, 0x44, 0xFF, 0x13,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x5F, 0x9E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    },
};

// STATIC: 0x00A3B308
static const int32_t s_ima_adjtable[32] = {
    -1, 0, -1, 4, -1, 2, -1, 6, -1, 1, -1, 5, -1, 3, -1, 7,
    -1, 1, -1, 5, -1, 3, -1, 7, -1, 2, -1, 4, -1, 6, -1, 8,
};

// STATIC: 0x00A3B388
static const uint32_t s_ima_steptable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

// STATIC: 0x00CB7464
static void* s_DecompressBuffer;

// STATIC: 0x00CB746C
static CCritSect s_decompCrit;

// STATIC: 0x00A3B4EC
static const _COMPRESSALGORITHM s_compressalgorithm[6] = {
    { MPQ_COMPRESSION_ADPCM_MONO, ImaAdpcmMonoCompress },
    { MPQ_COMPRESSION_ADPCM_STEREO, ImaAdpcmStereoCompress },
    { MPQ_COMPRESSION_SPARSE, RLEZeroCompress },
    { MPQ_COMPRESSION_HUFFMANN, HuffmanCompress },
    { MPQ_COMPRESSION_ZLIB, ZlibCompress },
    { MPQ_COMPRESSION_BZIP2, Bzip2Compress },
};

// STATIC: 0x00A3B51C
static const _DECOMPRESSALGORITHM s_decompressalgorithm[6] = {
    { MPQ_COMPRESSION_ADPCM_MONO, ImaAdpcmMonoDecompress },
    { MPQ_COMPRESSION_ADPCM_STEREO, ImaAdpcmStereoDecompress },
    { MPQ_COMPRESSION_SPARSE, RLEZeroDecompress },
    { MPQ_COMPRESSION_HUFFMANN, HuffmanDecompress },
    { MPQ_COMPRESSION_ZLIB, ZlibDecompress },
    { MPQ_COMPRESSION_BZIP2, Bzip2Decompress },
};

namespace {

// INLINED
CBitInput::CBitInput(const void* source) {
    this->m_currsource = static_cast<const uint32_t*>(source);
    this->m_rack = *this->m_currsource;
    this->m_currsource++;
    this->m_rackbits = 32;
}

// INLINED
uint32_t CBitInput::InputBit() {
    uint32_t result = this->m_rack & 1;
    this->m_rack >>= 1;

    if (!--this->m_rackbits) {
        this->m_rack = *this->m_currsource++;
        this->m_rackbits = 32;
    }

    return result;
}

// INLINED
uint32_t CBitInput::InputBits(uint32_t count, uint32_t mask) {
    uint32_t result = this->PeekBits(count, mask);
    this->RemoveBits(count);

    return result;
}

// INLINED
uint32_t CBitInput::PeekBits(uint32_t count, uint32_t mask) {
    if (this->m_rackbits <= count) {
        this->m_rack |= *reinterpret_cast<const uint16_t*>(this->m_currsource) << this->m_rackbits;
        this->m_rackbits += 16;
        this->m_currsource = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(this->m_currsource) + 2);
    }

    return mask & this->m_rack;
}

// INLINED
void CBitInput::RemoveBits(uint32_t count) {
    this->m_rack >>= count;
    this->m_rackbits -= count;
}

// INLINED
CBitOutput::CBitOutput(void* dest, uint32_t destsize) {
    this->m_basedest = static_cast<uint8_t*>(dest);
    this->m_bytesleft = destsize;
    this->m_currdest = static_cast<uint8_t*>(dest);
    this->m_rack = 0;
    this->m_rackbits = 0;
}

// INLINED
uint32_t CBitOutput::GetTotalBytes() {
    return static_cast<uint32_t>(this->m_currdest - this->m_basedest);
}

// VERIFY: 0d765a9c manual
// OFFSET: 0x00777A80
void CBitOutput::OutputBits(uint32_t value, uint32_t count) {
    this->m_rack |= value << this->m_rackbits;
    this->m_rackbits += count;

    while (this->m_rackbits >= 8) {
        if (this->m_bytesleft) {
            *this->m_currdest++ = static_cast<uint8_t>(this->m_rack);
            this->m_bytesleft--;
        }

        this->m_rack >>= 8;
        this->m_rackbits -= 8;
    }
}

// VERIFY: cd187739 manual
// OFFSET: 0x00777AD0
void CBitOutput::Pad() {
    while (this->m_rackbits) {
        if (this->m_bytesleft) {
            *this->m_currdest++ = static_cast<uint8_t>(this->m_rack);
            this->m_bytesleft--;
        }

        this->m_rack >>= 8;
        this->m_rackbits -= this->m_rackbits > 8 ? 8 : this->m_rackbits;
    }
}

// INLINED
void ImaAdpcmCheckOptimization(uint32_t optimization, uint8_t* bitspersample, uint32_t* hint) {
    if (optimization) {
        if (optimization <= 2) {
            *bitspersample = 4;
            *hint = 6;
            return;
        }

        if (optimization == 3) {
            *bitspersample = 6;
            *hint = 8;
            return;
        }
    }

    *bitspersample = 5;
    *hint = 7;
}

// VERIFY: cd08f35c manual
// OFFSET: 0x00777B60
uint32_t ImaAdpcmCompress(uint8_t* dest, uint32_t destsize, const int16_t* source, uint32_t sourcesize, uint32_t channels, uint8_t bitspersample) {
    uint8_t* basedest = dest;

    if (destsize < 2) {
        return 2;
    }

    _IMAHEADER header;
    header.reserved = 0;
    header.roundshift = bitspersample - 1;
    *reinterpret_cast<_IMAHEADER*>(dest) = header;

    uint8_t* currdest = dest + 2;
    uint32_t headersize = static_cast<uint32_t>(currdest + 2 * channels - basedest);

    if (static_cast<int32_t>(headersize) > static_cast<int32_t>(destsize)) {
        return headersize;
    }

    int32_t index[2] = { 44, 44 };
    int32_t last[2];

    for (uint32_t i = 0; i < channels; i++) {
        int16_t sample = *source++;
        *reinterpret_cast<int16_t*>(currdest) = sample;
        currdest += 2;
        last[i] = sample;
    }

    int32_t budget = static_cast<int32_t>(basedest + static_cast<int32_t>(sourcesize) / 2 - currdest);
    uint32_t escapebudget = budget < 0 ? 0 : budget;
    uint32_t channel = channels - 1;
    uint32_t samples = sourcesize / 2;
    uint32_t loop = channels;

    if (loop >= samples) {
        return static_cast<uint32_t>(currdest - basedest);
    }

    while (static_cast<int32_t>(currdest - basedest + 2) <= static_cast<int32_t>(destsize)) {
        if (channels == 2) {
            channel = channel == 0;
        }

        int32_t sample = *source++;
        int32_t difference = sample - last[channel];
        uint8_t negative = difference < 0 ? 0x40 : 0;
        uint32_t absdifference = abs(difference);
        uint32_t step = s_ima_steptable[index[channel]];

        if (absdifference < step >> bitspersample) {
            if (index[channel]) {
                index[channel]--;
            }

            *currdest++ = 0x80;
        } else {
            while (absdifference > 2 * step) {
                if (index[channel] >= 88 || !escapebudget) {
                    break;
                }

                index[channel] += 8;

                if (index[channel] > 88) {
                    index[channel] = 88;
                }

                step = s_ima_steptable[index[channel]];
                *currdest++ = 0x81;
                escapebudget--;
            }

            uint32_t round = step >> header.roundshift;
            uint32_t highbit = 1 << (bitspersample - 2);

            if (highbit > 0x20) {
                highbit = 0x20;
            }

            uint32_t delta = 0;
            uint32_t vpdiff = 0;
            uint32_t bit = 1;

            while (true) {
                if (vpdiff + step <= absdifference) {
                    delta |= bit;
                    vpdiff += step;
                }

                if (bit == highbit) {
                    break;
                }

                step >>= 1;
                bit <<= 1;
            }

            int32_t predicted = round + vpdiff;
            int32_t value;

            if (negative) {
                value = last[channel] - predicted;

                if (value < -32768) {
                    value = -32768;
                }
            } else {
                value = last[channel] + predicted;

                if (value > 0x7FFF) {
                    value = 0x7FFF;
                }
            }

            last[channel] = value;
            *currdest++ = negative | static_cast<uint8_t>(delta);

            index[channel] += s_ima_adjtable[delta & 0x1F];

            if (index[channel] < 0) {
                index[channel] = 0;
            } else if (index[channel] > 88) {
                index[channel] = 88;
            }
        }

        if (++loop >= samples) {
            return static_cast<uint32_t>(currdest - basedest);
        }
    }

    return static_cast<uint32_t>(currdest - basedest + 2);
}

// VERIFY: 60539bcf manual
// OFFSET: 0x00777DE0
uint32_t ImaAdpcmDecompress(int16_t* dest, uint32_t destsize, const uint8_t* source, uint32_t sourcesize, uint32_t channels) {
    int16_t* basedest = dest;
    const uint8_t* endsource = source + sourcesize;
    const _IMAHEADER* header = reinterpret_cast<const _IMAHEADER*>(source);
    int32_t index[2] = { 44, 44 };
    int32_t last[2];
    const uint8_t* curr = source + 2;

    for (uint32_t i = 0; i < channels; i++) {
        int16_t sample = *reinterpret_cast<const int16_t*>(curr);
        curr += 2;
        last[i] = sample;

        if (destsize < 2) {
            return static_cast<uint32_t>(reinterpret_cast<uint8_t*>(dest) - reinterpret_cast<uint8_t*>(basedest));
        }

        *dest++ = sample;
        destsize -= 2;
    }

    uint32_t channel = channels - 1;

    while (curr < endsource) {
        if (channels == 2) {
            channel = channel == 0;
        }

        uint8_t code = *curr++;

        if (code & 0x80) {
            switch (code & 0x7F) {
                case 0:
                    if (index[channel]) {
                        index[channel]--;
                    }

                    if (destsize < 2) {
                        return static_cast<uint32_t>(reinterpret_cast<uint8_t*>(dest) - reinterpret_cast<uint8_t*>(basedest));
                    }

                    *dest++ = static_cast<int16_t>(last[channel]);
                    destsize -= 2;
                    break;
                case 1:
                    index[channel] += 8;

                    if (index[channel] > 88) {
                        index[channel] = 88;
                    }

                    if (channels == 2) {
                        channel = channel == 0;
                    }

                    break;
                case 2:
                    index[channel] -= 8;

                    if (index[channel] < 0) {
                        index[channel] = 0;
                    }

                    if (channels == 2) {
                        channel = channel == 0;
                    }

                    break;
                default:
                    break;
            }
        } else {
            uint32_t step = s_ima_steptable[index[channel]];
            int32_t difference = step >> header->roundshift;

            if (code & 0x01) {
                difference += step;
            }

            if (code & 0x02) {
                difference += step >> 1;
            }

            if (code & 0x04) {
                difference += step >> 2;
            }

            if (code & 0x08) {
                difference += step >> 3;
            }

            if (code & 0x10) {
                difference += step >> 4;
            }

            if (code & 0x20) {
                difference += step >> 5;
            }

            int32_t value;

            if (code & 0x40) {
                value = last[channel] - difference;

                if (value <= -32768) {
                    value = -32768;
                }
            } else {
                value = last[channel] + difference;

                if (value >= 0x7FFF) {
                    value = 0x7FFF;
                }
            }

            last[channel] = value;

            if (destsize < 2) {
                return static_cast<uint32_t>(reinterpret_cast<uint8_t*>(dest) - reinterpret_cast<uint8_t*>(basedest));
            }

            *dest++ = static_cast<int16_t>(value);
            destsize -= 2;

            index[channel] += s_ima_adjtable[code & 0x1F];

            if (index[channel] < 0) {
                index[channel] = 0;
            } else if (index[channel] > 88) {
                index[channel] = 88;
            }
        }
    }

    return static_cast<uint32_t>(reinterpret_cast<uint8_t*>(dest) - reinterpret_cast<uint8_t*>(basedest));
}

}

// VERIFY: 42257291 manual
// OFFSET: 0x00777B10
void CHuffmanEncoder::EncodeSymbol(CBitOutput* output, int32_t symbol) {
    HUFFNODE* node = this->m_symbol[symbol];
    uint32_t value = 0;
    uint32_t count = 0;

    for (HUFFNODE* parent = node->parent; parent; parent = parent->parent) {
        value = (value << 1) | (parent->child != node);
        node = parent;
        count++;
    }

    output->OutputBits(value, count);
}

// VERIFY: f509188b manual
// OFFSET: 0x00777FC0
static voidpf ZlibAlloc(voidpf opaque, uInt items, uInt size) {
    ZLIBBUFFER* buffer = static_cast<ZLIBBUFFER*>(opaque);
    uint32_t bytes = items * size;

    if (bytes & 3) {
        bytes += 4 - (bytes & 3);
    }

    char* result = buffer->curr;

    if (bytes >= static_cast<uint32_t>(buffer->base + buffer->size - buffer->curr)) {
        return SMemAlloc(bytes, __FILE__, __LINE__, 0);
    }

    buffer->curr += bytes;

    return result;
}

// VERIFY: 47a3b631 manual
// OFFSET: 0x00778010
static void ZlibFree(voidpf opaque, voidpf address) {
    ZLIBBUFFER* buffer = static_cast<ZLIBBUFFER*>(opaque);

    if (address < buffer->base || address >= buffer->base + buffer->size) {
        SMemFree(address, __FILE__, __LINE__, 0);
    }
}

// VERIFY: e5907848 auto
// OFFSET: 0x00778040
int32_t STORMAPI zlib_compress(uint8_t* dest, uint32_t* destLen, const uint8_t* source, uint32_t sourceLen, int32_t level) {
    char storage[300000];
    ZLIBBUFFER buffer;
    buffer.curr = storage;
    buffer.size = sizeof(storage);
    buffer.base = storage;

    z_stream stream;
    stream.next_in = const_cast<uint8_t*>(source);
    stream.avail_in = sourceLen;
    stream.next_out = dest;
    stream.avail_out = *destLen;
    stream.zalloc = ZlibAlloc;
    stream.zfree = ZlibFree;
    stream.opaque = &buffer;

    int32_t err = deflateInit(&stream, level);

    if (err != Z_OK) {
        return err;
    }

    err = deflate(&stream, Z_FINISH);

    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }

    *destLen = stream.total_out;

    return deflateEnd(&stream);
}

// VERIFY: bb488e5e manual
// OFFSET: 0x00778110
void ZlibCompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize, uint32_t* hint, uint32_t optimization) {
    int32_t level;

    if (optimization == 1) {
        level = Z_BEST_COMPRESSION;
    } else if (optimization == 2) {
        level = Z_BEST_SPEED;
    } else {
        level = Z_DEFAULT_COMPRESSION;
    }

    uint32_t size = *destsize;

    if (!zlib_compress(static_cast<uint8_t*>(dest), &size, static_cast<const uint8_t*>(source), sourcesize, level)) {
        *destsize = size;
    }

    *hint = 0;
}

// VERIFY: 0fa76d03 auto
// OFFSET: 0x00778180
int32_t STORMAPI zlib_uncompress(uint8_t* dest, uint32_t* destLen, const uint8_t* source, uint32_t sourceLen) {
    char storage[47000];
    ZLIBBUFFER buffer;
    buffer.curr = storage;
    buffer.size = sizeof(storage);
    buffer.base = storage;

    z_stream stream;
    stream.next_in = const_cast<uint8_t*>(source);
    stream.avail_in = sourceLen;
    stream.next_out = dest;
    stream.avail_out = *destLen;
    stream.zalloc = ZlibAlloc;
    stream.zfree = ZlibFree;
    stream.opaque = &buffer;

    int32_t err = inflateInit(&stream);

    if (err != Z_OK) {
        return err;
    }

    err = inflate(&stream, Z_FINISH);

    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }

    *destLen = stream.total_out;

    return inflateEnd(&stream);
}

// VERIFY: 356d3f12 auto
// OFFSET: 0x00778250
int32_t ZlibDecompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize) {
    uint32_t size = *destsize;

    if (zlib_uncompress(static_cast<uint8_t*>(dest), &size, static_cast<const uint8_t*>(source), sourcesize)) {
        return 0;
    }

    *destsize = size;

    return 1;
}

// VERIFY: c10a7cae manual
// OFFSET: 0x00778290
void RLEZeroCompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize, uint32_t* hint, uint32_t optimization) {
    uint8_t* basedest = static_cast<uint8_t*>(dest);
    uint8_t* enddest = basedest + *destsize;
    const uint8_t* curr = static_cast<const uint8_t*>(source);
    const uint8_t* endsource = curr + sourcesize;

    if (basedest + 4 >= enddest) {
        return;
    }

    basedest[0] = static_cast<uint8_t>(sourcesize >> 24);
    basedest[1] = static_cast<uint8_t>(sourcesize >> 16);
    basedest[2] = static_cast<uint8_t>(sourcesize >> 8);
    basedest[3] = static_cast<uint8_t>(sourcesize);

    uint8_t* currdest = basedest + 4;

    while (curr < endsource - 32) {
        uint32_t zeros = 0;
        const uint8_t* literalend = curr;

        for (const uint8_t* scan = curr; scan < endsource; scan++) {
            if (*scan) {
                if (zeros >= 32) {
                    break;
                }

                zeros = 0;
                literalend = scan + 1;
            } else {
                zeros++;
            }
        }

        uint32_t literal = static_cast<uint32_t>(literalend - curr);

        if (literal) {
            while (literal > 0xBF) {
                if (currdest + 160 >= enddest) {
                    return;
                }

                *currdest++ = 0xFF;
                memcpy(currdest, curr, 159);
                currdest += 159;
                curr += 159;
                literal -= 159;
            }

            if (literal > 0x9F) {
                if (currdest + 33 >= enddest) {
                    return;
                }

                *currdest++ = 0x80;
                memcpy(currdest, curr, 32);
                currdest += 32;
                curr += 32;
                literal -= 32;
            }

            if (literal < 32) {
                if (currdest + 33 >= enddest) {
                    return;
                }

                *currdest++ = 0x80;
                memcpy(currdest, curr, 32);
                currdest += 32;
                curr += 32;
                zeros = zeros + literal - 32;
            } else {
                if (currdest + literal + 1 >= enddest) {
                    return;
                }

                *currdest++ = static_cast<uint8_t>((literal - 32) | 0x80);
                memcpy(currdest, curr, literal);
                currdest += literal;
                curr += literal;
            }
        }

        while (zeros > 191) {
            if (currdest + 1 >= enddest) {
                return;
            }

            *currdest++ = 0x7F;
            curr += 159;
            zeros -= 159;
        }

        if (zeros > 159) {
            if (currdest + 1 >= enddest) {
                return;
            }

            *currdest++ = 0;
            curr += 32;
            zeros -= 32;
        }

        if (zeros >= 32) {
            if (currdest + 1 >= enddest) {
                return;
            }

            *currdest++ = static_cast<uint8_t>(zeros - 32);
            curr += zeros;
        }
    }

    if (curr < endsource) {
        for (const uint8_t* scan = curr; scan < endsource; scan++) {
            if (*scan) {
                uint32_t literal = static_cast<uint32_t>(endsource - curr);

                if (currdest + literal + 1 < enddest) {
                    *currdest++ = 0x80;
                    memcpy(currdest, curr, literal);
                    *destsize = static_cast<uint32_t>(currdest + literal - basedest);
                }

                return;
            }
        }

        if (currdest + 1 >= enddest) {
            return;
        }

        *currdest++ = 0;
    }

    *destsize = static_cast<uint32_t>(currdest - basedest);
}

// VERIFY: 95b62d68 manual
// OFFSET: 0x00778500
int32_t RLEZeroDecompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize) {
    uint8_t* currdest = static_cast<uint8_t*>(dest);
    const uint8_t* curr = static_cast<const uint8_t*>(source);
    const uint8_t* endsource = curr + sourcesize;

    if (sourcesize < 5) {
        return 0;
    }

    uint32_t remaining = (curr[0] << 24) | (curr[1] << 16) | (curr[2] << 8) | curr[3];
    *destsize = remaining;
    curr += 4;

    while (curr < endsource) {
        uint8_t code = *curr++;
        uint32_t count = (code & 0x7F) + 32;

        if (count >= remaining) {
            count = remaining;
        }

        if (code & 0x80) {
            memcpy(currdest, curr, count);
            curr += count;
        } else {
            memset(currdest, 0, count);
        }

        currdest += count;
        remaining -= count;
    }

    return 1;
}

// VERIFY: e72b39e6 auto
// OFFSET: 0x007785A0
void Bzip2Compress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize, uint32_t* hint, uint32_t optimization) {
    int32_t result = BZ2_bzBuffToBuffCompress(static_cast<char*>(dest), destsize, static_cast<char*>(const_cast<void*>(source)), sourcesize, 9, 0, 0);

    if (result == BZ_OUTBUFF_FULL) {
        *destsize = sourcesize;
    } else if (result != BZ_OK) {
        char str[256];
        SStrPrintf(str, sizeof(str), "bzip2Compress:%d", result);
        SErrDisplayError(STORM_ERROR_FILE_CORRUPTED, "<unknown>", SLOG_FILE, str, 0, 1, 0x11111111);
    }

    *hint = 0;
}

struct Bzip2BlockCache {
    static const uint32_t SLOTS = 2;

    void* m_blocks[SLOTS] = {};

    ~Bzip2BlockCache() {
        for (uint32_t i = 0; i < SLOTS; i++) {
            free(this->m_blocks[i]);
        }
    }
};

// Every sector used to malloc and free about 3.6MB of decoder state, so each thread keeps its freed blocks for the next sector.
static thread_local Bzip2BlockCache t_bzip2Blocks;

static const size_t BZIP2_BLOCK_HEADER = 16;

static size_t Bzip2BlockSize(void* block) {
    return *static_cast<size_t*>(block);
}

static void* Bzip2Alloc(void* opaque, int32_t items, int32_t size) {
    size_t bytes = static_cast<size_t>(items) * static_cast<size_t>(size);
    Bzip2BlockCache& cache = t_bzip2Blocks;
    uint32_t best = Bzip2BlockCache::SLOTS;

    for (uint32_t i = 0; i < Bzip2BlockCache::SLOTS; i++) {
        void* block = cache.m_blocks[i];

        if (block && Bzip2BlockSize(block) >= bytes && (best == Bzip2BlockCache::SLOTS || Bzip2BlockSize(block) < Bzip2BlockSize(cache.m_blocks[best]))) {
            best = i;
        }
    }

    void* block;

    if (best != Bzip2BlockCache::SLOTS) {
        block = cache.m_blocks[best];
        cache.m_blocks[best] = nullptr;
    } else {
        block = malloc(bytes + BZIP2_BLOCK_HEADER);

        if (!block) {
            return nullptr;
        }

        *static_cast<size_t*>(block) = bytes;
    }

    return static_cast<uint8_t*>(block) + BZIP2_BLOCK_HEADER;
}

static void Bzip2Free(void* opaque, void* address) {
    if (!address) {
        return;
    }

    void* block = static_cast<uint8_t*>(address) - BZIP2_BLOCK_HEADER;
    Bzip2BlockCache& cache = t_bzip2Blocks;
    uint32_t smallest = 0;

    for (uint32_t i = 0; i < Bzip2BlockCache::SLOTS; i++) {
        if (!cache.m_blocks[i]) {
            cache.m_blocks[i] = block;
            return;
        }

        if (Bzip2BlockSize(cache.m_blocks[i]) < Bzip2BlockSize(cache.m_blocks[smallest])) {
            smallest = i;
        }
    }

    if (Bzip2BlockSize(block) > Bzip2BlockSize(cache.m_blocks[smallest])) {
        free(cache.m_blocks[smallest]);
        cache.m_blocks[smallest] = block;
        return;
    }

    free(block);
}

// VERIFY: 5ff122e8 auto
// OFFSET: 0x00778630
int32_t Bzip2Decompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize) {
    bz_stream stream = {};
    stream.bzalloc = Bzip2Alloc;
    stream.bzfree = Bzip2Free;

    if (BZ2_bzDecompressInit(&stream, 0, 0) != BZ_OK) {
        return 0;
    }

    stream.next_in = static_cast<char*>(const_cast<void*>(source));
    stream.avail_in = sourcesize;
    stream.next_out = static_cast<char*>(dest);
    stream.avail_out = *destsize;

    int32_t result = BZ2_bzDecompress(&stream);

    if (result == BZ_STREAM_END) {
        *destsize -= stream.avail_out;
    }

    BZ2_bzDecompressEnd(&stream);
    return result == BZ_STREAM_END;
}

// VERIFY: 1b0eface manual
// OFFSET: 0x00778660
void ImaAdpcmMonoCompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize, uint32_t* hint, uint32_t optimization) {
    uint8_t bitspersample;
    ImaAdpcmCheckOptimization(optimization, &bitspersample, hint);
    *destsize = ImaAdpcmCompress(static_cast<uint8_t*>(dest), *destsize, static_cast<const int16_t*>(source), sourcesize, 1, bitspersample);
}

// VERIFY: 49716049 auto
// OFFSET: 0x007786D0
int32_t ImaAdpcmMonoDecompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize) {
    *destsize = ImaAdpcmDecompress(static_cast<int16_t*>(dest), *destsize, static_cast<const uint8_t*>(source), sourcesize, 1);
    return 1;
}

// VERIFY: da8efbd6 manual
// OFFSET: 0x00778700
void ImaAdpcmStereoCompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize, uint32_t* hint, uint32_t optimization) {
    uint8_t bitspersample;
    ImaAdpcmCheckOptimization(optimization, &bitspersample, hint);
    *destsize = ImaAdpcmCompress(static_cast<uint8_t*>(dest), *destsize, static_cast<const int16_t*>(source), sourcesize, 2, bitspersample);
}

// VERIFY: a08eb470 auto
// OFFSET: 0x00778770
int32_t ImaAdpcmStereoDecompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize) {
    *destsize = ImaAdpcmDecompress(static_cast<int16_t*>(dest), *destsize, static_cast<const uint8_t*>(source), sourcesize, 2);
    return 1;
}

// VERIFY: 13162078 manual
// OFFSET: 0x007787A0
int32_t STORMAPI SCompDestroy() {
    if (s_DecompressBuffer) {
        SMemFree(s_DecompressBuffer, __FILE__, __LINE__, 0);
        s_DecompressBuffer = nullptr;
    }

    return 1;
}

// VERIFY: 22534a7e manual
// OFFSET: 0x007787D0
HUFFNODE* CHuffman::AllocNode(uint32_t linktype) {
    HUFFNODE* node = this->m_nodefreelist.Head();

    if (!node) {
        node = &this->m_nodebuffer[this->m_nodesused++];
    }

    this->m_nodelist.LinkNode(node, linktype, nullptr);
    node->parent = nullptr;
    node->child = nullptr;

    return node;
}

// VERIFY: 163da683 manual
// OFFSET: 0x00778830
void CHuffman::BuildTree(uint8_t hint) {
    HUFFNODE* node;

    while ((node = this->m_nodelist.Head()) != nullptr) {
        this->m_nodefreelist.LinkNode(node, TSLIST_LINK_AFTER, nullptr);
    }

    memset(this->m_symbol, 0, sizeof(this->m_symbol));

    uint32_t maxweight = 0;
    const uint8_t* probability = s_probability[hint];
    uint32_t symbol;

    for (symbol = 0; symbol < 256; symbol++) {
        if (!probability[symbol]) {
            continue;
        }

        HUFFNODE* newnode = this->AllocNode(2);
        this->m_symbol[symbol] = newnode;
        newnode->symbol = symbol;
        newnode->weight = probability[symbol];

        if (newnode->weight < maxweight) {
            HUFFNODE* insert;

            for (insert = this->m_nodelist.Head(); insert; insert = insert->Next()) {
                if (insert->weight >= newnode->weight) {
                    break;
                }
            }

            this->m_nodelist.LinkNode(newnode, TSLIST_LINK_BEFORE, insert);
        } else {
            maxweight = newnode->weight;
        }
    }

    for (; symbol < 258; symbol++) {
        HUFFNODE* newnode = this->AllocNode(1);
        this->m_symbol[symbol] = newnode;
        newnode->symbol = symbol;
        newnode->weight = 1;
    }

    HUFFNODE* child0 = this->m_nodelist.Head();

    while (child0) {
        HUFFNODE* child1 = child0->Next();

        if (!child1) {
            break;
        }

        HUFFNODE* parent = this->AllocNode(2);
        parent->weight = child0->weight + child1->weight;
        parent->child = child0;
        child0->parent = parent;
        child1->parent = parent;

        if (parent->weight < maxweight) {
            HUFFNODE* insert;

            for (insert = child1->Next(); insert; insert = insert->Next()) {
                if (insert->weight >= parent->weight) {
                    break;
                }
            }

            this->m_nodelist.LinkNode(parent, TSLIST_LINK_BEFORE, insert);
        } else {
            maxweight = parent->weight;
        }

        child0 = child1->Next();
    }

    this->m_changesequence = 1;
}

// VERIFY: 186ab7d5 manual
// OFFSET: 0x00778C30
void CHuffman::IncrementWeight(HUFFNODE* node) {
    for (HUFFNODE* curr = node; curr; curr = curr->parent) {
        curr->weight++;

        HUFFNODE* last = curr;
        HUFFNODE* next;

        for (next = curr->Next(); next; next = next->Next()) {
            if (next->weight >= curr->weight) {
                break;
            }

            last = next;
        }

        if (last == curr) {
            continue;
        }

        this->m_nodelist.LinkNode(last, TSLIST_LINK_BEFORE, curr);
        this->m_nodelist.LinkNode(curr, TSLIST_LINK_BEFORE, next);

        HUFFNODE* lastchild = last->parent->child;

        if (curr->parent->child == curr) {
            curr->parent->child = last;
        }

        if (lastchild == last) {
            last->parent->child = curr;
        }

        TSSwap(last->parent, curr->parent);
        this->m_changesequence++;
    }
}

// VERIFY: a9e74d9d manual
// OFFSET: 0x00778D50
int32_t CHuffmanDecoder::DecodeSymbol(CBitInput* input) {
    uint32_t cachebits = input->PeekBits(7, 0x7F);
    _CACHEREC* cacheslot = &this->m_cache[cachebits];
    int32_t cachevalid = this->m_changesequence == cacheslot->sequence;
    HUFFNODE* currnode;

    if (cachevalid) {
        if (cacheslot->bits <= 7) {
            input->RemoveBits(cacheslot->bits);
            return cacheslot->symbol;
        }

        input->RemoveBits(7);
        currnode = cacheslot->nodeptr;
    } else {
        currnode = this->m_nodelist.Tail();
    }

    uint32_t bits = 0;
    HUFFNODE* cachenode = nullptr;

    do {
        currnode = currnode->child;

        if (input->InputBit()) {
            currnode = currnode->RawNext();
        }

        if (++bits == 7) {
            cachenode = currnode;
        }
    } while (currnode->child);

    if (!cachevalid) {
        if (bits > 7) {
            cacheslot->sequence = this->m_changesequence;
            cacheslot->bits = bits;
            cacheslot->nodeptr = cachenode;
        } else {
            for (uint32_t index = cachebits & (0xFFFFFFFF >> (32 - bits)); index <= 0x7F; index += 1 << bits) {
                this->m_cache[index].sequence = this->m_changesequence;
                this->m_cache[index].bits = bits;
                this->m_cache[index].symbol = currnode->symbol;
            }
        }
    }

    return currnode->symbol;
}

// VERIFY: 2e64481f manual
// OFFSET: 0x00778EC0
void CHuffman::AddSymbol(int32_t symbol) {
    HUFFNODE* parent = this->m_nodelist.Head();

    HUFFNODE* oldnode = this->AllocNode(1);
    oldnode->symbol = parent->symbol;
    oldnode->weight = parent->weight;
    oldnode->parent = parent;
    this->m_symbol[parent->symbol] = oldnode;

    HUFFNODE* newnode = this->AllocNode(1);
    newnode->symbol = symbol;
    newnode->weight = 0;
    newnode->parent = parent;
    this->m_symbol[symbol] = newnode;

    parent->child = newnode;
    this->IncrementWeight(newnode);
}

// VERIFY: c033b55f manual
// OFFSET: 0x00778F30
CHuffman::~CHuffman() {
}

// VERIFY: cc595800 manual
// OFFSET: 0x00779000
uint32_t CHuffmanDecoder::Decompress(void* dest, uint32_t destsize, CBitInput* input) {
    if (!destsize) {
        return 0;
    }

    uint32_t hint = input->InputBits(8, 0xFF);
    this->BuildTree(static_cast<uint8_t>(hint));
    this->m_adaptive = hint == 0;

    uint8_t* currdest = static_cast<uint8_t*>(dest);

    while (true) {
        int32_t symbol = this->DecodeSymbol(input);

        if (symbol == 257) {
            symbol = input->InputBits(8, 0xFF);
            this->AddSymbol(symbol);

            if (!this->m_adaptive) {
                this->IncrementWeight(this->m_symbol[symbol]);
            }
        }

        if (symbol == 256) {
            break;
        }

        *currdest++ = static_cast<uint8_t>(symbol);

        if (!--destsize) {
            break;
        }

        if (this->m_adaptive) {
            this->IncrementWeight(this->m_symbol[symbol]);
        }
    }

    return static_cast<uint32_t>(currdest - static_cast<uint8_t*>(dest));
}

// VERIFY: b3e4b8da manual
// OFFSET: 0x00779110
uint32_t CHuffmanEncoder::Compress(CBitOutput* output, const void* source, uint32_t sourcesize, uint8_t hint) {
    this->BuildTree(hint);
    this->m_adaptive = hint == 0;
    output->OutputBits(hint, 8);

    const uint8_t* currsource = static_cast<const uint8_t*>(source);

    for (; sourcesize; sourcesize--) {
        uint8_t symbol = *currsource++;

        if (!this->m_symbol[symbol]) {
            this->EncodeSymbol(output, 257);
            output->OutputBits(symbol, 8);
            this->AddSymbol(symbol);

            if (!this->m_adaptive) {
                this->IncrementWeight(this->m_symbol[symbol]);
            }
        } else {
            this->EncodeSymbol(output, symbol);
        }

        if (this->m_adaptive) {
            this->IncrementWeight(this->m_symbol[symbol]);
        }
    }

    this->EncodeSymbol(output, 256);
    output->Pad();

    return output->GetTotalBytes();
}

// VERIFY: b545954c manual
// OFFSET: 0x007791E0
CHuffman::CHuffman() {
    this->m_changesequence = 1;
    this->m_nodesused = 0;
}

// INLINED
CHuffmanDecoder::CHuffmanDecoder() {
    for (uint32_t i = 0; i < 128; i++) {
        this->m_cache[i].sequence = 0;
    }
}

// VERIFY: b2762984 manual
// OFFSET: 0x00779250
int32_t HuffmanDecompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize) {
    CBitInput input(source);
    CHuffmanDecoder decoder;
    *destsize = decoder.Decompress(dest, *destsize, &input);

    return 1;
}

// VERIFY: 143ff329 auto
// OFFSET: 0x007792D0
void HuffmanCompress(void* dest, uint32_t* destsize, const void* source, uint32_t sourcesize, uint32_t* hint, uint32_t optimization) {
    CBitOutput output(dest, *destsize);
    CHuffmanEncoder encoder;
    *destsize = encoder.Compress(&output, source, sourcesize, static_cast<uint8_t>(*hint));
}
