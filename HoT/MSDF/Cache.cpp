#include "HoT/MSDF/Cache.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include "HoT/MSDF/Manager.h"
#include "HoT/MSDF/Utils.h"
#include "HoT/Platform/Files.h"

namespace HoT::MSDF {

Cache::Cache(const uint8_t* fontData, size_t dataSize, const char* familyName, const char* styleName, uint32_t sdfRenderSize, uint32_t sdfSpread) {
    this->m_key.m_sdfRenderSize = sdfRenderSize;
    this->m_key.m_sdfSpread = sdfSpread;
    this->m_enabled = Manager::CacheAvailable();
    this->m_basePath = GetCacheBasePath(familyName, styleName, sdfRenderSize, sdfSpread);
    this->m_manifestPath = this->m_basePath / "manifest.dat";
    this->m_manifestLockPath = this->m_basePath / "manifest.lock";
    this->m_manifestJournalPath = this->m_basePath / "manifest.jrn";
    this->m_fontId = Manager::RegisterFont(HashFont(fontData, fontData ? dataSize : 0));

    if (this->m_enabled) {
        std::error_code ec;
        std::filesystem::create_directories(this->m_basePath, ec);
    }
}

Cache::~Cache() {
    if (!this->m_enabled) {
        return;
    }

    this->FlushPendingWrites();
    this->CleanupOrphans();
    Manager::FlushAll();
}

std::string Cache::SanitizeName(std::string_view name) {
    if (name.empty()) {
        return "unnamed";
    }

    std::string out;
    out.reserve(name.size());

    for (char c : name) {
        if (std::string_view("/:*?\"<>|\\").find(c) != std::string_view::npos || std::iscntrl(static_cast<unsigned char>(c))) {
            out.push_back('_');
        } else {
            out.push_back(c);
        }
    }

    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) {
        out.pop_back();
    }

    return out.empty() ? "unnamed" : out;
}

std::string Cache::GetCacheBasePath(const char* familyName, const char* styleName, uint32_t sdfRenderSize, uint32_t sdfSpread) {
    std::string family = SanitizeName(familyName ? familyName : "");
    std::string style = SanitizeName(styleName ? styleName : "");
    std::string folder = family + "_" + style + "_s" + std::to_string(sdfRenderSize) + "_sp" + std::to_string(sdfSpread);
    std::filesystem::path base = std::filesystem::current_path() / CACHE_DIR / folder;

    return base.string();
}

void Cache::BuildBlockLockPath(uint32_t blockId, std::filesystem::path& outPath) const {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "block_%u.lock", blockId);
    outPath = this->m_basePath / buffer;
}

void Cache::BuildBlockPath(uint32_t blockId, std::filesystem::path& outPath) const {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "block_%u.dat", blockId);
    outPath = this->m_basePath / buffer;
}

uint32_t Cache::GetBlockId(uint32_t codepoint) {
    return codepoint / BLOCK_SIZE;
}

bool Cache::TryLoadGlyph(uint32_t codepoint, GlyphMetrics& outMetrics) {
    if (!this->m_enabled) {
        return false;
    }

    auto manifestIt = this->m_manifest.find(codepoint);

    if (manifestIt == this->m_manifest.end()) {
        return false;
    }

    uint32_t blockId = manifestIt->second.m_blockId;
    auto wrapIt = this->m_blockWrap.find(blockId);

    if (wrapIt != this->m_blockWrap.end()) {
        return Manager::LoadGlyph(wrapIt->second, codepoint, outMetrics);
    }

    BlockWrap wrap;
    wrap.m_key = BlockKey(this->m_fontId, blockId);
    this->BuildBlockPath(blockId, wrap.m_path);
    this->m_blockWrap[blockId] = wrap;

    return Manager::LoadGlyph(wrap, codepoint, outMetrics);
}

