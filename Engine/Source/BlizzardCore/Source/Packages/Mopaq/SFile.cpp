#include <cstdint>
#include <cstring>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/Packages/iounit/IOAlignUnit.h"
#include "Engine/Source/BlizzardCore/Source/Packages/iounit/IOChecksumUnit.h"
#include "Engine/Source/BlizzardCore/Source/Packages/iounit/IOFileUnit.h"
#include "Engine/Source/BlizzardCore/Source/Packages/iounit/IOUnit.h"
#include "Engine/Source/BlizzardCore/Source/Packages/iounit/IOUnitContainer.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/IOMpqUnit.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/MPQEncrypt.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/MPQGlobals.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFile.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Streaming.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Log/Log.h"
#include "Engine/Source/BlizzardCore/Source/System/String/Path.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00B38344
Blizzard::Thread::LocalObject<System_SFile::RawReadInfo> System_SFile::s_rawReadInfo;

// STATIC: 0x00B3858C
uint32_t System_SFile::s_threadingMode;

// STATIC: 0x00AB9090
static const char* s_specialFiles[3] = { "(signature)", "(listfile)", "(attributes)" };

// INLINED
System_SFile::RawReadInfo::RawReadInfo() {
    this->m_retryNative = false;
}

void STORMAPI SFile::CreateOverlapped(SOVERLAPPED* overlapped) {
}

void STORMAPI SFile::DestroyOverlapped(SOVERLAPPED* overlapped) {
}

// VERIFY: 65f10e31 auto
// OFFSET: 0x00426E20
bool System_SFile::CheckCrc(void* context, uint32_t crc) {
    return crc == static_cast<IOMpqArchiveUnit*>(context)->m_blockEntry.m_crc;
}

// VERIFY: dc1be6b1 auto
// OFFSET: 0x00426FD0
void Blizzard::Mopaq::SFileSetThreadingMode(uint32_t mode) {
    System_SFile::s_threadingMode = mode;
}

// VERIFY: 94d8b66b manual
// OFFSET: 0x0042ED90
IOUnit* System_SFile::GetLockedUnit(IOUnit* file) {
    if (!file || file->GetType() != 'mpq ') {
        return nullptr;
    }

    IOUnit* lock = file->m_next;

    if (!lock || lock->GetType() != 'lock') {
        return nullptr;
    }

    return lock->m_next;
}

// VERIFY: df584044 auto
// OFFSET: 0x0042EDD0
bool System_SFile::InsertLockedUnit(IOUnit* file, IOUnit* unit) {
    if (!file || !unit || file->GetType() != 'mpq ') {
        return false;
    }

    return IOLockUnit::ReplaceNext(file->m_next, unit);
}

// VERIFY: 07c1b49c manual
// OFFSET: 0x00437310
IOUnit* System_SFile::OpenFileUnit(IOUnitContainer* container, const char* path, int32_t mode) {
    if (mode & 0x40) {
        return System_Mopaq::OpenDiskFile(container, path, mode);
    }

    return IOFileUnit::Create(container, path, mode);
}

// VERIFY: 66befaed auto
// OFFSET: 0x00437340
IOUnit* System_SFile::CreateRangeUnit(IOUnitContainer* container, int64_t start, int64_t end, IOUnit* next) {
    return IORangeUnit::Create(container, start, end, next);
}

// VERIFY: 3c889fb5 manual
// OFFSET: 0x004609B0
int32_t Blizzard::Mopaq::SFileOpenFileEx(System_SFile::ARCHIVEREC* handle, const char* name, const Blizzard::Util::Digest<16>& md5, uint32_t flags, IOMpqUnit** result) {
    *result = nullptr;
    FoundFile location;
    location.m_record = nullptr;
    location.m_result = GetFileBlockEntry(handle, name, md5, flags, nullptr, &location.m_record, &location.m_blockIndex, nullptr, location.m_path);

    // The per-open profiling log went to class 3, which System_Log::Init hard-disables, so it only cost the global log mutex.

    return SFileOpenFoundFile(&location, name, flags, result);
}

