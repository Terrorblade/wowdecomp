#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "Engine/Source/Images/PngWriter.h"
#include "Engine/Source/Os/OsFile.h"

static const uint16_t s_lengthBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};

static const uint8_t s_lengthExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};

static const uint16_t s_distBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};

static const uint8_t s_distExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

static const uint8_t s_codeLengthOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static const uint32_t PNG_WINDOW_SIZE = 32768;
static const uint32_t PNG_WINDOW_MASK = PNG_WINDOW_SIZE - 1;
static const uint32_t PNG_HASH_BITS = 15;
static const uint32_t PNG_MAX_CHAIN = 48;
static const uint32_t PNG_NICE_LENGTH = 128;
static const uint32_t PNG_LAZY_LENGTH = 32;
static const uint32_t PNG_BLOCK_SYMBOLS = 32768;
static const uint32_t PNG_IDAT_SIZE = 1 << 20;

struct PngTables {
    uint32_t m_crc[256];
    uint8_t m_lengthCode[259];
    uint8_t m_distCode[PNG_WINDOW_SIZE + 1];

    PngTables() {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;

            for (uint32_t k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }

            this->m_crc[i] = c;
        }

        memset(this->m_lengthCode, 0, sizeof(this->m_lengthCode));

        for (uint32_t code = 0; code < 29; code++) {
            uint32_t end = code == 28 ? 259 : s_lengthBase[code] + (1u << s_lengthExtra[code]);

            for (uint32_t len = s_lengthBase[code]; len < end; len++) {
                this->m_lengthCode[len] = static_cast<uint8_t>(code);
            }
        }

        this->m_lengthCode[258] = 28;
        this->m_distCode[0] = 0;

        for (uint32_t code = 0; code < 30; code++) {
            uint32_t end = s_distBase[code] + (1u << s_distExtra[code]);

            for (uint32_t dist = s_distBase[code]; dist < end && dist <= PNG_WINDOW_SIZE; dist++) {
                this->m_distCode[dist] = static_cast<uint8_t>(code);
            }
        }
    }
};

static const PngTables& PngGetTables() {
    static const PngTables tables;
    return tables;
}

