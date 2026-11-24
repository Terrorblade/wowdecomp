#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include "Engine/Source/BlizzardCore/Source/Packages/iounit/IOUnitContainer.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/IOMpqUnit.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/MPQEncrypt.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/MPQGlobals.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SComp.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFile.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileSignatures.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/Hardware/Hardware.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
extern "C" {
#include "ThirdParty/zlib/contrib/blast/blast.h"
}
#include "ThirdParty/zlib/zlib.h"

// VERIFY: ca82d7d9 manual
// OFFSET: 0x00431DB0
IOMpqUnit::IOMpqUnit(System_SFile::ARCHIVEREC* archive, const char* name, bool ownsMemory, IOUnit* next) : IOUnit(next, ownsMemory), m_position(0), m_archive(archive), m_name((name ? static_cast<uint32_t>(strlen(name)) : 0) + 1) {
    Blizzard::String::Copy(this->m_name.m_data, name, this->m_name.m_count);

    if (this->m_archive) {
        this->m_archive->m_internalRefs.Increment();
    }
}

// VERIFY: a7d3cb7f auto
// OFFSET: 0x00429630
uint32_t IOMpqUnit::GetType() {
    return 'mpq ';
}

// VERIFY: 8c3b48ab auto
// OFFSET: 0x00435340
IOMpqUnit::~IOMpqUnit() {
}

// VERIFY: 915c8823 auto
// OFFSET: 0x00454060
bool IOMpqUnit::Close() {
    if (this->m_archive) {
        this->m_archive->RemoveInternalArchiveRef();
    }

    return true;
}

System_SFile::ARCHIVEREC* IOMpqUnit::GetArchive() {
    return this->m_archive;
}

const char* IOMpqUnit::GetName() {
    return this->m_name.m_data;
}

// VERIFY: 48195ff4 manual
// OFFSET: 0x00437640
IOMpqArchiveUnit::IOMpqArchiveUnit(System_SFile::ARCHIVEREC* archive, const char* name, uint32_t blockIndex, bool ownsMemory, IOUnit* next) : IOMpqUnit(archive, name, ownsMemory, next) {
    this->m_blockIndex = blockIndex;

    // An archive being patched grows its block table under the archive lock, and SetFileSignature fills the signatures under it too.
    Blizzard::Lock::ScopedMutex lock(System_MPQGlobals::GetArchiveLock()->m_mutex);
    this->m_blockEntry = archive->GetBlockEntry(blockIndex);

    if (this->m_blockEntry.m_md5Check) {
        System_SFile::s_signatures.GetSignature(name, &this->m_blockEntry.m_md5);
    }
}

// VERIFY: 8184d4ce auto
// OFFSET: 0x00431EB0
bool IOMpqArchiveUnit::GetFileSize(int64_t* size) {
    *size = this->m_blockEntry.m_fileSize;
    return true;
}

bool IOMpqArchiveUnit::SetFileSize(int64_t size) {
    return false;
}

// VERIFY: 6f089668 auto
// OFFSET: 0x00432010
IOMpqArchiveUnit::~IOMpqArchiveUnit() {
}

// VERIFY: 31dccac3 manual
// OFFSET: 0x00431EF0
const char* IOMpqArchiveUnit::GetPath() {
    return this->GetName();
}

// VERIFY: 0c8748b4 auto
// OFFSET: 0x00431ED0
bool IOMpqArchiveUnit::GetCompressedSize(int64_t* size) {
    *size = this->m_blockEntry.m_compressedSize;
    return true;
}

// VERIFY: 3f0e5717 auto
// OFFSET: 0x00431F00
IOUnit* IOMpqArchiveUnit::GetBaseUnit() {
    return this->m_archive->m_file;
}

// VERIFY: 1be0c5bf auto
// OFFSET: 0x00431F10
int64_t IOMpqArchiveUnit::GetOffset() {
    return this->m_blockEntry.m_offset;
}

// VERIFY: e6452137 auto
// OFFSET: 0x00431F20
uint64_t IOMpqArchiveUnit::GetFileTime() {
    return this->m_blockEntry.m_fileTime;
}

// VERIFY: fa7d866e auto
// OFFSET: 0x00431F30
uint32_t IOMpqArchiveUnit::GetCrc() {
    return this->m_blockEntry.m_crc;
}

// VERIFY: c2fc05db auto
// OFFSET: 0x00431F40
const Blizzard::Util::Digest<16>* IOMpqArchiveUnit::GetMd5() {
    return &this->m_blockEntry.m_md5;
}

// VERIFY: 9e9495a5 auto
// OFFSET: 0x00431F50
bool IOMpqArchiveUnit::HasMd5Check() {
    return this->m_blockEntry.m_md5Check;
}

// VERIFY: 636246fc auto
// OFFSET: 0x00431F80
void IOMpqArchiveUnit::DisableMd5Check() {
    // Table writers take the archive mutex and then the archive lock, lookups read the tables under the archive lock.
    {
        Blizzard::Lock::ScopedMutex archiveLock(this->m_archive->m_mutex.m_mutex);
        Blizzard::Lock::ScopedMutex lock(System_MPQGlobals::GetArchiveLock()->m_mutex);
        this->m_archive->SetMD5Check(this->m_blockIndex, false);
    }

    this->m_blockEntry.m_md5Check = false;
}