// HoT: the second half of SFileOpenFileEx, so a caller that already ran the lookup opens from its result.
int32_t Blizzard::Mopaq::SFileOpenFoundFile(FoundFile* location, const char* name, uint32_t flags, IOMpqUnit** result) {
    IOMpqUnit* file = nullptr;
    *result = nullptr;
    System_SFile::ARCHIVEREC* archive = location->m_record;
    location->m_record = nullptr;
    uint32_t error = 104;
    uint32_t blockIndex = location->m_blockIndex;
    const char* path = location->m_path;
    int32_t found = location->m_result;

    switch (found) {
        case 0:
        case 4: {
            error = 2;
            goto fail;
        }

        case 1: {
            System_SFile::BLOCKENTRY entry;

            // A patch job can grow the block table while this runs, so copy the entry under the archive lock.
            {
                Blizzard::Lock::ScopedMutex lock(System_MPQGlobals::GetArchiveLock()->m_mutex);
                entry = archive->GetBlockEntry(blockIndex);
            }

            if (!(entry.m_flags & 0x80000000)) {
                error = 1006;
                goto fail;
            }

            IOChecksumUnit* checksum = nullptr;

            if (entry.m_flags & 0xFFF00) {
                uint32_t fileSize = entry.m_fileSize;
                uint32_t key = 0;

                if (entry.m_flags & 0xF0000) {
                    key = System_MPQEncrypt::Hash(Blizzard::String::FindFilename(name), 3);

                    if (entry.m_flags & 0x20000) {
                        key = fileSize ^ (key + static_cast<uint32_t>(entry.m_offset) - static_cast<uint32_t>(archive->m_archiveOffset));
                    }
                }

                archive->m_file->m_refCount.Increment();

                if (entry.m_flags & 0x1000000) {
                    IOUnitContainer container(sizeof(IOMpqArchiveReadUnit), sizeof(IOCrcUnit), sizeof(IOAlignUnit), sizeof(IOLockUnit), sizeof(IOMpqReadUnit), sizeof(IORangeUnit), 0);
                    IOUnit* range = System_SFile::CreateRangeUnit(&container, entry.m_offset, entry.m_offset + entry.m_compressedSize, archive->m_file);
                    IOUnit* read = IOMpqReadUnit::CreateSingle(&container, fileSize, entry.m_compressedSize, entry.m_flags, key, range);
                    IOUnit* crc = IOCrcUnit::Create(&container, fileSize, entry.m_crc ? System_SFile::CheckCrc : nullptr, false, &checksum, read);
                    IOUnit* align = IOAlignUnit::Create(&container, 0, fileSize, 1, false, false, 1, crc);
                    IOUnit* lock = IOLockUnit::Create(&container, align);
                    file = IOMpqArchiveReadUnit::Create(&container, archive, name, blockIndex, lock);
                } else {
                    bool checkCrc = !(entry.m_flags & 0x4000000) && entry.m_crc;
                    IOUnitContainer container(sizeof(IOMpqArchiveReadUnit), sizeof(IOCrcUnit), sizeof(IOAlignUnit), sizeof(IOLockUnit), sizeof(IOMpqReadUnit), sizeof(IORangeUnit), 0);
                    IOUnit* range = System_SFile::CreateRangeUnit(&container, entry.m_offset, entry.m_offset + entry.m_compressedSize, archive->m_file);
                    IOUnit* read = IOMpqReadUnit::Create(&container, fileSize, entry.m_compressedSize, archive->m_sectorSize, entry.m_flags, key, range);
                    IOUnit* crc = IOCrcUnit::Create(&container, fileSize, checkCrc ? System_SFile::CheckCrc : nullptr, false, &checksum, read);
                    IOUnit* align = IOAlignUnit::Create(&container, 0, archive->m_sectorSize, 1, true, true, 1, crc);
                    IOUnit* lock = IOLockUnit::Create(&container, align);
                    file = IOMpqArchiveReadUnit::Create(&container, archive, name, blockIndex, lock);
                }
            } else {
                if (entry.m_compressedSize != entry.m_fileSize) {
                    error = 1006;
                    goto fail;
                }

                archive->m_file->m_refCount.Increment();

                IOUnitContainer container(sizeof(IOMpqArchiveReadUnit), sizeof(IOCrcUnit), sizeof(IOLockUnit), sizeof(IORangeUnit), 0);
                IOUnit* range = System_SFile::CreateRangeUnit(&container, entry.m_offset, entry.m_offset + entry.m_fileSize, archive->m_file);
                IOUnit* crc = IOCrcUnit::Create(&container, entry.m_fileSize, entry.m_crc ? System_SFile::CheckCrc : nullptr, false, &checksum, range);
                IOUnit* lock = IOLockUnit::Create(&container, crc);
                file = IOMpqArchiveReadUnit::Create(&container, archive, name, blockIndex, lock);
            }

            if (checksum) {
                checksum->SetContext(file);
            }

            found = 1;
            break;
        }

        case 2:
        case 3: {
            IOUnitContainer container(sizeof(IOMpqDiskUnit), sizeof(IOUnbufferedFileUnit), 0);
            IOUnit* disk = System_SFile::OpenFileUnit(&container, path, (((flags & 4) | 2) >> 1) | 0x1004);
            file = IOMpqDiskUnit::Create(&container, archive, name, path, disk);
            found = 2;
            break;
        }

        default: {
            goto fail;
        }
    }

    if (!file) {
        goto fail;
    }

    if (archive) {
        System_SFile::ARCHIVEREC* lookupRef = archive;
        archive = nullptr;

        if (!lookupRef->RemoveInternalArchiveRef()) {
            goto fail;
        }
    }

    if (found) {
        void (*callback)(const char* name, void* param);
        void* param;

        {
            Blizzard::Lock::ScopedMutex lock(System_MPQGlobals::GetMopaqGlobalsLock()->m_mutex);
            System_MPQGlobals::MopaqGlobals* globals = System_MPQGlobals::GetMopaqGlobals();
            callback = globals->m_openCallback;
            param = globals->m_openCallbackParam;
        }

        if (callback) {
            callback(name, param);
        }

        if (System_MPQGlobals::GetMopaqGlobals()->m_md5Mode == 1 && file->HasMd5Check()) {
            int64_t size;
            file->GetFileSize(&size);

            IOUnit* inner = System_SFile::GetLockedUnit(file);
            IOUnit* md5Unit = IOMd5Unit::Create(size, System_MPQGlobals::GetMopaqGlobals()->m_md5Callback, true, nullptr, inner);
            static_cast<IOChecksumUnit*>(md5Unit)->SetContext(file);

            bool inserted = System_SFile::InsertLockedUnit(file, md5Unit);

            if (!inner || !inserted) {
                inner->m_refCount.Increment();
                md5Unit->Release();
                found = 0;
            }
        }
    }

    *result = file;
    return found;

fail:
    System_SFile::s_lastError = error;
    found = 0;

    if (file) {
        file->Release();
        file = nullptr;
    }

    // GetFileBlockEntry handed us a ref on the archive, the original leaked it on every failed open.
    if (archive) {
        archive->RemoveInternalArchiveRef();
    }

    *result = file;
    return found;
}

