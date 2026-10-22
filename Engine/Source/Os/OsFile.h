#ifndef ENGINE_SOURCE_OS_OSFILE_H
#define ENGINE_SOURCE_OS_OSFILE_H

#include <cstdint>
#include <cstdio>
#include "Engine/Source/BlizzardCore/Source/System/File/Types.h"

struct HOSFILE__;
typedef HOSFILE__* HOSFILE;

#define OS_CREATE_ALWAYS 2
#define OS_FILE_ATTRIBUTE_NORMAL 0x80
#define OS_FILE_ATTRIBUTE_DIRECTORY 0x10
#define OS_FILE_TYPE_ANY 0x3F3F3F3F
#define OS_GENERIC_READ 0x80000000
#define OS_GENERIC_WRITE 0x40000000
#define OS_OPEN_EXISTING 3
#define OS_INVALID_FILE reinterpret_cast<HOSFILE>(static_cast<intptr_t>(-1))

struct OS_FILE_DATA {
    /* 0000 */ uint32_t size;
    /* 0004 */ uint32_t flags;
    /* 0008 */ char fileName[260];
};

typedef int32_t (*OsFileListCallback)(OS_FILE_DATA& data, void* param);

struct OsFileList_Internal {
    /* 0000 */ OsFileListCallback m_callback;
    /* 0004 */ void* m_param;
    /* 0008 */ int32_t m_includeHidden;
    /* 000C */ const char* m_pattern;

    static bool Callback(const Blizzard::File::ProcessDirParms& parms);
};

bool OsCloseFile(HOSFILE file);
int32_t OsCopyFile(const char* existingFileName, const char* newFileName, int32_t failIfExists);
int32_t OsCreateDirectory(const char* pathName, int32_t recursive);
HOSFILE OsCreateFile(const char* fileName, uint32_t desiredAccess, uint32_t shareMode, uint32_t createDisposition, uint32_t flagsAndAttributes, uint32_t extendedFileType);
int32_t OsDeleteFile(const char* fileName);
int32_t OsDirectoryExists(const char* pathName);
int32_t OsFileExists(const char* fileName);
char* OsFileGetDownloadFolder();
int32_t OsFileList(const char* dir, const char* pattern, OsFileListCallback callback, void* param, int32_t includeHidden);
int32_t OsGetCurrentDirectory(uint32_t bufferSize, char* buffer);
uint32_t OsGetFileAttributes(const char* fileName);
uint64_t OsGetFileSize(HOSFILE file);
int32_t OsMoveFile(const char* existingFileName, const char* newFileName);
int32_t OsReadFile(HOSFILE file, void* buffer, uint32_t bytesToRead, uint32_t* bytesRead);
int32_t OsRemoveDirectory(const char* pathName);
int32_t OsRemoveDirectoryRecurse(const char* pathName, uint32_t flags);
bool OsSetCurrentDirectory(const char* path);
int32_t OsSetFileAttributes(const char* fileName, uint32_t attributes);
int32_t OsWriteFile(HOSFILE file, const void* buffer, uint32_t bytesToWrite, uint32_t* bytesWritten);
// fopen for a game path ("Screenshots\\x.jpg"), mapped to the host path where the OS needs it.
FILE* OsFileOpenStdio(const char* fileName, const char* mode);

#endif
