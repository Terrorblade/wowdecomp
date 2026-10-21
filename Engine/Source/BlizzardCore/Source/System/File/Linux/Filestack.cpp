#if defined(WOWDECOMP_PLATFORM_LINUX)

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include "Engine/Source/BlizzardCore/Source/System/Debug/Debug.h"
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/File/Linux/LinuxPath.h"
#include "Engine/Source/BlizzardCore/Source/System/Lock/Lock.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"
#include "Engine/Source/BlizzardCore/Source/System/Memory/TempBuffer.h"
#include "Engine/Source/BlizzardCore/Source/System/String/Path.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/BlizzardCore/Source/System/Thread/Thread.h"
#include "Engine/Source/BlizzardCore/Source/System/Time/Time.h"
#include "Engine/Source/Storm/H/storm.h"

// StreamRecord::m_fileHandle holds the file descriptor itself, so a closed or failed stream (-1) compares equal to
// INVALID_HANDLE_VALUE like the Windows handle does.

namespace {

// The Windows values ToCreateFlags produces and the attribute bits FromNativeAttributes/ToNativeAttributes map.
const DWORD kGenericRead = 0x80000000;
const DWORD kGenericWrite = 0x40000000;
const DWORD kCreateNew = 1;
const DWORD kCreateAlways = 2;
const DWORD kOpenExisting = 3;
const DWORD kOpenAlways = 4;
const DWORD kFlagWriteThrough = 0x80000000;
const DWORD kFlagNoBuffering = 0x20000000;
const int32_t kAttributeReadOnly = 0x1;
const int32_t kAttributeHidden = 0x2;
const int32_t kAttributeSystem = 0x4;
const int32_t kAttributeDirectory = 0x10;
const int32_t kAttributeArchive = 0x20;
const int32_t kAttributeNormal = 0x80;
const int32_t kAttributeTemporary = 0x100;

inline int32_t ToFd(void* handle) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(handle));
}

inline void* FromFd(int32_t fd) {
    return reinterpret_cast<void*>(static_cast<intptr_t>(fd));
}

inline bool ValidHandle(void* handle) {
    return handle != INVALID_HANDLE_VALUE && handle != nullptr;
}

void ResolvePath(const char* path, char* out, size_t outSize, bool forCreate) {
    if (!System_File::LinuxResolvePath(path ? path : "", out, outSize, forCreate) && outSize) {
        // Too long: keep it as given (minus Windows separators) and let the system call fail.
        Blizzard::String::Copy(out, path ? path : "", static_cast<int32_t>(outSize));

        for (char* p = out; *p; p++) {
            if (*p == '\\') {
                *p = '/';
            }
        }
    }
}

// Unix seconds + nanoseconds to a Windows FILETIME (100 ns ticks since 1601), for Time::FromFileTime.
uint64_t ToFileTime(const timespec& time) {
    return static_cast<uint64_t>(static_cast<int64_t>(time.tv_sec) + 11644473600LL) * 10000000ULL + static_cast<uint64_t>(time.tv_nsec / 100);
}

