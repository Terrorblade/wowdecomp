#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Engine/Source/Storm/Source/Linux/StormLinux.h"
#include "Engine/Source/Storm/Source/IOUnit/IOFileUnit.h"
#include "Engine/Source/Storm/H/storm.h"

namespace Storm {

// IOFileParam carries CreateFile arguments; these are the values callers pass.
#define IO_GENERIC_READ 0x80000000u
#define IO_GENERIC_WRITE 0x40000000u
#define IO_CREATE_NEW 1
#define IO_CREATE_ALWAYS 2
#define IO_OPEN_EXISTING 3
#define IO_OPEN_ALWAYS 4
#define IO_TRUNCATE_EXISTING 5
#define IO_FILE_FLAG_NO_BUFFERING 0x20000000u

// The unit's handle is the file descriptor plus one, so a valid handle is never null.
static inline int32_t HandleToFd(void* handle) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(handle)) - 1;
}

static inline void* FdToHandle(int32_t fd) {
    return reinterpret_cast<void*>(static_cast<intptr_t>(fd) + 1);
}

// Share modes have no POSIX equivalent and are ignored; O_DIRECT is not used for FILE_FLAG_NO_BUFFERING since the
// unbuffered unit already aligns its own transfers.
static void* OpenFileHandle(const IOFileParam* param) {
    if (!param->m_name) {
        return nullptr;
    }

    bool create = param->m_creation != IO_OPEN_EXISTING && param->m_creation != IO_TRUNCATE_EXISTING;
    char path[MAX_PATH * 4];

    if (!StormLinux::ResolvePath(param->m_name, path, sizeof(path), create)) {
        return nullptr;
    }

    int32_t flags = O_CLOEXEC;
    bool read = (param->m_access & IO_GENERIC_READ) != 0;
    bool write = (param->m_access & IO_GENERIC_WRITE) != 0;

    if (read && write) {
        flags |= O_RDWR;
    } else if (write) {
        flags |= O_WRONLY;
    } else {
        flags |= O_RDONLY;
    }

    switch (param->m_creation) {
        case IO_CREATE_NEW:
            flags |= O_CREAT | O_EXCL;
            break;
        case IO_CREATE_ALWAYS:
            flags |= O_CREAT | O_TRUNC;
            break;
        case IO_OPEN_ALWAYS:
            flags |= O_CREAT;
            break;
        case IO_TRUNCATE_EXISTING:
            flags |= O_TRUNC;
            break;
        default:
            break;
    }

    int32_t fd = open(path, flags, 0644);

    return fd >= 0 ? FdToHandle(fd) : nullptr;
}

IOUnit::IOUnit() {
    this->m_refcount = nullptr;
    this->m_count = 1;
}

void IOUnit::Release() {
    IORefcount* refcount = this->m_refcount;

    if (--this->m_count == 0) {
        delete this;
    }

    if (refcount) {
        refcount->Release();
    }
}

IOWin32FileUnit::IOWin32FileUnit(void* handle) {
    this->m_handle = handle;
    this->m_position = -1;
    this->Seek(0);
}

bool IOWin32FileUnit::Seek(int64_t offset) {
    if (this->m_position != offset) {
        if (lseek(HandleToFd(this->m_handle), static_cast<off_t>(offset), SEEK_SET) == static_cast<off_t>(-1)) {
            this->m_position = -1;
            return false;
        }

        this->m_position = offset;
    }

    return true;
}

bool IOWin32FileUnit::Read(void* buffer, int64_t offset, uint32_t count, uint32_t* bytesRead) {
    uint32_t minimum = 1;
    uint32_t localRead;

    if (!bytesRead) {
        minimum = count;
        bytesRead = &localRead;
    }

    *bytesRead = 0;

    for (int32_t tries = 0; tries < 3; tries++) {
        if (!this->Seek(offset)) {
            return false;
        }

        // ReadFile returns once count bytes are read or the file ends; read() may stop short, so loop.
        bool result = true;
        uint32_t total = 0;

        while (total < count) {
            ssize_t got = read(HandleToFd(this->m_handle), static_cast<uint8_t*>(buffer) + total, count - total);

            if (got < 0) {
                if (errno == EINTR) {
                    continue;
                }

                result = false;
                break;
            }

            if (got == 0) {
                break;
            }

            total += static_cast<uint32_t>(got);
        }

        *bytesRead = total;
        this->m_position = offset + *bytesRead;

        if (result) {
            return *bytesRead >= minimum;
        }

        this->m_position = -1;
        StormLinux::SleepMs(10);
    }

    return false;
}

bool IOWin32FileUnit::Write(const void* buffer, int64_t offset, uint32_t count) {
    uint32_t written = 0;

    if (!this->Seek(offset)) {
        return false;
    }

    while (written < count) {
        ssize_t put = write(HandleToFd(this->m_handle), static_cast<const uint8_t*>(buffer) + written, count - written);

        if (put < 0 && errno == EINTR) {
            continue;
        }

        if (put <= 0) {
            break;
        }

        written += static_cast<uint32_t>(put);
    }

    if (written == count) {
        this->m_position = offset + written;
        return true;
    }

    this->m_position = -1;

    return false;
}

bool IOWin32FileUnit::Flush() {
    return fsync(HandleToFd(this->m_handle)) == 0;
}

bool IOWin32FileUnit::GetFileSize(int64_t* size) {
    struct stat st;

    if (fstat(HandleToFd(this->m_handle), &st) != 0) {
        *size = 0;
        return false;
    }

    *size = static_cast<int64_t>(st.st_size);

    return true;
}