// VERIFY: 5faa24df auto
// OFFSET: 0x00431F60
uint32_t IOMpqArchiveUnit::GetFlags() {
    return this->m_blockEntry.m_flags;
}

// VERIFY: 22f28fcc auto
// OFFSET: 0x00431FA0
bool IOMpqArchiveUnit::MarkDeleted() {
    this->m_blockEntry.m_flags |= MPQ_FILE_DELETE_MARKER;

    {
        Blizzard::Lock::ScopedMutex archiveLock(this->m_archive->m_mutex.m_mutex);
        Blizzard::Lock::ScopedMutex lock(System_MPQGlobals::GetArchiveLock()->m_mutex);
        this->m_archive->SetBlockEntry(this->m_blockIndex, this->m_blockEntry);
    }

    return true;
}

// VERIFY: f5430f38 auto
// OFFSET: 0x00431F70
bool IOMpqArchiveUnit::IsEncrypted() {
    return (this->m_blockEntry.m_flags & MPQ_FILE_ENCRYPTION_MASK) != 0;
}

// VERIFY: 41863b5d manual
// OFFSET: 0x004396E0
IOMpqArchiveReadUnit* IOMpqArchiveReadUnit::Create(IOUnitContainer* container, System_SFile::ARCHIVEREC* archive, const char* name, uint32_t blockIndex, IOUnit* next) {
    if (!next) {
        return nullptr;
    }

    bool ownsMemory;
    void* memory = IOUnitContainer::Alloc(container, sizeof(IOMpqArchiveReadUnit), &ownsMemory);

    if (!memory) {
        return nullptr;
    }

    return new (memory) IOMpqArchiveReadUnit(archive, name, blockIndex, ownsMemory, next);
}

// INLINED
IOMpqArchiveReadUnit::IOMpqArchiveReadUnit(System_SFile::ARCHIVEREC* archive, const char* name, uint32_t blockIndex, bool ownsMemory, IOUnit* next) : IOMpqArchiveUnit(archive, name, blockIndex, ownsMemory, next) {
}

// VERIFY: f8df25f8 auto
// OFFSET: 0x00426880
bool IOMpqArchiveReadUnit::Write(const void* buffer, int64_t offset, uint32_t count) {
    return false;
}

// VERIFY: 6194de2c manual
// OFFSET: 0x00435380
IOMpqDiskUnit* IOMpqDiskUnit::Create(IOUnitContainer* container, System_SFile::ARCHIVEREC* archive, const char* name, const char* path, IOUnit* next) {
    if (!next) {
        return nullptr;
    }

    bool ownsMemory;
    void* memory = IOUnitContainer::Alloc(container, sizeof(IOMpqDiskUnit), &ownsMemory);

    if (!memory) {
        return nullptr;
    }

    return new (memory) IOMpqDiskUnit(archive, name, path, ownsMemory, next);
}

// VERIFY: 27a64d53 manual
// OFFSET: 0x00432050
IOMpqDiskUnit::IOMpqDiskUnit(System_SFile::ARCHIVEREC* archive, const char* name, const char* path, bool ownsMemory, IOUnit* next) : IOMpqUnit(archive, name, ownsMemory, next), m_path((path ? static_cast<uint32_t>(strlen(path)) : 0) + 1) {
    Blizzard::String::Copy(this->m_path.m_data, path, this->m_path.m_count);
}

// VERIFY: 3682befb auto
// OFFSET: 0x00432140
IOMpqDiskUnit::~IOMpqDiskUnit() {
}

// VERIFY: deabac3d auto
// OFFSET: 0x00432130
const char* IOMpqDiskUnit::GetPath() {
    return this->m_path.m_data;
}

bool IOMpqDiskUnit::GetCompressedSize(int64_t* size) {
    return this->m_next->GetFileSize(size);
}

IOUnit* IOMpqDiskUnit::GetBaseUnit() {
    return this->m_next;
}

int64_t IOMpqDiskUnit::GetOffset() {
    return 0;
}

uint64_t IOMpqDiskUnit::GetFileTime() {
    return 0;
}

uint32_t IOMpqDiskUnit::GetCrc() {
    return 0;
}

// VERIFY: 70a1b3db auto
// OFFSET: 0x00432120
const Blizzard::Util::Digest<16>* IOMpqDiskUnit::GetMd5() {
    return &System_SFile::s_emptyMd5;
}

bool IOMpqDiskUnit::HasMd5Check() {
    return false;
}

void IOMpqDiskUnit::DisableMd5Check() {
}

uint32_t IOMpqDiskUnit::GetFlags() {
    return 0;
}

bool IOMpqDiskUnit::MarkDeleted() {
    return false;
}

bool IOMpqDiskUnit::IsEncrypted() {
    return false;
}

// VERIFY: 36098579 manual
// OFFSET: 0x004351E0
IOUnit* IOMpqReadUnit::Create(IOUnitContainer* container, uint32_t fileSize, uint32_t compressedSize, uint32_t sectorSize, uint32_t flags, uint32_t key, IOUnit* next) {
    if (!next) {
        return nullptr;
    }

    if (!fileSize) {
        return next;
    }

    uint32_t sectorCount = (fileSize + sectorSize - 1) / sectorSize;

    bool ownsMemory;
    void* memory = IOUnitContainer::Alloc(container, sizeof(IOMpqReadUnit), &ownsMemory);

    if (!memory) {
        return nullptr;
    }

    return new (memory) IOMpqReadUnit(fileSize, compressedSize, sectorCount, sectorSize, flags, key, ownsMemory, next);
}