// The FILE_ATTRIBUTE_* word GetFileAttributes would report.
int32_t NativeAttributes(const struct stat& st) {
    int32_t attributes = 0;

    if (S_ISDIR(st.st_mode)) {
        attributes |= kAttributeDirectory;
    }

    if (!(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) {
        attributes |= kAttributeReadOnly;
    }

    if (!attributes) {
        attributes = kAttributeNormal;
    }

    return attributes;
}

// Fills the parts of a FileInfo GetFileAttributesEx provides. The creation time is the birth time where the
// filesystem records one, else the change time.
void FillInfo(int32_t dirfd, const char* path, int32_t flags, const struct stat& st, Blizzard::File::FileInfo* info) {
    timespec created = st.st_ctim;
    struct statx stx;

    if (statx(dirfd, path, flags, STATX_BTIME, &stx) == 0 && (stx.stx_mask & STATX_BTIME)) {
        created.tv_sec = stx.stx_btime.tv_sec;
        created.tv_nsec = stx.stx_btime.tv_nsec;
    }

    info->m_createTime = Blizzard::Time::FromFileTime(ToFileTime(created));
    info->m_modTime = Blizzard::Time::FromFileTime(ToFileTime(st.st_mtim));
    info->m_accessTime = Blizzard::Time::FromFileTime(ToFileTime(st.st_atim));
    int32_t attributes = System_File::FromNativeAttributes(NativeAttributes(st));
    info->m_attributes = attributes;
    info->m_regularFile = (attributes & 0x20) != 0;
    info->m_existence = ((attributes & 0x40) != 0) + 1;
}

int32_t ToOpenFlags(DWORD desiredaccess, DWORD creationdisposition, DWORD flagsandattributes) {
    int32_t flags = O_CLOEXEC;

    if ((desiredaccess & kGenericRead) && (desiredaccess & kGenericWrite)) {
        flags |= O_RDWR;
    } else if (desiredaccess & kGenericWrite) {
        flags |= O_WRONLY;
    } else {
        flags |= O_RDONLY;
    }

    switch (creationdisposition) {
        case kCreateNew:
            flags |= O_CREAT | O_EXCL;
            break;
        case kCreateAlways:
            flags |= O_CREAT | O_TRUNC;
            break;
        case kOpenAlways:
            flags |= O_CREAT;
            break;
        default:
            break;
    }

    // FILE_FLAG_NO_BUFFERING has no safe counterpart (O_DIRECT needs aligned buffers); write-through maps to O_DSYNC.
    if (flagsandattributes & kFlagWriteThrough) {
        flags |= O_DSYNC;
    }

    return flags;
}

// CreateFileA: share modes have no Linux meaning; directories cannot be opened as files.
int32_t OpenNative(const char* path, uint32_t mode) {
    DWORD sharemode;
    DWORD desiredaccess;
    DWORD creationdisposition;
    DWORD flagsandattributes;
    System_File::ToCreateFlags(mode, &sharemode, &desiredaccess, &creationdisposition, &flagsandattributes);

    int32_t flags = ToOpenFlags(desiredaccess, creationdisposition, flagsandattributes);
    char resolved[1024];
    ResolvePath(path, resolved, sizeof(resolved), (flags & O_CREAT) != 0);

    int32_t fd;

    do {
        fd = open(resolved, flags, 0666);
    } while (fd == -1 && errno == EINTR);

    if (fd == -1) {
        return -1;
    }

    struct stat st;

    if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode)) {
        close(fd);
        return -1;
    }

    // Keep 0 free: a null m_fileHandle means "no file" to some callers.
    if (fd == 0) {
        int32_t moved = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        close(fd);
        fd = moved;
    }

    return fd;
}

}

System_File::IOFunc System_File::read_func = System_File::read_first;

System_File::IOFunc System_File::write_func = System_File::write_first;

bool System_File::s_read_init;

Blizzard::Lock::Atomic System_File::s_readInitCount;

bool System_File::s_write_init;

Blizzard::Lock::Atomic System_File::s_writeInitCount;

Blizzard::Lock::Mutex System_File::s_ioMutex;

Blizzard::Lock::Mutex* System_File::s_ioLock;

int32_t System_File::ToNativeAttributes(int32_t attributes) {
    int32_t result = (attributes & 0x1) != 0 ? kAttributeReadOnly : 0;

    if (attributes & 0x2) {
        result |= kAttributeHidden;
    }

    if (attributes & 0x4) {
        result |= kAttributeSystem;
    }

    if (attributes & 0x8) {
        result |= kAttributeArchive;
    }

    if (attributes & 0x10) {
        result |= kAttributeTemporary;
    }

    if (attributes & 0x20) {
        result |= kAttributeNormal;
    }

    if (attributes & 0x40) {
        result |= kAttributeDirectory;
    }

    return result;
}

int32_t System_File::FromNativeAttributes(int32_t attributes) {
    int32_t result = (attributes & kAttributeReadOnly) != 0 ? 0x1 : 0;

    if (attributes & kAttributeHidden) {
        result |= 0x2;
    }

    if (attributes & kAttributeSystem) {
        result |= 0x4;
    }

    if (attributes & kAttributeArchive) {
        result |= 0x8;
    }

    if (attributes & kAttributeTemporary) {
        result |= 0x10;
    }

    if (attributes & kAttributeDirectory) {
        result |= 0x40;
    }

    if ((attributes & kAttributeNormal) || !(attributes & kAttributeDirectory)) {
        result |= 0x20;
    }

    return result;
}