// VERIFY: fc7d831c manual
// OFFSET: 0x00426E60
int32_t Blizzard::Mopaq::SFileOpenFileEx(System_SFile::ARCHIVEREC* handle, const char* name, uint32_t flags, IOMpqUnit** result) {
    Blizzard::Util::Digest<16> md5 = System_SFile::s_emptyMd5;
    return SFileOpenFileEx(handle, name, md5, flags, result);
}

// VERIFY: 019950a8 manual
// OFFSET: 0x00426F70
uint32_t Blizzard::Mopaq::SFileGetFileSize(IOMpqUnit* file, uint32_t* sizeHigh) {
    int64_t size = 0;

    if (file->GetFileSize(&size)) {
        return static_cast<uint32_t>(size);
    }

    return -1;
}

// VERIFY: 7d8723e2 auto
// OFFSET: 0x00426FA0
bool Blizzard::Mopaq::SFileWriteFile(IOMpqUnit* file, const void* data, uint32_t size) {
    if (!file->Write(data, file->m_position, size)) {
        return false;
    }

    file->m_position += size;
    return true;
}

// VERIFY: 3d1301e5 auto
// OFFSET: 0x004321A0
bool Blizzard::Mopaq::SFileCloseFile(IOMpqUnit* file) {
    return file->Release();
}