static uint32_t PngCrc(uint32_t crc, const uint8_t* data, size_t size) {
    const uint32_t* table = PngGetTables().m_crc;

    for (size_t i = 0; i < size; i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc;
}

static uint32_t PngAdler(const uint8_t* data, size_t size) {
    uint32_t a = 1;
    uint32_t b = 0;

    while (size) {
        size_t chunk = std::min<size_t>(size, 5552);
        size -= chunk;

        while (chunk--) {
            a += *data++;
            b += a;
        }

        a %= 65521;
        b %= 65521;
    }

    return (b << 16) | a;
}

static void PngBuildLengths(const uint32_t* freq, uint32_t count, uint32_t maxBits, uint8_t* lengths) {
    memset(lengths, 0, count);

    std::vector<uint32_t> symbols;

    for (uint32_t i = 0; i < count; i++) {
        if (freq[i]) {
            symbols.push_back(i);
        }
    }

    uint32_t leaves = static_cast<uint32_t>(symbols.size());

    if (!leaves) {
        return;
    }

    if (leaves == 1) {
        lengths[symbols[0]] = 1;
        return;
    }

    std::sort(symbols.begin(), symbols.end(), [freq](uint32_t a, uint32_t b) {
        return freq[a] != freq[b] ? freq[a] < freq[b] : a < b;
    });

    uint32_t nodes = leaves * 2 - 1;
    std::vector<uint64_t> weight(nodes);
    std::vector<uint32_t> parent(nodes, 0);

    for (uint32_t i = 0; i < leaves; i++) {
        weight[i] = freq[symbols[i]];
    }

    uint32_t leaf = 0;
    uint32_t inner = leaves;

    for (uint32_t next = leaves; next < nodes; next++) {
        uint32_t pick[2];

        for (uint32_t j = 0; j < 2; j++) {
            if (leaf < leaves && (inner >= next || weight[leaf] <= weight[inner])) {
                pick[j] = leaf++;
            } else {
                pick[j] = inner++;
            }
        }

        weight[next] = weight[pick[0]] + weight[pick[1]];
        parent[pick[0]] = next;
        parent[pick[1]] = next;
    }

    std::vector<uint32_t> depth(nodes, 0);

    for (uint32_t i = nodes - 1; i-- > 0;) {
        depth[i] = depth[parent[i]] + 1;
    }

    uint32_t lengthCount[16] = {};

    for (uint32_t i = 0; i < leaves; i++) {
        lengthCount[std::min(depth[i], maxBits)]++;
    }

    uint32_t total = 0;

    for (uint32_t bits = 1; bits <= maxBits; bits++) {
        total += lengthCount[bits] << (maxBits - bits);
    }

    while (total > (1u << maxBits)) {
        lengthCount[maxBits]--;

        for (uint32_t bits = maxBits - 1; bits > 0; bits--) {
            if (lengthCount[bits]) {
                lengthCount[bits]--;
                lengthCount[bits + 1] += 2;
                break;
            }
        }

        total--;
    }

    uint32_t index = 0;

    for (uint32_t bits = maxBits; bits > 0; bits--) {
        for (uint32_t i = 0; i < lengthCount[bits]; i++) {
            lengths[symbols[index++]] = static_cast<uint8_t>(bits);
        }
    }
}

static void PngBuildCodes(const uint8_t* lengths, uint32_t count, uint16_t* codes) {
    uint32_t lengthCount[16] = {};

    for (uint32_t i = 0; i < count; i++) {
        lengthCount[lengths[i]]++;
    }

    lengthCount[0] = 0;

    uint32_t nextCode[16] = {};
    uint32_t code = 0;

    for (uint32_t bits = 1; bits < 16; bits++) {
        code = (code + lengthCount[bits - 1]) << 1;
        nextCode[bits] = code;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint32_t len = lengths[i];
        codes[i] = 0;

        if (!len) {
            continue;
        }

        uint32_t value = nextCode[len]++;
        uint32_t reversed = 0;

        for (uint32_t k = 0; k < len; k++) {
            reversed = (reversed << 1) | ((value >> k) & 1);
        }

        codes[i] = static_cast<uint16_t>(reversed);
    }
}

static void PngEnsureTwoCodes(uint32_t* freq, uint32_t count) {
    uint32_t used = 0;

    for (uint32_t i = 0; i < count; i++) {
        used += freq[i] != 0;
    }

    for (uint32_t i = 0; i < count && used < 2; i++) {
        if (!freq[i]) {
            freq[i] = 1;
            used++;
        }
    }
}

class PngDeflater {
    public:
    PngDeflater(std::vector<uint8_t>& out);
    void Compress(const uint8_t* data, uint32_t size);

    private:
    struct Symbol {
        uint16_t m_value;
        uint16_t m_dist;
    };

    struct RunLength {
        uint8_t m_symbol;
        uint8_t m_extra;
    };

    void PutBits(uint32_t value, uint32_t count);
    void FlushBits();
    void Insert(uint32_t end);
    uint32_t FindMatch(uint32_t pos, uint32_t& dist);
    void Literal(uint8_t value);
    void Match(uint32_t length, uint32_t dist);
    void FlushBlock(bool final);

    const PngTables& m_tables;
    std::vector<uint8_t>& m_out;
    uint64_t m_bits;
    uint32_t m_bitCount;
    const uint8_t* m_data;
    uint32_t m_size;
    uint32_t m_inserted;
    std::vector<int32_t> m_head;
    std::vector<int32_t> m_prev;
    std::vector<Symbol> m_symbols;
    uint32_t m_litFreq[286];
    uint32_t m_distFreq[30];
};

PngDeflater::PngDeflater(std::vector<uint8_t>& out)
    : m_tables(PngGetTables())
    , m_out(out)
    , m_bits(0)
    , m_bitCount(0)
    , m_data(nullptr)
    , m_size(0)
    , m_inserted(0) {
    memset(this->m_litFreq, 0, sizeof(this->m_litFreq));
    memset(this->m_distFreq, 0, sizeof(this->m_distFreq));
}

void PngDeflater::PutBits(uint32_t value, uint32_t count) {
    this->m_bits |= static_cast<uint64_t>(value) << this->m_bitCount;
    this->m_bitCount += count;

    while (this->m_bitCount >= 8) {
        this->m_out.push_back(static_cast<uint8_t>(this->m_bits));
        this->m_bits >>= 8;
        this->m_bitCount -= 8;
    }
}

void PngDeflater::FlushBits() {
    if (this->m_bitCount) {
        this->m_out.push_back(static_cast<uint8_t>(this->m_bits));
    }

    this->m_bits = 0;
    this->m_bitCount = 0;
}

void PngDeflater::Insert(uint32_t end) {
    const uint8_t* data = this->m_data;

    while (this->m_inserted < end && this->m_inserted + 3 <= this->m_size) {
        uint32_t pos = this->m_inserted++;
        uint32_t value = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16);
        uint32_t hash = (value * 2654435761u) >> (32 - PNG_HASH_BITS);
        this->m_prev[pos & PNG_WINDOW_MASK] = this->m_head[hash];
        this->m_head[hash] = static_cast<int32_t>(pos);
    }

    this->m_inserted = std::max(this->m_inserted, end);
}