bool System_File::ReadP(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    OVERLAPPED overlapped;
    overlapped.Offset = static_cast<DWORD>(parms->m_pos);
    overlapped.OffsetHigh = static_cast<DWORD>(parms->m_pos >> 32);
    overlapped.Internal = 0;
    overlapped.InternalHigh = 0;
    overlapped.hEvent = nullptr;

    return read_func(parms->m_stream, parms->m_buffer, &overlapped, &parms->m_amount);
}

bool System_File::WriteP(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    OVERLAPPED overlapped;
    overlapped.Offset = static_cast<DWORD>(parms->m_pos);
    overlapped.OffsetHigh = static_cast<DWORD>(parms->m_pos >> 32);
    overlapped.Internal = 0;
    overlapped.InternalHigh = 0;
    overlapped.hEvent = nullptr;

    return write_func(parms->m_stream, parms->m_buffer, &overlapped, &parms->m_amount);
}

bool System_File::Close(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    Blizzard::File::StreamRecord* stream = parms->m_stream;

    if (ValidHandle(stream->m_fileHandle)) {
        close(ToFd(stream->m_fileHandle));
    }

    SMemFree(stream, __FILE__, __LINE__, 0);

    return true;
}

bool System_File::GetRootChars(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    const char* path = parms->m_path;
    Blizzard::Memory::TempBuffer<char, 256> buffer((path ? static_cast<uint32_t>(strlen(path)) : 0) + 1);

    if (buffer.m_count < 2) {
        parms->m_pos = 0;
        return true;
    }

    Blizzard::String::MakeUnivPath(path, buffer.m_data, buffer.m_count);

    // The only root on Linux is "/".
    parms->m_pos = buffer.m_data[0] == '/' ? 1 : 0;

    return true;
}

void System_File::GetFileInfoByFile(Blizzard::File::StreamRecord* file) {
    if (file->m_infoValid) {
        return;
    }

    file->m_infoValid = false;
    struct stat st;

    if (!ValidHandle(file->m_fileHandle) || fstat(ToFd(file->m_fileHandle), &st) != 0) {
        return;
    }

    if (!file->m_streaming || !file->m_streaming->m_entry) {
        file->m_info.m_size = st.st_size;
    }

    FillInfo(ToFd(file->m_fileHandle), "", AT_EMPTY_PATH, st, &file->m_info);
    file->m_infoValid = true;
}

bool System_File::Exists(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    char path[1024];
    ResolvePath(parms->m_path, path, sizeof(path), false);

    struct stat st;

    if (stat(path, &st) != 0) {
        parms->m_info->m_existence = 0;
    } else if (S_ISDIR(st.st_mode)) {
        parms->m_info->m_existence = 2;
    } else {
        parms->m_info->m_existence = 1;
    }

    return true;
}

bool System_File::MakeAbsolutePath(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    // _fullpath: join with the working directory and fold "." and ".." lexically; the file need not exist.
    char joined[2048];
    const char* path = parms->m_path ? parms->m_path : "";

    if (path[0] == '/' || path[0] == '\\') {
        Blizzard::String::Copy(joined, path, sizeof(joined));
    } else {
        char cwd[1024];

        if (!getcwd(cwd, sizeof(cwd))) {
            cwd[0] = '\0';
        }

        Blizzard::String::Format(joined, sizeof(joined), "%s/%s", cwd, path);
    }

    char buffer[1024];
    size_t length = 0;
    const char* cursor = joined;

    while (*cursor) {
        while (*cursor == '/' || *cursor == '\\') {
            cursor++;
        }

        if (!*cursor) {
            break;
        }

        const char* end = cursor;

        while (*end && *end != '/' && *end != '\\') {
            end++;
        }

        size_t count = static_cast<size_t>(end - cursor);

        if (count == 1 && cursor[0] == '.') {
        } else if (count == 2 && cursor[0] == '.' && cursor[1] == '.') {
            while (length > 0 && buffer[length - 1] != '/') {
                length--;
            }

            if (length > 0) {
                length--;
            }
        } else if (length + count + 2 < sizeof(buffer)) {
            buffer[length++] = '/';
            memcpy(buffer + length, cursor, count);
            length += count;
        }

        cursor = end;
    }

    if (length == 0) {
        buffer[length++] = '/';
    }

    buffer[length] = '\0';

    FromNativeName(parms->m_cwdbuf, parms->m_cwdbuflen, buffer);
    Blizzard::String::MakeUnivPath(parms->m_cwdbuf, parms->m_cwdbuf, parms->m_cwdbuflen);

    path = parms->m_path;

    if (path[(path ? strlen(path) : 0) - 1] == '/') {
        Blizzard::String::ForceTrailingSeparator(parms->m_cwdbuf, parms->m_cwdbuflen, '/');
    }

    return true;
}