// VERIFY: cd8765a0 manual
// OFFSET: 0x00435290
IOUnit* IOMpqReadUnit::CreateSingle(IOUnitContainer* container, uint32_t fileSize, uint32_t compressedSize, uint32_t flags, uint32_t key, IOUnit* next) {
    if (!next) {
        return nullptr;
    }

    if (!fileSize) {
        return next;
    }

    bool ownsMemory;
    void* memory = IOUnitContainer::Alloc(container, sizeof(IOMpqReadUnit), &ownsMemory);

    if (!memory) {
        return nullptr;
    }

    return new (memory) IOMpqReadUnit(fileSize, compressedSize, flags, key, ownsMemory, next);
}

// VERIFY: a2649e4d manual
// OFFSET: 0x00459B90
bool IOMpqReadUnit::DecompressBlocks(IOMpqReadUnit* unit, uint32_t offset, uint32_t size, uint8_t* source, uint32_t first, uint32_t count, uint8_t* dest) {
    uint32_t sector = first;
    bool hasChecksums = unit->m_checksums.m_count != 0;
    uint32_t end = first + count;

    if (first >= end) {
        return true;
    }

    while (true) {
        uint32_t compressedSize = unit->m_sectorOffsets.m_data[sector + 1] - unit->m_sectorOffsets.m_data[sector];
        uint32_t sectorSize = sector >= unit->m_sectorOffsets.m_count - 3 ? unit->m_lastSectorSize : unit->m_sectorSize;

        if (unit->m_flags & MPQ_FILE_ENCRYPTION_MASK) {
            System_MPQEncrypt::Decrypt(reinterpret_cast<uint32_t*>(source), compressedSize & ~3u, sector + unit->m_key);
        }

        if (hasChecksums) {
            uint32_t checksum = unit->m_checksums.m_data[sector];

            if (checksum) {
                uint32_t adler = adler32(0, source, compressedSize);

                if (!adler) {
                    adler = -1;
                }

                if (adler != checksum) {
                    return false;
                }
            }
        }

        uint32_t outSize = sectorSize;
        uint32_t method = unit->m_flags & MPQ_FILE_COMPRESS_MASK;

        if (method == MPQ_FILE_IMPLODE) {
            System_SComp::PkwareBuffer buffer;
            buffer.m_output = dest;
            buffer.m_outputPos = 0;
            buffer.m_outputSize = sectorSize;
            buffer.m_input = source;
            buffer.m_inputSize = compressedSize;

            if (blast(System_SComp::PkwareBufferRead, &buffer, System_SComp::PkwareBufferWrite, &buffer)) {
                return false;
            }

            outSize = buffer.m_outputPos;
        } else if (method != MPQ_FILE_COMPRESS || !System_SComp::Decompress(dest, &outSize, source, compressedSize)) {
            return false;
        }

        if (sectorSize != outSize) {
            return false;
        }

        dest += outSize;
        source += compressedSize;

        if (++sector >= end) {
            return true;
        }
    }
}

// VERIFY: 6ceb9959 auto
// OFFSET: 0x0045D860
void IOMpqReadUnit::DecompressSectors(void* param) {
    SectorJob* job = static_cast<SectorJob*>(param);
    uint32_t sector = job->m_first;
    uint8_t* scratch = job->m_scratch;
    uint32_t none = 0;

    job->m_success = false;
    job->m_scratchLeft = job->m_scratchSize;
    job->m_destCursor = job->m_dest;

    while (true) {
        uint32_t compressed = job->m_unit->CountCompressed(sector, job->m_end);

        if (compressed) {
            if (!job->m_unit->DecompressRun(sector, compressed, &job->m_destCursor, &scratch, &job->m_scratchLeft)) {
                return;
            }

            sector += compressed;

            if (sector >= job->m_end) {
                break;
            }
        }

        uint32_t uncompressed = job->m_unit->CountUncompressed(sector, job->m_end);

        if (uncompressed) {
            if (!job->m_unit->VerifyRun(sector, uncompressed, &job->m_destCursor, nullptr, &none)) {
                return;
            }

            sector += uncompressed;

            if (sector >= job->m_end) {
                break;
            }
        }
    }

    job->m_success = true;
}

// VERIFY: 2652cd24 auto
// OFFSET: 0x004342F0
void IOMpqReadUnit::ReadSectors(SectorJob* job) {
    uint32_t sector = job->m_first;
    uint8_t* scratch = job->m_scratch;
    uint32_t none = 0;

    job->m_success = false;
    job->m_scratchLeft = job->m_scratchSize;
    job->m_destCursor = job->m_dest;

    while (true) {
        uint32_t compressed = job->m_unit->CountCompressed(sector, job->m_end);

        if (compressed) {
            if (!job->m_unit->ReadRun(sector, compressed, &job->m_destCursor, &scratch, &job->m_scratchLeft)) {
                return;
            }

            sector += compressed;

            if (sector >= job->m_end) {
                break;
            }
        }

        uint32_t uncompressed = job->m_unit->CountUncompressed(sector, job->m_end);

        if (uncompressed) {
            if (!job->m_unit->ReadRun(sector, uncompressed, &job->m_destCursor, nullptr, &none)) {
                return;
            }

            sector += uncompressed;

            if (sector >= job->m_end) {
                break;
            }
        }
    }

    job->m_success = true;
}