// VERIFY: 653d5feb manual
// OFFSET: 0x0045A1E0
bool Blizzard::Mopaq::SFileGetArchiveName(System_SFile::ARCHIVEREC* handle, char* name, uint32_t size) {
    *name = '\0';

    System_SFile::ARCHIVEREC* archive = System_SFile::GetArchivePtr(handle, true);

    if (!archive) {
        return false;
    }

    Blizzard::String::Copy(name, archive->m_name, size);
    archive->RemoveInternalArchiveRef();

    return true;
}

// VERIFY: 77e33064 manual
// OFFSET: 0x0042BF00
bool Blizzard::Mopaq::SFileUnloadFile(void* data) {
    SMemFree(data, __FILE__, __LINE__, 0);
    return true;
}

// VERIFY: 678ca972 manual
// OFFSET: 0x0045A4B0
bool Blizzard::Mopaq::SFileReadFile(IOMpqUnit* file, void* buffer, uint32_t count, uint32_t* bytesRead) {
    uint32_t read = 0;
    bool result = file->Read(buffer, file->m_position, count, &read);

    if (!result) {
        static_cast<System_SFile::RawReadInfo&>(System_SFile::s_rawReadInfo).m_retryNative = true;
        result = file->Read(buffer, file->m_position, count, &read);
        static_cast<System_SFile::RawReadInfo&>(System_SFile::s_rawReadInfo).m_retryNative = false;
    }

    file->m_position += read;

    if (bytesRead) {
        *bytesRead = read;
    }

    if (read != count) {
        System_SFile::s_lastError = 38;
    }

    if (!result) {
        char name[260];
        System_SFile::ARCHIVEREC* archive = file->GetArchive();

        if (archive) {
            SFileGetArchiveName(archive, name, sizeof(name));
            Blizzard::Streaming::MakePartFileName(name, sizeof(name));
            Blizzard::String::QuickFormat<1024> message("SFileReadFile - %s - %s", file->GetPath(), name);
            Blizzard::File::AddToLastErrorStack(0, message.m_buffer, 4);
            return false;
        }

        Blizzard::String::Copy(name, file->GetPath(), sizeof(name));
        Blizzard::Streaming::MakePartFileName(name, sizeof(name));
        Blizzard::String::QuickFormat<1024> message("SFileReadFile - %s", file->GetPath());
        Blizzard::File::AddToLastErrorStack(0, message.m_buffer, 4);
    }

    return result;
}

// VERIFY: dd4a5d22 manual
// OFFSET: 0x0045A610
int32_t Blizzard::Mopaq::SFileFileIsLocal(IOMpqUnit* file, int32_t flags) {
    if (!file) {
        return 1;
    }

    const char* path = file->GetPath();
    int32_t result = Blizzard::Streaming::CheckFileRange(nullptr, 0, 0, path, flags);

    if (result == 1) {
        return result;
    }

    int64_t offset = file->GetOffset();
    int64_t compressed = 0;
    int32_t size = file->GetCompressedSize(&compressed) ? static_cast<int32_t>(compressed) : -1;
    System_SFile::ARCHIVEREC* archive = file->GetArchive();

    // The original released the caller's file here, which left SFileImpl::m_file dangling for loose disk files.
    if (!archive) {
        return 1;
    }

    char name[260];
    SFileGetArchiveName(archive, name, sizeof(name));

    return Blizzard::Streaming::CheckFileRange(name, offset, size, path, flags);
}