bool System_File::Move(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    char src[1024];
    char dst[1024];
    ResolvePath(parms->m_path, src, sizeof(src), false);
    ResolvePath(parms->m_newpath, dst, sizeof(dst), true);

    // MoveFileA does not replace an existing destination.
    struct stat st;

    if (lstat(dst, &st) == 0) {
        return false;
    }

    return rename(src, dst) == 0;
}

bool System_File::Delete(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    char path[1024];
    ResolvePath(parms->m_path, path, sizeof(path), false);

    return unlink(path) == 0;
}

bool System_File::read_overlapped(Blizzard::File::StreamRecord* file, void* buffer, OVERLAPPED* overlapped, int32_t* count) {
    if (!ValidHandle(file->m_fileHandle)) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    if (!*count) {
        return true;
    }

    int32_t fd = ToFd(file->m_fileHandle);
    int64_t offset = overlapped ? ((static_cast<int64_t>(overlapped->OffsetHigh) << 32) | overlapped->Offset) : 0;
    int32_t total = 0;
    bool result = true;

    // ReadFile on a disk file only returns short at end of file.
    while (total < *count) {
        ssize_t bytes = overlapped ? pread(fd, static_cast<uint8_t*>(buffer) + total, *count - total, offset + total) : read(fd, static_cast<uint8_t*>(buffer) + total, *count - total);

        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }

            result = false;
            break;
        }

        if (bytes == 0) {
            break;
        }

        total += static_cast<int32_t>(bytes);
    }

    *count = total;

    if (result) {
        return true;
    }

    Blizzard::File::SetLastError(5);
    Blizzard::File::AddToLastErrorStack(5, Blizzard::String::QuickFormat<1024>("Win32 Read - %s", file->m_pathname).m_buffer, 1);

    return false;
}

bool System_File::write_overlapped(Blizzard::File::StreamRecord* file, void* buffer, OVERLAPPED* overlapped, int32_t* count) {
    if (!ValidHandle(file->m_fileHandle)) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    if (!*count) {
        return true;
    }

    file->m_infoValid = false;
    int32_t fd = ToFd(file->m_fileHandle);
    int64_t offset = overlapped ? ((static_cast<int64_t>(overlapped->OffsetHigh) << 32) | overlapped->Offset) : 0;
    int32_t total = 0;
    bool result = true;

    while (total < *count) {
        ssize_t bytes = overlapped ? pwrite(fd, static_cast<uint8_t*>(buffer) + total, *count - total, offset + total) : write(fd, static_cast<uint8_t*>(buffer) + total, *count - total);

        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }

            result = false;
            break;
        }

        if (bytes == 0) {
            result = false;
            break;
        }

        total += static_cast<int32_t>(bytes);
    }

    *count = total;

    if (result) {
        return true;
    }

    Blizzard::File::SetLastError(6);
    Blizzard::File::AddToLastErrorStack(6, Blizzard::String::QuickFormat<1024>("Win32 Write - %s", file->m_pathname).m_buffer, 1);

    return false;
}

bool System_File::ToCreateFlags(uint32_t flags, DWORD* sharemode, DWORD* desiredaccess, DWORD* creationdisposition, DWORD* flagsandattributes) {
    *sharemode = (flags >> 2) & 3;
    *desiredaccess = ((flags * 4) | (flags & 2)) << 29;
    *creationdisposition = 0;

    if ((flags & 0x100) || (flags & 0x400)) {
        *creationdisposition = (flags & 0x800) ? kCreateNew : kCreateAlways;
    } else if (flags & 0x800) {
        *creationdisposition = kCreateNew;
    } else {
        *creationdisposition = kOpenAlways - ((flags & 0x1000) != 0);
    }

    *flagsandattributes = (2 * ((flags & 0x80) | 0x40)) | ((flags & 0x40) ? (kFlagWriteThrough | kFlagNoBuffering) : 0);

    return true;
}