bool IOWin32FileUnit::SetFileSize(int64_t size) {
    if (!this->Seek(size)) {
        return false;
    }

    return ftruncate(HandleToFd(this->m_handle), static_cast<off_t>(size)) == 0;
}

void* IOWin32FileUnit::Win32Handle() {
    return this->m_handle;
}

bool IOUnbufferedFileUnit::Read(void* buffer, int64_t offset, uint32_t count, uint32_t* bytesRead) {
    if (!(this->m_alignMask & reinterpret_cast<uintptr_t>(buffer)) && !(this->m_alignMask & count)) {
        return this->m_unit->Read(buffer, offset, count, bytesRead);
    }

    if (!this->m_buffer) {
        return false;
    }

    uint32_t total = 0;
    bool result = true;

    while (count) {
        uint32_t chunk = count >= 0x100000 ? 0x100000 : count;
        uint32_t size = chunk;

        if (chunk & this->m_alignMask) {
            size = (this->m_alignMask + chunk) & ~this->m_alignMask;
        }

        uint32_t read;
        result = this->m_unit->Read(this->m_buffer, offset, size, &read);

        if (!result) {
            break;
        }

        read = size < chunk ? size : chunk;
        memcpy(buffer, this->m_buffer, read);
        buffer = static_cast<uint8_t*>(buffer) + read;
        offset += read;
        count -= read;
        total += read;

        if (read < chunk) {
            break;
        }
    }

    if (!bytesRead) {
        return count == 0;
    }

    *bytesRead = total;

    return result;
}

bool IOUnbufferedFileUnit::Write(const void* buffer, int64_t offset, uint32_t count) {
    if (!(this->m_alignMask & reinterpret_cast<uintptr_t>(buffer)) && !(this->m_alignMask & count)) {
        return this->m_unit->Write(buffer, offset, count);
    }

    if (!this->m_buffer) {
        return false;
    }

    bool result = true;

    while (count) {
        uint32_t chunk = count >= 0x100000 ? 0x100000 : count;
        memcpy(this->m_buffer, buffer, chunk);
        buffer = static_cast<const uint8_t*>(buffer) + chunk;

        uint32_t size = chunk;

        if (chunk & this->m_alignMask) {
            size = (this->m_alignMask + chunk) & ~this->m_alignMask;
        }

        result = this->m_unit->Write(this->m_buffer, offset, size);

        if (!result) {
            break;
        }

        offset += chunk;
        count -= chunk;
    }

    if (offset > this->m_size) {
        this->m_size = offset;
    }

    return result;
}

bool IOUnbufferedFileUnit::Flush() {
    return this->m_unit->Flush();
}

bool IOUnbufferedFileUnit::SetFileSize(int64_t size) {
    this->m_size = size;

    return this->m_unit->SetFileSize((size + this->m_alignMask) & ~this->m_alignMask);
}

bool IOUnbufferedFileUnit::GetFileSize(int64_t* size) {
    *size = this->m_size;

    return true;
}

IOUnit::~IOUnit() {
}

IOWin32FileUnit::~IOWin32FileUnit() {
    close(HandleToFd(this->m_handle));
    this->m_handle = nullptr;
}

IOUnbufferedFileUnit::IOUnbufferedFileUnit(void* handle, const IOFileParam* param) {
    this->m_unit = new IOWin32FileUnit(handle);
    this->m_size = 0;
    this->m_unit->GetFileSize(&this->m_size);
    SStrCopy(this->m_path, param->m_name, sizeof(this->m_path));
    this->m_buffer = aligned_alloc(4096, 0x100000);

    // GetDiskFreeSpace's sector size; the filesystem's preferred block size plays that role here.
    struct stat st;

    if (fstat(HandleToFd(handle), &st) != 0 || st.st_blksize <= 0 || ((st.st_blksize - 1) & st.st_blksize)) {
        this->m_alignMask = 0xFFFF;
    } else {
        this->m_alignMask = static_cast<uint32_t>(st.st_blksize) - 1;
    }
}

void* IOUnbufferedFileUnit::Win32Handle() {
    return this->m_unit->Win32Handle();
}

IOUnbufferedFileUnit* IOUnbufferedFileUnit::Open(const IOFileParam* param) {
    void* handle = OpenFileHandle(param);

    if (!handle) {
        return nullptr;
    }

    return new IOUnbufferedFileUnit(handle, param);
}

IOWin32FileUnit* IOWin32FileUnit::Open(const IOFileParam* param) {
    void* handle = OpenFileHandle(param);

    if (!handle) {
        return nullptr;
    }

    return new IOWin32FileUnit(handle);
}

IOUnit* IOFileUnit::Create(const IOFileParam& param, IOUnit* next) {
    if (next) {
        return nullptr;
    }

    if (param.m_flags & IO_FILE_FLAG_NO_BUFFERING) {
        return IOUnbufferedFileUnit::Open(&param);
    }

    return IOWin32FileUnit::Open(&param);
}

IOUnbufferedFileUnit::~IOUnbufferedFileUnit() {
    free(this->m_buffer);

    if (this->m_unit) {
        delete this->m_unit;
    }

    IOFileParam param;
    param.m_name = this->m_path;
    param.m_access = IO_GENERIC_WRITE;
    param.m_share = 0;
    param.m_creation = IO_OPEN_EXISTING;
    param.m_flags = 0;

    IOUnit* unit = IOWin32FileUnit::Open(&param);

    if (unit) {
        unit->SetFileSize(this->m_size);
        unit->Release();
    }
}

}

#endif
