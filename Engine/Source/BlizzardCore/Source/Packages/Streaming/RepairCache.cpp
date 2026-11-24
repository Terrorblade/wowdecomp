#include <cstdint>
#include <map>
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/RepairCache.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/BlizzardCore/Source/System/Process/Process.h"
#include "Engine/Source/BlizzardCore/Source/System/String/Path.h"

struct RepairRecord {
    /* 0000 */ Blizzard::Util::Digest<16> m_md5;
    /* 0010 */ int64_t m_index;
    /* 0018 */ uint32_t m_offset;
    /* 001C */ uint32_t m_size;
};

// STATIC: 0x00B37FF8
static void* s_reportContext;

// STATIC: 0x00B3800C
static void (*s_reportCallback)(void* context, uint32_t count, uint32_t repaired, uint32_t size);

// STATIC: 0x00B38360
Blizzard::Repair::RepairCache Blizzard::Repair::s_repairCache;

// INLINED
bool Blizzard::Repair::RepairKey::operator<(const RepairKey& other) const {
    if (this->m_md5 == other.m_md5) {
        return this->m_index < other.m_index;
    }

    return this->m_md5 < other.m_md5;
}

// VERIFY: 816b7dc5 manual
// OFFSET: 0x00440970
Blizzard::Repair::RepairCache::~RepairCache() {
    if (s_reportCallback && this->m_repairedCount) {
        s_reportCallback(s_reportContext, static_cast<uint32_t>(this->m_entries.size()), this->m_repairedCount, this->m_repairedSize);
    }

    for (std::map<RepairKey, RepairEntry>::iterator it = this->m_entries.begin(); it != this->m_entries.end(); ++it) {
        if (it->second.m_data) {
            Blizzard::Memory::Free(it->second.m_data);
        }
    }

    for (std::map<RepairKey, RepairEntry>::iterator it = this->m_repaired.begin(); it != this->m_repaired.end(); ++it) {
        if (it->second.m_data) {
            Blizzard::Memory::Free(it->second.m_data);
        }
    }
}

// VERIFY: 5fdd7411 manual
// OFFSET: 0x00440B10
bool Blizzard::Repair::RepairCache::AddEntry(const Blizzard::Util::Digest<16>& md5, int64_t index, uint32_t offset, uint32_t size) {
    RepairKey key;
    key.m_md5 = md5;
    key.m_index = index;

    if (this->m_entries.find(key) == this->m_entries.end()) {
        RepairEntry entry;
        entry.m_offset = offset;
        entry.m_size = size;
        entry.m_data = nullptr;
        entry.m_valid = true;
        this->m_entries.insert(std::pair<const RepairKey, RepairEntry>(key, entry));
        this->m_totalSize += size;
    }

    return true;
}

// VERIFY: a63a2d32 manual
// OFFSET: 0x00440BD0
bool Blizzard::Repair::RepairCache::Load(Blizzard::File::StreamRecord* file) {
    int64_t offset = 32 * static_cast<int64_t>(this->m_entries.size());

    for (;;) {
        RepairRecord record;
        int32_t count = sizeof(record);

        if (!Blizzard::File::Read(file, &record, offset, &count) || count != sizeof(record)) {
            break;
        }

        this->AddEntry(record.m_md5, record.m_index, record.m_offset, record.m_size);
        offset += sizeof(record);
    }

    return true;
}

// VERIFY: 8c309560 manual
// OFFSET: 0x00443EE0
Blizzard::Repair::RepairCache::RepairCache() {
    char directory[260];
    Blizzard::Process::GetProcessDirectory(directory, sizeof(directory));
    Blizzard::String::JoinPath(this->m_headPath, sizeof(this->m_headPath), directory, "Data/Repair.head");
    Blizzard::String::JoinPath(this->m_dataPath, sizeof(this->m_dataPath), directory, "Data/Repair.data");

    this->m_totalSize = 0;
    this->m_repairedCount = 0;
    this->m_repairedSize = 0;

    if (Blizzard::File::Exists(this->m_headPath) == EXISTENCE_FILE) {
        Blizzard::File::StreamRecord* file = nullptr;

        if (Blizzard::File::Open(this->m_headPath, 0x1001, file)) {
            this->Load(file);
            Blizzard::File::Close(file);
        }
    }
}