bool System_File::SetWorkingDirectory(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    if (!parms->m_path) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    char path[1024];
    ResolvePath(parms->m_path, path, sizeof(path), false);

    return chdir(path) == 0;
}

bool System_File::Create(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    Blizzard::File::SetLastError(9);

    return false;
}

bool System_File::GetWorkingDirectory(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    if (!parms->m_cwdbuf || parms->m_cwdbuflen <= 0) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    *parms->m_cwdbuf = '\0';
    char buffer[1024];

    if (!getcwd(buffer, sizeof(buffer))) {
        buffer[0] = '\0';
    }

    FromNativeName(parms->m_cwdbuf, parms->m_cwdbuflen, buffer);

    return true;
}

bool System_File::ProcessDirFast(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    void* cookie = parms->m_cookie;
    const char* path = parms->m_path;
    Blizzard::File::ProcessDirCallback callback = parms->m_dircallback;

    char directory[1024];
    ResolvePath(path, directory, sizeof(directory), false);

    DIR* dir = opendir(directory[0] ? directory : ".");

    if (!dir) {
        Blizzard::Debug::Printf(Blizzard::Debug::StderrOutput, "failed to open %s\n", path);
        Blizzard::File::SetLastError(8);
        return false;
    }

    Blizzard::File::ProcessDirParms dirParms;
    dirParms.m_pathRoot = path;
    dirParms.m_cookie = cookie;

    while (dirent* entry = readdir(dir)) {
        const char* name = entry->d_name;

        if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]))) {
            continue;
        }

        bool isDir;

        if (entry->d_type == DT_DIR) {
            isDir = true;
        } else if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
            // FindFirstFile reports what a link points to.
            struct stat st;
            isDir = fstatat(dirfd(dir), name, &st, 0) == 0 && S_ISDIR(st.st_mode);
        } else {
            isDir = false;
        }

        char filename[1024];
        FromNativeName(filename, sizeof(filename), name);
        dirParms.m_filename = filename;
        dirParms.m_isDir = isDir;

        if (!callback(dirParms)) {
            closedir(dir);
            return false;
        }
    }

    closedir(dir);

    return true;
}

bool System_File::Flush(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    if (!ValidHandle(parms->m_stream->m_fileHandle)) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    return fsync(ToFd(parms->m_stream->m_fileHandle)) == 0;
}

bool System_File::GetFileInfo(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    if (parms->m_path) {
        char path[1024];
        ResolvePath(parms->m_path, path, sizeof(path), false);

        struct stat st;

        if (stat(path, &st) != 0) {
            Blizzard::File::SetLastError(8);
            return false;
        }

        parms->m_info->m_size = st.st_size;
        parms->m_info->m_path = nullptr;
        FillInfo(AT_FDCWD, path, 0, st, parms->m_info);

        return true;
    }

    if (parms->m_stream) {
        GetFileInfoByFile(parms->m_stream);
        Blizzard::File::StreamRecord* stream = parms->m_stream;
        parms->m_info = &stream->m_info;

        if (!stream->m_info.m_path) {
            stream->m_info.m_path = stream->m_pathname;
        }

        return parms->m_stream->m_infoValid;
    }

    Blizzard::File::SetLastError(8);

    return false;
}

bool System_File::GetFreeSpace(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    const char* path = parms->m_path;

    if (path && *path) {
        char directory[MAX_PATH];
        Blizzard::String::Copy(directory, path, std::min<int32_t>(static_cast<int32_t>(Blizzard::String::FindFilename(path) - path + 1), MAX_PATH));

        char resolved[1024];
        ResolvePath(directory, resolved, sizeof(resolved), false);

        struct statvfs st;

        if (statvfs(resolved[0] ? resolved : ".", &st) == 0) {
            parms->m_pos = static_cast<int64_t>(st.f_bavail) * static_cast<int64_t>(st.f_frsize);
            return true;
        }
    }

    Blizzard::File::SetLastError(8);

    return false;
}