uint32_t PngDeflater::FindMatch(uint32_t pos, uint32_t& dist) {
    dist = 0;

    if (pos + 3 > this->m_size) {
        return 0;
    }

    const uint8_t* cur = this->m_data + pos;
    uint32_t maxLength = std::min<uint32_t>(258, this->m_size - pos);
    uint32_t value = cur[0] | (cur[1] << 8) | (cur[2] << 16);
    int32_t candidate = this->m_head[(value * 2654435761u) >> (32 - PNG_HASH_BITS)];
    uint32_t best = 2;

    for (uint32_t chain = PNG_MAX_CHAIN; candidate >= 0 && chain; chain--) {
        uint32_t distance = pos - static_cast<uint32_t>(candidate);

        if (!distance || distance > PNG_WINDOW_SIZE) {
            break;
        }

        const uint8_t* match = this->m_data + candidate;

        if (match[best] == cur[best] && match[0] == cur[0] && match[1] == cur[1]) {
            uint32_t length = 2;

            while (length < maxLength && match[length] == cur[length]) {
                length++;
            }

            if (length > best) {
                best = length;
                dist = distance;

                if (length >= PNG_NICE_LENGTH || length >= maxLength) {
                    break;
                }
            }
        }

        candidate = this->m_prev[candidate & PNG_WINDOW_MASK];
    }

    if (best < 3 || (best == 3 && dist > 4096)) {
        dist = 0;
        return 0;
    }

    return best;
}

void PngDeflater::Literal(uint8_t value) {
    this->m_symbols.push_back({ value, 0 });
    this->m_litFreq[value]++;
}

void PngDeflater::Match(uint32_t length, uint32_t dist) {
    this->m_symbols.push_back({ static_cast<uint16_t>(length), static_cast<uint16_t>(dist) });
    this->m_litFreq[257 + this->m_tables.m_lengthCode[length]]++;
    this->m_distFreq[this->m_tables.m_distCode[dist]]++;
}

