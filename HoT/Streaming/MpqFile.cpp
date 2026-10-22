#include "HoT/Streaming/MpqFile.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SComp.h"
#include "HoT/Platform/Files.h"
#include "HoT/Streaming/ArchiveManifest.h"

namespace HoT::Streaming::MpqFile {

static constexpr uint32_t s_magic = 0x1A51504D;
static constexpr uint32_t s_hashEmpty = 0xFFFFFFFF;
static constexpr uint32_t s_hashDeleted = 0xFFFFFFFE;
static constexpr uint32_t s_maxHashTableSize = 0x80000;
static constexpr long long s_headerScanLimit = 1 << 20;

static constexpr uint32_t s_flagImplode = 0x00000100;
static constexpr uint32_t s_flagCompress = 0x00000200;
static constexpr uint32_t s_flagEncrypted = 0x00010000;
static constexpr uint32_t s_flagFixKey = 0x00020000;
static constexpr uint32_t s_flagPatch = 0x00100000;
static constexpr uint32_t s_flagSingleUnit = 0x01000000;
static constexpr uint32_t s_flagDeleteMarker = 0x02000000;
static constexpr uint32_t s_flagSectorCrc = 0x04000000;
static constexpr uint32_t s_flagExists = 0x80000000;

#pragma pack(push, 1)
struct MpqHeader {
    /* 0000 */ uint32_t m_magic;
    /* 0004 */ uint32_t m_headerSize;
    /* 0008 */ uint32_t m_archiveSize;
    /* 000C */ uint16_t m_formatVersion;
    /* 000E */ uint16_t m_sectorShift;
    /* 0010 */ uint32_t m_hashTablePos;
    /* 0014 */ uint32_t m_blockTablePos;
    /* 0018 */ uint32_t m_hashTableSize;
    /* 001C */ uint32_t m_blockTableSize;
};

struct MpqHeaderV2 {
    /* 0000 */ uint64_t m_hiBlockTablePos;
    /* 0008 */ uint16_t m_hashTableHi;
    /* 000A */ uint16_t m_blockTableHi;
};

struct MpqHashEntry {
    /* 0000 */ uint32_t m_name1;
    /* 0004 */ uint32_t m_name2;
    /* 0008 */ uint16_t m_locale;
    /* 000A */ uint16_t m_platform;
    /* 000C */ uint32_t m_blockIndex;
};

struct MpqBlockEntry {
    /* 0000 */ uint32_t m_filePos;
    /* 0004 */ uint32_t m_compSize;
    /* 0008 */ uint32_t m_fileSize;
    /* 000C */ uint32_t m_flags;
};
#pragma pack(pop)

struct CryptTable {
    uint32_t m_v[0x500];

    CryptTable() {
        uint32_t seed = 0x00100001;

        for (uint32_t index1 = 0; index1 < 0x100; index1++) {
            for (uint32_t index2 = index1, i = 0; i < 5; i++, index2 += 0x100) {
                seed = (seed * 125 + 3) % 0x2AAAAB;
                uint32_t temp1 = (seed & 0xFFFF) << 0x10;
                seed = (seed * 125 + 3) % 0x2AAAAB;
                uint32_t temp2 = (seed & 0xFFFF);
                this->m_v[index2] = temp1 | temp2;
            }
        }
    }
};

static const CryptTable& Crypt() {
    static CryptTable table;
    return table;
}

static uint32_t HashString(const char* s, uint32_t type) {
    const CryptTable& t = Crypt();
    uint32_t seed1 = 0x7FED7FED;
    uint32_t seed2 = 0xEEEEEEEE;

    for (; *s; s++) {
        uint32_t ch = static_cast<unsigned char>(*s);

        if (ch == '/') {
            ch = '\\';
        }

        if (ch >= 'a' && ch <= 'z') {
            ch -= 32;
        }

        seed1 = t.m_v[(type << 8) + ch] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }

    return seed1;
}

static void Decrypt(uint32_t* data, size_t count, uint32_t key) {
    const CryptTable& t = Crypt();
    uint32_t seed = 0xEEEEEEEE;

    for (size_t i = 0; i < count; i++) {
        seed += t.m_v[0x400 + (key & 0xFF)];
        uint32_t ch = data[i] ^ (key + seed);
        key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
        seed = ch + seed + (seed << 5) + 3;
        data[i] = ch;
    }
}

static const char* PlainName(const char* name) {
    const char* plain = name;

    for (const char* p = name; *p; p++) {
        if (*p == '\\' || *p == '/') {
            plain = p + 1;
        }
    }

    return plain;
}

struct Reader {
    Platform::ReadOnlyFile m_file;
    long long m_size = 0;
    bool* m_unreadable = nullptr;

    bool Broken() {
        if (this->m_unreadable) {
            *this->m_unreadable = true;
        }

        return false;
    }

    bool Open(const std::wstring& path) {
        if (!this->m_file.Open(std::filesystem::path(path))) {
            return this->Broken();
        }

        this->m_size = this->m_file.Size();
        return true;
    }