bool System_File::GetPos(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    Blizzard::File::StreamRecord* stream = parms->m_stream;
    off_t pos;

    if (!ValidHandle(stream->m_fileHandle) || (pos = lseek(ToFd(stream->m_fileHandle), 0, SEEK_CUR)) == -1) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    parms->m_pos = pos;

    return true;
}

bool System_File::IsAbsolutePath(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    const char* path = parms->m_path;

    // Like the Windows test, a bare root does not count.
    if ((path[0] == '/' || path[0] == '\\') && path[1]) {
        return true;
    }

    Blizzard::File::SetLastError(8);

    return false;
}

bool System_File::IsReadOnly(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    char path[1024];
    ResolvePath(parms->m_path, path, sizeof(path), false);

    struct stat st;

    if (stat(path, &st) == 0) {
        return (NativeAttributes(st) & kAttributeReadOnly) != 0;
    }

    Blizzard::File::SetLastError(8);

    return false;
}

bool System_File::CreateDirectory(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    bool recursive = parms->m_recursive;
    const char* path = parms->m_path;
    bool grantEveryone = parms->m_grantEveryone;

    if (!path) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    char resolved[1024];

    if (!LinuxResolvePath(path, resolved, sizeof(resolved), true)) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    // The Windows ACL grants Builtin Users full access; the closest is a world-writable directory.
    mode_t mode = grantEveryone ? 0777 : 0755;

    if (recursive) {
        char* start = resolved;
        char* end = start;

        while (*end == '/') {
            end++;
        }

        for (;;) {
            while (*end && *end != '/') {
                end++;
            }

            char saved = *end;
            *end = '\0';

            if (mkdir(start, mode) != 0) {
                struct stat st;

                if ((errno != EEXIST && errno != EACCES) || stat(start, &st) != 0 || !S_ISDIR(st.st_mode)) {
                    Blizzard::File::SetLastError(8);
                    return false;
                }
            } else if (grantEveryone) {
                chmod(start, mode);
            }

            *end = saved;

            while (*end == '/') {
                end++;
            }

            if (!*end) {
                return true;
            }
        }
    }

    if (mkdir(resolved, mode) == 0) {
        if (grantEveryone) {
            chmod(resolved, mode);
        }

        return true;
    }

    return errno == EEXIST || errno == EACCES;
}

bool System_File::Copy(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    bool overwrite = parms->m_overwrite;
    const char* src = parms->m_path;
    const char* dst = parms->m_newpath;

    Blizzard::File::StreamRecord* input;

    if (!Blizzard::File::Open(src, 0x1005, input)) {
        Blizzard::File::SetLastError(2);
        return false;
    }

    Blizzard::File::StreamRecord* output;

    if (!Blizzard::File::Open(dst, (overwrite ? 0x400 : 0xC00) | 0x2, output)) {
        Blizzard::File::Close(input);
        Blizzard::File::SetLastError(4);
        return false;
    }

    int64_t size = Blizzard::File::GetFileInfo(input)->m_size;
    int64_t offset = 0;
    bool result = true;
    void* buffer = Blizzard::Memory::Allocate(size >= 0xA00000 ? 0xA00000 : static_cast<uint32_t>(size));

    if (size > 0) {
        for (;;) {
            int32_t count = size - offset >= 0xA00000 ? 0xA00000 : static_cast<int32_t>(size - offset);

            if (!Blizzard::File::Read(input, buffer, offset, &count)) {
                break;
            }

            if (!Blizzard::File::Write(output, buffer, offset, &count)) {
                break;
            }

            offset += count;

            if (offset >= size) {
                goto done;
            }
        }

        result = false;
    }

done:
    Blizzard::Memory::Free(buffer);
    Blizzard::File::Close(input);
    Blizzard::File::Close(output);

    if (!result) {
        Blizzard::File::Delete(dst);
    }

    return result;
}

