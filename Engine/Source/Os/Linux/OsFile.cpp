#if defined(WOWDECOMP_PLATFORM_POSIX)

#include <cstdint>
#include <cerrno>
#include "Engine/Source/BlizzardCore/Source/System/File/File.h"
#include "Engine/Source/BlizzardCore/Source/System/String/Path.h"
#include "Engine/Source/BlizzardCore/Source/System/String/String.h"
#include "Engine/Source/Os/Linux/OsPath.h"
#include "Engine/Source/Os/OsFile.h"

// The Windows constants the HOSFILE interface takes, with their Windows values.
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 0x1
#define FILE_SHARE_WRITE 0x2
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_ATTRIBUTE_READONLY 0x1
#define FILE_ATTRIBUTE_HIDDEN 0x2
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80

// Game paths use '\\' and Windows case-insensitivity; each path is mapped to the host path before it reaches the
// file layer (see OsPath.h). A path too long to map is passed on unchanged.
static const char* ResolvePath(const char* path, char* buffer, size_t size, bool forCreate) {
    OsLinuxResolvePath(path, buffer, size, forCreate);
    return buffer[0] ? buffer : path;
}

#define OS_RESOLVE(name, path, forCreate) \
    char name##Buffer[4096]; \
    const char* name = ResolvePath(path, name##Buffer, sizeof(name##Buffer), forCreate)

bool OsCloseFile(HOSFILE file) {
    return Blizzard::File::Close(reinterpret_cast<Blizzard::File::StreamRecord*>(file));
}

FILE* OsFileOpenStdio(const char* fileName, const char* mode) {
    OS_RESOLVE(path, fileName, strchr(mode, 'w') || strchr(mode, 'a'));
    return fopen(path, mode);
}

int32_t OsFileExists(const char* fileName) {
    OS_RESOLVE(path, fileName, false);
    return Blizzard::File::IsFile(path);
}

int32_t OsDirectoryExists(const char* pathName) {
    OS_RESOLVE(path, pathName, false);
    return Blizzard::File::IsDirectory(path);
}

int32_t OsReadFile(HOSFILE file, void* buffer, uint32_t bytesToRead, uint32_t* bytesRead) {
    if (buffer && bytesRead) {
        *bytesRead = bytesToRead;
        return Blizzard::File::Read(reinterpret_cast<Blizzard::File::StreamRecord*>(file), buffer, reinterpret_cast<int32_t*>(bytesRead));
    }

    errno = EINVAL;
    return 0;
}

int32_t OsWriteFile(HOSFILE file, const void* buffer, uint32_t bytesToWrite, uint32_t* bytesWritten) {
    if (buffer && bytesWritten) {
        *bytesWritten = bytesToWrite;
        return Blizzard::File::Write(reinterpret_cast<Blizzard::File::StreamRecord*>(file), buffer, reinterpret_cast<int32_t*>(bytesWritten));
    }

    errno = EINVAL;
    return 0;
}

uint64_t OsGetFileSize(HOSFILE file) {
    Blizzard::File::FileInfo* info = Blizzard::File::GetFileInfo(reinterpret_cast<Blizzard::File::StreamRecord*>(file));
    return info ? info->m_size : 0xFFFFFFFF;
}

uint32_t OsGetFileAttributes(const char* fileName) {
    if (!fileName) {
        errno = EINVAL;
        return 0;
    }

    OS_RESOLVE(path, fileName, false);

    Blizzard::File::FileInfo info;
    if (!Blizzard::File::GetFileInfo(path, &info)) {
        return 0xFFFFFFFF;
    }

    return info.m_attributes;
}

int32_t OsSetFileAttributes(const char* fileName, uint32_t attributes) {
    if (!fileName) {
        errno = EINVAL;
        return 0;
    }

    OS_RESOLVE(path, fileName, false);
    return Blizzard::File::SetAttributes(path, attributes);
}

int32_t OsMoveFile(const char* existingFileName, const char* newFileName) {
    if (existingFileName && newFileName) {
        OS_RESOLVE(src, existingFileName, false);
        OS_RESOLVE(dst, newFileName, true);
        return Blizzard::File::Move(src, dst);
    }

    errno = EINVAL;
    return 0;
}

int32_t OsCopyFile(const char* existingFileName, const char* newFileName, int32_t failIfExists) {
    if (existingFileName && newFileName) {
        OS_RESOLVE(src, existingFileName, false);
        OS_RESOLVE(dst, newFileName, true);
        return Blizzard::File::Copy(src, dst, failIfExists == 0);
    }

    errno = EINVAL;
    return 0;
}

int32_t OsDeleteFile(const char* fileName) {
    if (!fileName) {
        errno = EINVAL;
        return 0;
    }

    OS_RESOLVE(path, fileName, false);
    return Blizzard::File::Delete(path);
}

int32_t OsCreateDirectory(const char* pathName, int32_t recursive) {
    if (!pathName) {
        errno = EINVAL;
        return 0;
    }

    OS_RESOLVE(path, pathName, true);
    return Blizzard::File::CreateDirectory(path, recursive != 0);
}

int32_t OsRemoveDirectory(const char* pathName) {
    if (!pathName) {
        errno = EINVAL;
        return 0;
    }

    OS_RESOLVE(path, pathName, false);
    return Blizzard::File::RemoveDirectory(path);
}

int32_t OsRemoveDirectoryRecurse(const char* pathName, uint32_t flags) {
    if (!pathName) {
        errno = EINVAL;
        return 0;
    }

    OS_RESOLVE(path, pathName, false);
    return Blizzard::File::RemoveDirectoryAndContents(path, (flags & 0x1) != 0);
}

bool OsSetCurrentDirectory(const char* path) {
    if (path) {
        OS_RESOLVE(resolved, path, false);
        return Blizzard::File::SetWorkingDirectory(resolved) != 0;
    }

    errno = EINVAL;

    return false;
}

int32_t OsGetCurrentDirectory(uint32_t bufferSize, char* buffer) {
    if (!buffer) {
        errno = EINVAL;
        return 0;
    }

    return Blizzard::File::GetWorkingDirectory(buffer, bufferSize);
}

bool OsFileList_Internal::Callback(const Blizzard::File::ProcessDirParms& parms) {
    OS_FILE_DATA data;
    Blizzard::String::Copy(data.fileName, parms.m_filename, sizeof(data.fileName));

    char path[260];
    Blizzard::String::JoinPath(path, sizeof(path), parms.m_pathRoot, parms.m_filename);

    OsFileList_Internal* list = static_cast<OsFileList_Internal*>(parms.m_cookie);

    if (!Blizzard::File::SimpleGlob(parms.m_filename, list->m_pattern)) {
        return true;
    }

    Blizzard::File::FileInfo info;
    if (!Blizzard::File::GetFileInfo(path, &info)) {
        return true;
    }

    data.size = static_cast<uint32_t>(info.m_size);
    data.flags = 0;

    if (info.m_attributes & 0x40) {
        data.flags = FILE_ATTRIBUTE_DIRECTORY;
    }

    if (info.m_attributes & 0x1) {
        data.flags |= FILE_ATTRIBUTE_READONLY;
    }

    if (info.m_attributes & 0x2) {
        data.flags |= FILE_ATTRIBUTE_HIDDEN;
    }

    if (!list->m_includeHidden && (info.m_attributes & 0x2)) {
        return true;
    }

    return !list->m_callback(data, list->m_param);
}

uint32_t GetFileMode(uint32_t desiredAccess, uint32_t shareMode, uint32_t createDisposition) {
    uint32_t mode = 0;

    if (desiredAccess & GENERIC_READ) {
        mode = 0x1;
    }

    if (desiredAccess & GENERIC_WRITE) {
        mode |= 0x2;
    }

    if (shareMode & FILE_SHARE_READ) {
        mode |= 0x4;
    }

    if (shareMode & FILE_SHARE_WRITE) {
        mode |= 0x8;
    }

    switch (createDisposition) {
        case CREATE_NEW:
            mode |= 0xC00;
            break;

        case CREATE_ALWAYS:
            mode |= 0x400;
            break;

        case OPEN_EXISTING:
            mode |= 0x1000;
            break;

        case OPEN_ALWAYS:
            mode |= 0x200;
            break;

        case TRUNCATE_EXISTING:
            mode |= 0x100;
            break;
    }

    return mode;
}

char* OsFileGetDownloadFolder() {
        static char s_downloadfolder[260];

    if (!s_downloadfolder[0]) {
        Blizzard::File::GetWorkingDirectory(s_downloadfolder, sizeof(s_downloadfolder));
    }

    return s_downloadfolder;
}

HOSFILE OsCreateFile(const char* fileName, uint32_t desiredAccess, uint32_t shareMode, uint32_t createDisposition, uint32_t flagsAndAttributes, uint32_t extendedFileType) {
    if (!fileName || !desiredAccess || createDisposition < CREATE_NEW || createDisposition > TRUNCATE_EXISTING) {
        return reinterpret_cast<HOSFILE>(INVALID_HANDLE_VALUE);
    }

    OS_RESOLVE(path, fileName, createDisposition != OPEN_EXISTING && createDisposition != TRUNCATE_EXISTING);

    Blizzard::File::StreamRecord* file;
    if (!Blizzard::File::Open(path, GetFileMode(desiredAccess, shareMode, createDisposition), file)) {
        return reinterpret_cast<HOSFILE>(INVALID_HANDLE_VALUE);
    }

    if (!(flagsAndAttributes & FILE_ATTRIBUTE_NORMAL)) {
        OsSetFileAttributes(path, flagsAndAttributes);
    }

    return reinterpret_cast<HOSFILE>(file);
}

int32_t OsFileList(const char* dir, const char* pattern, OsFileListCallback callback, void* param, int32_t includeHidden) {
    char relative[260];
    Blizzard::String::Copy(relative, dir, sizeof(relative));

    char* separator = Blizzard::String::FindPathSeparatorReverse(relative, 0x7FFFFFFF);
    if (separator) {
        *separator = '\0';
    }

    OS_RESOLVE(resolved, relative, false);

    char path[260];
    if (!Blizzard::File::MakeAbsolutePath(resolved, path, sizeof(path), false)) {
        return 0;
    }

    OsFileList_Internal list;
    list.m_callback = callback;
    list.m_param = param;
    list.m_includeHidden = includeHidden;
    list.m_pattern = pattern;

    return Blizzard::File::ProcessDirFast(path, &list, OsFileList_Internal::Callback, false);
}

#endif