    bool ReadAt(long long offset, void* out, size_t n) {
        if (offset < 0 || n > 0x7FFFFFFF || offset + static_cast<long long>(n) > this->m_size) {
            return false;
        }

        uint8_t* dst = static_cast<uint8_t*>(out);

        while (n > 0) {
            int64_t got = this->m_file.ReadAt(offset, dst, n);

            if (got <= 0) {
                return this->Broken();
            }

            dst += got;
            offset += got;
            n -= static_cast<size_t>(got);
        }

        return true;
    }
};

static bool Fail(std::string* error, const char* what) {
    if (error) {
        *error = what;
    }

    return false;
}

static bool DecompressSector(void* out, uint32_t* inoutSize, const uint8_t* in, uint32_t inSize) {
    return System_SComp::Decompress(out, inoutSize, in, inSize);
}

bool ReadFile(const std::wstring& archivePath, const char* name, std::string& out, std::string* error, bool* unreadable) {
    out.clear();

    if (unreadable) {
        *unreadable = false;
    }

    Reader r;
    r.m_unreadable = unreadable;

    if (!r.Open(archivePath)) {
        return Fail(error, "cannot open archive");
    }

    MpqHeader hdr{};
    long long headerOff = -1;

    for (long long off = 0; off + static_cast<long long>(sizeof(MpqHeader)) <= r.m_size && off < s_headerScanLimit; off += 0x200) {
        if (!r.ReadAt(off, &hdr, sizeof(hdr))) {
            return Fail(error, "cannot read header");
        }

        if (hdr.m_magic == s_magic) {
            headerOff = off;
            break;
        }
    }

    if (headerOff < 0) {
        return Fail(error, "no MPQ header");
    }

    if (hdr.m_formatVersion > 1) {
        return Fail(error, "unsupported MPQ format version");
    }

    if (hdr.m_hashTableSize == 0 || hdr.m_hashTableSize > s_maxHashTableSize || (hdr.m_hashTableSize & (hdr.m_hashTableSize - 1)) != 0) {
        return Fail(error, "bad hash table size");
    }

    if (hdr.m_sectorShift > 16) {
        return Fail(error, "bad sector size");
    }

    long long hashPos = headerOff + hdr.m_hashTablePos;
    long long blockPos = headerOff + hdr.m_blockTablePos;
    MpqHeaderV2 v2{};

    if (hdr.m_formatVersion == 1) {
        if (!r.ReadAt(headerOff + sizeof(MpqHeader), &v2, sizeof(v2))) {
            return Fail(error, "cannot read extended header");
        }

        hashPos += static_cast<long long>(v2.m_hashTableHi) << 32;
        blockPos += static_cast<long long>(v2.m_blockTableHi) << 32;
    }

    const size_t hashBytes = static_cast<size_t>(hdr.m_hashTableSize) * sizeof(MpqHashEntry);
    const size_t blockBytes = static_cast<size_t>(hdr.m_blockTableSize) * sizeof(MpqBlockEntry);

    if (hashPos + static_cast<long long>(hashBytes) > r.m_size || blockPos + static_cast<long long>(blockBytes) > r.m_size) {
        return Fail(error, "tables lie outside the file");
    }

    std::vector<MpqHashEntry> hashes(hdr.m_hashTableSize);

    if (!r.ReadAt(hashPos, hashes.data(), hashBytes)) {
        return Fail(error, "cannot read hash table");
    }

    Decrypt(reinterpret_cast<uint32_t*>(hashes.data()), hashBytes / 4, HashString("(hash table)", 3));

    const uint32_t mask = hdr.m_hashTableSize - 1;
    const uint32_t start = HashString(name, 0) & mask;
    const uint32_t nameA = HashString(name, 1);
    const uint32_t nameB = HashString(name, 2);
    uint32_t blockIndex = s_hashEmpty;

    for (uint32_t i = 0; i < hdr.m_hashTableSize; i++) {
        const MpqHashEntry& e = hashes[(start + i) & mask];

        if (e.m_blockIndex == s_hashEmpty) {
            break;
        }

        if (e.m_blockIndex == s_hashDeleted) {
            continue;
        }

        if (e.m_name1 == nameA && e.m_name2 == nameB) {
            blockIndex = e.m_blockIndex;

            if (e.m_locale == 0) {
                break;
            }
        }
    }

    if (blockIndex == s_hashEmpty) {
        return Fail(error, "file not in archive");
    }

    if (blockIndex >= hdr.m_blockTableSize) {
        return Fail(error, "block index out of range");
    }

    std::vector<MpqBlockEntry> blocks(hdr.m_blockTableSize);

    if (!r.ReadAt(blockPos, blocks.data(), blockBytes)) {
        return Fail(error, "cannot read block table");
    }

    Decrypt(reinterpret_cast<uint32_t*>(blocks.data()), blockBytes / 4, HashString("(block table)", 3));

    const MpqBlockEntry b = blocks[blockIndex];

    if (!(b.m_flags & s_flagExists) || (b.m_flags & s_flagDeleteMarker)) {
        return Fail(error, "file not in archive");
    }

    if (b.m_flags & (s_flagPatch | s_flagImplode)) {
        return Fail(error, "unsupported file storage");
    }

    long long filePos = headerOff + b.m_filePos;

    if (hdr.m_formatVersion == 1 && v2.m_hiBlockTablePos != 0) {
        uint16_t hi = 0;

        if (!r.ReadAt(headerOff + static_cast<long long>(v2.m_hiBlockTablePos) + static_cast<long long>(blockIndex) * 2, &hi, sizeof(hi))) {
            return Fail(error, "cannot read hi-block table");
        }

        filePos += static_cast<long long>(hi) << 32;
    }

    if (filePos + static_cast<long long>(b.m_compSize) > r.m_size) {
        return Fail(error, "file data lies outside the archive");
    }

    if (b.m_fileSize == 0) {
        return true;
    }

    std::vector<uint8_t> comp(b.m_compSize);

    if (!r.ReadAt(filePos, comp.data(), b.m_compSize)) {
        return Fail(error, "cannot read file data");
    }

    const bool encrypted = (b.m_flags & s_flagEncrypted) != 0;
    const bool compressed = (b.m_flags & s_flagCompress) != 0;
    uint32_t key = 0;

    if (encrypted) {
        key = HashString(PlainName(name), 3);

        if (b.m_flags & s_flagFixKey) {
            key = (key + b.m_filePos) ^ b.m_fileSize;
        }
    }

    out.resize(b.m_fileSize);
    uint8_t* dst = reinterpret_cast<uint8_t*>(out.data());

    if (b.m_flags & s_flagSingleUnit) {
        if (encrypted) {
            Decrypt(reinterpret_cast<uint32_t*>(comp.data()), comp.size() / 4, key);
        }

        if (compressed) {
            uint32_t outSize = b.m_fileSize;

            if (!DecompressSector(dst, &outSize, comp.data(), b.m_compSize) || outSize != b.m_fileSize) {
                return Fail(error, "cannot decompress file");
            }
        } else {
            if (b.m_compSize < b.m_fileSize) {
                return Fail(error, "file data is truncated");
            }

            std::memcpy(dst, comp.data(), b.m_fileSize);
        }

        return true;
    }

    const uint32_t sectorSize = 0x200u << hdr.m_sectorShift;
    const uint32_t sectors = static_cast<uint32_t>((static_cast<uint64_t>(b.m_fileSize) + sectorSize - 1) / sectorSize);

    if (!compressed) {
        if (b.m_compSize < b.m_fileSize) {
            return Fail(error, "file data is truncated");
        }

        std::memcpy(dst, comp.data(), b.m_fileSize);

        if (encrypted) {
            for (uint32_t s = 0; s < sectors; s++) {
                uint32_t off = s * sectorSize;
                uint32_t n = std::min(sectorSize, b.m_fileSize - off);
                Decrypt(reinterpret_cast<uint32_t*>(dst + off), n / 4, key + s);
            }
        }

        return true;
    }

    const uint32_t offCount = sectors + 1 + ((b.m_flags & s_flagSectorCrc) ? 1 : 0);

    if (static_cast<uint64_t>(offCount) * 4 > b.m_compSize) {
        return Fail(error, "sector table is truncated");
    }

    std::vector<uint32_t> offs(offCount);
    std::memcpy(offs.data(), comp.data(), static_cast<size_t>(offCount) * 4);

    if (encrypted) {
        Decrypt(offs.data(), offCount, key - 1);
    }

    for (uint32_t s = 0; s < sectors; s++) {
        uint32_t begin = offs[s];
        uint32_t end = offs[s + 1];

        if (begin > end || end > b.m_compSize) {
            return Fail(error, "sector table is corrupt");
        }

        uint32_t rawOff = s * sectorSize;
        uint32_t rawSize = std::min(sectorSize, b.m_fileSize - rawOff);
        uint32_t compLen = end - begin;
        uint8_t* in = comp.data() + begin;

        if (encrypted) {
            Decrypt(reinterpret_cast<uint32_t*>(in), compLen / 4, key + s);
        }

        uint32_t outSize = rawSize;

        if (!DecompressSector(dst + rawOff, &outSize, in, compLen) || outSize != rawSize) {
            return Fail(error, "cannot decompress sector");
        }
    }

    return true;
}

bool ContentId(const std::wstring& archivePath, std::string& out) {
    out.clear();
    std::string bytes;
    bool unreadable = false;

    if (!ReadFile(archivePath, ArchiveManifest::s_fileName, bytes, nullptr, &unreadable)) {
        return !unreadable;
    }

    if (!bytes.empty()) {
        out = ArchiveManifest::ContentIdOf(bytes);
    }

    return true;
}

}