bool System_File::Open(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    int32_t mode = parms->m_mode;
    char path[1024];
    Blizzard::String::Copy(path, parms->m_path, sizeof(path));

    DWORD sharemode;
    DWORD desiredaccess;
    DWORD creationdisposition;
    DWORD flagsandattributes;

    if (!ToCreateFlags(mode, &sharemode, &desiredaccess, &creationdisposition, &flagsandattributes)) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    int32_t fd = OpenNative(path, mode);

    if (fd == -1) {
        Blizzard::File::SetLastError(4);
        Blizzard::File::AddToLastErrorStack(4, Blizzard::String::QuickFormat<1024>("Win32 Open - %s", path).m_buffer, 1);
        return false;
    }

    Blizzard::File::StreamRecord* stream = parms->m_stream;

    if (!stream) {
        uint32_t length = static_cast<uint32_t>(strlen(path));
        stream = static_cast<Blizzard::File::StreamRecord*>(SMemAlloc(sizeof(Blizzard::File::StreamRecord) + length + 1, __FILE__, __LINE__, SMEM_FLAG_ZEROMEMORY));
        stream->m_pathname = reinterpret_cast<char*>(stream + 1);
        Blizzard::String::Copy(stream->m_pathname, path, length + 1);
    }

    stream->m_fileHandle = FromFd(fd);
    stream->m_mode = mode;
    stream->m_infoValid = false;
    stream->m_info.m_path = stream->m_pathname;
    GetFileInfoByFile(stream);

    if (mode & 0x200) {
        Blizzard::File::SetPos(stream, stream->m_info.m_size, 0);
    }

    parms->m_stream = stream;

    return true;
}

bool System_File::Read(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    return read_overlapped(parms->m_stream, parms->m_buffer, nullptr, &parms->m_amount);
}

bool System_File::RemoveDirectory(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    const char* path = parms->m_path;

    if (parms->m_recursive) {
        return Blizzard::File::RemoveDirectoryAndContents(path, false);
    }

    if (!path) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    char resolved[1024];
    ResolvePath(path, resolved, sizeof(resolved), false);

    return rmdir(resolved) == 0;
}

bool System_File::SetCacheMode(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    Blizzard::File::StreamRecord* stream = parms->m_stream;
    stream->m_mode = parms->m_mode | (stream->m_mode & ~0x40);

    DWORD sharemode;
    DWORD desiredaccess;
    DWORD creationdisposition;
    DWORD flagsandattributes;

    if (!ToCreateFlags(stream->m_mode, &sharemode, &desiredaccess, &creationdisposition, &flagsandattributes)) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    if (ValidHandle(stream->m_fileHandle)) {
        close(ToFd(stream->m_fileHandle));
    }

    stream->m_fileHandle = FromFd(OpenNative(stream->m_info.m_path, stream->m_mode));

    if (stream->m_fileHandle == INVALID_HANDLE_VALUE) {
        Blizzard::File::SetLastError(4);
        Blizzard::File::AddToLastErrorStack(4, "Win32 Close", 1);
        return false;
    }

    return true;
}

bool System_File::SetEOF(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    int64_t pos = parms->m_pos;
    Blizzard::File::StreamRecord* stream = parms->m_stream;
    int32_t whence = parms->m_posMode;
    int64_t offset;

    if (!Blizzard::File::GetPos(stream, &offset) || !Blizzard::File::SetPos(stream, pos, whence)) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    int32_t fd = ToFd(stream->m_fileHandle);
    off_t end = lseek(fd, 0, SEEK_CUR);
    bool result = end != -1 && ftruncate(fd, end) == 0;

    if (result) {
        stream->m_infoValid = false;
    }

    Blizzard::File::SetPos(stream, offset, 0);

    return result;
}

