#ifndef HOT_MSDF_MANAGER_H
#define HOT_MSDF_MANAGER_H

#include <array>
#include <cstdint>
#include <unordered_map>
#include "HoT/MSDF/Types.h"
#include "HoT/MSDF/Utils.h"

namespace HoT::MSDF {

// Maps cache block files read only into a reserved placeholder arena so a glyph read is a pointer into the file.
class Manager {
    public:
    struct MappedBlock {
        FileGuard m_file;
        MappingGuard m_mapping;
        ViewGuard m_view;
        uint64_t m_fileSize = 0;
        const BlockFileHeader* m_header = nullptr;
        const GlyphEntry* m_entries = nullptr;
        const uint32_t* m_hashTable = nullptr;
        const uint8_t* m_payload = nullptr;
        uint32_t m_entryCount = 0;
        uint32_t m_slotIndex = 0xFFFFFFFF;
        BlockKey m_key;

        // clang rejects a defaulted constructor here because s_mappedBlocks below needs it before the class is complete.
        MappedBlock() {}
        MappedBlock(const MappedBlock&) = delete;
        MappedBlock& operator=(const MappedBlock&) = delete;

        void Close();
        void Reset();
    };

    static bool CacheAvailable();
    static size_t AllocationGranularity();

    static bool LoadGlyph(const BlockWrap& wrap, uint32_t codepoint, GlyphMetrics& outMetrics);
    static MappedBlock* GetOrLoadMappedBlock(const BlockWrap& wrap);
    static void FreeBlockByKey(BlockKey key);
    static void FlushAll();

    static uint32_t RegisterFont(FontHash hash);

    private:
    static const size_t MAX_ARENA_SLOTS = 16;
    static_assert(MAX_ARENA_SLOTS <= 64);
    static_assert(BLOCK_SIZE > 0 && (BLOCK_SIZE & (BLOCK_SIZE - 1)) == 0, "BLOCK_SIZE must be a power of 2");

    struct ArenaState {
        void* m_base = nullptr;
        size_t m_slotSize = 0;
        uint64_t m_freeMask = (1ULL << MAX_ARENA_SLOTS) - 1;
        std::array<void*, MAX_ARENA_SLOTS> m_slotAddresses;
        std::array<uint32_t, MAX_ARENA_SLOTS> m_slotToBlockIndex;

        ArenaState();
        ~ArenaState();

        bool IsSlotOccupied(uint32_t index) const { return !(this->m_freeMask & (1ULL << index)); }
        void* GetFreeSlot(uint32_t blockIndex, uint32_t& outSlotIndex);
        void FreeSlot(uint32_t slotIndex);
        void FlushAll();
    };

    static ArenaState& Arena();
    static bool LoadMappedBlock(const BlockWrap& wrap, MappedBlock& outBlock, void* slotAddr, uint32_t slotIndex);
    static void FreeBlock(uint32_t blockIndex);

    inline static std::array<MappedBlock, MAX_ARENA_SLOTS> s_mappedBlocks;
    inline static std::unordered_map<BlockKey, uint32_t, BlockKeyHash> s_blockCache;
    inline static uint32_t s_lastBlockIndex = 0xFFFFFFFF;
    inline static BlockKey s_lastBlockKey;
    inline static std::unordered_map<FontHash, uint32_t> s_fontHashToId;
    inline static uint32_t s_nextFontId = 0;
};

}

#endif