// VERIFY: 26ab9193 manual
// OFFSET: 0x0042BAC0
bool IOMpqReadUnit::VerifyBlocks(IOMpqReadUnit* unit, uint32_t offset, uint32_t size, uint8_t* source, uint32_t first, uint32_t count, uint8_t* dest) {
    bool hasChecksums = unit->m_checksums.m_count != 0;

    if (!(unit->m_flags & MPQ_FILE_ENCRYPTION_MASK) && !hasChecksums) {
        return true;
    }

    uint32_t sector = first;

    if (first >= first + count) {
        return true;
    }

    while (true) {
        uint32_t sectorSize = sector >= unit->m_sectorOffsets.m_count - 3 ? unit->m_lastSectorSize : unit->m_sectorSize;

        if (unit->m_flags & MPQ_FILE_ENCRYPTION_MASK) {
            System_MPQEncrypt::Decrypt(reinterpret_cast<uint32_t*>(dest), sectorSize & ~3u, sector + unit->m_key);
        }

        if (hasChecksums) {
            uint32_t checksum = unit->m_checksums.m_data[sector];

            if (checksum) {
                uint32_t adler = adler32(0, dest, sectorSize);

                if (!adler) {
                    adler = -1;
                }

                if (adler != checksum) {
                    return false;
                }
            }
        }

        dest += sectorSize;

        if (++sector >= first + count) {
            return true;
        }
    }
}

// VERIFY: 7713b1e2 manual
// OFFSET: 0x00430EB0
IOMpqReadUnit::IOMpqReadUnit(uint32_t fileSize, uint32_t compressedSize, uint32_t sectorCount, uint32_t sectorSize, uint32_t flags, uint32_t key, bool ownsMemory, IOUnit* next) : IOUnit(next, ownsMemory), m_sectorOffsets(sectorCount + 2), m_checksums(0) {
    this->m_key = key;
    this->m_flags = flags;
    this->m_fileSize = fileSize;
    this->m_sectorSize = sectorSize;
    this->m_lastSectorSize = fileSize % sectorSize;

    if (!this->m_lastSectorSize) {
        this->m_lastSectorSize = sectorSize;
    }

    this->m_sectorOffsets.m_data[0] = -3;
    this->m_sectorOffsets.m_data[1] = compressedSize;
}

// VERIFY: faed398b manual
// OFFSET: 0x00430FD0
IOMpqReadUnit::IOMpqReadUnit(uint32_t fileSize, uint32_t compressedSize, uint32_t flags, uint32_t key, bool ownsMemory, IOUnit* next) : IOUnit(next, ownsMemory), m_sectorOffsets(3), m_checksums(0) {
    this->m_sectorSize = fileSize;
    this->m_lastSectorSize = fileSize;
    this->m_flags = flags;
    this->m_fileSize = fileSize;
    this->m_key = key;
    this->m_sectorOffsets.m_data[0] = 0;
    this->m_sectorOffsets.m_data[1] = compressedSize;
}

// VERIFY: b5e816a0 auto
// OFFSET: 0x00429770
uint32_t IOMpqReadUnit::GetType() {
    return 'mpqR';
}