bool System_File::SetAttributes(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    if (parms->m_has.m_all & 0x10) {
        // Timestamp is nanoseconds since 2000-01-01 UTC (946684800 Unix seconds).
        Blizzard::Time::Timestamp time = parms->m_info->m_modTime;
        int64_t seconds = time / 1000000000;
        int64_t nanoseconds = time % 1000000000;

        if (nanoseconds < 0) {
            seconds--;
            nanoseconds += 1000000000;
        }

        timespec times[2];
        times[0].tv_sec = 0;
        times[0].tv_nsec = UTIME_OMIT;
        times[1].tv_sec = seconds + 946684800;
        times[1].tv_nsec = (nanoseconds / 100) * 100;

        if (!ValidHandle(parms->m_stream->m_fileHandle) || futimens(ToFd(parms->m_stream->m_fileHandle), times) != 0) {
            Blizzard::File::SetLastError(8);
            return false;
        }

        parms->m_stream->m_info.m_modTime = time;
        parms->m_has.m_all &= ~0x10u;
    }

    if (parms->m_has.m_all & 0x4) {
        // Only the read-only bit has a Linux counterpart (the write permission bits).
        int32_t attributes = ToNativeAttributes(parms->m_info->m_attributes);
        char path[1024];
        ResolvePath(parms->m_path, path, sizeof(path), false);

        struct stat st;
        bool result = stat(path, &st) == 0;

        if (result) {
            mode_t mode = st.st_mode & 07777;

            if (attributes & kAttributeReadOnly) {
                mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
            } else {
                mode |= S_IWUSR;
            }

            result = chmod(path, mode) == 0;
        }

        if (!result) {
            Blizzard::File::SetLastError(8);
            return false;
        }

        parms->m_has.m_all &= ~0x4u;
    }

    return parms->m_has.m_all == 0;
}

bool System_File::SetPos(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    Blizzard::File::StreamRecord* stream = parms->m_stream;
    int32_t method = parms->m_posMode;

    if (!ValidHandle(stream->m_fileHandle)) {
        Blizzard::File::SetLastError(8);
        return false;
    }

    // As in the Windows code, any mode other than 0 (begin) and 1 (current) seeks from the end.
    int32_t whence = method == 0 ? SEEK_SET : (method == 1 ? SEEK_CUR : SEEK_END);

    return lseek(ToFd(stream->m_fileHandle), parms->m_pos, whence) != -1;
}

bool System_File::Write(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {
    return write_overlapped(parms->m_stream, parms->m_buffer, nullptr, &parms->m_amount);
}

bool System_File::read_locked(Blizzard::File::StreamRecord* file, void* buffer, OVERLAPPED* overlapped, int32_t* count) {
    Blizzard::Lock::ScopedMutex lock(*s_ioLock);

    if (!Blizzard::File::SetPos(file, (static_cast<int64_t>(overlapped->OffsetHigh) << 32) | overlapped->Offset, 0)) {
        *count = 0;
        Blizzard::File::SetLastError(9);
        return false;
    }

    return read_overlapped(file, buffer, nullptr, count);
}

bool System_File::write_locked(Blizzard::File::StreamRecord* file, void* buffer, OVERLAPPED* overlapped, int32_t* count) {
    Blizzard::Lock::ScopedMutex lock(*s_ioLock);

    if (!Blizzard::File::SetPos(file, (static_cast<int64_t>(overlapped->OffsetHigh) << 32) | overlapped->Offset, 0)) {
        *count = 0;
        Blizzard::File::SetLastError(9);
        return false;
    }

    return write_overlapped(file, buffer, nullptr, count);
}

void System_File::detect_io_mode() {
    // pread/pwrite always exist, like overlapped I/O on NT 5+.
    read_func = read_overlapped;
    write_func = write_overlapped;
}

bool System_File::read_first(Blizzard::File::StreamRecord* file, void* buffer, OVERLAPPED* overlapped, int32_t* count) {
    if (!s_read_init) {
        if (s_readInitCount.Increment() == 1) {
            detect_io_mode();
            s_read_init = true;
        } else {
            do {
                Blizzard::Thread::Sleep(1);
            } while (!s_read_init);
        }
    }

    return read_func(file, buffer, overlapped, count);
}

bool System_File::write_first(Blizzard::File::StreamRecord* file, void* buffer, OVERLAPPED* overlapped, int32_t* count) {
    if (!s_write_init) {
        if (s_writeInitCount.Increment() == 1) {
            detect_io_mode();
            s_write_init = true;
        } else {
            do {
                Blizzard::Thread::Sleep(1);
            } while (!s_write_init);
        }
    }

    return write_func(file, buffer, overlapped, count);
}

void System_File::FromNativeName(char* buffer, uint32_t buffersize, const char* str) {
    // Native names are already UTF-8.
    Blizzard::String::Copy(buffer, str ? str : "", static_cast<int32_t>(buffersize));
}

#endif