void PngDeflater::FlushBlock(bool final) {
    this->m_litFreq[256] = 1;
    PngEnsureTwoCodes(this->m_litFreq, 286);
    PngEnsureTwoCodes(this->m_distFreq, 30);

    uint8_t litLengths[286];
    uint8_t distLengths[30];
    PngBuildLengths(this->m_litFreq, 286, 15, litLengths);
    PngBuildLengths(this->m_distFreq, 30, 15, distLengths);

    uint16_t litCodes[286];
    uint16_t distCodes[30];
    PngBuildCodes(litLengths, 286, litCodes);
    PngBuildCodes(distLengths, 30, distCodes);

    uint32_t litCount = 286;

    while (litCount > 257 && !litLengths[litCount - 1]) {
        litCount--;
    }

    uint32_t distCount = 30;

    while (distCount > 1 && !distLengths[distCount - 1]) {
        distCount--;
    }

    uint8_t lengths[316];
    memcpy(lengths, litLengths, litCount);
    memcpy(lengths + litCount, distLengths, distCount);

    uint32_t total = litCount + distCount;
    std::vector<RunLength> runs;

    for (uint32_t i = 0; i < total;) {
        uint8_t value = lengths[i];
        uint32_t run = 1;

        while (i + run < total && lengths[i + run] == value) {
            run++;
        }

        i += run;

        if (!value) {
            while (run >= 11) {
                uint32_t count = std::min<uint32_t>(run, 138);
                runs.push_back({ 18, static_cast<uint8_t>(count - 11) });
                run -= count;
            }

            if (run >= 3) {
                runs.push_back({ 17, static_cast<uint8_t>(run - 3) });
                run = 0;
            }
        } else {
            runs.push_back({ value, 0 });
            run--;

            while (run >= 3) {
                uint32_t count = std::min<uint32_t>(run, 6);
                runs.push_back({ 16, static_cast<uint8_t>(count - 3) });
                run -= count;
            }
        }

        for (; run; run--) {
            runs.push_back({ value, 0 });
        }
    }

    uint32_t clFreq[19] = {};

    for (const RunLength& entry : runs) {
        clFreq[entry.m_symbol]++;
    }

    PngEnsureTwoCodes(clFreq, 19);

    uint8_t clLengths[19];
    uint16_t clCodes[19];
    PngBuildLengths(clFreq, 19, 7, clLengths);
    PngBuildCodes(clLengths, 19, clCodes);

    uint32_t clCount = 19;

    while (clCount > 4 && !clLengths[s_codeLengthOrder[clCount - 1]]) {
        clCount--;
    }

    this->PutBits(final ? 1 : 0, 1);
    this->PutBits(2, 2);
    this->PutBits(litCount - 257, 5);
    this->PutBits(distCount - 1, 5);
    this->PutBits(clCount - 4, 4);

    for (uint32_t i = 0; i < clCount; i++) {
        this->PutBits(clLengths[s_codeLengthOrder[i]], 3);
    }

    for (const RunLength& entry : runs) {
        this->PutBits(clCodes[entry.m_symbol], clLengths[entry.m_symbol]);

        if (entry.m_symbol == 16) {
            this->PutBits(entry.m_extra, 2);
        } else if (entry.m_symbol == 17) {
            this->PutBits(entry.m_extra, 3);
        } else if (entry.m_symbol == 18) {
            this->PutBits(entry.m_extra, 7);
        }
    }

    for (const Symbol& symbol : this->m_symbols) {
        if (!symbol.m_dist) {
            this->PutBits(litCodes[symbol.m_value], litLengths[symbol.m_value]);
            continue;
        }

        uint32_t lengthCode = this->m_tables.m_lengthCode[symbol.m_value];
        this->PutBits(litCodes[257 + lengthCode], litLengths[257 + lengthCode]);

        if (s_lengthExtra[lengthCode]) {
            this->PutBits(symbol.m_value - s_lengthBase[lengthCode], s_lengthExtra[lengthCode]);
        }

        uint32_t distCode = this->m_tables.m_distCode[symbol.m_dist];
        this->PutBits(distCodes[distCode], distLengths[distCode]);

        if (s_distExtra[distCode]) {
            this->PutBits(symbol.m_dist - s_distBase[distCode], s_distExtra[distCode]);
        }
    }

    this->PutBits(litCodes[256], litLengths[256]);

    this->m_symbols.clear();
    memset(this->m_litFreq, 0, sizeof(this->m_litFreq));
    memset(this->m_distFreq, 0, sizeof(this->m_distFreq));
}

