#include <cstdint>
#include <cstring>
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SComp.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/TempBuffer.h"
#include "Engine/Source/Storm/Source/SComp.h"
extern "C" {
#include "ThirdParty/zlib/contrib/blast/blast.h"
}
#include "ThirdParty/bzip2/bzlib.h"
#include "ThirdParty/zlib/zlib.h"

extern "C" {
#include "ThirdParty/lzma/LzmaDecode.h"
}

// STATIC: 0x00B38324
Blizzard::Thread::LocalObject<System_SComp::ZlibAllocBuffer> System_SComp::s_zlibAllocBuffer;

// VERIFY: 07c244c0 manual
// OFFSET: 0x00463FD0
bool LzmaMemDecode(const uint8_t* source, uint32_t sourceSize, uint8_t* dest, uint32_t destSize, uint32_t* destProcessed, void* (*alloc)(uint32_t size), void (*free)(void* address)) {
    if (sourceSize < 14) {
        return false;
    }

    *destProcessed = 0;

    CLzmaDecoderState state;

    if (source[0] || LzmaDecodeProperties(&state.Properties, source + 1, 5) != LZMA_RESULT_OK) {
        return false;
    }

    state.Probs = static_cast<CProb*>(alloc(LzmaGetNumProbs(&state.Properties) * sizeof(CProb)));

    if (!state.Probs) {
        return false;
    }

    SizeT inProcessed;
    SizeT outProcessed;
    int result = LzmaDecode(&state, source + 14, sourceSize - 14, &inProcessed, dest, destSize, &outProcessed);
    free(state.Probs);

    if (result != LZMA_RESULT_OK) {
        return false;
    }

    *destProcessed = static_cast<uint32_t>(outProcessed);
    return true;
}

// VERIFY: 8d0660e7 manual
// OFFSET: 0x0042BE30
int32_t System_SComp::RLEZeroDecompress(void* dest, uint32_t* destSize, const void* source, uint32_t sourceSize) {
    uint8_t* output = static_cast<uint8_t*>(dest);
    const uint8_t* input = static_cast<const uint8_t*>(source);
    const uint8_t* end = input + sourceSize;

    if (sourceSize < 5) {
        return 0;
    }

    uint32_t remaining = (input[0] << 24) | (input[1] << 16) | (input[2] << 8) | input[3];
    *destSize = remaining;
    input += 4;

    while (input < end) {
        int8_t control = static_cast<int8_t>(*input++);
        uint32_t count;

        if (control >= 0) {
            count = (control & 0x7F) + 3;
            count = count >= remaining ? remaining : count;
            memset(output, 0, count);
        } else {
            count = (control & 0x7F) + 1;
            count = count >= remaining ? remaining : count;
            memcpy(output, input, count);
            input += count;
        }

        output += count;
        remaining -= count;
    }

    return 1;
}

// VERIFY: 1e9af3a0 manual
// OFFSET: 0x00431300
voidpf System_SComp::ZlibAlloc(voidpf opaque, uInt items, uInt size) {
    ZlibAllocBuffer* buffer = static_cast<ZlibAllocBuffer*>(opaque);
    uint32_t bytes = items * size;

    if (bytes & 3) {
        bytes += 4 - (bytes & 3);
    }

    uint8_t* result = buffer->m_next;

    if (bytes >= static_cast<uint32_t>(buffer->m_buffer + sizeof(buffer->m_buffer) - result)) {
        return Blizzard::Memory::Allocate(bytes);
    }

    buffer->m_next = result + bytes;
    return result;
}

// VERIFY: 7b0034c4 manual
// OFFSET: 0x00431350
void System_SComp::ZlibFree(voidpf opaque, voidpf address) {
    ZlibAllocBuffer* buffer = static_cast<ZlibAllocBuffer*>(opaque);

    if (address < buffer->m_buffer || address >= buffer->m_buffer + sizeof(buffer->m_buffer)) {
        Blizzard::Memory::Free(address);
    }
}