bool Cache::StoreGlyph(GlyphToStore&& glyph) {
    if (!this->m_enabled) {
        return true;
    }

    if (!this->m_manifestLoaded && !this->LoadManifest()) {
        return false;
    }

    this->m_pendingWrites.push_back(std::move(glyph));

    if (this->m_pendingWrites.size() >= WRITE_BATCH_SIZE) {
        this->FlushPendingWrites();
    }

    return true;
}

bool Cache::LoadManifest() {
    this->m_manifestLoaded = true;

    ScopedFileLock lock;

    if (!lock.AcquireShared(this->m_manifestLockPath, 10000)) {
        return false;
    }

    std::error_code ec;
    bool pathExists = std::filesystem::exists(this->m_manifestPath, ec);
    bool journalExists = std::filesystem::exists(this->m_manifestJournalPath, ec);

    if (!pathExists && !journalExists) {
        return true;
    }

    if (pathExists) {
        uintmax_t fileSize = std::filesystem::file_size(this->m_manifestPath, ec);

        if (!ec && fileSize > sizeof(ManifestHeader) && fileSize < MAX_SAFE_ALLOCATION) {
            size_t estimated = static_cast<size_t>((fileSize - sizeof(ManifestHeader)) / sizeof(ManifestEntry));
            this->m_manifest.reserve(std::min<size_t>(estimated + estimated / 10, 0x110000));
        }

        if (!this->LoadManifestFromFile(this->m_manifestPath, this->m_manifest)) {
            this->m_manifest.clear();
        }
    }

    size_t applied = 0;
    LoadManifestJournal(this->m_manifestJournalPath, this->m_manifest, applied);

    return true;
}

bool Cache::LoadManifestFromFile(const std::filesystem::path& path, ManifestMap& outMap) const {
    std::error_code ec;
    uintmax_t fileSize = std::filesystem::file_size(path, ec);

    if (ec || fileSize < sizeof(ManifestHeader) || fileSize > MAX_SAFE_ALLOCATION) {
        return false;
    }

    FileGuard file(Os::OpenFile(path, Os::Access::ReadSequential));

    if (!file.IsValid()) {
        return false;
    }

    ViewGuard view(Os::MapReadOnly(file.m_handle, static_cast<size_t>(fileSize)), static_cast<size_t>(fileSize));

    if (!view.m_ptr) {
        return false;
    }

    const ManifestHeader* header = static_cast<const ManifestHeader*>(view.m_ptr);

    if (header->m_magic != MANIFEST_MAGIC || header->m_version != CACHE_VERSION || !(header->m_key == this->m_key)) {
        return false;
    }

    uint64_t expectedSize = sizeof(ManifestHeader) + static_cast<uint64_t>(header->m_entryCount) * sizeof(ManifestEntry);

    if (fileSize < expectedSize) {
        return false;
    }

    const ManifestEntry* entries = reinterpret_cast<const ManifestEntry*>(static_cast<const uint8_t*>(view.m_ptr) + sizeof(ManifestHeader));

    for (uint32_t i = 0; i < header->m_entryCount; i++) {
        outMap.try_emplace(entries[i].m_codepoint, entries[i]);
    }

    return true;
}

bool Cache::LoadManifestJournal(const std::filesystem::path& journalPath, ManifestMap& outMap, size_t& outEntriesApplied) {
    outEntriesApplied = 0;

    std::error_code ec;

    if (!std::filesystem::exists(journalPath, ec) || ec) {
        return true;
    }

    uintmax_t journalSize = std::filesystem::file_size(journalPath, ec);

    if (ec || journalSize == 0) {
        return !ec;
    }

    if (journalSize % sizeof(ManifestEntry) != 0 || journalSize > MAX_SAFE_ALLOCATION) {
        return false;
    }

    std::ifstream in(journalPath, std::ios::binary);

    if (!in.good()) {
        return false;
    }

    ManifestEntry entry;

    while (in.read(reinterpret_cast<char*>(&entry), sizeof(ManifestEntry))) {
        outMap[entry.m_codepoint] = entry;
        outEntriesApplied++;
    }

    return true;
}