void PngDeflater::Compress(const uint8_t* data, uint32_t size) {
    this->m_data = data;
    this->m_size = size;
    this->m_inserted = 0;
    this->m_head.assign(1u << PNG_HASH_BITS, -1);
    this->m_prev.assign(PNG_WINDOW_SIZE, -1);
    this->m_symbols.reserve(PNG_BLOCK_SYMBOLS + 1);

    this->m_out.push_back(0x78);
    this->m_out.push_back(0x9C);

    uint32_t pos = 0;

    while (pos < size) {
        uint32_t dist;
        uint32_t length = this->FindMatch(pos, dist);
        this->Insert(pos + 1);

        if (length && length < PNG_LAZY_LENGTH && pos + 1 < size) {
            uint32_t nextDist;
            uint32_t nextLength = this->FindMatch(pos + 1, nextDist);

            if (nextLength > length) {
                this->Literal(data[pos]);
                pos++;
                this->Insert(pos + 1);
                length = nextLength;
                dist = nextDist;
            }
        }

        if (length) {
            this->Match(length, dist);
            this->Insert(pos + length);
            pos += length;
        } else {
            this->Literal(data[pos]);
            pos++;
        }

        if (this->m_symbols.size() >= PNG_BLOCK_SYMBOLS) {
            this->FlushBlock(false);
        }
    }

    this->FlushBlock(true);
    this->FlushBits();

    uint32_t adler = PngAdler(data, size);
    this->m_out.push_back(static_cast<uint8_t>(adler >> 24));
    this->m_out.push_back(static_cast<uint8_t>(adler >> 16));
    this->m_out.push_back(static_cast<uint8_t>(adler >> 8));
    this->m_out.push_back(static_cast<uint8_t>(adler));
}

static uint8_t PngPaeth(int32_t a, int32_t b, int32_t c) {
    int32_t p = a + b - c;
    int32_t pa = p > a ? p - a : a - p;
    int32_t pb = p > b ? p - b : b - p;
    int32_t pc = p > c ? p - c : c - p;

    if (pa <= pb && pa <= pc) {
        return static_cast<uint8_t>(a);
    }

    return static_cast<uint8_t>(pb <= pc ? b : c);
}

static uint32_t PngCost(uint8_t value) {
    return value < 128 ? value : 256 - value;
}

static void PngPutChunk(std::vector<uint8_t>& out, const char* type, const uint8_t* data, uint32_t size) {
    uint8_t header[8] = {
        static_cast<uint8_t>(size >> 24),
        static_cast<uint8_t>(size >> 16),
        static_cast<uint8_t>(size >> 8),
        static_cast<uint8_t>(size),
        static_cast<uint8_t>(type[0]),
        static_cast<uint8_t>(type[1]),
        static_cast<uint8_t>(type[2]),
        static_cast<uint8_t>(type[3]),
    };

    out.insert(out.end(), header, header + 8);

    if (size) {
        out.insert(out.end(), data, data + size);
    }

    uint32_t crc = PngCrc(0xFFFFFFFFu, header + 4, 4);
    crc = PngCrc(crc, data, size) ^ 0xFFFFFFFFu;
    out.push_back(static_cast<uint8_t>(crc >> 24));
    out.push_back(static_cast<uint8_t>(crc >> 16));
    out.push_back(static_cast<uint8_t>(crc >> 8));
    out.push_back(static_cast<uint8_t>(crc));
}