// VERIFY: 6e3399c3 manual
// OFFSET: 0x00431380
bool __stdcall System_SComp::DecompressBlock(void* dst, uint32_t* dstSize, const void* src, uint32_t srcSize, Decompressor* decompressors) {
    uint32_t capacity = *dstSize;

    if (!decompressors || !decompressors[0]) {
        return false;
    }

    uint32_t count = 1;

    while (count < 2 && decompressors[count]) {
        count++;
    }

    Blizzard::Memory::TempBuffer<uint8_t, 0x4000> scratch(0);
    uint8_t* output = static_cast<uint8_t*>(dst);
    const uint8_t* input = static_cast<const uint8_t*>(src);
    uint32_t inputSize = srcSize;

    if (count >= 2 || (output + capacity > input && input + srcSize > output)) {
        scratch.Resize(capacity);
    }

    for (uint32_t i = 0; i < count; i++) {
        uint8_t* target = ((count - i) & 1) ? static_cast<uint8_t*>(dst) : scratch.m_data;

        if (target + capacity > input && input + inputSize > target) {
            if (target + capacity <= scratch.m_data || scratch.m_data + inputSize <= target) {
                target = scratch.m_data;
            } else {
                target = static_cast<uint8_t*>(dst);
            }
        }

        uint32_t size = capacity;

        if (!decompressors[i](target, &size, input, inputSize)) {
            return false;
        }

        inputSize = size;
        input = target;
    }

    if (input != dst) {
        memcpy(dst, input, inputSize);
    }

    *dstSize = inputSize;
    return true;
}

// VERIFY: 6959d98d auto
// OFFSET: 0x00453B20
int System_SComp::zlib_decompress(uint8_t* dest, uint32_t* destLen, const uint8_t* source, uint32_t sourceLen) {
    z_stream stream;
    stream.next_in = const_cast<Bytef*>(source);
    stream.avail_in = sourceLen;
    stream.next_out = dest;
    stream.avail_out = *destLen;

    ZlibAllocBuffer* buffer = &static_cast<ZlibAllocBuffer&>(s_zlibAllocBuffer);
    buffer->m_next = buffer->m_buffer;
    stream.zalloc = ZlibAlloc;
    stream.zfree = ZlibFree;
    stream.opaque = buffer;

    int result = inflateInit_(&stream, "1.2.3", sizeof(stream));

    if (result != Z_OK) {
        return result;
    }

    result = inflate(&stream, Z_FINISH);

    if (result != Z_STREAM_END) {
        inflateEnd(&stream);
        return result ? result : Z_BUF_ERROR;
    }

    *destLen = stream.total_out;
    return inflateEnd(&stream);
}

// VERIFY: 4525b2cc auto
// OFFSET: 0x00453BB0
int32_t System_SComp::ZlibDecompress(void* dest, uint32_t* destSize, const void* source, uint32_t sourceSize) {
    uint32_t size = *destSize;

    if (zlib_decompress(static_cast<uint8_t*>(dest), &size, static_cast<const uint8_t*>(source), sourceSize)) {
        return 0;
    }

    *destSize = size;
    return 1;
}

// VERIFY: f56ff40c manual
// OFFSET: 0x00453BF0
void* System_SComp::alloc_wrap(uint32_t size) {
    ZlibAllocBuffer* buffer = &static_cast<ZlibAllocBuffer&>(s_zlibAllocBuffer);
    uint32_t bytes = size;

    if (size & 3) {
        bytes = size - (size & 3) + 4;
    }

    uint8_t* result = buffer->m_next;

    if (bytes >= static_cast<uint32_t>(buffer->m_buffer + sizeof(buffer->m_buffer) - result)) {
        return Blizzard::Memory::Allocate(bytes);
    }

    buffer->m_next = result + bytes;
    return result;
}

// VERIFY: 26a6ae96 manual
// OFFSET: 0x00453C40
void System_SComp::free_wrap(void* address) {
    ZlibAllocBuffer* buffer = &static_cast<ZlibAllocBuffer&>(s_zlibAllocBuffer);

    if (address < buffer->m_buffer || address >= buffer->m_buffer + sizeof(buffer->m_buffer)) {
        Blizzard::Memory::Free(address);
    }
}

// VERIFY: c8e5e01d auto
// OFFSET: 0x00453C70
int32_t System_SComp::LzmaDecompress(void* dest, uint32_t* destSize, const void* source, uint32_t sourceSize) {
    ZlibAllocBuffer* buffer = &static_cast<ZlibAllocBuffer&>(s_zlibAllocBuffer);
    buffer->m_next = buffer->m_buffer;
    return LzmaMemDecode(static_cast<const uint8_t*>(source), sourceSize, static_cast<uint8_t*>(dest), *destSize, destSize, alloc_wrap, free_wrap);
}

