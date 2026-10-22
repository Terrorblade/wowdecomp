#include "HoT/MSDF/Manager.h"

namespace HoT::MSDF {

void Manager::MappedBlock::Close() {
    this->m_view.Close();
    this->m_mapping.Close();
    this->m_file.Close();
    this->m_header = nullptr;
    this->m_entries = nullptr;
    this->m_hashTable = nullptr;
    this->m_payload = nullptr;
    this->m_entryCount = 0;
}

void Manager::MappedBlock::Reset() {
    this->Close();
    this->m_slotIndex = 0xFFFFFFFF;
    this->m_key = BlockKey();
}

Manager::ArenaState::ArenaState() {
    this->m_slotAddresses.fill(nullptr);
    this->m_slotToBlockIndex.fill(0xFFFFFFFF);

    if (!Os::ArenaSupported()) {
        return;
    }

    const uint32_t maxGlyphDim = SDF_RENDER_SIZE + 2 * SDF_SPREAD;
    const uint32_t maxBytesPerGlyph = maxGlyphDim * maxGlyphDim * 4;
    const size_t maxBlockSize = sizeof(BlockFileHeader) + BLOCK_SIZE * sizeof(GlyphEntry) + BLOCK_SIZE * sizeof(uint32_t) + BLOCK_SIZE * maxBytesPerGlyph;

    const size_t granularity = AllocationGranularity();
    this->m_slotSize = ((maxBlockSize + granularity - 1) / granularity) * granularity;

    this->m_base = Os::ReserveArena(this->m_slotSize, MAX_ARENA_SLOTS);

    if (!this->m_base) {
        return;
    }

    for (size_t i = 0; i < MAX_ARENA_SLOTS; i++) {
        this->m_slotAddresses[i] = static_cast<char*>(this->m_base) + i * this->m_slotSize;
    }
}

Manager::ArenaState::~ArenaState() {
    if (this->m_base) {
        Os::ReleaseArena(this->m_base, this->m_slotSize * MAX_ARENA_SLOTS);
        this->m_base = nullptr;
    }
}

void* Manager::ArenaState::GetFreeSlot(uint32_t blockIndex, uint32_t& outSlotIndex) {
    if (!this->m_base || this->m_freeMask == 0) {
        return nullptr;
    }

    uint32_t slotIndex = 0;

    while (!(this->m_freeMask & (1ULL << slotIndex))) {
        slotIndex++;
    }

    this->m_freeMask &= ~(1ULL << slotIndex);
    this->m_slotToBlockIndex[slotIndex] = blockIndex;
    outSlotIndex = slotIndex;

    return this->m_slotAddresses[slotIndex];
}

void Manager::ArenaState::FreeSlot(uint32_t slotIndex) {
    if (!this->m_base || slotIndex >= MAX_ARENA_SLOTS || !this->IsSlotOccupied(slotIndex)) {
        return;
    }

    Os::ResetSlot(this->m_slotAddresses[slotIndex], this->m_slotSize);

    this->m_freeMask |= 1ULL << slotIndex;
    this->m_slotToBlockIndex[slotIndex] = 0xFFFFFFFF;
}

void Manager::ArenaState::FlushAll() {
    for (uint32_t i = 0; i < MAX_ARENA_SLOTS; i++) {
        if (this->IsSlotOccupied(i)) {
            this->FreeSlot(i);
        }
    }
}

Manager::ArenaState& Manager::Arena() {
    static ArenaState s_arena;
    return s_arena;
}

bool Manager::CacheAvailable() {
    return Arena().m_base != nullptr;
}

size_t Manager::AllocationGranularity() {
    return Os::AllocationGranularity();
}

void Manager::FreeBlock(uint32_t blockIndex) {
    if (blockIndex >= MAX_ARENA_SLOTS) {
        return;
    }

    MappedBlock& block = s_mappedBlocks[blockIndex];
    BlockKey key = block.m_key;

    if (block.m_slotIndex != 0xFFFFFFFF) {
        Arena().FreeSlot(block.m_slotIndex);
    }

    block.Reset();
    s_blockCache.erase(key);

    if (s_lastBlockIndex == blockIndex) {
        s_lastBlockIndex = 0xFFFFFFFF;
        s_lastBlockKey = BlockKey();
    }
}

void Manager::FreeBlockByKey(BlockKey key) {
    auto it = s_blockCache.find(key);

    if (it != s_blockCache.end()) {
        FreeBlock(it->second);
    }
}

void Manager::FlushAll() {
    for (MappedBlock& block : s_mappedBlocks) {
        block.Reset();
    }

    s_blockCache.clear();
    Arena().FlushAll();

    s_lastBlockIndex = 0xFFFFFFFF;
    s_lastBlockKey = BlockKey();
}

uint32_t Manager::RegisterFont(FontHash hash) {
    auto it = s_fontHashToId.find(hash);

    if (it != s_fontHashToId.end()) {
        return it->second;
    }

    uint32_t fontId = s_nextFontId++;
    s_fontHashToId.emplace(hash, fontId);

    return fontId;
}

bool Manager::LoadMappedBlock(const BlockWrap& wrap, MappedBlock& outBlock, void* slotAddr, uint32_t slotIndex) {
    ArenaState& arena = Arena();

    outBlock.m_file.m_handle = Os::OpenFile(wrap.m_path, Os::Access::ReadRandom);

    if (!outBlock.m_file.IsValid()) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    // Mapping past EOF is refused on Windows and faults on first touch elsewhere, so require at least the header up
    // front (the remaining reads are bounded by m_fileSize below).
    if (!Os::FileSize(outBlock.m_file.m_handle, outBlock.m_fileSize) || outBlock.m_fileSize < sizeof(BlockFileHeader)) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    const size_t granularity = AllocationGranularity();
    uint64_t splitSize = ((outBlock.m_fileSize + granularity - 1) / granularity) * granularity;

    if (splitSize > arena.m_slotSize) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_slotIndex = slotIndex;
    outBlock.m_view.m_ptr = Os::MapIntoSlot(outBlock.m_file.m_handle, slotAddr, static_cast<size_t>(splitSize), outBlock.m_mapping.m_handle);

    if (!outBlock.m_view.m_ptr) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_view.m_size = static_cast<size_t>(splitSize);
    outBlock.m_view.m_placeholder = true;

    const uint8_t* base = static_cast<const uint8_t*>(outBlock.m_view.m_ptr);
    outBlock.m_header = reinterpret_cast<const BlockFileHeader*>(base);

    if (outBlock.m_header->m_magic != BLOCK_MAGIC || outBlock.m_header->m_version != CACHE_VERSION || outBlock.m_header->m_blockId != wrap.m_key.m_blockId || outBlock.m_header->m_entryCount > BLOCK_SIZE) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_entryCount = outBlock.m_header->m_entryCount;
    outBlock.m_entries = reinterpret_cast<const GlyphEntry*>(base + sizeof(BlockFileHeader));

    size_t hashTableOffset = sizeof(BlockFileHeader) + static_cast<size_t>(outBlock.m_entryCount) * sizeof(GlyphEntry);
    outBlock.m_hashTable = reinterpret_cast<const uint32_t*>(base + hashTableOffset);

    size_t payloadOffset = hashTableOffset + BLOCK_SIZE * sizeof(uint32_t);

    if (outBlock.m_fileSize < payloadOffset) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_payload = base + payloadOffset;

    size_t maxPayload = static_cast<size_t>(outBlock.m_fileSize - payloadOffset);

    for (uint32_t i = 0; i < outBlock.m_entryCount; i++) {
        const GlyphEntry& entry = outBlock.m_entries[i];

        if (entry.m_dataSize > 0 && static_cast<size_t>(entry.m_dataOffset) + entry.m_dataSize > maxPayload) {
            arena.FreeSlot(slotIndex);
            return false;
        }
    }

    outBlock.m_key = wrap.m_key;

    return true;
}

Manager::MappedBlock* Manager::GetOrLoadMappedBlock(const BlockWrap& wrap) {
    if (s_lastBlockIndex != 0xFFFFFFFF && s_lastBlockKey == wrap.m_key) {
        return &s_mappedBlocks[s_lastBlockIndex];
    }

    auto it = s_blockCache.find(wrap.m_key);

    if (it != s_blockCache.end()) {
        s_lastBlockIndex = it->second;
        s_lastBlockKey = wrap.m_key;
        return &s_mappedBlocks[it->second];
    }

    ArenaState& arena = Arena();

    if (arena.m_freeMask == 0) {
        FlushAll();
    }

    uint32_t slotIndex = 0;
    void* slotAddr = arena.GetFreeSlot(wrap.m_key.m_blockId, slotIndex);

    if (!slotAddr) {
        return nullptr;
    }

    MappedBlock& block = s_mappedBlocks[slotIndex];

    if (!LoadMappedBlock(wrap, block, slotAddr, slotIndex)) {
        // The slot is already freed, close the handles a failed map left open.
        block.m_view.m_ptr = nullptr;
        block.Reset();
        return nullptr;
    }

    s_lastBlockIndex = slotIndex;
    s_lastBlockKey = wrap.m_key;
    s_blockCache[wrap.m_key] = slotIndex;

    return &block;
}

bool Manager::LoadGlyph(const BlockWrap& wrap, uint32_t codepoint, GlyphMetrics& outMetrics) {
    MappedBlock* block = GetOrLoadMappedBlock(wrap);

    if (!block) {
        return false;
    }

    uint32_t entryIndex = block->m_hashTable[codepoint & (BLOCK_SIZE - 1)];

    if (entryIndex == 0xFFFFFFFF || entryIndex >= block->m_entryCount) {
        return false;
    }

    const GlyphEntry& entry = block->m_entries[entryIndex];

    if (entry.m_codepoint != codepoint) {
        return false;
    }

    outMetrics.m_width = entry.m_width;
    outMetrics.m_height = entry.m_height;
    outMetrics.m_bitmapTop = entry.m_bitmapTop;
    outMetrics.m_bitmapLeft = entry.m_bitmapLeft;
    outMetrics.m_pixels = entry.m_dataSize > 0 ? block->m_payload + entry.m_dataOffset : nullptr;

    return true;
}

}