bool PngEncodeBgra(const void* pixels, uint32_t width, uint32_t height, std::vector<uint8_t>& out) {
    out.clear();

    if (!pixels || !width || !height) {
        return false;
    }

    uint64_t rowBytes = static_cast<uint64_t>(width) * 3;
    uint64_t totalBytes = (rowBytes + 1) * height;

    if (totalBytes > 0x7FFFFFFF) {
        return false;
    }

    std::vector<uint8_t> filtered(static_cast<size_t>(totalBytes));
    std::vector<uint8_t> prevRow(static_cast<size_t>(rowBytes), 0);
    std::vector<uint8_t> curRow(static_cast<size_t>(rowBytes));
    const uint8_t* source = static_cast<const uint8_t*>(pixels);
    uint8_t* dest = filtered.data();
    uint32_t bytes = static_cast<uint32_t>(rowBytes);

    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* row = source + static_cast<size_t>(y) * width * 4;

        for (uint32_t x = 0; x < width; x++) {
            curRow[x * 3 + 0] = row[x * 4 + 2];
            curRow[x * 3 + 1] = row[x * 4 + 1];
            curRow[x * 3 + 2] = row[x * 4 + 0];
        }

        const uint8_t* cur = curRow.data();
        const uint8_t* prev = prevRow.data();
        uint64_t cost[5] = {};

        for (uint32_t i = 0; i < bytes; i++) {
            uint8_t a = i >= 3 ? cur[i - 3] : 0;
            uint8_t b = prev[i];
            uint8_t c = i >= 3 ? prev[i - 3] : 0;
            uint8_t v = cur[i];
            cost[0] += PngCost(v);
            cost[1] += PngCost(static_cast<uint8_t>(v - a));
            cost[2] += PngCost(static_cast<uint8_t>(v - b));
            cost[3] += PngCost(static_cast<uint8_t>(v - ((a + b) >> 1)));
            cost[4] += PngCost(static_cast<uint8_t>(v - PngPaeth(a, b, c)));
        }

        uint32_t filter = 0;

        for (uint32_t f = 1; f < 5; f++) {
            if (cost[f] < cost[filter]) {
                filter = f;
            }
        }

        *dest++ = static_cast<uint8_t>(filter);

        for (uint32_t i = 0; i < bytes; i++) {
            uint8_t a = i >= 3 ? cur[i - 3] : 0;
            uint8_t b = prev[i];
            uint8_t c = i >= 3 ? prev[i - 3] : 0;
            uint8_t predictor = 0;

            switch (filter) {
                case 1:
                    predictor = a;
                    break;
                case 2:
                    predictor = b;
                    break;
                case 3:
                    predictor = static_cast<uint8_t>((a + b) >> 1);
                    break;
                case 4:
                    predictor = PngPaeth(a, b, c);
                    break;
                default:
                    break;
            }

            *dest++ = static_cast<uint8_t>(cur[i] - predictor);
        }

        prevRow.swap(curRow);
    }

    std::vector<uint8_t> stream;
    stream.reserve(filtered.size() / 2 + 1024);
    PngDeflater deflater(stream);
    deflater.Compress(filtered.data(), static_cast<uint32_t>(filtered.size()));
    filtered.clear();
    filtered.shrink_to_fit();

    static const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    out.reserve(stream.size() + stream.size() / PNG_IDAT_SIZE * 12 + 128);
    out.insert(out.end(), signature, signature + 8);

    uint8_t header[13] = {
        static_cast<uint8_t>(width >> 24),
        static_cast<uint8_t>(width >> 16),
        static_cast<uint8_t>(width >> 8),
        static_cast<uint8_t>(width),
        static_cast<uint8_t>(height >> 24),
        static_cast<uint8_t>(height >> 16),
        static_cast<uint8_t>(height >> 8),
        static_cast<uint8_t>(height),
        8,
        2,
        0,
        0,
        0,
    };

    PngPutChunk(out, "IHDR", header, sizeof(header));

    for (size_t offset = 0; offset < stream.size(); offset += PNG_IDAT_SIZE) {
        uint32_t size = static_cast<uint32_t>(std::min<size_t>(PNG_IDAT_SIZE, stream.size() - offset));
        PngPutChunk(out, "IDAT", stream.data() + offset, size);
    }

    PngPutChunk(out, "IEND", nullptr, 0);
    return true;
}

int32_t PngWriteBgra(const char* fileName, const void* pixels, uint32_t width, uint32_t height) {
    if (!fileName) {
        return 0;
    }

    std::vector<uint8_t> png;

    if (!PngEncodeBgra(pixels, width, height, png)) {
        return 0;
    }

    FILE* file = OsFileOpenStdio(fileName, "wb");

    if (!file) {
        return 0;
    }

    bool written = fwrite(png.data(), 1, png.size(), file) == png.size();
    bool closed = fclose(file) == 0;

    if (!written || !closed) {
        remove(fileName);
        return 0;
    }

    return 1;
}