// VERIFY: 89255c8c auto
// OFFSET: 0x004268F0
unsigned int System_SComp::PkwareBufferRead(void* param, unsigned char** buffer) {
    PkwareBuffer* pkware = static_cast<PkwareBuffer*>(param);
    *buffer = const_cast<unsigned char*>(pkware->m_input);
    return pkware->m_inputSize;
}

// VERIFY: 68a4cabf auto
// OFFSET: 0x00426910
int System_SComp::PkwareBufferWrite(void* param, unsigned char* data, unsigned int size) {
    PkwareBuffer* pkware = static_cast<PkwareBuffer*>(param);
    memcpy(pkware->m_output + pkware->m_outputPos, data, size);
    pkware->m_outputPos += size;
    return 0;
}

// VERIFY: c24ff103 auto
// OFFSET: 0x00426940
int32_t System_SComp::PkwareDecompress(void* dest, uint32_t* destSize, const void* source, uint32_t sourceSize) {
    PkwareBuffer buffer;
    buffer.m_output = static_cast<uint8_t*>(dest);
    buffer.m_outputPos = 0;
    buffer.m_outputSize = *destSize;
    buffer.m_input = static_cast<const uint8_t*>(source);
    buffer.m_inputSize = sourceSize;

    if (blast(PkwareBufferRead, &buffer, PkwareBufferWrite, &buffer)) {
        return 0;
    }

    *destSize = buffer.m_outputPos;
    return 1;
}

// VERIFY: d59557d7 manual
// OFFSET: 0x004585F0
bool System_SComp::Decompress(void* dst, uint32_t* dstSize, const void* src, uint32_t srcSize) {
    if (*dstSize < srcSize || !srcSize) {
        return false;
    }

    if (srcSize == *dstSize) {
        if (dst != src) {
            memcpy(dst, src, srcSize);
        }

        return true;
    }

    int32_t type = *static_cast<const char*>(src);
    Decompressor decompressors[2] = { nullptr, nullptr };

    switch (type) {
        case COMPRESSION_ZLIB:
            decompressors[0] = ZlibDecompress;
            break;
        case COMPRESSION_PKWARE:
            decompressors[0] = PkwareDecompress;
            break;
        case COMPRESSION_BZIP2:
            decompressors[0] = ::Bzip2Decompress;
            break;
        case COMPRESSION_LZMA:
            decompressors[0] = LzmaDecompress;
            break;
        case COMPRESSION_RLE:
            decompressors[0] = RLEZeroDecompress;
            break;
        case COMPRESSION_RLE_ZLIB:
            decompressors[0] = ZlibDecompress;
            decompressors[1] = RLEZeroDecompress;
            break;
        case COMPRESSION_RLE_BZIP2:
            decompressors[0] = ::Bzip2Decompress;
            decompressors[1] = RLEZeroDecompress;
            break;
        default:
            Blizzard::File::AddToLastErrorStack(type, "System_SComp::Decompress - Unrecognized Compression Type", 4);
            return false;
    }

    return DecompressBlock(dst, dstSize, static_cast<const uint8_t*>(src) + 1, srcSize - 1, decompressors);
}

// STATIC: 0x009E6DF8
static const System_SComp::COMPRESSOR s_rleCompressor = { COMPRESSION_RLE, System_SComp::RLEZeroCompress };

// STATIC: 0x009E6E00
static const System_SComp::COMPRESSOR s_zlibCompressor = { COMPRESSION_ZLIB, System_SComp::ZlibCompress };

// STATIC: 0x009E6E08
static const System_SComp::COMPRESSOR s_pkwareCompressor = { COMPRESSION_PKWARE, System_SComp::PkwareCompress };

// STATIC: 0x009E6E10
static const System_SComp::COMPRESSOR s_bzip2Compressor = { COMPRESSION_BZIP2, System_SComp::Bzip2Compress };

// OFFSET: 0x004268D0
void System_SComp::PkwareCompress(void* dest, uint32_t* destSize, const void* source, uint32_t sourceSize, uint32_t* hint, uint8_t level) {
    *destSize = sourceSize;
    *hint = 0;
}

// OFFSET: 0x004269A0
void System_SComp::Bzip2Compress(void* dest, uint32_t* destSize, const void* source, uint32_t sourceSize, uint32_t* hint, uint8_t level) {
    int32_t result = BZ2_bzBuffToBuffCompress(static_cast<char*>(dest), destSize, static_cast<char*>(const_cast<void*>(source)), sourceSize, 9, 0, 0);

    if (result == BZ_OUTBUFF_FULL) {
        *destSize = sourceSize;
        *hint = 0;
        return;
    }

    if (result != BZ_OK) {
        *destSize = sourceSize;
    }

    *hint = 0;
}