// VERIFY: 672505f5 manual
// OFFSET: 0x0045B5A0
bool Blizzard::Mopaq::SFileLoadFile(System_SFile::ARCHIVEREC* handle, const char* name, void** data, uint32_t* size, uint32_t extra, uint32_t flags) {
    uint8_t* buffer = nullptr;
    *data = nullptr;

    if (size) {
        *size = 0;
    }

    Blizzard::Util::Digest<16> md5 = System_SFile::s_emptyMd5;
    IOMpqUnit* file = nullptr;

    if (SFileOpenFileEx(handle, name, md5, flags, &file)) {
        uint32_t bytes = SFileGetFileSize(file, nullptr);
        buffer = static_cast<uint8_t*>(SMemAlloc(bytes + extra, __FILE__, __LINE__, 0));

        if (SFileReadFile(file, buffer, bytes, nullptr)) {
            if (extra) {
                memset(buffer + bytes, 0, extra);
            }

            *data = buffer;
            buffer = nullptr;

            if (size) {
                *size = bytes;
            }
        }
    }

    SMemFree(buffer, __FILE__, __LINE__, 0);

    if (file) {
        file->Release();
    }

    return *data != nullptr;
}

// VERIFY: d17c29b0 manual
// OFFSET: 0x00426A40
bool System_SFile::ListfileParser::Next() {
    this->m_token = this->m_data + this->m_pos;

    while (this->m_pos < this->m_size) {
        char c = this->m_data[this->m_pos];

        if (c == '\r' || c == '\n') {
            this->m_data[this->m_pos] = '\0';
            this->m_pos++;

            if (this->m_pos < this->m_size && this->m_data[this->m_pos] == '\n') {
                this->m_pos++;
            }

            return false;
        }

        this->m_pos++;
    }

    if (this->m_special >= 2) {
        return true;
    }

    this->m_special++;
    this->m_token = s_specialFiles[this->m_special];
    this->m_isSpecial = true;

    return false;
}

// INLINED
System_SFile::ListfileParser::~ListfileParser() {
    SMemFree(this->m_data, __FILE__, __LINE__, 0);
}

// VERIFY: 9cda92e4 manual
// OFFSET: 0x0045B6A0
bool System_SFile::ARCHIVEREC::FillNewHashTable(int32_t hashBits) {
    ListfileParser parser;
    parser.m_special = 0;
    parser.m_pos = 0;
    parser.m_token = nullptr;
    parser.m_isSpecial = false;

    // 0x10 lets the lookup see this archive while s_OpenArchive still has it hidden for initialization.
    Blizzard::Mopaq::SFileLoadFile(this, "(listfile)", reinterpret_cast<void**>(&parser.m_data), &parser.m_size, 0, 0x10);

    while (!parser.Next()) {
        const char* name = parser.m_token;
        bool deleted;
        uint32_t hashIndex = SearchHashTable(this, name, true, &deleted);

        if (hashIndex == -1) {
            return false;
        }

        if (!this->m_hashEntryTable->AddEntry(name, this->m_hashTable[hashIndex].m_blockIndex, this->m_blockEntryTable)) {
            if (!this->IncreaseHashBits(&hashBits)) {
                return false;
            }

            parser.m_pos = 0;
            parser.m_special = 0;
            parser.m_isSpecial = false;
        }
    }

    return true;
}

// VERIFY: 97ac8f92 manual
// OFFSET: 0x00426EB0
int32_t Blizzard::Mopaq::SFileSetFilePointer(IOMpqUnit* file, int32_t distance, int32_t* distanceHigh, uint32_t method) {
    if (method == 0) {
        file->m_position = distance;
        return distance;
    }

    if (method == 1) {
        if (distance >= 0 || file->m_position >= static_cast<uint32_t>(-distance)) {
            file->m_position += distance;
        } else {
            file->m_position = 0;
        }

        return file->m_position;
    }

    if (method == 2) {
        int64_t size;

        if (!file->GetFileSize(&size)) {
            return file->m_position;
        }

        if (distance >= 0 || size >= -distance) {
            int32_t position = distance + static_cast<int32_t>(size);
            file->m_position = position;
            return position;
        }

        file->m_position = 0;
        return 0;
    }

    return file->m_position;
}
