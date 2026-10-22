#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include "Engine/Source/BlizzardCore/Source/Packages/iounit/IOFileUnit.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Linux/LinuxPath.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Unicode/Unicode.h"
#include "Engine/Source/Storm/H/storm.h"

bool IOFileUnit::ReadNative(uint32_t offset, const char* path, void* buffer, uint32_t count, uint32_t* bytesRead) {
    char nativePath[4096];

    if (!System_File::LinuxResolvePath(path, nativePath, sizeof(nativePath), false)) {
        return false;
    }

    // The Windows code opens with FILE_FLAG_NO_BUFFERING to bypass the cache; the sector-aligned read is kept, but
    // O_DIRECT is not used since its alignment rules depend on the filesystem.
    int fd = open(nativePath, O_RDONLY | O_CLOEXEC);

    if (fd < 0) {
        return false;
    }

    uint32_t head = offset & 0xFFF;
    uint32_t size = (head + count + 0xFFF) & ~0xFFFu;
    bool result = false;
    uint8_t* data = static_cast<uint8_t*>(SMemAlloc(size, __FILE__, __LINE__, 0));

    ssize_t read;

    do {
        read = pread(fd, data, size, static_cast<off_t>(offset - head));
    } while (read < 0 && errno == EINTR);

    if (read >= 0) {
        uint32_t available = static_cast<uint32_t>(read) > head ? static_cast<uint32_t>(read) - head : 0;
        uint32_t amount = available >= count ? count : available;
        Blizzard::String::MemCopy(buffer, data + head, amount);

        if (bytesRead) {
            *bytesRead = amount;
        }

        result = true;
    }

    SMemFree(data, __FILE__, __LINE__, 0);
    close(fd);

    return result;
}

IOUnbufferedFileUnit::IOUnbufferedFileUnit(Blizzard::File::StreamRecord* stream, bool ownsMemory) : IOFileUnit(stream, ownsMemory) {
    Blizzard::File::FileInfo* info = Blizzard::File::GetFileInfo(stream);
    this->m_size = info->m_size;

    char root[1024];
    Blizzard::String::Copy(root, info->m_path, Blizzard::File::GetRootChars(info->m_path) + 1);

    // GetDiskFreeSpaceA's bytes per sector: the filesystem block size.
    char nativeRoot[4096];
    struct statvfs fsInfo;
    bool valid = System_File::LinuxResolvePath(root[0] ? root : ".", nativeRoot, sizeof(nativeRoot), false) && statvfs(nativeRoot, &fsInfo) == 0;
    uint32_t bytesPerSector = valid ? static_cast<uint32_t>(fsInfo.f_bsize) : 0;

    if (!valid || !bytesPerSector || ((bytesPerSector - 1) & bytesPerSector)) {
        this->m_alignMask = 0xFFFF;
    } else {
        this->m_alignMask = bytesPerSector - 1;
    }

    this->m_buffer = Blizzard::Memory::Allocate(this->m_alignMask + 0x100000);
    this->m_alignedBuffer = reinterpret_cast<void*>(~static_cast<uintptr_t>(this->m_alignMask) & (reinterpret_cast<uintptr_t>(this->m_buffer) + this->m_alignMask));
}

#endif