// VERIFY: ab400dc7 manual
// OFFSET: 0x004269F0
uint8_t* System_SComp::BufferPool::Alloc() {
    uint8_t* result = this->m_base;
    uint32_t end = 1 << this->m_count;
    uint32_t bit = 0;

    if (!end) {
        return nullptr;
    }

    while (this->m_used & bit) {
        result += this->m_size;
        bit *= 2;

        if (bit == end) {
            return nullptr;
        }
    }

    this->m_used |= bit;
    return result;
}

// VERIFY: fc9c4cce manual
// OFFSET: 0x0042BBC0
void System_SComp::RLEZeroCompress(void* dest, uint32_t* destSize, const void* source, uint32_t sourceSize, uint32_t* hint, uint8_t level) {
    uint8_t* basedest = static_cast<uint8_t*>(dest);
    uint8_t* enddest = basedest + *destSize;
    const uint8_t* curr = static_cast<const uint8_t*>(source);
    const uint8_t* endsource = curr + sourceSize;

    if (basedest + 4 >= enddest) {
        return;
    }

    basedest[0] = static_cast<uint8_t>(sourceSize >> 24);
    basedest[1] = static_cast<uint8_t>(sourceSize >> 16);
    basedest[2] = static_cast<uint8_t>(sourceSize >> 8);
    basedest[3] = static_cast<uint8_t>(sourceSize);

    uint8_t* currdest = basedest + 4;

    while (curr < endsource - 3) {
        uint32_t zeros = 0;
        const uint8_t* literalend = curr;

        for (const uint8_t* scan = curr; scan < endsource; scan++) {
            if (*scan) {
                if (zeros >= 3) {
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
            while (literal > 0x81) {
                if (currdest + 129 >= enddest) {
                    return;
                }

                *currdest++ = 0xFF;
                memcpy(currdest, curr, 128);
                currdest += 128;
                curr += 128;
                literal -= 128;
            }

            if (literal > 0x80) {
                if (currdest + 2 >= enddest) {
                    return;
                }

                *currdest++ = 0x80;
                memcpy(currdest, curr, 1);
                currdest += 1;
                curr += 1;
                literal -= 1;
            }

            if (literal < 1) {
                if (currdest + 2 >= enddest) {
                    return;
                }

                *currdest++ = 0x80;
                memcpy(currdest, curr, 1);
                currdest += 1;
                curr += 1;
                zeros = zeros + literal - 1;
            } else {
                if (currdest + literal + 1 >= enddest) {
                    return;
                }

                *currdest++ = static_cast<uint8_t>((literal - 1) | 0x80);
                memcpy(currdest, curr, literal);
                currdest += literal;
                curr += literal;
            }
        }

        while (zeros > 0x85) {
            if (currdest + 1 >= enddest) {
                return;
            }

            *currdest++ = 0x7F;
            curr += 130;
            zeros -= 130;
        }

        if (zeros > 0x82) {
            if (currdest + 1 >= enddest) {
                return;
            }

            *currdest++ = 0;
            curr += 3;
            zeros -= 3;
        }

        if (zeros >= 3) {
            if (currdest + 1 >= enddest) {
                return;
            }

            *currdest++ = static_cast<uint8_t>(zeros - 3);
            curr += zeros;
        }
    }

    if (curr < endsource) {
        for (const uint8_t* scan = curr; scan < endsource; scan++) {
            if (*scan) {
                uint32_t literal = static_cast<uint32_t>(endsource - curr);

                if (currdest + literal + 1 < enddest) {
                    *currdest++ = 0xFF;
                    memcpy(currdest, curr, literal);
                    *destSize = static_cast<uint32_t>(currdest + literal - basedest);
                }

                return;
            }
        }

        if (currdest + 1 >= enddest) {
            return;
        }

        *currdest++ = 0x7F;
    }

    *destSize = static_cast<uint32_t>(currdest - basedest);
}

// VERIFY: f852877e auto
// OFFSET: 0x004539A0
int System_SComp::zlib_compress(uint8_t* dest, uint32_t* destLen, const uint8_t* source, uint32_t sourceLen, int level) {
    z_stream stream;
    stream.next_in = const_cast<Bytef*>(source);
    stream.avail_in = sourceLen;
    stream.next_out = dest;
    stream.avail_out = *destLen;

    ZlibAllocBuffer* buffer = &static_cast<ZlibAllocBuffer&>(s_zlibAllocBuffer);
    buffer->m_next = buffer->m_buffer;
    stream.zalloc = ZlibAlloc;
    stream.zfree = ZlibFree;
    stream.opaque = buffer;

    int windowBits;

    if (sourceLen <= 0x100) {
        windowBits = 8;
    } else if (sourceLen <= 0x200) {
        windowBits = 9;
    } else if (sourceLen <= 0x400) {
        windowBits = 10;
    } else if (sourceLen <= 0x800) {
        windowBits = 11;
    } else if (sourceLen <= 0x1000) {
        windowBits = 12;
    } else if (sourceLen <= 0x2000) {
        windowBits = 13;
    } else {
        windowBits = (sourceLen > 0x4000) + 14;
    }

    int result = deflateInit2_(&stream, level, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY, "1.2.3", sizeof(stream));

    if (result != Z_OK) {
        return result;
    }

    result = deflate(&stream, Z_FINISH);

    if (result != Z_STREAM_END) {
        deflateEnd(&stream);
        return result ? result : Z_BUF_ERROR;
    }

    *destLen = stream.total_out;
    return deflateEnd(&stream);
}

// OFFSET: 0x00453AB0
void System_SComp::ZlibCompress(void* dest, uint32_t* destSize, const void* source, uint32_t sourceSize, uint32_t* hint, uint8_t level) {
    int level2;

    switch (level) {
        case 1:
            level2 = Z_BEST_COMPRESSION;
            break;
        case 2:
            level2 = Z_BEST_SPEED;
            break;
        default:
            level2 = Z_DEFAULT_COMPRESSION;
            break;
    }

    uint32_t size = *destSize;

    if (!zlib_compress(static_cast<uint8_t*>(dest), &size, static_cast<const uint8_t*>(source), sourceSize, level2)) {
        *destSize = size;
    }

    *hint = 0;
}

// VERIFY: 804c8455 manual
// OFFSET: 0x00458080
bool System_SComp::Compress(void* dst, uint32_t* dstSize, bool* checksum, const void* src, uint32_t srcSize, uint32_t count, const Blizzard::Mopaq::CompressionAlgorithm* algorithms) {
    *checksum = false;

    const uint8_t* best = static_cast<const uint8_t*>(src);
    uint32_t bestSize = srcSize;
    bool bestPooled = false;
    uint8_t bestType = 0;

    if (count) {
        bool overlap = static_cast<const uint8_t*>(src) + srcSize > dst && static_cast<uint8_t*>(dst) + *dstSize > src;
        bool multiple = count > 1;
        bool twoStage = false;

        for (uint32_t i = 0; i < count; i++) {
            switch (algorithms[i].m_type) {
                case COMPRESSION_ZLIB:
                case COMPRESSION_PKWARE:
                case COMPRESSION_BZIP2:
                case COMPRESSION_LZMA:
                case COMPRESSION_RLE:
                    continue;
                default:
                    twoStage = true;
                    break;
            }
        }

        uint32_t buffers = overlap + multiple + twoStage;
        Blizzard::Memory::TempBuffer<uint8_t, 0x4000> scratch(0);

        if (buffers) {
            scratch.Resize(srcSize * buffers);
        }

        BufferPool pool;
        pool.m_base = scratch.m_data;
        pool.m_size = srcSize;
        pool.m_count = buffers;
        pool.m_used = 0;

        for (uint32_t j = 0; j < count; j++) {
            uint8_t type = algorithms[j].m_type;
            uint8_t level = algorithms[j].m_level;
            const COMPRESSOR* stages[2];
            uint32_t stageCount;

            switch (type) {
                case COMPRESSION_ZLIB:
                    stages[0] = &s_zlibCompressor;
                    stageCount = 1;
                    break;
                case COMPRESSION_PKWARE:
                    stages[0] = &s_pkwareCompressor;
                    stageCount = 1;
                    break;
                case COMPRESSION_BZIP2:
                    stages[0] = &s_bzip2Compressor;
                    stageCount = 1;
                    break;
                case COMPRESSION_LZMA:
                    Blizzard::File::AddToLastErrorStack(type, "System_SComp::Compress - Lzma compression isn't enabled", 4);
                    return false;
                case COMPRESSION_RLE:
                    stageCount = 1;
                    stages[0] = &s_rleCompressor;
                    break;
                case COMPRESSION_RLE_ZLIB:
                    stages[1] = &s_zlibCompressor;
                    stageCount = 2;
                    stages[0] = &s_rleCompressor;
                    break;
                case COMPRESSION_RLE_BZIP2:
                    stages[1] = &s_bzip2Compressor;
                    stageCount = 2;
                    stages[0] = &s_rleCompressor;
                    break;
                default:
                    Blizzard::File::AddToLastErrorStack(type, "System_SComp::Compress - Unrecognized Compression Type", 4);
                    return false;
            }

            uint8_t* output;
            bool outputPooled;

            if (!overlap && best != dst) {
                output = static_cast<uint8_t*>(dst);
                outputPooled = false;
            } else {
                output = pool.Alloc();
                outputPooled = true;
            }

            uint8_t* intermediate = nullptr;
            const uint8_t* inputs[2];
            uint8_t* outputs[2];

            if (stageCount == 1) {
                outputs[0] = output + 1;
                inputs[0] = static_cast<const uint8_t*>(src);
            } else {
                intermediate = pool.Alloc();
                inputs[0] = static_cast<const uint8_t*>(src);
                outputs[0] = intermediate;
                inputs[1] = intermediate;
                outputs[1] = output + 1;
            }

            uint32_t size = srcSize;
            uint8_t combined = 0;

            for (uint32_t k = 0; k < stageCount; k++) {
                const COMPRESSOR* stage = stages[k];
                uint32_t limit = size - 1;
                uint32_t outSize = limit;
                uint32_t hint = 0;
                stage->m_func(outputs[k], &outSize, inputs[k], size, &hint, level);
                size = outSize;

                if (outSize >= limit) {
                    size = 0xFFFFFFFF;
                    break;
                }

                if (!combined) {
                    combined = static_cast<uint8_t>(stage->m_type);
                } else if (combined == COMPRESSION_RLE && static_cast<uint8_t>(stage->m_type) == COMPRESSION_ZLIB) {
                    combined = COMPRESSION_RLE_ZLIB;
                } else if (combined == COMPRESSION_RLE && static_cast<uint8_t>(stage->m_type) == COMPRESSION_BZIP2) {
                    combined = COMPRESSION_RLE_BZIP2;
                } else {
                    size = 0xFFFFFFFF;
                    break;
                }
            }

            if (size < bestSize) {
                if (bestPooled && (1 << pool.m_count)) {
                    uint8_t* slot = pool.m_base;
                    uint32_t bit = 0;

                    while (slot != best) {
                        slot += pool.m_size;
                        bit *= 2;

                        if (bit == static_cast<uint32_t>(1 << pool.m_count)) {
                            break;
                        }
                    }

                    if (slot == best) {
                        pool.m_used &= ~bit;
                    }
                }

                *output = combined;
                bestType = combined;
                best = output;
                bestSize = size + 1;
                bestPooled = outputPooled;
            } else if (outputPooled && (1 << pool.m_count)) {
                uint8_t* slot = pool.m_base;
                uint32_t bit = 0;

                while (slot != output) {
                    slot += pool.m_size;
                    bit *= 2;

                    if (bit == static_cast<uint32_t>(1 << pool.m_count)) {
                        break;
                    }
                }

                if (slot == output) {
                    pool.m_used &= ~bit;
                }
            }

            if (intermediate && (1 << pool.m_count)) {
                uint8_t* slot = pool.m_base;
                uint32_t bit = 0;

                while (slot != intermediate) {
                    bit *= 2;
                    slot += pool.m_size;

                    if (bit == static_cast<uint32_t>(1 << pool.m_count)) {
                        break;
                    }
                }

                if (slot == intermediate) {
                    pool.m_used &= ~bit;
                }
            }
        }

        if (best != dst) {
            memcpy(dst, best, bestSize);

            if (bestPooled && (1 << pool.m_count)) {
                uint8_t* slot = pool.m_base;
                uint32_t bit = 0;

                while (slot != best) {
                    bit *= 2;
                    slot += pool.m_size;

                    if (bit == static_cast<uint32_t>(1 << pool.m_count)) {
                        break;
                    }
                }

                if (slot == best) {
                    pool.m_used &= ~bit;
                }
            }
        }
    } else if (src != dst) {
        memcpy(dst, src, srcSize);
    }

    *dstSize = bestSize;
    *checksum = !(bestType == COMPRESSION_ZLIB || bestType == COMPRESSION_BZIP2 || bestType == COMPRESSION_LZMA);

    return true;
}