// VERIFY: 04968570 manual
// OFFSET: 0x0045D900
bool IOMpqReadUnit::Read(void* buffer, int64_t offset, uint32_t count, uint32_t* bytesRead) {
    if (!count) {
        return true;
    }

    uint32_t state = this->m_sectorOffsets.m_data[0];

    if (state >= 0xFFFFFFFD) {
        if (state == 0xFFFFFFFF) {
            return false;
        }

        if (!this->LoadSectorTable()) {
            Blizzard::File::AddToLastErrorStack(0, "LoadSectorTable failed", 4);
            this->m_sectorOffsets.m_data[0] = state + 1;
            return false;
        }
    }

    uint32_t start = static_cast<uint32_t>(offset);

    if (start + count > this->m_fileSize) {
        if (!bytesRead) {
            return false;
        }

        count = this->m_fileSize - start;
    }

    uint32_t first = start / this->m_sectorSize;
    uint32_t sectors = (this->m_sectorSize + start + count - 1) / this->m_sectorSize - first;
    uint32_t workerCount;

    if (sectors >= 8) {
        workerCount = sectors >> 2;
        Blizzard::Hardware::GetProcessorFeatures();
        workerCount = std::min(std::min(workerCount, System_Hardware::s_processorFeatures.m_processorCount), 8u) - 1;
    } else {
        workerCount = 0;
    }

    if (System_SFile::s_threadingMode == 1) {
        workerCount >>= 1;
    } else if (System_SFile::s_threadingMode == 2) {
        workerCount = 0;
    }

    System_MPQGlobals::Worker* workers[7];
    System_MPQGlobals::GetWorkerPool()->Acquire(workerCount, workers);

    uint32_t perJob = sectors / (workerCount + 1);
    uint32_t extra = sectors % (workerCount + 1);
    bool dispatched[7] = {};
    Blizzard::Memory::TempBuffer<uint8_t, 0x20000> scratch(0x20000);
    uint8_t* scratchPtr = scratch.m_data;
    uint32_t scratchSize = 0x20000;
    uint8_t* dest = static_cast<uint8_t*>(buffer);
    SectorJob jobs[8];

    for (uint32_t i = 0;; i++) {
        SectorJob* job = &jobs[i];
        job->m_unit = this;
        job->m_first = first;
        job->m_end = first + perJob + (i < extra);
        job->m_scratch = scratchPtr;
        job->m_scratchSize = scratchSize;
        job->m_dest = dest;

        ReadSectors(job);

        first = job->m_end;
        scratchSize = job->m_scratchLeft;
        scratchPtr = scratch.m_buffer + 0x20000 - scratchSize;
        dest = job->m_destCursor;

        if (job->m_success) {
            if (i >= workerCount) {
                DecompressSectors(job);
            } else {
                System_MPQGlobals::Worker* worker = workers[i];

                if (!worker->m_proc) {
                    worker->m_proc = DecompressSectors;
                    worker->m_param = job;
                    Blizzard::Lock::EventSet(worker->m_start.m_event);
                }

                dispatched[i] = true;
            }
        }

        if (i + 1 > workerCount) {
            break;
        }
    }

    for (uint32_t i = 0; i < workerCount; i++) {
        if (dispatched[i]) {
            Blizzard::Lock::EventWaitForever(workers[i]->m_done.m_event);
        }
    }

    System_MPQGlobals::GetWorkerPool()->Release(workerCount, workers);

    for (uint32_t i = 0; jobs[i].m_success;) {
        if (++i > workerCount) {
            if (bytesRead) {
                *bytesRead = count;
            }

            return true;
        }
    }

    Blizzard::File::AddToLastErrorStack(0, "ReadSectors/DecompressData failed", 4);

    return false;
}

bool IOMpqReadUnit::Write(const void* buffer, int64_t offset, uint32_t count) {
    return false;
}

bool IOMpqReadUnit::Flush() {
    return true;
}

// VERIFY: 630ff2f3 auto
// OFFSET: 0x00426890
bool IOMpqReadUnit::GetFileSize(int64_t* size) {
    *size = this->m_fileSize;
    return true;
}

bool IOMpqReadUnit::SetFileSize(int64_t size) {
    return false;
}

// VERIFY: 47664f0f auto
// OFFSET: 0x00430F80
IOMpqReadUnit::~IOMpqReadUnit() {
}

bool IOMpqReadUnit::Close() {
    return true;
}

// VERIFY: a824d722 manual
// OFFSET: 0x0042FB60
uint32_t IOMpqReadUnit::CountCompressed(uint32_t first, uint32_t end) {
    uint32_t* offsets = this->m_sectorOffsets.m_data;
    uint32_t previous = offsets[first];
    uint32_t sector = first;

    if (first < end) {
        uint32_t last = this->m_sectorOffsets.m_count - 3;

        do {
            uint32_t next = offsets[sector + 1];
            uint32_t sectorSize = sector >= last ? this->m_lastSectorSize : this->m_sectorSize;

            if (next - previous == sectorSize) {
                break;
            }

            sector++;
            previous = next;
        } while (sector < end);
    }

    return sector - first;
}

// VERIFY: fc6c70df manual
// OFFSET: 0x0042FC60
uint32_t IOMpqReadUnit::CountUncompressed(uint32_t first, uint32_t end) {
    uint32_t* offsets = this->m_sectorOffsets.m_data;
    uint32_t previous = offsets[first];
    uint32_t sector = first;

    if (first < end) {
        uint32_t last = this->m_sectorOffsets.m_count - 3;

        do {
            uint32_t next = offsets[sector + 1];
            uint32_t sectorSize = sector >= last ? this->m_lastSectorSize : this->m_sectorSize;

            if (next - previous != sectorSize) {
                break;
            }

            sector++;
            previous = next;
        } while (sector < end);
    }

    return sector - first;
}

// VERIFY: 15ce3f88 manual
// OFFSET: 0x0045B4E0
bool IOMpqReadUnit::DecompressRun(uint32_t first, uint32_t count, uint8_t** dest, uint8_t** scratch, uint32_t* scratchLeft) {
    uint32_t start = this->m_sectorOffsets.m_data[first];
    uint32_t size = this->m_sectorOffsets.m_data[first + count] - start;
    uint32_t lastSize = first + count - 1 >= this->m_sectorOffsets.m_count - 3 ? this->m_lastSectorSize : this->m_sectorSize;
    uint32_t total = lastSize + this->m_sectorSize * (count - 1);

    if (*scratchLeft < size) {
        if (!DecompressBlocks(this, start, size, *dest + total - size, first, count, *dest)) {
            return false;
        }

        *dest += total;
        return true;
    }

    if (!DecompressBlocks(this, start, size, *scratch, first, count, *dest)) {
        return false;
    }

    *scratch += size;
    *scratchLeft -= size;
    *dest += total;
    return true;
}