bool Cache::AppendManifestJournal(const std::vector<ManifestEntry>& entries) {
    if (entries.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(this->m_basePath, ec);

    if (ec) {
        return false;
    }

    FileGuard file(Os::OpenFile(this->m_manifestJournalPath, Os::Access::Append));

    if (!file.IsValid() || entries.size() > UINT32_MAX / sizeof(ManifestEntry)) {
        return false;
    }

    uint32_t totalBytes = static_cast<uint32_t>(entries.size() * sizeof(ManifestEntry));
    std::vector<uint8_t> buffer = Pools::s_byte.AcquireSized(totalBytes);
    memcpy(buffer.data(), entries.data(), totalBytes);

    bool ok = Os::WriteAll(file.m_handle, buffer.data(), totalBytes);

    if (ok) {
        file.m_successful = true;
    }

    Pools::s_byte.Release(std::move(buffer));

    return ok;
}

bool Cache::SaveManifest(bool isLocked) {
    ScopedFileLock lock;

    if (!isLocked && !lock.AcquireExclusive(this->m_manifestLockPath, 1000)) {
        return false;
    }

    std::filesystem::path tmpManifest = this->m_manifestPath;
    tmpManifest.replace_extension(".tmp");

    FileGuard file(Os::OpenFile(tmpManifest, Os::Access::Create));

    if (!file.IsValid()) {
        return false;
    }

    ManifestHeader header = {};
    header.m_magic = MANIFEST_MAGIC;
    header.m_version = CACHE_VERSION;
    header.m_key = this->m_key;
    header.m_entryCount = static_cast<uint32_t>(this->m_manifest.size());
    header.m_pad = 0;

    if (!Os::WriteAll(file.m_handle, &header, sizeof(header))) {
        return false;
    }

    std::vector<ManifestEntry> entries = Pools::s_manifest.Acquire(this->m_manifest.size());

    FinalAction cleanup([&]() {
        if (!entries.empty()) {
            Pools::s_manifest.Release(std::move(entries));
        }
    });

    for (const auto& kv : this->m_manifest) {
        ManifestEntry entry;
        entry.m_codepoint = kv.first;
        entry.m_blockId = kv.second.m_blockId;
        entries.push_back(entry);
    }

    if (!entries.empty() && !Os::WriteAll(file.m_handle, entries.data(), entries.size() * sizeof(ManifestEntry))) {
        return false;
    }

    file.Close();

    if (Platform::RenameReplacing(tmpManifest, this->m_manifestPath)) {
        std::error_code ec;
        std::filesystem::remove(this->m_manifestJournalPath, ec);
        return true;
    }

    return false;
}

size_t Cache::GetManifestSize() {
    if (!this->m_enabled) {
        return 0;
    }

    if (!this->m_manifestLoaded) {
        this->LoadManifest();
    }

    return this->m_manifest.size();
}

bool Cache::FlushPendingWrites() {
    if (this->m_pendingWrites.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(this->m_basePath, ec);

    if (ec) {
        return false;
    }

    std::unordered_map<uint32_t, std::vector<GlyphToStore*>> byBlock;

    for (GlyphToStore& pending : this->m_pendingWrites) {
        byBlock[GetBlockId(pending.m_codepoint)].push_back(&pending);
    }

    std::vector<ManifestEntry> newEntries = Pools::s_manifest.Acquire(this->m_pendingWrites.size());

    size_t maxBlockSize = 0;

    for (const auto& kv : byBlock) {
        maxBlockSize = std::max(maxBlockSize, kv.second.size());
    }

    std::vector<ManifestEntry> blockEntries = Pools::s_manifest.Acquire(maxBlockSize);

    for (auto& kv : byBlock) {
        uint32_t blockId = kv.first;
        std::filesystem::path lockPath;
        this->BuildBlockLockPath(blockId, lockPath);

        ScopedFileLock lock;

        if (!lock.AcquireExclusive(lockPath, 1000)) {
            continue;
        }

        blockEntries.clear();

        if (!this->WriteBlockFile(blockId, kv.second, blockEntries)) {
            continue;
        }

        newEntries.insert(newEntries.end(), blockEntries.begin(), blockEntries.end());
    }

    this->m_pendingWrites.clear();

    if (!newEntries.empty()) {
        ScopedFileLock lock;

        if (lock.AcquireExclusive(this->m_manifestLockPath, 1000) && this->AppendManifestJournal(newEntries)) {
            for (const ManifestEntry& entry : newEntries) {
                this->m_manifest[entry.m_codepoint] = entry;
            }

            this->SaveManifest(true);
        }
    }

    Pools::s_manifest.Release(std::move(blockEntries));
    Pools::s_manifest.Release(std::move(newEntries));

    return true;
}

static GlyphEntry MakeEntry(const GlyphToStore* glyph) {
    GlyphEntry entry = {};
    entry.m_codepoint = glyph->m_codepoint;
    entry.m_width = glyph->m_width;
    entry.m_height = glyph->m_height;
    entry.m_bitmapTop = glyph->m_bitmapTop;
    entry.m_bitmapLeft = glyph->m_bitmapLeft;
    entry.m_dataOffset = 0;
    entry.m_dataSize = glyph->m_dataSize;

    return entry;
}

bool Cache::WriteBlockFile(uint32_t blockId, std::vector<GlyphToStore*>& pending, std::vector<ManifestEntry>& outEntries) {
    std::filesystem::path blockPath;
    this->BuildBlockPath(blockId, blockPath);

    std::sort(pending.begin(), pending.end(), [](const GlyphToStore* a, const GlyphToStore* b) {
        return a->m_codepoint < b->m_codepoint;
    });

    BlockWrap wrap;
    auto wrapIt = this->m_blockWrap.find(blockId);

    if (wrapIt != this->m_blockWrap.end()) {
        wrap = wrapIt->second;
    } else {
        wrap.m_key = BlockKey(this->m_fontId, blockId);
        wrap.m_path = blockPath;
        this->m_blockWrap[blockId] = wrap;
    }

    Manager::MappedBlock* cachedBlock = Manager::GetOrLoadMappedBlock(wrap);
    uint32_t oldCount = cachedBlock ? cachedBlock->m_entryCount : 0;

    std::vector<GlyphEntry> merged = Pools::s_glyph.Acquire(oldCount + pending.size());
    std::vector<uint32_t> hashTable = Pools::s_uint32.AcquireSized(BLOCK_SIZE, 0xFFFFFFFF);
    std::vector<uint8_t> payload;

    FinalAction cleanup([&]() {
        if (!hashTable.empty()) {
            Pools::s_uint32.Release(std::move(hashTable));
        }

        if (!merged.empty()) {
            Pools::s_glyph.Release(std::move(merged));
        }

        if (!payload.empty()) {
            Pools::s_byte.Release(std::move(payload));
        }
    });

    uint32_t oldIndex = 0;
    auto pendingIt = pending.begin();

    while (oldIndex < oldCount || pendingIt != pending.end()) {
        if (oldIndex < oldCount && (pendingIt == pending.end() || cachedBlock->m_entries[oldIndex].m_codepoint < (*pendingIt)->m_codepoint)) {
            merged.push_back(cachedBlock->m_entries[oldIndex++]);
        } else if (pendingIt != pending.end() && (oldIndex == oldCount || (*pendingIt)->m_codepoint < cachedBlock->m_entries[oldIndex].m_codepoint)) {
            merged.push_back(MakeEntry(*pendingIt++));
        } else {
            merged.push_back(MakeEntry(*pendingIt++));
            oldIndex++;
        }
    }

    if (merged.size() > BLOCK_SIZE) {
        return false;
    }

    uint32_t totalPayload = 0;

    for (GlyphEntry& entry : merged) {
        entry.m_dataOffset = totalPayload;
        totalPayload += entry.m_dataSize;
    }

    for (size_t i = 0; i < merged.size(); i++) {
        hashTable[merged[i].m_codepoint & (BLOCK_SIZE - 1)] = static_cast<uint32_t>(i);
    }

    payload = Pools::s_byte.AcquireSized(totalPayload);

    auto copyIt = pending.begin();

    for (const GlyphEntry& entry : merged) {
        const uint8_t* src = nullptr;

        while (copyIt != pending.end() && (*copyIt)->m_codepoint < entry.m_codepoint) {
            ++copyIt;
        }

        if (copyIt != pending.end() && (*copyIt)->m_codepoint == entry.m_codepoint) {
            src = (*copyIt)->m_pixels.data();
        } else if (cachedBlock) {
            uint32_t oldEntryIndex = cachedBlock->m_hashTable[entry.m_codepoint & (BLOCK_SIZE - 1)];

            if (oldEntryIndex != 0xFFFFFFFF && oldEntryIndex < cachedBlock->m_entryCount && cachedBlock->m_entries[oldEntryIndex].m_codepoint == entry.m_codepoint) {
                src = cachedBlock->m_payload + cachedBlock->m_entries[oldEntryIndex].m_dataOffset;
            }
        }

        if (src && entry.m_dataSize > 0) {
            memcpy(payload.data() + entry.m_dataOffset, src, entry.m_dataSize);
        }
    }

    Manager::FreeBlockByKey(wrap.m_key);

    std::filesystem::path tmpPath = blockPath;
    tmpPath.replace_extension(".tmp");

    {
        FileGuard tmpFile(Os::OpenFile(tmpPath, Os::Access::CreateSequential));

        if (!tmpFile.IsValid()) {
            return false;
        }

        BlockFileHeader header = {};
        header.m_magic = BLOCK_MAGIC;
        header.m_version = CACHE_VERSION;
        header.m_blockId = blockId;
        header.m_entryCount = static_cast<uint32_t>(merged.size());

        size_t dataSize = sizeof(header) + merged.size() * sizeof(GlyphEntry) + BLOCK_SIZE * sizeof(uint32_t) + payload.size();
        const size_t align = Manager::AllocationGranularity();
        size_t paddedSize = ((dataSize + align - 1) / align) * align;
        size_t padding = paddedSize - dataSize;

        if (padding > 0) {
            payload.resize(payload.size() + padding, 0);
        }

        if (!Os::WriteAll(tmpFile.m_handle, &header, sizeof(header)) || !Os::WriteAll(tmpFile.m_handle, merged.data(), merged.size() * sizeof(GlyphEntry)) || !Os::WriteAll(tmpFile.m_handle, hashTable.data(), BLOCK_SIZE * sizeof(uint32_t)) || !Os::WriteAll(tmpFile.m_handle, payload.data(), payload.size())) {
            return false;
        }
    }

    if (!Platform::RenameReplacing(tmpPath, blockPath)) {
        std::filesystem::path oldPath = blockPath;
        oldPath.replace_extension(".old");

        std::error_code ec;

        if (!Platform::RenameReplacing(tmpPath, oldPath) && !std::filesystem::exists(oldPath, ec)) {
            return false;
        }

        auto it = this->m_blockWrap.find(blockId);

        if (it != this->m_blockWrap.end()) {
            it->second.m_path = oldPath;
        }
    }

    for (GlyphToStore* glyph : pending) {
        ManifestEntry entry;
        entry.m_codepoint = glyph->m_codepoint;
        entry.m_blockId = blockId;
        outEntries.push_back(entry);
    }

    return true;
}

void Cache::CleanupOrphans() const {
    std::error_code ec;

    if (!std::filesystem::exists(this->m_basePath, ec)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(this->m_basePath, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string ext = entry.path().extension().string();

        if (ext == ".old" || ext == ".tmp") {
            std::filesystem::remove(entry.path(), ec);
        }
    }
}

}