// VERIFY: d7e23bc5 manual
// OFFSET: 0x00459A00
bool IOMpqReadUnit::LoadSectorTable() {
    uint32_t entries = this->m_sectorOffsets.m_count - 1;
    uint32_t tableSize = entries * 4;
    uint32_t compressedSize = this->m_sectorOffsets.m_data[1];
    uint32_t bytesRead = 0;

    if (!this->m_next->Read(this->m_sectorOffsets.m_data, 0, tableSize + 4, &bytesRead) || bytesRead < tableSize) {
        this->m_sectorOffsets.m_data[1] = compressedSize;
        return false;
    }

    bytesRead &= ~3u;

    if (this->m_flags & MPQ_FILE_ENCRYPTION_MASK) {
        System_MPQEncrypt::Decrypt(this->m_sectorOffsets.m_data, bytesRead, this->m_key - 1);
    }

    for (uint32_t i = 1; i < entries; i++) {
        if (this->m_sectorOffsets.m_data[i] <= this->m_sectorOffsets.m_data[i - 1] || this->m_sectorOffsets.m_data[i] > compressedSize) {
            this->m_sectorOffsets.m_data[1] = compressedSize;
            return false;
        }
    }

    uint32_t* offsets = this->m_sectorOffsets.m_data;

    if (offsets[0] - tableSize >= 4) {
        uint32_t checksumOffset = offsets[entries - 1];
        uint32_t checksumSize = offsets[entries] - checksumOffset;
        uint32_t expected = tableSize - 4;

        if (checksumSize) {
            if (checksumSize > expected) {
                this->m_sectorOffsets.m_data[1] = compressedSize;
                return false;
            }

            this->m_checksums.Resize(entries - 1);

            if (!this->m_next->Read(this->m_checksums.m_data, checksumOffset, checksumSize, nullptr)) {
                this->m_sectorOffsets.m_data[1] = compressedSize;
                return false;
            }

            uint32_t outSize = expected;

            if (!System_SComp::Decompress(this->m_checksums.m_data, &outSize, this->m_checksums.m_data, checksumSize) || expected != outSize) {
                this->m_sectorOffsets.m_data[1] = compressedSize;
                return false;
            }
        }
    }

    return true;
}

// VERIFY: 78cbfb13 manual
// OFFSET: 0x0042FBC0
bool IOMpqReadUnit::ReadRun(uint32_t first, uint32_t count, uint8_t** dest, uint8_t** scratch, uint32_t* scratchLeft) {
    uint32_t start = this->m_sectorOffsets.m_data[first];
    uint32_t size = this->m_sectorOffsets.m_data[first + count] - start;
    uint32_t lastSize = first + count - 1 >= this->m_sectorOffsets.m_count - 3 ? this->m_lastSectorSize : this->m_sectorSize;
    uint32_t total = lastSize + this->m_sectorSize * (count - 1);

    if (*scratchLeft < size) {
        if (!this->m_next->Read(*dest + total - size, start, size, nullptr)) {
            return false;
        }

        *dest += total;
        return true;
    }

    if (!this->m_next->Read(*scratch, start, size, nullptr)) {
        return false;
    }

    *scratch += size;
    *scratchLeft -= size;
    *dest += total;
    return true;
}

// VERIFY: ce01556e manual
// OFFSET: 0x0042FCC0
bool IOMpqReadUnit::VerifyRun(uint32_t first, uint32_t count, uint8_t** dest, uint8_t** scratch, uint32_t* scratchLeft) {
    uint32_t start = this->m_sectorOffsets.m_data[first];
    uint32_t size = this->m_sectorOffsets.m_data[first + count] - start;
    uint32_t lastSize = first + count - 1 >= this->m_sectorOffsets.m_count - 3 ? this->m_lastSectorSize : this->m_sectorSize;
    uint32_t total = lastSize + this->m_sectorSize * (count - 1);

    if (*scratchLeft < size) {
        if (!VerifyBlocks(this, start, size, *dest + total - size, first, count, *dest)) {
            return false;
        }

        *dest += total;
        return true;
    }

    if (!VerifyBlocks(this, start, size, *scratch, first, count, *dest)) {
        return false;
    }

    *scratch += size;
    *scratchLeft -= size;
    *dest += total;
    return true;
}

// VERIFY: 41bc2f9a manual
// OFFSET: 0x004397B0
IOMpqArchiveWriteUnit* IOMpqArchiveWriteUnit::Create(IOUnitContainer* container, System_SFile::ARCHIVEREC* archive, const char* name, uint32_t blockIndex, IOUnit* next) {
    if (!next) {
        return nullptr;
    }

    bool ownsMemory;
    void* memory = IOUnitContainer::Alloc(container, sizeof(IOMpqArchiveWriteUnit), &ownsMemory);

    if (!memory) {
        return nullptr;
    }

    return new (memory) IOMpqArchiveWriteUnit(archive, name, blockIndex, ownsMemory, next);
}

// INLINED
IOMpqArchiveWriteUnit::IOMpqArchiveWriteUnit(System_SFile::ARCHIVEREC* archive, const char* name, uint32_t blockIndex, bool ownsMemory, IOUnit* next) : IOMpqArchiveUnit(archive, name, blockIndex, ownsMemory, next) {
}

bool IOMpqArchiveWriteUnit::Read(void* buffer, int64_t offset, uint32_t count, uint32_t* bytesRead) {
    return false;
}

// VERIFY: b7df5c48 manual
// OFFSET: 0x004268B0
void IOMpqWriteUnit::SetContext(void* context) {
    this->m_context = context;
}

// VERIFY: 295973cf auto
// OFFSET: 0x00429780
uint32_t IOMpqWriteUnit::GetType() {
    return 'mpqW';
}

// VERIFY: f1455b0b manual
// OFFSET: 0x0042BB70
bool IOMpqWriteUnit::GetFileSize(int64_t* size) {
    *size = std::min(this->m_sectorSize * this->m_sectorIndex, this->m_fileSize);
    return true;
}

bool IOMpqWriteUnit::Read(void* buffer, int64_t offset, uint32_t count, uint32_t* bytesRead) {
    return false;
}

bool IOMpqWriteUnit::SetFileSize(int64_t size) {
    return false;
}

bool IOMpqWriteUnit::Close() {
    return true;
}

// VERIFY: 00b11648 manual
// OFFSET: 0x00443AC0
IOMpqWriteUnit::IOMpqWriteUnit(const std::vector<Blizzard::Mopaq::CompressionAlgorithm>& compression, uint32_t fileSize, uint32_t sectorCount, uint32_t sectorSize, Callback callback, IOMpqWriteUnit** self, bool ownsMemory, IOUnit* next) : IOUnit(next, ownsMemory), m_checksums(sectorCount), m_sectorOffsets(sectorCount + 2), m_compression(compression) {
    this->m_fileSize = fileSize;
    this->m_hasChecksums = false;
    this->m_sectorSize = sectorSize;
    this->m_sectorIndex = 0;
    this->m_callback = callback;
    this->m_context = nullptr;
    *self = this;
    this->m_sectorOffsets.m_data[0] = 4 * (sectorCount + 2);
    memset(this->m_sectorOffsets.m_data + 1, 0, 4 * sectorCount + 4);
    memset(this->m_checksums.m_data, 0, 4 * sectorCount);
}

// VERIFY: 8623bfd7 manual
// OFFSET: [0x00443BF0, 0x00443C60]
IOMpqWriteUnit::~IOMpqWriteUnit() {
}

// VERIFY: 53dd2286 manual
// OFFSET: 0x00443C90
IOMpqWriteUnit::IOMpqWriteUnit(const std::vector<Blizzard::Mopaq::CompressionAlgorithm>& compression, uint32_t fileSize, Callback callback, IOMpqWriteUnit** self, bool ownsMemory, IOUnit* next) : IOUnit(next, ownsMemory), m_checksums(0), m_sectorOffsets(0), m_compression(compression) {
    this->m_fileSize = fileSize;
    this->m_hasChecksums = false;
    this->m_sectorSize = fileSize;
    this->m_sectorIndex = 0;
    this->m_callback = callback;
    this->m_context = nullptr;
    *self = this;
}

// VERIFY: c03708dd manual
// OFFSET: 0x004460D0
IOUnit* IOMpqWriteUnit::Create(IOUnitContainer* container, const std::vector<Blizzard::Mopaq::CompressionAlgorithm>& compression, uint32_t fileSize, uint32_t sectorSize, Callback callback, IOMpqWriteUnit** self, IOUnit* next) {
    if (!next || !fileSize) {
        return next;
    }

    bool ownsMemory;
    void* memory = IOUnitContainer::Alloc(container, sizeof(IOMpqWriteUnit), &ownsMemory);

    if (!memory) {
        return nullptr;
    }

    return new (memory) IOMpqWriteUnit(compression, fileSize, (fileSize + sectorSize - 1) / sectorSize, sectorSize, callback, self, ownsMemory, next);
}

// VERIFY: 68f18f45 manual
// OFFSET: 0x00446180
IOUnit* IOMpqWriteUnit::CreateSingle(IOUnitContainer* container, const std::vector<Blizzard::Mopaq::CompressionAlgorithm>& compression, uint32_t fileSize, Callback callback, IOMpqWriteUnit** self, IOUnit* next) {
    if (!next) {
        return nullptr;
    }

    if (!fileSize || compression.empty()) {
        return next;
    }

    bool ownsMemory;
    void* memory = IOUnitContainer::Alloc(container, sizeof(IOMpqWriteUnit), &ownsMemory);

    if (!memory) {
        return nullptr;
    }

    return new (memory) IOMpqWriteUnit(compression, fileSize, callback, self, ownsMemory, next);
}

// VERIFY: 1a1255b6 manual
// OFFSET: 0x00459CE0
void IOMpqWriteUnit::CompressBlock(void* param) {
    CompressJob* job = static_cast<CompressJob*>(param);
    IOMpqWriteUnit* unit = job->m_unit;
    uint32_t sectorSize = job->m_sectorSize;
    uint8_t* dst = job->m_dst;
    uint32_t remaining = job->m_size;
    const uint8_t* src = job->m_src;
    uint32_t* sizes = job->m_sizes;
    uint32_t* checksums = job->m_checksums;

    job->m_success = false;

    while (remaining) {
        uint32_t chunk = std::min(sectorSize, remaining);
        uint32_t size = chunk;
        bool checksum = false;
        bool compressed;

        if (!unit->m_compression.empty()) {
            compressed = System_SComp::Compress(dst, &size, &checksum, src, chunk, static_cast<uint32_t>(unit->m_compression.size()), &unit->m_compression[0]);
        } else {
            compressed = System_SComp::Compress(dst, &size, &checksum, src, chunk, 0, nullptr);
        }

        if (!compressed) {
            return;
        }

        if (checksum && unit->m_sectorOffsets.m_count) {
            uint32_t adler = adler32(0, dst, size);

            if (!adler) {
                adler = 0xFFFFFFFF;
            }

            *checksums = adler;
            unit->m_hasChecksums = true;
        }

        checksums++;
        src += chunk;
        dst += size;
        *sizes++ = size;
        remaining -= chunk;
    }

    job->m_success = true;
}

// VERIFY: 46c75536 manual
// OFFSET: 0x00459E10
bool IOMpqWriteUnit::Write(const void* buffer, int64_t offset, uint32_t count) {
    if (!count) {
        return true;
    }

    if (count + static_cast<uint32_t>(offset) > this->m_fileSize) {
        return false;
    }

    if (static_cast<uint32_t>(offset) / this->m_sectorSize != this->m_sectorIndex) {
        return false;
    }

    bool single = !this->m_sectorOffsets.m_count;
    uint32_t* offsets;
    uint32_t offsetCount;
    uint32_t singleOffsets[3];

    if (!single) {
        offsets = this->m_sectorOffsets.m_data;
        offsetCount = this->m_sectorOffsets.m_count;
    } else {
        singleOffsets[1] = 0;
        singleOffsets[0] = 0;
        offsets = singleOffsets;
        offsetCount = 3;
    }

    uint32_t sectors = (count + this->m_sectorSize - 1) / this->m_sectorSize;
    uint32_t workerCount = 0;

    if (sectors >= 8) {
        workerCount = sectors >> 2;
        Blizzard::Hardware::GetProcessorFeatures();
        workerCount = std::min(std::min(workerCount, System_Hardware::s_processorFeatures.m_processorCount), 8u) - 1;
    }

    System_MPQGlobals::Worker* workers[7];
    System_MPQGlobals::GetWorkerPool()->Acquire(workerCount, workers);

    uint32_t extra = sectors % (workerCount + 1);
    uint32_t perJob = sectors / (workerCount + 1);
    Blizzard::Memory::TempBuffer<uint8_t, 0x1000> output(count);
    CompressJob jobs[8];
    uint32_t consumed = 0;
    uint32_t sectorsDone = 0;
    bool result = true;

    for (uint32_t i = 0;;) {
        CompressJob* job = &jobs[i];
        uint32_t jobSectors = perJob + (i < extra);
        job->m_src = static_cast<const uint8_t*>(buffer) + consumed;
        job->m_dst = output.m_data + consumed;
        job->m_sectorSize = this->m_sectorSize;
        job->m_unit = this;

        uint32_t bytes = std::min(jobSectors * this->m_sectorSize, count);
        consumed += bytes;
        count -= bytes;

        uint32_t index = sectorsDone + this->m_sectorIndex;
        sectorsDone += jobSectors;
        job->m_sizes = offsets + index + 1;
        job->m_checksums = this->m_checksums.m_data + index;
        job->m_size = bytes;

        if (i) {
            System_MPQGlobals::Worker* worker = workers[i - 1];

            if (!worker->m_proc) {
                worker->m_param = job;
                worker->m_proc = CompressBlock;
                Blizzard::Lock::EventSet(worker->m_start.m_event);
            }
        }

        if (++i > workerCount) {
            break;
        }
    }

    CompressBlock(&jobs[0]);

    for (uint32_t i = 0; i <= workerCount; i++) {
        CompressJob* job = &jobs[i];

        if (i) {
            Blizzard::Lock::EventWaitForever(workers[i - 1]->m_done.m_event);
        }

        if (!job->m_success) {
            result = false;
        }

        uint32_t* last = job->m_sizes + (perJob + (i < extra)) - 1;

        for (uint32_t* size = job->m_sizes; size <= last; size++) {
            *size += size[-1];
        }

        if (!this->m_next->Write(job->m_dst, job->m_sizes[-1], *last - job->m_sizes[-1])) {
            result = false;
        }
    }

    System_MPQGlobals::GetWorkerPool()->Release(workerCount, workers);

    if (!result) {
        return false;
    }

    this->m_sectorIndex += sectorsDone;

    if (this->m_sectorIndex == offsetCount - 2) {
        uint32_t end = offsets[this->m_sectorIndex];

        if (!single) {
            if (this->m_hasChecksums) {
                uint32_t size = 4 * this->m_checksums.m_count;
                Blizzard::Mopaq::CompressionAlgorithm algorithm;
                algorithm.m_type = COMPRESSION_RLE;
                algorithm.m_level = 0;
                bool checksum;

                if (!System_SComp::Compress(this->m_checksums.m_data, &size, &checksum, this->m_checksums.m_data, size, 1, &algorithm)) {
                    return false;
                }

                if (!this->m_next->Write(this->m_checksums.m_data, offsets[this->m_sectorIndex], size)) {
                    return false;
                }

                end += size;
            }

            offsets[this->m_sectorIndex + 1] = end;
            this->m_sectorIndex++;

            if (!this->m_next->Write(offsets, 0, 4 * offsetCount)) {
                return false;
            }
        }

        this->m_callback(this->m_context, end);
    }

    return true;
}
